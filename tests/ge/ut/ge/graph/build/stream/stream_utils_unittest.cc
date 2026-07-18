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

#include <cstdlib>
#include <string>

#include "graph/build/stream/stream_utils.h"

#include <graph_utils_ex.h>
#include <debug/ge_attr_define.h>

#include "graph/ge_local_context.h"
#include "common/multi_stream_share_graph.h"

namespace ge {
class UtestStreamUtils : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UtestStreamUtils, ConvertUserStreamLabelToInnerStreamLabel) {
  auto graph = MultiStreamShareGraph::TwoNodeGraphWithUserStreamLabel();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  auto trans1 = compute_graph->FindNode("trans1");
  std::string trans1_user_stream_label;
  ASSERT_TRUE(AttrUtils::GetStr(trans1->GetOpDesc(), "_user_stream_label", trans1_user_stream_label));
  ASSERT_STREQ(trans1_user_stream_label.c_str(), "test_label");

  EXPECT_EQ(StreamUtils::TransUserStreamLabel(compute_graph), SUCCESS);

  std::string trans1_final_stream_label;
  EXPECT_TRUE(AttrUtils::GetStr(trans1->GetOpDesc(), ATTR_NAME_STREAM_LABEL, trans1_final_stream_label));
  EXPECT_STREQ(trans1_final_stream_label.c_str(), "test_label");
}

TEST_F(UtestStreamUtils, Convert_Both_UserStreamLabel_InnerStreamLabel) {
  auto graph = MultiStreamShareGraph::TwoNodeGraphWithUserStreamLabel();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  auto trans1 = compute_graph->FindNode("trans1");
  AttrUtils::SetStr(trans1->GetOpDesc(), ATTR_NAME_STREAM_LABEL, "inner_label");

  std::string trans1_inner_stream_label;
  ASSERT_TRUE(AttrUtils::GetStr(trans1->GetOpDesc(), ATTR_NAME_STREAM_LABEL, trans1_inner_stream_label));
  ASSERT_STREQ(trans1_inner_stream_label.c_str(), "inner_label");
  std::string trans1_user_stream_label;
  ASSERT_TRUE(AttrUtils::GetStr(trans1->GetOpDesc(), "_user_stream_label", trans1_user_stream_label));
  ASSERT_STREQ(trans1_user_stream_label.c_str(), "test_label");

  EXPECT_EQ(StreamUtils::TransUserStreamLabel(compute_graph), SUCCESS);

  std::string trans1_final_stream_label;
  EXPECT_TRUE(AttrUtils::GetStr(trans1->GetOpDesc(), ATTR_NAME_STREAM_LABEL, trans1_final_stream_label));
  EXPECT_STREQ(trans1_final_stream_label.c_str(), trans1_user_stream_label.c_str());
}

TEST_F(UtestStreamUtils, EnableDynamicShapeMultiStream_WithOptionModesAndNoEnv_ReturnsFalse) {
  const char *const env_name = "ENABLE_DYNAMIC_SHAPE_MULTI_STREAM";
  const char *const env_value = std::getenv(env_name);
  const bool had_env = (env_value != nullptr);
  const std::string env_backup = had_env ? env_value : "";
  (void)unsetenv(env_name);

  const auto back_session_options = GetThreadLocalContext().GetAllSessionOptions();
  const auto back_graph_options = GetThreadLocalContext().GetAllGraphOptions();
  auto session_options = back_session_options;
  auto graph_options = back_graph_options;
  const std::string auto_multistream_modes[] = {"cv", "LoadBalance:8", "MainStream:8", "LoadBalance"};
  for (const auto &mode : auto_multistream_modes) {
    session_options["ge.autoMultistreamParallelMode"] = mode;
    graph_options["ge.autoMultistreamParallelMode"] = mode;
    GetThreadLocalContext().SetSessionOption(session_options);
    GetThreadLocalContext().SetGraphOption(graph_options);
    EXPECT_FALSE(StreamUtils::EnableDynamicShapeMultiStream()) << mode;
  }

  GetThreadLocalContext().SetSessionOption(back_session_options);
  GetThreadLocalContext().SetGraphOption(back_graph_options);
  if (had_env) {
    (void)setenv(env_name, env_backup.c_str(), 1);
  } else {
    (void)unsetenv(env_name);
  }
}
}  // namespace ge
