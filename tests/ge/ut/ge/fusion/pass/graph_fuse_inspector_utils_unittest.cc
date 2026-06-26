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
#include <gmock/gmock.h>
#include <vector>
#include "ge/fusion/graph_fuse_inspector_utils.h"
#include "graph/op_desc.h"
#include "graph/utils/attr_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/node_adapter.h"
#include "graph_builder_utils.h"
#include "graph/fusion/fusion_utils.h"

#define private public
#include "graph/utils/cycle_detector.h"
#include "graph/utils/connection_matrix.h"
#include "graph/utils/connection_matrix_impl.h"
#undef private

namespace ge {
namespace fusion {
namespace {
ComputeGraphPtr BuildGraphMayCauseCycleWhenFusion() {
  ut::GraphBuilder builder("cycle_graph");
  const auto data = builder.AddNode("data", "Data", 0, 1);
  const auto cast1 = builder.AddNode("cast1", "Cast", 1, 1);
  const auto cast2 = builder.AddNode("cast2", "Cast", 1, 1);
  const auto transdata = builder.AddNode("transdata", "TransData", 1, 1);

  builder.AddDataEdge(data, 0, cast1, 0);
  builder.AddDataEdge(data, 0, cast2, 0);
  builder.AddDataEdge(cast1, 0, transdata, 0);
  builder.AddControlEdge(transdata, cast2);
  return builder.GetGraph();
}

ComputeGraphPtr BuildLinearGraph(std::vector<NodePtr> &before_nodes) {
  ut::GraphBuilder builder("linear_graph");
  const auto data = builder.AddNode("data", "Data", 0, 1);
  const auto add1 = builder.AddNode("add1", "Add", 1, 1);
  const auto add2 = builder.AddNode("add2", "Add", 1, 1);
  const auto netoutput = builder.AddNode("netoutput", "NetOutput", 1, 0);

  builder.AddDataEdge(data, 0, add1, 0);
  builder.AddDataEdge(add1, 0, add2, 0);
  builder.AddDataEdge(add2, 0, netoutput, 0);
  auto graph = builder.GetGraph();
  before_nodes = {add1, add2};
  return graph;
}

std::vector<GNode> ToGNodes(const std::vector<NodePtr> &nodes) {
  std::vector<GNode> g_nodes;
  for (const auto &node : nodes) {
    g_nodes.emplace_back(NodeAdapter::Node2GNode(node));
  }
  return g_nodes;
}

ComputeGraphPtr BuildTwoGraphs(std::vector<NodePtr> &graph1_nodes, std::vector<NodePtr> &graph2_nodes) {
  graph1_nodes.clear();
  graph2_nodes.clear();
  ut::GraphBuilder builder1("graph_1");
  const auto data1 = builder1.AddNode("data1", "Data", 0, 1);
  const auto add1 = builder1.AddNode("add1", "Add", 1, 1);
  const auto netoutput1 = builder1.AddNode("netoutput1", "NetOutput", 1, 0);
  builder1.AddDataEdge(data1, 0, add1, 0);
  builder1.AddDataEdge(add1, 0, netoutput1, 0);
  const auto graph1 = builder1.GetGraph();
  graph1_nodes = {add1};

  ut::GraphBuilder builder2("graph_2");
  const auto data2 = builder2.AddNode("data2", "Data", 0, 1);
  const auto add2 = builder2.AddNode("add2", "Add", 1, 1);
  const auto netoutput2 = builder2.AddNode("netoutput2", "NetOutput", 1, 0);
  builder2.AddDataEdge(data2, 0, add2, 0);
  builder2.AddDataEdge(add2, 0, netoutput2, 0);
  const auto graph2 = builder2.GetGraph();
  graph2_nodes = {add2};

  // return graph1 for convenience; callers can use both node vectors.
  (void)graph2;
  return graph1;
}
}  // namespace

class UtestGraphFuseInspectorUtils : public testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(UtestGraphFuseInspectorUtils, CanFuseFailedWhenWillCauseCycle) {
  const auto graph = BuildGraphMayCauseCycleWhenFusion();
  ASSERT_NE(graph, nullptr);
  const std::vector<NodePtr> nodes = {graph->FindNode("cast1"), graph->FindNode("cast2")};
  ASSERT_NE(nodes[0], nullptr);
  ASSERT_NE(nodes[1], nullptr);

  AscendString failed_reason;
  EXPECT_FALSE(GraphFuseInspectorUtils::CanFuse(ToGNodes(nodes), failed_reason));
  const auto *reason = failed_reason.GetString();
  ASSERT_NE(reason, nullptr);
  EXPECT_THAT(std::string(reason), testing::HasSubstr("cycle"));
}

TEST_F(UtestGraphFuseInspectorUtils, CanFuseFailedOnEmptyNodesBeforeFuse) {
  AscendString failed_reason;
  EXPECT_FALSE(GraphFuseInspectorUtils::CanFuse({}, failed_reason));
  const auto *reason = failed_reason.GetString();
  ASSERT_NE(reason, nullptr);
  EXPECT_THAT(std::string(reason), testing::HasSubstr("empty"));
}

TEST_F(UtestGraphFuseInspectorUtils, CanFuseFailedOnInvalidGNode) {
  AscendString failed_reason;
  EXPECT_FALSE(GraphFuseInspectorUtils::CanFuse({GNode()}, failed_reason));
  const auto *reason = failed_reason.GetString();
  ASSERT_NE(reason, nullptr);
  EXPECT_THAT(std::string(reason), testing::HasSubstr("convert"));
}

TEST_F(UtestGraphFuseInspectorUtils, CanFuseFailedOnNodesBelongToDifferentGraphs) {
  std::vector<NodePtr> graph1_nodes;
  std::vector<NodePtr> graph2_nodes;
  const auto graph1 = BuildTwoGraphs(graph1_nodes, graph2_nodes);
  ASSERT_NE(graph1, nullptr);
  ASSERT_EQ(graph1_nodes.size(), 1U);
  ASSERT_EQ(graph2_nodes.size(), 1U);

  AscendString failed_reason;
  EXPECT_FALSE(GraphFuseInspectorUtils::CanFuse(ToGNodes({graph1_nodes[0], graph2_nodes[0]}), failed_reason));
  const auto *reason = failed_reason.GetString();
  ASSERT_NE(reason, nullptr);
  EXPECT_THAT(std::string(reason), testing::HasSubstr("different graphs"));
}

TEST_F(UtestGraphFuseInspectorUtils, CanFuseFailedOnIsSupportFuseFailed) {
  std::vector<NodePtr> before_nodes;
  const auto graph = BuildLinearGraph(before_nodes);
  ASSERT_NE(graph, nullptr);
  ASSERT_EQ(before_nodes.size(), 2U);
  ASSERT_NE(before_nodes[0], nullptr);
  ASSERT_NE(before_nodes[1], nullptr);

  // Trigger ComputeGraphImpl::IsSupportFuse failure by conflicting USER_STREAM_LABEL.
  (void)AttrUtils::SetStr(before_nodes[0]->GetOpDesc(), public_attr::USER_STREAM_LABEL, "stream_a");
  (void)AttrUtils::SetStr(before_nodes[1]->GetOpDesc(), public_attr::USER_STREAM_LABEL, "stream_b");

  AscendString failed_reason;
  EXPECT_FALSE(GraphFuseInspectorUtils::CanFuse(ToGNodes(before_nodes), failed_reason));
  const auto *reason = failed_reason.GetString();
  ASSERT_NE(reason, nullptr);
  EXPECT_THAT(std::string(reason), testing::HasSubstr("stream_a"));
  EXPECT_THAT(std::string(reason), testing::HasSubstr("stream_b"));
}

TEST_F(UtestGraphFuseInspectorUtils, CanFuseSuccessOnValidNodes) {
  std::vector<NodePtr> before_nodes;
  const auto graph = BuildLinearGraph(before_nodes);
  ASSERT_NE(graph, nullptr);
  ASSERT_EQ(before_nodes.size(), 2U);
  ASSERT_NE(before_nodes[0], nullptr);
  ASSERT_NE(before_nodes[1], nullptr);

  AscendString failed_reason;
  EXPECT_TRUE(GraphFuseInspectorUtils::CanFuse(ToGNodes(before_nodes), failed_reason));
}

TEST_F(UtestGraphFuseInspectorUtils, ReportFuseFailedWhenNodesBeforeFuseEmpty) {
  CustomPassContext ctx;
  ctx.SetPassName("ut_pass");
  EXPECT_EQ(GraphFuseInspectorUtils::ReportFuse({}, {}, ctx), FAILED);
}

TEST_F(UtestGraphFuseInspectorUtils, ReportFuseFailedOnInvalidGNode) {
  CustomPassContext ctx;
  ctx.SetPassName("ut_pass");
  EXPECT_EQ(GraphFuseInspectorUtils::ReportFuse({GNode()}, {}, ctx), FAILED);
}

TEST_F(UtestGraphFuseInspectorUtils, ReportFuseFailedWhenPassNameEmpty) {
  std::vector<NodePtr> before_nodes;
  const auto graph = BuildLinearGraph(before_nodes);
  ASSERT_NE(graph, nullptr);
  CustomPassContext ctx;
  EXPECT_EQ(GraphFuseInspectorUtils::ReportFuse(ToGNodes(before_nodes), {}, ctx), FAILED);
}

TEST_F(UtestGraphFuseInspectorUtils, ReportFuseWithAfterFuseEmptyUseUpdate) {
  std::vector<NodePtr> before_nodes;
  const auto graph = BuildLinearGraph(before_nodes);
  ASSERT_NE(graph, nullptr);
  CustomPassContext ctx;
  ctx.SetPassName("ut_pass");
  EXPECT_EQ(GraphFuseInspectorUtils::ReportFuse(ToGNodes(before_nodes), {}, ctx), SUCCESS);
}

TEST_F(UtestGraphFuseInspectorUtils, ReportFuseWithAfterFuseFailedOnInvalidAfterFuseGNode) {
  std::vector<NodePtr> before_nodes;
  const auto graph = BuildLinearGraph(before_nodes);
  ASSERT_NE(graph, nullptr);
  CustomPassContext ctx;
  ctx.SetPassName("ut_pass");
  EXPECT_EQ(GraphFuseInspectorUtils::ReportFuse(ToGNodes(before_nodes), {GNode()}, ctx), FAILED);
}

TEST_F(UtestGraphFuseInspectorUtils, ReportFuseWithAfterFuseFailedOnNodesBelongToDifferentGraphs) {
  std::vector<NodePtr> graph1_nodes;
  std::vector<NodePtr> graph2_nodes;
  const auto graph1 = BuildTwoGraphs(graph1_nodes, graph2_nodes);
  ASSERT_NE(graph1, nullptr);
  ASSERT_EQ(graph1_nodes.size(), 1U);
  ASSERT_EQ(graph2_nodes.size(), 1U);

  CustomPassContext ctx;
  ctx.SetPassName("ut_pass");
  EXPECT_EQ(GraphFuseInspectorUtils::ReportFuse(ToGNodes({graph1_nodes[0]}), ToGNodes({graph2_nodes[0]}), ctx), FAILED);
}
}  // namespace fusion
}  // namespace ge
