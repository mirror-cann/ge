/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/dag/dag_weighted_stream_merger.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "graph/build/dag/dag_checker.h"
#include "graph/build/dag/dag_log.h"

namespace minidag {
namespace {
static constexpr double kImproveEps = 1e-9;

struct UnitProfile {
  int32_t unit_id = -1;
  std::vector<int32_t> node_indices;
  int32_t size = 0;
  int32_t earliest_level = 0;
  int32_t latest_level = 0;
  int32_t peak_level_load = 0;
  std::vector<int32_t> level_hist;
  int32_t interaction = 0;
};

struct WeightedRuntimeInfo {
  double duration = 0.0;
  int32_t current_stream_id = -1;
  int32_t aiv_cores = 0;
  int32_t aic_cores = 0;
};

struct LiteScore {
  double total = 0.0;
  double time_conflict = 0.0;
  int32_t event_local = 0;
  double time_load = 0.0;
  int32_t candidate_stream = 0;
};

struct ResourceScheduleResult {
  bool feasible = false;
  double makespan = 0.0;
};

struct StreamingState {
  std::vector<int32_t> assignment;
  std::vector<double> stream_total_durations;
  std::vector<std::vector<double>> stream_duration_hists;
  std::map<int32_t, std::map<int32_t, int32_t>> origin_stream_to_flow_counts;
  int32_t active_stream_count = 0;
};

struct BestRepairMove {
  int32_t unit_id = -1;
  int32_t from_stream = -1;
  int32_t to_stream = -1;
  double improvement = 0.0;
};

class WeightedStreamMergeSolver {
 public:
  WeightedStreamMergeSolver(const DAGGraph &dag, const std::vector<std::vector<int32_t>> &logical_stream_routes,
                            const WeightedStreamMergeOptions &options)
      : dag_(dag), logical_stream_routes_(logical_stream_routes), options_(options) {}

  graphStatus Solve(std::vector<int32_t> &logical_to_physical_stream) {
    MINIDAG_ASSERT_SUCCESS(BuildNodeOrder(), "Build weighted node order failed.");
    MINIDAG_ASSERT_SUCCESS(BuildEdges(), "Build weighted edges failed.");
    MINIDAG_ASSERT_SUCCESS(BuildLevels(), "Build weighted levels failed.");
    MINIDAG_ASSERT_SUCCESS(BuildUnits(), "Build weighted units failed.");
    if (unit_count_ == 0) {
      logical_to_physical_stream.clear();
      return graphStatus::SUCCESS;
    }
    MINIDAG_ASSERT_SUCCESS(LoadNodeCostProfiles(), "Load weighted node cost profiles failed.");
    BuildUnitDependencyGraph();
    BuildUnitsByEarliestLevel();
    auto state = NewStreamingState();
    for (int32_t level = 0; level <= max_level_; ++level) {
      const auto level_iter = units_by_earliest_level_.find(level);
      if (level_iter == units_by_earliest_level_.end()) {
        continue;
      }
      std::vector<int32_t> recent_units;
      for (const auto unit_id : level_iter->second) {
        const auto candidate_flows = CandidateFlows(unit_id, level, state, true);
        if (candidate_flows.empty()) {
          MINIDAG_LOG_ERROR("No candidate physical stream for weighted unit %d.", unit_id);
          return graphStatus::FAILED;
        }
        const auto best_stream = SelectBestStream(unit_id, level, candidate_flows, state);
        ApplyUnitToStream(unit_id, best_stream, state);
        recent_units.emplace_back(unit_id);
      }
      if ((options_.repair_moves > 0) && (!recent_units.empty())) {
        RepairRecentUnits(level, recent_units, state);
      }
    }

    logical_to_physical_stream = CompactAssignment(state.assignment);
    if (logical_to_physical_stream.empty()) {
      return graphStatus::FAILED;
    }
    return graphStatus::SUCCESS;
  }

 private:
  struct RunningNode {
    double finish_time = 0.0;
    int32_t topo_pos = 0;
    int32_t node_index = 0;
  };

  struct RunningNodeGreater {
    bool operator()(const RunningNode &lhs, const RunningNode &rhs) const {
      const auto finish_cmp = CompareDouble(lhs.finish_time, rhs.finish_time);
      if (finish_cmp != 0) {
        return finish_cmp > 0;
      }
      if (lhs.topo_pos != rhs.topo_pos) {
        return lhs.topo_pos > rhs.topo_pos;
      }
      return lhs.node_index > rhs.node_index;
    }
  };

  using RunningNodeHeap = std::priority_queue<RunningNode, std::vector<RunningNode>, RunningNodeGreater>;

  struct SimulationContext {
    std::vector<int32_t> node_streams;
    std::map<int32_t, std::deque<int32_t>> stream_to_queue;
    std::vector<std::vector<int32_t>> filtered_preds;
    std::vector<char> finished;
    std::vector<char> running_in_stream;
    RunningNodeHeap running_heap;
    double current_time = 0.0;
    int32_t executed_count = 0;
  };

  static int32_t CompareDouble(const double lhs, const double rhs) {
    const auto diff = lhs - rhs;
    if (std::fabs(diff) <= kImproveEps) {
      return 0;
    }
    return (diff < 0.0) ? -1 : 1;
  }

  static bool LiteScoreLess(const LiteScore &lhs, const LiteScore &rhs) {
    const auto total_cmp = CompareDouble(lhs.total, rhs.total);
    if (total_cmp != 0) {
      return lhs.total < rhs.total;
    }
    const auto conflict_cmp = CompareDouble(lhs.time_conflict, rhs.time_conflict);
    if (conflict_cmp != 0) {
      return lhs.time_conflict < rhs.time_conflict;
    }
    if (lhs.event_local != rhs.event_local) {
      return lhs.event_local < rhs.event_local;
    }
    const auto load_cmp = CompareDouble(lhs.time_load, rhs.time_load);
    if (load_cmp != 0) {
      return lhs.time_load < rhs.time_load;
    }
    return lhs.candidate_stream < rhs.candidate_stream;
  }

  graphStatus BuildNodeOrder() {
    topo_nodes_.clear();
    const auto all_nodes = dag_.GetAllNodes();
    topo_nodes_.reserve(all_nodes.size());
    for (size_t idx = 0UL; idx < all_nodes.size(); ++idx) {
      MINIDAG_ASSERT_NOTNULL(all_nodes[idx], "Node at index %zu is nullptr.", idx);
      topo_nodes_.emplace_back(all_nodes[idx]);
    }
    std::stable_sort(topo_nodes_.begin(), topo_nodes_.end(),
                     [](const std::shared_ptr<DAGNode> &lhs, const std::shared_ptr<DAGNode> &rhs) {
                       return lhs->GetTopoId() < rhs->GetTopoId();
                     });
    node_name_to_index_.clear();
    for (size_t idx = 0UL; idx < topo_nodes_.size(); ++idx) {
      node_name_to_index_[topo_nodes_[idx]->GetName()] = static_cast<int32_t>(idx);
    }
    return graphStatus::SUCCESS;
  }

  graphStatus BuildEdges() {
    preds_.assign(topo_nodes_.size(), {});
    succs_.assign(topo_nodes_.size(), {});
    edge_pairs_.clear();
    for (size_t node_index = 0UL; node_index < topo_nodes_.size(); ++node_index) {
      const auto &node = topo_nodes_[node_index];
      std::set<int32_t> unique_pred_indices;
      for (const auto &pred_node : node->GetInputNodes()) {
        const auto pred_iter = node_name_to_index_.find(pred_node->GetName());
        if (pred_iter == node_name_to_index_.end()) {
          continue;
        }
        unique_pred_indices.insert(pred_iter->second);
      }
      preds_[node_index].assign(unique_pred_indices.begin(), unique_pred_indices.end());
      for (const auto pred_index : preds_[node_index]) {
        succs_[pred_index].emplace_back(static_cast<int32_t>(node_index));
        edge_pairs_.emplace_back(pred_index, static_cast<int32_t>(node_index));
      }
    }
    return graphStatus::SUCCESS;
  }

  graphStatus BuildLevels() {
    std::vector<int32_t> indegree(topo_nodes_.size(), 0);
    for (size_t idx = 0UL; idx < preds_.size(); ++idx) {
      indegree[idx] = static_cast<int32_t>(preds_[idx].size());
    }
    std::priority_queue<int32_t, std::vector<int32_t>, std::greater<int32_t>> ready_nodes;
    for (size_t idx = 0UL; idx < indegree.size(); ++idx) {
      if (indegree[idx] == 0) {
        ready_nodes.push(static_cast<int32_t>(idx));
      }
    }

    topo_order_.clear();
    topo_order_.reserve(topo_nodes_.size());
    while (!ready_nodes.empty()) {
      const auto node_index = ready_nodes.top();
      ready_nodes.pop();
      topo_order_.emplace_back(node_index);
      for (const auto succ_index : succs_[node_index]) {
        --indegree[succ_index];
        if (indegree[succ_index] == 0) {
          ready_nodes.push(succ_index);
        }
      }
    }
    if (topo_order_.size() != topo_nodes_.size()) {
      MINIDAG_LOG_ERROR("Weighted merge only supports DAG graphs.");
      return graphStatus::FAILED;
    }

    topo_position_.assign(topo_nodes_.size(), -1);
    levels_.assign(topo_nodes_.size(), 0);
    max_level_ = 0;
    for (size_t pos = 0UL; pos < topo_order_.size(); ++pos) {
      const auto node_index = topo_order_[pos];
      topo_position_[node_index] = static_cast<int32_t>(pos);
      int32_t level = 0;
      for (const auto pred_index : preds_[node_index]) {
        level = std::max(level, levels_[pred_index] + 1);
      }
      levels_[node_index] = level;
      max_level_ = std::max(max_level_, level);
    }
    return graphStatus::SUCCESS;
  }

  graphStatus BuildUnits() {
    unit_count_ = static_cast<int32_t>(logical_stream_routes_.size());
    unit_profiles_.clear();
    unit_profiles_.reserve(logical_stream_routes_.size());
    node_to_unit_.assign(topo_nodes_.size(), -1);

    for (size_t route_id = 0UL; route_id < logical_stream_routes_.size(); ++route_id) {
      MINIDAG_ASSERT_SUCCESS(BuildUnitProfile(route_id), "Build weighted unit profile %zu failed.", route_id);
    }

    return ValidateAllNodesAssigned();
  }

  graphStatus BuildUnitProfile(const size_t route_id) {
    std::vector<int32_t> node_indices;
    node_indices.reserve(logical_stream_routes_[route_id].size());
    for (const auto node_index : logical_stream_routes_[route_id]) {
      MINIDAG_ASSERT_SUCCESS(AddNodeToUnit(route_id, node_index, node_indices),
                             "Add node %d to weighted unit %zu failed.", node_index, route_id);
    }
    if (node_indices.empty()) {
      MINIDAG_LOG_ERROR("Weighted logical stream %zu should not be empty.", route_id);
      return graphStatus::FAILED;
    }
    std::sort(node_indices.begin(), node_indices.end(),
              [this](const int32_t lhs, const int32_t rhs) { return topo_position_[lhs] < topo_position_[rhs]; });
    unit_profiles_.emplace_back(MakeUnitProfile(route_id, std::move(node_indices)));
    return graphStatus::SUCCESS;
  }

  graphStatus AddNodeToUnit(const size_t route_id, const int32_t node_index, std::vector<int32_t> &node_indices) {
    if ((node_index < 0) || (node_index >= static_cast<int32_t>(topo_nodes_.size()))) {
      MINIDAG_LOG_ERROR("Node index %d is out of range [0, %zu).", node_index, topo_nodes_.size());
      return graphStatus::FAILED;
    }
    if (node_to_unit_[node_index] != -1) {
      MINIDAG_LOG_ERROR("Node %s appears in more than one logical stream.", topo_nodes_[node_index]->GetName().c_str());
      return graphStatus::FAILED;
    }
    node_to_unit_[node_index] = static_cast<int32_t>(route_id);
    node_indices.emplace_back(node_index);
    return graphStatus::SUCCESS;
  }

  UnitProfile MakeUnitProfile(const size_t route_id, std::vector<int32_t> node_indices) const {
    std::vector<int32_t> level_hist(static_cast<size_t>(max_level_ + 1), 0);
    int32_t earliest_level = max_level_;
    int32_t latest_level = 0;
    for (const auto node_index : node_indices) {
      const auto level = levels_[node_index];
      ++level_hist[level];
      earliest_level = std::min(earliest_level, level);
      latest_level = std::max(latest_level, level);
    }
    int32_t peak_level_load = 0;
    for (const auto load : level_hist) {
      peak_level_load = std::max(peak_level_load, load);
    }
    return {static_cast<int32_t>(route_id),
            std::move(node_indices),
            static_cast<int32_t>(logical_stream_routes_[route_id].size()),
            earliest_level,
            latest_level,
            peak_level_load,
            std::move(level_hist),
            0};
  }

  graphStatus ValidateAllNodesAssigned() const {
    for (size_t node_index = 0UL; node_index < node_to_unit_.size(); ++node_index) {
      if (node_to_unit_[node_index] < 0) {
        MINIDAG_LOG_ERROR("Node %s is not assigned to any logical stream.", topo_nodes_[node_index]->GetName().c_str());
        return graphStatus::FAILED;
      }
    }
    return graphStatus::SUCCESS;
  }

  graphStatus LoadNodeCostProfiles() {
    node_durations_.assign(topo_nodes_.size(), 0.0);
    node_origin_stream_hints_.assign(topo_nodes_.size(), 0);
    node_aiv_cores_.assign(topo_nodes_.size(), 0);
    node_aic_cores_.assign(topo_nodes_.size(), 0);
    node_bottom_ranks_.assign(topo_nodes_.size(), 0.0);

    size_t valid_cost_count = 0UL;
    size_t missing_cost_count = 0UL;
    for (size_t node_index = 0UL; node_index < topo_nodes_.size(); ++node_index) {
      const auto &node = topo_nodes_[node_index];
      const auto &cost = node->GetCost();
      WeightedRuntimeInfo info;
      info.duration = (cost.execution_time >= 0.0f) ? static_cast<double>(cost.execution_time) : 0.0;
      info.current_stream_id = node_to_unit_[node_index];
      info.aiv_cores = SizeToInt32(cost.vec_block_num);
      info.aic_cores = SizeToInt32(cost.cube_block_num);

      node_durations_[node_index] = info.duration;
      node_origin_stream_hints_[node_index] = info.current_stream_id;
      node_aiv_cores_[node_index] = info.aiv_cores;
      node_aic_cores_[node_index] = info.aic_cores;
      if (cost.execution_time >= 0.0f) {
        ++valid_cost_count;
      } else {
        ++missing_cost_count;
      }
    }
    if (missing_cost_count > 0UL) {
      MINIDAG_LOG_WARN("Weighted stream NodeCost summary: valid=%zu, missing=%zu.", valid_cost_count,
                       missing_cost_count);
    } else {
      MINIDAG_LOG_INFO("Weighted stream NodeCost summary: valid=%zu, missing=%zu.", valid_cost_count,
                       missing_cost_count);
    }

    ComputeBottomRanks();
    BuildResourceUnitProfiles();
    return graphStatus::SUCCESS;
  }

  static int32_t SizeToInt32(const size_t value) {
    const auto limit = static_cast<size_t>(std::numeric_limits<int32_t>::max());
    return static_cast<int32_t>(std::min(value, limit));
  }

  void ComputeBottomRanks() {
    for (auto iter = topo_order_.rbegin(); iter != topo_order_.rend(); ++iter) {
      const auto node_index = *iter;
      double succ_rank = 0.0;
      for (const auto succ_index : succs_[node_index]) {
        succ_rank = std::max(succ_rank, node_bottom_ranks_[succ_index]);
      }
      node_bottom_ranks_[node_index] = node_durations_[node_index] + succ_rank;
    }
  }

  void BuildResourceUnitProfiles() {
    InitResourceUnitProfiles();
    for (const auto &unit : unit_profiles_) {
      BuildResourceUnitProfile(unit);
    }
  }

  void InitResourceUnitProfiles() {
    const auto level_count = static_cast<size_t>(max_level_ + 1);
    unit_total_duration_.assign(static_cast<size_t>(unit_count_), 0.0);
    unit_total_aiv_time_.assign(static_cast<size_t>(unit_count_), 0.0);
    unit_total_aic_time_.assign(static_cast<size_t>(unit_count_), 0.0);
    unit_duration_hist_.assign(static_cast<size_t>(unit_count_), std::vector<double>(level_count, 0.0));
    unit_peak_aiv_hist_.assign(static_cast<size_t>(unit_count_), std::vector<int32_t>(level_count, 0));
    unit_peak_aic_hist_.assign(static_cast<size_t>(unit_count_), std::vector<int32_t>(level_count, 0));
    unit_peak_aiv_.assign(static_cast<size_t>(unit_count_), 0);
    unit_peak_aic_.assign(static_cast<size_t>(unit_count_), 0);
    unit_rank_.assign(static_cast<size_t>(unit_count_), 0.0);
    unit_origin_stream_hint_.assign(static_cast<size_t>(unit_count_), -1);
    unit_active_levels_.assign(static_cast<size_t>(unit_count_), {});
  }

  void BuildResourceUnitProfile(const UnitProfile &unit) {
    std::map<int32_t, int32_t> origin_counter;
    const auto unit_id = unit.unit_id;
    for (const auto node_index : unit.node_indices) {
      AccumulateUnitResourceNode(unit_id, node_index, origin_counter);
    }
    unit_peak_aiv_[unit_id] =
        *std::max_element(unit_peak_aiv_hist_[unit_id].begin(), unit_peak_aiv_hist_[unit_id].end());
    unit_peak_aic_[unit_id] =
        *std::max_element(unit_peak_aic_hist_[unit_id].begin(), unit_peak_aic_hist_[unit_id].end());
    unit_origin_stream_hint_[unit_id] = SelectOriginStreamHint(origin_counter);
    BuildUnitActiveLevels(unit_id);
  }

  void AccumulateUnitResourceNode(const int32_t unit_id, const int32_t node_index,
                                  std::map<int32_t, int32_t> &origin_counter) {
    const auto level = levels_[node_index];
    const auto duration = node_durations_[node_index];
    const auto aiv = node_aiv_cores_[node_index];
    const auto aic = node_aic_cores_[node_index];
    ++origin_counter[node_origin_stream_hints_[node_index]];
    unit_total_duration_[unit_id] += duration;
    unit_total_aiv_time_[unit_id] += duration * static_cast<double>(aiv);
    unit_total_aic_time_[unit_id] += duration * static_cast<double>(aic);
    unit_duration_hist_[unit_id][level] += duration;
    unit_peak_aiv_hist_[unit_id][level] = std::max(unit_peak_aiv_hist_[unit_id][level], aiv);
    unit_peak_aic_hist_[unit_id][level] = std::max(unit_peak_aic_hist_[unit_id][level], aic);
    unit_rank_[unit_id] = std::max(unit_rank_[unit_id], node_bottom_ranks_[node_index]);
  }

  int32_t SelectOriginStreamHint(const std::map<int32_t, int32_t> &origin_counter) const {
    int32_t best_count = -1;
    int32_t best_stream = -1;
    for (const auto &origin_and_count : origin_counter) {
      if ((origin_and_count.second > best_count) ||
          ((origin_and_count.second == best_count) && (origin_and_count.first < best_stream))) {
        best_count = origin_and_count.second;
        best_stream = origin_and_count.first;
      }
    }
    return best_stream;
  }

  void BuildUnitActiveLevels(const int32_t unit_id) {
    for (int32_t level = 0; level <= max_level_; ++level) {
      if ((unit_duration_hist_[unit_id][level] > kImproveEps) || (unit_peak_aiv_hist_[unit_id][level] > 0) ||
          (unit_peak_aic_hist_[unit_id][level] > 0)) {
        unit_active_levels_[unit_id].emplace_back(level);
      }
    }
  }

  void BuildUnitDependencyGraph() {
    unit_pred_edges_.assign(static_cast<size_t>(unit_count_), {});
    unit_succ_edges_.assign(static_cast<size_t>(unit_count_), {});
    std::vector<int32_t> interactions(static_cast<size_t>(unit_count_), 0);
    for (const auto &edge_pair : edge_pairs_) {
      const auto src_unit = node_to_unit_[edge_pair.first];
      const auto dst_unit = node_to_unit_[edge_pair.second];
      if (src_unit == dst_unit) {
        continue;
      }
      ++unit_succ_edges_[src_unit][dst_unit];
      ++unit_pred_edges_[dst_unit][src_unit];
      ++interactions[src_unit];
      ++interactions[dst_unit];
    }
    for (auto &unit : unit_profiles_) {
      unit.interaction = interactions[unit.unit_id];
    }
  }

  void BuildUnitsByEarliestLevel() {
    units_by_earliest_level_.clear();
    for (const auto &unit : unit_profiles_) {
      units_by_earliest_level_[unit.earliest_level].emplace_back(unit.unit_id);
    }
    for (auto &level_units : units_by_earliest_level_) {
      auto &unit_ids = level_units.second;
      std::sort(unit_ids.begin(), unit_ids.end(), [this](const int32_t lhs, const int32_t rhs) {
        if (CompareDouble(unit_total_duration_[lhs], unit_total_duration_[rhs]) != 0) {
          return unit_total_duration_[lhs] > unit_total_duration_[rhs];
        }
        const auto lhs_resource_time = unit_total_aiv_time_[lhs] + unit_total_aic_time_[lhs];
        const auto rhs_resource_time = unit_total_aiv_time_[rhs] + unit_total_aic_time_[rhs];
        if (CompareDouble(lhs_resource_time, rhs_resource_time) != 0) {
          return lhs_resource_time > rhs_resource_time;
        }
        const auto lhs_peak = std::max(unit_peak_aiv_[lhs], unit_peak_aic_[lhs]);
        const auto rhs_peak = std::max(unit_peak_aiv_[rhs], unit_peak_aic_[rhs]);
        if (lhs_peak != rhs_peak) {
          return lhs_peak > rhs_peak;
        }
        if (unit_profiles_[lhs].interaction != unit_profiles_[rhs].interaction) {
          return unit_profiles_[lhs].interaction > unit_profiles_[rhs].interaction;
        }
        if (unit_profiles_[lhs].earliest_level != unit_profiles_[rhs].earliest_level) {
          return unit_profiles_[lhs].earliest_level < unit_profiles_[rhs].earliest_level;
        }
        return lhs < rhs;
      });
    }
  }

  StreamingState NewStreamingState() const {
    const auto level_count = static_cast<size_t>(max_level_ + 1);
    StreamingState state;
    state.assignment.assign(static_cast<size_t>(unit_count_), -1);
    state.stream_total_durations.assign(static_cast<size_t>(options_.physical_stream_limit), 0.0);
    state.stream_duration_hists.assign(static_cast<size_t>(options_.physical_stream_limit),
                                       std::vector<double>(level_count, 0.0));
    state.active_stream_count = 0;
    return state;
  }

  int32_t WindowEnd(const int32_t start_level) const {
    return std::min(max_level_, start_level + options_.window_width - 1);
  }

  double WindowMass(const std::vector<double> &hist, const int32_t start_level) const {
    double mass = 0.0;
    const auto end_level = WindowEnd(start_level);
    for (int32_t level = start_level; level <= end_level; ++level) {
      mass += hist[level];
    }
    return mass;
  }

  double StreamWindowDurationLoad(const StreamingState &state, const int32_t physical_stream,
                                  const int32_t start_level) const {
    return WindowMass(state.stream_duration_hists[physical_stream], start_level);
  }

  std::vector<int32_t> WindowActiveLevels(const int32_t unit_id, const int32_t start_level) const {
    const auto end_level = WindowEnd(start_level);
    std::vector<int32_t> levels;
    for (const auto level : unit_active_levels_[unit_id]) {
      if ((level >= start_level) && (level <= end_level)) {
        levels.emplace_back(level);
      }
    }
    return levels.empty() ? unit_active_levels_[unit_id] : levels;
  }

  std::map<int32_t, int32_t> AdjacentAssignedFlows(const int32_t unit_id,
                                                   const std::vector<int32_t> &assignment) const {
    std::map<int32_t, int32_t> flow_weights;
    for (const auto &pred_flow : unit_pred_edges_[unit_id]) {
      const auto flow = assignment[pred_flow.first];
      if (flow >= 0) {
        flow_weights[flow] += pred_flow.second;
      }
    }
    for (const auto &succ_flow : unit_succ_edges_[unit_id]) {
      const auto flow = assignment[succ_flow.first];
      if (flow >= 0) {
        flow_weights[flow] += succ_flow.second;
      }
    }
    return flow_weights;
  }

  std::vector<int32_t> OriginHintFlows(const int32_t unit_id, const StreamingState &state) const {
    std::vector<int32_t> flows;
    const auto hint_stream_id = unit_origin_stream_hint_[unit_id];
    if (hint_stream_id < 0) {
      return flows;
    }
    const auto hint_iter = state.origin_stream_to_flow_counts.find(hint_stream_id);
    if (hint_iter == state.origin_stream_to_flow_counts.end()) {
      return flows;
    }
    std::vector<std::pair<int32_t, int32_t>> flow_counts(hint_iter->second.begin(), hint_iter->second.end());
    std::sort(
        flow_counts.begin(), flow_counts.end(),
        [&state](const std::pair<int32_t, int32_t> &lhs, const std::pair<int32_t, int32_t> &rhs) {
          if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
          }
          if (CompareDouble(state.stream_total_durations[lhs.first], state.stream_total_durations[rhs.first]) != 0) {
            return state.stream_total_durations[lhs.first] < state.stream_total_durations[rhs.first];
          }
          return lhs.first < rhs.first;
        });
    for (const auto &flow_and_count : flow_counts) {
      flows.emplace_back(flow_and_count.first);
    }
    return flows;
  }

  std::vector<int32_t> CandidateFlows(const int32_t unit_id, const int32_t level, const StreamingState &state,
                                      const bool include_new_flow) const {
    auto candidates = AdjacentCandidateFlows(unit_id, state);
    const auto hint_flows = OriginHintFlows(unit_id, state);
    candidates.insert(candidates.end(), hint_flows.begin(), hint_flows.end());

    const auto light_flows = LightCandidateFlows(level, state);
    candidates.insert(candidates.end(), light_flows.begin(), light_flows.end());
    if (include_new_flow && (state.active_stream_count < options_.physical_stream_limit)) {
      candidates.emplace_back(state.active_stream_count);
    }

    const auto unique_candidates = UniqueLimitedCandidates(candidates);
    if (!unique_candidates.empty()) {
      return unique_candidates;
    }
    return FallbackCandidateFlows(state, include_new_flow);
  }

  std::vector<int32_t> AdjacentCandidateFlows(const int32_t unit_id, const StreamingState &state) const {
    const auto adjacent_flow_weights = AdjacentAssignedFlows(unit_id, state.assignment);
    std::vector<std::pair<int32_t, int32_t>> weighted_flows(adjacent_flow_weights.begin(), adjacent_flow_weights.end());
    std::sort(
        weighted_flows.begin(), weighted_flows.end(),
        [&state](const std::pair<int32_t, int32_t> &lhs, const std::pair<int32_t, int32_t> &rhs) {
          if (lhs.second != rhs.second) {
            return lhs.second > rhs.second;
          }
          if (CompareDouble(state.stream_total_durations[lhs.first], state.stream_total_durations[rhs.first]) != 0) {
            return state.stream_total_durations[lhs.first] < state.stream_total_durations[rhs.first];
          }
          return lhs.first < rhs.first;
        });
    std::vector<int32_t> candidates;
    for (const auto &flow_and_weight : weighted_flows) {
      candidates.emplace_back(flow_and_weight.first);
    }
    return candidates;
  }

  std::vector<int32_t> LightCandidateFlows(const int32_t level, const StreamingState &state) const {
    std::vector<int32_t> light_flows;
    for (int32_t stream = 0; stream < state.active_stream_count; ++stream) {
      light_flows.emplace_back(stream);
    }
    std::sort(light_flows.begin(), light_flows.end(), [this, &state, level](const int32_t lhs, const int32_t rhs) {
      const auto lhs_load = StreamWindowDurationLoad(state, lhs, level);
      const auto rhs_load = StreamWindowDurationLoad(state, rhs, level);
      if (CompareDouble(lhs_load, rhs_load) != 0) {
        return lhs_load < rhs_load;
      }
      if (CompareDouble(state.stream_total_durations[lhs], state.stream_total_durations[rhs]) != 0) {
        return state.stream_total_durations[lhs] < state.stream_total_durations[rhs];
      }
      return lhs < rhs;
    });
    std::vector<int32_t> candidates;
    for (int32_t idx = 0; (idx < options_.light_stream_limit) && (idx < static_cast<int32_t>(light_flows.size()));
         ++idx) {
      candidates.emplace_back(light_flows[idx]);
    }
    return candidates;
  }

  std::vector<int32_t> UniqueLimitedCandidates(const std::vector<int32_t> &candidates) const {
    std::vector<int32_t> unique_candidates;
    std::set<int32_t> seen;
    for (const auto candidate : candidates) {
      if (seen.insert(candidate).second) {
        unique_candidates.emplace_back(candidate);
      }
      if (static_cast<int32_t>(unique_candidates.size()) >= options_.candidate_limit) {
        break;
      }
    }
    return unique_candidates;
  }

  std::vector<int32_t> FallbackCandidateFlows(const StreamingState &state, const bool include_new_flow) const {
    if (include_new_flow && (state.active_stream_count < options_.physical_stream_limit)) {
      return {state.active_stream_count};
    }
    std::vector<int32_t> fallback_candidates;
    for (int32_t stream = 0; (stream < state.active_stream_count) && (stream < options_.candidate_limit); ++stream) {
      fallback_candidates.emplace_back(stream);
    }
    return fallback_candidates;
  }

  LiteScore EvaluateLiteScore(const int32_t unit_id, const int32_t candidate_stream, const int32_t level,
                              const StreamingState &state) const {
    LiteScore score;
    const auto opens_new_stream = (candidate_stream == state.active_stream_count);
    for (const auto &pred_edge : unit_pred_edges_[unit_id]) {
      const auto neighbor_stream = state.assignment[pred_edge.first];
      if ((neighbor_stream >= 0) && (neighbor_stream != candidate_stream)) {
        score.event_local += pred_edge.second;
      }
    }
    for (const auto &succ_edge : unit_succ_edges_[unit_id]) {
      const auto neighbor_stream = state.assignment[succ_edge.first];
      if ((neighbor_stream >= 0) && (neighbor_stream != candidate_stream)) {
        score.event_local += succ_edge.second;
      }
    }
    for (const auto current_level : WindowActiveLevels(unit_id, level)) {
      score.time_conflict +=
          unit_duration_hist_[unit_id][current_level] * state.stream_duration_hists[candidate_stream][current_level];
    }
    score.time_load = state.stream_total_durations[candidate_stream] + unit_total_duration_[unit_id];
    score.candidate_stream = candidate_stream;
    score.total = options_.event_local_weight * static_cast<double>(score.event_local) +
                  options_.time_conflict_weight * score.time_conflict + options_.time_load_weight * score.time_load +
                  options_.new_flow_penalty_weight * static_cast<double>(opens_new_stream ? 1 : 0);
    return score;
  }

  bool ApplyUnitToStream(const int32_t unit_id, const int32_t candidate_stream, StreamingState &state) const {
    const auto opens_new_stream = (candidate_stream == state.active_stream_count);
    if (opens_new_stream) {
      ++state.active_stream_count;
    }
    state.assignment[unit_id] = candidate_stream;
    state.stream_total_durations[candidate_stream] += unit_total_duration_[unit_id];
    for (const auto level : unit_active_levels_[unit_id]) {
      state.stream_duration_hists[candidate_stream][level] += unit_duration_hist_[unit_id][level];
    }
    const auto hint_stream_id = unit_origin_stream_hint_[unit_id];
    if (hint_stream_id >= 0) {
      ++state.origin_stream_to_flow_counts[hint_stream_id][candidate_stream];
    }
    return opens_new_stream;
  }

  int32_t RemoveUnitFromStream(const int32_t unit_id, StreamingState &state) const {
    const auto physical_stream = state.assignment[unit_id];
    if (physical_stream < 0) {
      MINIDAG_LOG_ERROR("Try to remove unassigned weighted unit %d.", unit_id);
      return -1;
    }
    state.assignment[unit_id] = -1;
    state.stream_total_durations[physical_stream] -= unit_total_duration_[unit_id];
    for (const auto level : unit_active_levels_[unit_id]) {
      state.stream_duration_hists[physical_stream][level] -= unit_duration_hist_[unit_id][level];
    }
    const auto hint_stream_id = unit_origin_stream_hint_[unit_id];
    if (hint_stream_id >= 0) {
      auto hint_iter = state.origin_stream_to_flow_counts.find(hint_stream_id);
      if (hint_iter != state.origin_stream_to_flow_counts.end()) {
        auto &flow_counter = hint_iter->second;
        auto flow_iter = flow_counter.find(physical_stream);
        if (flow_iter != flow_counter.end()) {
          --flow_iter->second;
          if (flow_iter->second <= 0) {
            flow_counter.erase(flow_iter);
          }
        }
        if (flow_counter.empty()) {
          state.origin_stream_to_flow_counts.erase(hint_iter);
        }
      }
    }
    return physical_stream;
  }

  ResourceScheduleResult SimulateAssignment(const std::vector<int32_t> &assignment,
                                            const bool include_unassigned) const {
    return BuildAndRunSimulation(assignment, include_unassigned);
  }

  ResourceScheduleResult BuildAndRunSimulation(const std::vector<int32_t> &assignment,
                                               const bool include_unassigned) const {
    std::vector<int32_t> node_streams(topo_nodes_.size(), -1);
    std::vector<char> active_mask(topo_nodes_.size(), 0);
    const auto active_nodes = BuildSimulationNodeStreams(assignment, include_unassigned, node_streams, active_mask);
    if (active_nodes < 0) {
      return {false, 0.0};
    }
    if (active_nodes == 0) {
      return {true, 0.0};
    }

    auto context = NewSimulationContext(node_streams, active_mask);
    return RunSimulation(active_nodes, context);
  }

  SimulationContext NewSimulationContext(const std::vector<int32_t> &node_streams,
                                         const std::vector<char> &active_mask) const {
    SimulationContext context;
    context.node_streams = node_streams;
    context.stream_to_queue = BuildSimulationQueues(node_streams, active_mask);
    context.filtered_preds = BuildSimulationFilteredPreds(active_mask);
    context.finished.assign(topo_nodes_.size(), 0);
    context.running_in_stream.assign(static_cast<size_t>(options_.physical_stream_limit), 0);
    return context;
  }

  ResourceScheduleResult RunSimulation(const int32_t active_nodes, SimulationContext &context) const {
    while (context.executed_count < active_nodes) {
      if (!RunSimulationStep(context)) {
        return {false, context.current_time};
      }
    }
    return {true, context.current_time};
  }

  bool RunSimulationStep(SimulationContext &context) const {
    const auto ready_heads = SimulationReadyHeads(context.stream_to_queue, context.filtered_preds, context.finished,
                                                  context.running_in_stream);
    if (!StartSimulationReadyHead(ready_heads, context)) {
      return false;
    }
    if (context.running_heap.empty()) {
      return false;
    }
    FinishNextSimulationNodes(context);
    return true;
  }

  int32_t BuildSimulationNodeStreams(const std::vector<int32_t> &assignment, const bool include_unassigned,
                                     std::vector<int32_t> &node_streams, std::vector<char> &active_mask) const {
    int32_t active_nodes = 0;
    for (size_t node_index = 0UL; node_index < topo_nodes_.size(); ++node_index) {
      const auto unit_id = node_to_unit_[node_index];
      const auto physical_stream = assignment[unit_id];
      if (physical_stream < 0) {
        if (include_unassigned) {
          MINIDAG_LOG_ERROR("There is still an unassigned weighted unit in final simulation.");
          return -1;
        }
        continue;
      }
      node_streams[node_index] = physical_stream;
      active_mask[node_index] = 1;
      ++active_nodes;
    }
    return active_nodes;
  }

  std::map<int32_t, std::deque<int32_t>> BuildSimulationQueues(const std::vector<int32_t> &node_streams,
                                                               const std::vector<char> &active_mask) const {
    std::map<int32_t, std::deque<int32_t>> stream_to_queue;
    for (const auto node_index : topo_order_) {
      if (active_mask[node_index] == 0) {
        continue;
      }
      stream_to_queue[node_streams[node_index]].emplace_back(node_index);
    }
    return stream_to_queue;
  }

  std::vector<std::vector<int32_t>> BuildSimulationFilteredPreds(const std::vector<char> &active_mask) const {
    std::vector<std::vector<int32_t>> filtered_preds(topo_nodes_.size());
    for (size_t node_index = 0UL; node_index < topo_nodes_.size(); ++node_index) {
      if (active_mask[node_index] == 0) {
        continue;
      }
      for (const auto pred_index : preds_[node_index]) {
        if (active_mask[pred_index] != 0) {
          filtered_preds[node_index].emplace_back(pred_index);
        }
      }
    }
    return filtered_preds;
  }

  std::vector<int32_t> SimulationReadyHeads(const std::map<int32_t, std::deque<int32_t>> &stream_to_queue,
                                            const std::vector<std::vector<int32_t>> &filtered_preds,
                                            const std::vector<char> &finished,
                                            const std::vector<char> &running_in_stream) const {
    std::vector<int32_t> ready_heads;
    for (const auto &stream_and_queue : stream_to_queue) {
      const auto stream = stream_and_queue.first;
      const auto &queue = stream_and_queue.second;
      if (queue.empty() || (running_in_stream[stream] != 0)) {
        continue;
      }
      if (AreSimulationPredsFinished(queue.front(), filtered_preds, finished)) {
        ready_heads.emplace_back(queue.front());
      }
    }
    SortSimulationReadyHeads(ready_heads);
    return ready_heads;
  }

  bool AreSimulationPredsFinished(const int32_t node_index, const std::vector<std::vector<int32_t>> &filtered_preds,
                                  const std::vector<char> &finished) const {
    for (const auto pred_index : filtered_preds[node_index]) {
      if (finished[pred_index] == 0) {
        return false;
      }
    }
    return true;
  }

  void SortSimulationReadyHeads(std::vector<int32_t> &ready_heads) const {
    std::sort(ready_heads.begin(), ready_heads.end(), [this](const int32_t lhs, const int32_t rhs) {
      if (CompareDouble(node_bottom_ranks_[lhs], node_bottom_ranks_[rhs]) != 0) {
        return node_bottom_ranks_[lhs] > node_bottom_ranks_[rhs];
      }
      if (CompareDouble(node_durations_[lhs], node_durations_[rhs]) != 0) {
        return node_durations_[lhs] > node_durations_[rhs];
      }
      if (topo_position_[lhs] != topo_position_[rhs]) {
        return topo_position_[lhs] < topo_position_[rhs];
      }
      return lhs < rhs;
    });
  }

  bool StartSimulationReadyHead(const std::vector<int32_t> &ready_heads, SimulationContext &context) const {
    if (ready_heads.empty()) {
      return true;
    }
    const auto node_index = ready_heads.front();
    const auto stream = context.node_streams[node_index];
    auto &queue = context.stream_to_queue[stream];
    if (queue.empty() || (queue.front() != node_index)) {
      MINIDAG_LOG_ERROR("Weighted stream simulation queue state is inconsistent.");
      return false;
    }
    queue.pop_front();
    context.running_in_stream[stream] = 1;
    context.running_heap.push(
        {context.current_time + node_durations_[node_index], topo_position_[node_index], node_index});
    return true;
  }

  void FinishNextSimulationNodes(SimulationContext &context) const {
    const auto next_finish = context.running_heap.top().finish_time;
    context.current_time = next_finish;
    while (!context.running_heap.empty() && (CompareDouble(context.running_heap.top().finish_time, next_finish) == 0)) {
      const auto finished_node = context.running_heap.top();
      context.running_heap.pop();
      context.finished[finished_node.node_index] = 1;
      context.running_in_stream[context.node_streams[finished_node.node_index]] = 0;
      ++context.executed_count;
    }
  }

  int32_t SelectBestStream(const int32_t unit_id, const int32_t level, const std::vector<int32_t> &candidate_flows,
                           StreamingState &state) const {
    std::vector<LiteScore> scored_candidates;
    scored_candidates.reserve(candidate_flows.size());
    for (const auto candidate_stream : candidate_flows) {
      scored_candidates.emplace_back(EvaluateLiteScore(unit_id, candidate_stream, level, state));
    }
    std::sort(scored_candidates.begin(), scored_candidates.end(), LiteScoreLess);
    if (options_.resim_candidate_limit <= 0) {
      return scored_candidates.front().candidate_stream;
    }

    bool has_best_resim = false;
    double best_resim_span = 0.0;
    LiteScore best_resim_score;
    const auto resim_limit = std::min(static_cast<int32_t>(scored_candidates.size()), options_.resim_candidate_limit);
    for (int32_t idx = 0; idx < resim_limit; ++idx) {
      const auto &candidate = scored_candidates[idx];
      const auto previous_active_stream_count = state.active_stream_count;
      (void)ApplyUnitToStream(unit_id, candidate.candidate_stream, state);
      const auto schedule = SimulateAssignment(state.assignment, false);
      (void)RemoveUnitFromStream(unit_id, state);
      state.active_stream_count = previous_active_stream_count;
      if (!schedule.feasible) {
        continue;
      }
      if ((!has_best_resim) || (CompareDouble(schedule.makespan, best_resim_span) < 0) ||
          ((CompareDouble(schedule.makespan, best_resim_span) == 0) && LiteScoreLess(candidate, best_resim_score))) {
        has_best_resim = true;
        best_resim_span = schedule.makespan;
        best_resim_score = candidate;
      }
    }
    return has_best_resim ? best_resim_score.candidate_stream : scored_candidates.front().candidate_stream;
  }

  void RepairRecentUnits(const int32_t level, const std::vector<int32_t> &recent_unit_ids,
                         StreamingState &state) const {
    int32_t move_count = 0;
    while (move_count < options_.repair_moves) {
      BestRepairMove best_move;
      for (const auto unit_id : recent_unit_ids) {
        const auto previous_active_stream_count = state.active_stream_count;
        const auto current_stream = RemoveUnitFromStream(unit_id, state);
        if (current_stream < 0) {
          return;
        }
        const auto current_score = EvaluateLiteScore(unit_id, current_stream, level, state);
        auto candidate_flows = CandidateFlows(unit_id, level, state, false);
        if (std::find(candidate_flows.begin(), candidate_flows.end(), current_stream) == candidate_flows.end()) {
          candidate_flows.emplace_back(current_stream);
        }
        const auto best_candidate = SelectBestStream(unit_id, level, candidate_flows, state);
        const auto best_score = EvaluateLiteScore(unit_id, best_candidate, level, state);
        (void)ApplyUnitToStream(unit_id, current_stream, state);
        state.active_stream_count = previous_active_stream_count;
        const auto improvement = current_score.total - best_score.total;
        if ((best_candidate != current_stream) && (improvement > kImproveEps) &&
            (improvement > best_move.improvement)) {
          best_move = {unit_id, current_stream, best_candidate, improvement};
        }
      }
      if (best_move.unit_id < 0) {
        return;
      }
      (void)RemoveUnitFromStream(best_move.unit_id, state);
      (void)ApplyUnitToStream(best_move.unit_id, best_move.to_stream, state);
      ++move_count;
    }
  }

  std::vector<int32_t> CompactAssignment(const std::vector<int32_t> &assignment) const {
    std::set<int32_t> used_streams;
    for (const auto stream : assignment) {
      if (stream < 0) {
        MINIDAG_LOG_ERROR("There is still an unassigned weighted unit when compacting assignment.");
        return {};
      }
      used_streams.insert(stream);
    }
    if (used_streams.empty()) {
      MINIDAG_LOG_ERROR("Weighted merge should produce at least one physical stream.");
      return {};
    }

    std::map<int32_t, int32_t> remap;
    int32_t next_stream = 0;
    for (const auto stream : used_streams) {
      remap.emplace(stream, next_stream++);
    }
    std::vector<int32_t> compact_assignment;
    compact_assignment.reserve(assignment.size());
    for (const auto stream : assignment) {
      compact_assignment.emplace_back(remap.at(stream));
    }
    return compact_assignment;
  }

  const DAGGraph &dag_;
  const std::vector<std::vector<int32_t>> &logical_stream_routes_;
  const WeightedStreamMergeOptions &options_;

  std::vector<std::shared_ptr<DAGNode>> topo_nodes_;
  std::unordered_map<std::string, int32_t> node_name_to_index_;
  std::vector<std::vector<int32_t>> preds_;
  std::vector<std::vector<int32_t>> succs_;
  std::vector<int32_t> topo_order_;
  std::vector<int32_t> topo_position_;
  std::vector<int32_t> levels_;
  int32_t max_level_ = 0;
  std::vector<std::pair<int32_t, int32_t>> edge_pairs_;

  std::vector<UnitProfile> unit_profiles_;
  std::vector<int32_t> node_to_unit_;
  int32_t unit_count_ = 0;
  std::vector<std::map<int32_t, int32_t>> unit_pred_edges_;
  std::vector<std::map<int32_t, int32_t>> unit_succ_edges_;
  std::map<int32_t, std::vector<int32_t>> units_by_earliest_level_;

  std::vector<double> node_durations_;
  std::vector<int32_t> node_origin_stream_hints_;
  std::vector<int32_t> node_aiv_cores_;
  std::vector<int32_t> node_aic_cores_;
  std::vector<double> node_bottom_ranks_;

  std::vector<double> unit_total_duration_;
  std::vector<double> unit_total_aiv_time_;
  std::vector<double> unit_total_aic_time_;
  std::vector<std::vector<double>> unit_duration_hist_;
  std::vector<std::vector<int32_t>> unit_peak_aiv_hist_;
  std::vector<std::vector<int32_t>> unit_peak_aic_hist_;
  std::vector<int32_t> unit_peak_aiv_;
  std::vector<int32_t> unit_peak_aic_;
  std::vector<double> unit_rank_;
  std::vector<int32_t> unit_origin_stream_hint_;
  std::vector<std::vector<int32_t>> unit_active_levels_;
};

graphStatus ValidateWeightedStreamMergeOptions(const WeightedStreamMergeOptions &options) {
  if (options.physical_stream_limit <= 0) {
    MINIDAG_LOG_ERROR("Weighted stream merge physical stream limit must be greater than 0.");
    return graphStatus::FAILED;
  }
  if (options.window_width <= 0) {
    MINIDAG_LOG_ERROR("Weighted stream merge window width must be greater than 0.");
    return graphStatus::FAILED;
  }
  if (options.candidate_limit <= 0) {
    MINIDAG_LOG_ERROR("Weighted stream merge candidate limit must be greater than 0.");
    return graphStatus::FAILED;
  }
  if (options.light_stream_limit <= 0) {
    MINIDAG_LOG_ERROR("Weighted stream merge light_stream_limit must be greater than 0.");
    return graphStatus::FAILED;
  }
  if (options.repair_moves < 0) {
    MINIDAG_LOG_ERROR("Weighted stream merge repair_moves must be greater than or equal to 0.");
    return graphStatus::FAILED;
  }
  if (options.resim_candidate_limit < 0) {
    MINIDAG_LOG_ERROR("Weighted stream merge resim_candidate_limit must be greater than or equal to 0.");
    return graphStatus::FAILED;
  }
  return graphStatus::SUCCESS;
}
}  // namespace

WeightedStreamMerger::WeightedStreamMerger(const WeightedStreamMergeOptions &options) : options_(options) {}

graphStatus WeightedStreamMerger::Merge(const DAGGraph &dag,
                                        const std::vector<std::vector<int32_t>> &logical_stream_routes,
                                        std::vector<int32_t> &logical_to_physical_stream) const {
  logical_to_physical_stream.clear();
  MINIDAG_ASSERT_SUCCESS(ValidateWeightedStreamMergeOptions(options_),
                         "Validate weighted stream merge options failed.");
  if (logical_stream_routes.empty()) {
    return graphStatus::SUCCESS;
  }

  WeightedStreamMergeSolver solver(dag, logical_stream_routes, options_);
  return solver.Solve(logical_to_physical_stream);
}

}  // namespace minidag
