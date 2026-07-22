/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/passes/standard_optimize/start_of_sequence_pass.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

#include "common/ge_inner_error_codes.h"
#include "common/profiling/profiling_properties.h"
#include "graph_builder_utils.h"

namespace ge {
class UtestGraphPassesStartOfSequencePass : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};
namespace {
/*
 *         var                         StartOfSequence
 *         |  \                             ||
 *         |   assign                       var
 *         |   //         =======>          |  \
 *      allreduce                           |   assign
 *         |                                |   //
 *      netoutput                        allreduce
 *                                          |
 *                                       netoutput
 */
ComputeGraphPtr BuildGraph() {
  auto builder = ut::GraphBuilder("test");
  const auto var = builder.AddNode("var", VARIABLE, 0, 1);
  const auto assign = builder.AddNode("assign", ASSIGN, 1, 1);
  const auto allreduce = builder.AddNode("allreduce", HCOMALLREDUCE, 1, 1);
  const auto netoutput = builder.AddNode("netoutput", NETOUTPUT, 1, 0);

  builder.AddDataEdge(var, 0, assign, 0);
  builder.AddDataEdge(var, 0, allreduce, 0);
  builder.AddControlEdge(assign, allreduce);
  builder.AddDataEdge(allreduce, 0, netoutput, 0);
  return builder.GetGraph();
}
}  // namespace

TEST(UtestGraphPassesStartOfSequencePass, testInsertStartBeforeVar) {
  ComputeGraphPtr graph = BuildGraph();
  ProfilingProperties::Instance().SetTrainingTrace(true);
  StartOfSequencePass start_of_sequence_pass;
  EXPECT_EQ(start_of_sequence_pass.Run(graph), SUCCESS);
  ProfilingProperties::Instance().SetTrainingTrace(false);

  // check
  const auto dst_node = graph->FindNode("var");
  const auto in_node_before_dst_node = dst_node->GetInControlAnchor()->GetPeerOutControlAnchors().at(0)->GetOwnerNode();
  EXPECT_NE(in_node_before_dst_node, nullptr);
  EXPECT_EQ(in_node_before_dst_node->GetType(), STARTOFSEQUENCE);
}

TEST(UtestGraphPassesStartOfSequencePass, testProfilingOff) {
  ComputeGraphPtr graph = BuildGraph();
  ProfilingProperties::Instance().SetTrainingTrace(false);
  StartOfSequencePass pass;
  EXPECT_EQ(pass.Run(graph), SUCCESS);
  auto start_node = graph->FindFirstNodeMatchType(STARTOFSEQUENCE);
  EXPECT_EQ(start_node, nullptr);
}

TEST(UtestGraphPassesStartOfSequencePass, testSubgraphSkip) {
  ComputeGraphPtr graph = BuildGraph();
  auto parent_graph = ut::GraphBuilder("parent").GetGraph();
  graph->SetParentGraph(parent_graph);
  ProfilingProperties::Instance().SetTrainingTrace(true);
  StartOfSequencePass pass;
  EXPECT_EQ(pass.Run(graph), SUCCESS);
  ProfilingProperties::Instance().SetTrainingTrace(false);
  auto start_node = graph->FindFirstNodeMatchType(STARTOFSEQUENCE);
  EXPECT_EQ(start_node, nullptr);
}

TEST(UtestGraphPassesStartOfSequencePass, testNoInputLessNodes) {
  auto builder = ut::GraphBuilder("test_no_inputless");
  auto const1 = builder.AddNode("const1", CONSTANT, 0, 1);
  auto const2 = builder.AddNode("const2", CONSTANT, 0, 1);
  auto add1 = builder.AddNode("add1", ADD, 2, 1);
  auto netoutput = builder.AddNode("netoutput", NETOUTPUT, 1, 0);
  builder.AddDataEdge(const1, 0, add1, 0);
  builder.AddDataEdge(const2, 0, add1, 1);
  builder.AddDataEdge(add1, 0, netoutput, 0);
  auto graph = builder.GetGraph();
  ProfilingProperties::Instance().SetTrainingTrace(true);
  StartOfSequencePass pass;
  EXPECT_EQ(pass.Run(graph), SUCCESS);
  ProfilingProperties::Instance().SetTrainingTrace(false);
  auto start_node = graph->FindFirstNodeMatchType(STARTOFSEQUENCE);
  EXPECT_NE(start_node, nullptr);
}
}  // namespace ge
