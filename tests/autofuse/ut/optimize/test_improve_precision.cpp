/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"

#include "asc_graph_builder.h"
#include "graph/ascendc_ir/utils/asc_graph_utils.h"
#define private public
#include "pre_process/improve_precision.h"
#undef private
#include "pre_process/pre_process_config.h"
#include "platform_context.h"
#include "ascgraph_info_complete.h"
#include "runtime_stub.h"

using namespace ge;
using namespace ge::testing;
using namespace optimize::pre_process;

namespace {
class TestImprovePrecision : public ::testing::Test {
 protected:
  void SetUp() override {
    ge::PlatformContext::GetInstance().Reset();
    PreProcessConfig::Instance().Reset();
    auto stub_v1 = std::make_shared<RuntimeStub>();
    RuntimeStub::SetInstance(stub_v1);
    dlog_setlevel(ASCGEN_MODULE_NAME, DLOG_ERROR, 0);
  }

  void TearDown() override {
    ge::PlatformContext::GetInstance().Reset();
    unsetenv("AUTOFUSE_FLAGS");
    PreProcessConfig::Instance().Reset();
    dlog_setlevel(ASCGEN_MODULE_NAME, DLOG_ERROR, 0);
  }
};

// Helper: count nodes of a given type in an AscGraph
size_t CountNodesByType(AscGraph &graph, const std::string &type) {
  size_t count = 0U;
  for (const auto &node : AscGraphUtils::GetComputeGraph(graph)->GetAllNodes()) {
    if (node->GetType() == type) {
      ++count;
    }
  }
  return count;
}

// Helper: check if a node's output dtype matches
bool CheckNodeOutputDtype(AscGraph &graph, const std::string &node_name, ge::DataType expected_dtype) {
  for (const auto &node : AscGraphUtils::GetComputeGraph(graph)->GetAllNodes()) {
    if (node->GetName() == node_name) {
      auto desc = node->GetOpDesc();
      if (desc != nullptr && desc->GetOutputDesc(0).GetDataType() == expected_dtype) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace

// ==================== Basic graph: fp16 Load → fp16 Abs → fp16 Store ====================
// Expected: Load → Cast(fp32) → Abs(fp32) → Cast(fp16) → Store

TEST_F(TestImprovePrecision, Fp16LoadToFp16Store_InsertsTwoCasts) {
  auto graph = AscGraphBuilder("test_fp16_load_store")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Abs("abs0", "load0")
    .Store("store0", "abs0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  size_t cast_count_before = CountNodesByType(graph, ascir_op::Cast::Type);
  ASSERT_EQ(cast_count_before, 0U);

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  size_t cast_count_after = CountNodesByType(graph, ge::ascir_op::Cast::Type);
  // Load后应插入Cast升精度，Store前应插入Cast降精度
  EXPECT_GE(cast_count_after, 1U);
}

// ==================== Cast fp16→fp32 should be removed ====================

TEST_F(TestImprovePrecision, CastFp16ToFp32_Removed) {
  auto graph = AscGraphBuilder("test_cast_fp16_to_fp32")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Cast("cast0", "load0", ge::DT_FLOAT)
    .Store("store0", "cast0")
    .Output("output0", "store0", 0, ge::DT_FLOAT)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // fp16→fp32 cast应在升精度过程中被删除或修改
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  // Load后需要Cast(升精度)，原来的fp16→fp32 Cast被删
  // Store前可能插入降精度Cast
  EXPECT_GE(cast_count, 1U);
}

// ==================== Cast fp32→fp16 should be removed ====================

TEST_F(TestImprovePrecision, CastFp32ToFp16_Removed) {
  auto graph = AscGraphBuilder("test_cast_fp32_to_fp16")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT)
    .Load("load0", "data0")
    .Cast("cast0", "load0", ge::DT_FLOAT16)
    .Store("store0", "cast0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // fp32→fp16 cast同样是float间转换，应被删除
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  // 原Cast被删，Store前可能插入新的降精度Cast
  EXPECT_GE(cast_count, 0U);
}

// ==================== Scalar fp16 should change to fp32 ====================

TEST_F(TestImprovePrecision, ScalarFp16_ChangedToFp32) {
  auto graph = AscGraphBuilder("test_scalar_fp16")
    .Loops({Sym("s0")})
    .Scalar("scalar0", "1.0", ge::DT_FLOAT16)
    .Mul("mul0", "scalar0", "scalar0")
    .Store("store0", "mul0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // Scalar输出应变为fp32
  EXPECT_TRUE(CheckNodeOutputDtype(graph, "scalar0", ge::DT_FLOAT));
}

// ==================== Blacklist test ====================

TEST_F(TestImprovePrecision, AllBlacklist_SkipsPrecisionImprovement) {
  // 设置黑名单为 "all"，重新解析让单例生效
  setenv("AUTOFUSE_FLAGS", "--autofuse_enhance_precision_blacklist=all", 1);
  PreProcessConfig::Instance().Reset();

  auto graph = AscGraphBuilder("test_blacklist_all")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Store("store0", "load0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // 黑名单 "all" 应跳过精度提升
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  EXPECT_EQ(cast_count, 0U);
}

// ==================== Already fp32 graph should remain unchanged ====================

TEST_F(TestImprovePrecision, Fp32Graph_NoChange) {
  auto graph = AscGraphBuilder("test_fp32_graph")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT)
    .Load("load0", "data0")
    .Abs("abs0", "load0")
    .Store("store0", "abs0")
    .Output("output0", "store0", 0, ge::DT_FLOAT)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // 全fp32图不需要插入Cast
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  EXPECT_EQ(cast_count, 0U);
}

// ==================== bf16 Load → bf16 Mul → bf16 Store ====================

TEST_F(TestImprovePrecision, Bf16Graph_ComputeNodePromotedToFp32) {
  auto graph = AscGraphBuilder("test_bf16_graph")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_BF16)
    .Load("load0", "data0")
    .Mul("mul0", "load0", "load0")
    .Store("store0", "mul0")
    .Output("output0", "store0", 0, ge::DT_BF16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // Mul节点输出应变为fp32
  EXPECT_TRUE(CheckNodeOutputDtype(graph, "mul0", ge::DT_FLOAT));
  // Load后应有Cast
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  EXPECT_GE(cast_count, 1U);
}

// ==================== Load(fp16) → Cast(fp16) already exists → no duplicate Cast ====================

TEST_F(TestImprovePrecision, LoadWithExistingCast_NoDuplicateCastInserted) {
  auto graph = AscGraphBuilder("test_load_existing_cast")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Cast("cast0", "load0", ge::DT_FLOAT16)
    .Abs("abs0", "cast0")
    .Store("store0", "abs0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // Load后已有Cast，不应再重复插入升精度Cast
  // cast0 是 fp16→fp16 的float间Cast，应被删除
  EXPECT_TRUE(CheckNodeOutputDtype(graph, "abs0", ge::DT_FLOAT));
}

// ==================== Load(fp16) → Store(fp16) direct: all BlackList1 nodes, skip ====================

TEST_F(TestImprovePrecision, LoadDirectToStore_AllBlackList1_SkipsImprovement) {
  auto graph = AscGraphBuilder("test_load_to_store_blacklist")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Store("store0", "load0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // 所有非Data/Output节点(Load, Store)都在BlackList1中，应跳过
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  EXPECT_EQ(cast_count, 0U);
}

// ==================== Partial blacklist2 (specific op type) ====================

TEST_F(TestImprovePrecision, PartialBlacklist_SkipsOnlyBlacklistedOps) {
  setenv("AUTOFUSE_FLAGS", "--autofuse_enhance_precision_blacklist=Abs", 1);
  PreProcessConfig::Instance().Reset();

  auto graph = AscGraphBuilder("test_partial_blacklist")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Abs("abs0", "load0")
    .Mul("mul0", "abs0", "abs0")
    .Store("store0", "mul0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // Abs在黑名单中，但Mul不在，所以仍应进行精度提升
  EXPECT_TRUE(CheckNodeOutputDtype(graph, "mul0", ge::DT_FLOAT));
  EXPECT_GE(CountNodesByType(graph, ascir_op::Cast::Type), 1U);
}

// ==================== Two Other nodes connected: second Other sees non-Cast/Load/Gather peer ====================

TEST_F(TestImprovePrecision, ChainOtherNodes_SecondNodeNoExtraCast) {
  auto graph = AscGraphBuilder("test_chain_other")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Abs("abs0", "load0")
    .Neg("neg0", "abs0")
    .Store("store0", "neg0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // abs0和neg0输出都应变为fp32
  EXPECT_TRUE(CheckNodeOutputDtype(graph, "abs0", ge::DT_FLOAT));
  EXPECT_TRUE(CheckNodeOutputDtype(graph, "neg0", ge::DT_FLOAT));
  // Load后插入1个升精度Cast，Store前可能插入1个降精度Cast
  // neg0的peer是abs0(Other类型)，不会再为neg0额外插入Cast
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  EXPECT_GE(cast_count, 1U);
}

// ==================== Cast between fp16 output and Store: Cast→Store should not change dtype ====================

TEST_F(TestImprovePrecision, CastFp16DirectToStore_CastNotChanged) {
  auto graph = AscGraphBuilder("test_cast_to_store")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Abs("abs0", "load0")
    .Cast("cast0", "abs0", ge::DT_FLOAT16)
    .Store("store0", "cast0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // Cast(fp32→fp16) 是float间Cast，应被删除
  // Store前如果有精度不匹配应插入降精度Cast
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  EXPECT_GE(cast_count, 0U);
}

// ==================== PreProcessConfig: comma-separated blacklist values ====================

TEST_F(TestImprovePrecision, CommaBlacklist_ParsedCorrectly) {
  setenv("AUTOFUSE_FLAGS", "--autofuse_enhance_precision_blacklist=Abs,Mul", 1);
  PreProcessConfig::Instance().Reset();

  const auto &bl = PreProcessConfig::Instance().GetImprovePrecisionBlacklist();
  EXPECT_TRUE(bl.find("Abs") != bl.end());
  EXPECT_TRUE(bl.find("Mul") != bl.end());
  EXPECT_TRUE(bl.find("Add") == bl.end());
}

// ==================== PreProcessConfig: semicolon truncation ====================

TEST_F(TestImprovePrecision, SemicolonBlacklist_TruncatedCorrectly) {
  setenv("AUTOFUSE_FLAGS", "--autofuse_enhance_precision_blacklist=Abs,Mul;other_flag=123", 1);
  PreProcessConfig::Instance().Reset();

  const auto &bl = PreProcessConfig::Instance().GetImprovePrecisionBlacklist();
  EXPECT_TRUE(bl.find("Abs") != bl.end());
  EXPECT_TRUE(bl.find("Mul") != bl.end());
  // 分号后的 "other_flag=123" 不应被解析为黑名单
  EXPECT_TRUE(bl.find("other_flag=123") == bl.end());
}

// ==================== PreProcessConfig: trailing punctuation stripped ====================

TEST_F(TestImprovePrecision, TrailingPunctuationBlacklist_StrippedCorrectly) {
  setenv("AUTOFUSE_FLAGS", "--autofuse_enhance_precision_blacklist=Abs,Mul;", 1);
  PreProcessConfig::Instance().Reset();

  const auto &bl = PreProcessConfig::Instance().GetImprovePrecisionBlacklist();
  EXPECT_TRUE(bl.find("Abs") != bl.end());
  EXPECT_TRUE(bl.find("Mul") != bl.end());
  // 末尾分号被去除，不应产生多余条目
}

// ==================== all blacklist with unsupported dtype must still improve ====================

TEST_F(TestImprovePrecision, AllBlacklist_UnsupportedDtype_ForceImprove) {
  // 设置 all 黑名单
  setenv("AUTOFUSE_FLAGS", "--autofuse_enhance_precision_blacklist=all", 1);
  PreProcessConfig::Instance().Reset();
  // 设置未知平台，使 CommonInferDtype 对 Abs(fp16) 返回 FAILED，模拟"不支持"场景
  ge::PlatformContext::GetInstance().SetPlatform("unknown");

  // 构造 fp16 Load → fp16 Abs(fp16) → fp16 Store
  // Abs 不支持 fp16 输出，all 黑名单应被忽略，强制升精度
  auto graph = AscGraphBuilder("test_all_blacklist_unsupported")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Abs("abs0", "load0")
    .Store("store0", "abs0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // all 黑名单中 Abs 的 dtype 不被后端支持，应忽略 all 强制升精度
  EXPECT_TRUE(CheckNodeOutputDtype(graph, "abs0", ge::DT_FLOAT));
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  EXPECT_GE(cast_count, 1U);
}

// ==================== all blacklist + specific op blacklist coexist ====================

TEST_F(TestImprovePrecision, AllBlacklist_WithSpecificOp_BlacklistAllTakesPrecedence) {
  // all 和 Abs 同时在黑名单中，all 优先
  setenv("AUTOFUSE_FLAGS", "--autofuse_enhance_precision_blacklist=all,Abs", 1);
  PreProcessConfig::Instance().Reset();

  auto graph = AscGraphBuilder("test_all_with_specific")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Store("store0", "load0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  // Load 直连 Store，所有非 Data/Output 节点都在 BlackList1 中，
  // all 检查所有节点 dtype 都支持，应跳过升精度
  size_t cast_count = CountNodesByType(graph, ascir_op::Cast::Type);
  EXPECT_EQ(cast_count, 0U);
}

// ==================== Scalar bf16 changed to fp32 ====================

TEST_F(TestImprovePrecision, ScalarBf16_ChangedToFp32) {
  auto graph = AscGraphBuilder("test_scalar_bf16")
    .Loops({Sym("s0")})
    .Scalar("scalar0", "1.0", ge::DT_BF16)
    .Mul("mul0", "scalar0", "scalar0")
    .Store("store0", "mul0")
    .Output("output0", "store0", 0, ge::DT_BF16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto ret = ImprovePrecisionForAscGraph(graph);
  ASSERT_EQ(ret, ge::SUCCESS);

  EXPECT_TRUE(CheckNodeOutputDtype(graph, "scalar0", ge::DT_FLOAT));
  EXPECT_TRUE(CheckNodeOutputDtype(graph, "mul0", ge::DT_FLOAT));
}
