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
#include "graph/ge_tensor.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/graph_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/op_desc.h"
#include "graph/compute_graph.h"
#include "graph_optimizer/fusion_common/graph_pass_util.h"
#include "register/graph_optimizer/graph_fusion/fusion_quant_util.h"
#include "register/graph_optimizer/graph_fusion/fusion_quant_util_impl.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/utils/tensor_utils.h"

using namespace std;
using namespace ge;

namespace fe {

class FusionQuantUtilImplCovUT : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}

  static ComputeGraphPtr CreateSimpleGraphWithAnchors() {
    ComputeGraphPtr graph = std::make_shared<ComputeGraph>("quant_test");
    OpDescPtr data = std::make_shared<OpDesc>("data", "Data");
    OpDescPtr weight = std::make_shared<OpDesc>("weight", "Const");
    OpDescPtr mm = std::make_shared<OpDesc>("mm", "WeightQuantBatchMatmulV2");
    OpDescPtr y = std::make_shared<OpDesc>("y", "NetOutput");

    GeShape shape1({2, 4, 9, 16});
    GeTensorDesc tensor_desc1(shape1, FORMAT_NCHW, DT_FLOAT16);
    tensor_desc1.SetOriginFormat(FORMAT_NCHW);
    tensor_desc1.SetOriginDataType(DT_FLOAT16);
    tensor_desc1.SetOriginShape(shape1);

    GeTensorDesc tensor_desc2(shape1, FORMAT_NCHW, DT_INT8);
    tensor_desc2.SetOriginFormat(FORMAT_NCHW);
    tensor_desc2.SetOriginDataType(DT_INT8);
    tensor_desc2.SetOriginShape(shape1);

    GeShape shape2({1, 16});
    GeTensorDesc tensor_desc3(shape2, FORMAT_ND, DT_FLOAT);
    tensor_desc3.SetOriginFormat(FORMAT_ND);
    tensor_desc3.SetOriginDataType(DT_FLOAT);
    tensor_desc3.SetOriginShape(shape2);

    data->AddOutputDesc(tensor_desc1);
    weight->AddOutputDesc(tensor_desc2);

    mm->AddInputDesc(tensor_desc1);
    mm->AddInputDesc(tensor_desc2);
    mm->AddInputDesc(tensor_desc1);
    mm->AddInputDesc(tensor_desc3);
    mm->AddInputDesc(tensor_desc3);
    mm->AddOutputDesc(tensor_desc2);
    y->AddInputDesc(tensor_desc2);

    NodePtr data_node = graph->AddNode(data);
    NodePtr weight_node = graph->AddNode(weight);
    NodePtr mm_node = graph->AddNode(mm);
    NodePtr y_node = graph->AddNode(y);

    GraphUtils::AddEdge(data_node->GetOutDataAnchor(0), mm_node->GetInDataAnchor(0));
    GraphUtils::AddEdge(weight_node->GetOutDataAnchor(0), mm_node->GetInDataAnchor(1));
    GraphUtils::AddEdge(mm_node->GetOutDataAnchor(0), y_node->GetInDataAnchor(0));
    return graph;
  }
};

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_NullParam) {
  BiasOptimizeEdges param;
  param.quant_scale = nullptr;
  param.quant_offset = nullptr;
  param.cube_weight = nullptr;
  param.cube_bias = nullptr;
  param.deq_scale = nullptr;
  vector<NodePtr> fusion_nodes;
  EXPECT_EQ(QuantUtil::BiasOptimizeByEdge(param, fusion_nodes), FAILED);
}

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_WithQuantNode_NullParam) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  BiasOptimizeEdges param;
  param.cube_weight = nullptr;
  param.cube_bias = nullptr;
  vector<NodePtr> fusion_nodes;
  EXPECT_EQ(QuantUtil::BiasOptimizeByEdge(mm_node, param, fusion_nodes), FAILED);
}

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_WithQuantParam_NullParam) {
  QuantParam quant_param = {1.0F, 0.0F};
  BiasOptimizeEdges param;
  param.cube_weight = nullptr;
  param.cube_bias = nullptr;
  vector<NodePtr> fusion_nodes;
  EXPECT_EQ(QuantUtil::BiasOptimizeByEdge(quant_param, param, fusion_nodes), FAILED);
}

TEST_F(FusionQuantUtilImplCovUT, InsertFixpipeDequantScaleConvert_NullAnchor) {
  InDataAnchorPtr deq_scale = nullptr;
  vector<NodePtr> fusion_nodes;
  EXPECT_EQ(QuantUtil::InsertFixpipeDequantScaleConvert(deq_scale, fusion_nodes), FAILED);
}

TEST_F(FusionQuantUtilImplCovUT, InsertFixpipeDequantScaleConvert_TwoNullAnchors) {
  InDataAnchorPtr deq_scale = nullptr;
  InDataAnchorPtr quant_offset = nullptr;
  vector<NodePtr> fusion_nodes;
  EXPECT_NE(QuantUtil::InsertFixpipeDequantScaleConvert(deq_scale, quant_offset, fusion_nodes), SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, InsertQuantScaleConvert_NullAnchorsReturnsSuccess) {
  InDataAnchorPtr quant_scale = nullptr;
  InDataAnchorPtr quant_offset = nullptr;
  vector<NodePtr> fusion_nodes;
  EXPECT_EQ(QuantUtil::InsertQuantScaleConvert(quant_scale, quant_offset, fusion_nodes), SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, InsertRequantScaleConvert_NullAnchors) {
  InDataAnchorPtr req_scale = nullptr;
  InDataAnchorPtr quant_offset = nullptr;
  InDataAnchorPtr cube_bias = nullptr;
  vector<NodePtr> fusion_nodes;
  EXPECT_NE(QuantUtil::InsertRequantScaleConvert(req_scale, quant_offset, cube_bias, fusion_nodes), SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, InsertFixpipeDequantScaleConvert_WithValidAnchor) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  InDataAnchorPtr deq_scale = mm_node->GetInDataAnchor(0);
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::InsertFixpipeDequantScaleConvert(deq_scale, fusion_nodes);
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, InsertFixpipeDequantScaleConvert_WithTwoAnchorsFail) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  InDataAnchorPtr deq_scale = mm_node->GetInDataAnchor(0);
  InDataAnchorPtr quant_offset = mm_node->GetInDataAnchor(1);
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::InsertFixpipeDequantScaleConvert(deq_scale, quant_offset, fusion_nodes);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_WithValidButIncompleteParam) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  BiasOptimizeEdges param;
  param.quant_scale = mm_node->GetInDataAnchor(0);
  param.quant_offset = mm_node->GetInDataAnchor(1);
  param.cube_weight = mm_node->GetInDataAnchor(0);
  param.cube_bias = nullptr;
  param.deq_scale = nullptr;
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::BiasOptimizeByEdge(mm_node, param, fusion_nodes);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_WithQuantParamValidButIncomplete) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  QuantParam quant_param = {1.0F, 0.0F};
  BiasOptimizeEdges param;
  param.quant_scale = mm_node->GetInDataAnchor(0);
  param.quant_offset = mm_node->GetInDataAnchor(1);
  param.cube_weight = mm_node->GetInDataAnchor(0);
  param.cube_bias = nullptr;
  param.deq_scale = nullptr;
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::BiasOptimizeByEdge(quant_param, param, fusion_nodes);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_WithWeightMode2D_Fail) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  BiasOptimizeEdges param;
  param.quant_scale = mm_node->GetInDataAnchor(0);
  param.quant_offset = mm_node->GetInDataAnchor(1);
  param.cube_weight = mm_node->GetInDataAnchor(0);
  param.cube_bias = mm_node->GetInDataAnchor(1);
  param.deq_scale = nullptr;
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::BiasOptimizeByEdge(param, fusion_nodes);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_WithWeightMode5D) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  QuantParam quant_param = {1.0F, 0.0F};
  BiasOptimizeEdges param;
  param.quant_scale = mm_node->GetInDataAnchor(0);
  param.quant_offset = mm_node->GetInDataAnchor(1);
  param.cube_weight = mm_node->GetInDataAnchor(0);
  param.cube_bias = mm_node->GetInDataAnchor(1);
  param.deq_scale = nullptr;
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::BiasOptimizeByEdge(quant_param, param, fusion_nodes, WeightMode::WEIGHTWITH5D);
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_WithWeightMode2D_QuantParam) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  QuantParam quant_param = {1.0F, 0.0F};
  BiasOptimizeEdges param;
  param.quant_scale = mm_node->GetInDataAnchor(0);
  param.quant_offset = mm_node->GetInDataAnchor(1);
  param.cube_weight = mm_node->GetInDataAnchor(0);
  param.cube_bias = mm_node->GetInDataAnchor(1);
  param.deq_scale = nullptr;
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::BiasOptimizeByEdge(quant_param, param, fusion_nodes, WeightMode::WEIGHTWITH2D);
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, BiasOptimizeByEdge_WithQuantNodeAndValidParam) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  BiasOptimizeEdges param;
  param.quant_scale = mm_node->GetInDataAnchor(0);
  param.quant_offset = mm_node->GetInDataAnchor(1);
  param.cube_weight = mm_node->GetInDataAnchor(0);
  param.cube_bias = mm_node->GetInDataAnchor(1);
  param.deq_scale = nullptr;
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::BiasOptimizeByEdge(mm_node, param, fusion_nodes);
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, InsertQuantScaleConvert_WithValidAnchorsSuccess) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  InDataAnchorPtr quant_scale = mm_node->GetInDataAnchor(0);
  InDataAnchorPtr quant_offset = mm_node->GetInDataAnchor(1);
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::InsertQuantScaleConvert(quant_scale, quant_offset, fusion_nodes);
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, InsertRequantScaleConvert_WithValidButIncompleteAnchors) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  InDataAnchorPtr req_scale = mm_node->GetInDataAnchor(0);
  InDataAnchorPtr quant_offset = mm_node->GetInDataAnchor(1);
  InDataAnchorPtr cube_bias = mm_node->GetInDataAnchor(0);
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::InsertRequantScaleConvert(req_scale, quant_offset, cube_bias, fusion_nodes);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, InsertFixpipeDequantScaleConvert_WithDeqScaleOnly) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  InDataAnchorPtr deq_scale = mm_node->GetInDataAnchor(1);
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::InsertFixpipeDequantScaleConvert(deq_scale, fusion_nodes);
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(FusionQuantUtilImplCovUT, InsertFixpipeDequantScaleConvert_WithDeqScaleAndOffset_Fail) {
  ComputeGraphPtr graph = CreateSimpleGraphWithAnchors();
  NodePtr mm_node = graph->FindNode("mm");
  InDataAnchorPtr deq_scale = mm_node->GetInDataAnchor(1);
  InDataAnchorPtr quant_offset = mm_node->GetInDataAnchor(0);
  vector<NodePtr> fusion_nodes;
  auto ret = QuantUtil::InsertFixpipeDequantScaleConvert(deq_scale, quant_offset, fusion_nodes);
  EXPECT_NE(ret, SUCCESS);
}
}  // namespace fe
