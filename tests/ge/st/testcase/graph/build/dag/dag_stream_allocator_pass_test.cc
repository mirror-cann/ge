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
#include "graph/build/stream/stream_utils.h"
#include "graph/build/stream/logical_stream_allocator.h"
#include "graph/partition/engine_partitioner.h"
#include <graph_utils_ex.h>
#include <debug/ge_attr_define.h>
#include "graph/ge_local_context.h"
#include "graph/ge_context.h"
#include "register/custom_pass_helper.h"
#include "register/custom_pass_context_impl.h"
#include "graph/build/stream/dag_adapter.h"
#include "graph/build/stream/dag_stream_allocator_pass.h"
#include "graph/build/dag/dag_stream_allocator.h"
#include "graph/build/dag/dag_log.h"

namespace ge {

class MiniDAGStreamPassTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    std::map<std::string, std::string> options;
    EXPECT_EQ(GELib::Initialize(options), SUCCESS);
    GELib::GetInstance()->OpsKernelManagerObj().ops_kernel_store_.clear();

    GeRunningEnvFaker().Reset()
      .Install(FakeEngine("DNN_VM_GE_LOCAL").KernelInfoStore("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(FakeEngine("AIcoreEngine").KernelInfoStore("AIcoreEngine"))
      .Install(FakeOp(DATA).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(FakeOp(NETOUTPUT).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(FakeOp(ADD).InfoStoreAndBuilder("AIcoreEngine"));

    EngineConfPtr conf1 = std::make_shared<EngineConf>();
    conf1->id = "AIcoreEngine";
    conf1->scheduler_id = "scheduler";

    EngineConfPtr conf2 = std::make_shared<EngineConf>();
    conf2->id = "DNN_VM_GE_LOCAL";
    conf2->scheduler_id = "scheduler";
    conf2->skip_assign_stream = true;
    conf2->attach = true;

    SchedulerConf scheduler_conf;
    scheduler_conf.cal_engines[conf1->id] = conf1;
    scheduler_conf.cal_engines[conf2->id] = conf2;

    auto instance_ptr = GELib::GetInstance();
    instance_ptr->DNNEngineManagerObj().schedulers_["scheduler"] = scheduler_conf;
  }

  static void TearDownTestSuite() {
    GeRunningEnvFaker().Reset();
    GELib::GetInstance()->Finalize();
  }

  void SetUp() override {
    GetThreadLocalContext().SetGraphOption({});
  }

  void TearDown() override {
    GetThreadLocalContext().SetGraphOption({});
  }
};

// --------------------
// 场景 1：Pass 注册与执行测试
// --------------------

/**
 * 场景 1-1: 设置 LoadBalance:8，pass 执行
 */
TEST_F(MiniDAGStreamPassTest, Enable_WithLoadBalanceMode) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  std::map<std::string, int32_t> max_parallel_num;
  LogicalStreamAllocator allocator(max_parallel_num);
  int64_t total_stream_num = 0;
  int64_t main_stream_num = 0;

  EnginePartitioner partitioner;
  AttrUtils::SetStr(compute_graph, ATTR_NAME_SESSION_GRAPH_ID, "0");
  EXPECT_EQ(partitioner.Partition(compute_graph, EnginePartitioner::Mode::kSecondPartitioning), SUCCESS);

  const auto &graph_2_subgraphlist = partitioner.GetSubGraphMap();
  EXPECT_EQ(allocator.Assign(compute_graph, graph_2_subgraphlist, total_stream_num, main_stream_num), SUCCESS);

  int64_t compute_node_count = 0;
  for (const auto &node : compute_graph->GetAllNodes()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      continue;
    }
    std::string type = op_desc->GetType();
    if (type != "Data" && type != "NetOutput") {
      ++compute_node_count;
    }
  }

  EXPECT_GT(compute_node_count, 0);
  EXPECT_LE(total_stream_num, 2);
}

/**
 * 场景 1-2: 设置其他值，解析失败返回FAILED
 */
TEST_F(MiniDAGStreamPassTest, Fail_WithOtherValue) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "other_value";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = ge::RunMiniDAGStreamPass(ge_graph, context);
  EXPECT_EQ(ret, ge::FAILED);
}

/**
 * 场景 1-3: 不设置 option，pass 跳过
 */
TEST_F(MiniDAGStreamPassTest, Skip_WithoutOption) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  std::map<std::string, int32_t> max_parallel_num;
  LogicalStreamAllocator allocator(max_parallel_num);
  int64_t total_stream_num = 0;
  int64_t main_stream_num = 0;

  EnginePartitioner partitioner;
  AttrUtils::SetStr(compute_graph, ATTR_NAME_SESSION_GRAPH_ID, "0");
  EXPECT_EQ(partitioner.Partition(compute_graph, EnginePartitioner::Mode::kSecondPartitioning), SUCCESS);

  const auto &graph_2_subgraphlist = partitioner.GetSubGraphMap();
  EXPECT_EQ(allocator.Assign(compute_graph, graph_2_subgraphlist, total_stream_num, main_stream_num), SUCCESS);

  EXPECT_LE(total_stream_num, 2);
}

// --------------------
// 场景 2：端到端覆盖率补充测试
// --------------------

/**
 * 场景 2-1: 端到端 DAGGraph 节点属性验证
 */
TEST_F(MiniDAGStreamPassTest, EndToEnd_DAGNodeProperties) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildStaticAbsReluExpAddNodeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = minidag::DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  EXPECT_GT(nodes.size(), 0);

  for (const auto& node : nodes) {
    std::string type = node->GetType();
    EXPECT_FALSE(type.empty());
    int64_t topo_id = node->GetTopoId();
    EXPECT_GE(topo_id, 0);
    int64_t stream_id = node->GetStreamId();
    EXPECT_EQ(stream_id, minidag::INVALID_STREAM_ID);
    std::string name = node->GetName();
    EXPECT_FALSE(name.empty());
  }
}

/**
 * 场景 2-2: 端到端 DAGEdge 属性验证
 */
TEST_F(MiniDAGStreamPassTest, EndToEnd_DAGEdgeProperties) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = minidag::DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto edges = dag->GetAllEdges();
  for (const auto& edge : edges) {
    auto src = edge->GetSrcNode();
    auto dst = edge->GetDstNode();
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);
    int32_t src_port = edge->GetSrcPort();
    int32_t dst_port = edge->GetDstPort();
    EXPECT_GE(src_port, -1);
    EXPECT_GE(dst_port, -1);
  }
}

/**
 * 场景 2-3: 端到端 DAGNode 边关系验证
 */
TEST_F(MiniDAGStreamPassTest, EndToEnd_DAGNodeEdgeRelations) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildStaticAbsReluExpAddNodeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = minidag::DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, SUCCESS);
  ASSERT_NE(dag, nullptr);

  for (const auto& node : dag->GetAllNodes()) {
    size_t input_count = node->GetInputCount();
    size_t output_count = node->GetOutputCount();

    auto input_edges = node->GetInputEdges();
    auto output_edges = node->GetOutputEdges();
    EXPECT_EQ(input_edges.size(), input_count);
    EXPECT_EQ(output_edges.size(), output_count);

    auto input_nodes = node->GetInputNodes();
    auto output_nodes = node->GetOutputNodes();
    EXPECT_EQ(input_nodes.size(), input_count);
    EXPECT_EQ(output_nodes.size(), output_count);
  }
}

/**
 * 场景 2-4: 端到端 DAGNode Cost 属性验证
 */
TEST_F(MiniDAGStreamPassTest, EndToEnd_DAGNodeCostProperties) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = minidag::DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, SUCCESS);
  ASSERT_NE(dag, nullptr);

  for (const auto& node : dag->GetAllNodes()) {
    const auto& cost = node->GetCost();
    EXPECT_EQ(cost.execution_time, 0.0);
    EXPECT_EQ(cost.memory_usage, 0);
  }

  auto nodes = dag->GetAllNodes();
  if (!nodes.empty()) {
    minidag::NodeCost new_cost;
    new_cost.execution_time = 50.0;
    new_cost.memory_usage = 1024;
    nodes[0]->SetCost(new_cost);

    const auto& retrieved = nodes[0]->GetCost();
    EXPECT_EQ(retrieved.execution_time, 50.0);
    EXPECT_EQ(retrieved.memory_usage, 1024);
  }
}

/**
 * 场景 2-5: 端到端完整图结构验证
 */
TEST_F(MiniDAGStreamPassTest, EndToEnd_CompleteGraphStructure) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildStaticAbsReluExpAddNodeGraph();
  ASSERT_NE(compute_graph, nullptr);

  size_t ge_node_count = compute_graph->GetAllNodes().size();

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = minidag::DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, SUCCESS);
  ASSERT_NE(dag, nullptr);

  EXPECT_EQ(dag->GetNodeCount(), ge_node_count);
  EXPECT_GT(dag->GetEdgeCount(), 0);

  auto all_nodes = dag->GetAllNodes();
  EXPECT_EQ(all_nodes.size(), ge_node_count);

  auto all_edges = dag->GetAllEdges();
  EXPECT_EQ(all_edges.size(), dag->GetEdgeCount());

  EXPECT_FALSE(dag->GetName().empty());
}

/**
 * 场景 2-6: 端到端 DAGNode StreamId 设置验证
 */
TEST_F(MiniDAGStreamPassTest, EndToEnd_RefreshStreamIdsToGE) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = minidag::DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  int64_t stream_idx = 0;
  for (auto& node : nodes) {
    node->SetStreamId(stream_idx++);
    EXPECT_EQ(node->GetStreamId(), stream_idx - 1);
  }
}

// --------------------
// 场景 3：直接调用 RunMiniDAGStreamPass 函数测试
// --------------------

/**
 * 场景 3-1: 直接调用 RunMiniDAGStreamPass - 正常执行
 */
TEST_F(MiniDAGStreamPassTest, DirectCall_Success) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = ge::RunMiniDAGStreamPass(ge_graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);
}

/**
 * 场景 3-2: 直接调用 RunMiniDAGStreamPass - 空图（nullptr）
 */
TEST_F(MiniDAGStreamPassTest, DirectCall_NullGraph) {
  ge::StreamPassContext context(0);
  auto ret = ge::RunMiniDAGStreamPass(nullptr, context);
  EXPECT_EQ(ret, ge::FAILED);
}

/**
 * 场景 3-3: 直接调用 RunMiniDAGStreamPass - 带 StreamPassContext 分配
 */
TEST_F(MiniDAGStreamPassTest, DirectCall_WithStreamContext) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildStaticAbsReluExpAddNodeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  ge::StreamPassContext context(10);
  auto ret = ge::RunMiniDAGStreamPass(ge_graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  int64_t current_stream = context.GetCurrMaxStreamId();
  EXPECT_GE(current_stream, 10);
}

/**
 * 场景 3-4: 直接调用 RunMiniDAGStreamPass - 仅包含 Data/NetOutput 节点
 */
TEST_F(MiniDAGStreamPassTest, DirectCall_DataNetOutputOnly) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("data_netoutput_only");
  ge::Operator data1_op("data1", "Data");
  ge::Operator netoutput_op("netoutput1", "NetOutput");
  (void)graph->AddNodeByOp(data1_op);
  (void)graph->AddNodeByOp(netoutput_op);

  ge::StreamPassContext context(0);
  auto ret = ge::RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);
}

// --------------------
// 场景 4：dag_adapter.cc 控制边转换测试
// --------------------

/**
 * 场景 4-1: 控制边转换测试
 */
TEST_F(MiniDAGStreamPassTest, Adapter_ControlEdgeConversion) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("control_edge_test");
  ge::Operator data1_op("data1", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator relu1_op("relu1", "Relu");
  ge::Operator netoutput_op("netoutput1", "NetOutput");

  auto data1 = graph->AddNodeByOp(data1_op);
  auto add1 = graph->AddNodeByOp(add1_op);
  auto relu1 = graph->AddNodeByOp(relu1_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);

  graph->AddControlEdge(data1, relu1);
  graph->AddControlEdge(add1, netoutput);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = minidag::DAGAdapter::FromGEGraph(graph, dag);
  ASSERT_EQ(ret, ge::SUCCESS);
  ASSERT_NE(dag, nullptr);

  EXPECT_EQ(dag->GetNodeCount(), 4);
  EXPECT_EQ(dag->GetEdgeCount(), 2);

  auto edges = dag->GetAllEdges();
  for (const auto& edge : edges) {
    EXPECT_EQ(edge->GetSrcPort(), -1);
    EXPECT_EQ(edge->GetDstPort(), -1);
  }
}

/**
 * 场景 4-2: 控制边节点关系验证
 */
TEST_F(MiniDAGStreamPassTest, Adapter_ControlEdgeNodeRelation) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("control_edge_relation");
  ge::Operator data1_op("data1", "Data");
  ge::Operator relu1_op("relu1", "Relu");
  ge::Operator netoutput_op("netoutput1", "NetOutput");

  auto data1 = graph->AddNodeByOp(data1_op);
  auto relu1 = graph->AddNodeByOp(relu1_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);

  graph->AddControlEdge(data1, relu1);
  graph->AddControlEdge(relu1, netoutput);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = minidag::DAGAdapter::FromGEGraph(graph, dag);
  ASSERT_EQ(ret, ge::SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto relu_node = dag->FindNode("relu1");
  ASSERT_NE(relu_node, nullptr);

  auto input_nodes = relu_node->GetInputNodes();
  auto output_nodes = relu_node->GetOutputNodes();

  EXPECT_EQ(input_nodes.size(), 1);
  EXPECT_EQ(output_nodes.size(), 1);

  if (!input_nodes.empty()) {
    EXPECT_EQ(input_nodes[0]->GetName(), "data1");
  }
  if (!output_nodes.empty()) {
    EXPECT_EQ(output_nodes[0]->GetName(), "netoutput1");
  }
}

}  // namespace ge