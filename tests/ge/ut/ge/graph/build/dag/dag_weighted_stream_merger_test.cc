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

class WeightedStreamMergerTest : public testing::Test {
 protected:
  void SetUp() override {
    dag_ = std::make_shared<DAGGraph>("weighted_stream_merger_test");
    options_.physical_stream_limit = 2;
    options_.candidate_limit = 4;
    options_.light_stream_limit = 2;
    options_.resim_candidate_limit = 2;
  }

  void SetCost(const std::shared_ptr<DAGNode> &node, const float duration, const size_t cube_num,
               const size_t vec_num) {
    NodeCost cost;
    cost.execution_time = duration;
    cost.cube_block_num = cube_num;
    cost.vec_block_num = vec_num;
    node->SetCost(cost);
  }

  std::shared_ptr<DAGNode> AddNode(const std::string &name, const float duration, const size_t cube_num,
                                   const size_t vec_num) {
    auto node = dag_->AddNode(name, "Dummy");
    node->SetTopoId(static_cast<int64_t>(dag_->GetNodeCount()) - 1);
    SetCost(node, duration, cube_num, vec_num);
    return node;
  }

  void BuildDiamondGraph() {
    auto n0 = AddNode("n0", 10.0F, 1U, 2U);
    auto n1 = AddNode("n1", 60.0F, 4U, 1U);
    auto n2 = AddNode("n2", 20.0F, 1U, 4U);
    auto n3 = AddNode("n3", 10.0F, 2U, 2U);
    ASSERT_EQ(dag_->AddEdge(n0, 0, n1, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n0, 0, n2, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n1, 0, n3, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n2, 0, n3, 0), graphStatus::SUCCESS);
  }

  void BuildIndependentGraph(const int32_t count, const float duration) {
    for (int32_t idx = 0; idx < count; ++idx) {
      auto node = AddNode("n" + std::to_string(idx), duration, static_cast<size_t>((idx % 3) + 1),
                          static_cast<size_t>((idx % 2) + 1));
      node->SetStreamId(idx % 2);
    }
  }

  void BuildTwoStageParallelGraph() {
    auto n0 = AddNode("n0", 5.0F, 1U, 1U);
    auto n1 = AddNode("n1", 5.0F, 1U, 1U);
    auto n2 = AddNode("n2", 5.0F, 1U, 1U);
    auto n3 = AddNode("n3", 5.0F, 1U, 1U);
    auto n4 = AddNode("n4", 7.0F, 2U, 1U);
    auto n5 = AddNode("n5", 7.0F, 1U, 2U);
    n0->SetStreamId(0);
    n1->SetStreamId(1);
    n2->SetStreamId(0);
    n3->SetStreamId(1);
    n4->SetStreamId(0);
    n5->SetStreamId(1);
    ASSERT_EQ(dag_->AddEdge(n0, 0, n4, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n1, 0, n4, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n2, 0, n5, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n3, 0, n5, 0), graphStatus::SUCCESS);
  }

  void BuildCrossStreamDependencyGraph() {
    auto n0 = AddNode("n0", 30.0F, 4U, 1U);
    auto n1 = AddNode("n1", 10.0F, 1U, 4U);
    auto n2 = AddNode("n2", 20.0F, 2U, 2U);
    auto n3 = AddNode("n3", 15.0F, 3U, 1U);
    auto n4 = AddNode("n4", 8.0F, 1U, 3U);
    auto n5 = AddNode("n5", 12.0F, 2U, 2U);
    ASSERT_EQ(dag_->AddEdge(n0, 0, n3, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n1, 0, n3, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n2, 0, n4, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n3, 0, n5, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n4, 0, n5, 0), graphStatus::SUCCESS);
  }

  void BuildCycleGraph() {
    auto n0 = AddNode("n0", 10.0F, 1U, 1U);
    auto n1 = AddNode("n1", 10.0F, 1U, 1U);
    ASSERT_EQ(dag_->AddEdge(n0, 0, n1, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n1, 0, n0, 0), graphStatus::SUCCESS);
  }

  void BuildMissingAndExtremeCostGraph() {
    auto n0 = AddNode("n0", 5.0F, 1U, 1U);
    auto n1 = AddNode("n1", -1.0F, 2U, 2U);
    auto n2 = AddNode("n2", 7.0F, std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max());
    auto n3 = dag_->AddNode("n3", "Dummy");
    ASSERT_NE(n3, nullptr);
    n3->SetTopoId(3);
    ASSERT_EQ(dag_->AddEdge(n0, 0, n2, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n1, 0, n2, 0), graphStatus::SUCCESS);
    ASSERT_EQ(dag_->AddEdge(n2, 0, n3, 0), graphStatus::SUCCESS);
  }

  void ExpectValidMapping(const std::vector<int32_t> &mapping, const size_t expected_size) const {
    ASSERT_EQ(mapping.size(), expected_size);
    for (const auto stream_id : mapping) {
      EXPECT_GE(stream_id, 0);
      EXPECT_LT(stream_id, options_.physical_stream_limit);
    }
  }

  std::shared_ptr<DAGGraph> dag_;
  WeightedStreamMergeOptions options_;
};

TEST_F(WeightedStreamMergerTest, Merge_EmptyRoutes_ReturnsSuccess) {
  std::vector<std::vector<int32_t>> logical_routes;
  std::vector<int32_t> logical_to_physical;

  WeightedStreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_TRUE(logical_to_physical.empty());
}

TEST_F(WeightedStreamMergerTest, Merge_DiamondGraph_RespectsPhysicalStreamLimit) {
  BuildDiamondGraph();
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}, {3}};
  std::vector<int32_t> logical_to_physical;

  WeightedStreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ASSERT_EQ(logical_to_physical.size(), logical_routes.size());
  for (const auto stream_id : logical_to_physical) {
    EXPECT_GE(stream_id, 0);
    EXPECT_LT(stream_id, options_.physical_stream_limit);
  }
}

TEST_F(WeightedStreamMergerTest, Merge_GroupedLogicalRoutes_ReturnsRouteMapping) {
  BuildDiamondGraph();
  std::vector<std::vector<int32_t>> logical_routes = {{0, 1}, {2, 3}};
  std::vector<int32_t> logical_to_physical;

  WeightedStreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ASSERT_EQ(logical_to_physical.size(), logical_routes.size());
  for (const auto stream_id : logical_to_physical) {
    EXPECT_GE(stream_id, 0);
    EXPECT_LT(stream_id, options_.physical_stream_limit);
  }
}

TEST_F(WeightedStreamMergerTest, Merge_MissingNodeInRoutes_ReturnsFailed) {
  BuildDiamondGraph();
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}};
  std::vector<int32_t> logical_to_physical;

  WeightedStreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::FAILED);
}

TEST_F(WeightedStreamMergerTest, Merge_DuplicatedNodeInRoutes_ReturnsFailed) {
  BuildDiamondGraph();
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {1}, {2}, {3}};
  std::vector<int32_t> logical_to_physical;

  WeightedStreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::FAILED);
}

TEST_F(WeightedStreamMergerTest, Merge_InvalidOptions_ReturnsFailed) {
  BuildDiamondGraph();
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}, {3}};
  std::vector<int32_t> logical_to_physical;

  options_.physical_stream_limit = 0;
  EXPECT_EQ(WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical), graphStatus::FAILED);
  EXPECT_TRUE(logical_to_physical.empty());

  options_.physical_stream_limit = 2;
  options_.window_width = 0;
  EXPECT_EQ(WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical), graphStatus::FAILED);

  options_.window_width = 1;
  options_.candidate_limit = 0;
  EXPECT_EQ(WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical), graphStatus::FAILED);

  options_.candidate_limit = 1;
  options_.light_stream_limit = 0;
  EXPECT_EQ(WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical), graphStatus::FAILED);

  options_.light_stream_limit = 1;
  options_.repair_moves = -1;
  EXPECT_EQ(WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical), graphStatus::FAILED);

  options_.repair_moves = 0;
  options_.resim_candidate_limit = -1;
  EXPECT_EQ(WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical), graphStatus::FAILED);
}

TEST_F(WeightedStreamMergerTest, Merge_WithOriginHintsAndRepair_ReturnsValidMapping) {
  BuildIndependentGraph(8, 12.0F);
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}};
  std::vector<int32_t> logical_to_physical;
  options_.physical_stream_limit = 3;
  options_.candidate_limit = 3;
  options_.light_stream_limit = 2;
  options_.repair_moves = 2;
  options_.resim_candidate_limit = 3;

  WeightedStreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ASSERT_EQ(logical_to_physical.size(), logical_routes.size());
  for (const auto stream_id : logical_to_physical) {
    EXPECT_GE(stream_id, 0);
    EXPECT_LT(stream_id, options_.physical_stream_limit);
  }
}

TEST_F(WeightedStreamMergerTest, Merge_WithLimitedCandidates_ReturnsValidMapping) {
  BuildIndependentGraph(6, 8.0F);
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}, {3}, {4}, {5}};
  std::vector<int32_t> logical_to_physical;
  options_.physical_stream_limit = 2;
  options_.candidate_limit = 1;
  options_.light_stream_limit = 1;
  options_.resim_candidate_limit = 0;

  WeightedStreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ASSERT_EQ(logical_to_physical.size(), logical_routes.size());
  for (const auto stream_id : logical_to_physical) {
    EXPECT_GE(stream_id, 0);
    EXPECT_LT(stream_id, options_.physical_stream_limit);
  }
}

TEST_F(WeightedStreamMergerTest, Merge_TwoStageParallelGraph_TriggersSimulationTieBreaks) {
  BuildTwoStageParallelGraph();
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}, {3}, {4}, {5}};
  std::vector<int32_t> logical_to_physical;
  options_.physical_stream_limit = 3;
  options_.candidate_limit = 4;
  options_.light_stream_limit = 3;
  options_.resim_candidate_limit = 3;

  WeightedStreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ASSERT_EQ(logical_to_physical.size(), logical_routes.size());
  for (const auto stream_id : logical_to_physical) {
    EXPECT_GE(stream_id, 0);
    EXPECT_LT(stream_id, options_.physical_stream_limit);
  }
}

TEST_F(WeightedStreamMergerTest, Merge_EmptyRoutesWithNodes_ReturnsSuccess) {
  BuildIndependentGraph(3, 1.0F);
  std::vector<int32_t> logical_to_physical;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, {}, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_TRUE(logical_to_physical.empty());
}

TEST_F(WeightedStreamMergerTest, Merge_EmptyLogicalRoute_ReturnsFailed) {
  BuildDiamondGraph();
  std::vector<int32_t> logical_to_physical;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, {{0}, {}}, logical_to_physical);

  EXPECT_EQ(status, graphStatus::FAILED);
}

TEST_F(WeightedStreamMergerTest, Merge_NodeIndexOutOfRange_ReturnsFailed) {
  BuildDiamondGraph();
  std::vector<int32_t> logical_to_physical;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, {{0}, {1}, {2}, {4}}, logical_to_physical);

  EXPECT_EQ(status, graphStatus::FAILED);
}

TEST_F(WeightedStreamMergerTest, Merge_CyclicGraph_ReturnsFailed) {
  BuildCycleGraph();
  std::vector<int32_t> logical_to_physical;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, {{0}, {1}}, logical_to_physical);

  EXPECT_EQ(status, graphStatus::FAILED);
}

TEST_F(WeightedStreamMergerTest, Merge_SinglePhysicalStream_MapsAllToZero) {
  BuildIndependentGraph(6, 3.0F);
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}, {3}, {4}, {5}};
  std::vector<int32_t> logical_to_physical;
  options_.physical_stream_limit = 1;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ASSERT_EQ(logical_to_physical.size(), logical_routes.size());
  for (const auto stream_id : logical_to_physical) {
    EXPECT_EQ(stream_id, 0);
  }
}

TEST_F(WeightedStreamMergerTest, Merge_CrossStreamDependency_ReturnsValidMapping) {
  BuildCrossStreamDependencyGraph();
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}, {3}, {4}, {5}};
  std::vector<int32_t> logical_to_physical;
  options_.physical_stream_limit = 3;
  options_.repair_moves = 3;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ExpectValidMapping(logical_to_physical, logical_routes.size());
}

TEST_F(WeightedStreamMergerTest, Merge_MissingAndExtremeCost_ReturnsValidMapping) {
  BuildMissingAndExtremeCostGraph();
  std::vector<std::vector<int32_t>> logical_routes = {{0, 1}, {2}, {3}};
  std::vector<int32_t> logical_to_physical;
  options_.physical_stream_limit = 3;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ExpectValidMapping(logical_to_physical, logical_routes.size());
}

TEST_F(WeightedStreamMergerTest, Merge_EdgeFromExternalNode_IgnoresOutsidePred) {
  auto n0 = AddNode("n0", 4.0F, 1U, 1U);
  auto n1 = AddNode("n1", 6.0F, 2U, 1U);
  auto external = std::make_shared<DAGNode>("external", "Dummy");
  ASSERT_EQ(dag_->AddEdge(external, 0, n0, 0), graphStatus::SUCCESS);
  ASSERT_EQ(dag_->AddEdge(n0, 0, n1, 0), graphStatus::SUCCESS);
  std::vector<int32_t> logical_to_physical;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, {{0}, {1}}, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ExpectValidMapping(logical_to_physical, 2U);
}

TEST_F(WeightedStreamMergerTest, Merge_LargeScaleIndependentGraph_ReturnsValidMapping) {
  BuildIndependentGraph(20, 2.0F);
  std::vector<std::vector<int32_t>> logical_routes;
  for (int32_t idx = 0; idx < 20; ++idx) {
    logical_routes.push_back({idx});
  }
  std::vector<int32_t> logical_to_physical;
  options_.physical_stream_limit = 4;
  options_.repair_moves = 4;

  auto status = WeightedStreamMerger(options_).Merge(*dag_, logical_routes, logical_to_physical);

  ASSERT_EQ(status, graphStatus::SUCCESS);
  ExpectValidMapping(logical_to_physical, logical_routes.size());
}

}  // namespace test
}  // namespace minidag
