/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ge/fusion/pass/pattern_fusion_pass.h"

#include "graph/fusion/pass/pattern_fusion_run.h"

namespace ge {
namespace fusion {
PatternFusionPassV2::PatternFusionPassV2() : match_config_(PatternMatcherConfigBuilder().Build()) {}
PatternFusionPassV2::PatternFusionPassV2(std::unique_ptr<PatternMatcherConfig> match_config)
    : match_config_(std::move(match_config)) {}

Status PatternFusionPassV2::Run(GraphPtr &graph, CustomPassContext &pass_context) {
  return RunPatternFusion(
      graph, pass_context, *match_config_, Patterns(),
      [this, &pass_context](const std::unique_ptr<MatchResult> &match_result) {
        return MeetRequirements(match_result, pass_context);
      },
      [this, &pass_context](const std::unique_ptr<MatchResult> &match_result) {
        return Replacement(match_result, pass_context);
      });
}

bool PatternFusionPassV2::MeetRequirements(const std::unique_ptr<MatchResult> &match_result,
                                           CustomPassContext &pass_context) {
  (void)match_result;
  (void)pass_context;
  return true;
}
}  // namespace fusion
}  // namespace ge
