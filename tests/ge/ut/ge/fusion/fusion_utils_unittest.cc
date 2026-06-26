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
#include <memory>
#include <fstream>
#include "nlohmann/json.hpp"
#include "mmpa_api.h"
#include "graph/fusion/fusion_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_adapter.h"
#include "graph/ge_local_context.h"
#include "es_ge_test_ops.h"
#include "ge/fusion/pattern_matcher.h"
#include "ge/fusion/pattern.h"
#include "graph/utils/graph_utils.h"

namespace ge {
namespace fusion {
namespace {
std::string GetCodeDir() {
  ge::char_t current_path[MMPA_MAX_PATH] = {'\0'};
  getcwd(current_path, MMPA_MAX_PATH);
  return current_path;
}
}  // namespace
class UtestFusionUtils : public testing::Test {
 protected:
  void SetUp() {
    global_options_bak_ = ge::GetThreadLocalContext().GetAllGlobalOptions();
  }

  void TearDown() {
    GetThreadLocalContext().SetGlobalOption(global_options_bak_);
  }

 private:
  std::map<std::string, std::string> global_options_bak_;
};

TEST_F(UtestFusionUtils, GetFusionSwitchFilePathFromOption) {
  // No option config
  EXPECT_TRUE(FusionUtils::GetFusionSwitchFileFromOption().empty());

  // with option config
  std::string config_file_path = "./fusion_config_file/fusion_config.json";
  GetThreadLocalContext().SetGlobalOption({{FUSION_SWITCH_FILE, config_file_path}});
  EXPECT_STREQ(FusionUtils::GetFusionSwitchFileFromOption().c_str(), config_file_path.c_str());
}

TEST_F(UtestFusionUtils, ParseFusionSwitch) {
  // No option config
  EXPECT_TRUE(FusionUtils::ParseFusionSwitch().empty());
  std::string json_str =
      "{\n"
      "      \"Switch\": {\n"
      "          \"GraphFusion\": {\n"
      "            \"CUSTOM_PASS1\" : \"off\",\n"
      "            \"CUSTOM_PASS2\" : \"on\",\n"
      "            \"CUSTOM_PASS3\" : \"off\"\n"
      "          }\n"
      "      }\n"
      "  }";
  std::ofstream json_file("./fusion_switch_config.json");
  json_file << json_str << std::endl;

  // with option config
  std::string config_file_path = GetCodeDir() + "/fusion_switch_config.json";
  std::cout << "cur path " << config_file_path << std::endl;
  GetThreadLocalContext().SetGlobalOption({{FUSION_SWITCH_FILE, config_file_path}});
  auto pass_name_2_switches = FusionUtils::ParseFusionSwitch();
  EXPECT_EQ(pass_name_2_switches.size(), 3);
  EXPECT_EQ(pass_name_2_switches["CUSTOM_PASS1"], false);
  EXPECT_EQ(pass_name_2_switches["CUSTOM_PASS2"], true);
  EXPECT_EQ(pass_name_2_switches["CUSTOM_PASS3"], false);
  remove("./fusion_switch_config.json");
}

TEST_F(UtestFusionUtils, WillCauseCycleIfFuse_NullMatchResult_ReturnFalse) {
  EXPECT_FALSE(FusionUtils::WillCauseCycleIfFuse(nullptr));
}

TEST_F(UtestFusionUtils, WillCauseCycleIfFuse_LinearGraphNoCycle_ReturnFalse) {
  using namespace ge::es;

  auto graph_builder = EsGraphBuilder("linear_graph");
  auto esb_graph = graph_builder.GetCGraphBuilder();
  auto data = EsCreateGraphInput(esb_graph, 0);
  auto const_data = EsCreateScalarFloat(esb_graph, 1.0f);
  auto add = EsAdd(data, const_data);
  auto relu = EsRelu(add);
  esb_graph->SetGraphOutput(relu, 0);

  auto graph = std::make_shared<Graph>(*graph_builder.BuildAndReset());

  auto pattern_builder = EsGraphBuilder("pattern");
  auto pattern_esb = pattern_builder.GetCGraphBuilder();
  auto p_data = EsCreateGraphInput(pattern_esb, 0);
  auto p_const = EsCreateScalarFloat(pattern_esb, 1.0f);
  auto p_add = EsAdd(p_data, p_const);
  auto p_relu = EsRelu(p_add);
  pattern_esb->SetGraphOutput(p_relu, 0);

  auto pattern = std::make_unique<Pattern>(std::move(*pattern_builder.BuildAndReset()));
  PatternMatcher matcher(std::move(pattern), graph);

  auto match_result = matcher.MatchNext();
  ASSERT_NE(match_result, nullptr);
  EXPECT_FALSE(FusionUtils::WillCauseCycleIfFuse(match_result));
}

TEST_F(UtestFusionUtils, WillCauseCycleIfFuse_GraphWithControlEdgeCycle_ReturnTrue) {
  using namespace ge::es;

  auto graph_builder = EsGraphBuilder("cycle_graph");
  auto esb_graph = graph_builder.GetCGraphBuilder();

  auto data0 = EsCreateGraphInput(esb_graph, 0);
  auto data1 = EsCreateGraphInput(esb_graph, 1);
  auto abs = EsAbs(data0);
  auto relu = EsRelu(abs);
  auto abs1 = EsAbs(relu);
  auto relu1 = EsRelu(data1);
  auto add = EsAdd(abs1, relu1);

  GraphUtils::AddEdge(NodeAdapter::GNode2Node(abs->GetProducer())->GetOutControlAnchor(),
                      NodeAdapter::GNode2Node(relu1->GetProducer())->GetInControlAnchor());
  GraphUtils::AddEdge(NodeAdapter::GNode2Node(relu1->GetProducer())->GetOutControlAnchor(),
                      NodeAdapter::GNode2Node(abs1->GetProducer())->GetInControlAnchor());

  esb_graph->SetGraphOutput(add, 0);
  auto graph = std::make_shared<Graph>(*graph_builder.BuildAndReset());

  auto pattern_builder = EsGraphBuilder("pattern");
  auto pattern_esb = pattern_builder.GetCGraphBuilder();
  auto p_data = EsCreateGraphInput(pattern_esb, 0);
  auto p_abs = EsAbs(p_data);
  auto p_relu = EsRelu(p_abs);
  auto p_abs1 = EsAbs(p_relu);
  pattern_esb->SetGraphOutput(p_abs1, 0);

  auto pattern = std::make_unique<Pattern>(std::move(*pattern_builder.BuildAndReset()));
  PatternMatcher matcher(std::move(pattern), graph);

  auto match_result = matcher.MatchNext();
  ASSERT_NE(match_result, nullptr);
  EXPECT_TRUE(FusionUtils::WillCauseCycleIfFuse(match_result));
}
}  // namespace fusion
}  // namespace ge
