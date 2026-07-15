/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_CONSTANTS_H_
#define GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

#include "ge/ge_api_types.h"

namespace ge {
constexpr const char_t *kInputH2DOverlapOptionName = "ge.compile.h2dOverlappedWithCompute";
constexpr char kInputH2DOverlapPlanAttrName[] = "_ge_input_h2d_overlap_plan";
}  // namespace ge

#endif  // GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_CONSTANTS_H_
