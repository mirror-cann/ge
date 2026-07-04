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

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "graph/build/dag/dag_graph.h"
#include "graph/build/dag/dag_weighted_stream_merger.h"

namespace minidag {
namespace test {
namespace {
void SetNodeCost(const std::shared_ptr<DAGNode> &node, const float duration, const size_t cube_num,
                 const size_t vec_num) {
  NodeCost cost;
  cost.execution_time = duration;
  cost.cube_block_num = cube_num;
  cost.vec_block_num = vec_num;
  node->SetCost(cost);
}

std::shared_ptr<DAGNode> AddCostNode(DAGGraph &dag, const std::string &name, const int64_t topo_id,
                                     const float duration) {
  auto node = dag.AddNode(name, "Dummy");
  if (node == nullptr) {
    return nullptr;
  }
  node->SetTopoId(topo_id);
  SetNodeCost(node, duration, static_cast<size_t>((topo_id % 3) + 1), static_cast<size_t>((topo_id % 2) + 1));
  return node;
}

void BuildTwoNodeGraph(DAGGraph &dag) {
  auto n0 = AddCostNode(dag, "n0", 0, 5.0F);
  auto n1 = AddCostNode(dag, "n1", 1, 7.0F);
  ASSERT_NE(n0, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_EQ(dag.AddEdge(n0, 0, n1, 0), graphStatus::SUCCESS);
}

void BuildCycleGraph(DAGGraph &dag) {
  auto n0 = AddCostNode(dag, "n0", 0, 5.0F);
  auto n1 = AddCostNode(dag, "n1", 1, 7.0F);
  ASSERT_NE(n0, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_EQ(dag.AddEdge(n0, 0, n1, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n1, 0, n0, 0), graphStatus::SUCCESS);
}

void BuildCrossStreamGraph(DAGGraph &dag) {
  auto n0 = AddCostNode(dag, "n0", 0, 30.0F);
  auto n1 = AddCostNode(dag, "n1", 1, 10.0F);
  auto n2 = AddCostNode(dag, "n2", 2, 20.0F);
  auto n3 = AddCostNode(dag, "n3", 3, 15.0F);
  auto n4 = AddCostNode(dag, "n4", 4, 8.0F);
  auto n5 = AddCostNode(dag, "n5", 5, 12.0F);
  ASSERT_NE(n0, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);
  ASSERT_NE(n3, nullptr);
  ASSERT_NE(n4, nullptr);
  ASSERT_NE(n5, nullptr);
  ASSERT_EQ(dag.AddEdge(n0, 0, n3, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n1, 0, n3, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n2, 0, n4, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n3, 0, n5, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n4, 0, n5, 0), graphStatus::SUCCESS);
}

void BuildInternalEdgeGraph(DAGGraph &dag) {
  auto n0 = AddCostNode(dag, "n0", 0, 9.0F);
  auto n1 = AddCostNode(dag, "n1", 1, 3.0F);
  auto n2 = AddCostNode(dag, "n2", 2, 5.0F);
  auto n3 = AddCostNode(dag, "n3", 3, 0.0F);
  ASSERT_NE(n0, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);
  ASSERT_NE(n3, nullptr);
  SetNodeCost(n3, 0.0F, 0U, 0U);
  ASSERT_EQ(dag.AddEdge(n0, 0, n1, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n1, 0, n2, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n2, 0, n3, 0), graphStatus::SUCCESS);
}

void ExpectValidMapping(const std::vector<int32_t> &mapping, const size_t expected_size, const int32_t stream_limit) {
  ASSERT_EQ(mapping.size(), expected_size);
  for (const auto stream : mapping) {
    EXPECT_GE(stream, 0);
    EXPECT_LT(stream, stream_limit);
  }
}
}  // namespace

TEST(WeightedStreamMergerPublicStTest, Merge_CoversInvalidOptionsOnProductionObject) {
  DAGGraph dag("public_invalid_options");
  BuildTwoNodeGraph(dag);
  const std::vector<std::vector<int32_t>> routes = {{0}, {1}};
  std::vector<int32_t> mapping;

  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 0;
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, routes, mapping), graphStatus::FAILED);
  options.physical_stream_limit = 2;
  options.window_width = 0;
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, routes, mapping), graphStatus::FAILED);
  options.window_width = 1;
  options.candidate_limit = 0;
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, routes, mapping), graphStatus::FAILED);
  options.candidate_limit = 1;
  options.light_stream_limit = 0;
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, routes, mapping), graphStatus::FAILED);
  options.light_stream_limit = 1;
  options.repair_moves = -1;
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, routes, mapping), graphStatus::FAILED);
  options.repair_moves = 0;
  options.resim_candidate_limit = -1;
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, routes, mapping), graphStatus::FAILED);
}

TEST(WeightedStreamMergerPublicStTest, Merge_CoversRouteFailuresOnProductionObject) {
  DAGGraph dag("public_route_failures");
  BuildTwoNodeGraph(dag);
  std::vector<int32_t> mapping;
  WeightedStreamMergeOptions options;

  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, {}, mapping), graphStatus::SUCCESS);
  EXPECT_TRUE(mapping.empty());
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, {{0}}, mapping), graphStatus::FAILED);
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, {{0}, {0}, {1}}, mapping), graphStatus::FAILED);
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, {{0}, {2}}, mapping), graphStatus::FAILED);
  EXPECT_EQ(WeightedStreamMerger(options).Merge(dag, {{}, {1}}, mapping), graphStatus::FAILED);
}

TEST(WeightedStreamMergerPublicStTest, Merge_CoversCycleFailureOnProductionObject) {
  DAGGraph dag("public_cycle");
  BuildCycleGraph(dag);
  std::vector<int32_t> mapping;

  EXPECT_EQ(WeightedStreamMerger().Merge(dag, {{0}, {1}}, mapping), graphStatus::FAILED);
}

TEST(WeightedStreamMergerPublicStTest, Merge_CoversMissingCostAndExtremeCoresOnProductionObject) {
  DAGGraph dag("public_mixed_cost");
  auto n0 = AddCostNode(dag, "n0", 0, 5.0F);
  auto n1 = AddCostNode(dag, "n1", 1, -1.0F);
  auto n2 = AddCostNode(dag, "n2", 2, 7.0F);
  auto n3 = dag.AddNode("n3", "Dummy");
  ASSERT_NE(n0, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_NE(n2, nullptr);
  ASSERT_NE(n3, nullptr);
  n3->SetTopoId(3);
  SetNodeCost(n2, 7.0F, std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max());
  ASSERT_EQ(dag.AddEdge(n0, 0, n2, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n1, 0, n2, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n2, 0, n3, 0), graphStatus::SUCCESS);

  std::vector<int32_t> mapping;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 3;
  options.repair_moves = 1;
  ASSERT_EQ(WeightedStreamMerger(options).Merge(dag, {{1, 0}, {2}, {3}}, mapping), graphStatus::SUCCESS);
  ExpectValidMapping(mapping, 3U, options.physical_stream_limit);
}

TEST(WeightedStreamMergerPublicStTest, Merge_CoversCrossStreamDependencyOnProductionObject) {
  DAGGraph dag("public_cross_stream");
  BuildCrossStreamGraph(dag);
  std::vector<int32_t> mapping;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 3;
  options.candidate_limit = 4;
  options.light_stream_limit = 3;
  options.repair_moves = 3;
  options.resim_candidate_limit = 3;

  ASSERT_EQ(WeightedStreamMerger(options).Merge(dag, {{0}, {1}, {2}, {3}, {4}, {5}}, mapping), graphStatus::SUCCESS);
  ExpectValidMapping(mapping, 6U, options.physical_stream_limit);
}

TEST(WeightedStreamMergerPublicStTest, Merge_CoversInternalEdgeAndFastScorePathOnProductionObject) {
  DAGGraph dag("public_internal_edge");
  BuildInternalEdgeGraph(dag);
  std::vector<int32_t> mapping;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 2;
  options.resim_candidate_limit = 0;

  ASSERT_EQ(WeightedStreamMerger(options).Merge(dag, {{1, 0}, {2}, {3}}, mapping), graphStatus::SUCCESS);
  ExpectValidMapping(mapping, 3U, options.physical_stream_limit);
}

TEST(WeightedStreamMergerPublicStTest, Merge_CoversSinglePhysicalStreamOnProductionObject) {
  DAGGraph dag("public_single_stream");
  for (int32_t idx = 0; idx < 6; ++idx) {
    ASSERT_NE(AddCostNode(dag, "n" + std::to_string(idx), idx, 3.0F), nullptr);
  }
  std::vector<int32_t> mapping;
  WeightedStreamMergeOptions options;
  options.physical_stream_limit = 1;

  ASSERT_EQ(WeightedStreamMerger(options).Merge(dag, {{0}, {1}, {2}, {3}, {4}, {5}}, mapping), graphStatus::SUCCESS);
  ASSERT_EQ(mapping.size(), 6U);
  for (const auto stream : mapping) {
    EXPECT_EQ(stream, 0);
  }
}

TEST(WeightedStreamMergerPublicStTest, Merge_CoversExternalInputNodeOnProductionObject) {
  DAGGraph dag("public_external_pred");
  auto n0 = AddCostNode(dag, "n0", 0, 4.0F);
  auto n1 = AddCostNode(dag, "n1", 1, 6.0F);
  auto external = std::make_shared<DAGNode>("external", "Dummy");
  ASSERT_NE(n0, nullptr);
  ASSERT_NE(n1, nullptr);
  ASSERT_EQ(dag.AddEdge(external, 0, n0, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag.AddEdge(n0, 0, n1, 0), graphStatus::SUCCESS);

  std::vector<int32_t> mapping;
  ASSERT_EQ(WeightedStreamMerger().Merge(dag, {{0}, {1}}, mapping), graphStatus::SUCCESS);
  ExpectValidMapping(mapping, 2U, WeightedStreamMergeOptions().physical_stream_limit);
}

}  // namespace test
}  // namespace minidag
