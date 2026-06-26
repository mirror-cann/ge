/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef METADEF_CXX_INC_EXE_GRAPH_RUNTIME_EAGER_OP_EXECUTION_CONTEXT_H_
#define METADEF_CXX_INC_EXE_GRAPH_RUNTIME_EAGER_OP_EXECUTION_CONTEXT_H_

#include <type_traits>
#include "graph/tensor.h"
#include "exe_graph/runtime/extended_kernel_context.h"
#include "exe_graph/runtime/kernel_args.h"

namespace gert {
using rtStream = void *;

class EagerOpExecutionContext : public ExtendedKernelContext {
 public:
  /**
   * 获取所属的执行流
   * @return
   */
  rtStream GetStream() const;

  /**
   * 根据输入index，获取输入tensor指针
   * @param index 输入index
   * @return Tensor指针，异常时返回空指针
   */
  const Tensor *GetInputTensor(size_t index) const {
    if (static_cast<int64_t>(index) >= GetAdditionalInputStartIndex()) {
      return nullptr;
    }
    return GetInputPointer<Tensor>(index);
  }

  /**
   * 基于算子IR原型定义，获取`REQUIRED_INPUT`类型的输入Tensor指针
   * @param ir_index IR原型定义中的index
   * @return Tensor指针，异常时返回空指针
   */
  const Tensor *GetRequiredInputTensor(size_t ir_index) const {
    return GetDynamicInputPointer<Tensor>(ir_index, 0);
  }

  /**
   * 基于算子IR原型定义，获取`OPTIONAL_INPUT`类型的输入tensor指针
   * @param ir_index IR原型定义中的index
   * @return Tensor指针，异常时返回空指针
   */
  const Tensor *GetOptionalInputTensor(size_t ir_index) const {
    return GetDynamicInputPointer<Tensor>(ir_index, 0);
  }

  /**
   * 基于算子IR原型定义，获取`DYNAMIC_INPUT`类型的输入Tensor指针
   * @param ir_index IR原型定义中的index
   * @param relative_index
   * 该输入实例化后的相对index，例如某个DYNAMIC_INPUT实例化了3个输入，那么relative_index的有效范围是[0,2]
   * @return Tensor指针，异常时返回空指针
   */
  const Tensor *GetDynamicInputTensor(size_t ir_index, size_t relative_index) const {
    return GetDynamicInputPointer<Tensor>(ir_index, relative_index);
  }

  /**
   * 为某个输出tensor申请device内存，同时初始化输出tensor的基本信息
   * @param index 输出索引
   * @param shape 输出tensor的shape
   * @param format 输出tensor的format
   * @param dtype 输出tensor的data type
   * @return Tensor指针，异常时返回空指针
   * 生命周期: 该输出tensor的内存由context构造方管理。接口调用者不需要主动释放
   */
  Tensor *MallocOutputTensor(size_t index, const StorageShape &shape, const StorageFormat &format, ge::DataType dtype);

  /**
   * 指定某输出的内存地址引用自某个输入
   * @param output_index 输出索引
   * @param input_index 输入索引
   * @return output_index对应的输出Tensor指针
   */
  Tensor *MakeOutputRefInput(size_t output_index, size_t input_index) const;

  /**
   * 分配workspace内存，placement为device
   * @param size 内存大小，单位为字节
   * @return 地址指针，异常时返回空指针
   * 生命周期：内存由context构造方管理，接口调用者不需要主动释放
   */
  void *MallocWorkSpace(size_t size);

  /**
   * 获取index指定的输出Tensor指针
   * @param index 输出索引
   * @return 输出Tensor指针，异常时返回空指针
   */
  const Tensor *GetOutputTensor(size_t index) const {
    return GetOutputPointer<Tensor>(index);
  }

  /**
   * 为 host 侧 args buffer 分配设备侧内存，并返回只读 KernelArgs 指针。
   *
   * KernelArgs 的 args_data 字段保存设备侧 args 地址，用于自定义算子
   * EagerExecute 中 kernel launch。
   *
   * 本接口在模型执行前将 host_args 内容同步到设备侧
   * 接口返回后调用方可立即释放host_args，无需手动 H2D 拷贝。
   *
   * 返回的 args_data 地址为只读，调用方不得通过该指针修改 args_data指向的内容。
   *
   * @param host_args Host 侧 args buffer 指针，接口返回后调用方可立即释放。
   * @param args_size Args buffer 大小（字节）。
   * @return 只读 KernelArgs 指针，args_data 字段为设备侧 args 地址，
   *         失败返回 nullptr。
   */
  const KernelArgs *MallocReadOnlyDevArgs(void *host_args, size_t args_size) const;

  enum class AdditionalInputIndex : uint32_t {
    kDeviceAllocator = 0,
    kStream,
    kArgsHandler  // Args handler base class pointer
  };

  enum class AdditionalOutputIndex : uint32_t { kWorkSpace = 0, kNum };

 protected:
  int64_t GetAdditionalInputStartIndex() const {
    const auto compute_node_info = GetComputeNodeInfo();
    if (compute_node_info == nullptr) {
      return -1;
    }
    return compute_node_info->GetInputsNum();
  }

  int64_t GetAdditionalOutputStartIndex() const {
    const auto compute_node_info = GetComputeNodeInfo();
    if (compute_node_info == nullptr) {
      return -1;
    }
    return compute_node_info->GetOutputsNum();
  }
};

static_assert(std::is_standard_layout<EagerOpExecutionContext>::value,
              "The class EagerOpExecutionContext must be a POD");
}  // namespace gert
#endif  // METADEF_CXX_INC_EXE_GRAPH_RUNTIME_EAGER_OP_EXECUTION_CONTEXT_H_
