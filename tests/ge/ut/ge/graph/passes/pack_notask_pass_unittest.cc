/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <string>
#include "graph/utils/op_desc_utils.h"
#include "graph/utils/node_adapter.h"
#include "graph/utils/tensor_utils.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph_builder_utils.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "rt_external_mem.h"
#include "graph/passes/memory_optimize/pack_notask_pass.h"

using namespace std;
using namespace ge;

class UtestPackNotaskPass : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

// 构建基本的 Pack 图: data -> relu -> pack
static Graph BuildPackGraph(Format storage_format, Format origin_format,
                            const std::vector<int64_t> &origin_shape,
                            const std::vector<int64_t> &shape) {
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_1");
    auto data2 = OP_DATA(DATA)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_2");
    auto relu1 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_1");
    relu1->MutableOutputDesc(0)->SetOriginFormat(origin_format);
    relu1->MutableOutputDesc(0)->SetFormat(storage_format);
    relu1->MutableOutputDesc(0)->SetOriginShape(GeShape(origin_shape));
    relu1->MutableOutputDesc(0)->SetShape(GeShape(shape));
    auto relu2 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_2");
    relu2->MutableOutputDesc(0)->SetOriginFormat(origin_format);
    relu2->MutableOutputDesc(0)->SetFormat(storage_format);
    relu2->MutableOutputDesc(0)->SetOriginShape(GeShape(origin_shape));
    relu2->MutableOutputDesc(0)->SetShape(GeShape(shape));
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(origin_format);
    pack->MutableInputDesc(0)->SetFormat(storage_format);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape(origin_shape));
    pack->MutableInputDesc(0)->SetShape(GeShape(shape));
    pack->MutableInputDesc(1)->SetOriginFormat(origin_format);
    pack->MutableInputDesc(1)->SetFormat(storage_format);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape(origin_shape));
    pack->MutableInputDesc(1)->SetShape(GeShape(shape));
    CHAIN(NODE(data1)->NODE(relu1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(data2)->NODE(relu2)->EDGE(0, 1)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  return ToGeGraph(g1);
}

// 构建上游为 Data 的 Pack 图（应不优化）
static Graph BuildPackGraphWithDataInput(Format storage_format, Format origin_format,
                                         const std::vector<int64_t> &origin_shape,
                                         const std::vector<int64_t> &shape) {
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(storage_format, DT_FLOAT16, shape).Build("data_1");
    data1->MutableOutputDesc(0)->SetOriginFormat(origin_format);
    data1->MutableOutputDesc(0)->SetOriginShape(GeShape(origin_shape));
    auto data2 = OP_DATA(DATA)
        .TensorDesc(storage_format, DT_FLOAT16, shape).Build("data_2");
    data2->MutableOutputDesc(0)->SetOriginFormat(origin_format);
    data2->MutableOutputDesc(0)->SetOriginShape(GeShape(origin_shape));
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(origin_format);
    pack->MutableInputDesc(0)->SetFormat(storage_format);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape(origin_shape));
    pack->MutableInputDesc(0)->SetShape(GeShape(shape));
    pack->MutableInputDesc(1)->SetOriginFormat(origin_format);
    pack->MutableInputDesc(1)->SetFormat(storage_format);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape(origin_shape));
    pack->MutableInputDesc(1)->SetShape(GeShape(shape));
    CHAIN(NODE(data1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(data2)->EDGE(0, 1)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  return ToGeGraph(g1);
}

// axis=0, ND格式, 应标记NoTask
TEST_F(UtestPackNotaskPass, pack_axis0_nd_success) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, true);

  // 验证上游 tensor 被锁定
  auto relu_node1 = compute_graph->FindNode("relu_1");
  auto relu_desc1 = relu_node1->GetOpDesc();
  bool can_reuse = true;
  auto output_tensor_desc = relu_desc1->MutableOutputDesc(0);
  ge::AttrUtils::GetBool(output_tensor_desc, "can_reused_for_pack_optimize", can_reuse);
  EXPECT_EQ(can_reuse, false);
}

// axis=1, dim0=1, 应标记NoTask
TEST_F(UtestPackNotaskPass, pack_axis1_dim0_is1_success) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {1, 12288}, {1, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 1);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, true);
}

// axis=1, dim0=2, 前轴非1，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_axis1_dim0_not1_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2, 12288}, {2, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 1);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游为 Data 节点，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_node_is_data_fail) {
  auto train_graph =
      BuildPackGraphWithDataInput(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 非 PackD 类型节点，不做处理
TEST_F(UtestPackNotaskPass, non_pack_node_skip) {
  // 构建一个只有 ConcatD 的图，PackNotaskPass 应该跳过
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_1");
    auto relu1 = OP_CFG(RELU).InCnt(1).OutCnt(1).Build("relu_1");
    auto concat = OP_CFG("ConcatD").InCnt(1).OutCnt(1).Build("concat");
    CHAIN(NODE(data1)->NODE(relu1)->NODE(concat));
    ADD_OUTPUT(concat, 0);
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto concat_node = compute_graph->FindNode("concat");
  auto op_desc = concat_node->GetOpDesc();

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// tensor 未 32B 对齐，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_tensor_not_aligned_fail) {
  // shape {3, 5} 的 FP16 tensor size = 3*5*2 = 30, 不是32的倍数
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {3, 5}, {3, 5});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// axis=-1（负轴），对 rank=2 转换后 pack_dim=2=rank，交错布局，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_negative_axis_equal_rank_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {1, 12288}, {1, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", -1);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// axis=-2（负轴），对 rank=2 转换后 pack_dim=1，dim0=1 应标记NoTask
TEST_F(UtestPackNotaskPass, pack_negative_axis_not_rank_success) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {1, 12288}, {1, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", -2);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, true);
}

// axis=rank（末尾插入新维度），交错布局，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_axis_equal_rank_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {1, 12288}, {1, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  // axis=2 == rank of {1, 12288}
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 2);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 构建单输入 Pack 图: data -> relu -> pack
static Graph BuildSingleInputPackGraph(Format storage_format, Format origin_format,
                                       const std::vector<int64_t> &origin_shape,
                                       const std::vector<int64_t> &shape) {
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_1");
    auto relu1 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_1");
    relu1->MutableOutputDesc(0)->SetOriginFormat(origin_format);
    relu1->MutableOutputDesc(0)->SetFormat(storage_format);
    relu1->MutableOutputDesc(0)->SetOriginShape(GeShape(origin_shape));
    relu1->MutableOutputDesc(0)->SetShape(GeShape(shape));
    auto pack = OP_CFG("Pack")
        .InCnt(1).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(origin_format);
    pack->MutableInputDesc(0)->SetFormat(storage_format);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape(origin_shape));
    pack->MutableInputDesc(0)->SetShape(GeShape(shape));
    CHAIN(NODE(data1)->NODE(relu1)->EDGE(0, 0)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  return ToGeGraph(g1);
}

// axis=rank，所有维度为1，单输入Pack，应标记NoTask
TEST_F(UtestPackNotaskPass, pack_axis_equal_rank_all_ones_success) {
  auto train_graph =
      BuildSingleInputPackGraph(FORMAT_ND, FORMAT_ND, {1, 1, 1}, {1, 1, 1});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  // axis=3 == rank of {1, 1, 1}
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 3);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, true);
}

// FRACTAL_NZ 私有格式, axis=0, 应标记NoTask
TEST_F(UtestPackNotaskPass, pack_format_fractal_nz_success) {
  auto train_graph =
      BuildPackGraph(FORMAT_FRACTAL_NZ, FORMAT_ND, {1, 2048}, {1, 128, 16});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, true);
}

// 主格式为 FRACTAL_NZ + c0_format 非 0 的复合 format（如 0x0500001D），
// 应按 primary format 归一判断，识别为 NZ 并放行
TEST_F(UtestPackNotaskPass, pack_format_fractal_nz_with_c0_success) {
  const auto composite_format = static_cast<Format>(
      ge::GetFormatFromC0(static_cast<int32_t>(FORMAT_FRACTAL_NZ), 5));
  auto train_graph =
      BuildPackGraph(composite_format, FORMAT_ND, {1, 2048}, {1, 128, 16});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, true);
}

// NC1HWC0 私有格式（非FRACTAL_NZ），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_format_nc1hwc0_reject) {
  auto train_graph =
      BuildPackGraph(FORMAT_NC1HWC0, FORMAT_NCHW, {1, 32, 16, 16}, {1, 2, 16, 16, 16});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// FRACTAL_Z 私有格式（非FRACTAL_NZ），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_format_fractal_z_reject) {
  auto train_graph =
      BuildPackGraph(FORMAT_FRACTAL_Z, FORMAT_NCHW, {1, 32, 16, 16}, {1, 2, 16, 16, 16});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 混合格式：一个输入ND，另一个NC1HWC0，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_format_mixed_internal_reject) {
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_1");
    auto data2 = OP_DATA(DATA)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_2");
    auto relu1 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_1");
    relu1->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    relu1->MutableOutputDesc(0)->SetShape(GeShape({2048, 12288}));
    auto relu2 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_2");
    relu2->MutableOutputDesc(0)->SetOriginFormat(FORMAT_NCHW);
    relu2->MutableOutputDesc(0)->SetFormat(FORMAT_NC1HWC0);
    relu2->MutableOutputDesc(0)->SetOriginShape(GeShape({1, 32, 16, 16}));
    relu2->MutableOutputDesc(0)->SetShape(GeShape({1, 2, 16, 16, 16}));
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(0)->SetShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetOriginFormat(FORMAT_NCHW);
    pack->MutableInputDesc(1)->SetFormat(FORMAT_NC1HWC0);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape({1, 32, 16, 16}));
    pack->MutableInputDesc(1)->SetShape(GeShape({1, 2, 16, 16, 16}));
    CHAIN(NODE(data1)->NODE(relu1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(data2)->NODE(relu2)->EDGE(0, 1)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// NCHW -> NC1HWC0 format 转换场景, NC1HWC0是私有格式且非FRACTAL_NZ, 不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_format_nchw_to_nc1hwc0_reject) {
  auto train_graph =
      BuildPackGraph(FORMAT_NC1HWC0, FORMAT_NCHW, {1, 32, 16, 16}, {1, 2, 16, 16, 16});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// SingleOp场景，直接跳过不做优化
TEST_F(UtestPackNotaskPass, pack_single_op_scene_skip) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  (void)ge::AttrUtils::SetBool(compute_graph, ATTR_SINGLE_OP_SCENE, true);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 内存不连续分配场景，跳过优化
TEST_F(UtestPackNotaskPass, pack_memory_discontinuous_skip) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  (void)ge::AttrUtils::SetBool(compute_graph, ge::ATTR_NAME_MEMORY_DISCONTIGUOUS_ALLOCATION, true);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// unknown shape 输入，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_unknown_shape_skip) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  // 将输入shape设为unknown (-1)
  op_desc->MutableInputDesc(0)->SetShape(GeShape({-1, 12288}));

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 所属图为unknown graph场景，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_owner_graph_unknown_skip) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  compute_graph->SetGraphUnknownFlag(true);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 前驱节点有continuous_input属性，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_node_continuous_input_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  // 给前驱节点设置continuous_input属性
  auto relu_node = compute_graph->FindNode("relu_1");
  (void)ge::AttrUtils::SetBool(relu_node->GetOpDesc(), ge::ATTR_NAME_CONTINUOUS_INPUT, true);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 前驱节点有atomic输出，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_node_atomic_output_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  auto relu_node = compute_graph->FindNode("relu_1");
  (void)ge::AttrUtils::SetListInt(relu_node->GetOpDesc(), ge::ATOMIC_ATTR_OUTPUT_INDEX, {0});

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 前驱节点已有notask属性，不应标记NoTask（级联阻断）
TEST_F(UtestPackNotaskPass, pack_pre_node_has_notask_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  auto relu_node = compute_graph->FindNode("relu_1");
  (void)ge::AttrUtils::SetBool(relu_node->GetOpDesc(), ge::ATTR_NAME_NOTASK, true);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 下游节点有notask属性，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_next_node_has_notask_fail) {
  // 构建 data -> relu -> pack -> relu_next 图，给 relu_next 设 notask
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_1");
    auto data2 = OP_DATA(DATA)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_2");
    auto relu1 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_1");
    relu1->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    relu1->MutableOutputDesc(0)->SetShape(GeShape({2048, 12288}));
    auto relu2 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_2");
    relu2->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu2->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu2->MutableOutputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    relu2->MutableOutputDesc(0)->SetShape(GeShape({2048, 12288}));
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(0)->SetShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetShape(GeShape({2048, 12288}));
    auto relu_next = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_next");
    CHAIN(NODE(data1)->NODE(relu1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(data2)->NODE(relu2)->EDGE(0, 1)->NODE(pack));
    CHAIN(NODE(pack)->NODE(relu_next));
    ADD_OUTPUT(relu_next, 0);
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto pack_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(pack_desc, "axis", 0);
  // 给下游节点设置notask属性
  auto relu_next_node = compute_graph->FindNode("relu_next");
  (void)ge::AttrUtils::SetBool(relu_next_node->GetOpDesc(), ge::ATTR_NAME_NOTASK, true);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(pack_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// LxFusion场景：节点名包含lxslice，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_lxfusion_op_skip) {
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_1");
    auto data2 = OP_DATA(DATA)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_2");
    auto relu1 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_1");
    relu1->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    relu1->MutableOutputDesc(0)->SetShape(GeShape({2048, 12288}));
    auto relu2 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_2");
    relu2->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu2->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu2->MutableOutputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    relu2->MutableOutputDesc(0)->SetShape(GeShape({2048, 12288}));
    // Pack 节点名含 lxslice 关键词
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack_lxslice_0");
    pack->MutableInputDesc(0)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(0)->SetShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetShape(GeShape({2048, 12288}));
    CHAIN(NODE(data1)->NODE(relu1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(data2)->NODE(relu2)->EDGE(0, 1)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack_lxslice_0");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// LxFusion场景：输入有L1/L2内存类型，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_lxfusion_mem_skip) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  // 设置输入内存类型为L1
  (void)ge::AttrUtils::SetListInt(op_desc, ge::ATTR_NAME_INPUT_MEM_TYPE_LIST,
      std::vector<uint32_t>{static_cast<uint32_t>(RT_MEMORY_L1)});

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 两个输入来自同一源anchor，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_same_source_anchor_fail) {
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_1");
    auto relu1 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_1");
    relu1->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    relu1->MutableOutputDesc(0)->SetShape(GeShape({2048, 12288}));
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(0)->SetShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetShape(GeShape({2048, 12288}));
    // 两个输入来自同一个relu1的output 0
    CHAIN(NODE(data1)->NODE(relu1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(relu1)->EDGE(0, 1)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// Scalar输入（0维tensor），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_scalar_input_fail) {
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {1}).Build("data_1");
    auto data2 = OP_DATA(DATA)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {1}).Build("data_2");
    auto relu1 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_1");
    relu1->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetOriginShape(GeShape(std::vector<int64_t>{}));
    relu1->MutableOutputDesc(0)->SetShape(GeShape(std::vector<int64_t>{}));
    auto relu2 = OP_CFG(RELU)
        .InCnt(1).OutCnt(1).Build("relu_2");
    relu2->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu2->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu2->MutableOutputDesc(0)->SetOriginShape(GeShape(std::vector<int64_t>{}));
    relu2->MutableOutputDesc(0)->SetShape(GeShape(std::vector<int64_t>{}));
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape(std::vector<int64_t>{}));
    pack->MutableInputDesc(0)->SetShape(GeShape(std::vector<int64_t>{}));
    pack->MutableInputDesc(1)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape(std::vector<int64_t>{}));
    pack->MutableInputDesc(1)->SetShape(GeShape(std::vector<int64_t>{}));
    CHAIN(NODE(data1)->NODE(relu1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(data2)->NODE(relu2)->EDGE(0, 1)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// NHWC 公有格式, axis=0, 应标记NoTask
TEST_F(UtestPackNotaskPass, pack_format_nhwc_public_success) {
  auto train_graph =
      BuildPackGraph(FORMAT_NHWC, FORMAT_NHWC, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, true);
}

// 上游为 Variable 节点，InputCheck::IsPreNodeTypeValid 拒绝
TEST_F(UtestPackNotaskPass, pack_pre_node_is_variable_fail) {
  DEF_GRAPH(g1) {
    auto var1 = OP_CFG(VARIABLE)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {2048, 12288})
        .InCnt(0).OutCnt(1).Build("var_1");
    auto var2 = OP_CFG(VARIABLE)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {2048, 12288})
        .InCnt(0).OutCnt(1).Build("var_2");
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(0)->SetShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetShape(GeShape({2048, 12288}));
    CHAIN(NODE(var1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(var2)->EDGE(0, 1)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游为 Constant 节点，InputCheck::IsPreNodeTypeValid 拒绝
TEST_F(UtestPackNotaskPass, pack_pre_node_is_const_fail) {
  DEF_GRAPH(g1) {
    auto const1 = OP_CFG(CONSTANT)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {2048, 12288})
        .InCnt(0).OutCnt(1).Build("const_1");
    auto const2 = OP_CFG(CONSTANT)
        .TensorDesc(FORMAT_ND, DT_FLOAT16, {2048, 12288})
        .InCnt(0).OutCnt(1).Build("const_2");
    auto pack = OP_CFG("Pack")
        .InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(0)->SetShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetShape(GeShape({2048, 12288}));
    CHAIN(NODE(const1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(const2)->EDGE(0, 1)->NODE(pack));
    ADD_OUTPUT(pack, 0);
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游节点含子图（IsPreNodeWithSubgraph），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_node_has_subgraph_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  auto relu_node = compute_graph->FindNode("relu_1");
  auto relu_desc = relu_node->GetOpDesc();
  (void)relu_desc->AddSubgraphName("branch");
  (void)relu_desc->SetSubgraphInstanceName(0, "body");

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游 tensor 已被 lock（can_reused_for_pack_optimize=false），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_tensor_locked_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  auto relu_node = compute_graph->FindNode("relu_1");
  auto output_tensor_desc = relu_node->GetOpDesc()->MutableOutputDesc(0);
  (void)ge::AttrUtils::SetBool(output_tensor_desc, "can_reused_for_pack_optimize", false);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游输出同时连接 Pack 和 NetOutput（IsPreOutAnchorValidMultiRef），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_output_connect_netoutput_fail) {
  DEF_GRAPH(g1) {
    auto data1 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_1");
    auto data2 = OP_DATA(DATA)
      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 12288}).Build("data_2");
    auto relu1 = OP_CFG(RELU).InCnt(1).OutCnt(1).Build("relu_1");
    relu1->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu1->MutableOutputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    relu1->MutableOutputDesc(0)->SetShape(GeShape({2048, 12288}));
    auto relu2 = OP_CFG(RELU).InCnt(1).OutCnt(1).Build("relu_2");
    relu2->MutableOutputDesc(0)->SetOriginFormat(FORMAT_ND);
    relu2->MutableOutputDesc(0)->SetFormat(FORMAT_ND);
    relu2->MutableOutputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    relu2->MutableOutputDesc(0)->SetShape(GeShape({2048, 12288}));
    auto pack = OP_CFG("Pack").InCnt(2).OutCnt(1).Build("pack");
    pack->MutableInputDesc(0)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(0)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(0)->SetShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetOriginFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetFormat(FORMAT_ND);
    pack->MutableInputDesc(1)->SetOriginShape(GeShape({2048, 12288}));
    pack->MutableInputDesc(1)->SetShape(GeShape({2048, 12288}));
    auto netoutput = OP_CFG(NETOUTPUT).InCnt(2).OutCnt(1).Build("output");
    // relu1 的 output 0 同时连到 pack.in0 和 netoutput.in0
    CHAIN(NODE(data1)->NODE(relu1)->EDGE(0, 0)->NODE(pack));
    CHAIN(NODE(relu1)->EDGE(0, 0)->NODE(netoutput));
    CHAIN(NODE(data2)->NODE(relu2)->EDGE(0, 1)->NODE(pack));
    CHAIN(NODE(pack)->EDGE(0, 1)->NODE(netoutput));
  };
  auto train_graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游节点有 continuous_output 属性，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_node_continuous_output_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  auto relu_node = compute_graph->FindNode("relu_1");
  (void)ge::AttrUtils::SetBool(relu_node->GetOpDesc(), ge::ATTR_NAME_CONTINUOUS_OUTPUT, true);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游节点有 reference 属性，不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_node_reference_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  auto relu_node = compute_graph->FindNode("relu_1");
  (void)ge::AttrUtils::SetBool(relu_node->GetOpDesc(), ge::ATTR_NAME_REFERENCE, true);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游节点已有 output_reuse_input 属性（虚拟算子标识），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_node_output_reuse_input_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  auto relu_node = compute_graph->FindNode("relu_1");
  (void)ge::AttrUtils::SetBool(relu_node->GetOpDesc(), ge::ATTR_NAME_OUTPUT_REUSE_INPUT, true);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 上游节点已有 nopadding_continuous_input 属性（虚拟算子标识），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_pre_node_nopadding_continuous_input_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);
  auto relu_node = compute_graph->FindNode("relu_1");
  (void)ge::AttrUtils::SetBool(relu_node->GetOpDesc(), ge::ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// FRACTAL_NZ 下 ori_shape[axis] 非 align_shape[axis] 倍数（D4 padding 不对齐），不应标记NoTask
// ori={1, 30} FP16 FRACTAL_NZ 会将 N=30 对齐到 32、M=1 对齐到 16；
// axis=1 走 CheckDimForInput，最终在 CheckDimAlignment 触发 30 % 32 != 0 失败。
TEST_F(UtestPackNotaskPass, pack_fractal_nz_padding_mismatch_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_FRACTAL_NZ, FORMAT_ND, {1, 30}, {1, 2, 16, 16});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 1);

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}

// 两个上游输出的 mem_type 不一致（一个带 special 类型，一个默认 HBM），不应标记NoTask
TEST_F(UtestPackNotaskPass, pack_input_mem_type_mismatch_fail) {
  auto train_graph =
      BuildPackGraph(FORMAT_ND, FORMAT_ND, {2048, 12288}, {2048, 12288});
  auto compute_graph = GraphUtilsEx::GetComputeGraph(train_graph);
  auto pack_node = compute_graph->FindNode("pack");
  auto op_desc = pack_node->GetOpDesc();
  (void)ge::AttrUtils::SetInt(op_desc, "axis", 0);

  // 仅 relu_1 的 output_mem_type_list 设成 special 类型(L2)，
  // relu_2 未设置即默认 HBM；IsSameInputMemType 的集合大小变为 2 → 拒绝。
  // LxFusionCheck 仅检查 Pack 自身的 input/output mem_type_list，不会被此设置触发。
  auto relu_node = compute_graph->FindNode("relu_1");
  (void)ge::AttrUtils::SetListInt(relu_node->GetOpDesc(), ge::ATTR_NAME_OUTPUT_MEM_TYPE_LIST,
      std::vector<int64_t>{static_cast<int64_t>(RT_MEMORY_L2)});

  PackNotaskPass pass;
  auto ret = pass.Run(compute_graph);
  EXPECT_EQ(ret, SUCCESS);
  bool notask = false;
  (void)ge::AttrUtils::GetBool(op_desc, ge::ATTR_NAME_NOTASK, notask);
  EXPECT_EQ(notask, false);
}
