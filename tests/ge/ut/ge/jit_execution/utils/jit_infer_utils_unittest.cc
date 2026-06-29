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
#include "faker/space_registry_faker.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "es_ge_test_ops.h"
#include "graph/utils/graph_utils_ex.h"
#include "framework/common/framework_types_internal.h"
#include "jit_execution/exe_points/execution_order.h"
#include "api/session/jit_execution/utils/jit_infer_utils.h"
#include "framework/ge_runtime_stub/include/common/compliant_share_graph.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_shape_inference.h"
#include "graph/optimize/symbolic/symbolic_kernel_factory.h"
#include "tests/framework/ge_runtime_stub/include/common/summary_checker.h"
#include "tests/framework/ge_runtime_stub/include/faker/space_registry_faker.h"
#include "graph/ge_local_context.h"
#include "jit_execution/common_setup.h"
#include <vector>
#include <stack>
using namespace std;
using namespace testing;
using namespace ge;

namespace {
static UINT32 UnsupportedSymbolicKernel(gert::InferSymbolComputeContext *) {
  return static_cast<UINT32>(UNSUPPORTED);
}
REGISTER_SYMBOLIC_KERNEL(CustomOp, UnsupportedSymbolicKernel);
}  // namespace

class JitInferUtilsUT : public testing::Test {
 protected:
  void SetUp() override {
    gert::SpaceRegistryFaker::CreateDefaultSpaceRegistry();
    std::map<std::string, std::string> options = {{ge::SOC_VERSION, "Ascend310"}};
    GetThreadLocalContext().SetGlobalOption(options);
  }
  void TearDown() override {}
};

/**
 *           data
 *            |
 *           abs0
 *            ||
 *           add
 *          /   \
 *       relu0  relu1
 *        |       |
 *       abs1   abs2
 *        |       |
 *    output0  output1
 */
TEST_F(JitInferUtilsUT, InferGraphAndGetInferredNodes) {
  const auto graph = cg::BuildAbsAddReluReluGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  std::vector<NodePtr> nodes;
  for (const auto &node : graph->GetDirectNode()) {
    nodes.push_back(node);
  }

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);
}

TEST_F(JitInferUtilsUT, InferGraphSkipWhenSingleOpScene) {
  const auto graph = cg::BuildAbsAddReluReluGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  (void)AttrUtils::SetBool(graph, ATTR_SINGLE_OP_SCENE, true);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes.size(), 0);
}

std::vector<NodePtr> getPreviousNodes(ComputeGraphPtr graph, size_t nodeNum) {
  std::vector<NodePtr> nodes;
  size_t index = 1U;
  for (const auto &node : graph->GetDirectNode()) {
    nodes.push_back(node);
    if (index == nodeNum) {
      break;
    }
    index++;
  }
  return nodes;
}

ComputeGraphPtr BuildAddAbsReLuReshapeGraph(const vector<int64_t> &shape) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"),
                                                                             EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  const auto abs0 = EsAbs(data0);
  const auto add = EsAdd(abs0, abs0);
  const auto abs = EsAbs(add);
  const auto relu = EsRelu(abs);
  const auto reshape = EsReshape(relu, relu, 3, 3);
  EsSetGraphOutput(reshape, 0);

  const auto ge_graph =
      std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  GE_ASSERT_NOTNULL(ge_graph);
  return GraphUtilsEx::GetComputeGraph(*ge_graph);
}

/**
 * 包含不可推导的二类算子的图推导前置, 推导时到该节点时断图，输出子图
 * 输入7个节点的图，二类算子为
 * reshape，值依赖只能确定shape，没有值，断图在输出节点，避免输出节点成空图，输出7个节点的子图 data
 *        |
 *       abs0
 *        ||
 *       add
 *        |
 *       abs
 *        |
 *       reLu
 *        ||
 *      reshape
 *        |
 *      netoutput
 *
 */
TEST_F(JitInferUtilsUT, graphContainsUnInferableReshapeShouldBeSliceReshape) {
  const auto graph = BuildAddAbsReLuReshapeGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  std::vector<NodePtr> nodes = getPreviousNodes(graph, 7);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);
}

ComputeGraphPtr BuildAddAbsReLuReduceSumAbsGraph(const vector<int64_t> &shape) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"),
                                                                             EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));

  const auto abs0 = EsAbs(data0);
  const auto add = EsAdd(abs0, abs0);
  const auto abs = EsAbs(add);
  const auto relu = EsRelu(abs);
  const auto reduceSum = EsReduceSum(relu, relu, true, false);
  const auto abs2 = EsAbs(reduceSum);
  EsSetGraphOutput(abs2, 0);

  const auto ge_graph =
      std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  GE_ASSERT_NOTNULL(ge_graph);
  return GraphUtilsEx::GetComputeGraph(*ge_graph);
}

/**
 * 包含不可推导的二类算子的图推导前置, 推导时到该节点时断图，输出子图
 * 输入8个节点的图，二类算子为 ReduceSum，断图在 ReduceSum节点，输出6个节点的子图
 *       data
 *         |
 *       abs0
 *        ||
 *       add
 *        |
 *       abs
 *        |
 *       reLu
 *        ||
 *       reducesum
 *        |
 *       abs2
 *        |
 *      netoutput
 */
TEST_F(JitInferUtilsUT, graphContainsUnInferableReduceSumShouldBeSliceReducesum) {
  const auto graph = BuildAddAbsReLuReduceSumAbsGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  // ReduceSum+下游Abs输出→{-2}，使UseStaticShapeIfWeCan也返回UNSUPPORTED
  for (auto &node : graph->GetDirectNode()) {
    auto t = node->GetType();
    if (t == "ReduceSum" || t == "Abs") {
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<NodePtr> nodes = getPreviousNodes(graph, 6);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);
}

/**
 * 包含可推导的二类算子的图推导前置, 推导时到该节点不断图，继续向后推导
 * 输入8个节点的图，二类算子为reshape，且value参数是常量const，断图在输出节点，输出8个节点的子图
 *       data
 *        |
 *       abs0
 *        ||
 *       add
 *        |
 *       abs
 *        |
 *       reLu  const
 *        |    /
 *      reshape
 *        |
 *      netoutput
 *
 */
TEST_F(JitInferUtilsUT, graphContainsConstReshapeShouldContinueInfer) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"),
                                                                             EsDestroyGraphBuilder);
  auto data0 = EsCreateGraphInputWithDetails(graph.get(), 0, "data0", nullptr, C_DT_FLOAT, C_FORMAT_ND, nullptr, 0);
  std::vector<int64_t> shape = {-1, -1, -1, -1, -1};
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  es::EsTensorHolder data0_holder(data0);
  const auto abs0 = es::Abs(data0_holder);
  const auto add = es::Add(abs0, abs0);
  const auto abs = es::Abs(add);
  const auto relu = es::Relu(abs);

  std::vector<int64_t> dims_data{1, 1, 3, 3, -1};
  int64_t dims_size = 5;
  es::EsTensorHolder const_0(EsCreateConstInt64(graph.get(), dims_data.data(), &dims_size, 1));

  auto reshape = es::Reshape(relu, const_0, 0, -1);
  EsSetGraphOutput(reshape.GetCTensorHolder(), 0);

  auto ge_graph = std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph, nullptr);
  const auto computerGraph = GraphUtilsEx::GetComputeGraph(*ge_graph);
  ASSERT_NE(computerGraph, nullptr);

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 5, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 5, 5, 6})));
  inputs.emplace_back(td);

  auto reshape_op_0 = computerGraph->FindFirstNodeMatchType("Reshape");
  ASSERT_NE(reshape_op_0, nullptr);
  auto reshape_op_desc0 = reshape_op_0->GetOpDesc();
  reshape_op_desc0->MutableInputDesc(0)->SetDataType(DT_INT64);
  reshape_op_desc0->MutableInputDesc(1)->SetDataType(DT_INT64);

  std::vector<NodePtr> nodes;
  for (const auto &node : computerGraph->GetDirectNode()) {
    nodes.push_back(node);
  }

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(computerGraph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);
}

ComputeGraphPtr BuildAddAbsReLuConstReduceSumAbsGraph(const vector<int64_t> &shape) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"),
                                                                             EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  es::EsTensorHolder data0_holder(data0);

  const auto abs0 = es::Abs(data0_holder);
  const auto add = es::Add(abs0, abs0);
  const auto abs = es::Abs(add);
  const auto relu = es::Relu(abs);

  es::EsTensorHolder const_scalar_int32(EsCreateScalarInt32(graph.get(), 1));
  const auto reduceSum = es::ReduceSum(relu, const_scalar_int32, true, false);

  const auto abs2 = es::Abs(reduceSum);
  EsSetGraphOutput(abs2.GetCTensorHolder(), 0);

  const auto ge_graph =
      std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  GE_ASSERT_NOTNULL(ge_graph);
  return GraphUtilsEx::GetComputeGraph(*ge_graph);
}

/**
 * 包含不可推导的二类算子的图推导前置, 推导时到该节点时断图，输出子图
 * 输入8个节点的图，二类算子为 ReduceSum，输入参数是固定值，继续推导，输出8个节点的子图
 *       data
 *         |
 *       abs0
 *        ||
 *       add
 *        |
 *       abs
 *        |
 *       reLu  const
 *        |    /
 *       reducesum
 *        |
 *       abs2
 *        |
 *      netoutput
 */
TEST_F(JitInferUtilsUT, graphContainsInferableConstReduceSumShouldBeContinueInfer) {
  const auto graph = BuildAddAbsReLuConstReduceSumAbsGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  auto reducesum_op_0 = graph->FindFirstNodeMatchType("ReduceSum");
  ASSERT_NE(reducesum_op_0, nullptr);
  auto reducesum_op_desc0 = reducesum_op_0->GetOpDesc();
  reducesum_op_desc0->MutableInputDesc(0)->SetDataType(DT_INT64);
  reducesum_op_desc0->MutableInputDesc(1)->SetDataType(DT_INT64);

  std::vector<NodePtr> nodes;
  for (const auto &node : graph->GetDirectNode()) {
    nodes.push_back(node);
  }

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);
}

/**
 *  netoutput
 *       \
 *      add
 *   /       \
 * Relu      Relu
 *  \        /
 *  \      transposed(0,2,1)
 *   \      /
 *    \   transposed(0,2,1)
 *     \  /
 *    data
 */
TEST_F(JitInferUtilsUT, AddReluReluTransposeDGraphInferV1) {
  GeRunningEnvFaker().Reset().Install(
      FakeOp("Relu").Inputs({"x"}).Outputs({"y"}).InfoStoreAndBuilder("AIcoreEngine").InferShape(SingleIOForwardInfer));
  const auto graph = cg::BuildAddReluTransposeGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  SymbolicShapeInference symbolic;

  auto relu_node = graph->FindNode("Relu_2");
  ASSERT_NE(relu_node, nullptr);
  // before prepare no infershape
  EXPECT_EQ(relu_node->GetOpDescBarePtr()->GetInputDescPtr(0)->GetShape().GetDimNum(), 0);
  EXPECT_EQ(relu_node->GetOpDescBarePtr()->GetOutputDescPtr(0)->GetShape().GetDimNum(), 0);

  ASSERT_EQ(JitInferUtils::PrepareBeforeInferSymbol(graph, inputs), SUCCESS);

  // after prepare infershape Relu_2
  EXPECT_EQ(relu_node->GetOpDescBarePtr()->GetInputDescPtr(0)->GetShape(), GeShape({-1, -1, -1}));
  EXPECT_EQ(relu_node->GetOpDescBarePtr()->GetOutputDescPtr(0)->GetShape(), GeShape({-1, -1, -1}));

  ASSERT_EQ(SymbolicShapeSymbolizer::Symbolize(graph, inputs), SUCCESS);
  ASSERT_EQ(symbolic.Infer(graph), SUCCESS);

  ASSERT_EQ(JitInferUtils::PrepareBeforeInferSymbol(graph, inputs), SUCCESS);
  ASSERT_EQ(symbolic.Infer(graph), SUCCESS);

  ASSERT_EQ(JitInferUtils::PrepareBeforeInferSymbol(graph, inputs), SUCCESS);
  ASSERT_EQ(symbolic.Infer(graph), SUCCESS);

  std::map<std::string, size_t> node_types_to_count0;
  node_types_to_count0.emplace("Data", 1);
  node_types_to_count0.emplace("Add", 1);
  node_types_to_count0.emplace("Relu", 2);
  node_types_to_count0.emplace("TransposeD", 2);
  node_types_to_count0.emplace("NetOutput", 1);
  EXPECT_EQ(gert::SummaryChecker(graph).StrictDirectNodeTypes(node_types_to_count0), "success");
}

TEST_F(JitInferUtilsUT, AddReluReluTransposeDGraphInferV2) {
  const auto graph = cg::BuildAddReluTransposeGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  ASSERT_EQ(JitInferUtils::PrepareBeforeInferSymbol(graph, inputs), SUCCESS);
  std::vector<NodePtr> nodes;
  for (const auto &node : graph->GetDirectNode()) {
    nodes.push_back(node);
  }

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);

  ASSERT_EQ(JitInferUtils::PrepareBeforeInferSymbol(graph, inputs), SUCCESS);
  nodes.clear();
  for (const auto &node : graph->GetDirectNode()) {
    nodes.push_back(node);
  }
  ASSERT_EQ(infered_nodes, nodes);

  std::map<std::string, size_t> node_types_to_count0;
  node_types_to_count0.emplace("Data", 1);
  node_types_to_count0.emplace("Add", 1);
  node_types_to_count0.emplace("Relu", 2);
  node_types_to_count0.emplace("TransposeD", 2);
  node_types_to_count0.emplace("NetOutput", 1);
  EXPECT_EQ(gert::SummaryChecker(graph).StrictDirectNodeTypes(node_types_to_count0), "success");
}

ComputeGraphPtr BuildAddReshapeReLuConstReduceSumAbsGraph(const vector<int64_t> &shape) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"),
                                                                             EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  es::EsTensorHolder data0_holder(data0);

  const auto abs0 = es::Abs(data0_holder);
  const auto add = es::Add(abs0, abs0);
  const auto reshape = es::Reshape(add, add, 3, 3);
  const auto relu = es::Relu(reshape);

  es::EsTensorHolder const_scalar_int32(EsCreateScalarInt32(graph.get(), 1));
  const auto reduceSum = es::ReduceSum(relu, const_scalar_int32, true, false);

  const auto abs2 = es::Abs(reduceSum);
  EsSetGraphOutput(abs2.GetCTensorHolder(), 0);

  const auto ge_graph =
      std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  GE_ASSERT_NOTNULL(ge_graph);
  return GraphUtilsEx::GetComputeGraph(*ge_graph);
}

/**
 * 包含不可推导的二类算子的图推导前置, 推导时到该节点时断图，输出子图
 * 输入8个节点的图，二类算子为 reshape，中断推导，输出4个节点的子图，不包括const节点
 *       data
 *         |
 *       abs0
 *        ||
 *       add
 *        ||
 *     reshape
 *        |
 *       reLu
 *        |  \
 *        |   const
 *        |    /
 *       reducesum
 *        |
 *       abs2
 *        |
 *      netoutput
 */
TEST_F(JitInferUtilsUT, graphContainsConstNodeWithControlEdge) {
  const auto graph = BuildAddReshapeReLuConstReduceSumAbsGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  auto reducesum_op_0 = graph->FindFirstNodeMatchType("ReduceSum");
  ASSERT_NE(reducesum_op_0, nullptr);
  auto reducesum_op_desc0 = reducesum_op_0->GetOpDesc();
  reducesum_op_desc0->MutableInputDesc(0)->SetDataType(DT_INT64);
  reducesum_op_desc0->MutableInputDesc(1)->SetDataType(DT_INT64);

  auto relu_op_0 = graph->FindFirstNodeMatchType("Relu");
  auto const_0 = graph->FindFirstNodeMatchType("Const");
  ASSERT_EQ(GraphUtils::AddEdge(relu_op_0->GetOutControlAnchor(), const_0->GetInControlAnchor()), ge::SUCCESS);

  // Reshape shape输入dtype→INT64 + 所有有内核算子输出→{-2}
  for (auto &node : graph->GetDirectNode()) {
    auto t = node->GetType();
    if (t == "Reshape") {
      node->GetOpDesc()->MutableInputDesc(1)->SetDataType(DT_INT64);
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    } else if (t == "ReduceSum" || t == "Relu" || t == "Abs") {
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<NodePtr> nodes = getPreviousNodes(graph, 4);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);
}

ComputeGraphPtr BuildAddReshapeReLuConstReshapeConstReduceSumAbsGraph(const vector<int64_t> &shape) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"),
                                                                             EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  es::EsTensorHolder data0_holder(data0);

  const auto abs0 = es::Abs(data0_holder);
  const auto add = es::Add(abs0, abs0);
  const auto reshape0 = es::Reshape(add, add, 3, 3);
  const auto relu = es::Relu(reshape0);

  std::vector<int64_t> dims_data{1, 1, 3, 3, -1};
  int64_t dims_size = 5;
  es::EsTensorHolder const_0(EsCreateConstInt64(graph.get(), dims_data.data(), &dims_size, 1));
  auto reshape1 = es::Reshape(relu, const_0, 0, -1);

  es::EsTensorHolder const_scalar_int32(EsCreateScalarInt32(graph.get(), 1));
  const auto reduceSum = es::ReduceSum(reshape1, const_scalar_int32, true, false);

  const auto abs2 = es::Abs(reduceSum);
  EsSetGraphOutput(abs2.GetCTensorHolder(), 0);

  const auto ge_graph =
      std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  GE_ASSERT_NOTNULL(ge_graph);
  return GraphUtilsEx::GetComputeGraph(*ge_graph);
}

/**
 * 包含不可推导的二类算子的图推导前置, 推导时到该节点时断图，输出子图
 * 输入8个节点的图，二类算子为 reshape，中断推导，输出4个节点的子图，不包括const节点
 *       data
 *         |
 *       Abs_0
 *        ||
 *       Add_1
 *        ||
 *     Reshape_2
 *        |
 *       Relu_3
 *        |   \
 *        |     Const4
 *        |    /    |
 *       Reshape_5  |
 *        |        /
 *        |   Const6
 *        |    /
 *       ReduceSum_7
 *        |
 *       Abs_8
 *        |
 *      netoutput
 */
TEST_F(JitInferUtilsUT, graphContainsConstNodeWithControlEdge2) {
  const auto graph = BuildAddReshapeReLuConstReshapeConstReduceSumAbsGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);

  auto reducesum_op_0 = graph->FindFirstNodeMatchType("ReduceSum");
  ASSERT_NE(reducesum_op_0, nullptr);
  auto reducesum_op_desc0 = reducesum_op_0->GetOpDesc();
  reducesum_op_desc0->MutableInputDesc(0)->SetDataType(DT_INT64);
  reducesum_op_desc0->MutableInputDesc(1)->SetDataType(DT_INT64);

  auto relu_node = graph->FindFirstNodeMatchType("Relu");
  ASSERT_NE(relu_node, nullptr);
  std::vector<NodePtr> const_nodes;
  for (const auto &node : graph->GetDirectNode()) {
    if (node->GetType() == "Const") {
      const_nodes.push_back(node);
    }
  }
  ASSERT_GE(const_nodes.size(), 2U) << "graph should have at least 2 Const nodes";
  NodePtr const4 = const_nodes[0];
  NodePtr const6 = const_nodes[1];
  ASSERT_EQ(GraphUtils::AddEdge(relu_node->GetOutControlAnchor(), const4->GetInControlAnchor()), ge::SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(const4->GetOutControlAnchor(), const6->GetInControlAnchor()), ge::SUCCESS);

  // Reshape shape输入dtype→INT64 + 所有有内核算子+下游无内核算子输出→{-2}
  for (auto &node : graph->GetDirectNode()) {
    auto t = node->GetType();
    if (t == "Reshape") {
      node->GetOpDesc()->MutableInputDesc(1)->SetDataType(DT_INT64);
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    } else if (t == "ReduceSum" || t == "Relu" || t == "Abs") {
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<NodePtr> nodes = getPreviousNodes(graph, 4);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);
}

// 验证 Phase A deadend 传播: 值依赖输入来自无kernel的非Const源节点 → deadend传播
// data → Xop_A(deadend, 无kernel) → Reshape(data=input, shape=Xop_A_out) → output
TEST_F(JitInferUtilsUT, value_dep_deadend_propagation) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1};
  EsSetShape(data, shape.data(), static_cast<int64_t>(shape.size()));

  auto xop = [](EsCTensorHolder *input, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType("XopA")
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("y", std::vector<int64_t>{-2})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input->GetProducer(), input->GetOutIndex(), node, 0);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };

  const auto xop_out = xop(data, "XopA_0");
  const auto reshape = EsReshape(xop_out, xop_out, 3, 3);
  EsSetGraphOutput(reshape, 0);

  const auto ge_graph_ptr = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph_ptr, nullptr);
  const auto cg = GraphUtilsEx::GetComputeGraph(*ge_graph_ptr);
  ASSERT_NE(cg, nullptr);

  for (auto &node : cg->GetDirectNode()) {
    if (node->GetType() == "Reshape") {
      node->GetOpDesc()->MutableInputDesc(1)->SetDataType(DT_INT64);
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5, 6}));
  td.SetOriginShape(GeShape({4, 5, 6}));
  inputs.emplace_back(td);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(cg, inputs, infered_nodes), SUCCESS);
  ASSERT_GE(infered_nodes.size(), 3);  // data + XopA(deadend) + Reshape(propagated)
}

// 验证 HasNoSymbolicCallback: Xop无kernel且全部输出{-2} → deadend传播
// data → Xop0(deadend) → Xop1(deadend) → Xop2(deadend) → output
TEST_F(JitInferUtilsUT, no_kernel_all_outputs_unknown) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1};
  EsSetShape(data, shape.data(), static_cast<int64_t>(shape.size()));

  auto xop = [](EsCTensorHolder *input, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType("XopB")
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("y", std::vector<int64_t>{-2})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input->GetProducer(), input->GetOutIndex(), node, 0);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };

  const auto op0 = xop(data, "XopB_0");
  const auto op1 = xop(op0, "XopB_1");
  const auto op2 = xop(op1, "XopB_2");
  EsSetGraphOutput(op2, 0);

  const auto ge_graph_ptr = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph_ptr, nullptr);
  const auto cg = GraphUtilsEx::GetComputeGraph(*ge_graph_ptr);
  ASSERT_NE(cg, nullptr);

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5}));
  td.SetOriginShape(GeShape({4, 5}));
  inputs.emplace_back(td);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(cg, inputs, infered_nodes), SUCCESS);
  ASSERT_GE(infered_nodes.size(), 4);  // data + 3 Xop(propagated)
}

// 验证 IsValueDepOnDeadendSource: 值依赖输入来自有kernel的源 → 不传播
// Reshape的shape输入源(Relu)无kernel → IsValueDepOnDeadendSource需准确判断
TEST_F(JitInferUtilsUT, value_dep_from_const_no_propagation) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1};
  EsSetShape(data, shape.data(), static_cast<int64_t>(shape.size()));

  const auto relu = EsRelu(data);
  const auto reshape = EsReshape(relu, relu, 3, 3);
  EsSetGraphOutput(reshape, 0);

  const auto ge_graph_ptr = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph_ptr, nullptr);
  const auto cg = GraphUtilsEx::GetComputeGraph(*ge_graph_ptr);
  ASSERT_NE(cg, nullptr);

  for (auto &node : cg->GetDirectNode()) {
    if (node->GetType() == "Reshape") {
      node->GetOpDesc()->MutableInputDesc(1)->SetDataType(DT_INT64);
    }
  }

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5, 6}));
  td.SetOriginShape(GeShape({4, 5, 6}));
  inputs.emplace_back(td);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(cg, inputs, infered_nodes), SUCCESS);
  ASSERT_GE(infered_nodes.size(), 2);
}

// 验证 no_lowering 拉入逻辑: 自定义算子无 lowering 注册 → 被拉入当前 EP
// data → CustomOp(no lowering, breakpoint) → output
// CustomOp 所有输入有符号但推导失败(breakpoint), 且无 lowering 注册 → 被 no_lowering 逻辑拉入
TEST_F(JitInferUtilsUT, no_lowering_breakpoint_pulled_in) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1};
  EsSetShape(data, shape.data(), static_cast<int64_t>(shape.size()));

  auto custom_op = [](EsCTensorHolder *input, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType("CustomNoLowering")
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("y", std::vector<int64_t>{-2})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input->GetProducer(), input->GetOutIndex(), node, 0);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };

  const auto custom_out = custom_op(data, "CustomNoLowering_0");
  EsSetGraphOutput(custom_out, 0);

  const auto ge_graph_ptr = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph_ptr, nullptr);
  const auto cg = GraphUtilsEx::GetComputeGraph(*ge_graph_ptr);
  ASSERT_NE(cg, nullptr);

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5, 6}));
  td.SetOriginShape(GeShape({4, 5, 6}));
  inputs.emplace_back(td);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(cg, inputs, infered_nodes), SUCCESS);
  ASSERT_GE(infered_nodes.size(), 2);  // data + CustomNoLowering(pulled by no_lowering)
}

// 验证 no_lowering 拉入逻辑 - 场景1: data -> reshape -> opx -> abs -> opx
// reshape 因为值依赖输入不满足成为断点(breakpoint, inferred)
// opx 有 symbolic kernel 但返回 UNSUPPORTED (is_deadend=false), 未注册 lowering (no_lowering=true)
// abs 有 lowering 注册, 不会被 no_lowering 拉入
// 预期: reshape(breakpoint) + opx1(no_lowering拉入) = 2个节点, abs 和 opx2 留在 uninferred
TEST_F(JitInferUtilsUT, no_lowering_cascade_blocked_by_lowering_op) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1};
  EsSetShape(data, shape.data(), static_cast<int64_t>(shape.size()));

  // 自定义算子 opx: 有 symbolic kernel 但返回 UNSUPPORTED (is_deadend=false), 未注册 lowering (no_lowering=true)
  auto opx = [](EsCTensorHolder *input, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType("CustomOp")
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("y", std::vector<int64_t>{-1})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input->GetProducer(), input->GetOutIndex(), node, 0);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };

  // data -> reshape -> opx1 -> abs -> opx2
  const auto relu = EsRelu(data);
  es::EsTensorHolder relu_holder(relu);
  const auto reshape = EsReshape(relu_holder.GetCTensorHolder(), relu_holder.GetCTensorHolder(), 3, 3);
  es::EsTensorHolder reshape_holder(reshape);
  const auto opx1 = opx(reshape_holder.GetCTensorHolder(), "CustomOp_1");
  es::EsTensorHolder opx1_holder(opx1);
  const auto abs_node = es::Abs(opx1_holder);
  es::EsTensorHolder abs_holder(abs_node.GetCTensorHolder());
  const auto opx2 = opx(abs_holder.GetCTensorHolder(), "CustomOp_2");
  EsSetGraphOutput(opx2, 0);

  const auto ge_graph_ptr = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph_ptr, nullptr);
  const auto cg = GraphUtilsEx::GetComputeGraph(*ge_graph_ptr);
  ASSERT_NE(cg, nullptr);

  // 设置 reshape 的 shape 输入为 INT64, 使其成为值依赖断点
  for (auto &node : cg->GetDirectNode()) {
    if (node->GetType() == "Reshape") {
      node->GetOpDesc()->MutableInputDesc(1)->SetDataType(DT_INT64);
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5, 6}));
  td.SetOriginShape(GeShape({4, 5, 6}));
  inputs.emplace_back(td);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(cg, inputs, infered_nodes), SUCCESS);
  // data + Relu(inferred) + Reshape(breakpoint) + CustomOp_1(no_lowering拉入) = 4
  // Abs 有 lowering → 不被拉入, CustomOp_2 的父节点 Abs 不在 inferred → 不被拉入
  ASSERT_EQ(infered_nodes.size(), 4);
}

// 验证 no_lowering 拉入逻辑 - 场景2: data -> reshape -> opx -> opx
// reshape 成为断点, 两个 opx 都是 no_lowering=true, is_deadend=false
// 预期: reshape(breakpoint) + opx1 + opx2 = 3个节点都被拉入
TEST_F(JitInferUtilsUT, no_lowering_cascade_consecutive) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1};
  EsSetShape(data, shape.data(), static_cast<int64_t>(shape.size()));

  // 自定义算子 opx: 有 symbolic kernel 但返回 UNSUPPORTED (is_deadend=false), 未注册 lowering (no_lowering=true)
  auto opx = [](EsCTensorHolder *input, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType("CustomOp")
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("y", std::vector<int64_t>{-1})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input->GetProducer(), input->GetOutIndex(), node, 0);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };

  // data -> reshape -> opx1 -> opx2
  const auto relu = EsRelu(data);
  es::EsTensorHolder relu_holder(relu);
  const auto reshape = EsReshape(relu_holder.GetCTensorHolder(), relu_holder.GetCTensorHolder(), 3, 3);
  es::EsTensorHolder reshape_holder(reshape);
  const auto opx1 = opx(reshape_holder.GetCTensorHolder(), "CustomOp_1");
  es::EsTensorHolder opx1_holder(opx1);
  const auto opx2 = opx(opx1_holder.GetCTensorHolder(), "CustomOp_2");
  EsSetGraphOutput(opx2, 0);

  const auto ge_graph_ptr = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph_ptr, nullptr);
  const auto cg = GraphUtilsEx::GetComputeGraph(*ge_graph_ptr);
  ASSERT_NE(cg, nullptr);

  // 设置 reshape 的 shape 输入为 INT64, 使其成为值依赖断点
  for (auto &node : cg->GetDirectNode()) {
    if (node->GetType() == "Reshape") {
      node->GetOpDesc()->MutableInputDesc(1)->SetDataType(DT_INT64);
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5, 6}));
  td.SetOriginShape(GeShape({4, 5, 6}));
  inputs.emplace_back(td);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(cg, inputs, infered_nodes), SUCCESS);
  // data + Relu(inferred) + Reshape(breakpoint) + CustomOp_1(no_lowering) + CustomOp_2(no_lowering级联) + NetOutput = 6
  ASSERT_EQ(infered_nodes.size(), 6);
}

// 验证 no_lowering 不拉入逻辑 - 场景3: opx 有两个输入，一个来自 reshape(breakpoint)，一个来自 data
// data0 → relu → reshape(breakpoint) → opx → output
// data1 → opx (第二个输入)
// opx 有 symbolic kernel 但返回 UNSUPPORTED (is_deadend=false), 未注册 lowering (no_lowering=true)
// reshape 是 breakpoint(inferred), opx 的父节点 reshape 在 inferred → opx 被 no_lowering 拉入
TEST_F(JitInferUtilsUT, no_lowering_pulled_when_parents_inferred) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1};
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));

  const auto data1 = EsCreateGraphInput(graph.get(), 1);
  std::vector<int64_t> shape2 = {-1, -1, -1};
  EsSetShape(data1, shape2.data(), static_cast<int64_t>(shape2.size()));

  // 自定义算子 opx: 有 symbolic kernel 但返回 UNSUPPORTED (is_deadend=false), 未注册 lowering (no_lowering=true), 有两个输入
  auto opx = [](EsCTensorHolder *input0, EsCTensorHolder *input1, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input0);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType("CustomOp")
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""},
                        {"y", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"z", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("z", std::vector<int64_t>{-1})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input0->GetProducer(), input0->GetOutIndex(), node, 0);
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input1->GetProducer(), input1->GetOutIndex(), node, 1);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };

  // data0 → relu → reshape(breakpoint) → opx → output
  // data1 → opx (第二个输入)
  es::EsTensorHolder relu_holder(EsRelu(data0));
  const auto reshape = EsReshape(relu_holder.GetCTensorHolder(), relu_holder.GetCTensorHolder(), 3, 3);
  es::EsTensorHolder reshape_holder(reshape);
  es::EsTensorHolder data1_holder(data1);
  const auto opx_node = opx(reshape_holder.GetCTensorHolder(), data1_holder.GetCTensorHolder(), "CustomOp_0");
  EsSetGraphOutput(opx_node, 0);

  const auto ge_graph_ptr = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph_ptr, nullptr);
  const auto cg = GraphUtilsEx::GetComputeGraph(*ge_graph_ptr);
  ASSERT_NE(cg, nullptr);

  // 设置 reshape 的 shape 输入为 INT64, 使其成为值依赖断点
  for (auto &node : cg->GetDirectNode()) {
    if (node->GetType() == "Reshape") {
      node->GetOpDesc()->MutableInputDesc(1)->SetDataType(DT_INT64);
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5, 6}));
  td.SetOriginShape(GeShape({4, 5, 6}));
  inputs.emplace_back(td);
  inputs.emplace_back(td);

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(cg, inputs, infered_nodes), SUCCESS);
  // data0 + data1 + Relu(inferred) + Reshape(breakpoint) + CustomOp(no_lowering拉入) + NetOutput = 6
  // CustomOp 的父节点 Reshape 在 inferred → 被 no_lowering 拉入
  ASSERT_EQ(infered_nodes.size(), 6);
}
