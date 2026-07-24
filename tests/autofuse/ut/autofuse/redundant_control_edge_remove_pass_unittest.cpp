/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

#include "easy_graph/builder/graph_dsl.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "ge_graph_dsl/op_desc/op_desc_cfg_box.h"

#include "all_ops_cpp.h"
#include "graph_utils.h"
#include "op_creator_register.h"
#include "pattern_fusion/redundant_control_edge_remove_pass.h"
#include "graph_metadef/graph/debug/ge_util.h"

namespace ge {
class RedundantControlEdgeRemovePassTest : public testing::Test {
 protected:
  void SetUp() override {
    dlog_setlevel(0, 3, 0);
    RegisterAllOpCreator();
  }

  void TearDown() override {
    dlog_setlevel(0, 3, 0);
  }
};

// ========== RedundantControlEdge Remove 测试用例 ==========

TEST_F(RedundantControlEdgeRemovePassTest, RemoveConstControlEdgeForConcatFusion) {
  // 测试场景：单控制边输入，所有数据输出都以控制边源为数据输入
  // 图结构：
  //   data1 -data------> concat1
  //   data1 -ctrl-> const1 -data-> concat1
  //   data2 -----------> concat1
  //
  // 新逻辑：concat1 以 data1 为数据输入，const1 只有一个控制边输入 data1
  // 所有输出（concat1）都以所有控制边输入（data1）为数据输入 → 可删除
  // 预期：删除 data1->const1 的控制边

  auto data1 = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 20}).InCnt(0).OutCnt(1).Build("data1");
  auto data2 = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {5, 20}).InCnt(0).OutCnt(1).Build("data2");
  auto const1 = OP_CFG("Const").TensorDesc(FORMAT_ND, DT_FLOAT, {5, 20}).InCnt(0).OutCnt(1).Build("const1");

  auto concat1 = OP_CFG("ConcatD").TensorDesc(FORMAT_ND, DT_FLOAT, {15, 20}).InCnt(3).OutCnt(1).Build("concat1");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {15, 20}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data1)->DATA_EDGE(0, 0)->NODE(concat1));
    CHAIN(NODE(const1)->DATA_EDGE(0, 1)->NODE(concat1));
    CHAIN(NODE(data2)->DATA_EDGE(0, 2)->NODE(concat1));
    CHAIN(NODE(concat1)->NODE(relu)->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 concat_dim = 0 (在第0维拼接)
  AttrUtils::SetInt(graph->FindNode("concat1")->GetOpDesc(), "concat_dim", 0);
  AttrUtils::SetInt(graph->FindNode("concat1")->GetOpDesc(), "N", 3);

  // 添加控制边：data1 -ctrl-> const1
  auto data1_node = graph->FindNode("data1");
  auto const1_node = graph->FindNode("const1");
  ASSERT_NE(data1_node, nullptr);
  ASSERT_NE(const1_node, nullptr);
  auto ret = GraphUtils::AddEdge(data1_node->GetOutControlAnchor(), const1_node->GetInControlAnchor());
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  // 验证控制边已添加
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 1);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(RedundantControlEdgeRemovePass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证控制边已被删除
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 0);
}

TEST_F(RedundantControlEdgeRemovePassTest, RemoveConstControlEdgeSingleRef) {
  // 测试场景：单控制边输入，所有数据输出都以控制边源为数据输入
  // 图结构：
  //   data1 -data-> add
  //   const1 -data-> add
  //   data1 -ctrl-> const1
  //
  // 新逻辑：add 以 data1 为数据输入，const1 只有一个控制边输入 data1
  // 所有输出（add）都以所有控制边输入（data1）为数据输入 → 可删除
  // 预期：删除 data1->const1 的控制边

  auto data1 = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("data1");
  auto const1 = OP_CFG("Const").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("const1");
  auto add = OP_CFG("Add").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(2).OutCnt(1).Build("add");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data1)->DATA_EDGE(0, 0)->NODE(add));
    CHAIN(NODE(const1)->DATA_EDGE(0, 1)->NODE(add));
    CHAIN(NODE(add)->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 添加控制边：data1 -ctrl-> const1
  auto data1_node = graph->FindNode("data1");
  auto const1_node = graph->FindNode("const1");
  ASSERT_NE(data1_node, nullptr);
  ASSERT_NE(const1_node, nullptr);
  auto ret = GraphUtils::AddEdge(data1_node->GetOutControlAnchor(), const1_node->GetInControlAnchor());
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  // 验证控制边已添加
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 1);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(RedundantControlEdgeRemovePass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证控制边已被删除
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 0);
}

TEST_F(RedundantControlEdgeRemovePassTest, RemoveConstControlEdgeMultiRefAllOutputsControlled) {
  // 测试场景：单控制边输入，多数据输出，所有数据输出都以控制边源为数据输入
  // 图结构：
  //   data1 -data-> add1
  //   data1 -data-> add2
  //   const1 -data-> add1
  //   const1 -data-> add2
  //   data1 -ctrl-> const1
  //
  // 新逻辑：add1 和 add2 都以 data1 为数据输入，const1 只有一个控制边输入 data1
  // 所有输出都以所有控制边输入为数据输入 → 可删除
  // 预期：删除 data1->const1 的控制边

  auto data1 = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("data1");
  auto const1 = OP_CFG("Const").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("const1");
  auto add1 = OP_CFG("Add").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(2).OutCnt(1).Build("add1");
  auto add2 = OP_CFG("Add").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(2).OutCnt(1).Build("add2");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data1)->NODE(add1));
    CHAIN(NODE(data1)->NODE(add2));
    CHAIN(NODE(const1)->NODE(add1));
    CHAIN(NODE(const1)->NODE(add2));
    CHAIN(NODE(add1)->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(add2)->NODE("output_1", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 添加控制边：data1 -ctrl-> const1
  auto data1_node = graph->FindNode("data1");
  auto const1_node = graph->FindNode("const1");
  ASSERT_NE(data1_node, nullptr);
  ASSERT_NE(const1_node, nullptr);
  auto ret = GraphUtils::AddEdge(data1_node->GetOutControlAnchor(), const1_node->GetInControlAnchor());
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  // 验证控制边已添加
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 1);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(RedundantControlEdgeRemovePass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证控制边已被删除
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 0);
}

TEST_F(RedundantControlEdgeRemovePassTest, NotRemoveConstControlEdgePartialOutputsControlled) {
  // 测试场景：单控制边输入，多数据输出，只有部分数据输出以控制边源为数据输入
  // 图结构：
  //   data1 -data-> add1
  //   data2 -data-> add2  (data2 不是 data1)
  //   const1 -data-> add1
  //   const1 -data-> add2
  //   data1 -ctrl-> const1
  //
  // 新逻辑：add1 以 data1 为数据输入，但 add2 不以 data1 为数据输入
  // 不是所有输出都以所有控制边输入为数据输入 → 不能删除
  // 预期：保留 data1->const1 的控制边

  auto data1 = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("data1");
  auto data2 = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("data2");
  auto const1 = OP_CFG("Const").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("const1");
  auto add1 = OP_CFG("Add").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(2).OutCnt(1).Build("add1");
  auto add2 = OP_CFG("Add").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(2).OutCnt(1).Build("add2");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data1)->NODE(add1));
    CHAIN(NODE(data2)->NODE(add2));  // add2 的输入是 data2，不是 data1
    CHAIN(NODE(const1)->NODE(add1));
    CHAIN(NODE(const1)->NODE(add2));
    CHAIN(NODE(add1)->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(add2)->NODE("output_1", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 添加控制边：data1 -ctrl-> const1
  auto data1_node = graph->FindNode("data1");
  auto const1_node = graph->FindNode("const1");
  ASSERT_NE(data1_node, nullptr);
  ASSERT_NE(const1_node, nullptr);
  auto ret = GraphUtils::AddEdge(data1_node->GetOutControlAnchor(), const1_node->GetInControlAnchor());
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  // 验证控制边已添加
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 1);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(RedundantControlEdgeRemovePass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证控制边未被删除（add2 不以 data1 为数据输入）
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 1);
}

TEST_F(RedundantControlEdgeRemovePassTest, NotRemoveControlEdgeWhenSrcNotDataInputToAnyOutput) {
  // 测试场景：控制边来源不是任何数据输出的数据输入
  // 图结构：
  //   data1 -data-> add
  //   const1 -data-> add
  //   other -ctrl-> const1
  //
  // 新逻辑：add 不以 other 为数据输入
  // 不是所有输出都以所有控制边输入为数据输入 → 不能删除
  // 预期：保留 other->const1 的控制边

  auto data1 = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("data1");
  auto other = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("other");
  auto const1 = OP_CFG("Const").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("const1");
  auto add = OP_CFG("Add").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(2).OutCnt(1).Build("add");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data1)->DATA_EDGE(0, 0)->NODE(add));
    CHAIN(NODE(const1)->DATA_EDGE(0, 1)->NODE(add));
    CHAIN(NODE(other)->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(add)->NODE("output_1", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 添加控制边：other -ctrl-> const1
  auto other_node = graph->FindNode("other");
  auto const1_node = graph->FindNode("const1");
  ASSERT_NE(other_node, nullptr);
  ASSERT_NE(const1_node, nullptr);
  auto ret = GraphUtils::AddEdge(other_node->GetOutControlAnchor(), const1_node->GetInControlAnchor());
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  // 验证控制边已添加
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 1);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(RedundantControlEdgeRemovePass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证控制边未被删除
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 1);
}

TEST_F(RedundantControlEdgeRemovePassTest, NotRemoveMultiControlEdgeMultiDataOutput) {
  //   A -ctrl-> const1 -data-> B
  //   A -data-> B
  //   C -ctrl-> const1 -data-> D
  //   C -data-> D
  // 预期：保留所有控制边

  auto node_a = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(2).Build("A");
  auto node_c = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(2).Build("C");
  auto const1 = OP_CFG("Const").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(2).Build("const1");
  auto node_b = OP_CFG("Add").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(2).OutCnt(1).Build("B");
  auto node_d = OP_CFG("Add").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(2).OutCnt(1).Build("D");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(node_a)->DATA_EDGE(0, 0)->NODE(node_b));  // A -> B (data)
    CHAIN(NODE(node_c)->DATA_EDGE(0, 0)->NODE(node_d));  // C -> D (data)
    CHAIN(NODE(const1)->DATA_EDGE(0, 1)->NODE(node_b));  // const1 -> B (data)
    CHAIN(NODE(const1)->DATA_EDGE(1, 1)->NODE(node_d));  // const1 -> D (data)
    CHAIN(NODE(node_b)->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(node_d)->NODE("output_1", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 添加控制边：A -ctrl-> const1，C -ctrl-> const1
  auto a_node = graph->FindNode("A");
  auto c_node = graph->FindNode("C");
  auto const1_node = graph->FindNode("const1");
  ASSERT_NE(a_node, nullptr);
  ASSERT_NE(c_node, nullptr);
  ASSERT_NE(const1_node, nullptr);

  auto ret = GraphUtils::AddEdge(a_node->GetOutControlAnchor(), const1_node->GetInControlAnchor());
  ASSERT_EQ(ret, GRAPH_SUCCESS);
  ret = GraphUtils::AddEdge(c_node->GetOutControlAnchor(), const1_node->GetInControlAnchor());
  ASSERT_EQ(ret, GRAPH_SUCCESS);

  // 验证控制边已添加
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 2);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(RedundantControlEdgeRemovePass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证控制边未被删除（B 不以 C 为数据输入，D 不以 A 为数据输入）
  EXPECT_EQ(const1_node->GetInControlNodesSize(), 2);
}

}  // namespace ge
