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
#include <vector>

#include "macro_utils/dt_public_scope.h"
#include "graph/build/run_context_util.h"

#include "framework/common/util.h"
#include "framework/common/debug/ge_log.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/op_desc.h"
#include "graph/utils/tensor_utils.h"
#include "common/omg_util/omg_util.h"

#include "macro_utils/dt_public_unscope.h"

using namespace std;
using namespace testing;

namespace ge {

class UtestRunContext : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UtestRunContext, InitMemInfo) {
  RunContextUtil run_util;
  uint8_t *data_mem_base = nullptr;
  uint64_t data_mem_size = 1024;
  std::map<int64_t, uint8_t *> mem_type_to_data_mem_base;
  std::map<int64_t, uint64_t> mem_type_to_data_mem_size;
  uint8_t *weight_mem_base = nullptr;
  uint64_t weight_mem_size = 256;
  EXPECT_EQ(run_util.InitMemInfo(data_mem_base, data_mem_size, mem_type_to_data_mem_base, mem_type_to_data_mem_size,
                                 weight_mem_base, weight_mem_size),
            PARAM_INVALID);
  uint8_t buf1[1024] = {0};
  data_mem_base = buf1;
  EXPECT_EQ(run_util.InitMemInfo(data_mem_base, data_mem_size, mem_type_to_data_mem_base, mem_type_to_data_mem_size,
                                 weight_mem_base, weight_mem_size),
            PARAM_INVALID);
  uint8_t buf2[1024] = {0};
  weight_mem_base = buf2;
  EXPECT_EQ(run_util.InitMemInfo(data_mem_base, data_mem_size, mem_type_to_data_mem_base, mem_type_to_data_mem_size,
                                 weight_mem_base, weight_mem_size),
            PARAM_INVALID);
}

TEST_F(UtestRunContext, CreateRunContext) {
  RunContextUtil run_util;
  Model model("name", "1.1");
  ComputeGraphPtr graph = nullptr;
  Buffer buffer(1024);
  uint64_t session_id = 0;
  AttrUtils::SetInt(&model, ATTR_MODEL_STREAM_NUM, 10);
  EXPECT_EQ(run_util.CreateRunContext(model, graph, buffer, session_id), PARAM_INVALID);
}

TEST_F(UtestRunContext, CreateRunContextWithNotifySuccess) {
  RunContextUtil run_util;
  Model model("name", "1.1");

  AttrUtils::SetInt(&model, ATTR_MODEL_LABEL_NUM, 1);
  AttrUtils::SetInt(&model, ATTR_MODEL_NOTIFY_NUM, 1);
  AttrUtils::SetInt(&model, ATTR_MODEL_EVENT_NUM, 1);
  AttrUtils::SetInt(&model, ATTR_MODEL_STREAM_NUM, 1);
  ge::ComputeGraphPtr graph = make_shared<ge::ComputeGraph>("");
  Buffer buffer(1024);
  uint64_t session_id = 0;
  EXPECT_EQ(run_util.CreateRunContext(model, graph, buffer, session_id), SUCCESS);
}

TEST_F(UtestRunContext, GetOriginalType_NonFrameworkOp) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  std::string type;
  EXPECT_EQ(GetOriginalType(node, type), SUCCESS);
  EXPECT_EQ(type, "Add");
}

TEST_F(UtestRunContext, GetOriginalType_FrameworkOpWithoutAttr) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", FRAMEWORKOP);
  auto node = graph->AddNode(op_desc);
  std::string type;
  EXPECT_EQ(GetOriginalType(node, type), INTERNAL_ERROR);
}

TEST_F(UtestRunContext, GetOriginalType_FrameworkOpWithAttr) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", FRAMEWORKOP);
  AttrUtils::SetStr(op_desc, ATTR_NAME_FRAMEWORK_ORIGINAL_TYPE, "OriginalType");
  auto node = graph->AddNode(op_desc);
  std::string type;
  EXPECT_EQ(GetOriginalType(node, type), SUCCESS);
  EXPECT_EQ(type, "OriginalType");
}

TEST_F(UtestRunContext, SetStreamLabel_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  EXPECT_EQ(SetStreamLabel(node, "label1"), SUCCESS);
}

TEST_F(UtestRunContext, SetStreamLabel_NullNode) {
  NodePtr null_node = nullptr;
  EXPECT_EQ(SetStreamLabel(null_node, "label1"), PARAM_INVALID);
}

TEST_F(UtestRunContext, SetCycleEvent_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  EXPECT_EQ(SetCycleEvent(node), SUCCESS);
}

TEST_F(UtestRunContext, SetCycleEvent_NullNode) {
  NodePtr null_node = nullptr;
  EXPECT_EQ(SetCycleEvent(null_node), PARAM_INVALID);
}

TEST_F(UtestRunContext, SetActiveLabelList_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  std::vector<std::string> labels = {"label1", "label2"};
  EXPECT_EQ(SetActiveLabelList(node, labels), SUCCESS);
}

TEST_F(UtestRunContext, SetSwitchBranchNodeLabel_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  EXPECT_EQ(SetSwitchBranchNodeLabel(node, "branch1"), SUCCESS);
}

TEST_F(UtestRunContext, SetSwitchTrueBranchFlag_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  EXPECT_EQ(SetSwitchTrueBranchFlag(node, true), SUCCESS);
}

TEST_F(UtestRunContext, SetOriginalNodeName_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  EXPECT_EQ(SetOriginalNodeName(node, "orig_name"), SUCCESS);
}

TEST_F(UtestRunContext, SetCyclicDependenceFlag_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  EXPECT_EQ(SetCyclicDependenceFlag(node), SUCCESS);
}

TEST_F(UtestRunContext, SetNextIteration_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc1 = make_shared<OpDesc>("node1", "Merge");
  auto op_desc2 = make_shared<OpDesc>("node2", "NextIteration");
  auto node1 = graph->AddNode(op_desc1);
  auto node2 = graph->AddNode(op_desc2);
  EXPECT_EQ(SetNextIteration(node1, node2), SUCCESS);
}

TEST_F(UtestRunContext, SetNextIteration_NullNextNode) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc1 = make_shared<OpDesc>("node1", "Merge");
  auto node1 = graph->AddNode(op_desc1);
  NodePtr null_node = nullptr;
  EXPECT_EQ(SetNextIteration(node1, null_node), PARAM_INVALID);
}

TEST_F(UtestRunContext, AlignMemSize_Positive) {
  int64_t mem_size = 100;
  AlignMemSize(mem_size, 512);
  EXPECT_EQ(mem_size, 512);
}

TEST_F(UtestRunContext, AlignMemSize_AlreadyAligned) {
  int64_t mem_size = 512;
  AlignMemSize(mem_size, 512);
  EXPECT_EQ(mem_size, 512);
}

TEST_F(UtestRunContext, AlignMemSize_Zero) {
  int64_t mem_size = 0;
  AlignMemSize(mem_size, 512);
  EXPECT_EQ(mem_size, 0);
}

TEST_F(UtestRunContext, AlignMemSize_Negative) {
  int64_t mem_size = -1;
  AlignMemSize(mem_size, 512);
  EXPECT_EQ(mem_size, -1);
}

TEST_F(UtestRunContext, GetMemorySize_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "BufferPool");
  GeTensorDesc tensor_desc;
  TensorUtils::SetSize(tensor_desc, 100);
  op_desc->AddOutputDesc(tensor_desc);
  auto node = graph->AddNode(op_desc);
  int64_t output_size = 0;
  EXPECT_EQ(GetMemorySize(node, output_size), SUCCESS);
  EXPECT_EQ(output_size, 1536);
}

TEST_F(UtestRunContext, GetMemorySize_NoOutputDesc) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "BufferPool");
  auto node = graph->AddNode(op_desc);
  int64_t output_size = 0;
  EXPECT_EQ(GetMemorySize(node, output_size), PARAM_INVALID);
}

TEST_F(UtestRunContext, SetControlFlowGroup_Success) {
  auto graph = make_shared<ge::ComputeGraph>("test_graph");
  auto op_desc = make_shared<OpDesc>("test_node", "Add");
  auto node = graph->AddNode(op_desc);
  EXPECT_NO_THROW(SetControlFlowGroup(node, 1));
}

TEST_F(UtestRunContext, SetControlFlowGroup_NullNode) {
  NodePtr null_node = nullptr;
  EXPECT_NO_THROW(SetControlFlowGroup(null_node, 1));
}

TEST_F(UtestRunContext, InitMemInfo_NullDataBaseWithNonZeroSize_Failed) {
  RunContextUtil run_util;
  std::map<int64_t, uint8_t *> mem_type_base = {{0, nullptr}};
  std::map<int64_t, uint64_t> mem_type_size = {{0, 100}};
  EXPECT_NE(run_util.InitMemInfo(nullptr, 100, mem_type_base, mem_type_size, nullptr, 0), SUCCESS);
}

TEST_F(UtestRunContext, InitMemInfo_NullWeightBaseWithNonZeroSize_Failed) {
  RunContextUtil run_util;
  uint8_t data_base[100];
  std::map<int64_t, uint8_t *> mem_type_base = {{0, data_base}};
  std::map<int64_t, uint64_t> mem_type_size = {{0, 100}};
  EXPECT_NE(run_util.InitMemInfo(data_base, 100, mem_type_base, mem_type_size, nullptr, 100), SUCCESS);
}

TEST_F(UtestRunContext, InitMemInfo_EmptyMemTypeMaps_Failed) {
  RunContextUtil run_util;
  uint8_t data_base[100];
  std::map<int64_t, uint8_t *> mem_type_base;
  std::map<int64_t, uint64_t> mem_type_size;
  EXPECT_NE(run_util.InitMemInfo(data_base, 100, mem_type_base, mem_type_size, nullptr, 0), SUCCESS);
}

TEST_F(UtestRunContext, InitMemInfo_MismatchedMemTypeSizes_Failed) {
  RunContextUtil run_util;
  uint8_t data_base[100];
  std::map<int64_t, uint8_t *> mem_type_base = {{0, data_base}};
  std::map<int64_t, uint64_t> mem_type_size = {{0, 100}, {1, 200}};
  EXPECT_NE(run_util.InitMemInfo(data_base, 100, mem_type_base, mem_type_size, nullptr, 0), SUCCESS);
}

TEST_F(UtestRunContext, InitMemInfo_ZeroSizes_Success) {
  RunContextUtil run_util;
  uint8_t data_base[1];
  std::map<int64_t, uint8_t *> mem_type_base = {{0, data_base}};
  std::map<int64_t, uint64_t> mem_type_size = {{0, 0}};
  EXPECT_EQ(run_util.InitMemInfo(nullptr, 0, mem_type_base, mem_type_size, nullptr, 0), SUCCESS);
}

TEST_F(UtestRunContext, CreateRunContext_NullGraph_Failed) {
  RunContextUtil run_util;
  Model model;
  Buffer buffer;
  EXPECT_NE(run_util.CreateRunContext(model, nullptr, buffer, 1), SUCCESS);
}

TEST_F(UtestRunContext, CreateRunContext_NoNotifyNum_Failed) {
  RunContextUtil run_util;
  Model model;
  Buffer buffer;
  auto graph = std::make_shared<ComputeGraph>("test");
  EXPECT_NE(run_util.CreateRunContext(model, graph, buffer, 1), SUCCESS);
}

TEST_F(UtestRunContext, CreateRunContext_AllAttrs_Success) {
  RunContextUtil run_util;
  Model model;
  Buffer buffer;
  auto graph = std::make_shared<ComputeGraph>("test");
  AttrUtils::SetInt(&model, ATTR_MODEL_NOTIFY_NUM, 1);
  AttrUtils::SetInt(&model, ATTR_MODEL_EVENT_NUM, 1);
  AttrUtils::SetInt(&model, ATTR_MODEL_LABEL_NUM, 1);
  EXPECT_EQ(run_util.CreateRunContext(model, graph, buffer, 1), SUCCESS);
}

TEST_F(UtestRunContext, PrintMemInfo_NoCrash) {
  RunContextUtil run_util;
  EXPECT_NO_THROW(run_util.PrintMemInfo());
}
}  // namespace ge
