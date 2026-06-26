/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "common/share_graph.h"
#include "es_ge_test_ops.h"
#include "ge/fusion/pass/decompose_pass.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_adapter.h"
#include "register/custom_pass_context_impl.h"
#include "stub/gert_runtime_stub.h"

namespace ge {
namespace fusion {
using namespace ge::es;

class UtestDecomposePassV2 : public testing::Test {
 public:
  static void SetUpTestSuite() {}
  static void TearDownTestSuite() {}
};

namespace {
class ProbeDecomposeV2Pass : public DecomposePassV2 {
 public:
  using MeetReqProbe = std::function<bool(const GNode &, CustomPassContext &)>;
  using ReplacementProbe = std::function<GraphUniqPtr(const GNode &, CustomPassContext &)>;

  explicit ProbeDecomposeV2Pass(const std::vector<AscendString> &op_types) : DecomposePassV2(op_types) {}

  void SetMeetRequirementsProbe(MeetReqProbe probe) {
    meet_req_probe_ = std::move(probe);
  }
  void SetReplacementProbe(ReplacementProbe probe) {
    replacement_probe_ = std::move(probe);
  }

 protected:
  bool MeetRequirements(const GNode &matched_node, CustomPassContext &pass_context) override {
    if (meet_req_probe_) {
      return meet_req_probe_(matched_node, pass_context);
    }
    return DecomposePassV2::MeetRequirements(matched_node, pass_context);
  }

  GraphUniqPtr Replacement(const GNode &matched_node, CustomPassContext &pass_context) override {
    if (replacement_probe_) {
      return replacement_probe_(matched_node, pass_context);
    }
    return BuildReluReplacement();
  }

 private:
  static GraphUniqPtr BuildReluReplacement() {
    auto replace_graph = ge::es::EsGraphBuilder("replacement");
    auto esb_graph = replace_graph.GetCGraphBuilder();
    auto data = EsCreateGraphInput(esb_graph, 0);
    auto relu = EsRelu(data);
    esb_graph->SetGraphOutput(relu, 0);
    return replace_graph.BuildAndReset();
  }

  MeetReqProbe meet_req_probe_;
  ReplacementProbe replacement_probe_;
};

GraphPtr MakeTargetGraph() {
  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  return GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);
}
}  // namespace

TEST_F(UtestDecomposePassV2, RunSuccess_BasicV2Flow) {
  auto target_graph = MakeTargetGraph();
  ProbeDecomposeV2Pass pass({TRANSDATA});
  CustomPassContext context;
  EXPECT_EQ(pass.Run(target_graph, context), SUCCESS);

  // 第二次跑，无 transdata 可匹配 → NOT_CHANGED。
  EXPECT_EQ(pass.Run(target_graph, context), NOT_CHANGED);
}

TEST_F(UtestDecomposePassV2, MeetRequirements_ReceivesPassContext) {
  auto target_graph = MakeTargetGraph();
  ProbeDecomposeV2Pass pass({TRANSDATA});
  CustomPassContext context;
  context.SetPassName(AscendString("DecomposeV2_TestPass"));

  bool meet_invoked = false;
  std::string observed_pass_name;
  pass.SetMeetRequirementsProbe([&](const GNode &, CustomPassContext &ctx) {
    meet_invoked = true;
    observed_pass_name = ctx.GetPassName().GetString();
    return true;
  });

  EXPECT_EQ(pass.Run(target_graph, context), SUCCESS);
  EXPECT_TRUE(meet_invoked);
  EXPECT_EQ(observed_pass_name, "DecomposeV2_TestPass");
}

TEST_F(UtestDecomposePassV2, Replacement_ReceivesPassContextAndCanWriteErrorMessage) {
  auto target_graph = MakeTargetGraph();
  ProbeDecomposeV2Pass pass({TRANSDATA});
  CustomPassContext context;
  context.SetPassName(AscendString("DecomposeV2_TestPass"));

  bool replacement_invoked = false;
  std::string observed_pass_name;
  pass.SetReplacementProbe([&](const GNode &, CustomPassContext &ctx) -> GraphUniqPtr {
    replacement_invoked = true;
    observed_pass_name = ctx.GetPassName().GetString();
    ctx.SetErrorMessage(AscendString("decompose_v2_was_here"));
    auto replace_graph = ge::es::EsGraphBuilder("replacement");
    auto esb_graph = replace_graph.GetCGraphBuilder();
    auto data = EsCreateGraphInput(esb_graph, 0);
    auto relu = EsRelu(data);
    esb_graph->SetGraphOutput(relu, 0);
    return replace_graph.BuildAndReset();
  });

  EXPECT_EQ(pass.Run(target_graph, context), SUCCESS);
  EXPECT_TRUE(replacement_invoked);
  EXPECT_EQ(observed_pass_name, "DecomposeV2_TestPass");
  EXPECT_STREQ(context.GetErrorMessage().GetString(), "decompose_v2_was_here");
}

TEST_F(UtestDecomposePassV2, MeetRequirements_DefaultReturnsTrueAndReplaces) {
  auto target_graph = MakeTargetGraph();
  ProbeDecomposeV2Pass pass({TRANSDATA});
  CustomPassContext context;

  bool replacement_invoked = false;
  pass.SetReplacementProbe([&](const GNode &, CustomPassContext &) -> GraphUniqPtr {
    replacement_invoked = true;
    auto replace_graph = ge::es::EsGraphBuilder("replacement");
    auto esb_graph = replace_graph.GetCGraphBuilder();
    auto data = EsCreateGraphInput(esb_graph, 0);
    auto relu = EsRelu(data);
    esb_graph->SetGraphOutput(relu, 0);
    return replace_graph.BuildAndReset();
  });

  EXPECT_EQ(pass.Run(target_graph, context), SUCCESS);
  EXPECT_TRUE(replacement_invoked);
}

TEST_F(UtestDecomposePassV2, MeetRequirements_FalseSkipsReplacement) {
  auto target_graph = MakeTargetGraph();
  ProbeDecomposeV2Pass pass({TRANSDATA});
  CustomPassContext context;

  pass.SetMeetRequirementsProbe([](const GNode &, CustomPassContext &) { return false; });

  bool replacement_invoked = false;
  pass.SetReplacementProbe([&](const GNode &, CustomPassContext &) -> GraphUniqPtr {
    replacement_invoked = true;
    return nullptr;
  });

  EXPECT_EQ(pass.Run(target_graph, context), NOT_CHANGED);
  EXPECT_FALSE(replacement_invoked);
}

TEST_F(UtestDecomposePassV2, NoMatch_ReturnsNotChanged) {
  // op_types 为不存在的类型，期望直接 NOT_CHANGED 且钩子均不触发。
  auto target_graph = MakeTargetGraph();
  ProbeDecomposeV2Pass pass({AscendString("__NonExistOp__")});
  CustomPassContext context;

  bool any_hook_invoked = false;
  pass.SetMeetRequirementsProbe([&](const GNode &, CustomPassContext &) {
    any_hook_invoked = true;
    return true;
  });
  pass.SetReplacementProbe([&](const GNode &, CustomPassContext &) -> GraphUniqPtr {
    any_hook_invoked = true;
    return nullptr;
  });

  EXPECT_EQ(pass.Run(target_graph, context), NOT_CHANGED);
  EXPECT_FALSE(any_hook_invoked);
}
}  // namespace fusion
}  // namespace ge
