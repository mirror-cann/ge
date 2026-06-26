/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_OP_COMPILE_CONTEXT_H
#define CANN_GRAPH_ENGINE_OP_COMPILE_CONTEXT_H

#include "exe_graph/runtime/extended_kernel_context.h"
#include "graph/tensor.h"
#include "platform/platform_infos_def.h"

namespace gert {

class OpCompileContext : public ExtendedKernelContext {
 public:
  /**
   * 根据输入 index，获取输入 tensor 指针。
   * @param index 输入 index
   * @return 输入 tensor 指针，异常时返回空指针
   */
  const Tensor *GetInputTensor(size_t index) const;

  /**
   * 基于算子 IR 原型定义，获取 `REQUIRED_INPUT` 类型的输入 tensor 指针
   * @param ir_index IR 原型定义中的 index
   * @return 输入 tensor 指针，异常时返回空指针
   */
  const Tensor *GetRequiredInputTensor(size_t ir_index) const;

  /**
   * 基于算子 IR 原型定义，获取 `OPTIONAL_INPUT` 类型的输入 tensor 指针
   * @param ir_index IR 原型定义中的 index
   * @return 输入 tensor 指针，异常时返回空指针
   */
  const Tensor *GetOptionalInputTensor(size_t ir_index) const;

  /**
   * 基于算子 IR 原型定义，获取 `DYNAMIC_INPUT` 类型的输入 tensor 指针
   * @param ir_index IR 原型定义中的 index
   * @param relative_index 该输入实例化后的相对 index
   * @return 输入 tensor 指针，异常时返回空指针
   */
  const Tensor *GetDynamicInputTensor(size_t ir_index, size_t relative_index) const;

  /**
   * 根据输出 index，获取输出 tensor 指针。
   * @param index 输出 index
   * @return 输出 tensor 指针，异常时返回空指针
   */
  const Tensor *GetOutputTensor(size_t index) const;

  /**
   * 基于算子 IR 原型定义，获取 `REQUIRED_OUTPUT` 类型的输出 tensor 指针
   * @param ir_index IR 原型定义中的 index
   * @return 输出 tensor 指针，异常时返回空指针
   */
  const Tensor *GetRequiredOutputTensor(size_t ir_index) const;

  /**
   * 基于算子 IR 原型定义，获取 `DYNAMIC_OUTPUT` 类型的输出 tensor 指针
   * @param ir_index IR 原型定义中的 index
   * @param relative_index 该输出实例化后的相对 index
   * @return 输出 tensor 指针，异常时返回空指针
   */
  const Tensor *GetDynamicOutputTensor(size_t ir_index, size_t relative_index) const;

  /**
   * 获取编译期图上下文中的 option 配置
   * @param option_key option key
   * @param option 输出 option value
   * @return 状态码
   */
  ge::graphStatus GetOption(const ge::AscendString &option_key, ge::AscendString &option) const;

  /**
   * 获取平台信息与可选信息
   * @param platform_info 输出平台信息
   * @param optional_infos 输出可选信息
   * @return 状态码
   */
  ge::graphStatus GetPlatformInfos(fe::PlatFormInfos &platform_info, fe::OptionalInfos &optional_infos) const;
};

}  // namespace gert
#endif  // CANN_GRAPH_ENGINE_OP_COMPILE_CONTEXT_H
