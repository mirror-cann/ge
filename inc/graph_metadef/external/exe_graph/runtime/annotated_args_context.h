/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef METADEF_CXX_INC_EXE_GRAPH_RUNTIME_ANNOTATED_ARGS_CONTEXT_H_
#define METADEF_CXX_INC_EXE_GRAPH_RUNTIME_ANNOTATED_ARGS_CONTEXT_H_

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "exe_graph/runtime/extended_kernel_context.h"
#include "graph/ge_error_codes.h"
#include "graph/tensor.h"

namespace ge {
struct ArgDesc;
}

namespace gert {
/**
 * 逻辑输入地址描述符，用于 AnnotatedKernelArgs 追加输入地址参数。
 * @since 9.2.0(2026-07)
 */
struct InputAddr {
  uint32_t index;    ///< 算子 IR 原型定义中的输入 index
  const void *addr;  ///< 输入设备地址
};

/**
 * 逻辑输出地址描述符，用于 AnnotatedKernelArgs 追加输出地址参数。
 * @since 9.2.0(2026-07)
 */
struct OutputAddr {
  uint32_t index;    ///< 算子 IR 原型定义中的输出 index
  const void *addr;  ///< 输出设备地址
};

/**
 * 逻辑 Workspace 地址描述符，用于 AnnotatedKernelArgs 追加逻辑 workspace 地址参数。
 * index 由 AnnotatedArgsContext::MallocWorkSpace 返回。
 * @since 9.2.0(2026-07)
 */
struct WorkspaceAddr {
  uint32_t index;    ///< workspace 序号，由 MallocWorkSpace 返回
  const void *addr;  ///< workspace 设备地址
};

/**
 * Kernel launch 信息描述符，用于 AnnotatedArgsContext::AddLaunch 指定 kernel 元信息。
 * @since 9.2.0(2026-07)
 */
struct AnnotatedKernelLaunchInfo {
  const char *kernel_name = nullptr;  ///< kernel 二进制中的入口函数名称
  const void *kernel_bin = nullptr;   ///< kernel 二进制数据
  size_t kernel_bin_size = 0U;        ///< kernel 二进制数据大小，单位为字节
  uint32_t block_dim = 0U;            ///< kernel 的 block 维度
  uint32_t stream_id = 0U;            ///< launch 所在的逻辑 stream ID
};

/**
 * Kernel launch 参数构建器，用于按序追加逻辑输入地址、逻辑输出地址、逻辑 workspace 地址和标量参数。
 * 追加顺序即为 kernel args 的排列顺序，最终通过 AnnotatedArgsContext::AddLaunch 提交。
 * @since 9.2.0(2026-07)
 */
class AnnotatedKernelArgs {
 public:
  /**
   * 构造一个空的参数构建器。
   * @since 9.2.0(2026-07)
   */
  AnnotatedKernelArgs();

  /**
   * 变参构造函数，按参数顺序依次追加所有参数。
   * 每个参数类型须为 InputAddr、OutputAddr、WorkspaceAddr 或 uint64_t。
   * @tparam Arg   第一个参数类型
   * @tparam Args  剩余参数类型包
   * @param arg    第一个参数
   * @param args   剩余参数
   * @since 9.2.0(2026-07)
   */
  template <typename Arg, typename... Args,
            typename = typename std::enable_if<
                !std::is_same<typename std::decay<Arg>::type, AnnotatedKernelArgs>::value>::type>
  explicit AnnotatedKernelArgs(Arg &&arg, Args &&...args) : AnnotatedKernelArgs() {
    AppendAll(std::forward<Arg>(arg), std::forward<Args>(args)...);
  }

  /**
   * 拷贝构造函数。
   * @param other 源对象
   * @since 9.2.0(2026-07)
   */
  AnnotatedKernelArgs(const AnnotatedKernelArgs &other);

  /**
   * 移动构造函数。移动后原对象不可再用。
   * @param other 源对象
   * @since 9.2.0(2026-07)
   */
  AnnotatedKernelArgs(AnnotatedKernelArgs &&other) noexcept;

  /**
   * 拷贝赋值运算符。
   * @param other 源对象
   * @return *this
   * @since 9.2.0(2026-07)
   */
  AnnotatedKernelArgs &operator=(const AnnotatedKernelArgs &other);

  /**
   * 移动赋值运算符。移动后原对象不可再用。
   * @param other 源对象
   * @return *this
   * @since 9.2.0(2026-07)
   */
  AnnotatedKernelArgs &operator=(AnnotatedKernelArgs &&other) noexcept;

  /**
   * 析构函数。
   * @since 9.2.0(2026-07)
   */
  ~AnnotatedKernelArgs();

  /**
   * 追加一个逻辑输入地址参数。
   * @param addr 逻辑输入地址描述符，包含 IR index 和设备地址
   * @return GRAPH_SUCCESS 表示追加成功，否则返回错误码
   * @since 9.2.0(2026-07)
   */
  ge::graphStatus AppendArg(const InputAddr &addr);

  /**
   * 追加一个逻辑输出地址参数。
   * @param addr 逻辑输出地址描述符，包含 IR index 和设备地址
   * @return GRAPH_SUCCESS 表示追加成功，否则返回错误码
   * @since 9.2.0(2026-07)
   */
  ge::graphStatus AppendArg(const OutputAddr &addr);

  /**
   * 追加一个逻辑 workspace 地址参数。
   * @param addr 逻辑 workspace 地址描述符，index 由 AnnotatedArgsContext::MallocWorkSpace 返回
   * @return GRAPH_SUCCESS 表示追加成功，否则返回错误码
   * @since 9.2.0(2026-07)
   */
  ge::graphStatus AppendArg(const WorkspaceAddr &addr);

  /**
   * 追加一个标量参数（uint64_t），用于 kernel 需要的常量参数。
   * @param value 标量值
   * @return GRAPH_SUCCESS 表示追加成功，否则返回错误码
   * @since 9.2.0(2026-07)
   */
  ge::graphStatus AppendArg(uint64_t value);

  /**
   * 提取 kernel launch 参数数据。
   * @param args_data  输出 kernel args 字节数据
   * @param arg_descs  输出 kernel args 描述
   * @return GRAPH_SUCCESS 表示提取成功，否则返回错误码
   * @since 9.2.0(2026-07)
   */
  ge::graphStatus ExtractArgsData(std::vector<uint8_t> &args_data, std::vector<ge::ArgDesc> &arg_descs) const;

 private:
  void AppendAll() {}

  template <typename Arg, typename... Args>
  void AppendAll(Arg &&arg, Args &&...args) {
    if (AppendArg(std::forward<Arg>(arg)) != ge::GRAPH_SUCCESS) {
      return;
    }
    AppendAll(std::forward<Args>(args)...);
  }

  void *impl_;
};

/**
 * 声明式 kernel launch 参数上下文，供 AnnotatedArgsOp::DeclareLaunchArgs 在编译期使用。
 * 继承自 ExtendedKernelContext，提供输入/输出 tensor 访问、workspace 分配和
 * kernel launch 任务添加能力。
 * @since 9.2.0(2026-07)
 */
class AnnotatedArgsContext : public ExtendedKernelContext {
 public:
  /**
   * 分配逻辑 workspace 内存。
   * 分配的内存由上下文管理，调用方无需手动释放。
   * @param size 内存大小，单位为字节
   * @return WorkspaceAddr 描述符，包含序号和逻辑 Workspace 偏移；失败时 addr 为空
   * @since 9.2.0(2026-07)
   */
  WorkspaceAddr MallocWorkSpace(size_t size);

  /**
   * 获取当前上下文所属的逻辑 stream ID。
   * @return stream ID，异常时返回 0
   * @since 9.2.0(2026-07)
   */
  uint32_t GetStreamId() const;

  /**
   * 添加一个 kernel launch 任务。
   * 当前端侧离线模型 TaskDef 生成仅支持单次 AddLaunch 和主 stream launch。
   * @param launch_info     kernel launch 信息
   * @param args            kernel launch 参数，通过 AnnotatedKernelArgs 构建
   * @return GRAPH_SUCCESS 表示添加成功，否则返回错误码
   * @since 9.2.0(2026-07)
   */
  ge::graphStatus AddLaunch(const AnnotatedKernelLaunchInfo &launch_info, AnnotatedKernelArgs &&args);

  /**
   * 根据输入 index 获取输入 Tensor 指针。
   * @param index 输入索引
   * @return Tensor 指针，异常或越界时返回 nullptr
   * @since 9.2.0(2026-07)
   */
  const Tensor *GetInputTensor(size_t index) const {
    const auto additional_input_start = GetAdditionalInputStartIndex();
    if ((additional_input_start < 0) || (index >= static_cast<size_t>(additional_input_start))) {
      return nullptr;
    }
    return GetInputPointer<Tensor>(index);
  }

  /**
   * 根据输出 index 获取输出 Tensor 指针。
   * @param index 输出索引
   * @return Tensor 指针，异常或越界时返回 nullptr
   * @since 9.2.0(2026-07)
   */
  const Tensor *GetOutputTensor(size_t index) const {
    const auto compute_node_info = GetComputeNodeInfo();
    if (compute_node_info == nullptr) {
      return nullptr;
    }
    if (index >= compute_node_info->GetOutputsNum()) {
      return nullptr;
    }
    return GetOutputPointer<Tensor>(index);
  }

  /**
   * 基于算子 IR 原型定义，获取 REQUIRED_INPUT 类型的输入 Tensor 指针。
   * @param ir_index IR 原型定义中的输入 index
   * @return Tensor 指针，异常时返回 nullptr
   * @since 9.2.0(2026-07)
   */
  const Tensor *GetRequiredInputTensor(size_t ir_index) const {
    return GetDynamicInputPointer<Tensor>(ir_index, 0);
  }

  /**
   * 基于算子 IR 原型定义，获取 DYNAMIC_INPUT 类型的输入 Tensor 指针。
   * @param ir_index       IR 原型定义中的输入 index
   * @param relative_index 该输入实例化后的相对 index
   * @return Tensor 指针，异常时返回 nullptr
   * @since 9.2.0(2026-07)
   */
  const Tensor *GetDynamicInputTensor(size_t ir_index, size_t relative_index) const {
    return GetDynamicInputPointer<Tensor>(ir_index, relative_index);
  }

  /**
   * 基于算子 IR 原型定义，获取 OPTIONAL_INPUT 类型的输入 Tensor 指针。
   * @param ir_index IR 原型定义中的输入 index
   * @return Tensor 指针，异常时返回 nullptr
   * @since 9.2.0(2026-07)
   */
  const Tensor *GetOptionalInputTensor(size_t ir_index) const {
    return GetDynamicInputPointer<Tensor>(ir_index, 0);
  }

  /**
   * 基于算子 IR 原型定义，获取必选输出 Tensor 指针。
   * @param ir_index IR 原型定义中的输出 index
   * @return Tensor 指针，异常或无实例时返回 nullptr
   * @since 9.2.0(2026-07)
   */
  const Tensor *GetRequiredOutputTensor(size_t ir_index) const {
    const auto ins_info = GetIrOutputInstanceInfo(ir_index);
    if (ins_info == nullptr) {
      return nullptr;
    }
    if (ins_info->GetInstanceNum() == 0U) {
      return nullptr;
    }
    return GetOutputPointer<Tensor>(ins_info->GetInstanceStart());
  }

  /**
   * 基于算子 IR 原型定义，获取 DYNAMIC_OUTPUT 类型的输出 Tensor 指针。
   * @param ir_index       IR 原型定义中的输出 index
   * @param relative_index 该输出实例化后的相对 index
   * @return Tensor 指针，异常时返回 nullptr
   * @since 9.2.0(2026-07)
   */
  const Tensor *GetDynamicOutputTensor(size_t ir_index, size_t relative_index) const {
    const auto ins_info = GetIrOutputInstanceInfo(ir_index);
    if (ins_info == nullptr) {
      return nullptr;
    }
    if (ins_info->GetInstanceNum() <= relative_index) {
      return nullptr;
    }
    return GetOutputPointer<Tensor>(ins_info->GetInstanceStart() + relative_index);
  }

 private:
  enum class AdditionalInputIndex : uint32_t { kWorkspaceAllocator = 0U, kNum };

  enum class AdditionalOutputIndex : uint32_t { kWorkSpace = 0U, kArgsHandler, kNum };

 protected:
  int64_t GetAdditionalOutputStartIndex() const {
    const auto compute_node_info = GetComputeNodeInfo();
    if (compute_node_info == nullptr) {
      return -1;
    }
    return compute_node_info->GetOutputsNum();
  }

  int64_t GetAdditionalInputStartIndex() const {
    const auto compute_node_info = GetComputeNodeInfo();
    if (compute_node_info == nullptr) {
      return -1;
    }
    return compute_node_info->GetInputsNum();
  }
};

static_assert(std::is_standard_layout<AnnotatedArgsContext>::value, "AnnotatedArgsContext must be standard layout");
static_assert(sizeof(AnnotatedArgsContext) == sizeof(ExtendedKernelContext),
              "AnnotatedArgsContext must not add member variables");
}  // namespace gert

#endif  // METADEF_CXX_INC_EXE_GRAPH_RUNTIME_ANNOTATED_ARGS_CONTEXT_H_
