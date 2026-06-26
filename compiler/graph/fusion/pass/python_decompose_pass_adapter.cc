/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "python_pass_adapter.h"

#include <new>
#include <utility>
#include <vector>

#include "framework/common/debug/ge_log.h"

namespace ge {
namespace fusion {
namespace {
std::vector<AscendString> BuildDecomposeOpTypes(const PythonPassDescriptor &pass_desc) {
  std::vector<AscendString> op_types;
  op_types.reserve(pass_desc.op_types.size());
  for (const auto &op_type : pass_desc.op_types) {
    op_types.emplace_back(op_type.c_str());
  }
  return op_types;
}
}  // namespace

PythonDecomposePassAdapter::PythonDecomposePassAdapter(const PythonPassDescriptor &pass_desc)
    : DecomposePass(BuildDecomposeOpTypes(pass_desc)), holder_(new (std::nothrow) PythonPassHolder(pass_desc)) {}

PythonDecomposePassAdapter::~PythonDecomposePassAdapter() = default;

bool PythonDecomposePassAdapter::IsValid() const {
  return (holder_ != nullptr) && holder_->IsValid();
}

bool PythonDecomposePassAdapter::MeetRequirements(const GNode &matched_node) {
  if ((holder_ == nullptr) || (!holder_->IsValid()) || (holder_->GetHolder() == nullptr)) {
    return false;
  }
  if (holder_->GetCallbacks().decompose_meet_requirements == nullptr) {
    return DecomposePass::MeetRequirements(matched_node);
  }
  const bool ret = holder_->GetCallbacks().decompose_meet_requirements(holder_->GetHolder(), matched_node);
  GELOGI("PythonDecomposePassAdapter::MeetRequirements for pass[%s] returned[%d].",
         holder_->GetPassDescriptor().pass_name.c_str(), ret ? 1 : 0);
  return ret;
}

GraphUniqPtr PythonDecomposePassAdapter::Replacement(const GNode &matched_node) {
  if ((holder_ == nullptr) || (!holder_->IsValid()) || (holder_->GetHolder() == nullptr) ||
      (holder_->GetCallbacks().decompose_replacement == nullptr)) {
    GELOGW("PythonDecomposePassAdapter::Replacement() is invalid.");
    return nullptr;
  }
  GraphUniqPtr replacement_graph;
  const auto ret = holder_->GetCallbacks().decompose_replacement(holder_->GetHolder(), matched_node, replacement_graph);
  if (ret != SUCCESS) {
    GELOGW("Python decompose adapter Replacement callback failed, ret[%u].", static_cast<uint32_t>(ret));
    return nullptr;
  }
  GELOGI("PythonDecomposePassAdapter::Replacement succeeded for pass[%s], graph[%p].",
         holder_->GetPassDescriptor().pass_name.c_str(), replacement_graph.get());
  return replacement_graph;
}
}  // namespace fusion
}  // namespace ge
