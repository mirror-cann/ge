/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/input_h2d_overlap_plan.h"

#include <algorithm>
#include <cinttypes>
#include <deque>
#include <limits>
#include <map>
#include <unordered_set>
#include <utility>

#include "common/checker.h"
#include "common/ge_common/ge_types.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "framework/common/debug/ge_log.h"
#include "framework/common/framework_types_internal.h"
#include "graph/build/input_h2d_overlap_constants.h"
#include "graph/build/stream/graph_stream_allocator.h"
#include "graph/anchor.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/op_type_utils.h"
#include "graph/utils/tensor_utils.h"
#include "proto/task.pb.h"

namespace ge {
namespace input_h2d_overlap {
namespace {
constexpr uint32_t kDataOutputIndex = 0U;
constexpr int64_t kInvalidTopoIndex = -1;
constexpr int64_t kInputH2DOverlapInvalidStreamId = -1;
constexpr const char *kInputH2DOverlapPlanAttrVersion = "version";
constexpr const char *kInputH2DOverlapPlanAttrCopyStreamId = "copy_stream_id";
constexpr const char *kInputH2DOverlapPlanAttrGroups = "groups";
constexpr const char *kInputH2DOverlapPlanAttrInputs = "inputs";
constexpr const char *kInputH2DOverlapPlanAttrWaitPoints = "wait_points";
constexpr const char *kInputH2DOverlapPlanAttrInputIndex = "input_index";
constexpr const char *kInputH2DOverlapPlanAttrSize = "size";
constexpr const char *kInputH2DOverlapPlanAttrStreamId = "stream_id";
constexpr const char *kInputH2DOverlapPlanAttrEventId = "event_id";
constexpr const char *kInputH2DOverlapPlanAttrWaitTaskId = "wait_task_id";

using NodeToTopoIndex = std::map<NodePtr, int64_t>;

NodeToTopoIndex BuildTopoIndex(const ComputeGraphPtr &compute_graph) {
  NodeToTopoIndex topo_index;
  int64_t index = 0;
  for (const auto &node : compute_graph->GetDirectNode()) {
    if (node == nullptr) {
      continue;
    }
    topo_index.emplace(node, index);
    ++index;
  }
  return topo_index;
}

bool IsWaitPointBefore(const InputH2DOverlapWaitPoint &lhs, const InputH2DOverlapWaitPoint &rhs) {
  if (lhs.consumer_topo_index != rhs.consumer_topo_index) {
    if (lhs.consumer_topo_index == kInvalidTopoIndex) {
      return false;
    }
    if (rhs.consumer_topo_index == kInvalidTopoIndex) {
      return true;
    }
    return lhs.consumer_topo_index < rhs.consumer_topo_index;
  }
  return lhs.consumer_node_name < rhs.consumer_node_name;
}

}  // namespace

bool IsTaskConsumerNode(const NodePtr &node) {
  if (node == nullptr) {
    return false;
  }
  const auto op_desc = node->GetOpDesc();
  if (op_desc == nullptr) {
    return false;
  }
  bool no_task = false;
  if ((AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, no_task) && no_task) ||
      (op_desc->GetOpKernelLibName() == kEngineNameGeLocal)) {
    return false;
  }
  const auto &type = op_desc->GetType();
  return (!OpTypeUtils::IsDataNode(type)) && (type != NETOUTPUT) &&
         (op_desc->GetStreamId() != kInputH2DOverlapInvalidStreamId);
}

namespace {

InputH2DOverlapWaitPoint BuildWaitPoint(const uint32_t input_index, const NodePtr &node,
                                        const NodeToTopoIndex &topo_index) {
  InputH2DOverlapWaitPoint wait_point;
  wait_point.input_index = input_index;
  wait_point.consumer_node_name = node->GetName();
  wait_point.consumer_node = node;
  wait_point.logical_stream_id = node->GetOpDesc()->GetStreamId();
  const auto iter = topo_index.find(node);
  wait_point.consumer_topo_index = (iter == topo_index.end()) ? kInvalidTopoIndex : iter->second;
  return wait_point;
}

void EnqueuePeerNodes(const OutDataAnchor *const out_anchor, std::deque<NodePtr> &pending_nodes) {
  if (out_anchor == nullptr) {
    return;
  }
  for (const auto &peer_in_anchor : out_anchor->GetPeerInDataAnchorsPtr()) {
    if (peer_in_anchor == nullptr) {
      continue;
    }
    const auto peer_node = peer_in_anchor->GetOwnerNode();
    if (peer_node == nullptr) {
      continue;
    }
    pending_nodes.emplace_back(peer_node);
  }
}

void EnqueuePeerNodes(const OutDataAnchorPtr &out_anchor, std::deque<NodePtr> &pending_nodes) {
  EnqueuePeerNodes(out_anchor.get(), pending_nodes);
}

std::vector<InputH2DOverlapWaitPoint> FindFirstConsumerWaitPoint(const NodePtr &data_node, const uint32_t input_index,
                                                                 const NodeToTopoIndex &topo_index) {
  std::vector<InputH2DOverlapWaitPoint> wait_points;
  std::deque<NodePtr> pending_nodes;
  std::unordered_set<NodePtr> visited_nodes;

  EnqueuePeerNodes(data_node->GetOutDataAnchor(kDataOutputIndex), pending_nodes);
  while (!pending_nodes.empty()) {
    const auto node = pending_nodes.front();
    pending_nodes.pop_front();
    if ((node == nullptr) || (!visited_nodes.insert(node).second)) {
      continue;
    }
    if (IsTaskConsumerNode(node)) {
      const auto wait_point = BuildWaitPoint(input_index, node, topo_index);
      if (wait_points.empty() || IsWaitPointBefore(wait_point, wait_points[0U])) {
        wait_points.clear();
        wait_points.emplace_back(wait_point);
      }
      continue;
    }
    for (const auto &out_anchor : node->GetAllOutDataAnchorsPtr()) {
      EnqueuePeerNodes(out_anchor, pending_nodes);
    }
  }

  return wait_points;
}

bool GetDataInputIndex(const OpDescPtr &op_desc, const uint32_t data_op_index, uint32_t &input_index) {
  int64_t input_index_attr = static_cast<int64_t>(data_op_index);
  (void)AttrUtils::GetInt(op_desc, ATTR_NAME_INDEX, input_index_attr);
  if (input_index_attr < 0) {
    GELOGW("[InputH2DOverlap] skip data node:%s because input index:%ld is invalid.", op_desc->GetNamePtr(),
           input_index_attr);
    return false;
  }
  input_index = static_cast<uint32_t>(input_index_attr);
  return true;
}

bool GetDataOutputSize(const OpDescPtr &op_desc, int64_t &size) {
  const auto output_offsets = op_desc->GetOutputOffset();
  if (output_offsets.empty()) {
    GELOGD("[InputH2DOverlap] skip data node:%s because output offset is empty.", op_desc->GetNamePtr());
    return false;
  }
  const auto output_desc = op_desc->GetOutputDescPtr(kDataOutputIndex);
  if (output_desc == nullptr) {
    GELOGD("[InputH2DOverlap] skip data node:%s because output0 desc is null.", op_desc->GetNamePtr());
    return false;
  }
  size = 0;
  if ((TensorUtils::GetSize(*output_desc, size) != GRAPH_SUCCESS) || (size <= 0)) {
    size = 0;
    (void)TensorUtils::GetTensorSizeInBytes(*output_desc, size);
  }
  if (size <= 0) {
    GELOGD("[InputH2DOverlap] skip data node:%s because output0 size:%ld is invalid.", op_desc->GetNamePtr(), size);
    return false;
  }
  return true;
}

bool IsTopoIndexBefore(const int64_t lhs, const int64_t rhs) {
  if (lhs == rhs) {
    return false;
  }
  if (lhs == kInvalidTopoIndex) {
    return false;
  }
  if (rhs == kInvalidTopoIndex) {
    return true;
  }
  return lhs < rhs;
}

int64_t GetEarliestConsumerTopoIndex(const InputH2DOverlapCopyGroup &group) {
  int64_t earliest_topo_index = kInvalidTopoIndex;
  for (const auto &wait_point : group.wait_points) {
    if (IsTopoIndexBefore(wait_point.consumer_topo_index, earliest_topo_index)) {
      earliest_topo_index = wait_point.consumer_topo_index;
    }
  }
  return earliest_topo_index;
}

uint32_t GetFirstInputIndex(const InputH2DOverlapCopyGroup &group) {
  uint32_t first_input_index = std::numeric_limits<uint32_t>::max();
  for (const auto &input : group.inputs) {
    first_input_index = std::min(first_input_index, input.input_index);
  }
  return first_input_index;
}

bool IsCopyGroupBefore(const InputH2DOverlapCopyGroup &lhs, const InputH2DOverlapCopyGroup &rhs) {
  const auto lhs_earliest_topo_index = GetEarliestConsumerTopoIndex(lhs);
  const auto rhs_earliest_topo_index = GetEarliestConsumerTopoIndex(rhs);
  if (IsTopoIndexBefore(lhs_earliest_topo_index, rhs_earliest_topo_index)) {
    return true;
  }
  if (IsTopoIndexBefore(rhs_earliest_topo_index, lhs_earliest_topo_index)) {
    return false;
  }
  const auto lhs_first_input_index = GetFirstInputIndex(lhs);
  const auto rhs_first_input_index = GetFirstInputIndex(rhs);
  if (lhs_first_input_index != rhs_first_input_index) {
    return lhs_first_input_index < rhs_first_input_index;
  }
  if (lhs.inputs.empty() || rhs.inputs.empty()) {
    return lhs.inputs.size() < rhs.inputs.size();
  }
  return lhs.inputs.front().data_node_name < rhs.inputs.front().data_node_name;
}

Status AddInputToCopyGroup(const InputH2DOverlapCandidateInput &input, const InputH2DOverlapCopyInput &copy_item,
                           InputH2DOverlapCopyGroupPlan &group_plan) {
  GE_ASSERT_TRUE(!input.wait_points.empty(), "input:%u has no wait point.", input.input_index);

  InputH2DOverlapCopyGroup group;
  group.inputs.emplace_back(copy_item);
  group.wait_points = input.wait_points;
  group_plan.groups.emplace_back(std::move(group));
  return SUCCESS;
}
}  // namespace

Status AnalyzeInputs(const ComputeGraphPtr &compute_graph, InputH2DOverlapCandidatePlan &plan) {
  plan.inputs.clear();
  GE_CHECK_NOTNULL(compute_graph);

  const auto topo_index = BuildTopoIndex(compute_graph);
  uint32_t data_op_index = 0U;
  for (const auto &node : compute_graph->GetDirectNode()) {
    if (node == nullptr) {
      continue;
    }
    const auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      GELOGW("[InputH2DOverlap] skip node:%s because op desc is null.", node->GetNamePtr());
      continue;
    }
    if (!OpTypeUtils::IsDataNode(op_desc->GetType())) {
      continue;
    }

    uint32_t input_index = 0U;
    const bool has_valid_index = GetDataInputIndex(op_desc, data_op_index, input_index);
    ++data_op_index;
    if (!has_valid_index) {
      continue;
    }

    int64_t size = 0;
    if (!GetDataOutputSize(op_desc, size)) {
      continue;
    }

    InputH2DOverlapCandidateInput candidate_input;
    candidate_input.input_index = input_index;
    candidate_input.data_node_name = node->GetName();
    candidate_input.size = size;
    candidate_input.wait_points = FindFirstConsumerWaitPoint(node, input_index, topo_index);
    if (candidate_input.wait_points.empty()) {
      GELOGD("[InputH2DOverlap] skip data node:%s because no consumer wait point is found.", op_desc->GetNamePtr());
      continue;
    }
    plan.inputs.emplace_back(candidate_input);
  }

  std::sort(plan.inputs.begin(), plan.inputs.end(),
            [](const InputH2DOverlapCandidateInput &lhs, const InputH2DOverlapCandidateInput &rhs) {
              if (lhs.input_index != rhs.input_index) {
                return lhs.input_index < rhs.input_index;
              }
              return lhs.data_node_name < rhs.data_node_name;
            });
  return SUCCESS;
}

Status SelectInputs(const InputH2DOverlapCandidatePlan &plan, InputH2DOverlapCopyGroupPlan &group_plan) {
  group_plan.groups.clear();

  std::unordered_set<uint32_t> selected_input_indexes;
  for (const auto &input : plan.inputs) {
    if (!selected_input_indexes.emplace(input.input_index).second) {
      GELOGE(PARAM_INVALID, "[Check][InputH2DOverlap] duplicated selected input index:%u.", input.input_index);
      return PARAM_INVALID;
    }
    GE_ASSERT_TRUE(!input.wait_points.empty(), "selected input:%u has no wait point.", input.input_index);

    InputH2DOverlapCopyInput copy_item;
    copy_item.input_index = input.input_index;
    copy_item.data_node_name = input.data_node_name;
    copy_item.size = static_cast<uint64_t>(input.size);
    GELOGD("[InputH2DOverlap] admit input:%u, data:%s as runtime-resolved input.", input.input_index,
           input.data_node_name.c_str());
    GE_ASSERT_SUCCESS(AddInputToCopyGroup(input, copy_item, group_plan), "[Add][InputH2DOverlapCopyGroup] failed.");
  }
  std::stable_sort(group_plan.groups.begin(), group_plan.groups.end(), IsCopyGroupBefore);
  return SUCCESS;
}

Status BuildPendingPlan(const InputH2DOverlapCopyGroupPlan &group_plan, InputH2DOverlapPendingPlan &pending_plan) {
  pending_plan = InputH2DOverlapPendingPlan();
  pending_plan.copy_group_plan = group_plan;
  for (size_t group_index = 0U; group_index < group_plan.groups.size(); ++group_index) {
    const auto &group = group_plan.groups[group_index];
    if (group.inputs.empty()) {
      GELOGE(PARAM_INVALID, "[Check][InputH2DOverlap] copy group:%zu has no input.", group_index);
      return PARAM_INVALID;
    }
    if (group.wait_points.empty()) {
      GELOGE(PARAM_INVALID, "[Check][InputH2DOverlap] copy group:%zu has no wait point.", group_index);
      return PARAM_INVALID;
    }
    for (const auto &wait_point : group.wait_points) {
      GE_CHECK_NOTNULL(wait_point.consumer_node);
      InputH2DOverlapWaitRequest wait_request;
      wait_request.copy_group_index = static_cast<uint32_t>(group_index);
      wait_request.input_index = wait_point.input_index;
      wait_request.consumer_node_name = wait_point.consumer_node_name;
      wait_request.consumer_node = wait_point.consumer_node;
      wait_request.logical_stream_id = wait_point.logical_stream_id;
      wait_request.consumer_topo_index = wait_point.consumer_topo_index;
      pending_plan.wait_requests.emplace_back(wait_request);
    }
  }
  return SUCCESS;
}

Status AddCopyStream(InputH2DOverlapPendingPlan &pending_plan, int64_t &stream_num) {
  GE_ASSERT_TRUE(!pending_plan.empty(), "pending plan is empty.");
  GE_ASSERT_TRUE(pending_plan.copy_stream_id == UINT32_MAX, "copy stream id is already assigned:%u.",
                 pending_plan.copy_stream_id);
  GE_ASSERT_TRUE(stream_num > 0, "invalid stream num:%" PRId64 ".", stream_num);
  GE_ASSERT_TRUE(stream_num < static_cast<int64_t>(std::numeric_limits<uint32_t>::max()),
                 "stream num:%" PRId64 " exceeds uint32 range.", stream_num);

  pending_plan.copy_stream_id = static_cast<uint32_t>(stream_num);
  ++stream_num;
  GELOGI("[InputH2DOverlap] append copy stream:%u, stream num:%" PRId64 ".", pending_plan.copy_stream_id, stream_num);
  return SUCCESS;
}

Status RegisterWaits(const InputH2DOverlapPendingPlan &pending_plan, StreamAllocator &stream_allocator) {
  GE_ASSERT_TRUE(pending_plan.empty() || (!pending_plan.wait_requests.empty()), "pending plan has no wait request.");
  for (const auto &wait_request : pending_plan.wait_requests) {
    GE_CHECK_NOTNULL(wait_request.consumer_node);
    GE_CHK_STATUS_RET(stream_allocator.AddExternalWaitRequest(wait_request.consumer_node),
                      "[Add][ExternalWaitRequest] failed, group:%u, input:%u, consumer:%s.",
                      wait_request.copy_group_index, wait_request.input_index, wait_request.consumer_node_name.c_str());
  }
  GE_ASSERT_TRUE(stream_allocator.GetStandaloneWaitEvents().size() == pending_plan.wait_requests.size(),
                 "registered wait request count:%zu does not match expected:%zu.",
                 stream_allocator.GetStandaloneWaitEvents().size(), pending_plan.wait_requests.size());
  return SUCCESS;
}

Status BuildFinalPlan(const InputH2DOverlapPendingPlan &pending_plan, const StreamAllocator &stream_allocator,
                      const uint32_t copy_stream_id, const uint32_t stream_num, const uint32_t event_num,
                      const domi::ModelTaskDef &model_task_def, InputH2DOverlapFinalPlan &final_plan) {
  final_plan = InputH2DOverlapFinalPlan();
  GE_ASSERT_TRUE(copy_stream_id != UINT32_MAX, "copy stream id is not assigned.");
  GE_ASSERT_TRUE(copy_stream_id < stream_num, "copy stream id:%u exceeds stream num:%u.", copy_stream_id, stream_num);
  const auto &wait_events = stream_allocator.GetStandaloneWaitEvents();
  GE_ASSERT_TRUE(wait_events.size() == pending_plan.wait_requests.size(),
                 "registered wait count:%zu does not match pending count:%zu.", wait_events.size(),
                 pending_plan.wait_requests.size());

  final_plan.version = pending_plan.version;
  final_plan.copy_stream_id = copy_stream_id;
  final_plan.groups.resize(pending_plan.copy_group_plan.groups.size());
  for (size_t group_index = 0U; group_index < pending_plan.copy_group_plan.groups.size(); ++group_index) {
    final_plan.groups[group_index].inputs = pending_plan.copy_group_plan.groups[group_index].inputs;
  }

  for (size_t i = 0U; i < pending_plan.wait_requests.size(); ++i) {
    const auto &wait_request = pending_plan.wait_requests[i];
    const auto &wait_event = wait_events[i];
    GE_CHECK_NOTNULL(wait_request.consumer_node);
    GE_CHECK_NOTNULL(wait_event.consumer_node);
    GE_ASSERT_TRUE(wait_event.consumer_node == wait_request.consumer_node,
                   "registered wait event node:%s does not match request node:%s.",
                   wait_event.consumer_node->GetNamePtr(), wait_request.consumer_node_name.c_str());
    GE_ASSERT_TRUE(wait_event.event_id != UINT32_MAX, "wait event for node:%s is not assigned.",
                   wait_request.consumer_node_name.c_str());
    GE_ASSERT_TRUE(wait_request.copy_group_index < final_plan.groups.size(),
                   "wait request copy group index:%u exceeds group count:%zu.", wait_request.copy_group_index,
                   final_plan.groups.size());
    GE_ASSERT_TRUE(wait_event.event_id < event_num, "wait event id:%u exceeds event num:%u.", wait_event.event_id,
                   event_num);

    bool found_wait_task = false;
    InputH2DOverlapFinalWaitPoint final_wait_point;
    for (int32_t task_index = 0; task_index < model_task_def.task_size(); ++task_index) {
      const auto &task_def = model_task_def.task(task_index);
      if ((task_def.type() != static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT)) ||
          (task_def.event_id() != wait_event.event_id)) {
        continue;
      }
      GE_ASSERT_TRUE(!found_wait_task, "duplicated wait task for event id:%u.", wait_event.event_id);
      GE_ASSERT_TRUE(task_def.stream_id() != copy_stream_id, "wait task stream id:%u equals copy stream id.",
                     task_def.stream_id());
      final_wait_point.stream_id = task_def.stream_id();
      final_wait_point.event_id = task_def.event_id();
      final_wait_point.wait_task_id = static_cast<uint32_t>(task_index);
      found_wait_task = true;
    }
    GE_ASSERT_TRUE(found_wait_task, "wait task for event id:%u is not found.", wait_event.event_id);

    final_plan.groups[wait_request.copy_group_index].wait_points.emplace_back(final_wait_point);
  }
  return SUCCESS;
}

Status SetInputH2DOverlapPlanIntAttr(NamedAttrs &attrs, const char *const name, const uint64_t value) {
  GE_ASSERT_TRUE(value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()),
                 "attr:%s value:%" PRIu64 " exceeds int64 max.", name, value);
  GE_ASSERT_TRUE(AttrUtils::SetInt(attrs, name, static_cast<int64_t>(value)), "[Set][Attr] %s failed.", name);
  return SUCCESS;
}

Status BuildInputH2DOverlapInputAttr(const InputH2DOverlapCopyInput &input, NamedAttrs &input_attr) {
  input_attr.SetName("input");
  GE_CHK_STATUS_RET(SetInputH2DOverlapPlanIntAttr(input_attr, kInputH2DOverlapPlanAttrInputIndex, input.input_index),
                    "[Set][InputH2DOverlapInputIndex] failed.");
  GE_CHK_STATUS_RET(SetInputH2DOverlapPlanIntAttr(input_attr, kInputH2DOverlapPlanAttrSize, input.size),
                    "[Set][InputH2DOverlapInputSize] failed.");
  return SUCCESS;
}

Status BuildInputH2DOverlapWaitPointAttr(const InputH2DOverlapFinalWaitPoint &wait_point, NamedAttrs &wait_point_attr) {
  wait_point_attr.SetName("wait_point");
  GE_CHK_STATUS_RET(
      SetInputH2DOverlapPlanIntAttr(wait_point_attr, kInputH2DOverlapPlanAttrStreamId, wait_point.stream_id),
      "[Set][InputH2DOverlapWaitStreamId] failed.");
  GE_CHK_STATUS_RET(
      SetInputH2DOverlapPlanIntAttr(wait_point_attr, kInputH2DOverlapPlanAttrEventId, wait_point.event_id),
      "[Set][InputH2DOverlapWaitEventId] failed.");
  GE_CHK_STATUS_RET(
      SetInputH2DOverlapPlanIntAttr(wait_point_attr, kInputH2DOverlapPlanAttrWaitTaskId, wait_point.wait_task_id),
      "[Set][InputH2DOverlapWaitTaskId] failed.");
  return SUCCESS;
}

Status SerializePlan(const InputH2DOverlapFinalPlan &final_plan, NamedAttrs &plan_attr) {
  plan_attr = NamedAttrs();
  GE_ASSERT_TRUE(!final_plan.empty(), "final plan is empty.");
  GE_ASSERT_TRUE(final_plan.copy_stream_id != UINT32_MAX, "final plan copy stream id is not assigned.");

  std::unordered_set<uint32_t> planned_input_indexes;
  std::unordered_set<uint32_t> planned_event_ids;
  for (size_t group_index = 0U; group_index < final_plan.groups.size(); ++group_index) {
    const auto &group = final_plan.groups[group_index];
    GE_ASSERT_TRUE((!group.inputs.empty()) && (!group.wait_points.empty()),
                   "final copy group:%zu input or wait point is empty.", group_index);
    for (const auto &input : group.inputs) {
      GE_ASSERT_TRUE(input.size > 0U, "invalid final input, group:%zu, input:%u, size:%" PRIu64 ".", group_index,
                     input.input_index, input.size);
      GE_ASSERT_TRUE(planned_input_indexes.emplace(input.input_index).second, "duplicated final input index:%u.",
                     input.input_index);
    }
    for (const auto &wait_point : group.wait_points) {
      GE_ASSERT_TRUE((wait_point.stream_id != UINT32_MAX) && (wait_point.event_id != UINT32_MAX) &&
                         (wait_point.wait_task_id != UINT32_MAX),
                     "invalid final wait point, group:%zu, stream:%u, event:%u, task:%u.", group_index,
                     wait_point.stream_id, wait_point.event_id, wait_point.wait_task_id);
      GE_ASSERT_TRUE(planned_event_ids.emplace(wait_point.event_id).second, "duplicated final wait event id:%u.",
                     wait_point.event_id);
    }
  }

  plan_attr.SetName("input_h2d_overlap_plan");
  GE_CHK_STATUS_RET(SetInputH2DOverlapPlanIntAttr(plan_attr, kInputH2DOverlapPlanAttrVersion, final_plan.version),
                    "[Set][InputH2DOverlapPlanVersion] failed.");
  GE_CHK_STATUS_RET(
      SetInputH2DOverlapPlanIntAttr(plan_attr, kInputH2DOverlapPlanAttrCopyStreamId, final_plan.copy_stream_id),
      "[Set][InputH2DOverlapCopyStreamId] failed.");

  std::vector<NamedAttrs> group_attrs;
  group_attrs.reserve(final_plan.groups.size());
  for (const auto &group : final_plan.groups) {
    NamedAttrs group_attr;
    group_attr.SetName("copy_group");

    std::vector<NamedAttrs> input_attrs;
    input_attrs.reserve(group.inputs.size());
    for (const auto &input : group.inputs) {
      NamedAttrs input_attr;
      GE_CHK_STATUS_RET(BuildInputH2DOverlapInputAttr(input, input_attr), "[Build][InputH2DOverlapInputAttr] failed.");
      input_attrs.emplace_back(std::move(input_attr));
    }
    if (!AttrUtils::SetListNamedAttrs(group_attr, kInputH2DOverlapPlanAttrInputs, input_attrs)) {
      GELOGE(INTERNAL_ERROR, "[Set][InputH2DOverlap] group inputs attr failed.");
      return INTERNAL_ERROR;
    }

    std::vector<NamedAttrs> wait_point_attrs;
    wait_point_attrs.reserve(group.wait_points.size());
    for (const auto &wait_point : group.wait_points) {
      NamedAttrs wait_point_attr;
      GE_CHK_STATUS_RET(BuildInputH2DOverlapWaitPointAttr(wait_point, wait_point_attr),
                        "[Build][InputH2DOverlapWaitPointAttr] failed.");
      wait_point_attrs.emplace_back(std::move(wait_point_attr));
    }
    if (!AttrUtils::SetListNamedAttrs(group_attr, kInputH2DOverlapPlanAttrWaitPoints, wait_point_attrs)) {
      GELOGE(INTERNAL_ERROR, "[Set][InputH2DOverlap] group wait points attr failed.");
      return INTERNAL_ERROR;
    }
    group_attrs.emplace_back(std::move(group_attr));
  }
  if (!AttrUtils::SetListNamedAttrs(plan_attr, kInputH2DOverlapPlanAttrGroups, group_attrs)) {
    GELOGE(INTERNAL_ERROR, "[Set][InputH2DOverlap] plan groups attr failed.");
    return INTERNAL_ERROR;
  }
  return SUCCESS;
}

}  // namespace input_h2d_overlap
}  // namespace ge
