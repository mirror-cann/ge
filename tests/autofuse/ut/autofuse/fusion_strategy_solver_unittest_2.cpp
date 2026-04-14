/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in免root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "ge_graph_dsl/graph_dsl.h"
#include "common/ge_common/ge_types.h"
#include "attribute_group/attr_group_symbolic_desc.h"
#include "graph/symbolizer/symbolic.h"
#include "common/util/mem_utils.h"
#include "can_fuse/fusion_strategy_solver.h"
#include "can_fuse/backend/fusion_decider_registry.h"
#include "can_fuse/backend/asc_backend_fusion_decider.h"
#include "utils/autofuse_attrs.h"
#include "utils/autofuse_utils.h"
#include "graph/ascendc_ir/ascendc_ir_core/ascendc_ir.h"
#include "ascir_ops.h"
#include "graph/utils/node_utils.h"
#include "utils/auto_fuse_config.h"
#include "ascgen_log.h"
#include "autofuser.h"
#include "attribute_group/attr_group_shape_env.h"
#include "post_process/asc_backend_post_processor.h"
#include "lowering/op_helper/lower_concat_helper.h"
#include "can_fuse/graph_manager.h"
#include "op_creator_register.h"
#include "all_ops_cpp.h"
#include "esb_graph.h"

using namespace std;
using namespace testing;

namespace ge {
using namespace autofuse;

struct ReshapeAxes {
  Symbol ONE;
  Expression A, B, C, D, E;
  AxisId a, b, c, d, e;
  AxisId loop_axis;
};

static ReshapeAxes CreateReshapeAxes(ge::AscGraph &graph) {
  ReshapeAxes axes;
  axes.ONE = Symbol(1);
  axes.A = graph.CreateSizeVar("A");
  axes.B = graph.CreateSizeVar("B");
  axes.C = graph.CreateSizeVar("C");
  axes.D = graph.CreateSizeVar("D");
  axes.E = graph.CreateSizeVar("E");

  axes.a = graph.CreateAxis("A", axes.A).id;
  axes.b = graph.CreateAxis("B", axes.B).id;
  axes.c = graph.CreateAxis("C", axes.C).id;
  axes.d = graph.CreateAxis("D", axes.D).id;
  axes.e = graph.CreateAxis("E", axes.E).id;
  axes.loop_axis = axes.c;
  return axes;
}

static std::vector<int64_t> GetReshapeAxisIds(const ReshapeAxes &axes) {
  return {axes.a, axes.b, axes.c, axes.d, axes.e};
}

static std::shared_ptr<ge::AscGraph> CreateReshapeAscGraph(ge::AscGraph &graph) {
  auto axes = CreateReshapeAxes(graph);
  auto axis_ids = GetReshapeAxisIds(axes);

  ge::ascir_op::Data x1("data_reshape", graph);
  x1.attr.sched.axis = axis_ids;
  x1.attr.sched.loop_axis = axes.loop_axis;
  *x1.y.axis = axis_ids;
  *x1.y.repeats = {axes.A, axes.B, axes.C, axes.D, axes.E};
  *x1.y.strides = {axes.B * axes.C * axes.D * axes.E, axes.C * axes.D * axes.E,
                   axes.D * axes.E, axes.E, axes.ONE};

  ge::ascir_op::Load x1Local("load_reshape");
  x1Local.x = x1.y;
  x1Local.attr.sched.axis = axis_ids;
  *x1Local.y.axis = axis_ids;
  *x1Local.y.repeats = {axes.A, axes.B, axes.C, axes.D, axes.E};
  *x1Local.y.strides = {axes.B * axes.C * axes.D * axes.E, axes.C * axes.D * axes.E,
                        axes.D * axes.E, axes.E, axes.ONE};

  ge::ascir_op::Store x_store("store_reshape");
  x_store.x = x1Local.y;
  x_store.attr.sched.axis = axis_ids;
  x_store.attr.sched.loop_axis = axes.loop_axis;
  *x_store.y.axis = axis_ids;
  *x_store.y.repeats = {axes.A, axes.B, axes.C, axes.D, axes.E};
  *x_store.y.strides = {axes.B * axes.C * axes.D * axes.E, axes.C * axes.D * axes.E,
                        axes.D * axes.E, axes.E, axes.ONE};

  ge::ascir_op::Output x_out("out_reshape");
  x_out.x = x_store.y;
  x_out.attr.sched.axis = axis_ids;
  x_out.attr.sched.loop_axis = axes.loop_axis;
  *x_out.y.axis = axis_ids;
  *x_out.y.repeats = {axes.A, axes.B, axes.C, axes.D, axes.E};
  *x_out.y.strides = {axes.B * axes.C * axes.D * axes.E, axes.C * axes.D * axes.E,
                      axes.D * axes.E, axes.E, axes.ONE};

  auto x_out_node = graph.FindNode("out_reshape");
  auto compute_graph = x_out_node->GetOwnerComputeGraph();
  std::vector<std::pair<NodePtr, int32_t>> output_nodes = {{x_out_node, 0}};
  compute_graph->SetOutputSize(1U);
  compute_graph->SetGraphOutNodesInfo(output_nodes);
  return std::shared_ptr<ge::AscGraph>(new ge::AscGraph(graph));
}

class UtestFusionStrategySolverReshape : public testing::Test {
 public:
  static Status SetOutputSymbolicShape(const NodePtr &node) {
    for (const auto out_anchor : node->GetAllOutDataAnchorsPtr()) {
      GE_ASSERT_NOTNULL(out_anchor);
      const auto node_desc = node->GetOpDesc();
      GE_ASSERT_NOTNULL(node_desc);
      auto output_tensor_desc = node_desc->MutableOutputDesc(out_anchor->GetIdx());
      gert::SymbolShape symbol_shape({Symbol(1), Symbol(2), Symbol(3), Symbol(4)});
      output_tensor_desc->GetOrCreateAttrsGroup<SymbolicDescAttr>()->symbolic_tensor.MutableOriginSymbolShape() =
          symbol_shape;
    }
    return SUCCESS;
  }

  static Status SetInputSymbolicShape(const NodePtr &node) {
    for (const auto in_anchor : node->GetAllInDataAnchorsPtr()) {
      GE_ASSERT_NOTNULL(in_anchor);
      const auto node_desc = node->GetOpDesc();
      GE_ASSERT_NOTNULL(node_desc);
      auto input_tensor_desc = node_desc->MutableInputDesc(in_anchor->GetIdx());

      const auto &peer_out_anchor = in_anchor->GetPeerOutAnchor();
      if (peer_out_anchor == nullptr) {
        GELOGW("Node:%s in_anchor:%u peer_out_anchor is nullptr.", node->GetNamePtr(), in_anchor->GetIdx());
        continue;
      }
      const auto peer_node = peer_out_anchor->GetOwnerNodeBarePtr();
      GE_ASSERT_NOTNULL(peer_node);
      const auto peer_node_desc = peer_node->GetOpDesc();
      GE_ASSERT_NOTNULL(peer_node_desc);
      const auto output_tensor_desc = peer_node_desc->MutableOutputDesc(peer_out_anchor->GetIdx());
      GE_ASSERT_NOTNULL(output_tensor_desc);
      const auto attr_group = output_tensor_desc->GetOrCreateAttrsGroup<ge::SymbolicDescAttr>();
      GE_ASSERT_NOTNULL(attr_group);

      input_tensor_desc->GetOrCreateAttrsGroup<SymbolicDescAttr>()->symbolic_tensor.MutableOriginSymbolShape() =
          attr_group->symbolic_tensor.MutableOriginSymbolShape();
    }
    return SUCCESS;
  }

  static Status SetAscBackendOriginNames(const NodePtr &node) {
    if (node->GetType() == kAscBackendType) {
      std::vector<std::pair<std::string, int32_t>> origin_input_names;
      for (uint32_t i = 0; i < node->GetAllInDataAnchorsSize(); ++i) {
        origin_input_names.emplace_back("origin_input" + std::to_string(i), i);
      }
      std::vector<std::pair<std::string, int32_t>> origin_output_names;
      for (uint32_t i = 0; i < node->GetAllOutDataAnchorsSize(); ++i) {
        origin_output_names.emplace_back("origin_output" + std::to_string(i), i);
      }
      GetInterAttrs(GetOrCreateAutoFuseAttrs(node->GetOpDesc())).origin_input_names_ = origin_input_names;
      GetInterAttrs(GetOrCreateAutoFuseAttrs(node->GetOpDesc())).origin_output_names_ = origin_output_names;
    }
    return SUCCESS;
  }

  static Status SetAttrsGroup(const NodePtr &node) {
    auto op_desc = node->GetOpDescBarePtr();
    GE_ASSERT_NOTNULL(op_desc);
    auto attr = GetOrCreateAutoFuseAttrs(op_desc);
    GE_ASSERT_NOTNULL(attr);

    ge::AscGraph add_graph(node->GetName().c_str());
    if (node->GetName().find("Reshape") != std::string::npos) {
      attr->SetAscGraph(CreateReshapeAscGraph(add_graph), loop::FuseType::kReshape);
    } else if (node->GetName() == "A") {
      attr->SetAscGraph(CreateReshapeAscGraph(add_graph), loop::FuseType::kPointwise);
    }

    SetOutputSymbolicShape(node);
    SetInputSymbolicShape(node);
    SetAscBackendOriginNames(node);
    return SUCCESS;
  }

 protected:
  void SetUp() {
    RegisterAllOpCreator();
    dlog_setlevel(ASCGEN_MODULE_NAME, DLOG_ERROR, 0);
    setenv("ENABLE_LOWER_MATMUL", "true", 1);
  }
  void TearDown() {
    dlog_setlevel(ASCGEN_MODULE_NAME, DLOG_ERROR, 0);
  }
};

/*
 *     data
 *      |
 *   Reshape1
 *      |
 *   Reshape2
 *      |
 *   netoutput
 */
TEST_F(UtestFusionStrategySolverReshape, Not_Fuse_Reshape_And_Reshape) {
  class ReshapeFusionDecider : public AscBackendFusionDecider {
    NodePtr Fuse(const NodePtr &node1, const NodePtr &node2, const CounterPtr &counter) {
      return AscBackendFusionDecider::Fuse(node1, node2, counter);
    }
  };

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {1,2,3,4}).InCnt(0).OutCnt(1).InNames({"x"})
      .OutNames({"y"}).Build("data");
  auto reshape1 = OP_CFG(kAscBackendType).TensorDesc(FORMAT_ND, DT_FLOAT, {1,2,3,4}).InCnt(1).OutCnt(1).InNames({"x"})
      .OutNames({"y"}).Build("Reshape1");
  auto reshape2 = OP_CFG(kAscBackendType).TensorDesc(FORMAT_ND, DT_FLOAT, {1,2,3,4}).InCnt(1).OutCnt(1).InNames({"x"})
      .OutNames({"y"}).Build("Reshape2");
  DEF_GRAPH(g1) {
    CHAIN(NODE(data)->EDGE(0, 0)->NODE(reshape1)->EDGE(0, 0)->NODE(reshape2)->EDGE(0, 0)->NODE("NetOutput", kNetOutputType));
  };
  auto graph = ToComputeGraph(g1);
  for (const auto &node : graph->GetAllNodes()) {
    SetAttrsGroup(node);
  }
  const auto pre_nodes_size = graph->GetAllNodesSize();
  FusionDeciderRegistry::Instance().Register(std::unique_ptr<FusionDecider>(new ReshapeFusionDecider()));
  FusionStrategySolver fusion_strategy_solver;
  EXPECT_EQ(fusion_strategy_solver.Fuse(graph), SUCCESS);
  const auto post_nodes_size = graph->GetAllNodesSize();
  EXPECT_EQ(pre_nodes_size, post_nodes_size);
}

/*
 *     data
 *      |
 *   Reshape
 *      |
 *      A
 *      |
 *   netoutput
 */
TEST_F(UtestFusionStrategySolverReshape, Fuse_Reshape_And_Pointwise) {
  class ReshapeFusionDecider : public AscBackendFusionDecider {
    NodePtr Fuse(const NodePtr &node1, const NodePtr &node2, const CounterPtr &counter) {
      return AscBackendFusionDecider::Fuse(node1, node2, counter);
    }
  };

  auto data = OP_CFG("Data").TensorDesc(FORMAT_ND, DT_FLOAT, {1,2,3,4}).InCnt(0).OutCnt(1).InNames({"x"})
      .OutNames({"y"}).Build("data");
  auto reshape = OP_CFG(kAscBackendType).TensorDesc(FORMAT_ND, DT_FLOAT, {1,2,3,4}).InCnt(1).OutCnt(1).InNames({"x"})
      .OutNames({"y"}).Build("Reshape");
  auto a = OP_CFG(kAscBackendType).TensorDesc(FORMAT_ND, DT_FLOAT, {1,2,3,4}).InCnt(1).OutCnt(1).InNames({"x"})
      .OutNames({"y"}).Build("A");
  DEF_GRAPH(g1) {
    CHAIN(NODE(data)->EDGE(0, 0)->NODE(reshape)->EDGE(0, 0)->NODE(a)->EDGE(0, 0)->NODE("NetOutput", kNetOutputType));
  };
  auto graph = ToComputeGraph(g1);
  for (const auto &node : graph->GetAllNodes()) {
    SetAttrsGroup(node);
  }
  const auto pre_nodes_size = graph->GetAllNodesSize();
  FusionDeciderRegistry::Instance().Register(std::unique_ptr<FusionDecider>(new ReshapeFusionDecider()));
  FusionStrategySolver fusion_strategy_solver;
  EXPECT_EQ(fusion_strategy_solver.Fuse(graph), SUCCESS);
  const auto post_nodes_size = graph->GetAllNodesSize();
  EXPECT_EQ(pre_nodes_size - 1, post_nodes_size);
}

} // namespace ge
