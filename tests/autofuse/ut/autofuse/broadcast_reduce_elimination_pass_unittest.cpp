/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <limits>
#include <gtest/gtest.h>

#include "ge_tensor.h"
#include "easy_graph/builder/graph_dsl.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "ge_graph_dsl/op_desc/op_desc_cfg_box.h"

#include "all_ops_cpp.h"
#include "esb_funcs_cpp.h"
#include "esb_graph.h"
#include "graph_utils.h"
#include "graph_utils_ex.h"
#include "op_creator_register.h"
#include "pattern_fusion/broadcast_reduce_elimination_pass.h"
#include "graph_metadef/graph/debug/ge_util.h"

namespace ge {
class BroadcastReduceEliminationPassTest : public testing::Test {
 protected:
  void SetUp() override {
    dlog_setlevel(0, 3, 0);
    es_graph_ = std::unique_ptr<es::Graph>(new es::Graph("graph"));
    RegisterAllOpCreator();
  }

  void TearDown() override {
    dlog_setlevel(0, 3, 0);
  }

  template <typename T>
  static es::Tensor CreateConst(es::Graph &graph, ge::DataType dtype, const std::vector<int64_t> &dims,
                                std::vector<T> value) {
    auto result = es::FileConstant(graph, dims, dtype);
    GeTensorDesc desc(GeShape(dims), ge::FORMAT_ND, dtype);
    GeTensorPtr tensor =
        std::make_shared<GeTensor>(desc, reinterpret_cast<uint8_t *>(value.data()), sizeof(T) * value.size());
    AttrUtils::SetTensor(result.GetEsbTensor()->GetProducer()->GetOpDesc(), "value", tensor);
    result.GetEsbTensor()->GetProducer()->GetOpDesc()->SetType(ge::CONSTANT);
    return result;
  }

  struct BroadcastReduceGraph {
    ComputeGraphPtr graph;
    NodePtr data;
    NodePtr broadcast;
    NodePtr reduce;
    NodePtr output;
  };

  BroadcastReduceGraph BuildBroadcastReduceWithAxesInput(const es::Tensor &axes) {
    auto data = es_graph_->CreateInput(0, "data", nullptr);
    data.SetShape({10, 1});
    auto shape = CreateConst(*es_graph_, DT_INT64, {2}, std::vector<int64_t>{10, 5});
    auto broadcast = es::BroadcastTo(data, shape);
    broadcast.SetShape({10, 5});
    auto reduce = es::ReduceMax(broadcast, axes, true);
    reduce.SetShape({10, 1});
    auto relu = es::Relu(reduce);
    relu.SetShape({10, 1});
    es_graph_->SetOutput(relu, 0);

    auto graph = es_graph_->Build();
    auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph);
    auto broadcast_node = broadcast.GetEsbTensor()->GetProducer();
    GeTensorDesc input_desc(GeShape({10, 1}), FORMAT_ND, DT_FLOAT);
    EXPECT_EQ(broadcast_node->GetOpDesc()->UpdateInputDesc(0, input_desc), GRAPH_SUCCESS);
    return {compute_graph, data.GetEsbTensor()->GetProducer(), broadcast_node, reduce.GetEsbTensor()->GetProducer(),
            relu.GetEsbTensor()->GetProducer()};
  }

  std::unique_ptr<es::Graph> es_graph_;
};

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_EliminateSingleAxis) {
  // 测试场景：BroadcastTo + ReduceMax，相同轴完全抵消
  // 图结构：
  //   data [M, 1] -BroadcastTo-> [M, N] -ReduceMax(axis=1)-> [M, 1]
  // 预期：直接删除 BroadcastTo 和 ReduceMax，用原始 data 替换

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(0).OutCnt(1).Build("data");

  // BroadcastTo: [10, 1] -> [10, 5]
  auto broadcast = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(2).OutCnt(1).Build("broadcast");

  // ReduceMax: [10, 5] -> [10, 1], axis=1
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("reduce_max");

  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(broadcast)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 BroadcastTo 的输入 shape（输入是 [10, 1]，输出是 [10, 5]）
  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  GeShape input_shape({10, 1});
  GeTensorDesc input_desc(input_shape, FORMAT_ND, DT_FLOAT);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, input_desc);

  // 设置 BroadcastTo 的 shape 属性
  std::vector<int64_t> shape = {10, 5};
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", shape);

  // 设置 ReduceMax 的 axes 属性和 keep_dims 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> reduce_axes_1 = {1};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", reduce_axes_1);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证优化成功
  EXPECT_TRUE(changed);

  // 验证 BroadcastTo 和 ReduceMax 节点被删除
  EXPECT_EQ(graph->FindNode("broadcast"), nullptr);
  EXPECT_EQ(graph->FindNode("reduce_max"), nullptr);

  // 验证 data 直接连接到 relu
  auto data_node = graph->FindNode("data");
  auto relu_node = graph->FindNode("relu");
  ASSERT_NE(data_node, nullptr);
  ASSERT_NE(relu_node, nullptr);

  auto data_out_anchor = data_node->GetOutDataAnchor(0);
  auto relu_in_anchor = relu_node->GetInDataAnchor(0);
  EXPECT_EQ(data_out_anchor->GetPeerInDataAnchors().size(), 1);
  EXPECT_EQ(*data_out_anchor->GetPeerInDataAnchors().begin(), relu_in_anchor);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduce_NotEliminateWhenDifferentAxes) {
  // 测试场景：Broadcast 和 Reduce 在不同轴，不应优化
  // 图结构：
  //   data [M, 1] -BroadcastTo-> [M, N] -ReduceMax(axis=0)-> [N]
  // 预期：保留 BroadcastTo 和 ReduceMax

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(0).OutCnt(1).Build("data");

  auto broadcast = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(2).OutCnt(1).Build("broadcast");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {5}).InCnt(1).OutCnt(1).Build("reduce_max");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)->DATA_EDGE(0, 0)->NODE(broadcast)->DATA_EDGE(0, 0)->NODE(reduce_max)->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 BroadcastTo 的输入 shape
  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  GeShape input_shape({10, 1});
  GeTensorDesc input_desc(input_shape, FORMAT_ND, DT_FLOAT);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, input_desc);

  std::vector<int64_t> shape = {10, 5};
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", shape);

  // 设置 ReduceMax 的 axes 属性和 keep_dims 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> reduce_axes_0 = {0};  // axis=0，不是广播轴
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", reduce_axes_0);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", false);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证没有优化
  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("broadcast"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduce_NotEliminateWhenHasOtherConsumers) {
  // 测试场景：Broadcast 有多个消费者，不应优化
  // 图结构：
  //   data -BroadcastTo-> tmp1 -ReduceMax->
  //                      \
  //                       -> tmp2 -ReLU->
  // 预期：保留 BroadcastTo 和 ReduceMax（因为 Broadcast 有其他消费者）

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(0).OutCnt(1).Build("data");

  auto broadcast = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(2).OutCnt(1).Build("broadcast");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)->DATA_EDGE(0, 0)->NODE(broadcast)->DATA_EDGE(0, 0)->NODE(reduce_max)->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(broadcast)->DATA_EDGE(0, 0)->NODE(relu)->NODE("output_1", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 BroadcastTo 的输入 shape
  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  GeShape input_shape({10, 1});
  GeTensorDesc input_desc(input_shape, FORMAT_ND, DT_FLOAT);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, input_desc);

  std::vector<int64_t> shape = {10, 5};
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", shape);

  // 设置 ReduceMax 的 axes 属性和 keep_dims 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> reduce_axes_1 = {1};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", reduce_axes_1);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证没有优化（因为 Broadcast 有其他消费者）
  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("broadcast"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// 测试 keep_dims=false 的优化（需要插入 Squeeze 节点）
TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_EliminateWithSqueeze) {
  // 测试场景：BroadcastTo + ReduceMax (keep_dims=false)，需要插入 Squeeze 节点
  // 图结构：
  //   data [10, 1] -BroadcastTo-> [10, 5] -ReduceMax(axis=1, keep_dims=false)-> [10]
  // 预期：删除 BroadcastTo 和 ReduceMax，用 data + Squeeze(axis=1) 替换

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(0).OutCnt(1).Build("data");

  // BroadcastTo: [10, 1] -> [10, 5]
  auto broadcast = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(2).OutCnt(1).Build("broadcast");

  // ReduceMax: [10, 5] -> [10], axis=1, keep_dims=false
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {10}).InCnt(1).OutCnt(1).Build("reduce_max");

  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {10}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(broadcast)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 BroadcastTo 的输入 shape（输入是 [10, 1]，输出是 [10, 5]）
  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  GeShape input_shape({10, 1});
  GeTensorDesc input_desc(input_shape, FORMAT_ND, DT_FLOAT);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, input_desc);

  // 设置 BroadcastTo 的 shape 属性
  std::vector<int64_t> shape = {10, 5};
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", shape);

  // 设置 ReduceMax 的 axes 属性和 keep_dims=false 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> reduce_axes_1 = {1};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", reduce_axes_1);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", false);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证优化成功
  EXPECT_TRUE(changed);

  // 验证 BroadcastTo 和 ReduceMax 节点被删除
  EXPECT_EQ(graph->FindNode("broadcast"), nullptr);
  EXPECT_EQ(graph->FindNode("reduce_max"), nullptr);

  // 验证创建了 Reshape 节点
  auto reshape_node = graph->FindNode("data_reduce_max_reshape");
  ASSERT_NE(reshape_node, nullptr);
  EXPECT_EQ(reshape_node->GetType(), "Reshape");

  // 验证 data -> Reshape -> relu 的连接
  auto data_node = graph->FindNode("data");
  auto relu_node = graph->FindNode("relu");
  ASSERT_NE(data_node, nullptr);
  ASSERT_NE(relu_node, nullptr);

  auto reshape_out_anchor = reshape_node->GetOutDataAnchor(0);
  auto relu_in_anchor = relu_node->GetInDataAnchor(0);
  EXPECT_EQ(reshape_out_anchor->GetPeerInDataAnchors().size(), 1);
  EXPECT_EQ(*reshape_out_anchor->GetPeerInDataAnchors().begin(), relu_in_anchor);
}

// ========== Fill + Reduce 测试用例 ==========

// 测试 Fill + ReduceMax (keep_dims=true) 的优化
TEST_F(BroadcastReduceEliminationPassTest, FillReduceMax_EliminateWithKeepDims) {
  // 测试场景：Fill + ReduceMax，所有维度被归约
  // 图结构：
  //   dims [2] -> Fill -> [3, 4] -ReduceMax(axes=[0,1], keep_dims=true)-> [1, 1]
  //   value [1] -^
  // 预期：删除 Fill 和 ReduceMax，用 value 直接替换

  auto dims_data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_INT32, {2}).InCnt(0).OutCnt(1).Build("dims_data");
  auto value_data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {}).InCnt(0).OutCnt(1).Build("value_data");

  auto fill = OP_CFG("Fill").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 4}).InCnt(2).OutCnt(1).Build("fill");

  // ReduceMax: [3, 4] -> [1, 1], axes=[0,1], keep_dims=true
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 1}).InCnt(1).OutCnt(1).Build("reduce_max");

  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 1}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(dims_data)
              ->DATA_EDGE(0, 0)
              ->NODE(fill)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(value_data)->DATA_EDGE(0, 1)->NODE(fill));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 ReduceMax 的 axes 属性和 keep_dims 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> axes = {0, 1};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", axes);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证优化成功
  EXPECT_TRUE(changed);

  // 验证 Fill 和 ReduceMax 节点被删除
  EXPECT_EQ(graph->FindNode("fill"), nullptr);
  EXPECT_EQ(graph->FindNode("reduce_max"), nullptr);

  // 验证创建了 Reshape 节点（value 是 scalar，reduce 输出是 [1, 1]）
  auto reshape_node = graph->FindNode("value_data_reduce_max_reshape");
  ASSERT_NE(reshape_node, nullptr);
  EXPECT_EQ(reshape_node->GetType(), "Reshape");

  // 验证 value_data -> Reshape -> relu 的连接
  auto value_data_node = graph->FindNode("value_data");
  auto relu_node = graph->FindNode("relu");
  ASSERT_NE(value_data_node, nullptr);
  ASSERT_NE(relu_node, nullptr);

  auto reshape_out_anchor = reshape_node->GetOutDataAnchor(0);
  auto relu_in_anchor = relu_node->GetInDataAnchor(0);
  EXPECT_EQ(reshape_out_anchor->GetPeerInDataAnchors().size(), 1);
  EXPECT_EQ(*reshape_out_anchor->GetPeerInDataAnchors().begin(), relu_in_anchor);
}

// 测试 Fill + ReduceMax (keep_dims=false) 的优化
TEST_F(BroadcastReduceEliminationPassTest, FillReduceMax_EliminateWithoutKeepDims) {
  // 测试场景：Fill + ReduceMax，keep_dims=false
  // 图结构：
  //   dims [2] -> Fill -> [3, 4] -ReduceMax(axes=[0,1], keep_dims=false)-> []
  //   value [1] -^
  // 预期：删除 Fill 和 ReduceMax，用 value 直接替换（scalar 输出）

  auto dims_data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_INT32, {2}).InCnt(0).OutCnt(1).Build("dims_data");
  auto value_data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {}).InCnt(0).OutCnt(1).Build("value_data");

  auto fill = OP_CFG("Fill").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 4}).InCnt(2).OutCnt(1).Build("fill");

  // ReduceMax: [3, 4] -> [], axes=[0,1], keep_dims=false
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {}).InCnt(1).OutCnt(1).Build("reduce_max");

  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(dims_data)
              ->DATA_EDGE(0, 0)
              ->NODE(fill)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(value_data)->DATA_EDGE(0, 1)->NODE(fill));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 ReduceMax 的 axes 属性和 keep_dims=false 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> axes = {0, 1};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", axes);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", false);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证优化成功
  EXPECT_TRUE(changed);

  // 验证 Fill 和 ReduceMax 节点被删除
  EXPECT_EQ(graph->FindNode("fill"), nullptr);
  EXPECT_EQ(graph->FindNode("reduce_max"), nullptr);

  // 验证 value_data 直接连接到 relu
  auto value_data_node = graph->FindNode("value_data");
  auto relu_node = graph->FindNode("relu");
  ASSERT_NE(value_data_node, nullptr);
  ASSERT_NE(relu_node, nullptr);

  auto value_out_anchor = value_data_node->GetOutDataAnchor(0);
  auto relu_in_anchor = relu_node->GetInDataAnchor(0);
  EXPECT_EQ(value_out_anchor->GetPeerInDataAnchors().size(), 1);
  EXPECT_EQ(*value_out_anchor->GetPeerInDataAnchors().begin(), relu_in_anchor);
}

// ========== Tile + Reduce 测试用例 ==========

// 测试 Tile + ReduceMax (keep_dims=true) 的优化
TEST_F(BroadcastReduceEliminationPassTest, TileReduceMax_EliminateWithKeepDims) {
  // 测试场景：Tile + ReduceMax，广播轴被完全归约
  // 图结构：
  //   data [1, 5] -Tile(multiples=[3, 1])-> [3, 5] -ReduceMax(axes=[0], keep_dims=true)-> [1, 5]
  // 预期：删除 Tile 和 ReduceMax，用 data 直接替换

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(0).OutCnt(1).Build("data");

  // Tile: [1, 5] -> [3, 5], multiples=[3, 1]
  auto tile = OP_CFG("Tile").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 5}).InCnt(2).OutCnt(1).Build("tile");

  // ReduceMax: [3, 5] -> [1, 5], axes=[0], keep_dims=true
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(1).OutCnt(1).Build("reduce_max");

  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(tile)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 Tile 的 multiples 属性和输入 shape
  auto tile_node = graph->FindNode("tile");
  ASSERT_NE(tile_node, nullptr);

  // 设置 Tile 的输入 shape（输入是 [1, 5]，输出是 [3, 5]）
  GeShape tile_input_shape({1, 5});
  GeTensorDesc tile_input_desc(tile_input_shape, FORMAT_ND, DT_FLOAT);
  tile_node->GetOpDesc()->UpdateInputDesc(0, tile_input_desc);

  std::vector<int64_t> multiples = {3, 1};
  AttrUtils::SetListInt(tile_node->GetOpDesc(), "multiples", multiples);

  // 设置 ReduceMax 的 axes 属性和 keep_dims 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> axes = {0};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", axes);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证优化成功
  EXPECT_TRUE(changed);

  // 验证 Tile 和 ReduceMax 节点被删除
  EXPECT_EQ(graph->FindNode("tile"), nullptr);
  EXPECT_EQ(graph->FindNode("reduce_max"), nullptr);

  // 验证 data 直接连接到 relu
  auto data_node = graph->FindNode("data");
  auto relu_node = graph->FindNode("relu");
  ASSERT_NE(data_node, nullptr);
  ASSERT_NE(relu_node, nullptr);

  auto data_out_anchor = data_node->GetOutDataAnchor(0);
  auto relu_in_anchor = relu_node->GetInDataAnchor(0);
  EXPECT_EQ(data_out_anchor->GetPeerInDataAnchors().size(), 1);
  EXPECT_EQ(*data_out_anchor->GetPeerInDataAnchors().begin(), relu_in_anchor);
}

// 测试 Tile + ReduceMax (keep_dims=false) 的优化
TEST_F(BroadcastReduceEliminationPassTest, TileReduceMax_EliminateWithoutKeepDims) {
  // 测试场景：Tile + ReduceMax，keep_dims=false
  // 图结构：
  //   data [1, 5] -Tile(multiples=[3, 1])-> [3, 5] -ReduceMax(axes=[0], keep_dims=false)-> [5]
  // 预期：删除 Tile 和 ReduceMax，用 data + Squeeze(axis=0) 替换

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(0).OutCnt(1).Build("data");

  // Tile: [1, 5] -> [3, 5], multiples=[3, 1]
  auto tile = OP_CFG("Tile").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 5}).InCnt(2).OutCnt(1).Build("tile");

  // ReduceMax: [3, 5] -> [5], axes=[0], keep_dims=false
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {5}).InCnt(1).OutCnt(1).Build("reduce_max");

  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {5}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(tile)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 Tile 的 multiples 属性和输入 shape
  auto tile_node = graph->FindNode("tile");
  ASSERT_NE(tile_node, nullptr);

  // 设置 Tile 的输入 shape（输入是 [1, 5]，输出是 [3, 5]）
  GeShape tile_input_shape({1, 5});
  GeTensorDesc tile_input_desc(tile_input_shape, FORMAT_ND, DT_FLOAT);
  tile_node->GetOpDesc()->UpdateInputDesc(0, tile_input_desc);

  std::vector<int64_t> multiples = {3, 1};
  AttrUtils::SetListInt(tile_node->GetOpDesc(), "multiples", multiples);

  // 设置 ReduceMax 的 axes 属性和 keep_dims=false 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> axes = {0};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", axes);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", false);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证优化成功
  EXPECT_TRUE(changed);

  // 验证 Tile 和 ReduceMax 节点被删除
  EXPECT_EQ(graph->FindNode("tile"), nullptr);
  EXPECT_EQ(graph->FindNode("reduce_max"), nullptr);

  // 验证创建了 Reshape 节点
  auto reshape_node = graph->FindNode("data_reduce_max_reshape");
  ASSERT_NE(reshape_node, nullptr);
  EXPECT_EQ(reshape_node->GetType(), "Reshape");
}

// 测试 Tile 输入维度不为1的情况（不应优化）
TEST_F(BroadcastReduceEliminationPassTest, TileReduce_NoBroadcastWhenInputDimNotOne) {
  // 测试场景：Tile + Reduce，但 Tile 的输入维度不是1
  // 图结构：
  //   data [2, 3] -Tile(multiples=[2, 1])-> [4, 3] -ReduceSum(axes=[0], keep_dims=true)-> [1, 3]
  // 预期：不优化（因为 Tile 的输入维度不是1，不是广播操作）

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {2, 3}).InCnt(0).OutCnt(1).Build("data");

  // Tile: [2, 3] -> [4, 3], multiples=[2, 1]
  // 输入轴0是2（不是1），所以这不是广播操作，而是复制操作
  auto tile = OP_CFG("Tile").TensorDesc(FORMAT_ND, DT_FLOAT, {4, 3}).InCnt(2).OutCnt(1).Build("tile");

  // ReduceMax: [4, 3] -> [1, 3], axes=[0], keep_dims=true
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 3}).InCnt(1).OutCnt(1).Build("reduce_max");

  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 3}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(tile)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 Tile 的输入 shape 和 multiples 属性
  auto tile_node = graph->FindNode("tile");
  ASSERT_NE(tile_node, nullptr);
  GeShape tile_input_shape({2, 3});
  GeTensorDesc tile_input_desc(tile_input_shape, FORMAT_ND, DT_FLOAT);
  tile_node->GetOpDesc()->UpdateInputDesc(0, tile_input_desc);
  std::vector<int64_t> multiples = {2, 1};
  AttrUtils::SetListInt(tile_node->GetOpDesc(), "multiples", multiples);

  // 设置 ReduceMax 的 axes 属性和 keep_dims 属性
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> axes = {0};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", axes);
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证没有优化（因为输入维度不是1，Tile 不是广播操作）
  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("tile"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// 测试负轴索引的优化
TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_NegativeAxisEliminate) {
  // 测试场景：BroadcastTo + ReduceMax，使用负轴索引
  // 图结构：
  //   data [10, 1] -BroadcastTo-> [10, 5] -ReduceMax(axis=-1)-> [10, 1]
  // 预期：正确处理负轴，消除广播操作

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(0).OutCnt(1).Build("data");

  // BroadcastTo: [10, 1] -> [10, 5]
  auto broadcast = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(2).OutCnt(1).Build("broadcast");

  // ReduceMax: [10, 5] -> [10, 1], axis=-1 (等价于 axis=1)
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("reduce_max");

  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(broadcast)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 BroadcastTo 的输入 shape
  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  GeShape input_shape({10, 1});
  GeTensorDesc input_desc(input_shape, FORMAT_ND, DT_FLOAT);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, input_desc);

  std::vector<int64_t> shape = {10, 5};
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", shape);

  // 设置 ReduceMax 的 axes 属性为负值
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  std::vector<int64_t> reduce_axes_neg1 = {-1};
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", reduce_axes_neg1);  // 负轴索引
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  // 运行 Pass
  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 验证优化成功（负轴应被正确处理）
  EXPECT_TRUE(changed);
  EXPECT_EQ(graph->FindNode("broadcast"), nullptr);
  EXPECT_EQ(graph->FindNode("reduce_max"), nullptr);
}

// ========== 补充覆盖测试用例 ==========

// Bug 回归：BroadcastTo 链 + ReduceMax，reduce 覆盖了非广播轴
// data [128] -> BroadcastTo1 [128,128] -> BroadcastTo2 [256,128,128] -> ReduceMax(axes=[0,2]) -> [128]
// BroadcastTo2 的广播轴只有 axis 0，但 reduce 还覆盖了 axis 2（真实数据），不能消除
TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduce_NotEliminateWhenReduceCoversNonBroadcastAxis) {
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {128}).InCnt(0).OutCnt(1).Build("data");
  auto brc1 = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {128, 128}).InCnt(2).OutCnt(1).Build("brc1");
  auto brc2 = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {256, 128, 128}).InCnt(2).OutCnt(1).Build("brc2");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {128}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {128}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(brc1)
              ->DATA_EDGE(0, 0)
              ->NODE(brc2)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // BroadcastTo1: [128] -> [128, 128], brc_axes = {0}
  auto brc1_node = graph->FindNode("brc1");
  ASSERT_NE(brc1_node, nullptr);
  brc1_node->GetOpDesc()->UpdateInputDesc(0, GeTensorDesc(GeShape({128}), FORMAT_ND, DT_FLOAT));
  AttrUtils::SetListInt(brc1_node->GetOpDesc(), "shape", std::vector<int64_t>{128, 128});

  // BroadcastTo2: [128, 128] -> [256, 128, 128], brc_axes = {0}
  auto brc2_node = graph->FindNode("brc2");
  ASSERT_NE(brc2_node, nullptr);
  brc2_node->GetOpDesc()->UpdateInputDesc(0, GeTensorDesc(GeShape({128, 128}), FORMAT_ND, DT_FLOAT));
  AttrUtils::SetListInt(brc2_node->GetOpDesc(), "shape", std::vector<int64_t>{256, 128, 128});

  // ReduceMax: axes=[0, 2], keep_dims=false -> [128]
  // brc_axes={0} != reduce_axes={0,2} -> 不应消除
  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{0, 2});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", false);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);
  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("brc2"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// Bug 回归：单 BroadcastTo + ReduceMax，reduce 轴包含非广播轴
// data [10, 1, 5] -> BroadcastTo [10, 4, 5] (brc_axes={1}) -> ReduceMax(axes=[0,1], keep_dims=true) -> [1, 1, 5]
// axis 0 不是广播轴，不能消除
TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduce_NotEliminateWhenReduceIncludesNonBroadcastAxis) {
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1, 5}).InCnt(0).OutCnt(1).Build("data");
  auto broadcast =
      OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 4, 5}).InCnt(2).OutCnt(1).Build("broadcast");
  auto reduce_max =
      OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 1, 5}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 1, 5}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(broadcast)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, GeTensorDesc(GeShape({10, 1, 5}), FORMAT_ND, DT_FLOAT));
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", std::vector<int64_t>{10, 4, 5});

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{0, 1});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);
  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("broadcast"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// Bug 回归：广播轴未被完全 reduce，残留广播维度
// data [3, 1, 1] -> BroadcastTo [3, 4, 5] (brc_axes={1,2}) -> ReduceMax(axes=[1], keep_dims=true) -> [3, 1, 5]
// axis 2 是广播轴但未被 reduce，输出仍含广播维度，不能消除
TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduce_NotEliminateWhenPartialBroadcastCovered) {
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 1, 1}).InCnt(0).OutCnt(1).Build("data");
  auto broadcast =
      OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 4, 5}).InCnt(2).OutCnt(1).Build("broadcast");
  auto reduce_max =
      OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 1, 5}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 1, 5}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(broadcast)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, GeTensorDesc(GeShape({3, 1, 1}), FORMAT_ND, DT_FLOAT));
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", std::vector<int64_t>{3, 4, 5});

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{1});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);
  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("broadcast"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// 测试带控制边的消除：验证控制边迁移到替换节点
TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduce_EliminateWithControlEdges) {
  // 图结构：
  //   ctrl_data -ctrl-> broadcast
  //   ctrl_data -ctrl-> reduce_max
  //   data -data-> broadcast -data-> reduce_max -data-> relu
  // 预期：消除后控制边迁移到 data 节点
  auto ctrl_data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10}).InCnt(0).OutCnt(1).Build("ctrl_data");
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(0).OutCnt(1).Build("data");
  auto broadcast = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(2).OutCnt(1).Build("broadcast");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(broadcast)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(ctrl_data)->NODE("output_1", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  // 设置 BroadcastTo 输入 shape
  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  GeShape input_shape({10, 1});
  GeTensorDesc input_desc(input_shape, FORMAT_ND, DT_FLOAT);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, input_desc);
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", std::vector<int64_t>{10, 5});

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{1});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  // 添加控制边：ctrl_data -> broadcast, ctrl_data -> reduce_max
  auto ctrl_node = graph->FindNode("ctrl_data");
  ASSERT_NE(ctrl_node, nullptr);
  ASSERT_EQ(GraphUtils::AddEdge(ctrl_node->GetOutControlAnchor(), broadcast_node->GetInControlAnchor()), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(ctrl_node->GetOutControlAnchor(), reduce_max_node->GetInControlAnchor()),
            GRAPH_SUCCESS);
  EXPECT_EQ(broadcast_node->GetInControlNodesSize(), 1);
  EXPECT_EQ(reduce_max_node->GetInControlNodesSize(), 1);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  EXPECT_TRUE(changed);
  EXPECT_EQ(graph->FindNode("broadcast"), nullptr);
  EXPECT_EQ(graph->FindNode("reduce_max"), nullptr);

  // 验证控制边迁移到 data 节点
  auto data_node = graph->FindNode("data");
  ASSERT_NE(data_node, nullptr);
  EXPECT_EQ(data_node->GetInControlNodesSize(), 2);
}

// 测试动态 shape（输入含 -1）被拒绝
TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduce_DynamicShape_NoOptimize) {
  // 图结构：
  //   data [-1, 1] -BroadcastTo-> [10, 5] -ReduceMax(axes=[1])-> [10, 1]
  // 输入 shape 含 -1，不应优化
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {-1, 1}).InCnt(0).OutCnt(1).Build("data");
  auto broadcast = OP_CFG("BroadcastTo").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(2).OutCnt(1).Build("broadcast");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(broadcast)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  auto broadcast_node = graph->FindNode("broadcast");
  ASSERT_NE(broadcast_node, nullptr);
  GeShape input_shape({-1, 1});
  GeTensorDesc input_desc(input_shape, FORMAT_ND, DT_FLOAT);
  broadcast_node->GetOpDesc()->UpdateInputDesc(0, input_desc);
  AttrUtils::SetListInt(broadcast_node->GetOpDesc(), "shape", std::vector<int64_t>{10, 5});

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{1});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 动态输入 shape → GetBroadcastToAxes 返回 false → 不优化
  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("broadcast"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// 测试 Tile 动态 multiples（含 -1）被拒绝
TEST_F(BroadcastReduceEliminationPassTest, Tile_DynamicMultiples_NoOptimize) {
  // 图结构：
  //   data [1, 5] -Tile(multiples=[-1, 1])-> [?, 5] -ReduceMax(axes=[0])-> [1, 5]
  // multiples 含 -1，不应优化
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(0).OutCnt(1).Build("data");
  auto tile = OP_CFG("Tile").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 5}).InCnt(2).OutCnt(1).Build("tile");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(tile)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  auto tile_node = graph->FindNode("tile");
  ASSERT_NE(tile_node, nullptr);
  GeShape tile_input_shape({1, 5});
  GeTensorDesc tile_input_desc(tile_input_shape, FORMAT_ND, DT_FLOAT);
  tile_node->GetOpDesc()->UpdateInputDesc(0, tile_input_desc);
  AttrUtils::SetListInt(tile_node->GetOpDesc(), "multiples", std::vector<int64_t>{-1, 1});

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{0});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("tile"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// 测试 Tile 动态输入 shape（含 -1）被拒绝
TEST_F(BroadcastReduceEliminationPassTest, Tile_DynamicInputShape_NoOptimize) {
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(0).OutCnt(1).Build("data");
  auto tile = OP_CFG("Tile").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 5}).InCnt(2).OutCnt(1).Build("tile");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(tile)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  auto tile_node = graph->FindNode("tile");
  ASSERT_NE(tile_node, nullptr);
  GeShape tile_input_shape({-1, 5});
  GeTensorDesc tile_input_desc(tile_input_shape, FORMAT_ND, DT_FLOAT);
  tile_node->GetOpDesc()->UpdateInputDesc(0, tile_input_desc);
  AttrUtils::SetListInt(tile_node->GetOpDesc(), "multiples", std::vector<int64_t>{3, 1});

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{0});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("tile"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// 测试 Fill value 输入含动态 shape（-1）被拒绝
TEST_F(BroadcastReduceEliminationPassTest, Fill_DynamicValueShape_NoOptimize) {
  // 图结构：
  //   dims [2] -> Fill -> [3, 4] -ReduceMax(axes=[0,1])-> [1, 1]
  //   value [-1] -^
  // Fill 的 value 输入 shape 含 -1，不应优化
  auto dims_data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_INT32, {2}).InCnt(0).OutCnt(1).Build("dims_data");
  auto value_data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {-1}).InCnt(0).OutCnt(1).Build("value_data");
  auto fill = OP_CFG("Fill").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 4}).InCnt(2).OutCnt(1).Build("fill");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 1}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 1}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(dims_data)
              ->DATA_EDGE(0, 0)
              ->NODE(fill)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
    CHAIN(NODE(value_data)->DATA_EDGE(0, 1)->NODE(fill));
  };

  auto graph = ToComputeGraph(test_graph);

  // 更新 Fill 的 value 输入 desc 使其包含 -1
  auto fill_node = graph->FindNode("fill");
  ASSERT_NE(fill_node, nullptr);
  GeTensorDesc value_desc(GeShape({-1}), FORMAT_ND, DT_FLOAT);
  fill_node->GetOpDesc()->UpdateInputDesc(1, value_desc);

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{0, 1});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("fill"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// 测试 Tile multiples 维度数与输入 shape 维度数不匹配
TEST_F(BroadcastReduceEliminationPassTest, Tile_MultiplesSizeMismatch_NoOptimize) {
  // 图结构：
  //   data [1, 5] -Tile(multiples=[3])（维度数不匹配）-> ... -ReduceMax-> ...
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(0).OutCnt(1).Build("data");
  auto tile = OP_CFG("Tile").TensorDesc(FORMAT_ND, DT_FLOAT, {3, 5}).InCnt(2).OutCnt(1).Build("tile");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {1, 5}).InCnt(1).OutCnt(1).Build("relu");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)
              ->DATA_EDGE(0, 0)
              ->NODE(tile)
              ->DATA_EDGE(0, 0)
              ->NODE(reduce_max)
              ->NODE(relu)
              ->NODE("output_0", NETOUTPUT));
  };

  auto graph = ToComputeGraph(test_graph);

  auto tile_node = graph->FindNode("tile");
  ASSERT_NE(tile_node, nullptr);
  GeShape tile_input_shape({1, 5});
  GeTensorDesc tile_input_desc(tile_input_shape, FORMAT_ND, DT_FLOAT);
  tile_node->GetOpDesc()->UpdateInputDesc(0, tile_input_desc);
  // multiples 只有 1 个元素，但输入是 2 维 → size mismatch
  AttrUtils::SetListInt(tile_node->GetOpDesc(), "multiples", std::vector<int64_t>{3});

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{0});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("tile"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

// 测试 ReduceMax 输入不是广播类型时被跳过
TEST_F(BroadcastReduceEliminationPassTest, ReduceMax_NonBroadcastInput_NoOptimize) {
  // 图结构：data -> ReLU -> ReduceMax
  // ReLU 不是广播类型，应跳过
  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(0).OutCnt(1).Build("data");
  auto relu = OP_CFG("ReLU").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 5}).InCnt(1).OutCnt(1).Build("relu");
  auto reduce_max = OP_CFG("ReduceMax").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(1).Build("reduce_max");
  auto netoutput = OP_CFG("NetOutput").TensorDesc(FORMAT_ND, DT_FLOAT, {10, 1}).InCnt(1).OutCnt(0).Build("output_0");

  DEF_GRAPH(test_graph) {
    CHAIN(NODE(data)->NODE(relu)->DATA_EDGE(0, 0)->NODE(reduce_max)->NODE(netoutput));
  };

  auto graph = ToComputeGraph(test_graph);

  auto reduce_max_node = graph->FindNode("reduce_max");
  ASSERT_NE(reduce_max_node, nullptr);
  AttrUtils::SetListInt(reduce_max_node->GetOpDesc(), "axes", std::vector<int64_t>{1});
  AttrUtils::SetBool(reduce_max_node->GetOpDesc(), "keep_dims", true);

  bool changed = false;
  EXPECT_EQ(BroadcastReduceEliminationPass().Run(graph, changed), GRAPH_SUCCESS);

  // 输入不是广播类型 → 跳过
  EXPECT_FALSE(changed);
  EXPECT_NE(graph->FindNode("relu"), nullptr);
  EXPECT_NE(graph->FindNode("reduce_max"), nullptr);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_EliminateWithInt32AxesInput) {
  auto axes = CreateConst(*es_graph_, DT_INT32, {1}, std::vector<int32_t>{1});
  const auto nodes = BuildBroadcastReduceWithAxesInput(axes);

  bool changed = false;
  ASSERT_EQ(BroadcastReduceEliminationPass().Run(nodes.graph, changed), GRAPH_SUCCESS);

  EXPECT_TRUE(changed);
  EXPECT_EQ(nodes.graph->FindNode(nodes.broadcast->GetNamePtr()), nullptr);
  EXPECT_EQ(nodes.graph->FindNode(nodes.reduce->GetNamePtr()), nullptr);
  EXPECT_EQ(nodes.output->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.data);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_EliminateWithScalarInt64AxesInput) {
  auto axes = CreateConst(*es_graph_, DT_INT64, {}, std::vector<int64_t>{1});
  const auto nodes = BuildBroadcastReduceWithAxesInput(axes);

  bool changed = false;
  ASSERT_EQ(BroadcastReduceEliminationPass().Run(nodes.graph, changed), GRAPH_SUCCESS);

  EXPECT_TRUE(changed);
  EXPECT_EQ(nodes.graph->FindNode(nodes.broadcast->GetNamePtr()), nullptr);
  EXPECT_EQ(nodes.graph->FindNode(nodes.reduce->GetNamePtr()), nullptr);
  EXPECT_EQ(nodes.output->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.data);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_KeepWhenAxesInputIsNotConstant) {
  auto axes = es_graph_->CreateInput(1, "axes", nullptr);
  axes.SetShape({1});
  const auto nodes = BuildBroadcastReduceWithAxesInput(axes);

  bool changed = false;
  ASSERT_EQ(BroadcastReduceEliminationPass().Run(nodes.graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(nodes.graph->FindNode(nodes.broadcast->GetNamePtr()), nullptr);
  EXPECT_NE(nodes.graph->FindNode(nodes.reduce->GetNamePtr()), nullptr);
  EXPECT_EQ(nodes.reduce->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.broadcast);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_KeepWhenAxesInputDataIsNull) {
  auto axes = es::FileConstant(*es_graph_, {1}, DT_INT32);
  GeTensorDesc axes_desc(GeShape({1}), FORMAT_ND, DT_INT32);
  uint8_t axes_data = 0U;
  auto aligned_ptr = AlignedPtr::BuildFromData(&axes_data, [](const uint8_t *) {});
  ASSERT_NE(aligned_ptr, nullptr);
  // Make the deep-copy allocation fail without requesting a large buffer, leaving Tensor::GetData() null.
  GeTensor axes_tensor(axes_desc, aligned_ptr, std::numeric_limits<size_t>::max());
  auto axes_op_desc = axes.GetEsbTensor()->GetProducer()->GetOpDesc();
  ASSERT_TRUE(AttrUtils::SetShareTensor(axes_op_desc, "value", axes_tensor));
  axes_op_desc->SetType(CONSTANT);
  const auto nodes = BuildBroadcastReduceWithAxesInput(axes);

  bool changed = false;
  ASSERT_EQ(BroadcastReduceEliminationPass().Run(nodes.graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(nodes.graph->FindNode(nodes.broadcast->GetNamePtr()), nullptr);
  EXPECT_NE(nodes.graph->FindNode(nodes.reduce->GetNamePtr()), nullptr);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_KeepWhenAxesInputIsTwoDimensional) {
  auto axes = CreateConst(*es_graph_, DT_INT32, {1, 1}, std::vector<int32_t>{1});
  const auto nodes = BuildBroadcastReduceWithAxesInput(axes);

  bool changed = false;
  ASSERT_EQ(BroadcastReduceEliminationPass().Run(nodes.graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(nodes.graph->FindNode(nodes.broadcast->GetNamePtr()), nullptr);
  EXPECT_NE(nodes.graph->FindNode(nodes.reduce->GetNamePtr()), nullptr);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_KeepWhenAxesInputDtypeIsUnsupported) {
  auto axes = CreateConst(*es_graph_, DT_FLOAT, {1}, std::vector<float>{1.0F});
  const auto nodes = BuildBroadcastReduceWithAxesInput(axes);

  bool changed = false;
  ASSERT_EQ(BroadcastReduceEliminationPass().Run(nodes.graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(nodes.graph->FindNode(nodes.broadcast->GetNamePtr()), nullptr);
  EXPECT_NE(nodes.graph->FindNode(nodes.reduce->GetNamePtr()), nullptr);
}

TEST_F(BroadcastReduceEliminationPassTest, BroadcastReduceMax_KeepWhenAxesInputIsEmpty) {
  auto axes = CreateConst(*es_graph_, DT_INT32, {0}, std::vector<int32_t>{0});
  const auto nodes = BuildBroadcastReduceWithAxesInput(axes);

  bool changed = false;
  ASSERT_EQ(BroadcastReduceEliminationPass().Run(nodes.graph, changed), GRAPH_SUCCESS);

  EXPECT_FALSE(changed);
  EXPECT_NE(nodes.graph->FindNode(nodes.broadcast->GetNamePtr()), nullptr);
  EXPECT_NE(nodes.graph->FindNode(nodes.reduce->GetNamePtr()), nullptr);
}

}  // namespace ge
