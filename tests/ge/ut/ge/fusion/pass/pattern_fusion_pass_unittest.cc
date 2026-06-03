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
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_adapter.h"
#include "graph/utils/op_desc_utils.h"
#include "ge_graph_dsl/graph_dsl.h"

#include "stub/gert_runtime_stub.h"

#include "ge/fusion/pass/pattern_fusion_pass.h"
#include "ge/fusion/pattern.h"
#include "ge/fusion/infer_shape_util.h"

#include "common/topo_checker.h"
#include "register/custom_pass_context_impl.h"
#include "ge/ge_utils.h"
#include "graph/utils/connection_matrix_impl.h"

namespace ge {
namespace fusion {
using namespace ge::es;
namespace {
// Pattern: data -> transdata（多个用例共用的最小匹配图）
PatternUniqPtr BuildSingleTransDataPattern() {
  auto pattern_graph = ge::es::EsGraphBuilder("pattern");
  auto esb_graph = pattern_graph.GetCGraphBuilder();
  auto data = EsCreateGraphInput(esb_graph, 0);
  auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
  esb_graph->SetGraphOutput(transdata, 0);
  return std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
}

// Replacement: data -> relu，所有节点挂 input shape 透传到 output 的 stub infer func。
GraphUniqPtr BuildReluReplacementWithStubInfer() {
  auto replace_graph_builder = ge::es::EsGraphBuilder("replacement");
  auto esb_graph = replace_graph_builder.GetCGraphBuilder();
  auto data = EsCreateGraphInput(esb_graph, 0);
  auto relu = EsRelu(data);
  esb_graph->SetGraphOutput(relu, 0);
  auto replacement_graph = replace_graph_builder.BuildAndReset();
  const auto stub_infer_func = [](Operator &op) {
    auto out_desc = op.GetOutputDesc(0);
    out_desc.SetShape(op.GetInputDesc(0).GetShape());
    op.UpdateOutputDesc((int) 0, out_desc);
    return GRAPH_SUCCESS;
  };
  for (const auto &node : GraphUtilsEx::GetComputeGraph(*replacement_graph)->GetDirectNode()) {
    node->GetOpDesc()->AddInferFunc(stub_infer_func);
  }
  return replacement_graph;
}

// 校验融合后图的拓扑：DynamicRNNV3 / x_reshape / y_reshape 的连接关系，以及每个 Relu 的
// 数据 dump 属性；定位到 transdata_4 对应的 Relu 时再比对 input/output shape。
// 返回 true 表示找到了 transdata_4 对应的 Relu 节点。
bool VerifyReplacedTopologyAndShape(const ComputeGraphPtr &graph) {
  bool found_transdata_4 = false;
  for (const auto &node : graph->GetDirectNode()) {
    if (node->GetType() == "DynamicRNNV3") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}, {CONSTANT}, {"Relu"}, {"Relu"}, {CONSTANT}}),
                "success");
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(1, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(2, {{"Relu"}}), "success");
    } else if (node->GetName() == "x_reshape") {
      EXPECT_EQ(gert::NodeTopoChecker(node).StrictConnectTo(0, {{"Relu"}}), "success");
    } else if (node->GetName() == "y_reshape") {
      EXPECT_EQ(gert::NodeTopoChecker(node).StrictConnectFrom({{"Relu"}, {CONSTANT}}), "success");
    } else if (node->GetType() == "Relu") {
      std::vector<std::string> origin_types;
      EXPECT_TRUE(ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types));
      EXPECT_EQ(origin_types.size(), 1U);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);

      std::vector<std::string> origin_op_names;
      ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_NAMES, origin_op_names);
      if (!origin_op_names.empty() && origin_op_names[0] == "transdata_4") {
        found_transdata_4 = true;
        const std::vector<int64_t> expect_shape = {1, -1, 256};
        const auto in_shape = node->GetOpDesc()->GetInputDesc(0).GetShape();
        const auto out_shape = node->GetOpDesc()->GetOutputDesc(0).GetShape();
        EXPECT_EQ(in_shape.GetDimNum(), expect_shape.size());
        EXPECT_EQ(out_shape.GetDimNum(), expect_shape.size());
        for (size_t i = 0u; i < expect_shape.size(); ++i) {
          EXPECT_EQ(in_shape.GetDim(i), expect_shape[i]);
          EXPECT_EQ(out_shape.GetDim(i), expect_shape[i]);
        }
      }
    }
  }
  return found_transdata_4;
}
} // namespace

class UtestPatternFusionPass : public testing::Test {
 public:
  static void SetUpTestSuite() {}
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
TEST_F(UtestPatternFusionPass, SingleNode_1Input_1Output) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
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
  TransDataToReluPass transdata_2_relu_pass;
  CustomPassContext context;
  EXPECT_EQ(transdata_2_relu_pass.Run(target_graph, context), SUCCESS);

  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "DynamicRNNV3") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}, {CONSTANT}, {"Relu"}, {"Relu"}, {CONSTANT}}),
                "success");
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
      // check attr
      std::vector<std::string> origin_types;
      const bool has_origin_op_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types);
      EXPECT_TRUE(has_origin_op_attr);
      EXPECT_TRUE(origin_types.size() == 1);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);
    }
  }

  // check not changed
  EXPECT_EQ(transdata_2_relu_pass.Run(target_graph, context), NOT_CHANGED);
}

TEST_F(UtestPatternFusionPass, SingleNode_1Input_1Output_AfterInfershape) {
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      patterns.emplace_back(BuildSingleTransDataPattern());
      return patterns;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replacement_graph = BuildReluReplacementWithStubInfer();
      GE_ASSERT_SUCCESS(InferShapeUtil::InferShape(*replacement_graph, *match_result));
      return replacement_graph;
    }
  };

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  TransDataToReluPass transdata_2_relu_pass;
  CustomPassContext context;
  EXPECT_EQ(transdata_2_relu_pass.Run(target_graph, context), SUCCESS);

  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  EXPECT_TRUE(VerifyReplacedTopologyAndShape(target_compute_graph));
}

TEST_F(UtestPatternFusionPass, NotMeetRequirement) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return false;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
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
  TransDataToReluPass transdata_2_relu_pass;
  CustomPassContext context;
  EXPECT_EQ(transdata_2_relu_pass.Run(target_graph, context), NOT_CHANGED);
}

TEST_F(UtestPatternFusionPass, RepalceFailed) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      esb_graph->SetGraphOutput(data1, 0);
      return replace_graph.BuildAndReset();
    }
  };

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  TransDataToReluPass transdata_2_relu_pass;
  CustomPassContext context;
  EXPECT_NE(transdata_2_relu_pass.Run(target_graph, context), SUCCESS);
  EXPECT_NE(transdata_2_relu_pass.Run(target_graph, context), NOT_CHANGED);
}

/**
 * single node match
 *      data
 *        |
 *     transdata
 *        |
 *       out
 */
TEST_F(UtestPatternFusionPass, SingleNode_1Input_1Output_EnableIrAttrMatch_NotChange) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   public:
    TransDataToReluPass() : PatternFusionPass(PatternMatcherConfigBuilder().EnableIrAttrMatch().Build()) {}

   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
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
  TransDataToReluPass transdata_2_relu_pass;
  CustomPassContext context;
  EXPECT_EQ(transdata_2_relu_pass.Run(target_graph, context), NOT_CHANGED);
}

// target graph:                       pattern_graph:
//      data0                                data0
//        |                                    |
//       abs      data1                       abs
//      /    \c   /                            |
//    relu     relu1                          relu
//      \     /c  |                            |
//        abs1    |                           abs1
//            \   /                            |
//            add                           netoutput
//             |
//          netoutput
TEST_F(UtestPatternFusionPass, CycleMakerPass_NotChange) {
  // define target graph
  auto target_graph_builder = ge::es::EsGraphBuilder("target");
  auto esb_target_graph = target_graph_builder.GetCGraphBuilder();
  auto data0 = EsCreateGraphInput(esb_target_graph, 0);
  auto data1 = EsCreateGraphInput(esb_target_graph, 1);
  auto abs = EsAbs(data0);
  auto relu = EsRelu(abs);
  auto abs1 = EsAbs(relu);
  auto relu1 = EsRelu(data1);
  auto add = EsAdd(abs1, relu1);
  GraphUtils::AddEdge(NodeAdapter::GNode2Node(abs->GetProducer())->GetOutControlAnchor(), NodeAdapter::GNode2Node(relu1->GetProducer())->GetInControlAnchor());
  GraphUtils::AddEdge(NodeAdapter::GNode2Node(relu1->GetProducer())->GetOutControlAnchor(), NodeAdapter::GNode2Node(abs1->GetProducer())->GetInControlAnchor());
  esb_target_graph->SetGraphOutput(add, 0);
  auto target_graph = std::make_shared<Graph>(*target_graph_builder.BuildAndReset());

  class CycleMakerPass : public PatternFusionPass {
   public:
    //CycleMakerPass() : PatternFusionPass() {}
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto abs1 = EsAbs(EsRelu(EsAbs(data)));
      esb_graph->SetGraphOutput(abs1, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };

  CycleMakerPass cycle_maker_pass;
  CustomPassContext context;
  EXPECT_EQ(cycle_maker_pass.Run(target_graph, context), NOT_CHANGED);
}

TEST_F(UtestPatternFusionPass, CaptureTensors) {
  // define pass
  class TestPassForCaptureTensors : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      // build pattern graph
      std::vector<PatternUniqPtr> patterns;
      auto pattern_graph = ::ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto add_tensor = EsAdd(data, data1);
      esb_graph->SetGraphOutput(add_tensor, 0);
      auto graph = pattern_graph.BuildAndReset();
      auto pattern = std::make_unique<Pattern>(std::move(*graph));

      pattern->CaptureTensor({NodeAdapter::Node2GNode(NodeAdapter::GNode2Node(data->GetProducer())), 0})
          .CaptureTensor({NodeAdapter::Node2GNode(NodeAdapter::GNode2Node(data1->GetProducer())), 0});
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      NodeIo capture_io_0;
      NodeIo capture_io_1;
      EXPECT_EQ(match_result->GetCapturedTensor(0, capture_io_0), SUCCESS);
      EXPECT_EQ(match_result->GetCapturedTensor(1, capture_io_1), SUCCESS);
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ::ge::es::EsGraphBuilder("pattern");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      esb_graph->SetGraphOutput(data, 0);
      return replace_graph.BuildAndReset();
    }
  };

  auto target_compute_graph = gert::ShareGraph::AicoreGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  TestPassForCaptureTensors test_pass_for_capture_tensors;
  CustomPassContext context;
  test_pass_for_capture_tensors.Run(target_graph, context);
}

TEST_F(UtestPatternFusionPass, CaptureData) {
  // define pass
  class TestPassForCaptureTensors : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      // build pattern graph
      std::vector<PatternUniqPtr> patterns;
      auto pattern_graph = ::ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      std::vector<int64_t> x_reshape_const_data({-1, 1, 256});
      std::vector<int64_t> x_reshape_shape({3});
      auto shape_const =
          EsCreateConstInt64(esb_graph, x_reshape_const_data.data(), x_reshape_shape.data(), x_reshape_shape.size());
      auto reshape = EsReshape(data, shape_const, 0, 0);
      esb_graph->SetGraphOutput(reshape, 0);
      auto graph = pattern_graph.BuildAndReset();
      auto pattern = std::make_unique<Pattern>(std::move(*graph));

      pattern->CaptureTensor({NodeAdapter::Node2GNode(NodeAdapter::GNode2Node(shape_const->GetProducer())), 0});
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      NodeIo capture_io;
      EXPECT_EQ(match_result->GetCapturedTensor(0, capture_io), SUCCESS);
      auto capture_node = capture_io.node;
      TensorDesc tensor_desc;
      EXPECT_EQ(capture_node.GetOutputDesc(0, tensor_desc), GRAPH_SUCCESS);
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ::ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      esb_graph->SetGraphOutput(data, 0);
      return replace_graph.BuildAndReset();
    }
  };

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  TestPassForCaptureTensors test_pass_for_capture_tensors;
  CustomPassContext context;
  test_pass_for_capture_tensors.Run(target_graph, context);
}

TEST_F(UtestPatternFusionPass, GetPatternName) {
  // define pass
  class TestPassForCaptureTensors : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      // build pattern graph
      std::vector<PatternUniqPtr> patterns;
      auto pattern_graph = ::ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      std::vector<int64_t> x_reshape_const_data({-1, 1, 256});
      std::vector<int64_t> x_reshape_shape({3});
      auto shape_const =
          EsCreateConstInt64(esb_graph, x_reshape_const_data.data(), x_reshape_shape.data(), x_reshape_shape.size());
      auto reshape = EsReshape(data, shape_const, 0, 0);
      esb_graph->SetGraphOutput(reshape, 0);
      auto graph = pattern_graph.BuildAndReset();
      auto pattern = std::make_unique<Pattern>(std::move(*graph));

      pattern->CaptureTensor({NodeAdapter::Node2GNode(NodeAdapter::GNode2Node(shape_const->GetProducer())), 0});
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      auto pattern_name = match_result->GetPatternGraph().GetName();
      EXPECT_EQ(pattern_name, "pattern");
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ::ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      esb_graph->SetGraphOutput(data, 0);
      return replace_graph.BuildAndReset();
    }
  };

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  TestPassForCaptureTensors test_pass_for_capture_tensors;
  CustomPassContext context;
  test_pass_for_capture_tensors.Run(target_graph, context);
}

TEST_F(UtestPatternFusionPass, ReplaceOutput_AutoUpdateOutput) {
  // define pass
  class TestPassForReplaceOutput : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      // build pattern graph
      std::vector<PatternUniqPtr> patterns;
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data_1 = EsCreateGraphInput(esb_graph, 0);
      auto data_2 = EsCreateGraphInput(esb_graph, 1);
      auto add = EsAdd(data_1, data_2);
      esb_graph->SetGraphOutput(add, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));

      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      auto pattern_name = match_result->GetPatternGraph().GetName();
      EXPECT_EQ(pattern_name, "pattern");
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph_builder = ge::es::EsGraphBuilder("replace");
      auto replace_esb_graph = replace_graph_builder.GetCGraphBuilder();
      auto data_r_1 = EsCreateGraphInput(replace_esb_graph, 0);
      auto data_r_2 = EsCreateGraphInput(replace_esb_graph, 1);
      auto sub_r = EsSub(data_r_1, data_r_2);
      replace_esb_graph->SetGraphOutput(sub_r, 0);
      return replace_graph_builder.BuildAndReset();
    }
  };

  // build target graph
  DEF_GRAPH(g1) {
    CHAIN(NODE("data1", DATA)->NODE("relu1", RELU)->NODE("add", ADD));
    CHAIN(NODE("data2", DATA)->EDGE(0, 1)->NODE("add"));
  };

  auto target_compute_graph = ToComputeGraph(g1);
  auto net_output = target_compute_graph->FindFirstNodeMatchType(NETOUTPUT);
  ASSERT_EQ(net_output, nullptr);
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  auto compute_output_node = target_compute_graph->FindFirstNodeMatchType("Add");
  auto output_gnode = NodeAdapter::Node2GNode(compute_output_node);
  ASSERT_EQ(target_graph->SetOutputs({{output_gnode, {0}}}), SUCCESS);

  TestPassForReplaceOutput test_pass_for_capture_tensors;
  CustomPassContext context;
  test_pass_for_capture_tensors.Run(target_graph, context);

  net_output = target_compute_graph->FindFirstNodeMatchType(NETOUTPUT);
  ASSERT_NE(net_output, nullptr);
  ASSERT_EQ(net_output->GetInDataNodes().size(), 1U);
  ASSERT_EQ(net_output->GetInDataNodes().at(0)->GetType(), "Sub");
}

TEST_F(UtestPatternFusionPass, ReplceTarget_AutoUpdateTarget) {
  // define pass
  class TestPassForReplaceOutput : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      // build pattern graph
      std::vector<PatternUniqPtr> patterns;
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data_1 = EsCreateGraphInput(esb_graph, 0);
      auto data_2 = EsCreateGraphInput(esb_graph, 1);
      auto add = EsAdd(data_1, data_2);
      esb_graph->SetGraphOutput(add, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));

      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      auto pattern_name = match_result->GetPatternGraph().GetName();
      EXPECT_EQ(pattern_name, "pattern");
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph_builder = ge::es::EsGraphBuilder("replace");
      auto replace_esb_graph = replace_graph_builder.GetCGraphBuilder();
      auto data_r_1 = EsCreateGraphInput(replace_esb_graph, 0);
      auto data_r_2 = EsCreateGraphInput(replace_esb_graph, 1);
      auto sub_r = EsSub(data_r_1, data_r_2);
      replace_esb_graph->SetGraphOutput(sub_r, 0);
      return replace_graph_builder.BuildAndReset();
    }
  };

  // build target graph
  DEF_GRAPH(g1) {
    CHAIN(NODE("data1", DATA)->NODE("relu1", RELU)->NODE("add", ADD));
    CHAIN(NODE("data2", DATA)->EDGE(0, 1)->NODE("add"));
  };

  auto target_compute_graph = ToComputeGraph(g1);
  auto net_output = target_compute_graph->FindFirstNodeMatchType(NETOUTPUT);
  ASSERT_EQ(net_output, nullptr);
  auto compute_output_node = target_compute_graph->FindFirstNodeMatchType("Add");
  auto target_operator = OpDescUtils::CreateOperatorFromNode(compute_output_node);
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  target_graph->SetTargets({target_operator});
  net_output = target_compute_graph->FindFirstNodeMatchType(NETOUTPUT);
  ASSERT_NE(net_output, nullptr);

  TestPassForReplaceOutput test_pass_for_capture_tensors;
  CustomPassContext context;
  test_pass_for_capture_tensors.Run(target_graph, context);

  auto sub = target_compute_graph->FindFirstNodeMatchType(SUB);
  ASSERT_NE(sub, nullptr);
  ASSERT_EQ(sub->GetOutControlNodes().size(), 1U);
  ASSERT_EQ(sub->GetOutControlNodes().at(0)->GetOutControlNodes().at(0)->GetType(), NETOUTPUT);
}
// DEBUG 级别下的兜底判环（pattern_fusion_run.cc:87-88）：一次成功的融合使 is_changed=true，
// 而图里已存在前置 WillCauseCycleIfFuse 局部判环漏掉的环（这里人为在与融合无关的区域注入
// 一条反向控制边构造环），TopologicalSorting 失败，guard 命中 GELOGE 并返回 FAILED。
TEST_F(UtestPatternFusionPass, CycleGuard_DebugLevel_DetectsCyclicGraph) {
  // 打开 DEBUG 级别，使 IsLogEnable(GE_MODULE_NAME, DLOG_DEBUG) 为真。
  // 用 RAII 保证无论用例是否中途 ASSERT 失败都会复位，避免 DEBUG 级别泄漏到同一二进制内的
  // 其它用例（否则会误触它们的 cycle guard 造成连锁失败）。
  dlog_setlevel(GE_MODULE_NAME, DLOG_DEBUG, 1);
  struct DlogLevelGuard {
    ~DlogLevelGuard() { dlog_setlevel(GE_MODULE_NAME, DLOG_ERROR, 0); }
  } dlog_level_guard;

  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  // 在与 transdata 融合路径无关的两个节点之间人为构造一对互相指向的控制边，制造全图环。
  // WillCauseCycleIfFuse 只对融合节点集合做局部判环，不会发现这个环。
  NodePtr drnn = nullptr;
  NodePtr net_output = nullptr;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "DynamicRNNV3") {
      drnn = node;
    }
    if (node->GetType() == NETOUTPUT) {
      net_output = node;
    }
  }
  ASSERT_NE(drnn, nullptr);
  ASSERT_NE(net_output, nullptr);
  // net_output -> drnn 的反向控制边，与已有 drnn -> ... -> net_output 形成环。
  (void)GraphUtils::AddEdge(net_output->GetOutControlAnchor(), drnn->GetInControlAnchor());
  ASSERT_NE(target_compute_graph->TopologicalSorting(), GRAPH_SUCCESS);

  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  TransDataToReluPass transdata_2_relu_pass;
  CustomPassContext context;
  // 融合成功(is_changed) + DEBUG + 全图有环 => 命中 cycle guard，返回 FAILED。
  auto ret = transdata_2_relu_pass.Run(target_graph, context);
  EXPECT_EQ(ret, FAILED);
}
}  // namespace fusion
}  // namespace ge
