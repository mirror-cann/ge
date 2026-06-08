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
#include <ge_running_env/ge_running_env_faker.h>
#include <ge_running_env/fake_op.h>
#include <common/share_graph.h>
#include "api/gelib/gelib.h"
#include "engines/manager/engine_manager/dnnengine_manager.h"
#include <graph_utils_ex.h>

#include "graph/build/stream/dag_adapter.h"
#include "graph/build/dag/dag_types.h"
#include "framework/common/ge_inner_error_codes.h"
#include "external/graph/graph.h"
#include "external/graph/operator.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/node_adapter.h"
#include "register/custom_pass_context_impl.h"

namespace ge {

// 测试辅助函数
namespace {
ge::ConstGraphPtr BuildGraphWithNodes() {
  auto graph = std::make_shared<ge::Graph>("test_graph");
  ge::Operator data1_op("data1", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator netoutput_op("NetOutput", "NetOutput");
  (void)graph->AddNodeByOp(data1_op);
  (void)graph->AddNodeByOp(add1_op);
  (void)graph->AddNodeByOp(netoutput_op);
  return graph;
}

ge::ConstGraphPtr BuildGraphWithControlEdge() {
  auto graph = std::make_shared<ge::Graph>("control_edge_graph");
  ge::Operator data1_op("data1", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator relu1_op("relu1", "Relu");
  ge::Operator netoutput_op("NetOutput", "NetOutput");
  auto data1 = graph->AddNodeByOp(data1_op);
  auto add1 = graph->AddNodeByOp(add1_op);
  auto relu1 = graph->AddNodeByOp(relu1_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);
  graph->AddControlEdge(data1, relu1);
  graph->AddControlEdge(add1, netoutput);
  return graph;
}

ge::ConstGraphPtr BuildEmptyGraph() {
  return std::make_shared<ge::Graph>("empty_graph");
}

ge::ConstGraphPtr BuildMultiNodeGraph() {
  auto graph = std::make_shared<ge::Graph>("multi_node_graph");
  ge::Operator data1_op("data1", "Data");
  ge::Operator data2_op("data2", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator relu1_op("relu1", "Relu");
  ge::Operator netoutput_op("NetOutput", "NetOutput");
  auto data1 = graph->AddNodeByOp(data1_op);
  auto data2 = graph->AddNodeByOp(data2_op);
  auto add1 = graph->AddNodeByOp(add1_op);
  auto relu1 = graph->AddNodeByOp(relu1_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);
  graph->AddControlEdge(data1, add1);
  graph->AddControlEdge(data2, add1);
  graph->AddControlEdge(add1, relu1);
  graph->AddControlEdge(relu1, netoutput);
  return graph;
}

void SetStreamLabel(const ge::GNode &gnode, const std::string &stream_label) {
  auto node = NodeAdapter::GNode2Node(gnode);
  ASSERT_NE(node, nullptr);
  ASSERT_NE(node->GetOpDesc(), nullptr);
  ASSERT_TRUE(AttrUtils::SetStr(node->GetOpDesc(), ATTR_NAME_STREAM_LABEL, stream_label));
}

ge::ConstGraphPtr BuildGraphWithStreamLabel() {
  auto graph = std::make_shared<ge::Graph>("stream_label_graph");
  ge::Operator data1_op("data1", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator relu1_op("relu1", "Relu");
  ge::Operator pool1_op("pool1", "Pool");
  ge::Operator abs1_op("abs1", "Abs");
  ge::Operator netoutput_op("NetOutput", "NetOutput");

  auto data1 = graph->AddNodeByOp(data1_op);
  auto add1 = graph->AddNodeByOp(add1_op);
  auto relu1 = graph->AddNodeByOp(relu1_op);
  auto pool1 = graph->AddNodeByOp(pool1_op);
  auto abs1 = graph->AddNodeByOp(abs1_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);
  SetStreamLabel(add1, "serial_label");
  SetStreamLabel(relu1, "serial_label");
  SetStreamLabel(pool1, "another_label");
  SetStreamLabel(abs1, "");
  graph->AddControlEdge(data1, add1);
  graph->AddControlEdge(add1, relu1);
  graph->AddControlEdge(relu1, pool1);
  graph->AddControlEdge(pool1, abs1);
  graph->AddControlEdge(abs1, netoutput);
  return graph;
}
}  // namespace

class DAGAdapterToGEStatusTest : public testing::Test {};

// --------------------
// 场景 1：ToGEStatus 状态映射测试
// --------------------

/**
 * 场景 1-1: 所有 graphStatus 到 ge::graphStatus 的映射
 * 验证所有已定义的 graphStatus 值都能正确映射
 */
TEST_F(DAGAdapterToGEStatusTest, ToGEStatusAllMappings) {
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::SUCCESS), ge::GRAPH_SUCCESS);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::FAILED), ge::GRAPH_FAILED);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::INVALID_NODE), ge::GRAPH_FAILED);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::INVALID_EDGE), ge::GRAPH_FAILED);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::NODE_NOT_FOUND), ge::GE_GRAPH_GRAPH_NODE_NULL);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::DUPLICATE_NODE), ge::GRAPH_FAILED);
}

/**
 * 场景 1-2: 未知错误码映射
 * 验证未知错误码正确映射为 ge::GRAPH_FAILED
 */
TEST_F(DAGAdapterToGEStatusTest, ToGEStatusUnknown) {
  EXPECT_EQ(DAGAdapter::ToGEStatus(static_cast<minidag::graphStatus>(999)), ge::GRAPH_FAILED);
  EXPECT_EQ(DAGAdapter::ToGEStatus(static_cast<minidag::graphStatus>(-1)), ge::GRAPH_FAILED);
}

class DAGAdapterGEIntegrationTest : public testing::Test {};

// --------------------
// 场景 2：FromGEGraph 基本测试
// --------------------

/**
 * 场景 2-1: 节点数量正确
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NodeCount) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), 3);
}

/**
 * 场景 2-2: 控制边转换正确
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_ControlEdgeCount) {
  auto ge_graph = BuildGraphWithControlEdge();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetEdgeCount(), 2);
}

/**
 * 场景 2-3: 拓扑序正确
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_TopoOrder) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  EXPECT_EQ(nodes.size(), 3);
  EXPECT_EQ(nodes[0]->GetTopoId(), 0);
  EXPECT_EQ(nodes[1]->GetTopoId(), 1);
  EXPECT_EQ(nodes[2]->GetTopoId(), 2);
}

/**
 * 场景 2-4: 节点类型正确
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NodeTypes) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto data1 = dag->FindNode("data1");
  ASSERT_NE(data1, nullptr);
  EXPECT_EQ(data1->GetType(), "Data");

  auto add1 = dag->FindNode("add1");
  ASSERT_NE(add1, nullptr);
  EXPECT_EQ(add1->GetType(), "Add");
}

/**
 * 场景 2-5: 多节点图转换
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_MultiNodeGraph) {
  auto ge_graph = BuildMultiNodeGraph();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), 5);
  EXPECT_EQ(dag->GetEdgeCount(), 4);
}

/**
 * 场景 2-6: GE stream_label 转换为 minidag 串行标记
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_StreamLabelConvertedToSerialFlag) {
  auto ge_graph = BuildGraphWithStreamLabel();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto add1 = dag->FindNode("add1");
  auto relu1 = dag->FindNode("relu1");
  auto pool1 = dag->FindNode("pool1");
  auto abs1 = dag->FindNode("abs1");
  auto data1 = dag->FindNode("data1");
  ASSERT_NE(add1, nullptr);
  ASSERT_NE(relu1, nullptr);
  ASSERT_NE(pool1, nullptr);
  ASSERT_NE(abs1, nullptr);
  ASSERT_NE(data1, nullptr);
  EXPECT_EQ(add1->GetSerialFlag(), "serial_label");
  EXPECT_EQ(relu1->GetSerialFlag(), "serial_label");
  EXPECT_EQ(pool1->GetSerialFlag(), "another_label");
  EXPECT_TRUE(abs1->GetSerialFlag().empty());
  EXPECT_TRUE(data1->GetSerialFlag().empty());
}

// --------------------
// 场景 3：边界/异常场景
// --------------------

/**
 * 场景 3-1: 空图转换
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_EmptyGraph) {
  auto ge_graph = BuildEmptyGraph();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), 0);
  EXPECT_EQ(dag->GetEdgeCount(), 0);
}

/**
 * 场景 3-2: null 输入
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NullInput) {
  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(nullptr, dag);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 3-3: DAGGraph 重复节点报错
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGGraph_DuplicateNode) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  ASSERT_NE(node1, nullptr);
  auto node2 = dag->AddNode("node1", "Relu");
  EXPECT_EQ(node2, nullptr);
}

/**
 * 场景 3-4: DAGGraph 边添加失败
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGGraph_AddEdge_NullNode) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  minidag::graphStatus ret = dag->AddEdge(nullptr, 0, node2, 0);
  EXPECT_EQ(ret, minidag::graphStatus::INVALID_NODE);

  ret = dag->AddEdge(node1, 0, nullptr, 0);
  EXPECT_EQ(ret, minidag::graphStatus::INVALID_NODE);
}

// --------------------
// 场景 4：节点边关系测试
// --------------------

/**
 * 场景 4-1: 控制边端口验证（端口值都为 -1）
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_ControlEdgePortValues) {
  auto ge_graph = BuildGraphWithControlEdge();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto edges = dag->GetAllEdges();
  EXPECT_EQ(edges.size(), 2);
  for (const auto& edge : edges) {
    EXPECT_EQ(edge->GetSrcPort(), -1);
    EXPECT_EQ(edge->GetDstPort(), -1);
  }
}

/**
 * 场景 4-2: 节点输入输出边数量
 * 图结构: data1 -> relu1, add1 -> netoutput
 * data1: 输出=1, 输入=0
 * relu1: 输入=1, 输出=0
 * add1: 输出=1, 输入=0
 * netoutput: 输入=1, 输出=0
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NodeInputOutputCount) {
  auto ge_graph = BuildGraphWithControlEdge();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto data1 = dag->FindNode("data1");
  ASSERT_NE(data1, nullptr);
  EXPECT_EQ(data1->GetInputCount(), 0);
  EXPECT_EQ(data1->GetOutputCount(), 1);

  auto relu1 = dag->FindNode("relu1");
  ASSERT_NE(relu1, nullptr);
  EXPECT_EQ(relu1->GetInputCount(), 1);
  EXPECT_EQ(relu1->GetOutputCount(), 0);
}

/**
 * 场景 4-3: 多节点图连接验证
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_MultiNodeConnections) {
  auto ge_graph = BuildMultiNodeGraph();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto add1 = dag->FindNode("add1");
  ASSERT_NE(add1, nullptr);
  EXPECT_EQ(add1->GetInputCount(), 2);

  auto relu1 = dag->FindNode("relu1");
  ASSERT_NE(relu1, nullptr);
  auto relu_input_nodes = relu1->GetInputNodes();
  EXPECT_EQ(relu_input_nodes.size(), 1);
  EXPECT_EQ(relu_input_nodes[0]->GetName(), "add1");
}

// --------------------
// 场景 5：节点属性验证
// --------------------

/**
 * 场景 5-1: 节点 StreamId 默认值
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NodeStreamIdDefault) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  for (const auto& node : nodes) {
    EXPECT_EQ(node->GetStreamId(), INVALID_STREAM_ID);
  }
}

/**
 * 场景 5-2: 节点 StreamId 设置验证
 */
TEST_F(DAGAdapterGEIntegrationTest, NodeStreamIdSetGet) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node = dag->AddNode("node1", "Conv");
  ASSERT_NE(node, nullptr);

  EXPECT_EQ(node->GetStreamId(), INVALID_STREAM_ID);
  node->SetStreamId(5);
  EXPECT_EQ(node->GetStreamId(), 5);
}

/**
 * 场景 5-3: 节点 Cost 属性
 */
TEST_F(DAGAdapterGEIntegrationTest, NodeCostSetGet) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node = dag->AddNode("node1", "Conv");
  ASSERT_NE(node, nullptr);

  minidag::NodeCost cost;
  cost.execution_time = 100.0;
  cost.memory_usage = 1024;
  node->SetCost(cost);

  const auto& retrieved_cost = node->GetCost();
  EXPECT_EQ(retrieved_cost.execution_time, 100.0);
  EXPECT_EQ(retrieved_cost.memory_usage, 1024);
}

// --------------------
// 场景 6：RefreshStreamIdsToGE 测试
// --------------------

/**
 * 场景 6-1: null graph 返回失败
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_NullGraph) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");
  ge::StreamPassContext context(10);
  auto ret = DAGAdapter::RefreshStreamIdsToGE(*dag, nullptr, context);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-2: 空 DAG 返回成功
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_EmptyDAG) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);
  auto empty_dag = std::make_shared<minidag::DAGGraph>("empty_dag");
  ge::StreamPassContext context(10);
  auto ret = DAGAdapter::RefreshStreamIdsToGE(*empty_dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-3: 正常流程
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_NormalFlow) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  for (auto& node : nodes) {
    node->SetStreamId(0);
  }

  ge::StreamPassContext context(0);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-4: INVALID_STREAM_ID 节点跳过
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_InvalidStreamId) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  for (auto& node : nodes) {
    node->SetStreamId(INVALID_STREAM_ID);
  }

  ge::StreamPassContext context(10);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-5: 节点不在 GE 图中时跳过
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_NodeNotInGE) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node = dag->AddNode("nonexistent_node", "Conv");
  node->SetStreamId(0);

  ge::StreamPassContext context(0);
  auto ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-6: stream_id 超出范围时返回失败
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_StreamIdOutOfRange) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  for (auto& node : nodes) {
    node->SetStreamId(100);
  }

  ge::StreamPassContext context(0);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_FAILED);
}

// --------------------
// 场景 7：私有函数测试（覆盖率）
// --------------------

/**
 * 场景 7-1: ConvertNodes 重复节点测试
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertNodes_DuplicateNode) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("data1", "Data");

  auto ret = DAGAdapter::ConvertNodes(ge_graph, *dag);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 7-2: ConvertEdges DAG 缺少节点测试
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertEdges_NodeNotInDAG) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto ret = DAGAdapter::ConvertEdges(ge_graph, *dag);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 7-3: ConvertNodes 空图测试
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertNodes_EmptyGraph) {
  auto ge_graph = BuildEmptyGraph();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("empty_dag");
  auto ret = DAGAdapter::ConvertNodes(ge_graph, *dag);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_EQ(dag->GetNodeCount(), 0);
}

/**
 * 场景 7-4: ConvertEdges 空图测试
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertEdges_EmptyGraph) {
  auto ge_graph = BuildEmptyGraph();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("empty_dag");
  auto ret = DAGAdapter::ConvertEdges(ge_graph, *dag);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_EQ(dag->GetEdgeCount(), 0);
}

/**
 * 场景 7-5: ConvertControlEdgesForNode 目标节点未找到
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertControlEdgesForNode_DstNotFound) {
  auto ge_graph = BuildGraphWithControlEdge();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("data1", "Data");

  auto src_node = dag->FindNode("data1");
  ASSERT_NE(src_node, nullptr);

  auto gnodes = ge_graph->GetDirectNode();
  for (const auto& gnode : gnodes) {
    ge::AscendString name;
    gnode.GetName(name);
    if (std::string(name.GetString()) == "data1") {
      int64_t edge_count = 0;
      auto ret = DAGAdapter::ConvertControlEdgesForNode(gnode, src_node, *dag, edge_count);
      EXPECT_NE(ret, ge::GRAPH_SUCCESS);
      break;
    }
  }
}

/**
 * 场景 7-6: DAGGraph GetAllEdges
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGGraph_GetAllEdges) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  dag->AddEdge(node1, 0, node2, 0);

  auto edges = dag->GetAllEdges();
  EXPECT_EQ(edges.size(), 1);
  EXPECT_EQ(edges[0]->GetSrcNode()->GetName(), "node1");
  EXPECT_EQ(edges[0]->GetDstNode()->GetName(), "node2");
}

/**
 * 场景 7-7: DAGNode GetInputNodes/GetOutputNodes
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGNode_GetInputOutputNodes) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  dag->AddEdge(node1, 0, node2, 0);

  auto inputs = node2->GetInputNodes();
  EXPECT_EQ(inputs.size(), 1);
  EXPECT_EQ(inputs[0]->GetName(), "node1");

  auto outputs = node1->GetOutputNodes();
  EXPECT_EQ(outputs.size(), 1);
  EXPECT_EQ(outputs[0]->GetName(), "node2");
}

/**
 * 场景 7-8: DAGNode GetInputEdges/GetOutputEdges
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGNode_GetInputOutputEdges) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  dag->AddEdge(node1, 0, node2, 1);

  auto out_edges = node1->GetOutputEdges();
  EXPECT_EQ(out_edges.size(), 1);
  EXPECT_EQ(out_edges[0]->GetSrcPort(), 0);
  EXPECT_EQ(out_edges[0]->GetDstPort(), 1);

  auto in_edges = node2->GetInputEdges();
  EXPECT_EQ(in_edges.size(), 1);
}

/**
 * 场景 7-9: DAGEdge GetSrcNode/GetDstNode
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGEdge_GetSrcDstNode) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  dag->AddEdge(node1, 0, node2, 0);

  auto edges = dag->GetAllEdges();
  EXPECT_EQ(edges.size(), 1);
  EXPECT_EQ(edges[0]->GetSrcNode()->GetName(), "node1");
  EXPECT_EQ(edges[0]->GetDstNode()->GetName(), "node2");
}

// ============================================================================
// 数据边测试（需要算子原型注册）
// ============================================================================

class DAGAdapterDataEdgeTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    std::map<std::string, std::string> options;
    EXPECT_EQ(ge::GELib::Initialize(options), ge::SUCCESS);
    ge::GELib::GetInstance()->OpsKernelManagerObj().ops_kernel_store_.clear();

    ge::GeRunningEnvFaker().Reset()
      .Install(ge::FakeEngine("DNN_VM_GE_LOCAL").KernelInfoStore("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeEngine("AIcoreEngine").KernelInfoStore("AIcoreEngine"))
      .Install(ge::FakeOp(ge::DATA).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeOp(ge::NETOUTPUT).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeOp("Add").InfoStoreAndBuilder("AIcoreEngine"));

    ge::EngineConfPtr conf1 = std::make_shared<ge::EngineConf>();
    conf1->id = "AIcoreEngine";
    conf1->scheduler_id = "scheduler";

    ge::EngineConfPtr conf2 = std::make_shared<ge::EngineConf>();
    conf2->id = "DNN_VM_GE_LOCAL";
    conf2->scheduler_id = "scheduler";
    conf2->skip_assign_stream = true;
    conf2->attach = true;

    ge::SchedulerConf scheduler_conf;
    scheduler_conf.cal_engines[conf1->id] = conf1;
    scheduler_conf.cal_engines[conf2->id] = conf2;

    auto instance_ptr = ge::GELib::GetInstance();
    instance_ptr->DNNEngineManagerObj().schedulers_["scheduler"] = scheduler_conf;
  }

  static void TearDownTestSuite() {
    ge::GeRunningEnvFaker().Reset();
    ge::GELib::GetInstance()->Finalize();
  }

  static ge::ConstGraphPtr ToConstGraphPtr(const ge::ComputeGraphPtr& compute_graph) {
    return ge::GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  }
};

/**
 * 场景 8-1: 数据边转换测试
 */
TEST_F(DAGAdapterDataEdgeTest, FromGEGraph_DataEdges) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  EXPECT_GT(dag->GetEdgeCount(), 0);
}

/**
 * 场景 8-2: 数据边数量验证
 */
TEST_F(DAGAdapterDataEdgeTest, FromGEGraph_DataEdgeCount) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  size_t original_data_edges = 0;
  for (const auto& node : compute_graph->GetAllNodes()) {
    original_data_edges += node->GetOutDataNodes().size();
  }

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  EXPECT_EQ(dag->GetEdgeCount(), original_data_edges);
}

}  // namespace ge
