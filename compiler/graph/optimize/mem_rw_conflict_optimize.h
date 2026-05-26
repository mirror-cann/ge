/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_OPTIMIZE_MEM_RW_CONFLICT_OPTIMIZE_H_
#define GE_GRAPH_OPTIMIZE_MEM_RW_CONFLICT_OPTIMIZE_H_

#include <cstdint>

#include "framework/common/ge_inner_error_codes.h"
#include "graph/compute_graph.h"

namespace ge {

Status InitRWConflictCheck(const ComputeGraphPtr &compute_graph);

// Narrow query helpers used by graph passes that need RW conflict decisions without running the whole optimizer.
bool WouldDeleteTensorMoveCauseRWConflict(
    const NodePtr &src_node, uint32_t src_out_idx,
    const NodePtr &tm_succ, uint32_t tm_succ_in_idx);

bool IsNodeInputWritable(const NodePtr &node, uint32_t index);

}  // namespace ge

#endif  // GE_GRAPH_OPTIMIZE_MEM_RW_CONFLICT_OPTIMIZE_H_
