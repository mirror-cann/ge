/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ge/fusion/pass/decompose_pass.h"

#include "graph/fusion/pass/decompose_pass_run.h"

namespace ge {
namespace fusion {
DecomposePass::DecomposePass(const std::vector<AscendString> &op_types) : op_types_(op_types) {}

Status DecomposePass::Run(GraphPtr &graph, CustomPassContext &pass_context) {
  return RunDecomposePass(
      graph, pass_context, op_types_, [this](const GNode &matched_node) { return MeetRequirements(matched_node); },
      [this](const GNode &matched_node) { return Replacement(matched_node); });
}

bool DecomposePass::MeetRequirements(const GNode &matched_node) {
  (void)matched_node;
  return true;
}
}  // namespace fusion
}  // namespace ge
