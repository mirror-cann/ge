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
#include "graph/optimize/symbolic/infer_symbolic_shape/op_impl_infer_symbol_shape.h"
#include "tests/framework/ge_runtime_stub/include/common/summary_checker.h"
#include "tests/framework/ge_runtime_stub/include/faker/space_registry_faker.h"
#include "jit_execution/utils/partitioner/binary_partitioner.h"
#include "graph/passes/graph_builder_utils.h"
#include "graph/utils/graph_dump_utils.h"
#include "graph/attribute_group/attr_group_symbolic_desc.h"
#include "graph/ge_local_context.h"
#include "stub/gert_runtime_stub.h"
#include <dlfcn.h>
#include <vector>
#include <stack>
using namespace std;
using namespace testing;
using namespace ge;

std::vector<NodePtr> getPreviousNodes(ComputeGraphPtr graph, size_t nodeNum);

// 运行时通过dlsym注册lowering，避免link时符号依赖问题
void RegisterLoweringForTest(const std::string &op_type) {
  using RegisterFn = void (*)(const std::string &, const std::function<uint32_t(const NodePtr &)> &);
  auto *fn = reinterpret_cast<RegisterFn>(dlsym(RTLD_DEFAULT, "_ZN2ge15LoweringManager8RegisterERKSsRKSt8functionIFjRKSt10shared_ptrINS_4NodeEEEE"));
  if (fn != nullptr) {
    fn(op_type, [](const NodePtr &) -> uint32_t { return 0U; });
  }
}

namespace {
graphStatus InferShape4XopWithSymbolInfer(gert::InferSymbolShapeContext *context) {
  auto input_shape = context->GetInputSymbolShape(0);
  GE_ASSERT_NOTNULL(input_shape);
  auto output_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(output_shape);
  output_shape->MutableDims() = input_shape->GetDims();
  return ge::GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(XopWithSymbolInfer).InferSymbolShape(InferShape4XopWithSymbolInfer);
}  // namespace

// 从图Data节点描述符构建输入 → 推理 → 切分，供后续EP循环使用
static Status InferAndPartition(const ComputeGraphPtr &graph, std::vector<NodePtr> &infered_nodes,
                                PartionResult &result) {
  std::vector<GeTensor> inputs;
  for (const auto &node : graph->GetInputNodes()) {
    inputs.emplace_back(node->GetOpDesc()->GetOutputDesc(0));
  }
  infered_nodes.clear();
  if (JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes) != SUCCESS) return GRAPH_FAILED;
  if (BinaryPartitioner::Partition(graph, infered_nodes, result) != SUCCESS) return GRAPH_FAILED;
  return result.sliced_graph != nullptr ? SUCCESS : GRAPH_FAILED;
}

// 推理 + 全量切分验证：Infer → 首轮切分 → 循环切分remaining
static void FullPartition(const ComputeGraphPtr &graph, const std::vector<GeTensor> &inputs,
                          const std::vector<int> &expected_sliced_sizes) {
  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_FALSE(infered_nodes.empty());

  PartionResult pr;
  ASSERT_EQ(BinaryPartitioner::Partition(graph, infered_nodes, pr), SUCCESS);
  ASSERT_NE(pr.sliced_graph, nullptr);
  EXPECT_EQ(pr.sliced_graph->GetAllNodes().size(), expected_sliced_sizes[0]);

  int ep = 1;
  auto cur = pr.remaining_graph;
  std::vector<NodePtr> nodes;
  for (; cur != nullptr; ep++) {
    ASSERT_LT(ep, static_cast<int>(expected_sliced_sizes.size())) << "more EPs than expected";
    PartionResult next;
    ASSERT_EQ(InferAndPartition(cur, nodes, next), SUCCESS);
    EXPECT_EQ(next.sliced_graph->GetAllNodes().size(), expected_sliced_sizes[ep]);
    cur = next.remaining_graph;
  }
  EXPECT_EQ(ep, static_cast<int>(expected_sliced_sizes.size())) << "EP count mismatch";
}

class JitFullPartitionUT : public testing::Test {
 protected:
  void SetUp() override {
    gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2();
  }
  void TearDown() override {
    gert_stub_.Clear();
  }
  gert::GertRuntimeStub gert_stub_;
};

ComputeGraphPtr BuildAddAbsReLuReshapeAbsGraph(const vector<int64_t> &shape) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  const auto abs0 = EsAbs(data0);
  const auto add = EsAdd(abs0, abs0);
  const auto abs = EsAbs(add);
  const auto relu = EsRelu(abs);
  const auto reshape = EsReshape(relu, relu, 3, 3);
  const auto abs1 = EsAbs(reshape);
  EsSetGraphOutput(abs1, 0);

  const auto ge_graph = std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  GE_ASSERT_NOTNULL(ge_graph);
  return GraphUtilsEx::GetComputeGraph(*ge_graph);
}

/**
 * 包含可推导的二类算子的图推导前置, 推导时到该节点断图
 * 输入8个节点的图，二类算子为reshape，断图在输出节点，输出7个节点的sliced图,输出3个节点的remaining图
 * 分别对sliced图与remaining图再做推导，保证推导结果符合预期
 *       data
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
 *       abs
 *        |
 *      netoutput
 *
 */
TEST_F(JitFullPartitionUT, return_partition_two_graph_can_infer_success_when_origin_graph_infered_not_full) {
  const auto graph = BuildAddAbsReLuReshapeAbsGraph({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);
  auto abs5 = graph->FindNode("Abs_5");
  abs5->GetOpDesc()->MutableInputDesc(0)->SetOriginShape(GeShape({-1,-1,-1}));
  abs5->GetOpDesc()->MutableInputDesc(0)->SetShape(GeShape({-1,-1,-1}));
  abs5->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
  auto reshape_node = graph->FindFirstNodeMatchType("Reshape");
  reshape_node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));

  std::vector<NodePtr> nodes = getPreviousNodes(graph, 6);
  
  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);  

  PartionResult partition_result;
  ASSERT_EQ(BinaryPartitioner::Partition(graph, infered_nodes, partition_result), SUCCESS);

  // check sliced graph
  EXPECT_EQ(partition_result.sliced_graph->GetAllNodes().size(), 7);
  // check remaining graph
  EXPECT_EQ(partition_result.remaining_graph->GetAllNodes().size(), 3);
  // check io
  EXPECT_EQ(partition_result.out_idx_2_in_idxs.size(), 1);

  // infer sliced_graph
  infered_nodes.clear();
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(partition_result.sliced_graph, inputs, infered_nodes), SUCCESS);
  for (size_t i = 0; i < nodes.size(); ++i) {
    ASSERT_EQ(infered_nodes.at(i)->GetName(), nodes.at(i)->GetName());
  }

  // infer remaining_graph
  infered_nodes.clear();
  std::vector<NodePtr> uninfered_nodes;
  for (auto it : partition_result.remaining_graph->GetAllNodes()) {
    uninfered_nodes.push_back(it);
  }
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(partition_result.remaining_graph, inputs, infered_nodes), SUCCESS);
  for (size_t i = 0; i < infered_nodes.size(); ++i) {
    ASSERT_EQ(infered_nodes.at(i)->GetName(), uninfered_nodes.at(i)->GetName());
  }
}

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
TEST_F(JitFullPartitionUT, return_partition_one_graph) {
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
  std::sort(nodes.begin(), nodes.end());

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  std::sort(infered_nodes.begin(), infered_nodes.end());
  ASSERT_EQ(infered_nodes, nodes);

  PartionResult partition_result;
  ASSERT_EQ(BinaryPartitioner::Partition(graph, infered_nodes, partition_result), SUCCESS);

  // check sliced graph
  EXPECT_EQ(partition_result.sliced_graph->GetAllNodes().size(), 8);
  // check remaining graph
  EXPECT_EQ(partition_result.remaining_graph, nullptr);

  // infer sliced_graph again
  infered_nodes.clear();
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(partition_result.sliced_graph, inputs, infered_nodes), SUCCESS);
  std::sort(infered_nodes.begin(), infered_nodes.end());
  ASSERT_EQ(infered_nodes, nodes);
  for (size_t i = 0; i < infered_nodes.size(); ++i) {
    ASSERT_EQ(infered_nodes.at(i)->GetName(), nodes.at(i)->GetName());
  }
}

/**
 * 包含可推导的二类算子的图推导前置, 推导时到该节点不断图，继续向后推导
 * 输入7个节点的图，二类算子为reshape，断图在输出节点，输出7个节点的子图
 *       data
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
ComputeGraphPtr BuildAddAbsReLuReshapeGraph2(const vector<int64_t> &shape) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  const auto abs0 = EsAbs(data0);
  const auto add = EsAdd(abs0, abs0);
  const auto abs = EsAbs(add);
  const auto relu = EsRelu(abs);
  const auto reshape = EsReshape(relu, data0, 3, 3);
  EsSetGraphOutput(reshape, 0);

  const auto ge_graph = std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  GE_ASSERT_NOTNULL(ge_graph);
  return GraphUtilsEx::GetComputeGraph(*ge_graph);
}
TEST_F(JitFullPartitionUT, return_partition_two_graph_conatins_uninferable_reshape) {
  const auto graph = BuildAddAbsReLuReshapeGraph2({-1, -1, -1});
  ASSERT_NE(graph, nullptr);
  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);
  auto output = graph->GetOrUpdateNetOutputNode();
  ASSERT_NE(output, nullptr);
  output->GetOpDesc()->MutableInputDesc(0)->SetOriginShape(GeShape({-1, -1, -1}));
  output->GetOpDesc()->MutableInputDesc(0)->SetShape(GeShape({-1, -1, -1}));

  std::vector<NodePtr> nodes = getPreviousNodes(graph, 7);
  std::sort(nodes.begin(), nodes.end());

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(graph, inputs, infered_nodes), SUCCESS);
  std::sort(infered_nodes.begin(), infered_nodes.end());
  ASSERT_EQ(infered_nodes, nodes);

  PartionResult partition_result;
  ASSERT_EQ(BinaryPartitioner::Partition(graph, infered_nodes, partition_result), SUCCESS);

  // check sliced graph
  EXPECT_EQ(partition_result.sliced_graph->GetAllNodes().size(), 7);
  // check remaining graph
  EXPECT_EQ(partition_result.remaining_graph, nullptr);

  // infer sliced_graph again
  infered_nodes.clear();
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(partition_result.sliced_graph, inputs, infered_nodes), SUCCESS);
  std::sort(infered_nodes.begin(), infered_nodes.end());
  for (size_t i = 0; i < infered_nodes.size(); ++i) {
    ASSERT_EQ(infered_nodes.at(i)->GetName(), nodes.at(i)->GetName());
  }
}

/**
 * 包含可推导的二类算子的图推导前置, 推导时到该节点不断图，继续向后推导
 * 输入8个节点的图，二类算子为reshape，且value参数是常量const，断图在输出节点，输出8个节点的sliced图,输出0个节点的remaining图
 * 对sliced图再做推导，保证推导结果符合预期
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
TEST_F(JitFullPartitionUT, return_partition_one_graph_can_infer_success_when_origin_graph_infered_full) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);
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

  PartionResult partition_result;
  ASSERT_EQ(BinaryPartitioner::Partition(computerGraph, infered_nodes, partition_result), SUCCESS);
  // check sliced graph
  EXPECT_EQ(partition_result.sliced_graph->GetAllNodes().size(), 8);
  // check remaining graph
  EXPECT_EQ(partition_result.remaining_graph, nullptr);
  // check io
  EXPECT_EQ(partition_result.out_idx_2_in_idxs.size(), 0);

  // infer sliced_graph
  infered_nodes.clear();
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(partition_result.sliced_graph, inputs, infered_nodes), SUCCESS);
  ASSERT_EQ(infered_nodes, nodes);
}

/**
 * 包含不可推导的二类算子的图推导前置, 推导时到该节点断图
 * 剩余节点存在继续推导的情况（手动构造abs输出有符号），inferednode与uninfernode存在成环情况
 * 输入8个节点的图，二类算子为reshape
 * 输出切图失败，因为存在成环
 *       data0
 *        |
 *       reLu
 *        ||
 *      reshape
 *        |
 *       reLu
 *        |
 *       abs   data1
 *          \   /
 *           add
 *            |
 *          netout
 */
TEST_F(JitFullPartitionUT, return_partition_two_graph_can_infer_success_when_origin_graph_uninfernode_output_has_symbol) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  const auto data1 = EsCreateGraphInput(graph.get(), 1);
  std::vector<int64_t> shape = {-1, -1, -1};
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  EsSetShape(data1, shape.data(), static_cast<int64_t>(shape.size()));
  const auto relu = EsRelu(data0);
  const auto reshape = EsReshape(relu, relu, 3, 3);
  const auto relu1 = EsRelu(reshape);
  const auto abs = EsAbs(relu1);
  const auto add = EsAdd(abs, data1);

  EsSetGraphOutput(add, 0);

  const auto ge_graph = std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph, nullptr);
  const auto computerGraph = GraphUtilsEx::GetComputeGraph(*ge_graph);
  ASSERT_NE(computerGraph, nullptr);

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 6})));
  inputs.emplace_back(td);
  inputs.emplace_back(td);
  auto abs5 = computerGraph->FindNode("Abs_3");
  abs5->GetOpDesc()->MutableInputDesc(0)->SetOriginShape(GeShape({-1,-1,-1}));
  abs5->GetOpDesc()->MutableInputDesc(0)->SetShape(GeShape({-1,-1,-1}));
  auto attr = abs5->GetOpDesc()->MutableOutputDesc(0)->GetOrCreateAttrsGroup<SymbolicDescAttr>(); // 模拟构造abs节点不依赖输入能继续推导
  ASSERT_NE(attr, nullptr);
  auto add3 = computerGraph->FindNode("Add_4");
  add3->GetOpDesc()->MutableInputDesc(1)->SetOriginShape(GeShape({-1,-1,-1}));
  add3->GetOpDesc()->MutableInputDesc(1)->SetShape(GeShape({-1,-1,-1}));
  
  
  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(computerGraph, inputs, infered_nodes), SUCCESS);

  PartionResult partition_result;
  ASSERT_EQ(BinaryPartitioner::Partition(computerGraph, infered_nodes, partition_result), SUCCESS);
}

/**
 * 包含不可推导的二类算子的图推导前置, 推导时到该节点断图，const节点断在上一张图中
 * 剩余节点仍然存在二类算子继续推导的情况，期望剩余节点能完成推导，const继承到下一张图的输入
 *       data0
 *        |
 *       reLu
 *        ||
 *      reshape    const
 *          \      /
 *          reshape
 *             |
 *            relu
 *             |
 *           netout
 */
TEST_F(JitFullPartitionUT, return_partition_second_graph_can_infer_success_when_origin_graph_second_reshape_has_const_input) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data0 = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1, -1, -1};
  EsSetShape(data0, shape.data(), static_cast<int64_t>(shape.size()));
  es::EsTensorHolder data0_holder(data0);
  const auto relu = es::Relu(data0_holder);
  const auto reshape = es::Reshape(relu, relu, 0, -1);
  std::vector<int64_t> dims_data{1, 1, 3, 3, -1};
  int64_t dims_size = 5;
  es::EsTensorHolder const_0(EsCreateConstInt64(graph.get(), dims_data.data(), &dims_size, 1));

  auto reshape1 = es::Reshape(reshape, const_0, 0, -1);
  const auto relu1 = es::Relu(reshape1);
  EsSetGraphOutput(relu1.GetCTensorHolder(), 0);

  const auto ge_graph = std::unique_ptr<Graph>(static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph, nullptr);
  const auto computerGraph = GraphUtilsEx::GetComputeGraph(*ge_graph);
  ASSERT_NE(computerGraph, nullptr);

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape((GeShape({4, 5, 5, 5, 6})));
  td.SetOriginShape((GeShape({4, 5, 5, 5, 6})));
  inputs.emplace_back(td);
  auto reshape2 = computerGraph->FindNode("Reshape_3");
  reshape2->GetOpDesc()->MutableInputDesc(0)->SetOriginShape(GeShape({-1,-1,-1,-1,-1}));
  reshape2->GetOpDesc()->MutableInputDesc(0)->SetShape(GeShape({-1,-1,-1,-1,-1}));
  reshape2->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
  auto reshape_op_desc0 = reshape2->GetOpDesc();
  reshape_op_desc0->MutableInputDesc(0)->SetDataType(DT_INT64);
  reshape_op_desc0->MutableInputDesc(1)->SetDataType(DT_INT64);
  for (auto &node : computerGraph->GetDirectNode()) {
    if (node->GetType() == "Reshape") {
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    } else if (node->GetType() == "Relu" && !node->GetInDataNodes().empty()
               && node->GetInDataNodes().at(0)->GetType() == "Reshape") {
      node->GetOpDesc()->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<NodePtr> infered_nodes;
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(computerGraph, inputs, infered_nodes), SUCCESS);

  PartionResult pr;
  ASSERT_EQ(BinaryPartitioner::Partition(computerGraph, infered_nodes, pr), SUCCESS);
  EXPECT_EQ(pr.sliced_graph->GetAllNodes().size(), 5);
  EXPECT_EQ(pr.remaining_graph->GetAllNodes().size(), 5);

  // remaining图用原始static inputs做Infer，避免InferAndPartition自动构造的{-1}shape导致Symbolize失败
  infered_nodes.clear();
  ASSERT_EQ(JitInferUtils::InferGraphAndGetInferredNodes(pr.remaining_graph, inputs, infered_nodes), SUCCESS);
  EXPECT_EQ(infered_nodes.size(), pr.remaining_graph->GetAllNodes().size());
}

/**
 * 级联reshape-transpose链，每个op的输出是下一个op的shape/perm输入，
 * 每个op的data输入来自独立的图输入。
 * 验证现状：这些节点输出均为unknown，且Reshape/Transpose存在lowering注册，
 * 会被PropagatePullable级联拉入当前EP，最终整图进入同一个EP。
 *
 * 拓扑:
 *        data0 data1          data2           data3           data4
 *         |    /                |              |               |
 *      reshape#1 (data=data0, shape=data1)     |               |
 *         ||                                   |               |
 *      transpose#1 (data=data2, perm=reshape1_out)             |
 *         ||                                                   |
 *      reshape#2   (data=data3, shape=transpose1_out)          |
 *         ||                                                   |
 *      transpose#2 (data=data4, perm=reshape2_out)             |
 *         |                                                    |
 *      netoutput
 *
 * 预期切图结果（现状）:
 *   EP 0: data0..data4 + reshape#1 + transpose#1 + reshape#2 + transpose#2 + netoutput
 */
TEST_F(JitFullPartitionUT, consecutive_uninferable_reshape_transpose_chain_pulled_into_one_ep) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  // data0: reshape#1的数据输入 (4-D, FLOAT)
  const auto data0 = EsCreateGraphInputWithDetails(graph.get(), 0, "data0", nullptr, C_DT_FLOAT, C_FORMAT_ND, nullptr, 0);
  std::vector<int64_t> data_shape = {-1, -1, -1, -1};
  EsSetShape(data0, data_shape.data(), static_cast<int64_t>(data_shape.size()));

  // data1: reshape#1的shape输入 (非Const, 1-D, INT64)
  const auto data1 = EsCreateGraphInputWithDetails(graph.get(), 1, "data1", nullptr, C_DT_INT64, C_FORMAT_ND, nullptr, 0);
  std::vector<int64_t> param_dims = {4};
  EsSetShape(data1, param_dims.data(), static_cast<int64_t>(param_dims.size()));

  // data2: transpose#1的数据输入
  const auto data2 = EsCreateGraphInputWithDetails(graph.get(), 2, "data2", nullptr, C_DT_FLOAT, C_FORMAT_ND, nullptr, 0);
  EsSetShape(data2, data_shape.data(), static_cast<int64_t>(data_shape.size()));

  // data3: reshape#2的数据输入
  const auto data3 = EsCreateGraphInputWithDetails(graph.get(), 3, "data3", nullptr, C_DT_FLOAT, C_FORMAT_ND, nullptr, 0);
  EsSetShape(data3, data_shape.data(), static_cast<int64_t>(data_shape.size()));

  // data4: transpose#2的数据输入
  const auto data4 = EsCreateGraphInputWithDetails(graph.get(), 4, "data4", nullptr, C_DT_FLOAT, C_FORMAT_ND, nullptr, 0);
  EsSetShape(data4, data_shape.data(), static_cast<int64_t>(data_shape.size()));

  // 构建级联链: 每个op的output是下一个op的shape/perm输入
  // reshape#1(data=data0, shape=data1)
  const auto reshape1 = EsReshape(data0, data1, 4, 4);
  // transpose#1(data=data2, perm=reshape1)
  const auto transpose1 = EsTranspose(data2, reshape1);
  // reshape#2(data=data3, shape=transpose1)
  const auto reshape2 = EsReshape(data3, transpose1, 4, 4);
  // transpose#2(data=data4, perm=reshape2)
  const auto transpose2 = EsTranspose(data4, reshape2);
  EsSetGraphOutput(transpose2, 0);

  const auto ge_graph = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph, nullptr);
  const auto compute_graph = GraphUtilsEx::GetComputeGraph(*ge_graph);
  ASSERT_NE(compute_graph, nullptr);

  // 修正dtype+设置输出OriginShape→{-2}，确保三条推理路径均返回UNSUPPORTED
  for (auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc->GetType() == "Reshape") {
      op_desc->MutableInputDesc(0)->SetDataType(DT_FLOAT);
      op_desc->MutableInputDesc(1)->SetDataType(DT_INT64);
      op_desc->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    } else if (op_desc->GetType() == "Transpose") {
      op_desc->MutableInputDesc(0)->SetDataType(DT_FLOAT);
      op_desc->MutableInputDesc(1)->SetDataType(DT_INT64);
      op_desc->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  // 构造图输入 (5个Data节点)
  std::vector<GeTensor> inputs;
  GeTensorDesc data_td;
  data_td.SetShape(GeShape({4, 5, 6, 7}));
  data_td.SetOriginShape(GeShape({4, 5, 6, 7}));
  inputs.emplace_back(data_td);  // data0
  GeTensorDesc param_td;
  param_td.SetShape(GeShape({4}));
  param_td.SetOriginShape(GeShape({4}));
  inputs.emplace_back(param_td);  // data1 (shape输入)
  inputs.emplace_back(data_td);   // data2
  inputs.emplace_back(data_td);   // data3
  inputs.emplace_back(data_td);   // data4

  gert_stub_.GetSlogStub().SetLevelInfo();
  FullPartition(compute_graph, inputs, {10});
  gert_stub_.GetSlogStub().SetLevel(DLOG_ERROR);
  // 验证分类：Transpose_1/Reshape_2/Transpose_3 共3个节点，值依赖无法满足 → unsuppliable_input
  auto &log1 = gert_stub_.GetSlogStub();
  EXPECT_EQ(log1.CountLog(DLOG_INFO, "reason: value dependency unsuppliable"), 3);
  EXPECT_EQ(log1.CountLog(DLOG_INFO, "reason: no callback registered"), 0);
  EXPECT_EQ(log1.CountLog(DLOG_INFO, "reason: no lowering registered"), 0);
}

// 验证无符号化推导回调(但lowering已注册)的算子链 data→XopCallback×4→output：
// 注册lowering跳过no_lowering→命中no_callback→级联拉入同一个EP
TEST_F(JitFullPartitionUT, unregistered_symbol_infer_xop_chain_pulled_into_one_ep) {
  RegisterLoweringForTest("XopCallback");

  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1, -1};
  EsSetShape(data, shape.data(), static_cast<int64_t>(shape.size()));

  auto xop = [](EsCTensorHolder *input, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType("XopCallback")
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("y", std::vector<int64_t>{-2})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input->GetProducer(), input->GetOutIndex(), node, 0);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };

  const auto op0 = xop(data, "Xop_0");
  const auto op1 = xop(op0, "Xop_1");
  const auto op2 = xop(op1, "Xop_2");
  const auto op3 = xop(op2, "Xop_3");
  EsSetGraphOutput(op3, 0);

  const auto ge_graph = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph, nullptr);
  const auto compute_graph = GraphUtilsEx::GetComputeGraph(*ge_graph);
  ASSERT_NE(compute_graph, nullptr);

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5, 6, 7}));
  td.SetOriginShape(GeShape({4, 5, 6, 7}));
  inputs.emplace_back(td);

  gert_stub_.GetSlogStub().SetLevelInfo();
  FullPartition(compute_graph, inputs, {6});
  gert_stub_.GetSlogStub().SetLevel(DLOG_ERROR);
  // 验证分类：Xop_1~3 共3个节点，有lowering但无compute/infer → no_callback
  auto &log2 = gert_stub_.GetSlogStub();
  EXPECT_EQ(log2.CountLog(DLOG_INFO, "reason: no callback registered"), 3);
  EXPECT_EQ(log2.CountLog(DLOG_INFO, "reason: value dependency unsuppliable"), 0);
  EXPECT_EQ(log2.CountLog(DLOG_INFO, "reason: no lowering registered"), 0);
}

// 验证未注册lowering的自定义算子链 data→Xop×4→output：
// Xop无lowering→ClassifyPullable优先命中no_lowering→级联拉入同一个EP
TEST_F(JitFullPartitionUT, registered_symbol_infer_without_lowering_xop_chain_pulled_into_one_ep) {
  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);

  const auto data = EsCreateGraphInput(graph.get(), 0);
  std::vector<int64_t> shape = {-1, -1, -1, -1};
  EsSetShape(data, shape.data(), static_cast<int64_t>(shape.size()));

  auto xop = [](EsCTensorHolder *input, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType("Xop")
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("y", std::vector<int64_t>{-2})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input->GetProducer(), input->GetOutIndex(), node, 0);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };

  const auto op0 = xop(data, "Xop_0");
  const auto op1 = xop(op0, "Xop_1");
  const auto op2 = xop(op1, "Xop_2");
  const auto op3 = xop(op2, "Xop_3");
  EsSetGraphOutput(op3, 0);

  const auto ge_graph = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph, nullptr);
  const auto compute_graph = GraphUtilsEx::GetComputeGraph(*ge_graph);
  ASSERT_NE(compute_graph, nullptr);

  std::vector<GeTensor> inputs;
  GeTensorDesc td;
  td.SetShape(GeShape({4, 5, 6, 7}));
  td.SetOriginShape(GeShape({4, 5, 6, 7}));
  inputs.emplace_back(td);

  gert_stub_.GetSlogStub().SetLevelInfo();
  FullPartition(compute_graph, inputs, {6});
  gert_stub_.GetSlogStub().SetLevel(DLOG_ERROR);
  // 验证分类：Xop_1~3 共3个节点，无lowering → no_lowering
  auto &log3 = gert_stub_.GetSlogStub();
  EXPECT_EQ(log3.CountLog(DLOG_INFO, "reason: no lowering registered"), 3);
  EXPECT_EQ(log3.CountLog(DLOG_INFO, "reason: no callback registered"), 0);
  EXPECT_EQ(log3.CountLog(DLOG_INFO, "reason: value dependency unsuppliable"), 0);
}

TEST_F(JitFullPartitionUT, pullable_nodes_stop_before_subgraph_control_node) {
  RegisterLoweringForTest("XopNoCallbackMixed");
  RegisterLoweringForTest(IF);

  auto graph = std::unique_ptr<EsCGraphBuilder, void (*)(EsCGraphBuilder *)>(
      EsCreateGraphBuilder("graph"), EsDestroyGraphBuilder);
  const auto data = EsCreateGraphInputWithDetails(graph.get(), 0, "data", nullptr, C_DT_FLOAT, C_FORMAT_ND, nullptr, 0);
  std::vector<int64_t> data_shape = {-1, -1};
  EsSetShape(data, data_shape.data(), static_cast<int64_t>(data_shape.size()));
  const auto shape_data = EsCreateGraphInputWithDetails(
      graph.get(), 1, "shape_data", nullptr, C_DT_INT64, C_FORMAT_ND, nullptr, 0);
  std::vector<int64_t> shape_input_shape = {2};
  EsSetShape(shape_data, shape_input_shape.data(), static_cast<int64_t>(shape_input_shape.size()));
  const auto data2 = EsCreateGraphInputWithDetails(graph.get(), 2, "data2", nullptr, C_DT_FLOAT, C_FORMAT_ND, nullptr, 0);
  EsSetShape(data2, data_shape.data(), static_cast<int64_t>(data_shape.size()));
  const auto reshape = EsReshape(data, shape_data, 2, 2);
  const auto cond = EsRelu(data);
  es::EsTensorHolder reshape_holder(reshape);
  es::EsTensorHolder cond_holder(cond);

  auto build_xop = [](EsCTensorHolder *input, const char *type, const char *name) -> EsCTensorHolder* {
    auto *builder = ge::es::ResolveBuilder(input);
    auto *ge_graph = builder->GetGraph();
    auto node = ge::es::CompliantNodeBuilder(ge_graph)
        .OpType(type)
        .Name(name)
        .IrDefInputsV2({{"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
        .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
        .InstanceOutputOriginShape("y", std::vector<int64_t>{-2})
        .Build();
    ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, input->GetProducer(), input->GetOutIndex(), node, 0);
    return builder->GetTensorHolderFromNode(std::move(node), 0);
  };
  const auto xop_no_callback = build_xop(reshape_holder.GetCTensorHolder(), "XopNoCallbackMixed", "xop_no_callback");
  es::EsTensorHolder xop_holder(xop_no_callback);
  const auto xop_no_lowering = build_xop(xop_holder.GetCTensorHolder(), "XopNoLoweringMixed", "xop_no_lowering");
  es::EsTensorHolder no_lowering_holder(xop_no_lowering);
  const auto transpose = EsTranspose(data2, no_lowering_holder.GetCTensorHolder());
  es::EsTensorHolder transpose_holder(transpose);

  auto *builder = ge::es::ResolveBuilder(transpose_holder.GetCTensorHolder());
  auto *ge_graph = builder->GetGraph();
  auto if_builder_node = ge::es::CompliantNodeBuilder(ge_graph)
      .OpType(IF)
      .Name("if_op")
      .IrDefInputsV2({{"cond", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""},
                      {"x", ge::es::CompliantNodeBuilder::kEsIrInputRequired, ""}})
      .IrDefOutputsV2({{"y", ge::es::CompliantNodeBuilder::kEsIrOutputRequired, ""}})
      .InstanceOutputOriginShape("y", std::vector<int64_t>{-2})
      .Build();
  ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, cond_holder.GetCTensorHolder()->GetProducer(),
                                   cond_holder.GetCTensorHolder()->GetOutIndex(), if_builder_node, 0);
  ge::es::AddEdgeAndUpdatePeerDesc(*ge_graph, transpose_holder.GetCTensorHolder()->GetProducer(),
                                   transpose_holder.GetCTensorHolder()->GetOutIndex(), if_builder_node, 1);
  const auto if_output = builder->GetTensorHolderFromNode(std::move(if_builder_node), 0);
  EsSetGraphOutput(if_output, 0);

  const auto ge_graph_ptr = std::unique_ptr<Graph>(
      static_cast<Graph *>(static_cast<void *>(EsBuildGraphAndReset(graph.get()))));
  ASSERT_NE(ge_graph_ptr, nullptr);
  const auto compute_graph = GraphUtilsEx::GetComputeGraph(*ge_graph_ptr);
  ASSERT_NE(compute_graph, nullptr);

  auto then_graph = ComGraphMakeShared<ComputeGraph>("then_branch");
  auto else_graph = ComGraphMakeShared<ComputeGraph>("else_branch");
  ASSERT_NE(then_graph, nullptr);
  ASSERT_NE(else_graph, nullptr);
  const auto if_node = compute_graph->FindNode("if_op");
  ASSERT_NE(if_node, nullptr);
  if_node->GetOpDesc()->AddSubgraphName(then_graph->GetName());
  if_node->GetOpDesc()->SetSubgraphInstanceName(0, then_graph->GetName());
  if_node->GetOpDesc()->AddSubgraphName(else_graph->GetName());
  if_node->GetOpDesc()->SetSubgraphInstanceName(1, else_graph->GetName());
  then_graph->SetParentNode(if_node);
  then_graph->SetParentGraph(compute_graph);
  else_graph->SetParentNode(if_node);
  else_graph->SetParentGraph(compute_graph);
  ASSERT_EQ(compute_graph->AddSubgraph(then_graph->GetName(), then_graph), GRAPH_SUCCESS);
  ASSERT_EQ(compute_graph->AddSubgraph(else_graph->GetName(), else_graph), GRAPH_SUCCESS);

  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc->GetType() == RESHAPE) {
      op_desc->MutableInputDesc(0)->SetDataType(DT_FLOAT);
      op_desc->MutableInputDesc(1)->SetDataType(DT_INT64);
      op_desc->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    } else if (op_desc->GetType() == "Transpose") {
      op_desc->MutableInputDesc(0)->SetDataType(DT_FLOAT);
      op_desc->MutableInputDesc(1)->SetDataType(DT_INT64);
      op_desc->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    } else if (op_desc->GetType() == "XopNoCallbackMixed" || op_desc->GetType() == "XopNoLoweringMixed" ||
               op_desc->GetType() == IF) {
      op_desc->MutableOutputDesc(0)->SetOriginShape(GeShape({-2}));
    }
  }

  std::vector<GeTensor> inputs;
  GeTensorDesc data_td;
  data_td.SetShape(GeShape({4, 5}));
  data_td.SetOriginShape(GeShape({4, 5}));
  inputs.emplace_back(data_td);
  GeTensorDesc shape_td;
  shape_td.SetShape(GeShape({2}));
  shape_td.SetOriginShape(GeShape({2}));
  inputs.emplace_back(shape_td);
  inputs.emplace_back(data_td);

  gert_stub_.GetSlogStub().SetLevelInfo();
  FullPartition(compute_graph, inputs, {9, 4});
  gert_stub_.GetSlogStub().SetLevel(DLOG_ERROR);
  auto &log = gert_stub_.GetSlogStub();
  EXPECT_EQ(log.CountLog(DLOG_INFO, "reason: value dependency unsuppliable"), 1);
  EXPECT_EQ(log.CountLog(DLOG_INFO, "reason: no callback registered"), 1);
  EXPECT_EQ(log.CountLog(DLOG_INFO, "reason: no lowering registered"), 1);
  EXPECT_EQ(log.CountLog(DLOG_INFO, "[if_op][If] pulled into current EP"), 0);
}
