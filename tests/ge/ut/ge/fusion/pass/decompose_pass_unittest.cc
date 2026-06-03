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
#include <gmock/gmock.h>
#include "common/share_graph.h"
#include "es_ge_test_ops.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_adapter.h"

#include "stub/gert_runtime_stub.h"

#include "ge/fusion/pass/decompose_pass.h"
#include "ge/fusion/pattern.h"
#include "ge/fusion/infer_shape_util.h"
#include "graph/passes/base_pass.h"
#include "graph/shape_refiner.h"

#include "common/topo_checker.h"
#include "register/custom_pass_context_impl.h"

namespace ge {
namespace fusion {
using namespace ge::es;
namespace {
// v1 infer funcs avoid the symbolic infer-datatype path (which needs an
// initialized space registry, unavailable in this UT context).
graphStatus StubInferFunc(Operator &op) {
  auto in = op.GetInputDesc(0);
  auto out = op.GetOutputDesc(0);
  out.SetShape(in.GetShape());
  out.SetDataType(in.GetDataType());
  (void)op.UpdateOutputDesc(static_cast<uint32_t>(0), out);
  return GRAPH_SUCCESS;
}
graphStatus NoOpInferFunc(Operator &) {
  return GRAPH_SUCCESS;
}
}  // namespace
class UtestDecomposePass : public testing::Test {
public:
  static void SetUpTestSuite() {
  }
  static void TearDownTestSuite() {}
};
/**
 * single node match
 *      data
 *        |
 *     transdata
 *        |
 *       out
 */
TEST_F(UtestDecomposePass, SingleNode_1Input_1Output) {
  // define pass
  class TransDataToReluPass : public DecomposePass {
  TransDataToReluPass(const std::vector<AscendString> &op_types): DecomposePass(op_types) {}
  protected:
    std::unique_ptr<Graph> Replacement(const GNode &matched_node) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  CustomPassContext context;
  context.SetPassName("TransDataToReluPass");
  TransDataToReluPass transdata_2_relu_pass({TRANSDATA});
  EXPECT_EQ(transdata_2_relu_pass.Run(target_graph, context), SUCCESS);

  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "DynamicRNNV3") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}, {CONSTANT}, {"Relu"}, {"Relu"}, {CONSTANT}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(1, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(2, {{"Relu"}}), "success");
    }
    if (node->GetName() == "x_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
    }
    if (node->GetName() == "y_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}}), "success");
    }
    if (node->GetType() == "Relu") {
      std::cout << node->GetName() << std::endl;
      // check origin dump op attr
      std::vector<std::string> origin_types;
      const bool has_origin_op_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types);
      EXPECT_TRUE(has_origin_op_attr);
      EXPECT_TRUE(origin_types.size() == 1);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);
      // check pass_name attr
      const std::string kPassName = "pass_name";
      std::vector<std::string> pass_names;
      const bool has_pass_name_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), kPassName, pass_names);
      EXPECT_TRUE(has_pass_name_attr);
      EXPECT_TRUE(pass_names.size() == 1);
      EXPECT_STREQ(pass_names[0].c_str(), "TransDataToReluPass");
    }
  }
  EXPECT_EQ(transdata_2_relu_pass.Run(target_graph, context), NOT_CHANGED);
}

TEST_F(UtestDecomposePass, NotMeetRequirement_NOT_CHANGE) {
  // define pass
  class TransDataToReluPass : public DecomposePass {
   public:
    TransDataToReluPass(const std::vector<AscendString> &op_type) : DecomposePass(op_type) {}
   protected:
    bool MeetRequirements(const GNode &matched_node) override {
      return false;
    }
    std::unique_ptr<Graph> Replacement(const GNode &matched_node) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  std::vector<GNode> target_nodes;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == TRANSDATA) {
      target_nodes.emplace_back(NodeAdapter::Node2GNode(node));
    }
  }
  CustomPassContext context;
  TransDataToReluPass transdata_2_relu_pass({TRANSDATA});
  EXPECT_EQ(transdata_2_relu_pass.Run(target_graph, context), NOT_CHANGED);
}
TEST_F(UtestDecomposePass, ReplacementInvalid_Failed) {
  // define pass
  class TransDataToReluPass : public DecomposePass {
    TransDataToReluPass(const std::vector<AscendString> &op_types): DecomposePass(op_types) {}
   protected:
    bool MeetRequirements(const GNode &matched_node) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const GNode &matched_node) override { // WRONG REPLACEMENT
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      esb_graph->SetGraphOutput(data1, 1);
      return replace_graph.BuildAndReset();
    }
  };

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  CustomPassContext context;
  TransDataToReluPass transdata_2_relu_pass({TRANSDATA});
  auto ret = transdata_2_relu_pass.Run(target_graph, context);
  EXPECT_NE(ret, NOT_CHANGED);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(UtestDecomposePass, InferShapeForReplacementGraph_BasicShapeDtypeInfer) {
  class TransDataInferShapePass : public DecomposePass {
   public:
    TransDataInferShapePass(const std::vector<AscendString> &op_types) : DecomposePass(op_types) {}
   protected:
    std::unique_ptr<Graph> Replacement(const GNode &matched_node) override {
      // The matched Add node has 2 inputs, so the replacement must declare 2
      // Data nodes (input counts must match the matched subgraph boundary).
      // The second input is unused here (Relu only consumes input 0).
      auto replacement = std::make_shared<ComputeGraph>("replacement");
      const GeTensorDesc desc(GeShape({-1, -1, -1}), FORMAT_ND, DT_FLOAT);

      auto data0_op = std::make_shared<OpDesc>("input_0", "Data");
      data0_op->AddInputDesc(desc);
      data0_op->AddOutputDesc(desc);
      AttrUtils::SetInt(data0_op, ATTR_NAME_INDEX, 0);
      data0_op->AddInferFunc(NoOpInferFunc);
      auto data0 = replacement->AddNode(data0_op);

      auto data1_op = std::make_shared<OpDesc>("input_1", "Data");
      data1_op->AddInputDesc(desc);
      data1_op->AddOutputDesc(desc);
      AttrUtils::SetInt(data1_op, ATTR_NAME_INDEX, 1);
      data1_op->AddInferFunc(NoOpInferFunc);
      (void)replacement->AddNode(data1_op);

      auto relu_op = std::make_shared<OpDesc>("relu", "Relu");
      relu_op->AddInputDesc(desc);
      relu_op->AddOutputDesc(desc);
      relu_op->AddInferFunc(StubInferFunc);
      auto relu = replacement->AddNode(relu_op);

      auto netoutput_op = std::make_shared<OpDesc>("netoutput", "NetOutput");
      netoutput_op->AddInputDesc(desc);
      netoutput_op->AddInferFunc(NoOpInferFunc);
      auto netoutput = replacement->AddNode(netoutput_op);

      (void)GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu->GetInDataAnchor(0));
      (void)GraphUtils::AddEdge(relu->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0));

      auto replacement_graph =
          std::make_unique<Graph>(GraphUtilsEx::CreateGraphFromComputeGraph(replacement));
      EXPECT_EQ(InferShapeUtil::InferShape(*replacement_graph, matched_node), SUCCESS);
      return replacement_graph;
    }
  };

  auto target_compute_graph = gert::ShareGraph::AicoreGraph();
  auto data1 = target_compute_graph->FindNode("data1");
  ASSERT_NE(data1, nullptr);
  GeTensorDesc data1_desc(GeShape({1, 2, 3}), FORMAT_ND, DT_FLOAT16);
  data1->GetOpDesc()->MutableOutputDesc(0)->SetShape(GeShape({1, 2, 3}));
  data1->GetOpDesc()->MutableOutputDesc(0)->SetDataType(DT_FLOAT16);
  data1->GetOpDesc()->MutableOutputDesc(0)->SetFormat(FORMAT_ND);

  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  CustomPassContext context;
  context.SetPassName("TransDataInferShapePass");
  TransDataInferShapePass pass({"Add"});
  EXPECT_EQ(pass.Run(target_graph, context), SUCCESS);

  NodePtr relu_node_after_replace = nullptr;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      relu_node_after_replace = node;
      break;
    }
  }
  ASSERT_NE(relu_node_after_replace, nullptr);

  auto relu_out_desc = relu_node_after_replace->GetOpDesc()->GetOutputDesc(0);
  auto relu_shape = relu_out_desc.GetShape();
  EXPECT_EQ(relu_shape.GetDimNum(), 3U);
  EXPECT_EQ(relu_shape.GetDim(0), 1);
  EXPECT_EQ(relu_shape.GetDim(1), 2);
  EXPECT_EQ(relu_shape.GetDim(2), 3);
  EXPECT_EQ(relu_out_desc.GetDataType(), DT_FLOAT16);
}
} // namespace fusion
} // namespace ge