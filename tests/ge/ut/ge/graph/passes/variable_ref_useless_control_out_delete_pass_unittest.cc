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
#include "graph/passes/variable_optimize/variable_ref_useless_control_out_delete_pass.h"
#include "graph/passes/standard_optimize/useless_control_out_remove_pass.h"
#include "graph_builder_utils.h"
namespace ge {
class UtestVariableRefUselessControlOutDeletePass : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}
};

namespace {
/*
 *      c
 * var1 -> addn2
 *   \    /
 *    addn1
 *     |
 *    data1
 */
ComputeGraphPtr BuildGraph1() {
  ut::GraphBuilder builder("g1");
  auto data1 = builder.AddNode("data1", "Data", 1, 1);
  auto addn1 = builder.AddNode("addn1", "AddN", 1, 1);
  auto var1 = builder.AddNode("var1", "Variable", 1, 1);
  auto addn2 = builder.AddNode("addn2", "AddN", 1, 1);
  AttrUtils::SetStr(var1->GetOpDesc(), REF_VAR_SRC_VAR_NAME, "TestVar");
  builder.AddDataEdge(data1, 0, addn1, 0);
  builder.AddDataEdge(addn1, 0, var1, 0);
  builder.AddDataEdge(addn1, 0, addn2, 0);
  builder.AddControlEdge(var1, addn2);
  return builder.GetGraph();
}

/*
 *      c
 * var1 -> addn2
 *   \       |
 *    addn1  |
 *        \  |
 *        data1
 */
ComputeGraphPtr BuildGraph2() {
  ut::GraphBuilder builder("g2");
  auto data1 = builder.AddNode("data1", "Data", 1, 1);
  auto addn1 = builder.AddNode("addn1", "AddN", 1, 1);
  auto var1 = builder.AddNode("var1", "Variable", 1, 1);
  auto addn2 = builder.AddNode("addn2", "AddN", 1, 1);

  AttrUtils::SetStr(var1->GetOpDesc(), REF_VAR_SRC_VAR_NAME, "TestVar");

  builder.AddDataEdge(data1, 0, addn1, 0);
  builder.AddDataEdge(addn1, 0, var1, 0);
  builder.AddDataEdge(data1, 0, addn2, 0);
  builder.AddControlEdge(var1, addn2);

  return builder.GetGraph();
}

/*
 *   netoutput1
 *   /    \
 * var1  addn2
 *   \    /
 *    addn1
 *     |
 *    data1
 */
ComputeGraphPtr BuildGraph3() {
  ut::GraphBuilder builder("g3");
  auto data1 = builder.AddNode("data1", "Data", 1, 1);
  auto addn1 = builder.AddNode("addn1", "AddN", 1, 1);
  auto var1 = builder.AddNode("var1", "Variable", 1, 1);
  auto addn2 = builder.AddNode("addn2", "AddN", 1, 1);
  auto netoutput1 = builder.AddNode("netoutput1", "NetOutput", 2, 2);

  AttrUtils::SetStr(var1->GetOpDesc(), REF_VAR_SRC_VAR_NAME, "TestVar");

  builder.AddDataEdge(data1, 0, addn1, 0);
  builder.AddDataEdge(addn1, 0, var1, 0);
  builder.AddDataEdge(addn1, 0, addn2, 0);
  builder.AddDataEdge(var1, 0, netoutput1, 0);
  builder.AddDataEdge(addn2, 0, netoutput1, 1);

  return builder.GetGraph();
}
}  // namespace

TEST_F(UtestVariableRefUselessControlOutDeletePass, DeleteControlOutSuccess) {
  auto graph = BuildGraph1();
  VariableRefUselessControlOutDeletePass pass;

  auto var1 = graph->FindNode("var1");
  EXPECT_EQ(var1->GetOutControlNodes().size(), 1);
  EXPECT_EQ(pass.Run(graph), SUCCESS);
  EXPECT_EQ(graph->GetAllNodes().size(), 4);
  EXPECT_EQ(var1->GetOutControlNodes().size(), 0);
}

TEST_F(UtestVariableRefUselessControlOutDeletePass, KeepControlOutSuccess) {
  auto graph = BuildGraph2();
  VariableRefUselessControlOutDeletePass pass;

  auto var1 = graph->FindNode("var1");
  EXPECT_EQ(var1->GetOutControlNodes().size(), 1);
  EXPECT_EQ(pass.Run(graph), SUCCESS);
  EXPECT_EQ(graph->GetAllNodes().size(), 4);
  EXPECT_EQ(var1->GetOutControlNodes().size(), 1);
}

TEST_F(UtestVariableRefUselessControlOutDeletePass, NothingChanged) {
  auto graph = BuildGraph3();
  VariableRefUselessControlOutDeletePass pass;

  auto var1 = graph->FindNode("var1");
  EXPECT_EQ(var1->GetOutNodes().size(), 1);
  EXPECT_EQ(pass.Run(graph), SUCCESS);
  EXPECT_EQ(graph->GetAllNodes().size(), 5);
  EXPECT_EQ(var1->GetOutNodes().size(), 1);
}

TEST_F(UtestVariableRefUselessControlOutDeletePass, NonVarNodeSkip) {
  ut::GraphBuilder builder("g_nonvar");
  auto data1 = builder.AddNode("data1", "Data", 1, 1);
  auto add1 = builder.AddNode("add1", "AddN", 1, 1);
  auto addn2 = builder.AddNode("addn2", "AddN", 1, 1);
  builder.AddDataEdge(data1, 0, add1, 0);
  builder.AddDataEdge(add1, 0, addn2, 0);
  builder.AddControlEdge(add1, addn2);
  auto graph = builder.GetGraph();
  VariableRefUselessControlOutDeletePass pass;
  EXPECT_EQ(pass.Run(graph), SUCCESS);
  EXPECT_EQ(graph->GetAllNodes().size(), 3);
}

TEST_F(UtestVariableRefUselessControlOutDeletePass, NoSrcVarNameAttr) {
  ut::GraphBuilder builder("g_no_srcvar");
  auto data1 = builder.AddNode("data1", "Data", 1, 1);
  auto var1 = builder.AddNode("var1", "Variable", 1, 1);
  auto addn2 = builder.AddNode("addn2", "AddN", 1, 1);
  builder.AddDataEdge(data1, 0, var1, 0);
  builder.AddControlEdge(var1, addn2);
  auto graph = builder.GetGraph();
  VariableRefUselessControlOutDeletePass pass;
  EXPECT_EQ(pass.Run(graph), SUCCESS);
  EXPECT_EQ(var1->GetOutControlNodes().size(), 1);
}

TEST_F(UtestVariableRefUselessControlOutDeletePass, UselessControlOutRemove_NotConstType) {
  ut::GraphBuilder builder("g_ucr1");
  auto data1 = builder.AddNode("data1", "Data", 0, 1);
  auto add1 = builder.AddNode("add1", "Add", 1, 1);
  builder.AddDataEdge(data1, 0, add1, 0);
  auto graph = builder.GetGraph();
  UselessControlOutRemovePass pass;
  auto node = graph->FindNode("add1");
  EXPECT_EQ(pass.Run(node), SUCCESS);
}

TEST_F(UtestVariableRefUselessControlOutDeletePass, UselessControlOutRemove_IsolatedConst) {
  ut::GraphBuilder builder("g_ucr2");
  auto const1 = builder.AddNode("const1", CONSTANT, 0, 1);
  auto graph = builder.GetGraph();
  UselessControlOutRemovePass pass;
  auto node = graph->FindNode("const1");
  EXPECT_EQ(pass.Run(node), SUCCESS);
}

TEST_F(UtestVariableRefUselessControlOutDeletePass, UselessControlOutRemove_ConstWithDataOutNoCtrlIn) {
  ut::GraphBuilder builder("g_ucr3");
  auto const1 = builder.AddNode("const1", CONSTANT, 0, 1);
  auto add1 = builder.AddNode("add1", "Add", 1, 1);
  auto netoutput = builder.AddNode("netoutput", "NetOutput", 1, 0);
  builder.AddDataEdge(const1, 0, add1, 0);
  builder.AddDataEdge(add1, 0, netoutput, 0);
  builder.AddControlEdge(const1, add1);
  auto graph = builder.GetGraph();
  UselessControlOutRemovePass pass;
  auto const_node = graph->FindNode("const1");
  EXPECT_EQ(const_node->GetOutControlNodes().size(), 1);
  EXPECT_EQ(pass.Run(const_node), SUCCESS);
  EXPECT_EQ(const_node->GetOutControlNodes().size(), 0);
}
}  // namespace ge
