/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_PLAN_H_
#define GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_PLAN_H_

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "ge/ge_api_types.h"
#include "graph/compute_graph.h"

namespace domi {
class ModelTaskDef;
}

namespace ge {
class NamedAttrs;
class StreamAllocator;

namespace input_h2d_overlap {
struct InputH2DOverlapWaitPoint {
  uint32_t input_index = 0U;
  std::string consumer_node_name;
  NodePtr consumer_node;
  int64_t logical_stream_id = -1;
  int64_t consumer_topo_index = -1;
};

struct InputH2DOverlapCandidateInput {
  uint32_t input_index = 0U;
  std::string data_node_name;
  int64_t size = 0;
  std::vector<InputH2DOverlapWaitPoint> wait_points;
};

struct InputH2DOverlapCandidatePlan {
  std::vector<InputH2DOverlapCandidateInput> inputs;

  bool empty() const {
    return inputs.empty();
  }

  size_t WaitPointCount() const {
    size_t count = 0U;
    for (const auto &input : inputs) {
      count += input.wait_points.size();
    }
    return count;
  }
};

struct InputH2DOverlapCopyInput {
  uint32_t input_index = 0U;
  std::string data_node_name;
  uint64_t size = 0U;
};

struct InputH2DOverlapCopyGroup {
  std::vector<InputH2DOverlapCopyInput> inputs;
  std::vector<InputH2DOverlapWaitPoint> wait_points;
};

struct InputH2DOverlapCopyGroupPlan {
  std::vector<InputH2DOverlapCopyGroup> groups;

  bool empty() const {
    return groups.empty();
  }

  size_t InputCount() const {
    size_t count = 0U;
    for (const auto &group : groups) {
      count += group.inputs.size();
    }
    return count;
  }

  size_t WaitPointCount() const {
    size_t count = 0U;
    for (const auto &group : groups) {
      count += group.wait_points.size();
    }
    return count;
  }
};

struct InputH2DOverlapWaitRequest {
  uint32_t copy_group_index = 0U;
  uint32_t input_index = 0U;
  std::string consumer_node_name;
  NodePtr consumer_node;
  int64_t logical_stream_id = -1;
  int64_t consumer_topo_index = -1;
};

struct InputH2DOverlapPendingPlan {
  uint32_t version = 1U;
  uint32_t copy_stream_id = UINT32_MAX;
  InputH2DOverlapCopyGroupPlan copy_group_plan;
  std::vector<InputH2DOverlapWaitRequest> wait_requests;

  bool empty() const {
    return copy_group_plan.empty();
  }

  size_t InputCount() const {
    return copy_group_plan.InputCount();
  }

  size_t WaitRequestCount() const {
    return wait_requests.size();
  }
};

struct InputH2DOverlapFinalWaitPoint {
  uint32_t stream_id = UINT32_MAX;
  uint32_t event_id = UINT32_MAX;
  uint32_t wait_task_id = UINT32_MAX;
};

struct InputH2DOverlapFinalCopyGroup {
  std::vector<InputH2DOverlapCopyInput> inputs;
  std::vector<InputH2DOverlapFinalWaitPoint> wait_points;
};

struct InputH2DOverlapFinalPlan {
  uint32_t version = 1U;
  uint32_t copy_stream_id = UINT32_MAX;
  std::vector<InputH2DOverlapFinalCopyGroup> groups;

  bool empty() const {
    return groups.empty();
  }
};

Status AnalyzeInputs(const ComputeGraphPtr &compute_graph, InputH2DOverlapCandidatePlan &plan);
bool IsTaskConsumerNode(const NodePtr &node);
Status SelectInputs(const InputH2DOverlapCandidatePlan &plan, InputH2DOverlapCopyGroupPlan &group_plan);
Status BuildPendingPlan(const InputH2DOverlapCopyGroupPlan &group_plan, InputH2DOverlapPendingPlan &pending_plan);
Status AddCopyStream(InputH2DOverlapPendingPlan &pending_plan, int64_t &stream_num);
Status RegisterWaits(const InputH2DOverlapPendingPlan &pending_plan, StreamAllocator &stream_allocator);
Status BuildFinalPlan(const InputH2DOverlapPendingPlan &pending_plan, const StreamAllocator &stream_allocator,
                      uint32_t copy_stream_id, uint32_t stream_num, uint32_t event_num,
                      const domi::ModelTaskDef &model_task_def, InputH2DOverlapFinalPlan &final_plan);
Status SerializePlan(const InputH2DOverlapFinalPlan &final_plan, NamedAttrs &plan_attr);
}  // namespace input_h2d_overlap
}  // namespace ge

#endif  // GE_GRAPH_BUILD_INPUT_H2D_OVERLAP_PLAN_H_
