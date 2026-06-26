/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "fusion/autofuse_attrs.h"
#include "graph/ascendc_ir/utils/asc_graph_utils.h"
#include "graph/ascendc_ir/ascendc_ir_core/asc_graph_ge_bridge.h"
#include "graph/utils/op_desc_utils.h"
// Forward-declare ge::NodeUtilsEx to avoid include-guard collision with af::NodeUtilsEx from CANN headers.
namespace ge {
class NodeUtilsEx {
 public:
  static graphStatus SetNodeToOperator(Operator &op, const ConstNodePtr &node);
};
}  // namespace ge

namespace optimize::autoschedule {
std::string optimize::autoschedule::AxisGroup::ToString() const {
  std::ostringstream oss;
  oss << "x_group: ";
  for (const auto &value : x_group) {
    oss << value << " ";
  }
  oss << "; y_group: ";
  for (const auto &value : y_group) {
    oss << value << " ";
  }
  oss << "; r_group: ";
  for (const auto &value : r_group) {
    oss << value << " ";
  }
  oss << "; n_group: ";
  for (const auto &value : n_group) {
    oss << value << " ";
  }
  return oss.str();
}
}  // namespace optimize::autoschedule

static thread_local af::AscGraph *g_current_asc_graph = nullptr;

namespace af {
Axis &AscGraph::CreateAxis(const std::string &name, const Expression &size) {
  const auto serialized_expr = size.Serialize();
  const auto axis_id = AscGraphCreateAxisBySerializedExpr(*this, name.c_str(), serialized_expr.get());
  auto axis = FindAxis(axis_id);
  return *axis;
}

AscNodePtr AscGraph::AddNode(Operator &op) {
  g_current_asc_graph = this;
  auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
  GE_ASSERT_NOTNULL(op_desc);
  // Use AscGraphAddNodeFromOpDescRaw so libaihac_ir.so creates the AscNode from the
  // original op_desc (not a fresh one).  This ensures y.axis pointers (set during
  // AscOpOutput construction) point to the same AttrStore that autofuse reads later.
  // Pass raw pointer to avoid af::OpDescPtr vs ge::OpDescPtr mangling mismatch
  // caused by AUTOFUSE_USE_GE_METADEF.
  auto asc_node = AscGraphAddNodeFromOpDescRaw(*this, op_desc.get());
  GE_ASSERT_NOTNULL(asc_node);
  // AscGraphAddNodeFromOpDescRaw binds the AscNode to an internal temp Operator;
  // re-bind it to the caller's original op so op->GetNode() works correctly.
  (void)ge::NodeUtilsEx::SetNodeToOperator(op, asc_node);
  return asc_node;
}

AscNodeAttr *AscNodeAttr::Create(Operator &op) {
  return CreateImpl(op);
}

// Use bridge API to avoid symbol-mangling mismatch: with AUTOFUSE_USE_GE_METADEF,
// af::Operator becomes ge::Operator, so our symbol is GetOpDescFromOperator(ge::Operator&)
// instead of the GetOpDescFromOperator(af::Operator&) that libaihac_ir.so expects.
// The bridge runs inside libaihac_ir.so (compiled without the macro) with correct types.
AscNodeAttr *AscNodeAttr::CreateImpl(Operator &op) {
  auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
  GE_ASSERT_NOTNULL(op_desc);
  return AscNodeAttrGetOrCreateForOp(op_desc.get());
}

AscTensorAttr *AscTensorAttr::GetTensorAttrPtr(Operator *op, const uint32_t index) {
  GE_ASSERT_NOTNULL(op);
  // If op is already bound to an AscNode, use the AscNode's af::OpDesc so that
  // attr reads/writes are visible to autofuse internals (which go through AscNode).
  auto node = op->GetNode();
  if (node != nullptr) {
    auto node_op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(node_op_desc);
    return AscTensorAttrGetOrCreateForOpOutput(node_op_desc.get(), index);
  }
  // No AscNode yet (called during op constructor before AddNode).
  // Use ge::OpDesc; TryInitTensorAttr will be re-called after OutputRegister.
  auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(*op);
  GE_ASSERT_NOTNULL(op_desc);
  return AscTensorAttrGetOrCreateForOpOutput(op_desc.get(), index);
}

AscTensorAttr *AscTensorAttr::GetTensorAttrPtr(const OutDataAnchor &output) {
  auto owner_node = output.GetOwnerNode();
  GE_ASSERT_NOTNULL(owner_node);
  auto op_desc = owner_node->GetOpDesc();
  GE_ASSERT_NOTNULL(op_desc);
  auto tensor_desc = op_desc->MutableOutputDesc(output.GetIdx());
  GE_ASSERT_NOTNULL(tensor_desc);
  return tensor_desc->GetOrCreateAttrsGroup<AscTensorAttr>();
}

AscTensorAttr &AscTensorAttr::GetTensorAttr(Operator *op, const uint32_t index) {
  auto tensor_attr = GetTensorAttrPtr(op, index);
  return *tensor_attr;
}

AscTensorAttr &AscTensorAttr::GetTensorAttr(const OutDataAnchor &output) {
  auto tensor_attr = GetTensorAttrPtr(output);
  return *tensor_attr;
}

graphStatus LinkByIrIndex(const Operator &src_op, uint32_t src_ir_index, Operator &dst_op, uint32_t dst_ir_index,
                          uint32_t dynamic_index) {
  auto dst_op_desc = ge::OpDescUtils::GetOpDescFromOperator(dst_op);
  GE_ASSERT_NOTNULL(dst_op_desc);
  auto src_op_desc = ge::OpDescUtils::GetOpDescFromOperator(src_op);
  GE_ASSERT_NOTNULL(src_op_desc);

  // Resolve the actual in/out anchor indices (handling dynamic inputs/outputs).
  const auto &ir_inputs = dst_op_desc->GetIrInputs();
  const auto &ir_outputs = src_op_desc->GetIrOutputs();
  GE_ASSERT_TRUE(dst_ir_index < ir_inputs.size());

  uint32_t dst_index;
  if (ir_inputs[dst_ir_index].second == ge::IrInputType::kIrInputDynamic) {
    std::map<size_t, std::pair<size_t, size_t>> ir_input_2_range;
    (void)ge::OpDescUtils::GetIrInputInstanceDescRange(dst_op_desc, ir_input_2_range);
    dst_index = static_cast<uint32_t>(ir_input_2_range[dst_ir_index].first) + dynamic_index;
  } else {
    dst_index = dst_op_desc->MutableAllInputName()[ir_inputs[dst_ir_index].first];
  }

  uint32_t src_index;
  bool only_has_one_dynamic_output =
      (ir_outputs.size() == 1U && ir_outputs[0].second == ge::IrOutputType::kIrOutputDynamic);
  if (only_has_one_dynamic_output) {
    src_index = src_ir_index;
  } else {
    src_index = src_op_desc->MutableAllOutputName()[ir_outputs[src_ir_index].first];
  }

  auto src_node = src_op.GetNode();
  GE_ASSERT_NOTNULL(src_node);
  auto dst_node = dst_op.GetNode();
  if (dst_node == nullptr) {
    GE_ASSERT_NOTNULL(g_current_asc_graph);
    auto asc_node = AscGraphAddNodeFromOpDescRaw(*g_current_asc_graph, dst_op_desc.get());
    GE_ASSERT_NOTNULL(asc_node);
    (void)ge::NodeUtilsEx::SetNodeToOperator(dst_op, asc_node);
    dst_node = dst_op.GetNode();
    GE_ASSERT_NOTNULL(dst_node);
  }
  dst_op.SetInput(dst_index, src_op, src_index);
  return src_node->GetOutDataAnchor(src_index)->LinkTo(
      std::const_pointer_cast<Node>(dst_node)->GetInDataAnchor(dst_index));
}

graphStatus SetDynamicInputNumByIrIndex(Operator &op, uint32_t ir_index, uint32_t dynamic_num) {
  auto op_desc = ge::OpDescUtils::GetOpDescFromOperator(op);
  GE_ASSERT_NOTNULL(op_desc);
  const auto &ir_inputs = op_desc->GetIrInputs();
  GE_ASSERT_TRUE(ir_index < ir_inputs.size());
  GE_ASSERT_TRUE(ir_inputs[ir_index].second == ge::IrInputType::kIrInputDynamic);
  std::map<size_t, std::pair<size_t, size_t>> ir_input_2_range;
  (void)ge::OpDescUtils::GetIrInputInstanceDescRange(op_desc, ir_input_2_range);
  GE_ASSERT_TRUE(ir_input_2_range[ir_index].second < dynamic_num);
  op_desc->AddDynamicInputDescByIndex(ir_inputs[ir_index].first, dynamic_num, ir_input_2_range[ir_index].first);
  return GRAPH_SUCCESS;
}
}  // namespace af

extern "C" {
int32_t GenAscGraphAxisGroup(const af::AscGraph &graph, optimize::autoschedule::AxisGroup &axes_group) {
  axes_group.x_group.push_back(0);
  axes_group.y_group.push_back(0);
  axes_group.r_group.push_back(0);
  axes_group.n_group.push_back(0);
  for (const auto &node : af::AscGraphUtils::GetComputeGraph(graph)->GetAllNodes()) {
    auto attr = af::GetOrCreateAutoFuseAttrs(node->GetOpDesc());
    GE_ASSERT_NOTNULL(attr);
    af::GetInterAttrs(attr).axis_group = axes_group;
  }
  return 0;
}

bool CanMergeAxisGroup(const optimize::autoschedule::AxisGroup &lhs, const optimize::autoschedule::AxisGroup &rhs,
                       optimize::autoschedule::AxisGroup &merged_group) {
  merged_group = lhs;
  return true;
}
}
