/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"

#include "ascir.h"
#include "ascir_ops.h"
#include "schedule_utils.h"
#define private public
#include "optimize/pre_process/pre_process.h"
#include "optimize/pre_process/pre_process_config.h"
#undef private
#include "common/platform_context.h"
#include "ascgraph_info_complete.h"
#include "tests/autofuse/framework/easy_asc_graph/asc_graph_builder.h"
#include "runtime_stub.h"

using namespace ge;
using namespace ge::ascir_op;
using ge::testing::Sym;
using ge::testing::AscGraphBuilder;
using namespace optimize::pre_process;

namespace {

size_t CountNodesByType(AscGraph &graph, const std::string &type) {
  size_t count = 0U;
  for (const auto &node : AscGraphUtils::GetComputeGraph(graph)->GetAllNodes()) {
    if (node->GetType() == type) {
      ++count;
    }
  }
  return count;
}

bool HasEdge(AscGraph &graph, const std::string &src_name, const std::string &dst_name) {
  for (const auto &node : AscGraphUtils::GetComputeGraph(graph)->GetAllNodes()) {
    if (node->GetName() != src_name) {
      continue;
    }
    for (const auto &out_anchor : node->GetAllOutDataAnchors()) {
      for (const auto &peer : out_anchor->GetPeerInDataAnchors()) {
        if (peer != nullptr && peer->GetOwnerNode()->GetName() == dst_name) {
          return true;
        }
      }
    }
  }
  return false;
}

class TestPreProcessST : public ::testing::Test {
 protected:
  void SetUp() override {
    ge::PlatformContext::GetInstance().Reset();
    auto stub_v1 = std::make_shared<RuntimeStub>();
    RuntimeStub::SetInstance(stub_v1);
    dlog_setlevel(ASCGEN_MODULE_NAME, DLOG_ERROR, 0);
  }

  void TearDown() override {
    ge::PlatformContext::GetInstance().Reset();
    dlog_setlevel(ASCGEN_MODULE_NAME, DLOG_ERROR, 0);
  }
};

}  // namespace

// ==================== Scalar 直连计算节点 → 插入 Broadcast ====================

TEST_F(TestPreProcessST, ScalarDirectToCompute_InsertsBroadcast) {
  auto graph = AscGraphBuilder("st_scalar_to_add")
    .Loops({Sym("s0"), Sym("s1")})
    .Data("data0", 0)
    .Data("data1", 1)
    .Load("load0", "data0")
    .Load("load1", "data1")
    .Scalar("scalar0", "1.0")
    .Add("add0", "load0", "scalar0")
    .Mul("mul0", "add0", "load1")
    .Store("store0", "mul0")
    .Output("output0", "store0")
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 1U);
  EXPECT_FALSE(HasEdge(graph, "scalar0", "add0"));
}

// ==================== 纯 Scalar 图（无 Load）→ 只插 Broadcast ====================

TEST_F(TestPreProcessST, PureScalarGraph_OnlyBroadcastInserted) {
  auto graph = AscGraphBuilder("st_pure_scalar")
    .Loops({Sym("s0")})
    .Scalar("s0", "1.0")
    .Scalar("s1", "2.0")
    .Add("add0", "s0", "s1")
    .Store("store0", "add0")
    .Output("output0", "store0")
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 2U);
  EXPECT_FALSE(HasEdge(graph, "s0", "add0"));
  EXPECT_FALSE(HasEdge(graph, "s1", "add0"));
}

// ==================== Scalar 多下游共享 → 一个 Broadcast ====================

TEST_F(TestPreProcessST, ScalarMultipleDownstreams_OneSharedBroadcast) {
  auto graph = AscGraphBuilder("st_scalar_multi_downstream")
    .Loops({Sym("s0")})
    .Data("data0", 0)
    .Data("data1", 1)
    .Load("load0", "data0")
    .Load("load1", "data1")
    .Scalar("scalar0", "1.0")
    .Add("add0", "load0", "scalar0")
    .Mul("mul0", "load1", "scalar0")
    .Store("store0", "add0")
    .Store("store1", "mul0")
    .Output("output0", "store0")
    .Output("output1", "store1", 1)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 1U);

  for (const auto &node : AscGraphUtils::GetComputeGraph(graph)->GetAllNodes()) {
    if (node->GetType() == Broadcast::Type) {
      auto out_anchor = node->GetOutDataAnchor(0);
      ASSERT_TRUE(out_anchor != nullptr);
      EXPECT_EQ(out_anchor->GetPeerInDataAnchors().size(), 2U);
      break;
    }
  }
}

// ==================== 多 Scalar 各自独立 Broadcast ====================

TEST_F(TestPreProcessST, MultipleScalars_EachGetsOwnBroadcast) {
  auto graph = AscGraphBuilder("st_multi_scalars")
    .Loops({Sym("s0")})
    .Data("data0", 0)
    .Load("load0", "data0")
    .Scalar("scalar0", "1.0")
    .Scalar("scalar1", "2.0")
    .Add("add0", "load0", "scalar0")
    .Mul("mul0", "load0", "scalar1")
    .Store("store0", "add0")
    .Store("store1", "mul0")
    .Output("output0", "store0")
    .Output("output1", "store1", 1)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 2U);
  EXPECT_FALSE(HasEdge(graph, "scalar0", "add0"));
  EXPECT_FALSE(HasEdge(graph, "scalar1", "mul0"));
}

// ==================== Scalar 已有 Broadcast → 不再插入 ====================

TEST_F(TestPreProcessST, ScalarAlreadyHasBroadcast_NoInsert) {
  auto graph = AscGraphBuilder("st_scalar_with_brc")
    .Loops({Sym("s0"), Sym("s1")})
    .Data("data0", 0)
    .Load("load0", "data0")
    .Scalar("scalar0", "1.0")
    .Broadcast("brc0", "scalar0", {Sym("s0"), Sym("s1")})
    .Add("add0", "load0", "brc0")
    .Store("store0", "add0")
    .Output("output0", "store0")
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 1U);
  EXPECT_TRUE(HasEdge(graph, "scalar0", "brc0"));
}

// ==================== 部分 Scalar 已有 Broadcast，部分没有 ====================

TEST_F(TestPreProcessST, MixedScalarOnlyInsertsForDirectOnes) {
  auto graph = AscGraphBuilder("st_mixed_scalar")
    .Loops({Sym("s0"), Sym("s1")})
    .Data("data0", 0)
    .Data("data1", 1)
    .Load("load0", "data0")
    .Load("load1", "data1")
    .Scalar("scalar0", "1.0")
    .Add("add0", "load0", "scalar0")
    .Scalar("scalar1", "2.0")
    .Broadcast("brc0", "scalar1", {Sym("s0"), Sym("s1")})
    .Mul("mul0", "load1", "brc0")
    .Store("store0", "add0")
    .Store("store1", "mul0")
    .Output("output0", "store0")
    .Output("output1", "store1", 1)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 2U);
  EXPECT_FALSE(HasEdge(graph, "scalar0", "add0"));
  EXPECT_TRUE(HasEdge(graph, "scalar1", "brc0"));
}

// ==================== 无 Scalar → 无变化 ====================

TEST_F(TestPreProcessST, NoScalarInGraph_NoChange) {
  auto graph = AscGraphBuilder("st_no_scalar")
    .Loops({Sym("s0"), Sym("s1")})
    .Data("data0", 0)
    .Data("data1", 1)
    .Load("load0", "data0")
    .Load("load1", "data1")
    .Add("add0", "load0", "load1")
    .Mul("mul0", "add0", "add0")
    .Store("store0", "mul0")
    .Output("output0", "store0")
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  auto node_count_before = AscGraphUtils::GetComputeGraph(graph)->GetAllNodes().size();

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  auto node_count_after = AscGraphUtils::GetComputeGraph(graph)->GetAllNodes().size();
  EXPECT_EQ(node_count_before, node_count_after);
  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 0U);
}

// ==================== Scalar 直连 Store → 插入 Broadcast ====================

TEST_F(TestPreProcessST, ScalarDirectToStore_InsertsBroadcast) {
  auto graph = AscGraphBuilder("st_scalar_to_store")
    .Loops({Sym("s0")})
    .Scalar("scalar0", "2.0")
    .Store("store0", "scalar0")
    .Output("output0", "store0")
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 1U);
  EXPECT_FALSE(HasEdge(graph, "scalar0", "store0"));
}

// ==================== fp16 Scalar → ImprovePrecision 提升为 fp32 后插 Broadcast ====================

TEST_F(TestPreProcessST, Fp16Scalar_UpcastThenInsertBroadcast) {
  auto graph = AscGraphBuilder("st_fp16_scalar")
    .Loops({Sym("s0")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Scalar("scalar0", "1.0", ge::DT_FLOAT16)
    .Add("add0", "load0", "scalar0")
    .Store("store0", "add0")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 1U);
  EXPECT_FALSE(HasEdge(graph, "scalar0", "add0"));

  for (const auto &node : AscGraphUtils::GetComputeGraph(graph)->GetAllNodes()) {
    if (node->GetType() == Broadcast::Type) {
      auto desc = node->GetOpDesc();
      ASSERT_TRUE(desc != nullptr);
      auto out_dtype = desc->MutableOutputDesc(0)->GetDataType();
      EXPECT_EQ(out_dtype, ge::DT_FLOAT);
      break;
    }
  }
}

// ==================== 多 dtype Scalar 混合 → 各自正确 ====================

TEST_F(TestPreProcessST, MultiDtypeScalars_EachBroadcastMatchesDtype) {
  auto graph = AscGraphBuilder("st_multi_dtype_scalars")
    .Loops({Sym("s0")})
    .Data("data0", 0)
    .Load("load0", "data0")
    .Scalar("fp16_s", "1.0", ge::DT_FLOAT16)
    .Scalar("bf16_s", "2.0", ge::DT_BF16)
    .Scalar("fp32_s", "3.0")
    .Mul("mul0", "load0", "fp16_s")
    .Add("add0", "mul0", "bf16_s")
    .Sub("sub0", "add0", "fp32_s")
    .Store("store0", "sub0")
    .Output("output0", "store0")
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 3U);

  for (const auto &node : AscGraphUtils::GetComputeGraph(graph)->GetAllNodes()) {
    if (node->GetType() == Broadcast::Type) {
      auto desc = node->GetOpDesc();
      ASSERT_TRUE(desc != nullptr);
      auto out_dtype = desc->MutableOutputDesc(0)->GetDataType();
      EXPECT_EQ(out_dtype, ge::DT_FLOAT);
    }
  }
}

// ==================== Scalar 直连 Output → 不插入 Broadcast ====================

TEST_F(TestPreProcessST, ScalarDirectToOutput_NoInsert) {
  auto graph = AscGraphBuilder("st_scalar_to_output")
    .Loops({Sym("s0")})
    .Data("data0", 0)
    .Load("load0", "data0")
    .Scalar("scalar0", "1.0")
    .Add("add0", "load0", "scalar0")
    .Store("store0", "add0")
    .Output("output0", "store0")
    .Output("output1", "scalar0", 1)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 1U);
  EXPECT_FALSE(HasEdge(graph, "scalar0", "add0"));
  EXPECT_TRUE(HasEdge(graph, "scalar0", "output1"));
}

// ==================== 3 轴 + 多 Scalar + fp16 端到端 ====================

TEST_F(TestPreProcessST, ThreeAxisMultiScalarFp16_EndToEnd) {
  auto graph = AscGraphBuilder("st_3axis_multi_scalar_fp16")
    .Loops({Sym("s0"), Sym("s1"), Sym("s2")})
    .Data("data0", 0, ge::DT_FLOAT16)
    .Data("data1", 1, ge::DT_FLOAT16)
    .Load("load0", "data0")
    .Load("load1", "data1")
    .Scalar("scale", "2.0", ge::DT_FLOAT16)
    .Scalar("bias", "1.0", ge::DT_FLOAT16)
    .Mul("mul0", "load0", "scale")
    .Add("add0", "mul0", "bias")
    .Add("add1", "add0", "load1")
    .Store("store0", "add1")
    .Output("output0", "store0", 0, ge::DT_FLOAT16)
    .Build();

  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);

  PreProcess preprocessor;
  ASSERT_EQ(preprocessor.Run(graph), ge::SUCCESS);

  EXPECT_EQ(CountNodesByType(graph, Broadcast::Type), 2U);
  EXPECT_FALSE(HasEdge(graph, "scale", "mul0"));
  EXPECT_FALSE(HasEdge(graph, "bias", "add0"));
}
