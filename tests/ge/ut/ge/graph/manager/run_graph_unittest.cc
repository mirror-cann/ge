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
#include <memory>

#include "graph/anchor.h"
#include "graph/attr_value.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/utils/tensor_utils.h"
#include "omg/omg_inner_types.h"

#include "macro_utils/dt_public_scope.h"
#include "graph/manager/graph_manager_utils.h"
#include "graph/manager/graph_manager.h"
#include "macro_utils/dt_public_unscope.h"

using namespace std;
using namespace testing;
using namespace ge;
using domi::GetContext;

class UtestGraphRunTest : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {
    GetContext().out_nodes_map.clear();
  }
};

TEST_F(UtestGraphRunTest, RunGraphWithStreamAsync) {
  GraphManager graph_manager;
  GeTensor input0, input1;
  std::vector<GeTensor> inputs{input0, input1};
  std::vector<GeTensor> outputs;
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_manager.AddGraphNode(1, graph_node);
  GraphPtr graph = std::make_shared<Graph>("test");
  graph_node->SetGraph(graph);
  graph_node->SetRunFlag(false);
  graph_node->SetBuildFlag(true);
  Status ret = graph_manager.RunGraphWithStreamAsync(1, nullptr, 0, inputs, outputs);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(UtestGraphRunTest, RunGraphWithStreamAsync1) {
  GraphManager graph_manager;
  GeTensor input0, input1;
  GeTensor output0, output1;
  std::vector<GeTensor> inputs{input0, input1};
  std::vector<GeTensor> outputs{output0, output1};
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_manager.AddGraphNode(1, graph_node);

  auto compute_graph = std::make_shared<ComputeGraph>("test");
  std::vector<std::pair<NodePtr, int32_t>> output_nodes_info;
  OpDescPtr op_desc0 = std::make_shared<OpDesc>("Data", "Data");
  op_desc0->AddInputDesc(GeTensorDesc());
  OpDescPtr op_desc1 = std::make_shared<OpDesc>("Output", "Output");
  op_desc1->AddInputDesc(GeTensorDesc());
  NodePtr out_node0 = compute_graph->AddNode(op_desc0);
  NodePtr out_node1 = compute_graph->AddNode(op_desc1);
  output_nodes_info.emplace_back(std::make_pair(out_node0, 0));
  output_nodes_info.emplace_back(std::make_pair(out_node1, 1));
  compute_graph->SetGraphOutNodesInfo(output_nodes_info);

  GraphPtr graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  graph_node->SetGraph(graph);
  graph_node->SetRunFlag(false);
  graph_node->SetBuildFlag(false);
  Status ret = graph_manager.RunGraphWithStreamAsync(1, nullptr, 0, inputs, outputs);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(UtestGraphRunTest, GraphNode_LockUnlock) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->Lock();
  graph_node->Unlock();
  graph_node->Lock();
  graph_node->Unlock();
}

TEST_F(UtestGraphRunTest, GraphNode_IncreaseLoadCount) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->IncreaseLoadCount();
  graph_node->IncreaseLoadCount();
  graph_node->SetLoaded();
}

TEST_F(UtestGraphRunTest, GraphNode_SetGetRunGraphMode) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->SetRunGraphMode(RunGraphMode::kRunGraphAsync);
  EXPECT_EQ(graph_node->GetRunGraphMode(), RunGraphMode::kRunGraphAsync);
  graph_node->SetRunGraphMode(RunGraphMode::kRunGraphWithStreamAsync);
  EXPECT_EQ(graph_node->GetRunGraphMode(), RunGraphMode::kRunGraphWithStreamAsync);
}

TEST_F(UtestGraphRunTest, GraphNode_SetGetOptions) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  std::map<std::string, std::string> options = {{"key1", "val1"}, {"key2", "val2"}};
  graph_node->SetOptions(options);
  const auto &ret_options = graph_node->GetOptions();
  EXPECT_EQ(ret_options.at("key1"), "val1");
  EXPECT_EQ(ret_options.at("key2"), "val2");
}

TEST_F(UtestGraphRunTest, GraphNode_SetIsSpecificStream) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->SetIsSpecificStream(true);
  EXPECT_TRUE(graph_node->IsSpecificStream());
  graph_node->SetIsSpecificStream(false);
  EXPECT_FALSE(graph_node->IsSpecificStream());
}

TEST_F(UtestGraphRunTest, GraphNode_SetAsync) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->SetAsync(true);
  EXPECT_TRUE(graph_node->IsAsync());
  graph_node->SetAsync(false);
  EXPECT_FALSE(graph_node->IsAsync());
}

TEST_F(UtestGraphRunTest, GraphNode_SetCompiledFlag) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->SetCompiledFlag(true);
  EXPECT_TRUE(graph_node->GetCompiledFlag());
  graph_node->SetCompiledFlag(false);
  EXPECT_FALSE(graph_node->GetCompiledFlag());
}

TEST_F(UtestGraphRunTest, GraphNode_SetBuildFlag) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->SetBuildFlag(true);
  EXPECT_TRUE(graph_node->GetBuildFlag());
  graph_node->SetBuildFlag(false);
  EXPECT_FALSE(graph_node->GetBuildFlag());
}

TEST_F(UtestGraphRunTest, GraphNode_SetRunFlag) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->SetRunFlag(true);
  EXPECT_TRUE(graph_node->GetRunFlag());
  graph_node->SetRunFlag(false);
  EXPECT_FALSE(graph_node->GetRunFlag());
}

TEST_F(UtestGraphRunTest, GraphNode_SetLoadFlag) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  graph_node->SetLoadFlag(true);
  EXPECT_TRUE(graph_node->GetLoadFlag());
  graph_node->SetLoadFlag(false);
  EXPECT_FALSE(graph_node->GetLoadFlag());
}

TEST_F(UtestGraphRunTest, GraphModelListener_OnComputeDone) {
  GraphModelListener listener;
  std::vector<gert::Tensor> outputs;
  EXPECT_EQ(listener.OnComputeDone(1, 0, 0, outputs), SUCCESS);
  EXPECT_EQ(listener.GetResultCode(), 0U);
}

TEST_F(UtestGraphRunTest, GraphModelListener_ResetResult) {
  GraphModelListener listener;
  std::vector<gert::Tensor> outputs;
  EXPECT_EQ(listener.OnComputeDone(1, 0, 42, outputs), SUCCESS);
  EXPECT_EQ(listener.GetResultCode(), 42U);
  EXPECT_EQ(listener.ResetResult(), SUCCESS);
}

TEST_F(UtestGraphRunTest, SubGraphInfo_SetGet) {
  SubGraphInfo info;
  info.SetEngineName("test_engine");
  EXPECT_EQ(info.GetEngineName(), "test_engine");
  info.SetStreamLabel("stream_1");
  EXPECT_EQ(info.GetStreamLabel(), "stream_1");
  info.SetUserStreamLabel("user_stream_1");
  EXPECT_EQ(info.GetUserStreamLabel(), "user_stream_1");
  info.SetOutputContext("output_ctx");
}

TEST_F(UtestGraphRunTest, GraphNode_ParseFrozenInputIndex_Empty) {
  GraphNodePtr graph_node = std::make_shared<GraphNode>(1);
  EXPECT_EQ(graph_node->ParseFrozenInputIndex(), SUCCESS);
}
