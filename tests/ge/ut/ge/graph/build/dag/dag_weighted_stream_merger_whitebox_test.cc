/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

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

#include "graph/build/dag/dag_edge.h"
#include "graph/build/dag/dag_graph.h"
#include "graph/build/dag/dag_log.h"
#include "graph/build/dag/dag_node.h"
#include "graph/build/dag/dag_types.h"

#define private public
#define WeightedStreamMerger WeightedStreamMergerWhiteBox
#include "graph/build/dag/dag_weighted_stream_merger.cc"
#undef WeightedStreamMerger
#undef private

namespace minidag {
namespace test {
namespace {
void PrepareTwoUnitSolver(WeightedStreamMergeSolver &solver) {
  solver.max_level_ = 2;
  solver.unit_count_ = 2;
  solver.unit_total_duration_ = {10.0, 1.0};
  solver.unit_total_aiv_time_ = {20.0, 1.0};
  solver.unit_total_aic_time_ = {10.0, 1.0};
  solver.unit_duration_hist_ = {{5.0, 0.0, 5.0}, {0.0, 1.0, 0.0}};
  solver.unit_peak_aiv_hist_ = {{2, 0, 1}, {0, 1, 0}};
  solver.unit_peak_aic_hist_ = {{1, 0, 3}, {0, 1, 0}};
  solver.unit_peak_aiv_ = {2, 1};
  solver.unit_peak_aic_ = {3, 1};
  solver.unit_origin_stream_hint_ = {-1, -1};
  solver.unit_active_levels_ = {{0, 2}, {1}};
  solver.unit_pred_edges_ = {{}, {{0, 2}}};
  solver.unit_succ_edges_ = {{{1, 3}}, {}};
}
}  // namespace

TEST(WeightedStreamMergerWhiteBoxTest, RunningNodeGreater_TieBreaksByFinishTopoAndNode) {
  WeightedStreamMergeSolver::RunningNodeGreater greater;

  EXPECT_TRUE(greater({2.0, 0, 0}, {1.0, 0, 0}));
  EXPECT_FALSE(greater({1.0, 0, 0}, {2.0, 0, 0}));
  EXPECT_TRUE(greater({1.0, 2, 0}, {1.0, 1, 0}));
  EXPECT_TRUE(greater({1.0, 1, 2}, {1.0, 1, 1}));
}

TEST(WeightedStreamMergerWhiteBoxTest, RunningNodeHeap_OrdersByFinishTopoAndNode) {
  WeightedStreamMergeSolver::RunningNodeHeap heap;

  heap.push({3.0, 0, 0});
  heap.push({1.0, 2, 0});
  heap.push({1.0, 1, 3});
  heap.push({1.0, 1, 2});

  EXPECT_EQ(heap.top().node_index, 2);
  heap.pop();
  EXPECT_EQ(heap.top().node_index, 3);
  heap.pop();
  EXPECT_EQ(heap.top().node_index, 0);
}

TEST(WeightedStreamMergerWhiteBoxTest, LiteScoreLess_TieBreaksByConflictEventLoadAndStream) {
  LiteScore lhs;
  LiteScore rhs;
  lhs.total = 1.0;
  rhs.total = 1.0;
  lhs.time_conflict = 1.0;
  rhs.time_conflict = 2.0;
  EXPECT_TRUE(WeightedStreamMergeSolver::LiteScoreLess(lhs, rhs));

  rhs.time_conflict = lhs.time_conflict;
  lhs.event_local = 1;
  rhs.event_local = 2;
  EXPECT_TRUE(WeightedStreamMergeSolver::LiteScoreLess(lhs, rhs));

  rhs.event_local = lhs.event_local;
  lhs.time_load = 3.0;
  rhs.time_load = 4.0;
  EXPECT_TRUE(WeightedStreamMergeSolver::LiteScoreLess(lhs, rhs));

  rhs.time_load = lhs.time_load;
  lhs.candidate_stream = 0;
  rhs.candidate_stream = 1;
  EXPECT_TRUE(WeightedStreamMergeSolver::LiteScoreLess(lhs, rhs));
}

TEST(WeightedStreamMergerWhiteBoxTest, OriginHintFlows_SortsByCountLoadAndStreamId) {
  DAGGraph dag("whitebox_origin_hint");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.unit_origin_stream_hint_ = {7};

  StreamingState state;
  state.stream_total_durations = {5.0, 4.0, 2.0, 2.0};
  state.origin_stream_to_flow_counts[7][0] = 3;
  state.origin_stream_to_flow_counts[7][1] = 1;
  state.origin_stream_to_flow_counts[7][2] = 1;
  state.origin_stream_to_flow_counts[7][3] = 1;

  const auto flows = solver.OriginHintFlows(0, state);

  ASSERT_EQ(flows.size(), 4U);
  EXPECT_EQ(flows[0], 0);
  EXPECT_EQ(flows[1], 2);
  EXPECT_EQ(flows[2], 3);
  EXPECT_EQ(flows[3], 1);
}

TEST(WeightedStreamMergerWhiteBoxTest, CandidateFlows_AddsOriginHintsBeforeLightFlows) {
  DAGGraph dag("whitebox_origin_hint_candidate");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 4;
  options.candidate_limit = 4;
  options.light_stream_limit = 2;
  WeightedStreamMergeSolver solver(dag, routes, options);
  PrepareTwoUnitSolver(solver);
  solver.unit_origin_stream_hint_ = {9, -1};
  solver.unit_pred_edges_ = {{}, {}};
  solver.unit_succ_edges_ = {{}, {}};

  StreamingState state;
  state.assignment = {-1, -1};
  state.stream_total_durations = {8.0, 4.0, 2.0, 0.0};
  state.stream_duration_hists = {{3.0, 0.0, 0.0}, {2.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
  state.origin_stream_to_flow_counts[9][0] = 1;
  state.origin_stream_to_flow_counts[9][1] = 2;
  state.origin_stream_to_flow_counts[9][2] = 1;
  state.active_stream_count = 3;

  EXPECT_EQ(solver.CandidateFlows(0, 0, state, true), std::vector<int32_t>({1, 2, 0, 3}));
}

TEST(WeightedStreamMergerWhiteBoxTest, OriginHintFlows_ReturnsEmptyWithoutHintOrCounter) {
  DAGGraph dag("whitebox_origin_hint_empty");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);
  StreamingState state;

  solver.unit_origin_stream_hint_ = {-1};
  EXPECT_TRUE(solver.OriginHintFlows(0, state).empty());

  solver.unit_origin_stream_hint_ = {3};
  EXPECT_TRUE(solver.OriginHintFlows(0, state).empty());
}

TEST(WeightedStreamMergerWhiteBoxTest, AdjacentAndLightCandidateFlows_SortAndLimitCandidates) {
  DAGGraph dag("whitebox_candidates");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 4;
  options.candidate_limit = 3;
  options.light_stream_limit = 2;
  WeightedStreamMergeSolver solver(dag, routes, options);
  PrepareTwoUnitSolver(solver);

  StreamingState state;
  state.assignment = {0, 2};
  state.stream_total_durations = {8.0, 1.0, 3.0, 0.0};
  state.stream_duration_hists = {{4.0, 4.0, 0.0}, {1.0, 0.0, 0.0}, {2.0, 0.0, 1.0}, {0.0, 0.0, 0.0}};
  state.active_stream_count = 3;

  const std::map<int32_t, int32_t> expected_unit0_flows = {{2, 3}};
  const std::map<int32_t, int32_t> expected_unit1_flows = {{0, 2}};
  EXPECT_EQ(solver.AdjacentAssignedFlows(0, state.assignment), expected_unit0_flows);
  EXPECT_EQ(solver.AdjacentAssignedFlows(1, state.assignment), expected_unit1_flows);
  EXPECT_EQ(solver.AdjacentCandidateFlows(0, state), std::vector<int32_t>({2}));
  EXPECT_EQ(solver.LightCandidateFlows(0, state), std::vector<int32_t>({1, 2}));
  EXPECT_EQ(solver.UniqueLimitedCandidates({2, 1, 2, 0}), std::vector<int32_t>({2, 1, 0}));
  EXPECT_EQ(solver.CandidateFlows(0, 0, state, true), std::vector<int32_t>({2, 1, 3}));
}

TEST(WeightedStreamMergerWhiteBoxTest, WindowHelpers_ReturnExpectedLevelsAndLoad) {
  DAGGraph dag("whitebox_window");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.window_width = 2;
  options.physical_stream_limit = 2;
  WeightedStreamMergeSolver solver(dag, routes, options);
  PrepareTwoUnitSolver(solver);

  StreamingState state;
  state.stream_duration_hists = {{1.0, 2.0, 4.0}, {0.0, 3.0, 5.0}};

  EXPECT_EQ(solver.WindowEnd(0), 1);
  EXPECT_EQ(solver.WindowEnd(2), 2);
  EXPECT_DOUBLE_EQ(solver.WindowMass(state.stream_duration_hists[0], 0), 3.0);
  EXPECT_DOUBLE_EQ(solver.StreamWindowDurationLoad(state, 1, 1), 8.0);
  EXPECT_EQ(solver.WindowActiveLevels(0, 1), std::vector<int32_t>({2}));
  EXPECT_EQ(solver.WindowActiveLevels(1, 2), std::vector<int32_t>({1}));
}

TEST(WeightedStreamMergerWhiteBoxTest, FallbackCandidateFlows_ReturnsNewOrExistingStreams) {
  DAGGraph dag("whitebox_fallback");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 3;
  options.candidate_limit = 2;
  WeightedStreamMergeSolver solver(dag, routes, options);

  StreamingState state;
  state.active_stream_count = 1;
  EXPECT_EQ(solver.FallbackCandidateFlows(state, true), std::vector<int32_t>({1}));

  state.active_stream_count = 3;
  EXPECT_EQ(solver.FallbackCandidateFlows(state, false), std::vector<int32_t>({0, 1}));
}

TEST(WeightedStreamMergerWhiteBoxTest, CandidateFlows_FallsBackWhenNoRegularCandidateExists) {
  DAGGraph dag("whitebox_candidate_fallback");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 2;
  options.candidate_limit = 2;
  options.light_stream_limit = 1;
  WeightedStreamMergeSolver solver(dag, routes, options);
  PrepareTwoUnitSolver(solver);
  solver.unit_origin_stream_hint_ = {-1, -1};
  solver.unit_pred_edges_ = {{}, {}};
  solver.unit_succ_edges_ = {{}, {}};

  StreamingState state;
  state.assignment = {-1, -1};
  state.stream_total_durations = {0.0, 0.0};
  state.stream_duration_hists = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
  state.active_stream_count = 0;

  EXPECT_EQ(solver.CandidateFlows(0, 0, state, true), std::vector<int32_t>({0}));

  state.active_stream_count = 2;
  EXPECT_EQ(solver.CandidateFlows(0, 0, state, false), std::vector<int32_t>({0}));
}

TEST(WeightedStreamMergerWhiteBoxTest, ScoreApplyAndRemove_UpdateStateAndCounters) {
  DAGGraph dag("whitebox_score");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 3;
  options.window_width = 3;
  WeightedStreamMergeSolver solver(dag, routes, options);
  PrepareTwoUnitSolver(solver);
  solver.unit_origin_stream_hint_ = {7, 9};

  StreamingState state;
  state.assignment = {-1, 1};
  state.stream_total_durations = {3.0, 2.0, 0.0};
  state.stream_duration_hists = {{1.0, 1.0, 1.0}, {0.0, 2.0, 0.0}, {0.0, 0.0, 0.0}};
  state.active_stream_count = 2;

  const auto score = solver.EvaluateLiteScore(0, 2, 0, state);
  EXPECT_EQ(score.event_local, 3);
  EXPECT_DOUBLE_EQ(score.time_conflict, 0.0);
  EXPECT_DOUBLE_EQ(score.time_load, 10.0);
  EXPECT_EQ(score.candidate_stream, 2);
  EXPECT_GT(score.total, score.time_load);

  EXPECT_TRUE(solver.ApplyUnitToStream(0, 2, state));
  EXPECT_EQ(state.active_stream_count, 3);
  EXPECT_EQ(state.assignment[0], 2);
  EXPECT_DOUBLE_EQ(state.stream_total_durations[2], 10.0);
  EXPECT_EQ(state.origin_stream_to_flow_counts[7][2], 1);

  EXPECT_EQ(solver.RemoveUnitFromStream(0, state), 2);
  EXPECT_EQ(state.assignment[0], -1);
  EXPECT_DOUBLE_EQ(state.stream_total_durations[2], 0.0);
  EXPECT_EQ(state.origin_stream_to_flow_counts.count(7), 0U);
}

TEST(WeightedStreamMergerWhiteBoxTest, RepairRecentUnits_MovesUnitWhenScoreImproves) {
  DAGGraph dag("whitebox_repair");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 2;
  options.candidate_limit = 1;
  options.light_stream_limit = 2;
  options.repair_moves = 1;
  options.resim_candidate_limit = 0;
  WeightedStreamMergeSolver solver(dag, routes, options);

  solver.max_level_ = 0;
  solver.unit_active_levels_ = {{0}, {0}};
  solver.unit_duration_hist_ = {{10.0}, {1.0}};
  solver.unit_total_duration_ = {10.0, 1.0};
  solver.unit_origin_stream_hint_ = {-1, -1};
  solver.unit_pred_edges_ = {{}, {}};
  solver.unit_succ_edges_ = {{}, {}};

  StreamingState state;
  state.assignment = {0, 0};
  state.stream_total_durations = {11.0, 0.0};
  state.stream_duration_hists = {{11.0}, {0.0}};
  state.active_stream_count = 2;

  solver.RepairRecentUnits(0, {0}, state);

  EXPECT_EQ(state.assignment[0], 1);
  EXPECT_EQ(state.assignment[1], 0);
}

TEST(WeightedStreamMergerWhiteBoxTest, RepairRecentUnits_ReturnsWhenRecentUnitIsUnassigned) {
  DAGGraph dag("whitebox_repair_unassigned");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 1;
  options.candidate_limit = 1;
  options.light_stream_limit = 1;
  options.repair_moves = 1;
  WeightedStreamMergeSolver solver(dag, routes, options);

  solver.max_level_ = 0;
  solver.unit_active_levels_ = {{0}};
  solver.unit_duration_hist_ = {{1.0}};
  solver.unit_total_duration_ = {1.0};
  solver.unit_origin_stream_hint_ = {-1};
  solver.unit_pred_edges_ = {{}};
  solver.unit_succ_edges_ = {{}};

  StreamingState state;
  state.assignment = {-1};
  state.stream_total_durations = {0.0};
  state.stream_duration_hists = {{0.0}};
  state.active_stream_count = 1;

  solver.RepairRecentUnits(0, {0}, state);

  EXPECT_EQ(state.assignment[0], -1);
}

TEST(WeightedStreamMergerWhiteBoxTest, RepairRecentUnits_ReturnsWhenNoMoveImproves) {
  DAGGraph dag("whitebox_repair_no_move");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 2;
  options.candidate_limit = 2;
  options.light_stream_limit = 2;
  options.repair_moves = 1;
  options.resim_candidate_limit = 0;
  WeightedStreamMergeSolver solver(dag, routes, options);

  solver.max_level_ = 0;
  solver.unit_active_levels_ = {{0}, {0}};
  solver.unit_duration_hist_ = {{10.0}, {1.0}};
  solver.unit_total_duration_ = {10.0, 1.0};
  solver.unit_origin_stream_hint_ = {-1, -1};
  solver.unit_pred_edges_ = {{}, {}};
  solver.unit_succ_edges_ = {{}, {}};

  StreamingState state;
  state.assignment = {0, 1};
  state.stream_total_durations = {10.0, 1.0};
  state.stream_duration_hists = {{10.0}, {1.0}};
  state.active_stream_count = 2;

  solver.RepairRecentUnits(0, {0}, state);

  EXPECT_EQ(state.assignment[0], 0);
  EXPECT_EQ(state.assignment[1], 1);
}

TEST(WeightedStreamMergerWhiteBoxTest, SimulationHelpers_HandleSuccessFailureAndTieBreaks) {
  DAGGraph dag("whitebox_simulation");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 2;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.topo_nodes_ = {std::make_shared<DAGNode>("n0", "Dummy"), std::make_shared<DAGNode>("n1", "Dummy"),
                        std::make_shared<DAGNode>("n2", "Dummy")};
  solver.topo_order_ = {0, 1, 2};
  solver.topo_position_ = {0, 1, 2};
  solver.node_to_unit_ = {0, 1, 2};
  solver.node_durations_ = {5.0, 3.0, 3.0};
  solver.node_bottom_ranks_ = {8.0, 3.0, 3.0};
  solver.preds_ = {{}, {0}, {}};

  std::vector<int32_t> node_streams(3, -1);
  std::vector<char> active_mask(3, 0);
  EXPECT_EQ(solver.BuildSimulationNodeStreams({0, -1, 1}, false, node_streams, active_mask), 2);
  EXPECT_EQ(node_streams, std::vector<int32_t>({0, -1, 1}));
  EXPECT_EQ(active_mask, std::vector<char>({1, 0, 1}));

  node_streams.assign(3, -1);
  active_mask.assign(3, 0);
  EXPECT_EQ(solver.BuildSimulationNodeStreams({0, -1, 1}, true, node_streams, active_mask), -1);

  auto queues = solver.BuildSimulationQueues({0, 1, 0}, {1, 1, 0});
  ASSERT_EQ(queues.size(), 2U);
  EXPECT_EQ(queues[0], std::deque<int32_t>({0}));
  EXPECT_EQ(queues[1], std::deque<int32_t>({1}));

  auto filtered_preds = solver.BuildSimulationFilteredPreds({1, 1, 0});
  EXPECT_EQ(filtered_preds[1], std::vector<int32_t>({0}));
  EXPECT_TRUE(filtered_preds[2].empty());

  EXPECT_FALSE(solver.AreSimulationPredsFinished(1, filtered_preds, {0, 0, 0}));
  EXPECT_TRUE(solver.AreSimulationPredsFinished(1, filtered_preds, {1, 0, 0}));

  std::vector<int32_t> ready_heads = {2, 1};
  solver.SortSimulationReadyHeads(ready_heads);
  EXPECT_EQ(ready_heads, std::vector<int32_t>({1, 2}));

  auto result = solver.BuildAndRunSimulation({0, 1, 0}, true);
  EXPECT_TRUE(result.feasible);
  EXPECT_DOUBLE_EQ(result.makespan, 11.0);

  result = solver.BuildAndRunSimulation({0, -1, 0}, true);
  EXPECT_FALSE(result.feasible);
}

TEST(WeightedStreamMergerWhiteBoxTest, ResourceProfilesAndUnitOrder_UpdateDerivedFields) {
  DAGGraph dag("whitebox_resource_profiles");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 3;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.max_level_ = 2;
  solver.unit_count_ = 3;
  solver.levels_ = {0, 1, 1, 2};
  solver.node_durations_ = {2.0, 5.0, 3.0, 7.0};
  solver.node_aiv_cores_ = {1, 2, 3, 1};
  solver.node_aic_cores_ = {2, 1, 1, 4};
  solver.node_origin_stream_hints_ = {4, 4, 2, 2};
  solver.node_bottom_ranks_ = {10.0, 8.0, 6.0, 7.0};
  solver.unit_profiles_ = {
      {0, {0, 1}, 2, 0, 1, 1, {1, 1, 0}, 0}, {1, {2}, 1, 1, 1, 1, {0, 1, 0}, 0}, {2, {3}, 1, 1, 1, 1, {0, 1, 0}, 0}};

  solver.InitResourceUnitProfiles();
  solver.BuildResourceUnitProfile(solver.unit_profiles_[0]);

  EXPECT_DOUBLE_EQ(solver.unit_total_duration_[0], 7.0);
  EXPECT_DOUBLE_EQ(solver.unit_total_aiv_time_[0], 12.0);
  EXPECT_DOUBLE_EQ(solver.unit_total_aic_time_[0], 9.0);
  EXPECT_EQ(solver.unit_peak_aiv_[0], 2);
  EXPECT_EQ(solver.unit_peak_aic_[0], 2);
  EXPECT_EQ(solver.unit_origin_stream_hint_[0], 4);
  EXPECT_EQ(solver.unit_active_levels_[0], std::vector<int32_t>({0, 1}));

  EXPECT_EQ(solver.SelectOriginStreamHint({{5, 1}, {3, 1}}), 3);

  solver.unit_total_duration_ = {10.0, 10.0, 10.0};
  solver.unit_total_aiv_time_ = {1.0, 2.0, 2.0};
  solver.unit_total_aic_time_ = {1.0, 1.0, 1.0};
  solver.unit_peak_aiv_ = {1, 1, 1};
  solver.unit_peak_aic_ = {1, 1, 1};
  solver.unit_profiles_[0].earliest_level = 1;
  solver.unit_profiles_[0].interaction = 0;
  solver.unit_profiles_[1].interaction = 2;
  solver.unit_profiles_[2].interaction = 1;
  solver.unit_profiles_[2].earliest_level = 0;
  solver.BuildUnitsByEarliestLevel();

  ASSERT_EQ(solver.units_by_earliest_level_[1].size(), 2U);
  EXPECT_EQ(solver.units_by_earliest_level_[1][0], 1);
  EXPECT_EQ(solver.units_by_earliest_level_[1][1], 0);
  EXPECT_EQ(solver.units_by_earliest_level_[0], std::vector<int32_t>({2}));
}

TEST(WeightedStreamMergerWhiteBoxTest, Solve_EmptyDagClearsMapping) {
  DAGGraph dag("whitebox_empty_solve");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);
  std::vector<int32_t> mapping = {1, 2};

  EXPECT_EQ(solver.Solve(mapping), graphStatus::SUCCESS);
  EXPECT_TRUE(mapping.empty());
}

TEST(WeightedStreamMergerWhiteBoxTest, UnitProfile_SortsRouteNodesByTopoPosition) {
  DAGGraph dag("whitebox_route_sort");
  std::vector<std::vector<int32_t>> routes = {{1, 0}};
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.topo_nodes_ = {std::make_shared<DAGNode>("n0", "Dummy"), std::make_shared<DAGNode>("n1", "Dummy")};
  solver.topo_position_ = {0, 1};
  solver.levels_ = {0, 1};
  solver.max_level_ = 1;
  solver.node_to_unit_.assign(2U, -1);

  ASSERT_EQ(solver.BuildUnitProfile(0), graphStatus::SUCCESS);
  EXPECT_EQ(solver.unit_profiles_[0].node_indices, std::vector<int32_t>({0, 1}));
  EXPECT_EQ(solver.unit_profiles_[0].earliest_level, 0);
  EXPECT_EQ(solver.unit_profiles_[0].latest_level, 1);
}

TEST(WeightedStreamMergerWhiteBoxTest, SizeToInt32_ClampsLargeValue) {
  const auto max_int = std::numeric_limits<int32_t>::max();

  EXPECT_EQ(WeightedStreamMergeSolver::SizeToInt32(16U), 16);
  EXPECT_EQ(WeightedStreamMergeSolver::SizeToInt32(std::numeric_limits<size_t>::max()), max_int);
}

TEST(WeightedStreamMergerWhiteBoxTest, SimulationStep_ReturnsFalseForBlockedAndInconsistentState) {
  DAGGraph dag("whitebox_simulation_step");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 1;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.topo_nodes_ = {std::make_shared<DAGNode>("n0", "Dummy")};
  solver.topo_order_ = {0};
  solver.topo_position_ = {0};
  solver.node_durations_ = {1.0};
  solver.node_bottom_ranks_ = {1.0};

  WeightedStreamMergeSolver::SimulationContext context;
  context.node_streams = {0};
  context.stream_to_queue[0] = {0};
  context.filtered_preds = {{}};
  context.finished = {0};
  context.running_in_stream = {1};
  EXPECT_FALSE(solver.RunSimulationStep(context));

  EXPECT_TRUE(solver.StartSimulationReadyHead({}, context));
  EXPECT_TRUE(context.running_heap.empty());

  context.running_in_stream = {0};
  context.stream_to_queue[0].clear();
  EXPECT_FALSE(solver.StartSimulationReadyHead({0}, context));
}

TEST(WeightedStreamMergerWhiteBoxTest, FinishNextSimulationNodes_FinishesSameTimeNodes) {
  DAGGraph dag("whitebox_finish_same_time");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 2;
  WeightedStreamMergeSolver solver(dag, routes, options);

  WeightedStreamMergeSolver::SimulationContext context;
  context.node_streams = {0, 1};
  context.finished = {0, 0};
  context.running_in_stream = {1, 1};
  context.running_heap.push({2.0, 0, 0});
  context.running_heap.push({2.0, 1, 1});

  solver.FinishNextSimulationNodes(context);

  EXPECT_EQ(context.executed_count, 2);
  EXPECT_EQ(context.current_time, 2.0);
  EXPECT_EQ(context.finished, std::vector<char>({1, 1}));
  EXPECT_EQ(context.running_in_stream, std::vector<char>({0, 0}));
}

TEST(WeightedStreamMergerWhiteBoxTest, SelectBestStream_UsesSimulationResultTieBreaks) {
  DAGGraph dag("whitebox_select_best");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 2;
  options.resim_candidate_limit = 2;
  options.new_flow_penalty_weight = 0.0;
  WeightedStreamMergeSolver solver(dag, routes, options);
  PrepareTwoUnitSolver(solver);
  solver.topo_nodes_ = {std::make_shared<DAGNode>("n0", "Dummy"), std::make_shared<DAGNode>("n1", "Dummy")};
  solver.topo_order_ = {0, 1};
  solver.topo_position_ = {0, 1};
  solver.node_to_unit_ = {0, 1};
  solver.node_durations_ = {10.0, 1.0};
  solver.node_bottom_ranks_ = {11.0, 1.0};
  solver.preds_ = {{}, {0}};

  StreamingState state;
  state.assignment = {-1, 0};
  state.stream_total_durations = {1.0, 0.0};
  state.stream_duration_hists = {{0.0, 1.0, 0.0}, {0.0, 0.0, 0.0}};
  state.active_stream_count = 1;

  EXPECT_EQ(solver.SelectBestStream(0, 0, {0, 1}, state), 0);
}

TEST(WeightedStreamMergerWhiteBoxTest, SelectBestStream_FallsBackWhenAllResimulationsAreInfeasible) {
  DAGGraph dag("whitebox_select_best_infeasible");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 2;
  options.resim_candidate_limit = 2;
  options.new_flow_penalty_weight = 0.0;
  WeightedStreamMergeSolver solver(dag, routes, options);
  PrepareTwoUnitSolver(solver);
  solver.topo_nodes_ = {std::make_shared<DAGNode>("n0", "Dummy"), std::make_shared<DAGNode>("n1", "Dummy")};
  solver.topo_order_ = {0, 1};
  solver.topo_position_ = {0, 1};
  solver.node_to_unit_ = {0, 1};
  solver.node_durations_ = {10.0, 1.0};
  solver.node_bottom_ranks_ = {11.0, 1.0};
  solver.preds_ = {{1}, {0}};

  StreamingState state;
  state.assignment = {-1, 0};
  state.stream_total_durations = {1.0, 0.0};
  state.stream_duration_hists = {{0.0, 1.0, 0.0}, {0.0, 0.0, 0.0}};
  state.active_stream_count = 1;

  EXPECT_EQ(solver.SelectBestStream(0, 0, {0, 1}, state), 0);
}

TEST(WeightedStreamMergerWhiteBoxTest, BuildUnitDependencyGraph_IgnoresSameUnitEdges) {
  DAGGraph dag("whitebox_unit_dependency");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.unit_count_ = 2;
  solver.node_to_unit_ = {0, 0, 1};
  solver.edge_pairs_ = {{0, 1}, {1, 2}, {2, 0}};
  solver.unit_profiles_ = {{0, {0, 1}, 2, 0, 1, 1, {1, 1}, 0}, {1, {2}, 1, 1, 1, 1, {0, 1}, 0}};

  solver.BuildUnitDependencyGraph();

  EXPECT_EQ(solver.unit_succ_edges_[0].count(0), 0U);
  EXPECT_EQ(solver.unit_succ_edges_[0][1], 1);
  EXPECT_EQ(solver.unit_pred_edges_[1][0], 1);
  EXPECT_EQ(solver.unit_succ_edges_[1][0], 1);
  EXPECT_EQ(solver.unit_pred_edges_[0][1], 1);
  EXPECT_EQ(solver.unit_profiles_[0].interaction, 2);
  EXPECT_EQ(solver.unit_profiles_[1].interaction, 2);
}

TEST(WeightedStreamMergerWhiteBoxTest, SimulationReadyHeads_FiltersAndSortsQueues) {
  DAGGraph dag("whitebox_ready_heads");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.topo_position_ = {0, 1, 2, 3, 4};
  solver.node_bottom_ranks_ = {0.0, 1.0, 1.0, 4.0, 4.0};
  solver.node_durations_ = {0.0, 1.0, 1.0, 2.0, 3.0};

  const std::map<int32_t, std::deque<int32_t>> queues = {{0, {}}, {1, {1}}, {2, {2}}, {3, {3}}, {4, {4}}};
  const std::vector<std::vector<int32_t>> filtered_preds = {{}, {0}, {}, {}, {}};
  const std::vector<char> finished = {0, 0, 0, 0, 0};
  const std::vector<char> running = {0, 0, 1, 0, 0};

  EXPECT_EQ(solver.SimulationReadyHeads(queues, filtered_preds, finished, running), std::vector<int32_t>({4, 3}));
}

TEST(WeightedStreamMergerWhiteBoxTest, BuildAndRunSimulation_ReturnsZeroWhenNoNodeActive) {
  DAGGraph dag("whitebox_empty_simulation");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.topo_nodes_ = {std::make_shared<DAGNode>("n0", "Dummy"), std::make_shared<DAGNode>("n1", "Dummy")};
  solver.node_to_unit_ = {0, 1};

  const auto result = solver.BuildAndRunSimulation({-1, -1}, false);

  EXPECT_TRUE(result.feasible);
  EXPECT_DOUBLE_EQ(result.makespan, 0.0);
}

TEST(WeightedStreamMergerWhiteBoxTest, RemoveUnitFromStream_HandlesMissingOriginCounter) {
  DAGGraph dag("whitebox_remove_missing_origin");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);
  solver.unit_total_duration_ = {2.0};
  solver.unit_duration_hist_ = {{2.0}};
  solver.unit_active_levels_ = {{0}};
  solver.unit_origin_stream_hint_ = {8};

  StreamingState state;
  state.assignment = {0};
  state.stream_total_durations = {2.0};
  state.stream_duration_hists = {{2.0}};
  state.origin_stream_to_flow_counts[8][1] = 1;

  EXPECT_EQ(solver.RemoveUnitFromStream(0, state), 0);
  EXPECT_EQ(state.origin_stream_to_flow_counts[8][1], 1);
  EXPECT_EQ(state.assignment[0], -1);
}

TEST(WeightedStreamMergerWhiteBoxTest, CompactAssignment_CompactsAndRejectsInvalidAssignments) {
  DAGGraph dag("whitebox_compact");
  std::vector<std::vector<int32_t>> routes;
  WeightedStreamMergeOptions options;
  WeightedStreamMergeSolver solver(dag, routes, options);

  EXPECT_EQ(solver.CompactAssignment({2, 5, 2}), std::vector<int32_t>({0, 1, 0}));
  EXPECT_TRUE(solver.CompactAssignment({}).empty());
  EXPECT_TRUE(solver.CompactAssignment({0, -1}).empty());
}

}  // namespace test
}  // namespace minidag
