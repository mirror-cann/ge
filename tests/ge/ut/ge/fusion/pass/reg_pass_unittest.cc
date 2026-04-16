/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <new>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "common/share_graph.h"
#include "es_ge_test_ops.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_adapter.h"

#include "stub/gert_runtime_stub.h"
#include "ge/fusion/pattern.h"
#include "common/topo_checker.h"
#include "ge/fusion/pass/decompose_pass.h"
#include "ge/fusion/pass/pattern_fusion_pass.h"
#include "register/custom_pass_context_impl.h"
// 单例，为了保证ut效果，需要清理其成员
#define private public
#include "compiler/graph/fusion/pass/pass_registry.h"
#undef private

namespace ge {
namespace fusion {
using namespace ge::es;
namespace {
struct PythonCreateSnapshot {
  bool has_context{false};
  bool has_descriptor{false};
  PythonPassCreateContext create_context;
  PythonPassDescriptor descriptor;
};

PythonCreateSnapshot g_python_create_snapshot;

void ResetPythonCreateSnapshot() {
  g_python_create_snapshot = {};
}

class Stage1PythonFusionPass : public FusionBasePass {
 public:
  explicit Stage1PythonFusionPass(const PythonPassDescriptor &descriptor) : descriptor_(descriptor) {}

  const PythonPassDescriptor &GetDescriptor() const {
    return descriptor_;
  }

  Status Run(GraphPtr &, CustomPassContext &) override {
    return SUCCESS;
  }

 private:
  PythonPassDescriptor descriptor_;
};

FusionBasePass *CreateStage1PythonFusionPass() {
  ResetPythonCreateSnapshot();
  g_python_create_snapshot.has_context = GetCurrentPythonPassCreateContext(g_python_create_snapshot.create_context);
  g_python_create_snapshot.has_descriptor =
      PassRegistry::GetInstance().ResolveCurrentPythonPassDescriptor(g_python_create_snapshot.descriptor);
  if (!g_python_create_snapshot.has_descriptor) {
    return nullptr;
  }
  return new (std::nothrow) Stage1PythonFusionPass(g_python_create_snapshot.descriptor);
}
} // namespace

class UtestRegPass : public testing::Test {
 public:
  void SetUp() override {
    PassRegistry::GetInstance().name_2_fusion_pass_regs_.clear();
    PassRegistry::GetInstance().descriptor_key_2_python_pass_descs_.clear();
    PassRegistry::GetInstance().pass_name_2_python_pass_create_contexts_.clear();
    ClearCurrentPythonPassCreateContext();
    ResetPythonCreateSnapshot();
  }
  void TearDown() override {
    PassRegistry::GetInstance().name_2_fusion_pass_regs_.clear();
    PassRegistry::GetInstance().descriptor_key_2_python_pass_descs_.clear();
    PassRegistry::GetInstance().pass_name_2_python_pass_create_contexts_.clear();
    ClearCurrentPythonPassCreateContext();
    ResetPythonCreateSnapshot();
  }
};
/**
 * single node match
 *      data
 *        |
 *     transdata
 *        |
 *       out
 */
TEST_F(UtestRegPass, PatternFusionPassReg_Run) {
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
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  auto passes_before_infershape =
      PassRegistry::GetInstance().GetFusionPassRegDataByStage(CustomPassStage::kAfterInferShape);

  EXPECT_EQ(passes_before_infershape.size(), 1);
  FusionBasePass* transdata_2_relu_pass = passes_before_infershape[0].GetCreatePassFn()();

  CustomPassContext context;
  EXPECT_EQ(transdata_2_relu_pass->Run(target_graph, context), SUCCESS);

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
      // check attr
      std::vector<std::string> origin_types;
      const bool has_origin_op_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types);
      EXPECT_TRUE(has_origin_op_attr);
      EXPECT_TRUE(origin_types.size() == 1);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);
    }
  }
  delete transdata_2_relu_pass;
}

/**
 * single node match
 *      data
 *        |
 *     transdata
 *        |
 *       out
 */
TEST_F(UtestRegPass, DecomposePassReg_Run) {
  // define pass
  class DecomposeTransDataPass : public DecomposePass {
  DecomposeTransDataPass(const std::vector<AscendString> &op_types) : DecomposePass(op_types) {}
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

  REG_DECOMPOSE_PASS(DecomposeTransDataPass, {TRANSDATA}).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  auto passes_after_infershape =
      PassRegistry::GetInstance().GetFusionPassRegDataByStage(CustomPassStage::kAfterInferShape);

  EXPECT_EQ(passes_after_infershape.size(), 1);
  DecomposePass* transdata_2_relu_pass = dynamic_cast<DecomposePass *>(passes_after_infershape[0].GetCreatePassFn()());

  CustomPassContext context;
  EXPECT_EQ(transdata_2_relu_pass->Run(target_graph, context), SUCCESS);

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
      // check attr
      std::vector<std::string> origin_types;
      const bool has_origin_op_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types);
      EXPECT_TRUE(has_origin_op_attr);
      EXPECT_TRUE(origin_types.size() == 1);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);
    }
  }
  delete transdata_2_relu_pass;
}

TEST_F(UtestRegPass, FusionPass_Reg_InWrongStage) {
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
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterAssignLogicStream);


  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
  auto passes_before_infershape =
      PassRegistry::GetInstance().GetFusionPassRegDataByStage(CustomPassStage::kAfterInferShape);

  EXPECT_EQ(passes_before_infershape.size(), 0);
}

TEST_F(UtestRegPass, PythonPassCreateScope_RestorePreviousContext) {
  PythonPassCreateContext outer_context{"holder_outer", "OuterPass", PythonPassKind::kFusionBase};
  PythonPassCreateContext inner_context{"holder_inner", "InnerPass", PythonPassKind::kPatternFusion};
  PythonPassCreateContext current_context;

  EXPECT_FALSE(GetCurrentPythonPassCreateContext(current_context));
  {
    PythonPassCreateScope outer_scope(outer_context);
    ASSERT_TRUE(GetCurrentPythonPassCreateContext(current_context));
    EXPECT_EQ(current_context.descriptor_key, outer_context.descriptor_key);
    {
      PythonPassCreateScope inner_scope(inner_context);
      ASSERT_TRUE(GetCurrentPythonPassCreateContext(current_context));
      EXPECT_EQ(current_context.descriptor_key, inner_context.descriptor_key);
      EXPECT_EQ(current_context.pass_name, inner_context.pass_name);
      EXPECT_EQ(current_context.kind, inner_context.kind);
    }
    ASSERT_TRUE(GetCurrentPythonPassCreateContext(current_context));
    EXPECT_EQ(current_context.descriptor_key, outer_context.descriptor_key);
    EXPECT_EQ(current_context.pass_name, outer_context.pass_name);
    EXPECT_EQ(current_context.kind, outer_context.kind);
  }
  EXPECT_FALSE(GetCurrentPythonPassCreateContext(current_context));
}

TEST_F(UtestRegPass, PythonPassDescriptor_Reg_CreatePassWithTlsContext) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "stage1.python.descriptor";
  pass_desc.pass_name = "Stage1PythonPass";
  pass_desc.module_name = "stage1.sample.module";
  pass_desc.class_name = "Stage1PythonPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kFusionBase;

  ASSERT_TRUE(PassRegistry::GetInstance().RegisterPythonPass(pass_desc, CreateStage1PythonFusionPass));

  auto passes = PassRegistry::GetInstance().GetFusionPassRegDataByStage(CustomPassStage::kAfterInferShape);
  ASSERT_EQ(passes.size(), 1);
  PythonPassDescriptor stored_desc;
  ASSERT_TRUE(PassRegistry::GetInstance().GetPythonPassDescriptor(pass_desc.descriptor_key, stored_desc));
  EXPECT_EQ(stored_desc.pass_name, pass_desc.pass_name);
  EXPECT_EQ(stored_desc.module_name, pass_desc.module_name);
  EXPECT_EQ(stored_desc.class_name, pass_desc.class_name);

  auto *pass_from_raw_creator = passes[0].GetCreatePassFn()();
  EXPECT_EQ(pass_from_raw_creator, nullptr);
  EXPECT_FALSE(g_python_create_snapshot.has_context);
  delete pass_from_raw_creator;

  auto *created_pass = PassRegistry::GetInstance().CreatePass(passes[0]);
  ASSERT_NE(created_pass, nullptr);
  EXPECT_TRUE(g_python_create_snapshot.has_context);
  EXPECT_TRUE(g_python_create_snapshot.has_descriptor);
  EXPECT_EQ(g_python_create_snapshot.create_context.descriptor_key, pass_desc.descriptor_key);
  EXPECT_EQ(g_python_create_snapshot.create_context.pass_name, pass_desc.pass_name);
  EXPECT_EQ(g_python_create_snapshot.descriptor.module_name, pass_desc.module_name);
  EXPECT_EQ(g_python_create_snapshot.descriptor.class_name, pass_desc.class_name);

  auto *python_pass = dynamic_cast<Stage1PythonFusionPass *>(created_pass);
  ASSERT_NE(python_pass, nullptr);
  EXPECT_EQ(python_pass->GetDescriptor().descriptor_key, pass_desc.descriptor_key);
  EXPECT_EQ(python_pass->GetDescriptor().pass_name, pass_desc.pass_name);
  delete created_pass;
}

TEST_F(UtestRegPass, PythonPassDescriptor_Reg_DuplicateHolderKeyRejected) {
  PythonPassDescriptor first_pass_desc;
  first_pass_desc.descriptor_key = "stage1.python.dup";
  first_pass_desc.pass_name = "Stage1PythonPassA";
  first_pass_desc.module_name = "sample.module_a";
  first_pass_desc.class_name = "Stage1PythonPassA";
  first_pass_desc.stage = CustomPassStage::kAfterInferShape;

  PythonPassDescriptor second_pass_desc = first_pass_desc;
  second_pass_desc.pass_name = "Stage1PythonPassB";
  second_pass_desc.class_name = "Stage1PythonPassB";

  ASSERT_TRUE(PassRegistry::GetInstance().RegisterPythonPass(first_pass_desc, CreateStage1PythonFusionPass));
  EXPECT_FALSE(PassRegistry::GetInstance().RegisterPythonPass(second_pass_desc, CreateStage1PythonFusionPass));

  auto passes = PassRegistry::GetInstance().GetFusionPassRegDataByStage(CustomPassStage::kAfterInferShape);
  ASSERT_EQ(passes.size(), 1);
  EXPECT_EQ(passes[0].GetPassName().GetString(), first_pass_desc.pass_name);
}
} // namespace fusion
} // namespace ge
