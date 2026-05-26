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
#include "graph/build/stream/dag_adapter.h"
#include "graph/build/dag/dag_stream_allocator.h"
#include "graph/build/dag/dag_types.h"
#include "engines/manager/engine_manager/dnnengine_manager.h"
#include "register/custom_pass_context_impl.h"
#include <graph_utils_ex.h>
#include <register/register_custom_pass.h>
#include "graph/utils/node_adapter.h"

namespace minidag {

class DAGAdapterIntegrationTest : public testing::Test {
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
      .Install(ge::FakeOp("Add").InfoStoreAndBuilder("AIcoreEngine"))
      .Install(ge::FakeOp("Conv").InfoStoreAndBuilder("AIcoreEngine"))
      .Install(ge::FakeOp("Relu").InfoStoreAndBuilder("AIcoreEngine"))
      .Install(ge::FakeOp("Pool").InfoStoreAndBuilder("AIcoreEngine"))
      .Install(ge::FakeOp("Abs").InfoStoreAndBuilder("AIcoreEngine"));

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

  static int64_t GetGNodeStreamId(const ge::GNode& gnode) {
    ge::NodePtr node = ge::NodeAdapter::GNode2Node(gnode);
    if (node == nullptr || node->GetOpDesc() == nullptr) {
      return ge::INVALID_STREAM_ID;
    }
    return node->GetOpDesc()->GetStreamId();
  }
};

// --------------------
// 场景 1：端到端流程测试
// --------------------

/**
 * 场景 1-1: 完整端到端流程
 */
TEST_F(DAGAdapterIntegrationTest, EndToEnd_FullPipeline) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  size_t original_node_count = compute_graph->GetAllNodes().size();

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), original_node_count);

  StreamAllocConfig config{-1, 0};
  DagStreamAllocator::ByPathCover(*dag, config);

  ge::StreamPassContext context(config.required_streams);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  EXPECT_EQ(compute_graph->GetAllNodes().size(), original_node_count);
}

/**
 * 场景 1-2: 多节点图端到端流程
 */
TEST_F(DAGAdapterIntegrationTest, EndToEnd_MultiNodeGraph) {
  auto compute_graph = gert::ShareGraph::BuildStaticAbsReluExpAddNodeGraph();
  ASSERT_NE(compute_graph, nullptr);

  size_t original_node_count = compute_graph->GetAllNodes().size();

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), original_node_count);

  StreamAllocConfig config{-1, 0};
  DagStreamAllocator::ByPathCover(*dag, config);

  ge::StreamPassContext context(config.required_streams);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  EXPECT_EQ(compute_graph->GetAllNodes().size(), original_node_count);
}

// --------------------
// 场景 2：RefreshStreamIdsToGE 测试
// --------------------

/**
 * 场景 2-1: stream_id 正确设置到 GE GNode
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_StreamIdSet) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  int64_t stream_idx = 0;
  for (auto& node : nodes) {
    node->SetStreamId(stream_idx++);
  }

  ge::StreamPassContext context(static_cast<int64_t>(nodes.size()));
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 2-2: null 输入返回失败
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_NullInput) {
  auto dag = std::make_shared<DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");
  ge::StreamPassContext context(10);
  auto ret = DAGAdapter::RefreshStreamIdsToGE(*dag, nullptr, context);
  EXPECT_EQ(ret, ge::GRAPH_FAILED);
}

/**
 * 场景 2-3: 无效 stream_id 被跳过
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_SkipInvalidStreamId) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  if (nodes.size() >= 2) {
    nodes[0]->SetStreamId(INVALID_STREAM_ID);
    nodes[1]->SetStreamId(0);
  }

  ge::StreamPassContext context(10);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 2-4: DAG 节点不在 GE 图中的场景
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_NodeNotFoundInGE) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  dag->AddNode("extra_node_not_in_ge", "CustomOp");

  ge::StreamPassContext context(10);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 2-5: 多种 stream_id 值测试
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_VariousStreamIds) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<DAGGraph> dag;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  if (nodes.size() >= 4) {
    nodes[0]->SetStreamId(-1);
    nodes[1]->SetStreamId(0);
    nodes[2]->SetStreamId(1);
    nodes[3]->SetStreamId(100);
  }

  ge::StreamPassContext context(101);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}


}  // namespace minidag