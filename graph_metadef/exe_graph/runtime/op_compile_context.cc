/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ge_context.h"
#include "exe_graph/runtime/op_compile_context.h"
#include "platform/platform_info.h"

namespace gert {
const Tensor *OpCompileContext::GetInputTensor(size_t index) const {
  const auto compute_node_info = GetComputeNodeInfo();
  if (compute_node_info == nullptr) {
    return nullptr;
  }
  if (index >= compute_node_info->GetInputsNum()) {
    return nullptr;
  }
  return GetInputPointer<Tensor>(index);
}

const Tensor *OpCompileContext::GetRequiredInputTensor(size_t ir_index) const {
  return GetDynamicInputPointer<Tensor>(ir_index, 0);
}

const Tensor *OpCompileContext::GetOptionalInputTensor(size_t ir_index) const {
  return GetDynamicInputPointer<Tensor>(ir_index, 0);
}

const Tensor *OpCompileContext::GetDynamicInputTensor(size_t ir_index, size_t relative_index) const {
  return GetDynamicInputPointer<Tensor>(ir_index, relative_index);
}

const Tensor *OpCompileContext::GetOutputTensor(size_t index) const {
  const auto compute_node_info = GetComputeNodeInfo();
  if (compute_node_info == nullptr) {
    return nullptr;
  }
  if (index >= compute_node_info->GetOutputsNum()) {
    return nullptr;
  }
  return GetOutputPointer<Tensor>(index);
}

const Tensor *OpCompileContext::GetRequiredOutputTensor(size_t ir_index) const {
  const auto ins_info = GetIrOutputInstanceInfo(ir_index);
  if (ins_info == nullptr) {
    return nullptr;
  }
  if (ins_info->GetInstanceNum() == 0U) {
    return nullptr;
  }
  return GetOutputPointer<Tensor>(ins_info->GetInstanceStart());
}

const Tensor *OpCompileContext::GetDynamicOutputTensor(size_t ir_index, size_t relative_index) const {
  const auto ins_info = GetIrOutputInstanceInfo(ir_index);
  if (ins_info == nullptr) {
    return nullptr;
  }
  if (ins_info->GetInstanceNum() <= relative_index) {
    return nullptr;
  }
  return GetOutputPointer<Tensor>(ins_info->GetInstanceStart() + relative_index);
}

ge::graphStatus OpCompileContext::GetOption(const ge::AscendString &option_key, ge::AscendString &option) const {
  std::string option_str;
  const auto ret = ge::GetContext().GetOption(option_key.GetString(), option_str);
  option = ge::AscendString(option_str.c_str());
  return ret;
}
ge::graphStatus OpCompileContext::GetPlatformInfos(fe::PlatFormInfos &platform_info,
                                                   fe::OptionalInfos &optional_infos) const {
  const auto res =
      fe::PlatformInfoManager::GeInstance().GetPlatformInfoWithOutSocVersion(platform_info, optional_infos);
  return res;
}
}  // namespace gert
