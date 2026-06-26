/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef COMPILER_GRAPH_FUSION_PASS_DECOMPOSE_PASS_RUN_H
#define COMPILER_GRAPH_FUSION_PASS_DECOMPOSE_PASS_RUN_H

#include <functional>
#include <memory>
#include <vector>

#include "ge/fusion/pass/decompose_pass.h"
#include "graph/graph.h"
#include "register/register_custom_pass.h"

namespace ge {
namespace fusion {
using DecomposeMeetRequirementsFn = std::function<bool(const GNode &)>;
using DecomposeReplacementFn = std::function<GraphUniqPtr(const GNode &)>;

// DecomposePass V1/V2 共享的 match-filter-replace 主循环。V1/V2 仅在钩子绑定上区分，
// loop 本身在此实现以避免重复。
Status RunDecomposePass(GraphPtr &graph, CustomPassContext &pass_context, const std::vector<AscendString> &op_types,
                        const DecomposeMeetRequirementsFn &meet_requirements,
                        const DecomposeReplacementFn &replacement);
}  // namespace fusion
}  // namespace ge
#endif  // COMPILER_GRAPH_FUSION_PASS_DECOMPOSE_PASS_RUN_H
