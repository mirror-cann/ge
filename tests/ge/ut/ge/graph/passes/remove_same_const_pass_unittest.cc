/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

// To test the RemoveSameConstPass

#include <gtest/gtest.h>
#include "ge_graph_dsl/graph_dsl.h"
#include "graph/passes/standard_optimize/remove_same_const_pass.h"

using namespace testing;
using namespace ge;
namespace ge {
class UtestRemoveSameConstPass : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

namespace {
/**
 *
 *        netoutput1
 *            |
 *           mul
 *          /   \
 *       add1  add2
 *         / \/ \
 *        /  /\  \
 *     const1 const2
 *        c\   /c
 *         no_op
 */
static ComputeGraphPtr BuildGraph() {
  DEF_GRAPH(g1) {
    CHAIN(NODE("const1", CONSTANTOP)->EDGE(0, 0)->NODE("add1", ADD));
    CHAIN(NODE("const1", CONSTANTOP)->EDGE(0, 0)->NODE("add2", ADD));
    CHAIN(NODE("const2", CONSTANTOP)->EDGE(0, 1)->NODE("add1", ADD));
    CHAIN(NODE("const2", CONSTANTOP)->EDGE(0, 1)->NODE("add2", ADD));

    CHAIN(NODE("add1", ADD)->EDGE(0, 0)->NODE("mul", MUL));
    CHAIN(NODE("add2", ADD)->EDGE(0, 1)->NODE("mul", MUL));
    CHAIN(NODE("mul", MUL)->EDGE(0, 0)->NODE("net_output", NETOUTPUT));

    CTRL_CHAIN(NODE("no_op", NOOP)->NODE("const1", CONSTANTOP));
    CTRL_CHAIN(NODE("no_op", NOOP)->NODE("const2", CONSTANTOP));
  };
  return ToComputeGraph(g1);
}
}  // namespace

TEST_F(UtestRemoveSameConstPass, test_normal_succ) {
  const auto graph = BuildGraph();
  int32_t weight[1] = {1};
  GeTensorDesc weight_desc(GeShape({1}), FORMAT_NHWC, DT_INT32);
  GeTensorPtr tensor = std::make_shared<GeTensor>(weight_desc, (uint8_t *)weight, sizeof(weight));
  for (const auto &node : graph->GetDirectNode()) {
    if (node->GetType() == CONSTANTOP) {
      const auto &op_desc = node->GetOpDesc();
      AttrUtils::SetTensor(op_desc, ATTR_NAME_WEIGHTS, tensor);
    }
  }
  EXPECT_EQ(graph->GetDirectNodesSize(), 7);
  RemoveSameConstPass pass;
  auto ret = pass.Run(graph);
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(graph->GetDirectNodesSize(), 6);
  const auto add1 = graph->FindNode("add1");
  ASSERT_NE(add1, nullptr);
  ASSERT_EQ(add1->GetInDataNodes().size(), 2);
  EXPECT_EQ(add1->GetInDataNodes().at(0)->GetName(), add1->GetInDataNodes().at(1)->GetName());
  const auto add2 = graph->FindNode("add2");
  ASSERT_NE(add2, nullptr);
  ASSERT_EQ(add2->GetInDataNodes().size(), 2);
  ASSERT_EQ(add1->GetInDataNodes().at(0)->GetName(), add2->GetInDataNodes().at(1)->GetName());
}

TEST_F(UtestRemoveSameConstPass, test_unknown_const) {
  const auto graph = BuildGraph();
  EXPECT_EQ(graph->GetDirectNodesSize(), 7);
  const auto const1 = graph->FindNode("const1");
  ASSERT_NE(const1, nullptr);
  const1->GetOpDesc()->MutableOutputDesc(0)->SetShape(GeShape({-1, 2}));
  const auto const2 = graph->FindNode("const2");
  ASSERT_NE(const2, nullptr);
  const2->GetOpDesc()->MutableOutputDesc(0)->SetShape(GeShape({-1, 2}));
  RemoveSameConstPass pass;
  auto ret = pass.Run(graph);
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(graph->GetDirectNodesSize(), 7);
  const auto add1 = graph->FindNode("add1");
  ASSERT_NE(add1, nullptr);
  ASSERT_EQ(add1->GetInDataNodes().size(), 2);
  EXPECT_NE(add1->GetInDataNodes().at(0)->GetName(), add1->GetInDataNodes().at(1)->GetName());
}

TEST_F(UtestRemoveSameConstPass, test_no_const_nodes) {
  DEF_GRAPH(g2) {
    CHAIN(NODE("data1", DATA)->EDGE(0, 0)->NODE("add1", ADD));
    CHAIN(NODE("add1", ADD)->EDGE(0, 0)->NODE("net_output", NETOUTPUT));
  };
  const auto graph = ToComputeGraph(g2);
  RemoveSameConstPass pass;
  auto ret = pass.Run(graph);
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(graph->GetDirectNodesSize(), 3);
}

TEST_F(UtestRemoveSameConstPass, test_different_weights) {
  const auto graph = BuildGraph();
  int32_t weight1[1] = {1};
  int32_t weight2[2] = {2, 3};
  GeTensorDesc weight_desc1(GeShape({1}), FORMAT_NHWC, DT_INT32);
  GeTensorPtr tensor1 = std::make_shared<GeTensor>(weight_desc1, (uint8_t *)weight1, sizeof(weight1));
  GeTensorDesc weight_desc2(GeShape({2}), FORMAT_NHWC, DT_INT32);
  GeTensorPtr tensor2 = std::make_shared<GeTensor>(weight_desc2, (uint8_t *)weight2, sizeof(weight2));
  const auto const1 = graph->FindNode("const1");
  const auto const2 = graph->FindNode("const2");
  AttrUtils::SetTensor(const1->GetOpDesc(), ATTR_NAME_WEIGHTS, tensor1);
  AttrUtils::SetTensor(const2->GetOpDesc(), ATTR_NAME_WEIGHTS, tensor2);
  RemoveSameConstPass pass;
  auto ret = pass.Run(graph);
  EXPECT_EQ(ret, SUCCESS);
  EXPECT_EQ(graph->GetDirectNodesSize(), 7);
}

TEST_F(UtestRemoveSameConstPass, test_different_control_inputs) {
  const auto graph = BuildGraph();
  int32_t weight[1] = {1};
  GeTensorDesc weight_desc(GeShape({1}), FORMAT_NHWC, DT_INT32);
  GeTensorPtr tensor = std::make_shared<GeTensor>(weight_desc, (uint8_t *)weight, sizeof(weight));
  for (const auto &node : graph->GetDirectNode()) {
    if (node->GetType() == CONSTANTOP) {
      AttrUtils::SetTensor(node->GetOpDesc(), ATTR_NAME_WEIGHTS, tensor);
    }
  }
  // Add different control edges to const1 and const2
  DEF_GRAPH(g_ctrl) {
    CHAIN(NODE("extra_ctrl1", NOOP)->NODE("extra_target1", ADD));
    CHAIN(NODE("extra_ctrl2", NOOP)->NODE("extra_target2", ADD));
  };
  const auto ctrl_graph = ToComputeGraph(g_ctrl);
  const auto extra_ctrl1 = ctrl_graph->FindNode("extra_ctrl1");
  const auto extra_ctrl2 = ctrl_graph->FindNode("extra_ctrl2");
  const auto const1 = graph->FindNode("const1");
  const auto const2 = graph->FindNode("const2");
  // const1 and const2 already have same control from no_op, give one an extra different control
  GraphUtils::AddEdge(extra_ctrl1->GetOutControlAnchor(), const1->GetInControlAnchor());
  GraphUtils::AddEdge(extra_ctrl2->GetOutControlAnchor(), const2->GetInControlAnchor());
  RemoveSameConstPass pass;
  auto ret = pass.Run(graph);
  EXPECT_EQ(ret, SUCCESS);
  // Should not merge because control inputs are different
  EXPECT_EQ(graph->GetDirectNodesSize(), 7);
}
}  // namespace ge
