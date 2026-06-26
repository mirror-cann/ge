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

#include <memory>
#include <string>
#include <vector>

#include "graph/build/dag/dag_graph.h"
#include "graph/build/dag/dag_node.h"
#include "graph/build/dag/dag_edge.h"
#include "graph/build/dag/dag_stream_allocator.h"
#include "graph/build/dag/dag_types.h"

namespace minidag {

class DAGGraphTest : public testing::Test {
 protected:
  void SetUp() override {
    graph_ = std::make_shared<DAGGraph>("test_graph");
  }

  void TearDown() override {
    graph_.reset();
  }

  std::shared_ptr<DAGGraph> graph_;
};

// --------------------
// 场景 1：DAGGraph 基本操作
// --------------------

/**
 * 场景 1-1: 节点添加、计数、默认值验证
 * 验证 AddNode 返回有效节点指针，节点计数正确，默认值为无效值
 */
TEST_F(DAGGraphTest, AddNodeBasic) {
  auto node = graph_->AddNode("node1", "Conv");
  ASSERT_NE(node, nullptr);
  EXPECT_EQ(node->GetName(), "node1");
  EXPECT_EQ(node->GetType(), "Conv");
  EXPECT_EQ(node->GetStreamId(), INVALID_STREAM_ID);
  EXPECT_EQ(node->GetTopoId(), INVALID_TOPO_ID);
  EXPECT_EQ(graph_->GetNodeCount(), 1);
  EXPECT_EQ(graph_->GetEdgeCount(), 0);
  EXPECT_EQ(node->GetInputCount(), 0);
  EXPECT_EQ(node->GetOutputCount(), 0);
}

/**
 * 场景 1-2: 重复节点添加返回 nullptr
 * 验证重复添加同名节点时返回 nullptr，节点计数保持不变
 */
TEST_F(DAGGraphTest, AddDuplicateNode) {
  auto node1 = graph_->AddNode("node1", "Conv");
  ASSERT_NE(node1, nullptr);
  auto node2 = graph_->AddNode("node1", "Relu");
  EXPECT_EQ(node2, nullptr);
  EXPECT_EQ(graph_->GetNodeCount(), 1);
}

/**
 * 场景 1-3: 节点查找功能
 * 验证 FindNode 能正确查找已存在节点，不存在的节点返回 nullptr
 */
TEST_F(DAGGraphTest, FindNode) {
  auto node1 = graph_->AddNode("node1", "Conv");
  auto node2 = graph_->AddNode("node2", "Relu");

  auto found1 = graph_->FindNode("node1");
  ASSERT_NE(found1, nullptr);
  EXPECT_EQ(found1->GetName(), "node1");
  EXPECT_EQ(found1->GetType(), "Conv");

  auto found2 = graph_->FindNode("node2");
  ASSERT_NE(found2, nullptr);

  auto not_found = graph_->FindNode("node3");
  EXPECT_EQ(not_found, nullptr);
}

/**
 * 场景 1-4: 节点返回顺序验证
 * 验证 GetAllNodes 返回的节点顺序与添加顺序一致
 */
TEST_F(DAGGraphTest, GetAllNodesOrder) {
  graph_->AddNode("node1", "Conv");
  graph_->AddNode("node2", "Relu");
  graph_->AddNode("node3", "Pool");

  auto all_nodes = graph_->GetAllNodes();
  EXPECT_EQ(all_nodes.size(), 3);
  EXPECT_EQ(all_nodes[0]->GetName(), "node1");
  EXPECT_EQ(all_nodes[1]->GetName(), "node2");
  EXPECT_EQ(all_nodes[2]->GetName(), "node3");
}

/**
 * 场景 1-5: 空图验证
 * 验证空图 GetAllNodes/GetAllEdges 返回空列表
 */
TEST_F(DAGGraphTest, EmptyGraph) {
  auto all_nodes = graph_->GetAllNodes();
  auto all_edges = graph_->GetAllEdges();
  EXPECT_EQ(all_nodes.size(), 0);
  EXPECT_EQ(all_edges.size(), 0);
  EXPECT_EQ(graph_->GetNodeCount(), 0);
  EXPECT_EQ(graph_->GetEdgeCount(), 0);
}

// --------------------
// 场景 2：边与节点关系
// --------------------

/**
 * 场景 2-1: 数据边添加验证
 * 验证 AddEdge 正确添加数据边，边计数、边属性、节点输入输出边正确
 */
TEST_F(DAGGraphTest, AddDataEdge) {
  auto node1 = graph_->AddNode("node1", "Conv");
  auto node2 = graph_->AddNode("node2", "Relu");

  auto status = graph_->AddEdge(node1, 0, node2, 0);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(graph_->GetEdgeCount(), 1);

  auto edges = graph_->GetAllEdges();
  EXPECT_EQ(edges.size(), 1);
  EXPECT_EQ(edges[0]->GetSrcNode()->GetName(), "node1");
  EXPECT_EQ(edges[0]->GetSrcPort(), 0);
  EXPECT_EQ(edges[0]->GetDstNode()->GetName(), "node2");
  EXPECT_EQ(edges[0]->GetDstPort(), 0);

  EXPECT_EQ(node1->GetOutputCount(), 1);
  EXPECT_EQ(node2->GetInputCount(), 1);
}

/**
 * 场景 2-2: 控制边（port=-1）添加
 * 验证 AddEdge 正确添加控制边，port 值为 -1
 */
TEST_F(DAGGraphTest, AddControlEdge) {
  auto node1 = graph_->AddNode("node1", "Conv");
  auto node2 = graph_->AddNode("node2", "Relu");

  auto status = graph_->AddEdge(node1, -1, node2, -1);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(graph_->GetEdgeCount(), 1);

  auto edges = graph_->GetAllEdges();
  EXPECT_EQ(edges[0]->GetSrcPort(), -1);
  EXPECT_EQ(edges[0]->GetDstPort(), -1);
}

/**
 * 场景 2-3: 空指针边添加返回错误
 * 验证 AddEdge 在节点为 nullptr 时返回错误，边计数保持为 0
 */
TEST_F(DAGGraphTest, AddEdgeWithNullNode) {
  auto node1 = graph_->AddNode("node1", "Conv");

  auto status1 = graph_->AddEdge(nullptr, 0, node1, 0);
  EXPECT_NE(status1, graphStatus::SUCCESS);

  auto status2 = graph_->AddEdge(node1, 0, nullptr, 0);
  EXPECT_NE(status2, graphStatus::SUCCESS);

  auto status3 = graph_->AddEdge(nullptr, 0, nullptr, 0);
  EXPECT_NE(status3, graphStatus::SUCCESS);

  EXPECT_EQ(graph_->GetEdgeCount(), 0);
}

/**
 * 场景 2-4: 节点连接关系验证
 * 验证 GetInputNodes/GetOutputNodes 正确返回连接的输入输出节点
 */
TEST_F(DAGGraphTest, NodeConnection) {
  auto node1 = graph_->AddNode("node1", "Conv");
  auto node2 = graph_->AddNode("node2", "Relu");
  auto node3 = graph_->AddNode("node3", "Pool");

  graph_->AddEdge(node1, 0, node2, 0);
  graph_->AddEdge(node2, 0, node3, 0);

  auto node1_outputs = node1->GetOutputNodes();
  EXPECT_EQ(node1_outputs.size(), 1);
  EXPECT_EQ(node1_outputs[0]->GetName(), "node2");

  auto node2_inputs = node2->GetInputNodes();
  EXPECT_EQ(node2_inputs.size(), 1);
  EXPECT_EQ(node2_inputs[0]->GetName(), "node1");

  auto node2_outputs = node2->GetOutputNodes();
  EXPECT_EQ(node2_outputs.size(), 1);
  EXPECT_EQ(node2_outputs[0]->GetName(), "node3");

  auto node3_inputs = node3->GetInputNodes();
  EXPECT_EQ(node3_inputs.size(), 1);
  EXPECT_EQ(node3_inputs[0]->GetName(), "node2");
}

/**
 * 场景 2-5: 多边统计验证
 * 验证多边添加后边数量正确，节点多输入输出边计数正确
 */
TEST_F(DAGGraphTest, MultiEdgeCount) {
  auto node1 = graph_->AddNode("node1", "Conv");
  auto node2 = graph_->AddNode("node2", "Relu");
  auto node3 = graph_->AddNode("node3", "Pool");

  graph_->AddEdge(node1, 0, node2, 0);
  graph_->AddEdge(node2, 0, node3, 0);
  graph_->AddEdge(node1, 1, node3, 0);

  EXPECT_EQ(graph_->GetEdgeCount(), 3);
  EXPECT_EQ(node3->GetInputCount(), 2);
  EXPECT_EQ(node1->GetOutputCount(), 2);
}

// --------------------
// 场景 3：节点属性测试
// --------------------

/**
 * 场景 3-1: 节点 StreamId 设置和获取
 * 验证 SetStreamId 和 GetStreamId 正确工作
 */
TEST_F(DAGGraphTest, NodeStreamIdSetGet) {
  auto node = graph_->AddNode("node1", "Conv");
  ASSERT_NE(node, nullptr);

  EXPECT_EQ(node->GetStreamId(), INVALID_STREAM_ID);

  node->SetStreamId(0);
  EXPECT_EQ(node->GetStreamId(), 0);

  node->SetStreamId(5);
  EXPECT_EQ(node->GetStreamId(), 5);

  node->SetStreamId(-1);
  EXPECT_EQ(node->GetStreamId(), -1);
}

/**
 * 场景 3-2: 节点 TopoId 设置和获取
 * 验证 SetTopoId 和 GetTopoId 正确工作
 */
TEST_F(DAGGraphTest, NodeTopoIdSetGet) {
  auto node = graph_->AddNode("node1", "Conv");
  ASSERT_NE(node, nullptr);

  EXPECT_EQ(node->GetTopoId(), INVALID_TOPO_ID);

  node->SetTopoId(0);
  EXPECT_EQ(node->GetTopoId(), 0);

  node->SetTopoId(10);
  EXPECT_EQ(node->GetTopoId(), 10);
}

/**
 * 场景 3-3: 节点 Cost 设置和获取
 * 验证 SetCost 和 GetCost 正确工作，包括默认值和覆盖
 */
TEST_F(DAGGraphTest, NodeCostSetGet) {
  auto node = graph_->AddNode("node1", "Conv");
  ASSERT_NE(node, nullptr);

  const auto &default_cost = node->GetCost();
  EXPECT_EQ(default_cost.execution_time, -1.0);
  EXPECT_EQ(default_cost.memory_usage, 0);

  NodeCost cost;
  cost.execution_time = 100.5;
  cost.memory_usage = 2048;
  node->SetCost(cost);

  const auto &retrieved_cost = node->GetCost();
  EXPECT_EQ(retrieved_cost.execution_time, 100.5);
  EXPECT_EQ(retrieved_cost.memory_usage, 2048);

  // 覆盖测试
  NodeCost cost2;
  cost2.execution_time = 200.0;
  cost2.memory_usage = 4096;
  node->SetCost(cost2);
  EXPECT_EQ(node->GetCost().execution_time, 200.0);
  EXPECT_EQ(node->GetCost().memory_usage, 4096);
}

/**
 * 场景 3-4: 多节点属性独立性验证
 * 验证多节点各自设置的属性不互相影响
 */
TEST_F(DAGGraphTest, MultiNodeAttributesIndependence) {
  auto node1 = graph_->AddNode("node1", "Conv");
  auto node2 = graph_->AddNode("node2", "Relu");

  node1->SetStreamId(0);
  node2->SetStreamId(1);
  EXPECT_EQ(node1->GetStreamId(), 0);
  EXPECT_EQ(node2->GetStreamId(), 1);

  node1->SetTopoId(0);
  node2->SetTopoId(5);
  EXPECT_EQ(node1->GetTopoId(), 0);
  EXPECT_EQ(node2->GetTopoId(), 5);

  NodeCost cost1;
  cost1.execution_time = 10.0;
  node1->SetCost(cost1);

  NodeCost cost2;
  cost2.execution_time = 20.0;
  node2->SetCost(cost2);

  EXPECT_EQ(node1->GetCost().execution_time, 10.0);
  EXPECT_EQ(node2->GetCost().execution_time, 20.0);
}

/**
 * 场景 4-2: 图名称获取验证
 * 验证 GetName 正确返回图名称
 */
TEST_F(DAGGraphTest, GraphName) {
  EXPECT_EQ(graph_->GetName(), "test_graph");

  auto graph2 = std::make_shared<DAGGraph>("another_graph");
  EXPECT_EQ(graph2->GetName(), "another_graph");
}

}  // namespace minidag
