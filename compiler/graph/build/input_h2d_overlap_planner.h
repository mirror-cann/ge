/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_PLANNER_H_
#define GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_PLANNER_H_

#include "graph/build/input_h2d_overlap_plan.h"
#include "graph/compute_graph.h"

namespace ge {
class Model;
class StreamAllocator;

Status IsInputH2DOverlapEnabled(bool &enabled);

class InputH2DOverlapPlanner {
 public:
  Status Prepare(const ComputeGraphPtr &compute_graph, StreamAllocator &stream_allocator);
  Status AddCopyStream(int64_t &stream_num);
  Status SaveToModel(const StreamAllocator &stream_allocator, int64_t stream_num, int64_t event_num, Model &model);

 private:
  input_h2d_overlap::InputH2DOverlapPendingPlan plan_;
};
}  // namespace ge

#endif  // GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_PLANNER_H_
