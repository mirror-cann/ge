/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

/**
 * @file custom_op.cpp
 * @brief ArgsUpdater 地址刷新自定义算子示例
 *
 * 本文件实现两个功能相同的自定义算子：
 * - AddRefreshOp: 实现 ArgsUpdater 接口，支持地址刷新优化
 * - AddNoRefreshOp: 不实现 ArgsUpdater，GE 框架插入额外 Identity 算子产生 MEMCPY_ASYNC 开销
 *
 * 核心机制：
 * - MallocReadOnlyDevArgs: 在 device 侧分配只读 kernel args 内存
 * - UpdateHostArgs: 后续执行时仅刷新 args 中的地址字段，避免重新分配
 * - aclrtLaunchKernelV2: 使用 device 侧 args 下发 kernel
 *
 * 性能收益：
 * - 避免每轮执行的 device 内存分配和 H2D 拷贝开销
 * - 在高频执行场景下可带来约 1.17x 加速
 */

#include <algorithm>
#include "acl/acl_rt.h"
#include "graph/custom_op.h"
#include "add_custom.h"
#include "add_custom_kernel.h"
#include "utils/rtc_kernel_loader.h"
#include "utils/log.h"

namespace {
/**
 * @brief 常量定义
 *
 * kInputX/kInputY: 输入张量索引
 * kOutputZ: 输出张量索引
 * kMaxBlocks: 最大 block 数量
 * kAddCustomBlockSize: kernel block 大小（定义在 add_custom_kernel.h 中）
 * kAddCustomKernelName: kernel 函数名称（定义在 add_custom_kernel.h 中）
 */
constexpr size_t kInputX = 0U;
constexpr size_t kInputY = 1U;
constexpr size_t kOutputZ = 0U;
constexpr uint32_t kMaxBlocks = 65535;
constexpr const char *kKernelSourceFile = "add_custom.asc";

/**
 * @brief Kernel 参数结构体
 *
 * packed 属性确保结构体紧凑排列，避免 padding
 * aligned(8) 确保每个指针 8 字节对齐
 *
 * 字段说明：
 * - x_ptr: 输入 x 的设备内存地址
 * - y_ptr: 输入 y 的设备内存地址
 * - z_ptr: 输出 z 的设备内存地址
 */
struct __attribute__((packed)) AddArgs {
  const void *x_ptr __attribute__((aligned(8)));
  const void *y_ptr __attribute__((aligned(8)));
  void *z_ptr __attribute__((aligned(8)));
};

/**
 * @brief 全局 RTC Kernel 加载器实例
 *
 * 使用单例模式管理 kernel 的加载和生命周期
 * 首次调用 Load() 时编译并加载 kernel，后续调用直接返回
 */
RtcKernelLoader g_kernel_loader;

/**
 * @brief 加载 kernel
 *
 * 委托给 RtcKernelLoader 处理 RTC 编译和加载流程
 * 使用全局实例缓存句柄，避免重复加载
 *
 * @return GRAPH_SUCCESS 表示成功，GRAPH_FAILED 表示失败
 */
ge::graphStatus LoadKernel() {
  return g_kernel_loader.Load(kAddCustomKernelName, kKernelSourceFile);
}

/**
 * @brief 计算 kernel 启动的 block 数量
 *
 * 根据元素总数计算需要的 block 数量，上限为 kMaxBlocks
 * 公式：ceil(n_elements / kAddCustomBlockSize)，但不超过 kMaxBlocks
 *
 * @param n_elements 元素总数
 * @return block 数量
 */
uint32_t CalcNumBlocks(uint32_t n_elements) {
  return std::min((n_elements + kAddCustomBlockSize - 1) / kAddCustomBlockSize, kMaxBlocks);
}
}  // namespace

namespace ge {

/**
 * @brief 带 ArgsUpdater 的 Add 算子
 *
 * 继承关系：
 * - EagerExecuteOp: 提供 Execute 接口，支持在线执行
 * - ArgsUpdater: 提供 UpdateHostArgs 接口，支持地址刷新优化
 * - ShapeInferOp: 提供 InferShape/InferDataType 接口
 *
 * 核心优化：
 * - 模型加载时通过 MallocReadOnlyDevArgs 在 device 侧分配只读 args 内存
 * - 后续执行时 GE 框架调用 UpdateHostArgs，仅刷新 args 中的地址字段
 * - 避免每轮执行的 device 内存分配和 H2D 拷贝开销
 */
class AddRefreshOp : public EagerExecuteOp, public ArgsUpdater, public ShapeInferOp {
 public:
  /**
   * @brief 执行算子
   *
   * 执行流程：
    * 1. 加载 kernel（模型加载时）
    * 2. 获取输入输出张量
    * 3. 计算 block 数量
    * 4. 通过 MallocReadOnlyDevArgs 注册 args
     * 5. 使用 aclrtLaunchKernelV2 下发 kernel
    */
   graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
     // 加载 kernel（模型加载时编译并缓存）
     if (LoadKernel() != GRAPH_SUCCESS) {
       LOG_ERROR("LoadKernel failed");
       return GRAPH_FAILED;
     }

     // 获取输入张量
     const gert::Tensor *input_x = ctx->GetInputTensor(kInputX);
     const gert::Tensor *input_y = ctx->GetInputTensor(kInputY);
     if (input_x == nullptr || input_y == nullptr) {
       LOG_ERROR("GetInputTensor failed, input_x=", input_x, " input_y=", input_y);
       return GRAPH_FAILED;
     }

     // 分配输出张量（与输入相同的 shape/format/datatype）
     gert::Tensor *output_z =
         ctx->MallocOutputTensor(kOutputZ, input_x->GetShape(), input_x->GetFormat(), input_x->GetDataType());
     if (output_z == nullptr) {
       LOG_ERROR("MallocOutputTensor failed");
       return GRAPH_FAILED;
     }

     // 计算 kernel 启动参数
     const uint32_t num_blocks = CalcNumBlocks(static_cast<uint32_t>(input_x->GetShapeSize()));
     AddArgs args = {input_x->GetAddr(), input_y->GetAddr(), const_cast<void *>(output_z->GetAddr())};

     // 关键优化：通过 MallocReadOnlyDevArgs 注册 args
     // - 模型加载时：在 device 侧分配只读内存并拷贝 args
    // - 后续执行：GE 框架调用 UpdateHostArgs 刷新地址，避免重新分配
    const gert::KernelArgs *registered_args = ctx->MallocReadOnlyDevArgs(&args, sizeof(args));
    if (registered_args == nullptr) {
      LOG_ERROR("MallocReadOnlyDevArgs failed");
      return GRAPH_FAILED;
    }

    // 使用 device 侧的 args 启动 kernel
    aclError ret = aclrtLaunchKernelV2(g_kernel_loader.GetFuncHandle(), num_blocks, registered_args->args_data,
                                       registered_args->args_size, nullptr, ctx->GetStream());
    if (ret != ACL_ERROR_NONE) {
      LOG_ERROR("aclrtLaunchKernelV2 failed, error: ", ret);
      return GRAPH_FAILED;
    }

    return GRAPH_SUCCESS;
  }

  /**
   * @brief 更新 host 侧 args
   *
   * 由 GE 框架在后续执行时调用，仅刷新 args 中的地址字段
   * GE 框架会自动将更新后的 host args 同步到 device 侧
   *
   * 这是 ArgsUpdater 的核心优化点：
   * - 避免每轮执行重新分配 device 内存
   * - 避免每轮执行完整的 H2D 拷贝
   * - 仅更新变化的地址字段
   */
  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    // 获取输入输出张量的最新地址
    const gert::Tensor *input_x = ctx->GetInputTensor(kInputX);
    const gert::Tensor *input_y = ctx->GetInputTensor(kInputY);
    const gert::Tensor *output_z = ctx->GetOutputTensor(kOutputZ);

    if (input_x == nullptr || input_y == nullptr || output_z == nullptr) {
      LOG_ERROR("UpdateHostArgs failed, input_x=", input_x, " input_y=", input_y, " output_z=", output_z);
      return GRAPH_FAILED;
    }

    // 获取 host 侧 args 指针
    const gert::KernelArgs *host_args = ctx->GetKernelArgs(gert::Placement::kPlacementHost, 0);
    if (host_args == nullptr) {
      LOG_ERROR("UpdateHostArgs failed, host_args is null");
      return GRAPH_FAILED;
    }

    // 仅刷新 args 中的地址字段
    auto *args = static_cast<AddArgs *>(host_args->args_data);
    args->x_ptr = input_x->GetAddr();
    args->y_ptr = input_y->GetAddr();
    args->z_ptr = const_cast<void *>(output_z->GetAddr());

    return GRAPH_SUCCESS;
  }

  /**
   * @brief 推导输出 shape
   *
   * Add 算子的输出 shape 与输入相同
   */
  graphStatus InferShape(gert::InferShapeContext *ctx) override {
    const auto *input_shape = ctx->GetInputShape(kInputX);
    auto *output_shape = ctx->GetOutputShape(kOutputZ);
    if (input_shape == nullptr || output_shape == nullptr) {
      LOG_ERROR("InferShape failed, input_shape=", input_shape, " output_shape=", output_shape);
      return GRAPH_FAILED;
    }
    output_shape->SetDimNum(input_shape->GetDimNum());
    for (size_t i = 0; i < input_shape->GetDimNum(); ++i) {
      output_shape->SetDim(i, input_shape->GetDim(i));
    }
    return GRAPH_SUCCESS;
  }

  /**
   * @brief 推导输出数据类型
   *
   * Add 算子的输出数据类型与输入相同
   */
  graphStatus InferDataType(gert::InferDataTypeContext *ctx) override {
    return ctx->SetOutputDataType(kOutputZ, ctx->GetInputDataType(kInputX));
  }
};

REG_AUTO_MAPPING_OP(AddRefreshOp);

/**
 * @brief 不带 ArgsUpdater 的 Add 算子
 *
 * 继承关系：
 * - EagerExecuteOp: 提供 Execute 接口，支持在线执行
 * - ShapeInferOp: 提供 InferShape/InferDataType 接口
 *
 * 与 AddRefreshOp 的区别：
 * - 不实现 ArgsUpdater 接口，未通过 MallocReadOnlyDevArgs 注册 args
 * - GE 框架无法感知地址变化，会插入额外的 Identity 算子搬运数据，产生 MEMCPY_ASYNC 开销
 * - 无法享受地址刷新优化
 *
 * 用途：作为性能对比的 baseline
 */
class AddNoRefreshOp : public EagerExecuteOp, public ShapeInferOp {
 public:
  /**
   * @brief 执行算子（仅在模型加载时调用一次）
   *
   * 执行流程：
   * 1. 加载 kernel（模型加载时）
   * 2. 获取输入输出张量
   * 3. 计算 block 数量
   * 4. 分配 device 内存并拷贝 args
   * 5. 使用 aclrtLaunchKernelV2 下发 kernel
   *
   * 注意：由于未注册 args，GE 框架在后续执行时会插入 Identity 算子搬运数据，
   * 产生额外的 MEMCPY_ASYNC 开销，这是与 AddRefreshOp 的性能差异来源。
   */
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
     // 加载 kernel（模型加载时编译并缓存）
     if (LoadKernel() != GRAPH_SUCCESS) {
       LOG_ERROR("LoadKernel failed");
       return GRAPH_FAILED;
     }

     // 获取输入张量
     const gert::Tensor *input_x = ctx->GetInputTensor(kInputX);
     const gert::Tensor *input_y = ctx->GetInputTensor(kInputY);
     if (input_x == nullptr || input_y == nullptr) {
       LOG_ERROR("GetInputTensor failed, input_x=", input_x, " input_y=", input_y);
       return GRAPH_FAILED;
     }

     // 分配输出张量（与输入相同的 shape/format/datatype）
     gert::Tensor *output_z =
         ctx->MallocOutputTensor(kOutputZ, input_x->GetShape(), input_x->GetFormat(), input_x->GetDataType());
     if (output_z == nullptr) {
       LOG_ERROR("MallocOutputTensor failed");
       return GRAPH_FAILED;
     }

     // 计算 kernel 启动参数
     const uint32_t num_blocks = CalcNumBlocks(static_cast<uint32_t>(input_x->GetShapeSize()));
     AddArgs args = {input_x->GetAddr(), input_y->GetAddr(), const_cast<void *>(output_z->GetAddr())};

     // 分配 device 内存（Execute 仅在加载时调用一次）
    void *dev_args = nullptr;
    aclError ret = aclrtMalloc(&dev_args, sizeof(args), ACL_MEM_MALLOC_HUGE_FIRST);
    if (ret != ACL_ERROR_NONE) {
      LOG_ERROR("aclrtMalloc failed, error: ", ret);
      return GRAPH_FAILED;
    }

    // 拷贝 args 到 device（Execute 仅在加载时调用一次）
    ret = aclrtMemcpy(dev_args, sizeof(args), &args, sizeof(args), ACL_MEMCPY_HOST_TO_DEVICE);
    if (ret != ACL_ERROR_NONE) {
      LOG_ERROR("aclrtMemcpy failed, error: ", ret);
      aclrtFree(dev_args);
      return GRAPH_FAILED;
    }

    // 使用 device 侧的 args 启动 kernel
    ret = aclrtLaunchKernelV2(g_kernel_loader.GetFuncHandle(), num_blocks, dev_args, sizeof(args), nullptr,
                              ctx->GetStream());
    if (ret != ACL_ERROR_NONE) {
      LOG_ERROR("aclrtLaunchKernelV2 failed, error: ", ret);
      aclrtFree(dev_args);
      return GRAPH_FAILED;
    }

    return GRAPH_SUCCESS;
  }

  /**
   * @brief 推导输出 shape
   *
   * Add 算子的输出 shape 与输入相同
   */
  graphStatus InferShape(gert::InferShapeContext *ctx) override {
    const auto *input_shape = ctx->GetInputShape(kInputX);
    auto *output_shape = ctx->GetOutputShape(kOutputZ);
    if (input_shape == nullptr || output_shape == nullptr) {
      LOG_ERROR("InferShape failed, input_shape=", input_shape, " output_shape=", output_shape);
      return GRAPH_FAILED;
    }
    output_shape->SetDimNum(input_shape->GetDimNum());
    for (size_t i = 0; i < input_shape->GetDimNum(); ++i) {
      output_shape->SetDim(i, input_shape->GetDim(i));
    }
    return GRAPH_SUCCESS;
  }

  /**
   * @brief 推导输出数据类型
   *
   * Add 算子的输出数据类型与输入相同
   */
  graphStatus InferDataType(gert::InferDataTypeContext *ctx) override {
    return ctx->SetOutputDataType(kOutputZ, ctx->GetInputDataType(kInputX));
  }
};

REG_AUTO_MAPPING_OP(AddNoRefreshOp);

}  // namespace ge
