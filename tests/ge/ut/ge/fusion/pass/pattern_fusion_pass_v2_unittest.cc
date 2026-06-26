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
#include "ge/fusion/pass/pattern_fusion_pass.h"
#include "ge/fusion/pattern.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_adapter.h"
#include "register/custom_pass_context_impl.h"
#include "stub/gert_runtime_stub.h"

namespace ge {
namespace fusion {
using namespace ge::es;

class UtestPatternFusionPassV2 : public testing::Test {
 public:
  static void SetUpTestSuite() {}
  static void TearDownTestSuite() {}
};

namespace {
// 构造 transdata→relu 的简单 V2 pass，提供两个 hook 的可观测 callback。
// MeetRequirements / Replacement 可被外部 lambda 接管，用来观测入参 pass_context。
class ProbeV2Pass : public PatternFusionPassV2 {
 public:
  using MeetReqProbe = std::function<bool(const std::unique_ptr<MatchResult> &, CustomPassContext &)>;
  using ReplacementProbe = std::function<GraphUniqPtr(const std::unique_ptr<MatchResult> &, CustomPassContext &)>;

  void SetMeetRequirementsProbe(MeetReqProbe probe) {
    meet_req_probe_ = std::move(probe);
  }
  void SetReplacementProbe(ReplacementProbe probe) {
    replacement_probe_ = std::move(probe);
  }

 protected:
  std::vector<PatternUniqPtr> Patterns() override {
    std::vector<PatternUniqPtr> patterns;
    auto pattern_graph = ge::es::EsGraphBuilder("pattern");
    auto esb_graph = pattern_graph.GetCGraphBuilder();
    auto data = EsCreateGraphInput(esb_graph, 0);
    auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
    esb_graph->SetGraphOutput(transdata, 0);
    patterns.emplace_back(std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset())));
    return patterns;
  }

  bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result, CustomPassContext &pass_context) override {
    if (meet_req_probe_) {
      return meet_req_probe_(match_result, pass_context);
    }
    return PatternFusionPassV2::MeetRequirements(match_result, pass_context);
  }

  GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result, CustomPassContext &pass_context) override {
    if (replacement_probe_) {
      return replacement_probe_(match_result, pass_context);
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

TEST_F(UtestPatternFusionPassV2, RunSuccess_BasicV2Flow) {
  auto target_graph = MakeTargetGraph();
  ProbeV2Pass pass;
  CustomPassContext context;
  EXPECT_EQ(pass.Run(target_graph, context), SUCCESS);

  // 再跑一次，已无 transdata 可匹配，期望 NOT_CHANGED。
  EXPECT_EQ(pass.Run(target_graph, context), NOT_CHANGED);
}

TEST_F(UtestPatternFusionPassV2, MeetRequirements_ReceivesPassContext) {
  auto target_graph = MakeTargetGraph();
  ProbeV2Pass pass;
  CustomPassContext context;
  context.SetPassName(AscendString("V2_TestPass"));

  bool meet_invoked = false;
  std::string observed_pass_name;
  pass.SetMeetRequirementsProbe([&](const std::unique_ptr<MatchResult> &, CustomPassContext &ctx) {
    meet_invoked = true;
    observed_pass_name = ctx.GetPassName().GetString();
    return true;
  });

  EXPECT_EQ(pass.Run(target_graph, context), SUCCESS);
  EXPECT_TRUE(meet_invoked);
  EXPECT_EQ(observed_pass_name, "V2_TestPass");
}

TEST_F(UtestPatternFusionPassV2, Replacement_ReceivesPassContextAndCanWriteErrorMessage) {
  auto target_graph = MakeTargetGraph();
  ProbeV2Pass pass;
  CustomPassContext context;
  context.SetPassName(AscendString("V2_TestPass"));

  bool replacement_invoked = false;
  std::string observed_pass_name;
  pass.SetReplacementProbe([&](const std::unique_ptr<MatchResult> &, CustomPassContext &ctx) -> GraphUniqPtr {
    replacement_invoked = true;
    observed_pass_name = ctx.GetPassName().GetString();
    ctx.SetErrorMessage(AscendString("v2_replacement_was_here"));
    auto replace_graph = ge::es::EsGraphBuilder("replacement");
    auto esb_graph = replace_graph.GetCGraphBuilder();
    auto data = EsCreateGraphInput(esb_graph, 0);
    auto relu = EsRelu(data);
    esb_graph->SetGraphOutput(relu, 0);
    return replace_graph.BuildAndReset();
  });

  EXPECT_EQ(pass.Run(target_graph, context), SUCCESS);
  EXPECT_TRUE(replacement_invoked);
  EXPECT_EQ(observed_pass_name, "V2_TestPass");
  EXPECT_STREQ(context.GetErrorMessage().GetString(), "v2_replacement_was_here");
}

TEST_F(UtestPatternFusionPassV2, MeetRequirements_DefaultReturnsTrueAndReplaces) {
  // 仅设置 ReplacementProbe，不 override MeetRequirements；期望走 PatternFusionPassV2 默认实现（返回 true）。
  auto target_graph = MakeTargetGraph();
  ProbeV2Pass pass;
  CustomPassContext context;

  bool replacement_invoked = false;
  pass.SetReplacementProbe([&](const std::unique_ptr<MatchResult> &, CustomPassContext &) -> GraphUniqPtr {
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

TEST_F(UtestPatternFusionPassV2, MeetRequirements_FalseSkipsReplacement) {
  auto target_graph = MakeTargetGraph();
  ProbeV2Pass pass;
  CustomPassContext context;

  pass.SetMeetRequirementsProbe([](const std::unique_ptr<MatchResult> &, CustomPassContext &) { return false; });

  bool replacement_invoked = false;
  pass.SetReplacementProbe([&](const std::unique_ptr<MatchResult> &, CustomPassContext &) -> GraphUniqPtr {
    replacement_invoked = true;
    return nullptr;
  });

  EXPECT_EQ(pass.Run(target_graph, context), NOT_CHANGED);
  EXPECT_FALSE(replacement_invoked);
}

TEST_F(UtestPatternFusionPassV2, Replacement_NullptrAborts) {
  auto target_graph = MakeTargetGraph();
  ProbeV2Pass pass;
  CustomPassContext context;

  pass.SetReplacementProbe(
      [](const std::unique_ptr<MatchResult> &, CustomPassContext &) -> GraphUniqPtr { return nullptr; });

  EXPECT_NE(pass.Run(target_graph, context), SUCCESS);
}
}  // namespace fusion
}  // namespace ge
