/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/dag/dag_stream_merger.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>
#include "graph/build/dag/dag_log.h"

namespace minidag {
namespace {
constexpr double kMergeImproveEps = 1e-9;

int32_t CompareDouble(const double lhs, const double rhs) {
  const auto diff = lhs - rhs;
  if (std::fabs(diff) <= kMergeImproveEps) {
    return 0;
  }
  return (diff < 0.0) ? -1 : 1;
}

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

struct ConcurrentScore {
  double total = 0.0;
  int32_t event_local = 0;
  int32_t parallel_conflict = 0;
  int32_t new_flow_penalty = 0;
  int32_t main_stream_bonus = 0;
};

bool operator<(const ConcurrentScore &lhs, const ConcurrentScore &rhs) {
  const auto total_cmp = CompareDouble(lhs.total, rhs.total);
  if (total_cmp != 0) {
    return lhs.total < rhs.total;
  }
  if (lhs.event_local != rhs.event_local) {
    return lhs.event_local < rhs.event_local;
  }
  if (lhs.parallel_conflict != rhs.parallel_conflict) {
    return lhs.parallel_conflict < rhs.parallel_conflict;
  }
  if (lhs.new_flow_penalty != rhs.new_flow_penalty) {
    return lhs.new_flow_penalty < rhs.new_flow_penalty;
  }
  return lhs.main_stream_bonus < rhs.main_stream_bonus;
}

struct LiteScore {
  double total = 0.0;
  int32_t event_local = 0;
  double window_balance = 0.0;
  int32_t total_load = 0;
  int32_t candidate_stream = 0;
};

bool operator<(const LiteScore &lhs, const LiteScore &rhs) {
  const auto total_cmp = CompareDouble(lhs.total, rhs.total);
  if (total_cmp != 0) {
    return lhs.total < rhs.total;
  }
  if (lhs.event_local != rhs.event_local) {
    return lhs.event_local < rhs.event_local;
  }
  const auto window_balance_cmp = CompareDouble(lhs.window_balance, rhs.window_balance);
  if (window_balance_cmp != 0) {
    return lhs.window_balance < rhs.window_balance;
  }
  if (lhs.total_load != rhs.total_load) {
    return lhs.total_load < rhs.total_load;
  }
  return lhs.candidate_stream < rhs.candidate_stream;
}

class StreamMergeSolver {
 public:
  // 构造流合并求解器，按逻辑流路由将节点分配到物理流
  StreamMergeSolver(const DAGGraph& dag, const std::vector<std::vector<int32_t>> &logical_stream_routes,
                      const StreamMergeOptions &options)
      : dag_(dag), logical_stream_routes_(logical_stream_routes), options_(options) {}

  graphStatus Solve(std::vector<int32_t> &logical_to_physical_stream) {
    if (BuildNodeOrder() != graphStatus::SUCCESS) return graphStatus::FAILED;
    if (BuildEdges() != graphStatus::SUCCESS) return graphStatus::FAILED;
    if (BuildLevels() != graphStatus::SUCCESS) return graphStatus::FAILED;
    if (BuildUnits() != graphStatus::SUCCESS) return graphStatus::FAILED;
    if (unit_count_ == 0) {
      logical_to_physical_stream.clear();
      return graphStatus::SUCCESS;
    }
    BuildUnitDependencyGraph();
    BuildUnitsByEarliestLevel();
    if (options_.strategy == StreamMergeStrategy::kLoadBalance) {
      MINIDAG_LOG_INFO("Use kLoadBalance strategy.");
      return SolveLoadBalance(logical_to_physical_stream);
    }
    BuildGlobalLevelLoads();
    BuildUnitSoloMass();
    MINIDAG_LOG_INFO("Use kMainStream strategy.");
    return SolveMainStream(logical_to_physical_stream);
  }

 private:
  struct BestRepairMove {
    int32_t unit_id = -1;
    int32_t from_stream = -1;
    int32_t to_stream = -1;
    double improvement = 0.0;
  };

  graphStatus SolveLoadBalance(std::vector<int32_t> &logical_to_physical_stream) const {
    std::vector<int32_t> assignment(static_cast<size_t>(unit_count_), -1);
    std::vector<int32_t> stream_sizes;
    std::vector<std::vector<int32_t>> stream_level_loads;
    int32_t active_stream_count = 0;
    for (int32_t level = 0; level <= max_level_; ++level) {
      const auto level_iter = units_by_earliest_level_.find(level);
      if (level_iter == units_by_earliest_level_.end()) {
        continue;
      }

      std::vector<int32_t> recent_unit_ids;
      for (const auto unit_id : level_iter->second) {
        const auto candidate_flows = LoadBalanceCandidateFlows(unit_id, level, assignment, stream_sizes,
                                                             stream_level_loads, active_stream_count, true);
        if (candidate_flows.empty()) {
          MINIDAG_LOG_ERROR("No candidate physical stream for unit %d.", unit_id);
          return graphStatus::FAILED;
        }

        int32_t best_stream = candidate_flows.front();
        auto best_score = EvaluateLiteScore(unit_id, best_stream, level, assignment, stream_sizes, stream_level_loads,
                                            active_stream_count);
        for (size_t idx = 1UL; idx < candidate_flows.size(); ++idx) {
          const auto candidate_stream = candidate_flows[idx];
          const auto score = EvaluateLiteScore(unit_id, candidate_stream, level, assignment, stream_sizes,
                                               stream_level_loads, active_stream_count);
          if (score < best_score) {
            best_score = score;
            best_stream = candidate_stream;
          }
        }
        active_stream_count = ApplyUnitToLoadBalanceStream(unit_id, best_stream, assignment, stream_sizes,
                                                         stream_level_loads, active_stream_count);
        recent_unit_ids.emplace_back(unit_id);
      }

      if ((options_.repair_moves > 0) && (!recent_unit_ids.empty())) {
        RepairLoadBalanceRecentUnits(level, recent_unit_ids, assignment, stream_sizes, stream_level_loads,
                                   active_stream_count);
      }
    }

    logical_to_physical_stream = CompactAssignment(assignment);
    return graphStatus::SUCCESS;
  }

  graphStatus SolveMainStream(std::vector<int32_t> &logical_to_physical_stream) const {
    std::vector<int32_t> assignment(static_cast<size_t>(unit_count_), -1);
    std::vector<std::vector<int32_t>> stream_level_loads;
    int32_t active_stream_count = 0;
    for (int32_t level = 0; level <= max_level_; ++level) {
      const auto level_iter = units_by_earliest_level_.find(level);
      if (level_iter == units_by_earliest_level_.end()) {
        continue;
      }

      std::vector<int32_t> recent_unit_ids;
      for (const auto unit_id : level_iter->second) {
        const auto candidate_flows =
            MainStreamCandidateFlows(unit_id, level, assignment, stream_level_loads, active_stream_count, true);
        if (candidate_flows.empty()) {
          MINIDAG_LOG_ERROR("No candidate physical stream for unit %d.", unit_id);
          return graphStatus::FAILED;
        }

        int32_t best_stream = candidate_flows.front();
        auto best_score = EvaluateConcurrentScore(unit_id, best_stream, level, assignment, stream_level_loads,
                                                  active_stream_count);
        for (size_t idx = 1UL; idx < candidate_flows.size(); ++idx) {
          const auto candidate_stream = candidate_flows[idx];
          const auto score =
              EvaluateConcurrentScore(unit_id, candidate_stream, level, assignment, stream_level_loads,
                                      active_stream_count);
          if (score < best_score) {
            best_score = score;
            best_stream = candidate_stream;
          }
        }
        active_stream_count = ApplyUnitToMainStream(unit_id, best_stream, assignment, stream_level_loads,
                                                         active_stream_count);
        recent_unit_ids.emplace_back(unit_id);
      }

      if ((options_.repair_moves > 0) && (!recent_unit_ids.empty())) {
        RepairMainStreamRecentUnits(level, recent_unit_ids, assignment, stream_level_loads, active_stream_count);
      }
    }

    logical_to_physical_stream = CompactAssignment(assignment);
    return graphStatus::SUCCESS;
  }

  // 建立拓扑序：按 topo id 升序排列所有节点
  graphStatus BuildNodeOrder() {
    topo_nodes_.clear();
    auto all_nodes = dag_.GetAllNodes();
    for (size_t idx = 0UL; idx < all_nodes.size(); ++idx) {
      if (all_nodes[idx] == nullptr) {
        MINIDAG_LOG_ERROR("Node at index %zu is nullptr.", idx);
        return graphStatus::FAILED;
      }
      topo_nodes_.emplace_back(all_nodes[idx]);
    }
    std::stable_sort(topo_nodes_.begin(), topo_nodes_.end(),
      [](const std::shared_ptr<DAGNode> &lhs, const std::shared_ptr<DAGNode> &rhs) {
        return lhs->GetTopoId() < rhs->GetTopoId();
      });

    // 建立节点名称到索引的映射，用于快速查找前驱/后继
    node_name_to_index_.clear();
    node_name_to_index_.reserve(topo_nodes_.size());
    for (size_t idx = 0UL; idx < topo_nodes_.size(); ++idx) {
      node_name_to_index_[topo_nodes_[idx]->GetName()] = static_cast<int32_t>(idx);
    }
    return graphStatus::SUCCESS;
  }

  // 构建前驱/后继边表，用于拓扑排序和依赖分析
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
      const int32_t node_index = ready_nodes.top();
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
      MINIDAG_LOG_ERROR("Merge only supports DAG graphs.");
      return graphStatus::FAILED;
    }

    topo_position_.assign(topo_nodes_.size(), -1);
    levels_.assign(topo_nodes_.size(), 0);
    max_level_ = 0;
    for (size_t pos = 0UL; pos < topo_order_.size(); ++pos) {
      const int32_t node_index = topo_order_[pos];
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

  graphStatus BuildUnitProfile(const size_t route_id, std::vector<int32_t> &node_indices) {
    node_indices.reserve(logical_stream_routes_[route_id].size());
    for (const auto node_index : logical_stream_routes_[route_id]) {
      if (node_index < 0 || node_index >= static_cast<int32_t>(topo_nodes_.size())) {
        MINIDAG_LOG_ERROR("Node index %d is out of range [0, %zu).", node_index, topo_nodes_.size());
        return graphStatus::FAILED;
      }
      if (node_to_unit_[node_index] != -1) {
        MINIDAG_LOG_ERROR("Node %s appears in more than one logical stream.",
                         topo_nodes_[node_index]->GetName().c_str());
        return graphStatus::FAILED;
      }
      node_to_unit_[node_index] = static_cast<int32_t>(route_id);
      node_indices.emplace_back(node_index);
    }

    if (node_indices.empty()) {
      MINIDAG_LOG_ERROR("Logical stream %zu should not be empty.", route_id);
      return graphStatus::FAILED;
    }
    std::sort(node_indices.begin(), node_indices.end(), [this](const int32_t lhs, const int32_t rhs) {
      return topo_position_[lhs] < topo_position_[rhs];
    });

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
    const auto node_count = static_cast<int32_t>(node_indices.size());
    unit_profiles_.push_back({static_cast<int32_t>(route_id), std::move(node_indices), node_count, earliest_level,
                              latest_level, peak_level_load, std::move(level_hist), 0});
    return graphStatus::SUCCESS;
  }

  graphStatus ValidateAllNodesAssigned() const {
    for (size_t node_index = 0UL; node_index < node_to_unit_.size(); ++node_index) {
      if (node_to_unit_[node_index] < 0) {
        MINIDAG_LOG_ERROR("Node %s is not assigned to any logical stream.",
                         topo_nodes_[node_index]->GetName().c_str());
        return graphStatus::FAILED;
      }
    }
    return graphStatus::SUCCESS;
  }

  // 构建logical stream routes，记录每个单元包含的节点和各层级分布
  graphStatus BuildUnits() {
    unit_count_ = static_cast<int32_t>(logical_stream_routes_.size());
    unit_profiles_.clear();
    unit_profiles_.reserve(logical_stream_routes_.size());
    node_to_unit_.assign(topo_nodes_.size(), -1);

    for (size_t route_id = 0UL; route_id < logical_stream_routes_.size(); ++route_id) {
      std::vector<int32_t> node_indices;
      if (BuildUnitProfile(route_id, node_indices) != graphStatus::SUCCESS) {
        return graphStatus::FAILED;
      }
    }

    return ValidateAllNodesAssigned();
  }

  void BuildGlobalLevelLoads() {
    global_level_loads_.assign(static_cast<size_t>(max_level_ + 1), 0);
    for (const auto level : levels_) {
      ++global_level_loads_[level];
    }
  }

  void BuildUnitSoloMass() {
    unit_solo_mass_.assign(static_cast<size_t>(unit_count_), 0);
    for (const auto &unit : unit_profiles_) {
      int32_t solo_mass = 0;
      for (size_t level = 0UL; level < unit.level_hist.size(); ++level) {
        if (global_level_loads_[level] == 1) {
          solo_mass += unit.level_hist[level];
        }
      }
      unit_solo_mass_[unit.unit_id] = solo_mass;
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
    for (auto &level_to_units : units_by_earliest_level_) {
      auto &unit_ids = level_to_units.second;
      std::sort(unit_ids.begin(), unit_ids.end(), [this](const int32_t lhs, const int32_t rhs) {
        const auto &left = unit_profiles_[lhs];
        const auto &right = unit_profiles_[rhs];
        if (left.size != right.size) {
          return left.size > right.size;
        }
        if (left.interaction != right.interaction) {
          return left.interaction > right.interaction;
        }
        if (left.peak_level_load != right.peak_level_load) {
          return left.peak_level_load > right.peak_level_load;
        }
        if (left.earliest_level != right.earliest_level) {
          return left.earliest_level < right.earliest_level;
        }
        return left.unit_id < right.unit_id;
      });
    }
  }

  int32_t WindowEnd(const int32_t start_level) const {
    return std::min(max_level_, start_level + options_.window_width - 1);
  }

  int32_t WindowMass(const std::vector<int32_t> &hist, const int32_t start_level) const {
    int32_t mass = 0;
    const auto end_level = WindowEnd(start_level);
    for (int32_t level = start_level; level <= end_level; ++level) {
      mass += hist[level];
    }
    return mass;
  }

  int32_t StreamWindowLoad(const std::vector<std::vector<int32_t>> &stream_level_loads, const int32_t physical_stream,
                           const int32_t start_level) const {
    return WindowMass(stream_level_loads[physical_stream], start_level);
  }

  bool MustAssignToMainStream(const int32_t unit_id, const int32_t level) const {
    return (global_level_loads_[level] == 1) || (unit_solo_mass_[unit_id] == unit_profiles_[unit_id].size);
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

  int32_t ParallelConflictValue(const std::vector<int32_t> &unit_hist, const std::vector<int32_t> &stream_hist,
                                 const int32_t start_level) const {
    int32_t conflict = 0;
    const auto end_level = WindowEnd(start_level);
    for (int32_t level = start_level; level <= end_level; ++level) {
      conflict += unit_hist[level] * stream_hist[level];
    }
    return conflict;
  }

  std::vector<int32_t> DeduplicateCandidates(std::vector<int32_t> &candidates,
                                              const int32_t active_stream_count,
                                              const bool include_new_flow) const {
    if (include_new_flow && (active_stream_count < options_.physical_stream_limit)) {
      candidates.emplace_back(active_stream_count);
    }

    std::vector<int32_t> unique_candidates;
    unique_candidates.reserve(candidates.size());
    std::set<int32_t> seen;
    for (const auto candidate_stream : candidates) {
      if (seen.insert(candidate_stream).second) {
        unique_candidates.emplace_back(candidate_stream);
      }
      if (static_cast<int32_t>(unique_candidates.size()) >= options_.candidate_limit) {
        break;
      }
    }

    if (!unique_candidates.empty()) {
      return unique_candidates;
    }
    if (include_new_flow && (active_stream_count < options_.physical_stream_limit)) {
      return {active_stream_count};
    }

    std::vector<int32_t> fallback_candidates;
    for (int32_t stream = 0; (stream < active_stream_count) && (stream < options_.candidate_limit); ++stream) {
      fallback_candidates.emplace_back(stream);
    }
    return fallback_candidates;
  }

  std::vector<int32_t> LoadBalanceCandidateFlows(const int32_t unit_id, const int32_t level,
                                                const std::vector<int32_t> &assignment,
                                                const std::vector<int32_t> &stream_sizes,
                                                const std::vector<std::vector<int32_t>> &stream_level_loads,
                                                const int32_t active_stream_count,
                                                const bool include_new_flow) const {
    std::vector<int32_t> candidates;
    AddAdjacentFlowCandidatesWithSize(candidates, unit_id, assignment, stream_sizes);
    AddLightStreamCandidates(candidates, level, stream_sizes, stream_level_loads, active_stream_count);
    return DeduplicateCandidates(candidates, active_stream_count, include_new_flow);
  }

  int32_t CalcEventLocal(const int32_t unit_id, const int32_t candidate_stream,
                          const std::vector<int32_t> &assignment) const {
    int32_t event_local = 0;
    for (const auto &pred_edge : unit_pred_edges_[unit_id]) {
      const auto neighbor_stream = assignment[pred_edge.first];
      if ((neighbor_stream >= 0) && (neighbor_stream != candidate_stream)) {
        event_local += pred_edge.second;
      }
    }
    for (const auto &succ_edge : unit_succ_edges_[unit_id]) {
      const auto neighbor_stream = assignment[succ_edge.first];
      if ((neighbor_stream >= 0) && (neighbor_stream != candidate_stream)) {
        event_local += succ_edge.second;
      }
    }
    return event_local;
  }

  void AddMainStreamCandidate(std::vector<int32_t> &candidates,
                               const int32_t active_stream_count,
                               const bool include_new_flow) const {
    if (active_stream_count > options_.main_stream) {
      candidates.emplace_back(options_.main_stream);
    } else if (include_new_flow && (active_stream_count == 0)) {
      candidates.emplace_back(options_.main_stream);
    }
  }

  void AddAdjacentFlowCandidates(std::vector<int32_t> &candidates,
                                  const int32_t unit_id,
                                  const std::vector<int32_t> &assignment) const {
    const auto adjacent_flow_weights = AdjacentAssignedFlows(unit_id, assignment);
    std::vector<std::pair<int32_t, int32_t>> weighted_flows(adjacent_flow_weights.begin(), adjacent_flow_weights.end());
    std::sort(weighted_flows.begin(), weighted_flows.end(), [](const auto &lhs, const auto &rhs) {
      if (lhs.second != rhs.second) { return lhs.second > rhs.second; }
      return lhs.first < rhs.first;
    });
    for (const auto &flow_and_weight : weighted_flows) {
      candidates.emplace_back(flow_and_weight.first);
    }
  }

  void AddLowConflictCandidates(std::vector<int32_t> &candidates,
                                 const int32_t unit_id,
                                 const int32_t level,
                                 const std::vector<std::vector<int32_t>> &stream_level_loads,
                                 const int32_t active_stream_count) const {
    std::vector<std::pair<int32_t, int32_t>> low_conflict_flows;
    low_conflict_flows.reserve(static_cast<size_t>(active_stream_count));
    const auto &unit_hist = unit_profiles_[unit_id].level_hist;
    for (int32_t physical_stream = 0; physical_stream < active_stream_count; ++physical_stream) {
      low_conflict_flows.emplace_back(
          ParallelConflictValue(unit_hist, stream_level_loads[physical_stream], level), physical_stream);
    }
    std::sort(low_conflict_flows.begin(), low_conflict_flows.end(), [](const auto &lhs, const auto &rhs) {
      if (lhs.first != rhs.first) { return lhs.first < rhs.first; }
      return lhs.second < rhs.second;
    });
    for (int32_t idx = 0; (idx < options_.low_conflict_limit) && (idx < static_cast<int32_t>(low_conflict_flows.size()));
         ++idx) {
      candidates.emplace_back(low_conflict_flows[idx].second);
    }
  }

  void AddAdjacentFlowCandidatesWithSize(std::vector<int32_t> &candidates,
                                          const int32_t unit_id,
                                          const std::vector<int32_t> &assignment,
                                          const std::vector<int32_t> &stream_sizes) const {
    const auto adjacent_flow_weights = AdjacentAssignedFlows(unit_id, assignment);
    std::vector<std::pair<int32_t, int32_t>> weighted_flows(adjacent_flow_weights.begin(), adjacent_flow_weights.end());
    std::sort(weighted_flows.begin(), weighted_flows.end(),
              [&stream_sizes](const auto &lhs, const auto &rhs) {
                if (lhs.second != rhs.second) { return lhs.second > rhs.second; }
                const auto lhs_size = stream_sizes[lhs.first];
                const auto rhs_size = stream_sizes[rhs.first];
                if (lhs_size != rhs_size) { return lhs_size < rhs_size; }
                return lhs.first < rhs.first;
              });
    for (const auto &flow_and_weight : weighted_flows) {
      candidates.emplace_back(flow_and_weight.first);
    }
  }

  void AddLightStreamCandidates(std::vector<int32_t> &candidates,
                                 const int32_t level,
                                 const std::vector<int32_t> &stream_sizes,
                                 const std::vector<std::vector<int32_t>> &stream_level_loads,
                                 const int32_t active_stream_count) const {
    std::vector<std::pair<std::pair<int32_t, int32_t>, int32_t>> light_flows;
    light_flows.reserve(static_cast<size_t>(active_stream_count));
    for (int32_t physical_stream = 0; physical_stream < active_stream_count; ++physical_stream) {
      light_flows.emplace_back(std::make_pair(StreamWindowLoad(stream_level_loads, physical_stream, level),
                                               stream_sizes[physical_stream]),
                               physical_stream);
    }
    std::sort(light_flows.begin(), light_flows.end(), [](const auto &lhs, const auto &rhs) {
      if (lhs.first.first != rhs.first.first) { return lhs.first.first < rhs.first.first; }
      if (lhs.first.second != rhs.first.second) { return lhs.first.second < rhs.first.second; }
      return lhs.second < rhs.second;
    });
    for (int32_t idx = 0; (idx < options_.light_stream_limit) && (idx < static_cast<int32_t>(light_flows.size()));
         ++idx) {
      candidates.emplace_back(light_flows[idx].second);
    }
  }

  std::vector<int32_t> MainStreamCandidateFlows(const int32_t unit_id, const int32_t level,
                                                const std::vector<int32_t> &assignment,
                                                const std::vector<std::vector<int32_t>> &stream_level_loads,
                                                const int32_t active_stream_count,
                                                const bool include_new_flow) const {
    if (MustAssignToMainStream(unit_id, level)) {
      return {options_.main_stream};
    }
    std::vector<int32_t> candidates;
    AddMainStreamCandidate(candidates, active_stream_count, include_new_flow);
    AddAdjacentFlowCandidates(candidates, unit_id, assignment);
    AddLowConflictCandidates(candidates, unit_id, level, stream_level_loads, active_stream_count);
    return DeduplicateCandidates(candidates, active_stream_count, include_new_flow);
  }

LiteScore EvaluateLiteScore(const int32_t unit_id, const int32_t candidate_stream, const int32_t level,
                                 const std::vector<int32_t> &assignment, const std::vector<int32_t> &stream_sizes,
                                 const std::vector<std::vector<int32_t>> &stream_level_loads,
                                 const int32_t active_stream_count) const {
    LiteScore score;
    const auto &unit = unit_profiles_[unit_id];
    const bool opens_new_stream = (candidate_stream == active_stream_count);
    const auto projected_stream_count = active_stream_count + (opens_new_stream ? 1 : 0);

    score.event_local = CalcEventLocal(unit_id, candidate_stream, assignment);

    std::vector<int32_t> window_loads;
    window_loads.reserve(static_cast<size_t>(projected_stream_count));
    for (int32_t physical_stream = 0; physical_stream < active_stream_count; ++physical_stream) {
      window_loads.emplace_back(StreamWindowLoad(stream_level_loads, physical_stream, level));
    }
    if (opens_new_stream) {
      window_loads.emplace_back(0);
    }
    window_loads[candidate_stream] += WindowMass(unit.level_hist, level);

    double window_mean = 0.0;
    int32_t window_peak = 0;
    for (const auto load : window_loads) {
      window_mean += static_cast<double>(load);
      window_peak = std::max(window_peak, load);
    }
    window_mean /= static_cast<double>(projected_stream_count);

    double window_var = 0.0;
    for (const auto load : window_loads) {
      const auto diff = static_cast<double>(load) - window_mean;
      window_var += diff * diff;
    }
    window_var /= static_cast<double>(projected_stream_count);

    score.window_balance = static_cast<double>(window_peak) + window_var;
    score.total_load = (opens_new_stream ? 0 : stream_sizes[candidate_stream]) + unit.size;
    score.candidate_stream = candidate_stream;
    score.total = options_.event_local_weight * static_cast<double>(score.event_local) +
                  options_.window_balance_weight * score.window_balance +
                  options_.total_load_weight * static_cast<double>(score.total_load) +
                  options_.new_flow_penalty_weight * static_cast<double>(opens_new_stream ? 1 : 0);
    return score;
  }

  ConcurrentScore EvaluateConcurrentScore(const int32_t unit_id, const int32_t candidate_stream,
                                            const int32_t level, const std::vector<int32_t> &assignment,
                                            const std::vector<std::vector<int32_t>> &stream_level_loads,
                                            const int32_t active_stream_count) const {
    ConcurrentScore score;
    const bool opens_new_stream = (candidate_stream == active_stream_count);
    score.event_local = CalcEventLocal(unit_id, candidate_stream, assignment);
    if (!opens_new_stream) {
      score.parallel_conflict =
          ParallelConflictValue(unit_profiles_[unit_id].level_hist, stream_level_loads[candidate_stream], level);
    }
    score.new_flow_penalty = opens_new_stream ? 1 : 0;
    score.main_stream_bonus = (candidate_stream == options_.main_stream) ? unit_solo_mass_[unit_id] : 0;
    score.total = options_.event_local_weight * static_cast<double>(score.event_local) +
                  options_.parallel_conflict_weight * static_cast<double>(score.parallel_conflict) +
                  options_.new_flow_penalty_weight * static_cast<double>(score.new_flow_penalty) -
                  options_.main_stream_bonus_weight * static_cast<double>(score.main_stream_bonus);
    return score;
  }

  int32_t ApplyUnitToLoadBalanceStream(const int32_t unit_id, const int32_t candidate_stream,
                                     std::vector<int32_t> &assignment, std::vector<int32_t> &stream_sizes,
                                     std::vector<std::vector<int32_t>> &stream_level_loads,
                                     int32_t active_stream_count) const {
    if (candidate_stream == active_stream_count) {
      stream_sizes.emplace_back(0);
      stream_level_loads.emplace_back(static_cast<size_t>(max_level_ + 1), 0);
      ++active_stream_count;
    }
    assignment[unit_id] = candidate_stream;
    const auto &unit = unit_profiles_[unit_id];
    stream_sizes[candidate_stream] += unit.size;
    for (size_t level = 0UL; level < unit.level_hist.size(); ++level) {
      if (unit.level_hist[level] != 0) {
        stream_level_loads[candidate_stream][level] += unit.level_hist[level];
      }
    }
    return active_stream_count;
  }

  int32_t ApplyUnitToMainStream(const int32_t unit_id, const int32_t candidate_stream,
                                     std::vector<int32_t> &assignment,
                                     std::vector<std::vector<int32_t>> &stream_level_loads,
                                     int32_t active_stream_count) const {
    if (candidate_stream == active_stream_count) {
      stream_level_loads.emplace_back(static_cast<size_t>(max_level_ + 1), 0);
      ++active_stream_count;
    }
    assignment[unit_id] = candidate_stream;
    const auto &unit = unit_profiles_[unit_id];
    for (size_t level = 0UL; level < unit.level_hist.size(); ++level) {
      if (unit.level_hist[level] != 0) {
        stream_level_loads[candidate_stream][level] += unit.level_hist[level];
      }
    }
    return active_stream_count;
  }

  int32_t RemoveUnitFromLoadBalanceStream(const int32_t unit_id, std::vector<int32_t> &assignment,
                                        std::vector<int32_t> &stream_sizes,
                                        std::vector<std::vector<int32_t>> &stream_level_loads) const {
    const auto physical_stream = assignment[unit_id];
    if (physical_stream < 0) {
      MINIDAG_LOG_ERROR("Try to remove unassigned unit %d.", unit_id);
      return -1;
    }

    assignment[unit_id] = -1;
    const auto &unit = unit_profiles_[unit_id];
    stream_sizes[physical_stream] -= unit.size;
    for (size_t level = 0UL; level < unit.level_hist.size(); ++level) {
      if (unit.level_hist[level] != 0) {
        stream_level_loads[physical_stream][level] -= unit.level_hist[level];
      }
    }
    return physical_stream;
  }

  int32_t RemoveUnitFromMainStream(const int32_t unit_id, std::vector<int32_t> &assignment,
                                        std::vector<std::vector<int32_t>> &stream_level_loads) const {
    const auto physical_stream = assignment[unit_id];
    if (physical_stream < 0) {
      MINIDAG_LOG_ERROR("Try to remove unassigned unit %d.", unit_id);
      return -1;
    }

    assignment[unit_id] = -1;
    const auto &unit = unit_profiles_[unit_id];
    for (size_t level = 0UL; level < unit.level_hist.size(); ++level) {
      if (unit.level_hist[level] != 0) {
        stream_level_loads[physical_stream][level] -= unit.level_hist[level];
      }
    }
    return physical_stream;
  }

  void RepairLoadBalanceRecentUnits(const int32_t level, const std::vector<int32_t> &recent_unit_ids,
                                  std::vector<int32_t> &assignment, std::vector<int32_t> &stream_sizes,
                                  std::vector<std::vector<int32_t>> &stream_level_loads,
                                  const int32_t active_stream_count) const {
    int32_t move_count = 0;
    while (move_count < options_.repair_moves) {
      BestRepairMove best_move;
      for (const auto unit_id : recent_unit_ids) {
        const auto current_stream =
            RemoveUnitFromLoadBalanceStream(unit_id, assignment, stream_sizes, stream_level_loads);
        const auto current_score = EvaluateLiteScore(unit_id, current_stream, level, assignment, stream_sizes,
                                                     stream_level_loads, active_stream_count);

        auto candidate_flows = LoadBalanceCandidateFlows(unit_id, level, assignment, stream_sizes, stream_level_loads,
                                                       active_stream_count, false);
        if (std::find(candidate_flows.begin(), candidate_flows.end(), current_stream) == candidate_flows.end()) {
          candidate_flows.emplace_back(current_stream);
        }

        int32_t best_candidate = current_stream;
        auto best_score = EvaluateLiteScore(unit_id, current_stream, level, assignment, stream_sizes,
                                            stream_level_loads, active_stream_count);
        for (const auto candidate_stream : candidate_flows) {
          const auto score = EvaluateLiteScore(unit_id, candidate_stream, level, assignment, stream_sizes,
                                               stream_level_loads, active_stream_count);
          if (score < best_score) {
            best_score = score;
            best_candidate = candidate_stream;
          }
        }
        (void)ApplyUnitToLoadBalanceStream(unit_id, current_stream, assignment, stream_sizes, stream_level_loads,
                                         active_stream_count);

        const auto improvement = current_score.total - best_score.total;
        if ((best_candidate != current_stream) && (improvement > kMergeImproveEps) &&
            (improvement > best_move.improvement)) {
          best_move = {unit_id, current_stream, best_candidate, improvement};
        }
      }

      if (best_move.unit_id < 0) {
        return;
      }
      (void)RemoveUnitFromLoadBalanceStream(best_move.unit_id, assignment, stream_sizes, stream_level_loads);
      (void)ApplyUnitToLoadBalanceStream(best_move.unit_id, best_move.to_stream, assignment, stream_sizes,
                                       stream_level_loads, active_stream_count);
      ++move_count;
    }
  }

  void RepairMainStreamRecentUnits(const int32_t level, const std::vector<int32_t> &recent_unit_ids,
                                  std::vector<int32_t> &assignment,
                                  std::vector<std::vector<int32_t>> &stream_level_loads,
                                  const int32_t active_stream_count) const {
    int32_t move_count = 0;
    while (move_count < options_.repair_moves) {
      BestRepairMove best_move;
      for (const auto unit_id : recent_unit_ids) {
        if (MustAssignToMainStream(unit_id, level)) {
          continue;
        }
        const auto current_stream = RemoveUnitFromMainStream(unit_id, assignment, stream_level_loads);
        const auto current_score =
            EvaluateConcurrentScore(unit_id, current_stream, level, assignment, stream_level_loads, active_stream_count);

        auto candidate_flows =
            MainStreamCandidateFlows(unit_id, level, assignment, stream_level_loads, active_stream_count, false);
        if (std::find(candidate_flows.begin(), candidate_flows.end(), current_stream) == candidate_flows.end()) {
          candidate_flows.emplace_back(current_stream);
        }

        int32_t best_candidate = current_stream;
        auto best_score =
            EvaluateConcurrentScore(unit_id, current_stream, level, assignment, stream_level_loads, active_stream_count);
        for (const auto candidate_stream : candidate_flows) {
          const auto score =
              EvaluateConcurrentScore(unit_id, candidate_stream, level, assignment, stream_level_loads,
                                      active_stream_count);
          if (score < best_score) {
            best_score = score;
            best_candidate = candidate_stream;
          }
        }
        (void)ApplyUnitToMainStream(unit_id, current_stream, assignment, stream_level_loads, active_stream_count);

        const auto improvement = current_score.total - best_score.total;
        if ((best_candidate != current_stream) && (improvement > kMergeImproveEps) &&
            (improvement > best_move.improvement)) {
          best_move = {unit_id, current_stream, best_candidate, improvement};
        }
      }

      if (best_move.unit_id < 0) {
        return;
      }
      (void)RemoveUnitFromMainStream(best_move.unit_id, assignment, stream_level_loads);
      (void)ApplyUnitToMainStream(best_move.unit_id, best_move.to_stream, assignment, stream_level_loads,
                                       active_stream_count);
      ++move_count;
    }
  }

  std::vector<int32_t> CompactAssignment(const std::vector<int32_t> &assignment) const {
    std::set<int32_t> used_streams;
    for (const auto stream : assignment) {
      if (stream < 0) {
        MINIDAG_LOG_ERROR("There is still an unassigned unit when compacting assignment.");
        return {};
      }
      used_streams.insert(stream);
    }
    if (used_streams.empty()) {
      MINIDAG_LOG_ERROR("Merge should produce at least one physical stream.");
      return {};
    }

    std::map<int32_t, int32_t> stream_remap;
    int32_t next_stream = 0;
    for (const auto stream : used_streams) {
      stream_remap.emplace(stream, next_stream++);
    }

    std::vector<int32_t> compact_assignment;
    compact_assignment.reserve(assignment.size());
    for (const auto stream : assignment) {
      compact_assignment.emplace_back(stream_remap.at(stream));
    }
    return compact_assignment;
  }

  const DAGGraph& dag_;
  const std::vector<std::vector<int32_t>> &logical_stream_routes_;
  const StreamMergeOptions &options_;

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
  std::vector<int32_t> global_level_loads_;
  std::vector<int32_t> unit_solo_mass_;
  std::vector<std::map<int32_t, int32_t>> unit_pred_edges_;
  std::vector<std::map<int32_t, int32_t>> unit_succ_edges_;
  std::map<int32_t, std::vector<int32_t>> units_by_earliest_level_;
};
}  // namespace

StreamMerger::StreamMerger(const StreamMergeOptions &options) : options_(options) {}

graphStatus StreamMerger::Merge(const DAGGraph& dag,
                                   const std::vector<std::vector<int32_t>> &logical_stream_routes,
                                   std::vector<int32_t> &logical_to_physical_stream) const {
  logical_to_physical_stream.clear();
  if (options_.physical_stream_limit <= 0) {
    MINIDAG_LOG_ERROR("Merge physical stream limit must be greater than 0.");
    return graphStatus::FAILED;
  }
  if (options_.window_width <= 0) {
    MINIDAG_LOG_ERROR("Merge window width must be greater than 0.");
    return graphStatus::FAILED;
  }
  if (options_.candidate_limit <= 0) {
    MINIDAG_LOG_ERROR("Merge candidate limit must be greater than 0.");
    return graphStatus::FAILED;
  }
  if (options_.repair_moves < 0) {
    MINIDAG_LOG_ERROR("Merge repair moves must be greater than or equal to 0.");
    return graphStatus::FAILED;
  }
  if (options_.strategy == StreamMergeStrategy::kLoadBalance) {
    if (options_.light_stream_limit <= 0) {
      MINIDAG_LOG_ERROR("LoadBalance light stream limit must be greater than 0.");
      return graphStatus::FAILED;
    }
  } else {
    if (options_.low_conflict_limit <= 0) {
      MINIDAG_LOG_ERROR("MainStream low conflict limit must be greater than 0.");
      return graphStatus::FAILED;
    }
    if (options_.main_stream < 0) {
      MINIDAG_LOG_ERROR("MainStream main stream must be greater than or equal to 0.");
      return graphStatus::FAILED;
    }
  }
  if (logical_stream_routes.empty()) {
    return graphStatus::SUCCESS;
  }

  StreamMergeSolver solver(dag, logical_stream_routes, options_);
  return solver.Solve(logical_to_physical_stream);
}
}  // namespace minidag