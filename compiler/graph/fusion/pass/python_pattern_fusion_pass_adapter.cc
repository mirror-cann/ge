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
#include "graph_metadef/graph/debug/ge_util.h"

namespace ge {
namespace fusion {
namespace {
std::unique_ptr<PatternMatcherConfig> BuildDefaultPatternMatcherConfig() {
  return PatternMatcherConfigBuilder().Build();
}

std::pair<std::unique_ptr<PythonPassHolder>, std::unique_ptr<PatternMatcherConfig>>
PreparePythonPatternPassAdapterInit(const PythonPassDescriptor &pass_desc) {
  std::unique_ptr<PatternMatcherConfig> match_config;
  auto holder = ComGraphMakeUnique<PythonPassHolder>(pass_desc);
  if ((holder == nullptr) || (!holder->IsValid())) {
    return std::make_pair(std::move(holder), std::move(match_config));
  }

  const auto &callbacks = holder->GetCallbacks();
  if (callbacks.get_matcher_config == nullptr) {
    return std::make_pair(std::move(holder), std::move(match_config));
  }

  const auto ret = callbacks.get_matcher_config(holder->GetHolder(), match_config);
  if (ret == SUCCESS) {
    return std::make_pair(std::move(holder), std::move(match_config));
  }

  GELOGW("Python pattern fusion adapter failed to get matcher config, ret[%u].", static_cast<uint32_t>(ret));
  return std::make_pair(std::unique_ptr<PythonPassHolder>(), std::unique_ptr<PatternMatcherConfig>());
}
}  // namespace

PythonPatternFusionPassAdapter::PythonPatternFusionPassAdapter(const PythonPassDescriptor &pass_desc)
    : PythonPatternFusionPassAdapter(PreparePythonPatternPassAdapterInit(pass_desc)) {}

PythonPatternFusionPassAdapter::PythonPatternFusionPassAdapter(
    std::pair<std::unique_ptr<PythonPassHolder>, std::unique_ptr<PatternMatcherConfig>> init)
    : PatternFusionPass(init.second == nullptr ? BuildDefaultPatternMatcherConfig() : std::move(init.second)),
      holder_(std::move(init.first)) {}

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
  GELOGI("PythonPatternFusionPassAdapter::Patterns begin for pass[%s].",
         holder_->GetPassDescriptor().pass_name.c_str());
  std::vector<PatternUniqPtr> patterns;
  const auto ret = holder_->GetCallbacks().patterns(holder_->GetHolder(), patterns);
  if (ret != SUCCESS) {
    GELOGW("Python pattern fusion adapter Patterns callback failed, ret[%u].", static_cast<uint32_t>(ret));
    return {};
  }
  GELOGI("PythonPatternFusionPassAdapter::Patterns end for pass[%s], pattern_count[%zu].",
         holder_->GetPassDescriptor().pass_name.c_str(),
         patterns.size());
  return patterns;
}

bool PythonPatternFusionPassAdapter::MeetRequirements(const std::unique_ptr<MatchResult> &match_result) {
  if ((holder_ == nullptr) || (!holder_->IsValid()) || (holder_->GetHolder() == nullptr)) {
    return false;
  }
  if (holder_->GetCallbacks().meet_requirements == nullptr) {
    return PatternFusionPass::MeetRequirements(match_result);
  }
  const bool ret = holder_->GetCallbacks().meet_requirements(holder_->GetHolder(), match_result);
  GELOGI("PythonPatternFusionPassAdapter::MeetRequirements for pass[%s] returned[%d].",
         holder_->GetPassDescriptor().pass_name.c_str(),
         ret ? 1 : 0);
  return ret;
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
  GELOGI("PythonPatternFusionPassAdapter::Replacement succeeded for pass[%s], graph[%p].",
         holder_->GetPassDescriptor().pass_name.c_str(),
         replacement_graph.get());
  return replacement_graph;
}
}  // namespace fusion
}  // namespace ge
