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

#include <memory>
#include <utility>
#include <vector>

#include "framework/common/debug/ge_log.h"

namespace ge {
namespace fusion {
PythonPatternFusionPassAdapter::PythonPatternFusionPassAdapter(const PythonPassDescriptor &pass_desc)
    : holder_(new (std::nothrow) PythonPassHolder(pass_desc)) {}

PythonPatternFusionPassAdapter::~PythonPatternFusionPassAdapter() = default;

bool PythonPatternFusionPassAdapter::IsValid() const {
  return (holder_ != nullptr) && holder_->IsValid();
}

std::vector<PatternUniqPtr> PythonPatternFusionPassAdapter::Patterns() {
  if ((holder_ == nullptr) || (!holder_->IsValid()) || (holder_->GetHolder() == nullptr) ||
      (holder_->GetCallbacks().patterns == nullptr)) {
    GELOGW("PythonPatternFusionPassAdapter::Patterns() is invalid.");
    return {};
  }
  std::vector<PatternUniqPtr> patterns;
  const auto ret = holder_->GetCallbacks().patterns(holder_->GetHolder(), patterns);
  if (ret != SUCCESS) {
    GELOGW("Python pattern fusion adapter Patterns callback failed, ret[%u].", static_cast<uint32_t>(ret));
    return {};
  }
  return patterns;
}

bool PythonPatternFusionPassAdapter::MeetRequirements(const std::unique_ptr<MatchResult> &match_result) {
  if ((holder_ == nullptr) || (!holder_->IsValid()) || (holder_->GetHolder() == nullptr)) {
    return false;
  }
  if (holder_->GetCallbacks().meet_requirements == nullptr) {
    return PatternFusionPass::MeetRequirements(match_result);
  }
  return holder_->GetCallbacks().meet_requirements(holder_->GetHolder(), match_result);
}

GraphUniqPtr PythonPatternFusionPassAdapter::Replacement(const std::unique_ptr<MatchResult> &match_result) {
  if ((holder_ == nullptr) || (!holder_->IsValid()) || (holder_->GetHolder() == nullptr) ||
      (holder_->GetCallbacks().replacement == nullptr)) {
    GELOGW("PythonPatternFusionPassAdapter::Replacement() is invalid.");
    return nullptr;
  }
  GraphUniqPtr replacement_graph;
  const auto ret = holder_->GetCallbacks().replacement(holder_->GetHolder(), match_result, replacement_graph);
  if (ret != SUCCESS) {
    GELOGW("Python pattern fusion adapter Replacement callback failed, ret[%u].", static_cast<uint32_t>(ret));
    return nullptr;
  }
  return replacement_graph;
}
}  // namespace fusion
}  // namespace ge
