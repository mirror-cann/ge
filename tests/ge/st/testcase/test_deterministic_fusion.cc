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

#include "graph/compute_graph.h"
#include "graph/ge_tensor.h"
#include "graph/op_desc.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/graph_utils.h"

namespace ge {
namespace {
constexpr const char *kDeterministicAttr = "_deterministic";
constexpr const char *kDeterministicLevelAttr = "_deterministic_level";

struct FusionGraph {
  ComputeGraphPtr graph;
  NodePtr relu1;
  NodePtr relu2;
};

OpDescPtr BuildOpDesc(const std::string &name, const std::string &type, const size_t input_num,
                      const size_t output_num) {
  auto tensor_desc = std::make_shared<GeTensorDesc>();
  tensor_desc->SetShape(GeShape({1}));
  tensor_desc->SetDataType(DT_FLOAT);
  tensor_desc->SetFormat(FORMAT_CHWN);

  auto op_desc = std::make_shared<OpDesc>(name, type);
  for (size_t i = 0U; i < input_num; ++i) {
    op_desc->AddInputDesc(tensor_desc->Clone());
  }
  for (size_t i = 0U; i < output_num; ++i) {
    op_desc->AddOutputDesc(tensor_desc->Clone());
  }
  return op_desc;
}

FusionGraph BuildFusionGraph() {
  auto graph = std::make_shared<ComputeGraph>("graph");
  const auto data = graph->AddNode(BuildOpDesc("Data", "Data", 0U, 1U));
  const auto relu1 = graph->AddNode(BuildOpDesc("Relu1", "Relu", 1U, 1U));
  const auto relu2 = graph->AddNode(BuildOpDesc("Relu2", "Relu", 1U, 1U));
  const auto netoutput = graph->AddNode(BuildOpDesc("Netoutput", "NetOutput", 1U, 0U));
  (void)GraphUtils::AddEdge(data->GetOutDataAnchor(0), relu1->GetInDataAnchor(0));
  (void)GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), relu2->GetInDataAnchor(0));
  (void)GraphUtils::AddEdge(relu2->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0));
  return {graph, relu1, relu2};
}

OpDescPtr BuildFusionOpDesc() {
  return BuildOpDesc("fuse_node", "Relu", 1U, 1U);
}

}  // namespace

class DeterministicFusionSTest : public testing::Test {};

TEST_F(DeterministicFusionSTest, FuseNodeKeepTopoInheritDeterministic) {
  auto fusion_graph = BuildFusionGraph();
  auto fusion_op = BuildFusionOpDesc();
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu1->GetOpDesc(), kDeterministicAttr, "1"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu2->GetOpDesc(), kDeterministicAttr, "1"));

  const auto fuse_nodes = fusion_graph.graph->FuseNodeKeepTopo({fusion_graph.relu1, fusion_graph.relu2}, {fusion_op});
  ASSERT_EQ(fuse_nodes.size(), 1U);
  std::string deterministic;
  EXPECT_TRUE(AttrUtils::GetStr(fusion_op, kDeterministicAttr, deterministic));
  EXPECT_EQ(deterministic, "1");
}

TEST_F(DeterministicFusionSTest, FuseNodeKeepTopoInheritDeterministicLevel) {
  auto fusion_graph = BuildFusionGraph();
  auto fusion_op = BuildFusionOpDesc();
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu1->GetOpDesc(), kDeterministicLevelAttr, "2"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu2->GetOpDesc(), kDeterministicLevelAttr, "2"));

  const auto fuse_nodes = fusion_graph.graph->FuseNodeKeepTopo({fusion_graph.relu1, fusion_graph.relu2}, {fusion_op});
  ASSERT_EQ(fuse_nodes.size(), 1U);
  std::string deterministic_level;
  EXPECT_TRUE(AttrUtils::GetStr(fusion_op, kDeterministicLevelAttr, deterministic_level));
  EXPECT_EQ(deterministic_level, "2");
}

TEST_F(DeterministicFusionSTest, FuseNodeKeepTopoFailedWhenDeterministicConflict) {
  auto fusion_graph = BuildFusionGraph();
  auto fusion_op = BuildFusionOpDesc();
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu1->GetOpDesc(), kDeterministicAttr, "0"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu2->GetOpDesc(), kDeterministicAttr, "1"));

  std::string not_support_reason;
  EXPECT_FALSE(fusion_graph.graph->IsSupportFuse({fusion_graph.relu1, fusion_graph.relu2}, not_support_reason));
  EXPECT_NE(not_support_reason.find(kDeterministicAttr), std::string::npos);
  const auto fuse_nodes = fusion_graph.graph->FuseNodeKeepTopo({fusion_graph.relu1, fusion_graph.relu2}, {fusion_op});
  EXPECT_TRUE(fuse_nodes.empty());
}

TEST_F(DeterministicFusionSTest, FuseNodeKeepTopoFailedWhenDeterministicLevelConflict) {
  auto fusion_graph = BuildFusionGraph();
  auto fusion_op = BuildFusionOpDesc();
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu1->GetOpDesc(), kDeterministicLevelAttr, "1"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu2->GetOpDesc(), kDeterministicLevelAttr, "2"));

  std::string not_support_reason;
  EXPECT_FALSE(fusion_graph.graph->IsSupportFuse({fusion_graph.relu1, fusion_graph.relu2}, not_support_reason));
  EXPECT_NE(not_support_reason.find(kDeterministicLevelAttr), std::string::npos);
  const auto fuse_nodes = fusion_graph.graph->FuseNodeKeepTopo({fusion_graph.relu1, fusion_graph.relu2}, {fusion_op});
  EXPECT_TRUE(fuse_nodes.empty());
}

TEST_F(DeterministicFusionSTest, FuseNodeKeepTopoFailedWhenDeterministicInvalid) {
  auto fusion_graph = BuildFusionGraph();
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu1->GetOpDesc(), kDeterministicAttr, "2"));

  std::string not_support_reason;
  EXPECT_FALSE(fusion_graph.graph->IsSupportFuse({fusion_graph.relu1, fusion_graph.relu2}, not_support_reason));
  EXPECT_NE(not_support_reason.find(kDeterministicAttr), std::string::npos);
}

TEST_F(DeterministicFusionSTest, FuseNodeKeepTopoFailedWhenDeterministicLevelInvalid) {
  auto fusion_graph = BuildFusionGraph();
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu1->GetOpDesc(), kDeterministicLevelAttr, "4"));

  std::string not_support_reason;
  EXPECT_FALSE(fusion_graph.graph->IsSupportFuse({fusion_graph.relu1, fusion_graph.relu2}, not_support_reason));
  EXPECT_NE(not_support_reason.find(kDeterministicLevelAttr), std::string::npos);
}

TEST_F(DeterministicFusionSTest, FuseNodeKeepTopoKeepSameAttrsOnFusionOp) {
  auto fusion_graph = BuildFusionGraph();
  auto fusion_op = BuildFusionOpDesc();
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu1->GetOpDesc(), kDeterministicAttr, "1"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu2->GetOpDesc(), kDeterministicAttr, "1"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu1->GetOpDesc(), kDeterministicLevelAttr, "3"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_graph.relu2->GetOpDesc(), kDeterministicLevelAttr, "3"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_op, kDeterministicAttr, "1"));
  ASSERT_TRUE(AttrUtils::SetStr(fusion_op, kDeterministicLevelAttr, "3"));

  const auto fuse_nodes = fusion_graph.graph->FuseNodeKeepTopo({fusion_graph.relu1, fusion_graph.relu2}, {fusion_op});
  ASSERT_EQ(fuse_nodes.size(), 1U);
}
}  // namespace ge
