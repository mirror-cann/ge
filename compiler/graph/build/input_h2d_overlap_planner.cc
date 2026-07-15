/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/input_h2d_overlap_planner.h"

#include <algorithm>
#include <cinttypes>
#include <cctype>
#include <limits>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/checker.h"
#include "common/proto_util/proto_util.h"
#include "framework/common/debug/ge_log.h"
#include "framework/common/framework_types_internal.h"
#include "graph/build/input_h2d_overlap_constants.h"
#include "graph/build/input_h2d_overlap_plan.h"
#include "graph/build/stream/graph_stream_allocator.h"
#include "graph/buffer.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/ge_context.h"
#include "graph/model.h"
#include "graph/utils/attr_utils.h"
#include "proto/task.pb.h"

namespace ge {
using namespace input_h2d_overlap;

namespace {
enum class InputH2DOverlapOption { kDisabled, kEnabled };

constexpr double kInputH2DOverlapDefaultComputeOpUs = 2.0;
constexpr double kInputH2DOverlapDefaultH2DPeakBandwidthGbps = 32.0;
constexpr double kInputH2DOverlapDefaultH2DLargeAlpha = 0.31;
constexpr double kInputH2DOverlapDefaultH2DMidAlpha = 0.15;
constexpr double kInputH2DOverlapDefaultH2DSmallUs = 6.0;
constexpr uint64_t kInputH2DOverlapDefaultH2DSmallThreshold = 64U * 1024U;
constexpr uint64_t kInputH2DOverlapDefaultH2DLargeThreshold = 8U * 1024U * 1024U;
constexpr double kInputH2DOverlapDefaultH2DItemUs = 16.0;
constexpr double kInputH2DOverlapDefaultEventUs = 1.0;
constexpr size_t kInputH2DOverlapDpMaxStateCount = 512U;
constexpr double kInputH2DOverlapBytesPerUsPerGbps = 1000.0;

struct InputH2DOverlapDpEstimation {
  double compute_op_us = kInputH2DOverlapDefaultComputeOpUs;
  double h2d_peak_bandwidth_gbps = kInputH2DOverlapDefaultH2DPeakBandwidthGbps;
  double h2d_large_alpha = kInputH2DOverlapDefaultH2DLargeAlpha;
  double h2d_mid_alpha = kInputH2DOverlapDefaultH2DMidAlpha;
  double h2d_small_us = kInputH2DOverlapDefaultH2DSmallUs;
  uint64_t h2d_small_threshold = kInputH2DOverlapDefaultH2DSmallThreshold;
  uint64_t h2d_large_threshold = kInputH2DOverlapDefaultH2DLargeThreshold;
  double h2d_item_us = kInputH2DOverlapDefaultH2DItemUs;
  double event_us = kInputH2DOverlapDefaultEventUs;
};

struct InputH2DOverlapConfig {
  std::vector<size_t> boundaries;
};

struct InputH2DOverlapDpState {
  double sync_us = 0.0;
  double worker_us = 0.0;
  double max_delay_us = 0.0;
  std::vector<std::pair<size_t, size_t>> planned_group_ranges;
};

struct InputH2DOverlapDpGroupEstimate {
  double legacy_us = 0.0;
  double planned_us = 0.0;
  double event_us = 0.0;
  double cover_us = 0.0;
  int64_t first_consumer_topo = -1;
};

struct InputH2DOverlapDpContext {
  std::vector<InputH2DOverlapDpGroupEstimate> group_estimates;
  std::vector<std::vector<double>> planned_interval_us;
  std::vector<std::vector<double>> event_interval_us;
  std::vector<double> compute_cover_to_group;
};

struct InputH2DOverlapDpAdmitStats {
  size_t before_group_count = 0U;
  size_t before_input_count = 0U;
  size_t before_wait_point_count = 0U;
  uint64_t before_total_bytes = 0U;
  size_t legacy_group_count = 0U;
  size_t legacy_input_count = 0U;
  uint64_t planned_bytes = 0U;
  uint64_t legacy_bytes = 0U;
};

int64_t GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(const InputH2DOverlapCopyGroup &group);
std::string BuildInputH2DOverlapInputSummary(const InputH2DOverlapCopyGroup &group);
std::string BuildInputH2DOverlapWaitSummary(const InputH2DOverlapCopyGroup &group);
void LogInputH2DOverlapGroups(const std::string &graph_name, const char *mode, size_t base_group_count,
                              const std::vector<std::pair<size_t, size_t>> &planned_ranges);
void LogInputH2DOverlapManualConfigHint(const std::string &graph_name,
                                        const std::vector<std::pair<size_t, size_t>> &ranges, size_t base_group_count);

const char *InputH2DOverlapOptionToString(const InputH2DOverlapOption option) {
  switch (option) {
    case InputH2DOverlapOption::kEnabled:
      return "enabled";
    default:
      return "disabled";
  }
}

Status ParseInputH2DOverlapOptionValue(const std::string &option_value, InputH2DOverlapOption &option,
                                       std::string &manual_config) {
  option = InputH2DOverlapOption::kDisabled;
  manual_config.clear();
  if (option_value.empty() || (option_value == "0")) {
    return SUCCESS;
  }
  option = InputH2DOverlapOption::kEnabled;
  if (option_value == "1") {
    return SUCCESS;
  }
  GE_ASSERT_TRUE(option_value.find(',') != std::string::npos,
                 "[Check][InputH2DOverlapOption] invalid option value:%s. Use 0, 1, or comma-separated manual "
                 "boundaries such as \"44,\" or \",44\".",
                 option_value.c_str());
  manual_config = option_value;
  return SUCCESS;
}

Status GetInputH2DOverlapOption(InputH2DOverlapOption &option, std::string &manual_config) {
  option = InputH2DOverlapOption::kDisabled;
  manual_config.clear();
  std::string option_value;
  if (GetContext().GetOption(kInputH2DOverlapOptionName, option_value) != SUCCESS) {
    return SUCCESS;
  }
  return ParseInputH2DOverlapOptionValue(option_value, option, manual_config);
}

bool IsInputH2DOverlapDynamicShapeGraph(const ComputeGraphPtr &compute_graph, std::string &dynamic_graph_name) {
  dynamic_graph_name.clear();
  std::unordered_set<const ComputeGraph *> visited_graphs;
  for (auto graph = compute_graph; graph != nullptr; graph = graph->GetParentGraph()) {
    if (!visited_graphs.insert(graph.get()).second) {
      break;
    }
    bool is_dynamic_shape_partitioned = false;
    (void)AttrUtils::GetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, is_dynamic_shape_partitioned);
    if (is_dynamic_shape_partitioned || graph->GetGraphUnknownFlag()) {
      dynamic_graph_name = graph->GetName();
      return true;
    }
  }
  return false;
}

uint64_t AccumulateInputH2DOverlapBytes(const uint64_t current_bytes, const uint64_t input_size) {
  const uint64_t max_bytes = std::numeric_limits<uint64_t>::max();
  if (input_size > (max_bytes - current_bytes)) {
    return max_bytes;
  }
  return current_bytes + input_size;
}

uint64_t GetInputH2DOverlapCopyGroupBytes(const InputH2DOverlapCopyGroup &group) {
  uint64_t group_bytes = 0U;
  for (const auto &input : group.inputs) {
    group_bytes = AccumulateInputH2DOverlapBytes(group_bytes, input.size);
  }
  return group_bytes;
}

uint64_t GetInputH2DOverlapPlannedTotalBytes(const InputH2DOverlapCopyGroupPlan &group_plan) {
  uint64_t planned_total_bytes = 0U;
  for (const auto &group : group_plan.groups) {
    planned_total_bytes = AccumulateInputH2DOverlapBytes(planned_total_bytes, GetInputH2DOverlapCopyGroupBytes(group));
  }
  return planned_total_bytes;
}

uint64_t GetInputH2DOverlapNonFirstGroupBytes(const InputH2DOverlapCopyGroupPlan &group_plan) {
  uint64_t non_first_group_bytes = 0U;
  for (size_t group_index = 1U; group_index < group_plan.groups.size(); ++group_index) {
    non_first_group_bytes = AccumulateInputH2DOverlapBytes(
        non_first_group_bytes, GetInputH2DOverlapCopyGroupBytes(group_plan.groups[group_index]));
  }
  return non_first_group_bytes;
}

std::string TrimInputH2DOverlapString(const std::string &value) {
  size_t begin = 0U;
  while ((begin < value.size()) && (std::isspace(static_cast<unsigned char>(value[begin])) != 0)) {
    ++begin;
  }
  size_t end = value.size();
  while ((end > begin) && (std::isspace(static_cast<unsigned char>(value[end - 1U])) != 0)) {
    --end;
  }
  return value.substr(begin, end - begin);
}

Status SplitInputH2DOverlapManualConfig(const std::string &manual_config, const char delimiter,
                                        std::vector<std::string> &tokens) {
  tokens.clear();
  size_t empty_edge_token_count = 0U;
  size_t begin = 0U;
  while (begin <= manual_config.size()) {
    const size_t end = manual_config.find(delimiter, begin);
    const auto token = TrimInputH2DOverlapString(
        manual_config.substr(begin, (end == std::string::npos) ? std::string::npos : end - begin));
    if (token.empty()) {
      const bool is_edge_token = (begin == 0U) || (end == std::string::npos);
      GE_ASSERT_TRUE(is_edge_token, "[Check][InputH2DOverlapConfig] empty token in manual config:%s.",
                     manual_config.c_str());
      ++empty_edge_token_count;
      GE_ASSERT_TRUE(empty_edge_token_count == 1U,
                     "[Check][InputH2DOverlapConfig] manual config:%s can only omit one edge boundary.",
                     manual_config.c_str());
    } else {
      tokens.emplace_back(token);
    }
    if (end == std::string::npos) {
      break;
    }
    begin = end + 1U;
  }
  return SUCCESS;
}

Status ParseInputH2DOverlapManualIndex(const std::string &token, size_t &index) {
  const auto trimmed_token = TrimInputH2DOverlapString(token);
  GE_ASSERT_TRUE(!trimmed_token.empty(), "[Check][InputH2DOverlapConfig] manual index is empty.");
  uint64_t parsed = 0U;
  for (const char ch : trimmed_token) {
    GE_ASSERT_TRUE(std::isdigit(static_cast<unsigned char>(ch)) != 0,
                   "[Check][InputH2DOverlapConfig] manual index:%s is not a non-negative integer.",
                   trimmed_token.c_str());
    const uint64_t digit = static_cast<uint64_t>(ch - '0');
    GE_ASSERT_TRUE(parsed <= ((std::numeric_limits<uint64_t>::max() - digit) / 10U),
                   "[Check][InputH2DOverlapConfig] manual index:%s exceeds uint64 range.", trimmed_token.c_str());
    parsed = (parsed * 10U) + digit;
  }
  GE_ASSERT_TRUE(parsed <= static_cast<uint64_t>(std::numeric_limits<size_t>::max()),
                 "[Check][InputH2DOverlapConfig] manual index:%s exceeds size_t range.", trimmed_token.c_str());
  index = static_cast<size_t>(parsed);
  return SUCCESS;
}

Status ParseInputH2DOverlapBoundaryConfig(const std::string &manual_config, InputH2DOverlapConfig &config) {
  std::vector<std::string> tokens;
  GE_CHK_STATUS_RET(SplitInputH2DOverlapManualConfig(manual_config, ',', tokens),
                    "[Split][InputH2DOverlapBoundaryConfig] failed.");
  GE_ASSERT_TRUE(!tokens.empty(), "[Check][InputH2DOverlapConfig] boundary config:%s needs at least one index.",
                 manual_config.c_str());
  config.boundaries.reserve(tokens.size());
  for (const auto &token : tokens) {
    size_t boundary = 0U;
    GE_CHK_STATUS_RET(ParseInputH2DOverlapManualIndex(token, boundary),
                      "[Parse][InputH2DOverlapBoundaryIndex] failed.");
    config.boundaries.emplace_back(boundary);
  }
  for (size_t i = 1U; i < config.boundaries.size(); ++i) {
    GE_ASSERT_TRUE(config.boundaries[i - 1U] < config.boundaries[i],
                   "[Check][InputH2DOverlapConfig] boundary config:%s must be strictly increasing.",
                   manual_config.c_str());
  }
  return SUCCESS;
}

Status LoadInputH2DOverlapConfig(const std::string &manual_config, InputH2DOverlapConfig &config) {
  config = InputH2DOverlapConfig();
  if (manual_config.empty()) {
    return SUCCESS;
  }
  const auto trimmed_config = TrimInputH2DOverlapString(manual_config);
  GE_ASSERT_TRUE(!trimmed_config.empty(), "[Check][InputH2DOverlapConfig] manual config is empty.");
  GELOGI("[InputH2DOverlap] use manual boundary config %s=%s.", kInputH2DOverlapOptionName, trimmed_config.c_str());
  GE_CHK_STATUS_RET(ParseInputH2DOverlapBoundaryConfig(trimmed_config, config),
                    "[Parse][InputH2DOverlapBoundaryConfig] failed.");
  GELOGI("[InputH2DOverlap] loaded manual boundary config:%s, boundary_count:%zu.", trimmed_config.c_str(),
         config.boundaries.size());
  return SUCCESS;
}

double EstimateInputH2DOverlapH2DUs(const uint64_t bytes, const InputH2DOverlapDpEstimation &estimation) {
  if (bytes < estimation.h2d_small_threshold) {
    return estimation.h2d_small_us;
  }
  const double alpha = (bytes > estimation.h2d_large_threshold) ? estimation.h2d_large_alpha : estimation.h2d_mid_alpha;
  return static_cast<double>(bytes) / (estimation.h2d_peak_bandwidth_gbps * kInputH2DOverlapBytesPerUsPerGbps * alpha);
}

double EstimateInputH2DOverlapBaseH2DUs(const InputH2DOverlapCopyGroup &group,
                                        const InputH2DOverlapDpEstimation &estimation) {
  double estimate_us = 0.0;
  for (const auto &input : group.inputs) {
    estimate_us += EstimateInputH2DOverlapH2DUs(input.size, estimation);
  }
  return estimate_us;
}

double EstimateInputH2DOverlapCopyGroupUs(const InputH2DOverlapCopyGroup &group,
                                          const InputH2DOverlapDpEstimation &estimation) {
  return EstimateInputH2DOverlapBaseH2DUs(group, estimation) +
         (static_cast<double>(group.inputs.size()) * estimation.h2d_item_us);
}

double EstimateInputH2DOverlapEventUs(const InputH2DOverlapCopyGroup &group,
                                      const InputH2DOverlapDpEstimation &estimation) {
  return static_cast<double>(group.wait_points.size()) * estimation.event_us;
}

bool IsInputH2DOverlapComputeCoverNode(const NodePtr &node, const int64_t logical_stream_id) {
  if ((logical_stream_id < 0) || (!input_h2d_overlap::IsTaskConsumerNode(node))) {
    return false;
  }
  return node->GetOpDesc()->GetStreamId() == logical_stream_id;
}

std::vector<size_t> BuildInputH2DOverlapComputeCoverPrefix(const ComputeGraphPtr &compute_graph,
                                                           const int64_t logical_stream_id) {
  std::vector<size_t> compute_cover_prefix{0U};
  if (compute_graph == nullptr) {
    return compute_cover_prefix;
  }
  size_t compute_node_count = 0U;
  for (const auto &node : compute_graph->GetDirectNode()) {
    if (node == nullptr) {
      continue;
    }
    if (IsInputH2DOverlapComputeCoverNode(node, logical_stream_id)) {
      ++compute_node_count;
    }
    compute_cover_prefix.emplace_back(compute_node_count);
  }
  return compute_cover_prefix;
}

size_t CountInputH2DOverlapComputeCoverNodes(const std::vector<size_t> &compute_cover_prefix,
                                             const int64_t current_topo, const int64_t previous_topo) {
  if ((current_topo < 0) || (previous_topo < 0) || (current_topo <= previous_topo) || compute_cover_prefix.empty()) {
    return 0U;
  }
  const auto begin = static_cast<size_t>(previous_topo);
  const auto end = static_cast<size_t>(current_topo);
  if ((begin >= end) || (end >= compute_cover_prefix.size())) {
    return 0U;
  }
  return compute_cover_prefix[end] - compute_cover_prefix[begin];
}

double EstimateInputH2DOverlapComputeCoverUs(const std::vector<size_t> &compute_cover_prefix,
                                             const int64_t current_topo, const int64_t previous_topo,
                                             const InputH2DOverlapDpEstimation &estimation) {
  const auto compute_node_count =
      CountInputH2DOverlapComputeCoverNodes(compute_cover_prefix, current_topo, previous_topo);
  return static_cast<double>(compute_node_count) * estimation.compute_op_us;
}

bool IsInputH2DOverlapDominated(const InputH2DOverlapDpState &lhs, const InputH2DOverlapDpState &rhs) {
  return (lhs.sync_us >= rhs.sync_us) && (lhs.worker_us >= rhs.worker_us) && (lhs.max_delay_us >= rhs.max_delay_us);
}

double GetInputH2DOverlapDpStateCost(const InputH2DOverlapDpState &state) {
  return state.sync_us + state.max_delay_us;
}

bool IsInputH2DOverlapWaitPointBefore(const InputH2DOverlapWaitPoint &lhs, const InputH2DOverlapWaitPoint &rhs) {
  if (lhs.consumer_topo_index != rhs.consumer_topo_index) {
    if (lhs.consumer_topo_index < 0) {
      return false;
    }
    if (rhs.consumer_topo_index < 0) {
      return true;
    }
    return lhs.consumer_topo_index < rhs.consumer_topo_index;
  }
  return lhs.consumer_node_name < rhs.consumer_node_name;
}

void AddEarliestInputH2DOverlapWaitPoint(const InputH2DOverlapWaitPoint &wait_point,
                                         std::map<int64_t, InputH2DOverlapWaitPoint> &stream_to_wait_point) {
  const auto iter = stream_to_wait_point.find(wait_point.logical_stream_id);
  if ((iter == stream_to_wait_point.end()) || IsInputH2DOverlapWaitPointBefore(wait_point, iter->second)) {
    stream_to_wait_point[wait_point.logical_stream_id] = wait_point;
  }
}

void AddInputH2DOverlapDpState(std::vector<InputH2DOverlapDpState> &states, InputH2DOverlapDpState &&new_state) {
  for (const auto &state : states) {
    if (IsInputH2DOverlapDominated(new_state, state)) {
      return;
    }
  }
  states.erase(std::remove_if(states.begin(), states.end(),
                              [&new_state](const InputH2DOverlapDpState &state) {
                                return IsInputH2DOverlapDominated(state, new_state);
                              }),
               states.end());
  states.emplace_back(std::move(new_state));
  if (states.size() <= kInputH2DOverlapDpMaxStateCount) {
    return;
  }
  std::stable_sort(states.begin(), states.end(),
                   [](const InputH2DOverlapDpState &lhs, const InputH2DOverlapDpState &rhs) {
                     return GetInputH2DOverlapDpStateCost(lhs) < GetInputH2DOverlapDpStateCost(rhs);
                   });
  states.resize(kInputH2DOverlapDpMaxStateCount);
}

InputH2DOverlapCopyGroup MergeInputH2DOverlapCopyGroupRange(const InputH2DOverlapCopyGroupPlan &group_plan,
                                                            const size_t begin, const size_t end) {
  InputH2DOverlapCopyGroup merged_group;
  std::map<int64_t, InputH2DOverlapWaitPoint> stream_to_wait_point;
  for (size_t group_index = begin; group_index < end; ++group_index) {
    const auto &group = group_plan.groups[group_index];
    merged_group.inputs.insert(merged_group.inputs.end(), group.inputs.begin(), group.inputs.end());
    for (const auto &wait_point : group.wait_points) {
      AddEarliestInputH2DOverlapWaitPoint(wait_point, stream_to_wait_point);
    }
  }
  for (const auto &stream_and_wait_point : stream_to_wait_point) {
    merged_group.wait_points.emplace_back(stream_and_wait_point.second);
  }
  return merged_group;
}

Status AddInputH2DOverlapMergedRange(const InputH2DOverlapCopyGroupPlan &group_plan, const size_t begin,
                                     const size_t end, std::vector<std::pair<size_t, size_t>> &ranges,
                                     std::vector<InputH2DOverlapCopyGroup> &groups) {
  if (begin == end) {
    return SUCCESS;
  }
  GE_ASSERT_TRUE((begin < end) && (end <= group_plan.groups.size()),
                 "[Check][InputH2DOverlapManualPlan] invalid group range:[%zu,%zu), base_group_count:%zu.", begin, end,
                 group_plan.groups.size());
  ranges.emplace_back(begin, end);
  groups.emplace_back(MergeInputH2DOverlapCopyGroupRange(group_plan, begin, end));
  return SUCCESS;
}

Status ApplyInputH2DOverlapManualPlan(const std::string &graph_name, const InputH2DOverlapConfig &config,
                                      InputH2DOverlapCopyGroupPlan &group_plan) {
  const size_t before_group_count = group_plan.groups.size();
  const size_t before_input_count = group_plan.InputCount();
  const size_t before_wait_point_count = group_plan.WaitPointCount();
  const auto &boundaries = config.boundaries;
  GE_ASSERT_TRUE(!boundaries.empty(), "[Check][InputH2DOverlapManualPlan] empty manual boundary, graph:%s.",
                 graph_name.c_str());

  std::vector<InputH2DOverlapCopyGroup> planned_groups;
  planned_groups.reserve(boundaries.size());
  std::vector<std::pair<size_t, size_t>> planned_ranges;
  planned_ranges.reserve(boundaries.size());

  size_t range_begin = boundaries.front();
  GE_ASSERT_TRUE(range_begin <= before_group_count,
                 "[Check][InputH2DOverlapManualPlan] boundary:%zu exceeds base_group_count:%zu, graph:%s.", range_begin,
                 before_group_count, graph_name.c_str());
  for (size_t i = 1U; i < boundaries.size(); ++i) {
    const size_t range_end = boundaries[i];
    GE_ASSERT_TRUE(range_end <= before_group_count,
                   "[Check][InputH2DOverlapManualPlan] boundary:%zu exceeds base_group_count:%zu, graph:%s.", range_end,
                   before_group_count, graph_name.c_str());
    GE_CHK_STATUS_RET(AddInputH2DOverlapMergedRange(group_plan, range_begin, range_end, planned_ranges, planned_groups),
                      "[Add][InputH2DOverlapManualRange] failed.");
    range_begin = range_end;
  }
  GE_CHK_STATUS_RET(
      AddInputH2DOverlapMergedRange(group_plan, range_begin, before_group_count, planned_ranges, planned_groups),
      "[Add][InputH2DOverlapManualRange] failed.");

  LogInputH2DOverlapGroups(graph_name, "manual", before_group_count, planned_ranges);
  for (size_t range_index = 0U; range_index < planned_ranges.size(); ++range_index) {
    const auto &range = planned_ranges[range_index];
    const auto &merged_group = planned_groups[range_index];
    const auto inputs = BuildInputH2DOverlapInputSummary(merged_group);
    const auto wait_points = BuildInputH2DOverlapWaitSummary(merged_group);
    GELOGI(
        "[InputH2DOverlap] manual plan range wait[%zu], range:[%zu,%zu), wait_point_count:%zu, "
        "first_consumer_topo:%" PRId64 ", wait_points:%s.",
        range_index, range.first, range.second, merged_group.wait_points.size(),
        GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(merged_group), wait_points.c_str());
    GELOGI(
        "[InputH2DOverlap] manual plan range[%zu], range:[%zu,%zu), input_count:%zu, wait_point_count:%zu, "
        "bytes:%" PRIu64 ", wait_points:%s, inputs:%s.",
        range_index, range.first, range.second, merged_group.inputs.size(), merged_group.wait_points.size(),
        GetInputH2DOverlapCopyGroupBytes(merged_group), wait_points.c_str(), inputs.c_str());
  }

  group_plan.groups = std::move(planned_groups);
  GELOGI(
      "[InputH2DOverlap] manual plan admits copy groups, graph:%s, group_count:%zu->%zu, "
      "input_count:%zu->%zu, wait_point_count:%zu->%zu.",
      graph_name.c_str(), before_group_count, group_plan.groups.size(), before_input_count, group_plan.InputCount(),
      before_wait_point_count, group_plan.WaitPointCount());
  return SUCCESS;
}

std::unordered_set<size_t> GetPlannedGroupIndexes(const InputH2DOverlapDpState &state) {
  std::unordered_set<size_t> planned_group_indexes;
  for (const auto &range : state.planned_group_ranges) {
    for (size_t group_index = range.first; group_index < range.second; ++group_index) {
      (void)planned_group_indexes.emplace(group_index);
    }
  }
  return planned_group_indexes;
}

void LogInputH2DOverlapDpErrorSummary(const std::string &graph_name, const InputH2DOverlapDpEstimation &estimation,
                                      const InputH2DOverlapCopyGroupPlan &group_plan,
                                      const std::vector<InputH2DOverlapDpGroupEstimate> &group_estimates,
                                      const std::vector<std::vector<double>> &planned_interval_us,
                                      const std::vector<std::vector<double>> &event_interval_us,
                                      const std::vector<double> &compute_cover_to_group,
                                      const std::unordered_set<size_t> &planned_group_indexes,
                                      const InputH2DOverlapDpState &best_state) {
  size_t legacy_group_count = 0U;
  size_t legacy_input_count = 0U;
  uint64_t legacy_bytes = 0U;
  uint64_t planned_bytes = 0U;
  for (size_t group_index = 0U; group_index < group_plan.groups.size(); ++group_index) {
    const auto &group = group_plan.groups[group_index];
    const uint64_t group_bytes = GetInputH2DOverlapCopyGroupBytes(group);
    if (planned_group_indexes.find(group_index) != planned_group_indexes.end()) {
      planned_bytes = AccumulateInputH2DOverlapBytes(planned_bytes, group_bytes);
    } else {
      ++legacy_group_count;
      legacy_input_count += group.inputs.size();
      legacy_bytes = AccumulateInputH2DOverlapBytes(legacy_bytes, group_bytes);
    }
  }
  GELOGI(
      "[InputH2DOverlap] default dp summary, graph:%s, base_group_count:%zu, input_count:%zu, "
      "planned_group_count:%zu, planned_base_group_count:%zu, planned_bytes:%" PRIu64
      ", legacy_group_count:%zu, legacy_input_count:%zu, legacy_bytes:%" PRIu64
      ", estimate_sync_us:%f, estimate_worker_us:%f, "
      "estimate_max_delay_us:%f, "
      "estimate_total_us:%f, compute_op_us:%f, h2d_peak_bandwidth_gbps:%f, "
      "h2d_large_alpha:%f, h2d_mid_alpha:%f, h2d_small_us:%f, h2d_small_threshold:%" PRIu64
      ", h2d_large_threshold:%" PRIu64 ", h2d_item_us:%f, event_us:%f.",
      graph_name.c_str(), group_plan.groups.size(), group_plan.InputCount(), best_state.planned_group_ranges.size(),
      planned_group_indexes.size(), planned_bytes, legacy_group_count, legacy_input_count, legacy_bytes,
      best_state.sync_us, best_state.worker_us, best_state.max_delay_us, GetInputH2DOverlapDpStateCost(best_state),
      estimation.compute_op_us, estimation.h2d_peak_bandwidth_gbps, estimation.h2d_large_alpha,
      estimation.h2d_mid_alpha, estimation.h2d_small_us, estimation.h2d_small_threshold, estimation.h2d_large_threshold,
      estimation.h2d_item_us, estimation.event_us);
  for (size_t group_index = 0U; group_index < group_plan.groups.size(); ++group_index) {
    const auto &group = group_plan.groups[group_index];
    const auto &estimate = group_estimates[group_index];
    const auto inputs = BuildInputH2DOverlapInputSummary(group);
    const auto wait_points = BuildInputH2DOverlapWaitSummary(group);
    const bool planned = (planned_group_indexes.find(group_index) != planned_group_indexes.end());
    GELOGI("[InputH2DOverlap] default dp group[%zu], decision:%s, bytes:%" PRIu64
           ", input_count:%zu, wait_point_count:%zu, first_consumer_topo:%" PRId64
           ", estimate_legacy_us:%f, estimate_planned_us:%f"
           ", estimate_event_us:%f, estimate_cover_us:%f, inputs:%s, wait_points:%s.",
           group_index, planned ? "planned" : "legacy", GetInputH2DOverlapCopyGroupBytes(group), group.inputs.size(),
           group.wait_points.size(), GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(group), estimate.legacy_us,
           estimate.planned_us, estimate.event_us, estimate.cover_us, inputs.c_str(), wait_points.c_str());
  }

  size_t planned_range_index = 0U;
  size_t group_index = 0U;
  double sync_us = 0.0;
  double worker_us = 0.0;
  double max_delay_us = 0.0;
  while (group_index < group_plan.groups.size()) {
    if ((planned_range_index < best_state.planned_group_ranges.size()) &&
        (best_state.planned_group_ranges[planned_range_index].first == group_index)) {
      const auto &range = best_state.planned_group_ranges[planned_range_index];
      const auto merged_group = MergeInputH2DOverlapCopyGroupRange(group_plan, range.first, range.second);
      const double range_planned_us = planned_interval_us[range.first][range.second];
      const double range_event_us = event_interval_us[range.first][range.second];
      const double worker_before_us = worker_us;
      worker_us += range_planned_us + range_event_us;
      const double cover_to_first_wait_us = compute_cover_to_group[range.first];
      const double exposed_delay_us = std::max(0.0, worker_us - cover_to_first_wait_us);
      max_delay_us = std::max(max_delay_us, exposed_delay_us);
      const auto inputs = BuildInputH2DOverlapInputSummary(merged_group);
      const auto wait_points = BuildInputH2DOverlapWaitSummary(merged_group);
      GELOGI(
          "[InputH2DOverlap] default dp planned range wait[%zu], "
          "base_group_range:[%zu,%zu), final_wait_point_count:%zu, first_consumer_topo:%" PRId64 ", wait_points:%s.",
          planned_range_index, range.first, range.second, merged_group.wait_points.size(),
          GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(merged_group), wait_points.c_str());
      GELOGI(
          "[InputH2DOverlap] default dp planned range[%zu], base_group_range:[%zu,%zu), "
          "base_group_count:%zu, input_count:%zu, bytes:%" PRIu64
          ", final_wait_point_count:%zu, "
          "first_consumer_topo:%" PRId64
          ", estimate_h2d_us:%f, estimate_event_us:%f, "
          "estimate_worker_before_us:%f, estimate_worker_after_us:%f, estimate_sync_before_us:%f, "
          "estimate_cover_to_first_wait_us:%f, estimate_exposed_delay_us:%f, estimate_max_delay_us:%f, "
          "wait_points:%s, inputs:%s.",
          planned_range_index, range.first, range.second, range.second - range.first, merged_group.inputs.size(),
          GetInputH2DOverlapCopyGroupBytes(merged_group), merged_group.wait_points.size(),
          GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(merged_group), range_planned_us, range_event_us,
          worker_before_us, worker_us, sync_us, cover_to_first_wait_us, exposed_delay_us, max_delay_us,
          wait_points.c_str(), inputs.c_str());
      group_index = range.second;
      ++planned_range_index;
      continue;
    }
    sync_us += group_estimates[group_index].legacy_us;
    ++group_index;
  }
}

InputH2DOverlapDpContext BuildInputH2DOverlapDpContext(const ComputeGraphPtr &compute_graph,
                                                       const InputH2DOverlapCopyGroupPlan &group_plan,
                                                       const InputH2DOverlapDpEstimation &estimation) {
  InputH2DOverlapDpContext context;
  const int64_t logical_stream_id = group_plan.groups[0U].wait_points[0U].logical_stream_id;
  const auto compute_cover_prefix = BuildInputH2DOverlapComputeCoverPrefix(compute_graph, logical_stream_id);

  context.group_estimates.reserve(group_plan.groups.size());
  std::vector<double> base_h2d_prefix_us(group_plan.groups.size() + 1U, 0.0);
  std::vector<size_t> input_count_prefix(group_plan.groups.size() + 1U, 0U);
  context.compute_cover_to_group.resize(group_plan.groups.size(), 0.0);
  double cumulative_compute_cover_us = 0.0;
  for (size_t group_index = 0U; group_index < group_plan.groups.size(); ++group_index) {
    const auto &group = group_plan.groups[group_index];
    const double base_h2d_us = EstimateInputH2DOverlapBaseH2DUs(group, estimation);
    const double copy_us = EstimateInputH2DOverlapCopyGroupUs(group, estimation);
    const double event_us = EstimateInputH2DOverlapEventUs(group, estimation);
    const int64_t current_topo = GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(group);
    const int64_t previous_topo =
        (group_index == 0U) ? current_topo
                            : GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(group_plan.groups[group_index - 1U]);
    const double cover_us =
        EstimateInputH2DOverlapComputeCoverUs(compute_cover_prefix, current_topo, previous_topo, estimation);
    cumulative_compute_cover_us += cover_us;
    context.compute_cover_to_group[group_index] = cumulative_compute_cover_us;
    base_h2d_prefix_us[group_index + 1U] = base_h2d_prefix_us[group_index] + base_h2d_us;
    input_count_prefix[group_index + 1U] = input_count_prefix[group_index] + group.inputs.size();
    context.group_estimates.push_back({copy_us, copy_us, event_us, cover_us, current_topo});
  }

  context.planned_interval_us.assign(group_plan.groups.size(), std::vector<double>(group_plan.groups.size() + 1U, 0.0));
  context.event_interval_us.assign(group_plan.groups.size(), std::vector<double>(group_plan.groups.size() + 1U, 0.0));
  for (size_t begin = 0U; begin < group_plan.groups.size(); ++begin) {
    std::map<int64_t, InputH2DOverlapWaitPoint> stream_to_wait_point;
    for (size_t end = begin + 1U; end <= group_plan.groups.size(); ++end) {
      for (const auto &wait_point : group_plan.groups[end - 1U].wait_points) {
        AddEarliestInputH2DOverlapWaitPoint(wait_point, stream_to_wait_point);
      }
      const size_t input_count = input_count_prefix[end] - input_count_prefix[begin];
      context.planned_interval_us[begin][end] = base_h2d_prefix_us[end] - base_h2d_prefix_us[begin] +
                                                (static_cast<double>(input_count) * estimation.h2d_item_us);
      context.event_interval_us[begin][end] = static_cast<double>(stream_to_wait_point.size()) * estimation.event_us;
    }
  }
  return context;
}

bool FindBestInputH2DOverlapDpState(const InputH2DOverlapDpContext &context, InputH2DOverlapDpState &best_state) {
  std::vector<std::vector<InputH2DOverlapDpState>> dp(context.group_estimates.size() + 1U);
  dp[0].emplace_back();
  for (size_t group_index = 0U; group_index < context.group_estimates.size(); ++group_index) {
    for (const auto &state : dp[group_index]) {
      if (state.planned_group_ranges.empty()) {
        auto legacy_state = state;
        legacy_state.sync_us += context.group_estimates[group_index].legacy_us;
        AddInputH2DOverlapDpState(dp[group_index + 1U], std::move(legacy_state));
      }

      for (size_t end = group_index + 1U; end <= context.group_estimates.size(); ++end) {
        auto planned_state = state;
        const double range_planned_us = context.planned_interval_us[group_index][end];
        const double range_event_us = context.event_interval_us[group_index][end];
        planned_state.worker_us += range_planned_us + range_event_us;
        // Worker copy is submitted after legacy H2D, so only model compute before the wait point can cover it.
        const double exposed_delay_us =
            std::max(0.0, planned_state.worker_us - context.compute_cover_to_group[group_index]);
        planned_state.max_delay_us = std::max(planned_state.max_delay_us, exposed_delay_us);
        planned_state.planned_group_ranges.emplace_back(group_index, end);
        AddInputH2DOverlapDpState(dp[end], std::move(planned_state));
      }
    }
  }

  auto &final_states = dp[context.group_estimates.size()];
  auto best_iter = std::min_element(final_states.begin(), final_states.end(),
                                    [](const InputH2DOverlapDpState &lhs, const InputH2DOverlapDpState &rhs) {
                                      return GetInputH2DOverlapDpStateCost(lhs) < GetInputH2DOverlapDpStateCost(rhs);
                                    });
  if (best_iter == final_states.end() || best_iter->planned_group_ranges.empty()) {
    return false;
  }
  best_state = *best_iter;
  return true;
}

InputH2DOverlapDpAdmitStats CollectInputH2DOverlapDpAdmitStats(const InputH2DOverlapCopyGroupPlan &group_plan,
                                                               const InputH2DOverlapDpState &best_state) {
  InputH2DOverlapDpAdmitStats stats;
  stats.before_group_count = group_plan.groups.size();
  stats.before_input_count = group_plan.InputCount();
  stats.before_wait_point_count = group_plan.WaitPointCount();
  stats.before_total_bytes = GetInputH2DOverlapPlannedTotalBytes(group_plan);
  const auto planned_group_indexes = GetPlannedGroupIndexes(best_state);
  for (size_t group_index = 0U; group_index < group_plan.groups.size(); ++group_index) {
    const auto &group = group_plan.groups[group_index];
    const uint64_t group_bytes = GetInputH2DOverlapCopyGroupBytes(group);
    if (planned_group_indexes.find(group_index) != planned_group_indexes.end()) {
      stats.planned_bytes = AccumulateInputH2DOverlapBytes(stats.planned_bytes, group_bytes);
    } else {
      ++stats.legacy_group_count;
      stats.legacy_input_count += group.inputs.size();
      stats.legacy_bytes = AccumulateInputH2DOverlapBytes(stats.legacy_bytes, group_bytes);
    }
  }
  return stats;
}

void ApplyInputH2DOverlapDpState(const InputH2DOverlapDpState &best_state, InputH2DOverlapCopyGroupPlan &group_plan) {
  std::vector<InputH2DOverlapCopyGroup> planned_groups;
  planned_groups.reserve(best_state.planned_group_ranges.size());
  for (const auto &range : best_state.planned_group_ranges) {
    planned_groups.emplace_back(MergeInputH2DOverlapCopyGroupRange(group_plan, range.first, range.second));
  }
  group_plan.groups = std::move(planned_groups);
}

void ApplyInputH2DOverlapDefaultDp(const ComputeGraphPtr &compute_graph, const std::string &graph_name,
                                   const InputH2DOverlapDpEstimation &estimation,
                                   InputH2DOverlapCopyGroupPlan &group_plan) {
  if (group_plan.groups.empty()) {
    return;
  }
  const auto dp_context = BuildInputH2DOverlapDpContext(compute_graph, group_plan, estimation);
  InputH2DOverlapDpState best_state;
  if (!FindBestInputH2DOverlapDpState(dp_context, best_state)) {
    GELOGI("[InputH2DOverlap] default dp filters all copy groups, graph:%s. Keep original input copy path.",
           graph_name.c_str());
    group_plan.groups.clear();
    return;
  }

  const auto stats = CollectInputH2DOverlapDpAdmitStats(group_plan, best_state);
  const auto planned_group_indexes = GetPlannedGroupIndexes(best_state);
  LogInputH2DOverlapDpErrorSummary(graph_name, estimation, group_plan, dp_context.group_estimates,
                                   dp_context.planned_interval_us, dp_context.event_interval_us,
                                   dp_context.compute_cover_to_group, planned_group_indexes, best_state);
  LogInputH2DOverlapGroups(graph_name, "auto", group_plan.groups.size(), best_state.planned_group_ranges);
  LogInputH2DOverlapManualConfigHint(graph_name, best_state.planned_group_ranges, group_plan.groups.size());
  ApplyInputH2DOverlapDpState(best_state, group_plan);
  GELOGI(
      "[InputH2DOverlap] default dp admits copy groups, graph:%s, group_count:%zu->%zu, "
      "input_count:%zu->%zu, wait_point_count:%zu->%zu, planned_total_bytes:%" PRIu64 "->%" PRIu64
      ", planned_bytes:%" PRIu64 ", legacy_group_count:%zu, legacy_input_count:%zu, legacy_bytes:%" PRIu64
      ", estimate_sync_us:%f, estimate_worker_us:%f, estimate_max_delay_us:%f, estimate_total_us:%f.",
      graph_name.c_str(), stats.before_group_count, group_plan.groups.size(), stats.before_input_count,
      group_plan.InputCount(), stats.before_wait_point_count, group_plan.WaitPointCount(), stats.before_total_bytes,
      GetInputH2DOverlapPlannedTotalBytes(group_plan), stats.planned_bytes, stats.legacy_group_count,
      stats.legacy_input_count, stats.legacy_bytes, best_state.sync_us, best_state.worker_us, best_state.max_delay_us,
      GetInputH2DOverlapDpStateCost(best_state));
}

int64_t GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(const InputH2DOverlapCopyGroup &group) {
  int64_t first_consumer_topo_index = -1;
  for (const auto &wait_point : group.wait_points) {
    if ((first_consumer_topo_index < 0) || (wait_point.consumer_topo_index < first_consumer_topo_index)) {
      first_consumer_topo_index = wait_point.consumer_topo_index;
    }
  }
  return first_consumer_topo_index;
}

void AppendInputH2DOverlapItem(std::string &items, const std::string &item) {
  if (!items.empty()) {
    items += ",";
  }
  items += item;
}

std::string BuildInputH2DOverlapInputSummary(const InputH2DOverlapCopyGroup &group) {
  std::string inputs;
  for (const auto &input : group.inputs) {
    AppendInputH2DOverlapItem(inputs, std::to_string(input.input_index) + "(" + input.data_node_name +
                                          ",bytes:" + std::to_string(input.size) + ")");
  }
  return inputs;
}

std::string BuildInputH2DOverlapWaitSummary(const InputH2DOverlapCopyGroup &group) {
  std::string wait_points;
  for (const auto &wait_point : group.wait_points) {
    AppendInputH2DOverlapItem(wait_points, wait_point.consumer_node_name +
                                               "(stream:" + std::to_string(wait_point.logical_stream_id) +
                                               ",topo:" + std::to_string(wait_point.consumer_topo_index) + ")");
  }
  return wait_points;
}

std::string BuildInputH2DOverlapBaseGroupRangeSummary(const std::vector<std::pair<size_t, size_t>> &ranges) {
  std::string summary;
  for (const auto &range : ranges) {
    AppendInputH2DOverlapItem(summary, "[" + std::to_string(range.first) + "," + std::to_string(range.second) + ")");
  }
  return summary;
}

std::string BuildInputH2DOverlapBoundarySummary(const std::vector<size_t> &boundaries) {
  std::string summary = "[";
  for (size_t i = 0U; i < boundaries.size(); ++i) {
    if (i > 0U) {
      summary += ",";
    }
    summary += std::to_string(boundaries[i]);
  }
  summary += "]";
  return summary;
}

std::vector<size_t> BuildInputH2DOverlapBoundariesFromRanges(const std::vector<std::pair<size_t, size_t>> &ranges,
                                                             const size_t base_group_count) {
  std::vector<size_t> boundaries;
  if (ranges.empty()) {
    return boundaries;
  }
  boundaries.emplace_back(ranges[0U].first);
  for (const auto &range : ranges) {
    if (range.second < base_group_count) {
      boundaries.emplace_back(range.second);
    }
  }
  return boundaries;
}

std::string BuildInputH2DOverlapBoundaryConfigString(const std::vector<std::pair<size_t, size_t>> &ranges,
                                                     const size_t base_group_count, bool &is_exact) {
  is_exact = true;
  if (ranges.empty()) {
    return "";
  }
  std::string config = std::to_string(ranges[0U].first);
  size_t previous_end = ranges[0U].first;
  for (const auto &range : ranges) {
    if (range.first != previous_end) {
      is_exact = false;
      config += "," + std::to_string(range.first);
    }
    if (range.second < base_group_count) {
      config += "," + std::to_string(range.second);
    }
    previous_end = range.second;
  }
  return config;
}

void LogInputH2DOverlapGroups(const std::string &graph_name, const char *const mode, const size_t base_group_count,
                              const std::vector<std::pair<size_t, size_t>> &planned_ranges) {
  const auto boundaries = BuildInputH2DOverlapBoundariesFromRanges(planned_ranges, base_group_count);
  const size_t sync_end = planned_ranges.empty() ? base_group_count : planned_ranges.front().first;
  const auto overlap_ranges = BuildInputH2DOverlapBaseGroupRangeSummary(planned_ranges);
  const auto boundary_summary = BuildInputH2DOverlapBoundarySummary(boundaries);
  GELOGI(
      "[InputH2DOverlap] H2D overlap groups: mode=%s, graph:%s, boundaries=%s, base_group_count:%zu, "
      "sync_range:[0,%zu), overlap_group_count:%zu, overlap_ranges:%s.",
      mode, graph_name.c_str(), boundary_summary.c_str(), base_group_count, sync_end, planned_ranges.size(),
      overlap_ranges.c_str());
}

void LogInputH2DOverlapManualConfigHint(const std::string &graph_name,
                                        const std::vector<std::pair<size_t, size_t>> &ranges,
                                        const size_t base_group_count) {
  bool is_exact = true;
  const auto boundary_config = BuildInputH2DOverlapBoundaryConfigString(ranges, base_group_count, is_exact);
  if (boundary_config.empty()) {
    return;
  }
  std::vector<std::string> tokens;
  if (SplitInputH2DOverlapManualConfig(boundary_config, ',', tokens) != SUCCESS) {
    return;
  }
  std::vector<size_t> boundaries;
  boundaries.reserve(tokens.size());
  for (const auto &token : tokens) {
    size_t boundary = 0U;
    if (ParseInputH2DOverlapManualIndex(token, boundary) != SUCCESS) {
      return;
    }
    boundaries.emplace_back(boundary);
  }
  const auto boundary_summary = BuildInputH2DOverlapBoundarySummary(boundaries);
  if (is_exact) {
    GELOGI("[InputH2DOverlap] manual boundary config hint, graph:%s, boundaries=%s, exact:true.", graph_name.c_str(),
           boundary_summary.c_str());
    return;
  }
  const auto range_summary = BuildInputH2DOverlapBaseGroupRangeSummary(ranges);
  GELOGI(
      "[InputH2DOverlap] manual boundary config hint, graph:%s, boundaries=%s, exact:false, "
      "default_dp_base_group_ranges:%s. Manual boundary config cannot keep non-contiguous gaps as legacy, "
      "so this hint also plans the gap base groups.",
      graph_name.c_str(), boundary_summary.c_str(), range_summary.c_str());
}

void LogInputH2DOverlapBaseGroupsForManualConfig(const std::string &graph_name,
                                                 const InputH2DOverlapCopyGroupPlan &group_plan) {
  GELOGI(
      "[InputH2DOverlap] manual config base groups, graph:%s, base_group_count:%zu. "
      "Use boundaries like 44,64,68.",
      graph_name.c_str(), group_plan.groups.size());
  for (size_t group_index = 0U; group_index < group_plan.groups.size(); ++group_index) {
    const auto &group = group_plan.groups[group_index];
    const auto inputs = BuildInputH2DOverlapInputSummary(group);
    const auto wait_points = BuildInputH2DOverlapWaitSummary(group);
    GELOGI("[InputH2DOverlap] manual config base group[%zu], bytes:%" PRIu64
           ", input_count:%zu, wait_point_count:%zu, first_consumer_topo:%" PRId64 ", inputs:%s, wait_points:%s.",
           group_index, GetInputH2DOverlapCopyGroupBytes(group), group.inputs.size(), group.wait_points.size(),
           GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(group), inputs.c_str(), wait_points.c_str());
  }
}

void LogInputH2DOverlapPlanSummary(const InputH2DOverlapOption option, const InputH2DOverlapCopyGroupPlan &group_plan,
                                   const std::string &graph_name) {
  const uint64_t planned_total_bytes = GetInputH2DOverlapPlannedTotalBytes(group_plan);
  const uint64_t non_first_group_bytes = GetInputH2DOverlapNonFirstGroupBytes(group_plan);
  GELOGI(
      "[InputH2DOverlap] plan admitted graph:%s, mode:%s, group_count:%zu, input_count:%zu, "
      "wait_point_count:%zu, planned_total_bytes:%" PRIu64 ", non_first_group_bytes:%" PRIu64
      ", copy_stream_record_count:%zu.",
      graph_name.c_str(), InputH2DOverlapOptionToString(option), group_plan.groups.size(), group_plan.InputCount(),
      group_plan.WaitPointCount(), planned_total_bytes, non_first_group_bytes, group_plan.WaitPointCount());
  for (size_t group_index = 0U; group_index < group_plan.groups.size(); ++group_index) {
    const auto &group = group_plan.groups[group_index];
    const auto inputs = BuildInputH2DOverlapInputSummary(group);
    const auto wait_points = BuildInputH2DOverlapWaitSummary(group);
    GELOGI("[InputH2DOverlap] plan group[%zu], bytes:%" PRIu64 ", first_consumer_topo:%" PRId64
           ", inputs:%s, wait_points:%s.",
           group_index, GetInputH2DOverlapCopyGroupBytes(group),
           GetInputH2DOverlapCopyGroupFirstConsumerTopoIndex(group), inputs.c_str(), wait_points.c_str());
  }
}

Status CheckInputH2DOverlapGraphSupported(const ComputeGraphPtr &compute_graph,
                                          const InputH2DOverlapOption selected_option) {
  std::string dynamic_graph_name;
  if (IsInputH2DOverlapDynamicShapeGraph(compute_graph, dynamic_graph_name)) {
    GELOGE(UNSUPPORTED, "[Check][InputH2DOverlap] dynamic shape is not supported, graph:%s, dynamic graph:%s, mode:%s.",
           compute_graph->GetName().c_str(), dynamic_graph_name.c_str(),
           InputH2DOverlapOptionToString(selected_option));
    return UNSUPPORTED;
  }
  return SUCCESS;
}

Status ApplyInputH2DOverlapPlanningPolicy(const ComputeGraphPtr &compute_graph, const int64_t stream_num,
                                          const std::string &manual_config, InputH2DOverlapCopyGroupPlan &group_plan) {
  if (stream_num > 1) {
    GELOGW("[InputH2DOverlap] skip graph:%s because graph has multiple allocated streams, stream_num:%" PRId64
           ". Current input H2D overlap only supports single-stream graphs. Keep original input copy path.",
           compute_graph->GetName().c_str(), stream_num);
    group_plan.groups.clear();
    return SUCCESS;
  }

  LogInputH2DOverlapBaseGroupsForManualConfig(compute_graph->GetName(), group_plan);
  if (!manual_config.empty()) {
    InputH2DOverlapConfig overlap_config;
    GE_CHK_STATUS_RET(LoadInputH2DOverlapConfig(manual_config, overlap_config),
                      "[Load][InputH2DOverlapConfig] failed.");
    GE_CHK_STATUS_RET(ApplyInputH2DOverlapManualPlan(compute_graph->GetName(), overlap_config, group_plan),
                      "[Apply][InputH2DOverlapManualPlan] failed.");
    return SUCCESS;
  }

  InputH2DOverlapDpEstimation dp_estimation;
  ApplyInputH2DOverlapDefaultDp(compute_graph, compute_graph->GetName(), dp_estimation, group_plan);
  return SUCCESS;
}

Status BuildInputH2DOverlapPendingPlan(const ComputeGraphPtr &compute_graph,
                                       const InputH2DOverlapOption selected_option,
                                       InputH2DOverlapCopyGroupPlan &group_plan,
                                       InputH2DOverlapPendingPlan &pending_plan) {
  LogInputH2DOverlapPlanSummary(selected_option, group_plan, compute_graph->GetName());
  GE_ASSERT_SUCCESS(BuildPendingPlan(group_plan, pending_plan), "[Build][InputH2DOverlapPendingPlan] failed.");
  GELOGI(
      "[InputH2DOverlap] compiler feature gate is enabled in %s mode for graph:%s. "
      "Built pending plan, version:%u, copy_stream_id:%u, "
      "group_count:%zu, input_count:%zu, wait_request_count:%zu. "
      "Plan finalization and serialization will continue after stream and task refresh.",
      InputH2DOverlapOptionToString(selected_option), compute_graph->GetName().c_str(), pending_plan.version,
      pending_plan.copy_stream_id, pending_plan.copy_group_plan.groups.size(), pending_plan.InputCount(),
      pending_plan.WaitRequestCount());
  return SUCCESS;
}

Status BuildPendingPlanIfEnabled(const ComputeGraphPtr &compute_graph, const int64_t stream_num,
                                 InputH2DOverlapPendingPlan &pending_plan) {
  GE_CHECK_NOTNULL(compute_graph);
  pending_plan = InputH2DOverlapPendingPlan();
  InputH2DOverlapOption selected_option = InputH2DOverlapOption::kDisabled;
  std::string manual_config;
  GE_CHK_STATUS_RET(GetInputH2DOverlapOption(selected_option, manual_config), "[Get][InputH2DOverlapOption] failed.");
  if (selected_option == InputH2DOverlapOption::kDisabled) {
    return SUCCESS;
  }
  GE_CHK_STATUS_RET(CheckInputH2DOverlapGraphSupported(compute_graph, selected_option),
                    "[Check][InputH2DOverlapGraphSupported] failed.");

  InputH2DOverlapCandidatePlan candidate_plan;
  GE_ASSERT_SUCCESS(AnalyzeInputs(compute_graph, candidate_plan), "[Analyze][InputH2DOverlapInputs] failed.");
  if (candidate_plan.empty()) {
    GELOGI(
        "[InputH2DOverlap] compiler feature gate is enabled in %s mode for graph:%s, "
        "but no eligible input candidate is found. Keep original input copy path.",
        InputH2DOverlapOptionToString(selected_option), compute_graph->GetName().c_str());
    return SUCCESS;
  }

  InputH2DOverlapCopyGroupPlan group_plan;
  GE_ASSERT_SUCCESS(SelectInputs(candidate_plan, group_plan), "[Select][InputH2DOverlapInputs] failed.");
  GE_CHK_STATUS_RET(ApplyInputH2DOverlapPlanningPolicy(compute_graph, stream_num, manual_config, group_plan),
                    "[Apply][InputH2DOverlapPlanningPolicy] failed.");
  if (group_plan.empty()) {
    GELOGI(
        "[InputH2DOverlap] compiler feature gate is enabled in %s mode for graph:%s. "
        "Built candidates, input_count:%zu, wait_point_count:%zu, but no input is admitted. "
        "Keep original input copy path.",
        InputH2DOverlapOptionToString(selected_option), compute_graph->GetName().c_str(), candidate_plan.inputs.size(),
        candidate_plan.WaitPointCount());
    return SUCCESS;
  }
  return BuildInputH2DOverlapPendingPlan(compute_graph, selected_option, group_plan, pending_plan);
}

Status LoadModelTaskDefFromModel(const Model &model, domi::ModelTaskDef &model_task_def) {
  Buffer task_def_bytes;
  GE_ASSERT_TRUE(AttrUtils::GetZeroCopyBytes(model, MODEL_ATTR_TASKS, task_def_bytes), "[Get][Attr] %s failed.",
                 MODEL_ATTR_TASKS.c_str());
  GE_ASSERT_TRUE(task_def_bytes.GetSize() <= static_cast<size_t>(std::numeric_limits<int32_t>::max()),
                 "ModelTaskDef attr:%s size:%zu exceeds protobuf array limit.", MODEL_ATTR_TASKS.c_str(),
                 task_def_bytes.GetSize());
  GE_ASSERT_TRUE(
      ReadProtoFromArray(task_def_bytes.GetData(), static_cast<int32_t>(task_def_bytes.GetSize()), &model_task_def),
      "[Read][ModelTaskDef] from attr:%s failed.", MODEL_ATTR_TASKS.c_str());
  return SUCCESS;
}
}  // namespace

Status IsInputH2DOverlapEnabled(bool &enabled) {
  enabled = false;
  InputH2DOverlapOption selected_option = InputH2DOverlapOption::kDisabled;
  std::string manual_config;
  GE_CHK_STATUS_RET(GetInputH2DOverlapOption(selected_option, manual_config), "[Get][InputH2DOverlapOption] failed.");
  enabled = (selected_option != InputH2DOverlapOption::kDisabled);
  return SUCCESS;
}

Status InputH2DOverlapPlanner::Prepare(const ComputeGraphPtr &compute_graph, StreamAllocator &stream_allocator) {
  plan_ = InputH2DOverlapPendingPlan();

  InputH2DOverlapPendingPlan pending_plan;
  GE_CHK_STATUS_RET(BuildPendingPlanIfEnabled(compute_graph, stream_allocator.GetStreamNum(), pending_plan),
                    "[Build][InputH2DOverlapPendingPlan] failed.");
  if (pending_plan.empty()) {
    return SUCCESS;
  }

  GE_ASSERT_SUCCESS(input_h2d_overlap::RegisterWaits(pending_plan, stream_allocator),
                    "[Register][InputH2DOverlapWaits] failed.");
  plan_ = std::move(pending_plan);
  return SUCCESS;
}

Status InputH2DOverlapPlanner::AddCopyStream(int64_t &stream_num) {
  if (plan_.empty()) {
    return SUCCESS;
  }
  return input_h2d_overlap::AddCopyStream(plan_, stream_num);
}

Status InputH2DOverlapPlanner::SaveToModel(const StreamAllocator &stream_allocator, const int64_t stream_num,
                                           const int64_t event_num, Model &model) {
  if (plan_.empty()) {
    return SUCCESS;
  }

  domi::ModelTaskDef model_task_def;
  GE_ASSERT_SUCCESS(LoadModelTaskDefFromModel(model, model_task_def), "[Load][InputH2DOverlapModelTaskDef] failed.");
  InputH2DOverlapFinalPlan final_plan;
  GE_ASSERT_SUCCESS(input_h2d_overlap::BuildFinalPlan(plan_, stream_allocator, plan_.copy_stream_id,
                                                      static_cast<uint32_t>(stream_num),
                                                      static_cast<uint32_t>(event_num), model_task_def, final_plan),
                    "[Build][InputH2DOverlapFinalPlan] failed.");
  NamedAttrs plan_attr;
  GE_ASSERT_SUCCESS(input_h2d_overlap::SerializePlan(final_plan, plan_attr),
                    "[Serialize][InputH2DOverlapPlan] failed.");
  GE_ASSERT_TRUE(AttrUtils::SetNamedAttrs(model, kInputH2DOverlapPlanAttrName, plan_attr),
                 "[Set][InputH2DOverlapPlanAttr] %s failed.", kInputH2DOverlapPlanAttrName);
  GELOGI("[InputH2DOverlap] save model plan, copy stream:%u, group count:%zu.", final_plan.copy_stream_id,
         final_plan.groups.size());
  return SUCCESS;
}

}  // namespace ge
