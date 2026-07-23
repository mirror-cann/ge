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
#include <string>
#include "macro_utils/dt_public_scope.h"
#include "common/ge_inner_error_codes.h"
#include "graph/utils/op_desc_utils.h"
#include "graph_builder_utils.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "graph/utils/node_adapter.h"
#include "graph/ge_context.h"
#include "graph/ge_local_context.h"
#include "graph/passes/control_flow_and_stream/switch_to_stream_switch_pass.h"
#include "macro_utils/dt_public_unscope.h"
using namespace ge;

class UtestSwitch2StreamSwitchPass : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

// string name, string type, int in_cnt, int out_cnt, Format format,
// DataType data_type, vector<int64_t> shape
static ComputeGraphPtr BuildGraph() {
  DEF_GRAPH(switchGraph) {
    CHAIN(NODE("arg_0", DATA)->NODE("addNode", ADD)->NODE("output", NETOUTPUT));
    CHAIN(NODE("arg_1", DATA)->NODE("addNode"));

    CHAIN(NODE("cast0", CAST)->EDGE(0U, 0U)->NODE("switch1", SWITCH));
    CHAIN(NODE("switch1", SWITCH)->EDGE(0U, 0U)->NODE("switch2", SWITCH));

    CHAIN(NODE("data0", DATA)->EDGE(0U, 0U)->NODE("switch3", SWITCH));
  };

  return ToComputeGraph(switchGraph);
  ;
}

TEST_F(UtestSwitch2StreamSwitchPass, Run0) {
  Status retStatus;
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  retStatus = switch2StrPass.Run(graph);

  retStatus = switch2StrPass.ClearStatus();
  EXPECT_EQ(retStatus, SUCCESS);
}

TEST_F(UtestSwitch2StreamSwitchPass, FindSwitchCondInputSuccess) {
  Status retStatus;
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  auto node = graph->FindNode("switch1");
  auto data_anchor = node->GetOutDataAnchor(0U);
  retStatus = switch2StrPass.FindSwitchCondInput(data_anchor);
  EXPECT_EQ(retStatus, SUCCESS);
}

TEST_F(UtestSwitch2StreamSwitchPass, MarkBranchesSuccess) {
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  auto node = graph->FindNode("cast0");
  auto data_anchor = node->GetOutDataAnchor(0U);
  auto vec = std::vector<std::list<NodePtr>>();
  std::map<int64_t, std::vector<std::list<NodePtr>>> m;

  switch2StrPass.cond_node_map_[data_anchor] = m;
  EXPECT_EQ(switch2StrPass.MarkBranches(data_anchor, node, true), ge::SUCCESS);
}

TEST_F(UtestSwitch2StreamSwitchPass, GetGroupIdSuccess) {
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  auto node = graph->FindNode("cast0");
  GetThreadLocalContext().graph_options_.insert({OPTION_EXEC_ENABLE_TAILING_OPTIMIZATION, "1"});
  auto ret = switch2StrPass.GetGroupId(node);
  EXPECT_EQ(ret, 0);

  AttrUtils::SetStr(node->GetOpDesc(), ATTR_NAME_HCCL_FUSED_GROUP, "a");
  ret = switch2StrPass.GetGroupId(node);
  EXPECT_EQ(ret, 0);

  AttrUtils::SetStr(node->GetOpDesc(), ATTR_NAME_HCCL_FUSED_GROUP, "_123");
  ret = switch2StrPass.GetGroupId(node);
  EXPECT_EQ(ret, 123);
}

TEST_F(UtestSwitch2StreamSwitchPass, ModifySwitchInCtlEdgesFailed) {
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  auto switch1 = graph->FindNode("switch1");
  auto cast0 = graph->FindNode("cast0");
  auto s = std::set<NodePtr>({cast0});
  auto ret = switch2StrPass.ModifySwitchInCtlEdges(switch1, cast0, s);
  EXPECT_EQ(ret, INTERNAL_ERROR);
}

TEST_F(UtestSwitch2StreamSwitchPass, MoveCtrlEdgesFailed) {
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  auto switch3 = graph->FindNode("switch3");
  auto switch2 = graph->FindNode("switch2");
  auto data0 = graph->FindNode("data0");
  auto cast0 = graph->FindNode("cast0");
  switch2StrPass.switch_cyclic_map_[data0] = {"switch2"};

  GraphUtils::AddEdge(switch2->GetOutControlAnchor(), data0->GetInControlAnchor());
  GraphUtils::AddEdge(data0->GetOutControlAnchor(), cast0->GetInControlAnchor());
  EXPECT_NO_THROW(switch2StrPass.MoveCtrlEdges(data0, switch3));
}

TEST_F(UtestSwitch2StreamSwitchPass, CheckDuplicateNameSuccess) {
  SwitchToStreamSwitchPass switch2StrPass;
  auto name1 = switch2StrPass.CheckDuplicateName("test_node");
  EXPECT_EQ(name1, "test_node");
  auto name2 = switch2StrPass.CheckDuplicateName("test_node");
  EXPECT_EQ(name2, "test_node_1");
  auto name3 = switch2StrPass.CheckDuplicateName("test_node");
  EXPECT_EQ(name3, "test_node_2");
}

TEST_F(UtestSwitch2StreamSwitchPass, ModifySwitchOutCtlEdgesNotFound) {
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  auto switch1 = graph->FindNode("switch1");
  auto switch2 = graph->FindNode("switch2");
  auto cast0 = graph->FindNode("cast0");
  auto ret = switch2StrPass.ModifySwitchOutCtlEdges(switch1, switch2, cast0);
  EXPECT_EQ(ret, INTERNAL_ERROR);
}

TEST_F(UtestSwitch2StreamSwitchPass, UpdateControlFlowGroupSuccess) {
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  auto ret = switch2StrPass.UpdateControlFlowGroup(graph);
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(UtestSwitch2StreamSwitchPass, ClearStatusClearsAllMembers) {
  SwitchToStreamSwitchPass switch2StrPass;
  ComputeGraphPtr graph = BuildGraph();
  switch2StrPass.Run(graph);
  auto ret = switch2StrPass.ClearStatus();
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_TRUE(switch2StrPass.switch_nodes_.empty());
  EXPECT_TRUE(switch2StrPass.switch_cyclic_map_.empty());
  EXPECT_TRUE(switch2StrPass.bypass_nodes_.empty());
  EXPECT_TRUE(switch2StrPass.stream_switch_nodes_.empty());
  EXPECT_TRUE(switch2StrPass.cond_node_map_.empty());
  EXPECT_TRUE(switch2StrPass.switch_node_map_.empty());
  EXPECT_TRUE(switch2StrPass.node_num_map_.empty());
}
