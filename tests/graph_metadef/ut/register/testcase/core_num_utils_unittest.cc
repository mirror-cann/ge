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
#include <memory>
#include <string>
#include "register/core_num_utils.h"
#include "graph/compute_graph.h"
#include "graph/op_desc.h"
#include "graph/utils/attr_utils.h"
#include "graph/ge_local_context.h"

namespace ge {
namespace {

class CoreNumValidateUT : public testing::Test {
 protected:
  void SetUp() {
    platform_info_.soc_info.ai_core_cnt = 32;
    platform_info_.soc_info.vector_core_cnt = 16;
  }

  void TearDown() {
    // 统一清理 thread-local 选项，消除跨用例 SOC_VERSION 污染隐患（GetOption 查 graph/session/global）。
    GetThreadLocalContext().SetGlobalOption({});
    GetThreadLocalContext().SetSessionOption({});
    GetThreadLocalContext().SetGraphOption({});
  }

  fe::PlatformInfo platform_info_;
};

// --- ValidateCoreNumWithOpDesc ---

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_NullOpDesc_ReturnsError) {
  OpDescPtr null_op;
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, null_op), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_NoAttrs_ReturnsSuccess) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

// 属性存在但类型非 string（如 int），GetStr 失败 -> "It is not string." 分支。
TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_AiCoreNumNotString_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetInt(op_desc, kAiCoreNumOp, 16);
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_VectorCoreNumNotString_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetInt(op_desc, kVectorCoreNumOp, 8);
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_ValidAiCoreNum_ReturnsSuccess) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "16");
  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_ZeroAiCoreNum_ReturnsSuccess) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "0");
  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_MaxAiCoreNum_ReturnsSuccess) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "32");
  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_AiCoreNumOutOfRange_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "33");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_AiCoreNumNotInteger_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "abc");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_AiCoreNumNegative_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "-1");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_ValidVectorCoreNum_ReturnsSuccess) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kVectorCoreNumOp, "8");
  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_VectorCoreNumOutOfRange_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kVectorCoreNumOp, "17");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_VectorCoreNumNotInteger_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kVectorCoreNumOp, "xyz");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_BothAttrsValid_ReturnsSuccess) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "16");
  (void)ge::AttrUtils::SetStr(op_desc, kVectorCoreNumOp, "8");
  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_BothAttrsOneInvalid_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "16");
  (void)ge::AttrUtils::SetStr(op_desc, kVectorCoreNumOp, "99");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

// --- ValidateCoreNumWithOpDesc edge cases ---

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_AiCoreNumEmptyString_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_AiCoreNumOverflow_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "99999999999999999999");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_AiCoreNumLeadingZero_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kAiCoreNumOp, "01");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithOpDesc_VectorCoreNumLeadingZero_ReturnsError) {
  auto op_desc = std::make_shared<OpDesc>("test_op", "Relu");
  (void)ge::AttrUtils::SetStr(op_desc, kVectorCoreNumOp, "01");
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithOpDesc(platform_info_, op_desc), GRAPH_SUCCESS);
}

// --- ValidateCoreNumWithGraph(compute_graph) single-param overload ---

TEST_F(CoreNumValidateUT, ValidateCoreNumWithGraphSingleParam_NullGraph_ReturnsError) {
  ComputeGraphPtr null_graph;
  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithGraph(null_graph), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithGraphSingleParam_NoCoreNumAttrs_ReturnsSuccess) {
  auto graph = std::make_shared<ComputeGraph>("test_graph");
  auto op1 = std::make_shared<OpDesc>("op1", "Relu");
  graph->AddNode(op1);

  auto op2 = std::make_shared<OpDesc>("op2", "Add");
  graph->AddNode(op2);

  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithGraph(graph), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithGraphSingleParam_EmptyGraph_ReturnsSuccess) {
  auto graph = std::make_shared<ComputeGraph>("test_graph");
  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithGraph(graph), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithGraphSingleParam_AiCoreNumValid_ReturnsSuccess) {
  std::map<std::string, std::string> global_opts = {{"ge.socVersion", "Ascend910B2"}};
  GetThreadLocalContext().SetGlobalOption(global_opts);

  auto graph = std::make_shared<ComputeGraph>("test_graph");
  auto op1 = std::make_shared<OpDesc>("op1", "Relu");
  (void)ge::AttrUtils::SetStr(op1, kAiCoreNumOp, "16");
  graph->AddNode(op1);

  EXPECT_EQ(CoreNumUtils::ValidateCoreNumWithGraph(graph), GRAPH_SUCCESS);
}

TEST_F(CoreNumValidateUT, ValidateCoreNumWithGraphSingleParam_AiCoreNumInvalid_ReturnsError) {
  std::map<std::string, std::string> global_opts = {{"ge.socVersion", "Ascend910B2"}};
  GetThreadLocalContext().SetGlobalOption(global_opts);

  auto graph = std::make_shared<ComputeGraph>("test_graph");
  auto op1 = std::make_shared<OpDesc>("op1", "Relu");
  (void)ge::AttrUtils::SetStr(op1, kAiCoreNumOp, "99999");
  graph->AddNode(op1);

  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithGraph(graph), GRAPH_SUCCESS);
}

// fail-fast：图中存在核数属性但 SOC_VERSION 未设置时，应报错而非静默跳过（外部错误码 E10001）。
TEST_F(CoreNumValidateUT, ValidateCoreNumWithGraphSingleParam_CoreNumWithoutSocVersion_ReturnsError) {
  // 清理可能由其他用例残留的 SOC_VERSION，确保本用例处于"未设置"状态。
  GetThreadLocalContext().SetGlobalOption({});
  GetThreadLocalContext().SetSessionOption({});

  auto graph = std::make_shared<ComputeGraph>("test_graph");
  auto op1 = std::make_shared<OpDesc>("op1", "Relu");
  (void)ge::AttrUtils::SetStr(op1, kAiCoreNumOp, "16");
  graph->AddNode(op1);

  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithGraph(graph), GRAPH_SUCCESS);
}

// fail-fast：vectorcore 属性同样在 SOC_VERSION 缺失时报错。
TEST_F(CoreNumValidateUT, ValidateCoreNumWithGraphSingleParam_VectorCoreNumWithoutSocVersion_ReturnsError) {
  GetThreadLocalContext().SetGlobalOption({});
  GetThreadLocalContext().SetSessionOption({});

  auto graph = std::make_shared<ComputeGraph>("test_graph");
  auto op1 = std::make_shared<OpDesc>("op1", "Relu");
  (void)ge::AttrUtils::SetStr(op1, kVectorCoreNumOp, "8");
  graph->AddNode(op1);

  EXPECT_NE(CoreNumUtils::ValidateCoreNumWithGraph(graph), GRAPH_SUCCESS);
}

}  // namespace
}  // namespace ge
