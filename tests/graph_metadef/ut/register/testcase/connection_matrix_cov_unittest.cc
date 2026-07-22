/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "graph/compute_graph.h"
#include "graph/graph.h"
#include "graph/utils/graph_utils.h"
#include "register/graph_optimizer/graph_fusion/fusion_connection_matrix.h"
#include "graph_builder_utils.h"

using namespace std;
using namespace ge;

namespace fe {

class FusionConnectionMatrixCovUT : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}
};

TEST_F(FusionConnectionMatrixCovUT, DefaultConstructor_Test) {
  ConnectionMatrix cm;
  EXPECT_NO_THROW(cm.Generate(*ut::GraphBuilder("test").GetGraph()));
}

TEST_F(FusionConnectionMatrixCovUT, EnableDataFlowConstructor_Test) {
  ConnectionMatrix cm(true);
  EXPECT_NO_THROW(cm.Generate(*ut::GraphBuilder("test").GetGraph()));
}

TEST_F(FusionConnectionMatrixCovUT, GraphConstructor_Test) {
  auto graph = ut::GraphBuilder("test").GetGraph();
  EXPECT_NO_THROW(ConnectionMatrix cm(*graph));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_SimpleGraph) {
  ut::GraphBuilder builder("test_graph");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsConnected(node1, node2));
  EXPECT_FALSE(cm.IsConnected(node2, node1));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_WithDataFlow) {
  ut::GraphBuilder builder("test_graph_df");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsDataConnected(node1, node2));
  EXPECT_FALSE(cm.IsDataConnected(node2, node1));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_WithControlEdge) {
  ut::GraphBuilder builder("test_graph_ctrl");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  builder.AddControlEdge(node1, node2);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsConnected(node1, node2));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_WithDataFlowControlEdge) {
  ut::GraphBuilder builder("test_graph_df_ctrl");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  builder.AddControlEdge(node1, node2);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsConnected(node1, node2));
  EXPECT_FALSE(cm.IsDataConnected(node1, node2));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_SelfLoop) {
  ut::GraphBuilder builder("test_graph_self");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsConnected(node1, node1));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_MultiNodeChain) {
  ut::GraphBuilder builder("test_graph_chain");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto node3 = builder.AddNode("node3", "Relu", 1, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  builder.AddDataEdge(node2, 0, node3, 0);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsConnected(node1, node3));
  EXPECT_FALSE(cm.IsConnected(node3, node1));
}

TEST_F(FusionConnectionMatrixCovUT, Update_FusionNodes) {
  ut::GraphBuilder builder("test_graph_update");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto node3 = builder.AddNode("node3", "Relu", 1, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  builder.AddDataEdge(node2, 0, node3, 0);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  vector<NodePtr> fusion_nodes = {node1, node2};
  EXPECT_NO_THROW(cm.Update(*graph, fusion_nodes));
}

TEST_F(FusionConnectionMatrixCovUT, Update_WithDataFlow) {
  ut::GraphBuilder builder("test_graph_update_df");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto node3 = builder.AddNode("node3", "Relu", 1, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  builder.AddDataEdge(node2, 0, node3, 0);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  vector<NodePtr> fusion_nodes = {node1, node2};
  EXPECT_NO_THROW(cm.Update(*graph, fusion_nodes));
}

TEST_F(FusionConnectionMatrixCovUT, BackupAndRestoreBitMap) {
  ut::GraphBuilder builder("test_graph_backup");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  EXPECT_NO_THROW(cm.BackupBitMap());
  EXPECT_NO_THROW(cm.RestoreBitMap());
}

TEST_F(FusionConnectionMatrixCovUT, BackupAndRestoreBitMap_WithDataFlow) {
  ut::GraphBuilder builder("test_graph_backup_df");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  EXPECT_NO_THROW(cm.BackupBitMap());
  EXPECT_NO_THROW(cm.RestoreBitMap());
}

TEST_F(FusionConnectionMatrixCovUT, IsConnected_NotConnected) {
  ut::GraphBuilder builder("test_graph_not_conn");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  EXPECT_FALSE(cm.IsConnected(node1, node2));
  EXPECT_FALSE(cm.IsConnected(node2, node1));
}

TEST_F(FusionConnectionMatrixCovUT, IsDataConnected_NotConnected) {
  ut::GraphBuilder builder("test_graph_data_not_conn");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  EXPECT_FALSE(cm.IsDataConnected(node1, node2));
}

TEST_F(FusionConnectionMatrixCovUT, IsDataConnected_WithSelf) {
  ut::GraphBuilder builder("test_graph_data_self");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsDataConnected(node1, node1));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_EmptyGraph) {
  auto graph = ut::GraphBuilder("empty_graph").GetGraph();
  ConnectionMatrix cm;
  EXPECT_NO_THROW(cm.Generate(*graph));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_EmptyGraphWithDataFlow) {
  auto graph = ut::GraphBuilder("empty_graph_df").GetGraph();
  ConnectionMatrix cm(true);
  EXPECT_NO_THROW(cm.Generate(*graph));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_MultiInputNodes) {
  ut::GraphBuilder builder("test_graph_multi_input");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto node3 = builder.AddNode("node3", "Relu", 2, 1);
  builder.AddDataEdge(node1, 0, node3, 0);
  builder.AddDataEdge(node2, 0, node3, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsConnected(node1, node3));
  EXPECT_TRUE(cm.IsConnected(node2, node3));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_MultiInputNodesWithDataFlow) {
  ut::GraphBuilder builder("test_graph_multi_input_df");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto node3 = builder.AddNode("node3", "Relu", 2, 1);
  builder.AddDataEdge(node1, 0, node3, 0);
  builder.AddDataEdge(node2, 0, node3, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsDataConnected(node1, node3));
  EXPECT_TRUE(cm.IsDataConnected(node2, node3));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_DiamondShape) {
  ut::GraphBuilder builder("test_graph_diamond");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto node3 = builder.AddNode("node3", "Relu", 1, 1);
  auto node4 = builder.AddNode("node4", "Relu", 2, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  builder.AddDataEdge(node1, 0, node3, 0);
  builder.AddDataEdge(node2, 0, node4, 0);
  builder.AddDataEdge(node3, 0, node4, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsConnected(node1, node4));
}

TEST_F(FusionConnectionMatrixCovUT, Generate_DiamondShapeWithDataFlow) {
  ut::GraphBuilder builder("test_graph_diamond_df");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto node2 = builder.AddNode("node2", "Relu", 1, 1);
  auto node3 = builder.AddNode("node3", "Relu", 1, 1);
  auto node4 = builder.AddNode("node4", "Relu", 2, 1);
  builder.AddDataEdge(node1, 0, node2, 0);
  builder.AddDataEdge(node1, 0, node3, 0);
  builder.AddDataEdge(node2, 0, node4, 0);
  builder.AddDataEdge(node3, 0, node4, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  EXPECT_TRUE(cm.IsDataConnected(node1, node4));
}

TEST_F(FusionConnectionMatrixCovUT, Update_EmptyFusionNodes) {
  ut::GraphBuilder builder("test_graph_update_empty");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm;
  cm.Generate(*graph);
  vector<NodePtr> fusion_nodes;
  EXPECT_NO_THROW(cm.Update(*graph, fusion_nodes));
}

TEST_F(FusionConnectionMatrixCovUT, Update_EmptyFusionNodesWithDataFlow) {
  ut::GraphBuilder builder("test_graph_update_empty_df");
  auto node1 = builder.AddNode("node1", "Relu", 1, 1);
  auto graph = builder.GetGraph();

  ConnectionMatrix cm(true);
  cm.Generate(*graph);
  vector<NodePtr> fusion_nodes;
  EXPECT_NO_THROW(cm.Update(*graph, fusion_nodes));
}
}  // namespace fe
