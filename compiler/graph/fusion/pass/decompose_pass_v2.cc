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
DecomposePassV2::DecomposePassV2(const std::vector<AscendString> &op_types) : op_types_(op_types) {}

Status DecomposePassV2::Run(GraphPtr &graph, CustomPassContext &pass_context) {
  return RunDecomposePass(
      graph, pass_context, op_types_,
      [this, &pass_context](const GNode &matched_node) { return MeetRequirements(matched_node, pass_context); },
      [this, &pass_context](const GNode &matched_node) { return Replacement(matched_node, pass_context); });
}

bool DecomposePassV2::MeetRequirements(const GNode &matched_node, CustomPassContext &pass_context) {
  (void)matched_node;
  (void)pass_context;
  return true;
}
}  // namespace fusion
}  // namespace ge
