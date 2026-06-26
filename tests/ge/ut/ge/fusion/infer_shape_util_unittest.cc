/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string>
#include <gtest/gtest.h>
#include "common/ge_inner_error_codes.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/node_adapter.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/ge_tensor.h"
#include "api/gelib/gelib.h"
#include "ge/ge_api.h"
#include "common/framework_types_internal.h"

#include "common/env_path.h"
#include "common/share_graph.h"
#include "ge/fusion/infer_shape_util.h"
#include "ge/fusion/subgraph_boundary.h"

namespace ge {
namespace fusion {
namespace {
graphStatus StubInferFunc(Operator &op) {
  auto in = op.GetInputDesc(0);
  auto out = op.GetOutputDesc(0);
  out.SetShape(in.GetShape());
  out.SetDataType(in.GetDataType());
  op.UpdateOutputDesc((int)0, out);
  return GRAPH_SUCCESS;
}

graphStatus NoOpInferFunc(Operator &) {
  return GRAPH_SUCCESS;
}

graphStatus VDepInferFunc(Operator &op) {
  const auto *const_data = OpDescUtils::GetInputConstData(op, 0U);
  if (const_data == nullptr) {
    return GRAPH_FAILED;
  }
  const auto *dims = reinterpret_cast<const int64_t *>(const_data->GetData().GetData());
  const size_t dim_count = const_data->GetData().GetSize() / sizeof(int64_t);
  auto out_desc = op.GetOutputDesc(0U);
  out_desc.SetShape(ge::Shape(std::vector<int64_t>(dims, dims + dim_count)));
  (void)op.UpdateOutputDesc((int)0, out_desc);
  return GRAPH_SUCCESS;
}

NodePtr AddDataNode(const ComputeGraphPtr &graph, const std::string &name, int64_t index, const GeTensorDesc &desc) {
  auto op = std::make_shared<OpDesc>(name, DATA);
  op->AddInputDesc(desc);
  op->AddOutputDesc(desc);
  AttrUtils::SetInt(op, ATTR_NAME_INDEX, index);
  op->AddInferFunc(NoOpInferFunc);
  return graph->AddNode(op);
}

NodePtr AddReluNode(const ComputeGraphPtr &graph, const std::string &name, const GeTensorDesc &desc) {
  auto op = std::make_shared<OpDesc>(name, "Relu");
  op->AddInputDesc(desc);
  op->AddOutputDesc(desc);
  op->AddInferFunc(StubInferFunc);
  return graph->AddNode(op);
}

struct DataReluChain {
  NodePtr data;
  NodePtr relu;
};

DataReluChain AddDataReluChain(const ComputeGraphPtr &graph, const std::string &suffix, int64_t index,
                               const GeTensorDesc &desc) {
  auto data = AddDataNode(graph, "data_r" + suffix, index, desc);
  auto relu = AddReluNode(graph, "relu_r" + suffix, desc);
  GraphUtils::AddEdge(data->GetOutDataAnchor(0), relu->GetInDataAnchor(0));
  return {data, relu};
}

NodePtr AddInt64ConstNode(const ComputeGraphPtr &graph, const std::string &name, const std::vector<int64_t> &vals) {
  GeTensorDesc desc(GeShape({static_cast<int64_t>(vals.size())}), FORMAT_ND, DT_INT64);
  auto op = std::make_shared<OpDesc>(name, CONSTANT);
  op->AddOutputDesc(desc);
  op->AddInferFunc(NoOpInferFunc);
  auto node = graph->AddNode(op);
  auto tensor =
      std::make_shared<GeTensor>(desc, reinterpret_cast<const uint8_t *>(vals.data()), vals.size() * sizeof(int64_t));
  (void)OpDescUtils::SetWeights(*node, {tensor});
  return node;
}

NodePtr AddVDepNode(const ComputeGraphPtr &graph, const std::string &name, const GeTensorDesc &in_desc,
                    const GeTensorDesc &out_desc) {
  auto op = std::make_shared<OpDesc>(name, "VDep");
  op->AddInputDesc(in_desc);
  op->AddOutputDesc(out_desc);
  op->AddInferFunc(VDepInferFunc);
  return graph->AddNode(op);
}

void AddBoundaryInput(SubgraphBoundary &boundary, int64_t boundary_idx, const NodePtr &consumer,
                      int32_t consumer_input_idx) {
  SubgraphInput si;
  (void)si.AddInput({NodeAdapter::Node2GNode(consumer), consumer_input_idx});
  (void)boundary.AddInput(boundary_idx, si);
}

void ExpectOutputShapeDtype(const NodePtr &node, const std::vector<int64_t> &expected_dims, DataType expected_dtype) {
  const auto out_desc = node->GetOpDesc()->GetOutputDesc(0);
  const auto shape = out_desc.GetShape();
  EXPECT_EQ(shape.GetDimNum(), expected_dims.size());
  for (size_t i = 0; i < expected_dims.size(); ++i) {
    EXPECT_EQ(shape.GetDim(i), expected_dims[i]);
  }
  EXPECT_EQ(out_desc.GetDataType(), expected_dtype);
}

ComputeGraphPtr BuildReplacementGraphWithRelu(int64_t data_index, const GeShape &data_shape, DataType data_dtype) {
  auto replacement_compute_graph = std::make_shared<ComputeGraph>("replacement");

  auto data_op = std::make_shared<OpDesc>("data_r", DATA);
  GeTensorDesc data_desc(data_shape, FORMAT_ND, data_dtype);
  data_op->AddInputDesc(data_desc);
  data_op->AddOutputDesc(data_desc);
  AttrUtils::SetInt(data_op, ATTR_NAME_INDEX, data_index);
  data_op->AddInferFunc(NoOpInferFunc);
  auto data_node = replacement_compute_graph->AddNode(data_op);

  auto relu_op = std::make_shared<OpDesc>("relu_r", "Relu");
  relu_op->AddInputDesc(GeTensorDesc(data_shape, FORMAT_ND, data_dtype));
  relu_op->AddOutputDesc(GeTensorDesc(data_shape, FORMAT_ND, data_dtype));
  relu_op->AddInferFunc(StubInferFunc);
  auto relu_node = replacement_compute_graph->AddNode(relu_op);

  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), relu_node->GetInDataAnchor(0));
  return replacement_compute_graph;
}
}  // namespace

class UtestInferShapeUtil : public testing::Test {
 public:
  static void SetUpTestSuite() {
    std::map<string, string> options;
    EXPECT_EQ(GELib::Initialize(options), SUCCESS);
    auto instance = GELib::GetInstance();
    EXPECT_NE(instance, nullptr);
  }
  static void TearDownTestSuite() {
    GELib::GetInstance()->Finalize();
  }
};

TEST_F(UtestInferShapeUtil, FromBoundary_BasicShapeDtypeInfer) {
  auto target_compute_graph = gert::ShareGraph::AicoreGraph();
  auto data1 = target_compute_graph->FindNode("data1");
  ASSERT_NE(data1, nullptr);
  GeTensorDesc data1_desc(GeShape({1, 2, 3}), FORMAT_ND, DT_FLOAT16);
  data1->GetOpDesc()->MutableOutputDesc(0)->SetShape(GeShape({1, 2, 3}));
  data1->GetOpDesc()->MutableOutputDesc(0)->SetDataType(DT_FLOAT16);
  data1->GetOpDesc()->MutableOutputDesc(0)->SetFormat(FORMAT_ND);

  auto add1 = target_compute_graph->FindNode("add1");
  ASSERT_NE(add1, nullptr);

  auto replacement_compute_graph = BuildReplacementGraphWithRelu(0, GeShape({-1}), DT_FLOAT);
  auto replacement_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(replacement_compute_graph);

  SubgraphBoundary boundary;
  SubgraphInput subgraph_input;
  ASSERT_EQ(subgraph_input.AddInput({NodeAdapter::Node2GNode(add1), 0}), SUCCESS);
  ASSERT_EQ(boundary.AddInput(0, subgraph_input), SUCCESS);

  dlog_setlevel(GE_MODULE_NAME_U16, 0, 1);
  EXPECT_EQ(InferShapeUtil::InferShape(*replacement_graph, boundary), SUCCESS);

  auto relu_r = replacement_compute_graph->FindNode("relu_r");
  ASSERT_NE(relu_r, nullptr);
  const auto output_shape = relu_r->GetOpDesc()->GetOutputDesc(0).GetShape();
  EXPECT_EQ(output_shape.GetDimNum(), 3U);
  EXPECT_EQ(output_shape.GetDim(0), 1);
  EXPECT_EQ(output_shape.GetDim(1), 2);
  EXPECT_EQ(output_shape.GetDim(2), 3);
  EXPECT_EQ(relu_r->GetOpDesc()->GetOutputDesc(0).GetDataType(), DT_FLOAT16);
}

TEST_F(UtestInferShapeUtil, FromBoundary_ConstProducerPropagatesValue) {
  auto target = std::make_shared<ComputeGraph>("target_const");
  GeTensorDesc const_desc(GeShape({2}), FORMAT_ND, DT_INT64);
  auto const_node = AddInt64ConstNode(target, "const1", {4L, 5L});

  auto consumer_op = std::make_shared<OpDesc>("consumer1", "Consumer");
  consumer_op->AddInputDesc(const_desc);
  consumer_op->AddOutputDesc(GeTensorDesc(GeShape({-1, -1}), FORMAT_ND, DT_FLOAT));
  auto consumer_node = target->AddNode(consumer_op);
  GraphUtils::AddEdge(const_node->GetOutDataAnchor(0), consumer_node->GetInDataAnchor(0));

  auto replacement = std::make_shared<ComputeGraph>("replacement_vdep");
  auto data_node = AddDataNode(replacement, "data_r", 0, const_desc);
  auto vdep_node = AddVDepNode(replacement, "vdep", const_desc, GeTensorDesc(GeShape({-1, -1}), FORMAT_ND, DT_FLOAT));
  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), vdep_node->GetInDataAnchor(0));

  SubgraphBoundary boundary;
  AddBoundaryInput(boundary, 0, consumer_node, 0);

  auto replacement_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(replacement);
  EXPECT_EQ(InferShapeUtil::InferShape(*replacement_graph, boundary), SUCCESS);

  ExpectOutputShapeDtype(vdep_node, {4, 5}, DT_FLOAT);
}

TEST_F(UtestInferShapeUtil, FromBoundary_NonConstProducerNoValueSet) {
  auto target_compute_graph = gert::ShareGraph::AicoreGraph();
  auto add1 = target_compute_graph->FindNode("add1");
  ASSERT_NE(add1, nullptr);

  auto replacement_compute_graph = BuildReplacementGraphWithRelu(0, GeShape({-1, -1, -1}), DT_FLOAT);
  auto replacement_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(replacement_compute_graph);

  SubgraphBoundary boundary;
  SubgraphInput subgraph_input;
  ASSERT_EQ(subgraph_input.AddInput({NodeAdapter::Node2GNode(add1), 0}), SUCCESS);
  ASSERT_EQ(boundary.AddInput(0, subgraph_input), SUCCESS);

  EXPECT_EQ(InferShapeUtil::InferShape(*replacement_graph, boundary), SUCCESS);

  auto relu_r = replacement_compute_graph->FindNode("relu_r");
  ASSERT_NE(relu_r, nullptr);
  ConstGeTensorPtr value_tensor;
  EXPECT_FALSE(AttrUtils::GetTensor(relu_r->GetOpDesc()->MutableInputDesc(0), ATTR_NAME_VALUE, value_tensor));
}

TEST_F(UtestInferShapeUtil, FromBoundary_BoundaryInputCountMismatch) {
  auto target_compute_graph = gert::ShareGraph::AicoreGraph();
  auto add1 = target_compute_graph->FindNode("add1");
  ASSERT_NE(add1, nullptr);

  auto replacement_compute_graph = std::make_shared<ComputeGraph>("replacement_mismatch");
  auto data_op0 = std::make_shared<OpDesc>("data_r0", DATA);
  GeTensorDesc desc0(GeShape({-1}), FORMAT_ND, DT_FLOAT);
  data_op0->AddInputDesc(desc0);
  data_op0->AddOutputDesc(desc0);
  AttrUtils::SetInt(data_op0, ATTR_NAME_INDEX, static_cast<int64_t>(0));
  auto data_node0 = replacement_compute_graph->AddNode(data_op0);

  auto data_op1 = std::make_shared<OpDesc>("data_r1", DATA);
  GeTensorDesc desc1(GeShape({-1}), FORMAT_ND, DT_FLOAT);
  data_op1->AddInputDesc(desc1);
  data_op1->AddOutputDesc(desc1);
  AttrUtils::SetInt(data_op1, ATTR_NAME_INDEX, static_cast<int64_t>(1));
  auto data_node1 = replacement_compute_graph->AddNode(data_op1);

  auto add_op = std::make_shared<OpDesc>("add_r", ADD);
  add_op->AddInputDesc(desc0);
  add_op->AddInputDesc(desc1);
  add_op->AddOutputDesc(desc0);
  auto add_r_node = replacement_compute_graph->AddNode(add_op);

  GraphUtils::AddEdge(data_node0->GetOutDataAnchor(0), add_r_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(data_node1->GetOutDataAnchor(0), add_r_node->GetInDataAnchor(1));
  auto replacement_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(replacement_compute_graph);

  SubgraphBoundary boundary;
  SubgraphInput subgraph_input;
  ASSERT_EQ(subgraph_input.AddInput({NodeAdapter::Node2GNode(add1), 0}), SUCCESS);
  ASSERT_EQ(boundary.AddInput(0, subgraph_input), SUCCESS);

  EXPECT_NE(InferShapeUtil::InferShape(*replacement_graph, boundary), SUCCESS);
}

TEST_F(UtestInferShapeUtil, FromBoundary_MultipleInputsDifferentProducers) {
  auto target = std::make_shared<ComputeGraph>("target_multi_diff");
  GeTensorDesc desc1(GeShape({2, 3}), FORMAT_ND, DT_FLOAT16);
  GeTensorDesc desc2(GeShape({4, 5}), FORMAT_ND, DT_FLOAT);
  auto data1_node = AddDataNode(target, "data1", 0, desc1);
  auto data2_node = AddDataNode(target, "data2", 1, desc2);

  auto add_op = std::make_shared<OpDesc>("add1", ADD);
  add_op->AddInputDesc(desc1);
  add_op->AddInputDesc(desc2);
  add_op->AddOutputDesc(desc1);
  add_op->AddInferFunc(StubInferFunc);
  auto add_node = target->AddNode(add_op);
  GraphUtils::AddEdge(data1_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(data2_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(1));

  auto replacement = std::make_shared<ComputeGraph>("replacement_multi_diff");
  GeTensorDesc wild_desc(GeShape({-1}), FORMAT_ND, DT_FLOAT);
  auto chain0 = AddDataReluChain(replacement, "0", 0, wild_desc);
  auto chain1 = AddDataReluChain(replacement, "1", 1, wild_desc);

  SubgraphBoundary boundary;
  AddBoundaryInput(boundary, 0, add_node, 0);
  AddBoundaryInput(boundary, 1, add_node, 1);

  auto replacement_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(replacement);
  EXPECT_EQ(InferShapeUtil::InferShape(*replacement_graph, boundary), SUCCESS);

  ExpectOutputShapeDtype(chain0.relu, {2, 3}, DT_FLOAT16);
  ExpectOutputShapeDtype(chain1.relu, {4, 5}, DT_FLOAT);
}

TEST_F(UtestInferShapeUtil, FromBoundary_MultipleInputsSameProducer) {
  auto target = std::make_shared<ComputeGraph>("target_same_producer");
  GeTensorDesc data_desc(GeShape({3, 4}), FORMAT_ND, DT_FLOAT);
  auto data_node = AddDataNode(target, "data1", 0, data_desc);

  auto add_op = std::make_shared<OpDesc>("add1", ADD);
  add_op->AddInputDesc(data_desc);
  add_op->AddInputDesc(data_desc);
  add_op->AddOutputDesc(data_desc);
  add_op->AddInferFunc(StubInferFunc);
  auto add_node = target->AddNode(add_op);
  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), add_node->GetInDataAnchor(1));

  auto replacement = std::make_shared<ComputeGraph>("replacement_same_producer");
  GeTensorDesc wild_desc(GeShape({-1}), FORMAT_ND, DT_FLOAT16);
  auto chain0 = AddDataReluChain(replacement, "0", 0, wild_desc);
  auto chain1 = AddDataReluChain(replacement, "1", 1, wild_desc);

  SubgraphBoundary boundary;
  AddBoundaryInput(boundary, 0, add_node, 0);
  AddBoundaryInput(boundary, 1, add_node, 1);

  auto replacement_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(replacement);
  EXPECT_EQ(InferShapeUtil::InferShape(*replacement_graph, boundary), SUCCESS);

  ExpectOutputShapeDtype(chain0.relu, {3, 4}, DT_FLOAT);
  ExpectOutputShapeDtype(chain1.relu, {3, 4}, DT_FLOAT);
}

TEST_F(UtestInferShapeUtil, FromBoundary_DynamicShapePropagated) {
  auto target_compute_graph = std::make_shared<ComputeGraph>("target_dynamic");
  GeTensorDesc data_desc(GeShape({-1, 4}), FORMAT_ND, DT_FLOAT);
  auto data_op = std::make_shared<OpDesc>("data1", DATA);
  data_op->AddInputDesc(data_desc);
  data_op->AddOutputDesc(data_desc);
  AttrUtils::SetInt(data_op, ATTR_NAME_INDEX, static_cast<int64_t>(0));
  data_op->AddInferFunc(NoOpInferFunc);
  auto data_node = target_compute_graph->AddNode(data_op);

  auto relu_op = std::make_shared<OpDesc>("relu1", "Relu");
  relu_op->AddInputDesc(data_desc);
  relu_op->AddOutputDesc(data_desc);
  relu_op->AddInferFunc(StubInferFunc);
  auto relu_node = target_compute_graph->AddNode(relu_op);
  ASSERT_EQ(GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), relu_node->GetInDataAnchor(0)), GRAPH_SUCCESS);

  auto replacement_compute_graph = BuildReplacementGraphWithRelu(0, GeShape({2, 3}), DT_FLOAT16);
  auto replacement_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(replacement_compute_graph);

  SubgraphBoundary boundary;
  SubgraphInput si;
  ASSERT_EQ(si.AddInput({NodeAdapter::Node2GNode(relu_node), 0}), SUCCESS);
  ASSERT_EQ(boundary.AddInput(0, si), SUCCESS);

  EXPECT_EQ(InferShapeUtil::InferShape(*replacement_graph, boundary), SUCCESS);

  auto relu_r = replacement_compute_graph->FindNode("relu_r");
  ASSERT_NE(relu_r, nullptr);
  const auto shape = relu_r->GetOpDesc()->GetOutputDesc(0).GetShape();
  EXPECT_EQ(shape.GetDimNum(), 2U);
  EXPECT_EQ(shape.GetDim(0), -1);
  EXPECT_EQ(shape.GetDim(1), 4);
  EXPECT_EQ(relu_r->GetOpDesc()->GetOutputDesc(0).GetDataType(), DT_FLOAT);
}

TEST_F(UtestInferShapeUtil, FromBoundary_MixedConstAndNonConstInputs) {
  auto target = std::make_shared<ComputeGraph>("target_mixed");
  GeTensorDesc const_desc(GeShape({2}), FORMAT_ND, DT_INT64);
  GeTensorDesc data_desc(GeShape({3, 4}), FORMAT_ND, DT_FLOAT);
  auto const_node = AddInt64ConstNode(target, "const1", {6L, 7L});
  auto data_node = AddDataNode(target, "data1", 0, data_desc);

  auto consumer_op = std::make_shared<OpDesc>("consumer1", "Consumer");
  consumer_op->AddInputDesc(const_desc);
  consumer_op->AddInputDesc(data_desc);
  consumer_op->AddOutputDesc(GeTensorDesc(GeShape({-1}), FORMAT_ND, DT_FLOAT));
  consumer_op->AddInferFunc(StubInferFunc);
  auto consumer_node = target->AddNode(consumer_op);
  GraphUtils::AddEdge(const_node->GetOutDataAnchor(0), consumer_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), consumer_node->GetInDataAnchor(1));

  auto replacement = std::make_shared<ComputeGraph>("replacement_mixed");
  auto dr0_node = AddDataNode(replacement, "data_r0", 0, const_desc);
  auto vdep_node = AddVDepNode(replacement, "vdep", const_desc, GeTensorDesc(GeShape({-1, -1}), FORMAT_ND, DT_FLOAT));
  GraphUtils::AddEdge(dr0_node->GetOutDataAnchor(0), vdep_node->GetInDataAnchor(0));

  GeTensorDesc wild_desc(GeShape({-1}), FORMAT_ND, DT_FLOAT);
  auto chain1 = AddDataReluChain(replacement, "1", 1, wild_desc);

  SubgraphBoundary boundary;
  AddBoundaryInput(boundary, 0, consumer_node, 0);
  AddBoundaryInput(boundary, 1, consumer_node, 1);

  auto replacement_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(replacement);
  EXPECT_EQ(InferShapeUtil::InferShape(*replacement_graph, boundary), SUCCESS);

  ExpectOutputShapeDtype(vdep_node, {6, 7}, DT_FLOAT);
  ExpectOutputShapeDtype(chain1.relu, {3, 4}, DT_FLOAT);

  ConstGeTensorPtr value_tensor;
  EXPECT_FALSE(AttrUtils::GetTensor(chain1.relu->GetOpDesc()->MutableInputDesc(0), ATTR_NAME_VALUE, value_tensor));
}
}  // namespace fusion
}  // namespace ge
