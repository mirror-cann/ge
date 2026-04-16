/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/passes/memory_optimize/notask_pass_base.h"
#include "graph/utils/node_utils.h"
#include "runtime/mem.h"
#include "graph/utils/type_utils.h"
#include "common/memory/mem_type_utils.h"
#include "common/checker.h"
#include "graph/utils/graph_utils.h"

namespace ge {
Status NotaskPassBase::Run(ComputeGraphPtr graph) {
  GE_CHECK_NOTNULL(graph);
  if (ShouldSkipGraph(graph)) {
    return SUCCESS;
  }

  for (const auto &node : graph->GetDirectNode()) {
    const auto op_desc = node->GetOpDesc();
    GE_CHECK_NOTNULL(op_desc);
    if (IsTargetOp(op_desc)) {
      RunOnTargetNode(node);
    }
  }
  return SUCCESS;
}

bool NotaskPassBase::ShouldSkipGraph(const ComputeGraphPtr &graph) const {
  if (ge::GraphUtils::IsSingleOpScene(graph)) {
    GELOGI("Single op scene has no need to do %s optimize.", GetOpLabel().c_str());
    return true;
  }

  bool is_memory_discontinuous = false;
  (void)ge::AttrUtils::GetBool(graph, ge::ATTR_NAME_MEMORY_DISCONTIGUOUS_ALLOCATION, is_memory_discontinuous);
  if (is_memory_discontinuous) {
    GELOGI("memory discontinuous scene has no need to do %s optimize.", GetOpLabel().c_str());
    return true;
  }
  return false;
}

void NotaskPassBase::RunOnTargetNode(const ge::NodePtr &node) {
  const auto op_desc = node->GetOpDesc();
  GELOGI("%s node [%s] start notask check.", GetOpLabel().c_str(), node->GetName().c_str());
  cur_pro_node_name_ = node->GetName();

  if (IsUnknownShapeOp(op_desc)) {
    GELOGI("%s node [%s] is unknown shape op.", GetOpLabel().c_str(), node->GetName().c_str());
  } else if (IsOwnerGraphUnknown(node)) {
    GELOGI("%s node [%s] is belong to unknown graph.", GetOpLabel().c_str(), node->GetName().c_str());
  } else if (!InputCheck(node)) {
    GELOGI("%s node [%s] input does not meet the conditions.", GetOpLabel().c_str(), node->GetName().c_str());
  } else if (!CheckFormat(op_desc)) {
    GELOGI("%s node [%s] format does not meet the conditions.", GetOpLabel().c_str(), node->GetName().c_str());
  } else if (!CheckDim(op_desc)) {
    GELOGI("%s node [%s] dim does not meet the conditions.", GetOpLabel().c_str(), node->GetName().c_str());
  } else if (!OutputCheck(node)) {
    GELOGI("%s node [%s] output does not meet the conditions.", GetOpLabel().c_str(), node->GetName().c_str());
  } else if (!LxFusionCheck(node)) {
    GELOGI("%s node [%s] lxFusion does not meet the conditions.", GetOpLabel().c_str(), node->GetName().c_str());
  } else {
    SetNotaskAttr(node);
  }
}

constexpr int32_t NOTASK_TENSOR_ALIGN_SIZE = 32;
const int32_t kNotaskDeepth = 100;
const std::string kNotaskLxSlice = "lxslice";

bool NotaskPassBase::CheckDimAlignment(const ge::OpDescPtr &op_desc, const gert::Shape &align_shape,
  const int64_t dim, const ge::GeShape &ori_shape) const {
  GE_ASSERT_TRUE(!(ori_shape.GetDimNum() <= static_cast<size_t>(dim) ||
    align_shape.GetDimNum() <= static_cast<size_t>(dim) || align_shape[dim] <= 0),
    "notask [%s] dim %lld, ori shape size %zu, align shape size %zu, dim value %lld.",
      op_desc->GetName().c_str(), dim, ori_shape.GetDimNum(), align_shape.GetDimNum(), align_shape[dim]);
  if ((ori_shape.GetDim(dim) % align_shape[dim]) != 0) {
    GELOGD("notask [%s] dim %lld, ori shape %lld, align shape %lld.",
      op_desc->GetName().c_str(), dim, ori_shape.GetDim(dim), align_shape[dim]);
    return false;
  }
  return true;
}

void NotaskPassBase::PrintTransferDims(const std::string name,
  const std::vector<std::vector<int32_t>> &transfer_dims) const {
  std::stringstream ss;
  ss << "{";
  for (size_t i = 0; i < transfer_dims.size(); i++) {
    ss << "{";
    for (size_t j = 0; j < transfer_dims[i].size(); j++) {
      ss << transfer_dims[i][j];
      if (j != transfer_dims[i].size() - 1) {
        ss << ",";
      }
    }
    ss << "}";
    if (i != transfer_dims.size() - 1) {
    ss << ",";
  }
  }
  ss << "}";
  GELOGI("[%s]: %s", name.c_str(), ss.str().c_str());
}

void NotaskPassBase::PrintShape(const std::string name, const gert::Shape &shape) const {
  std::stringstream ss;
  ss << "{";
  for (size_t i = 0; i < shape.GetDimNum(); i++) {
    ss << shape[i];
    if (i != shape.GetDimNum() - 1) {
      ss << ",";
    }
  }
  ss << "}";
  GELOGI("[%s]: %s", name.c_str(), ss.str().c_str());
}

bool NotaskPassBase::CheckSplitAxis(const std::vector<int32_t> &src_axes, const int64_t &axis_idx,
  const int32_t &from_axis, const gert::Shape &align_shape, const gert::Shape &src_shape) const {
  const auto out = src_axes[0];
  if (out == axis_idx) {
    return src_shape.GetDim(from_axis) <= align_shape.GetDim(from_axis);
  } else {
    return align_shape.GetDim(from_axis) == 1;
  }
}

bool NotaskPassBase::IsFromAxisOne(const int64_t &axis_idx,
  const transformer::AxisIndexMapping &axis_index_mapping,
  const gert::Shape &align_shape, const gert::Shape &src_shape, const int32_t &from_axis) const {
  GE_ASSERT_TRUE(axis_index_mapping.src_to_dst_transfer_dims.size() > static_cast<size_t>(from_axis));
  if (axis_index_mapping.src_to_dst_transfer_dims[from_axis].size() > 1) {
    if (!CheckSplitAxis(axis_index_mapping.src_to_dst_transfer_dims[from_axis], axis_idx,
      from_axis, align_shape, src_shape)) {
      GELOGD("The value of from axis[%d] is %lld, align shape is %lld, [%s] not meet optimize condition.",
        from_axis, src_shape.GetDim(from_axis), align_shape.GetDim(from_axis), cur_pro_node_name_.c_str());
      return false;
    }
  } else {
    return src_shape.GetDim(from_axis) == 1;
  }

  return true;
}

bool NotaskPassBase::IsMergedAxisAllOnes(const int64_t &axis_idx,
  const std::vector<int64_t> &shape) const {
  return shape[axis_idx] == 1;
}

bool NotaskPassBase::IsFrontDimsAllOnesInMergedAxis(const gert::Shape &align_shape, const gert::Shape &src_shape,
  const transformer::AxisIndexMapping &axis_index_mapping, const int64_t &real_dim,
   const int64_t &dim) const {
  const auto src_axes = axis_index_mapping.dst_to_src_transfer_dims[real_dim];
  const auto merge_it = std::find(src_axes.begin(), src_axes.end(), dim);
  GE_ASSERT_TRUE(merge_it != src_axes.end());
  for (auto it = src_axes.begin(); it != merge_it; it++) {
    const auto from_axis = *it;
    if (!IsFromAxisOne(real_dim, axis_index_mapping, align_shape, src_shape, from_axis)) {
      GELOGD("The value of from axis[%d] is %lld, [%s] not meet optimize condition.",
        from_axis, src_shape.GetDim(from_axis), cur_pro_node_name_.c_str());
      return false;
    }
  }
  return true;
}

bool NotaskPassBase::IsFrontDimsAllOnes(const transformer::AxisIndexMapping &axis_index_mapping,
  const std::vector<int64_t> &shape, const int64_t &real_dim) const {
  for (auto axis = 0; axis < real_dim; axis++) {
    const auto src_axes = axis_index_mapping.dst_to_src_transfer_dims[axis];
    if (src_axes.size() > 1) {
      if (!IsMergedAxisAllOnes(axis, shape)) {
        GELOGD("The value of Merged axis[%d] is %lld, [%s] not meet optimize condition.",
          axis, shape[axis], cur_pro_node_name_.c_str());
        return false;
      }
    } else {
      if (shape[axis] != 1) {
        GELOGD("The value of axis[%d] is %lld, [%s] not meet optimize condition.", axis, shape[axis], cur_pro_node_name_.c_str());
        return false;
      }
    }
  }

  return true;
}

bool NotaskPassBase::CheckRealDim(const gert::Shape &align_shape, const gert::Shape &src_shape,
  const transformer::AxisIndexMapping &axis_index_mapping, const int64_t &dim,
  const ge::GeTensorDesc &input_tensor) const {
  int64_t real_dim = 0;

  GE_ASSERT_TRUE(axis_index_mapping.src_to_dst_transfer_dims[dim].size() > 0);
  real_dim = axis_index_mapping.src_to_dst_transfer_dims[dim][0];

  const auto shape = input_tensor.GetShape().GetDims();
  GE_ASSERT_TRUE((real_dim >= 0) && (static_cast<size_t>(real_dim) < shape.size()));
  const auto src_real_dims = axis_index_mapping.dst_to_src_transfer_dims[real_dim];
  if (src_real_dims.size() > 1) {
    return IsFrontDimsAllOnes(axis_index_mapping, shape, real_dim) &&
      IsFrontDimsAllOnesInMergedAxis(align_shape, src_shape, axis_index_mapping, real_dim, dim);
  } else {
    return IsFrontDimsAllOnes(axis_index_mapping, shape, real_dim);
  }
}

bool NotaskPassBase::GetTransferDims(const ge::OpDescPtr &op_desc, const gert::Shape &src_shape,
  const int64_t &reshape_type_mask,
  const ge::GeTensorDesc &input_tensor, transformer::AxisIndexMapping &axis_index_mapping) const {
  const auto input_format = input_tensor.GetFormat();
  const ge::Format input_orinal_format = input_tensor.GetOriginFormat();
  transformer::TransferDimsInfo transfer_dims_info;
  transfer_dims_info.src_format = input_orinal_format;
  transfer_dims_info.dst_format = input_format;
  transfer_dims_info.src_shape = src_shape;
  transfer_dims_info.reshape_type_mask = reshape_type_mask;

  GELOGD("Node [%s] original_format=%d, format=%d, reshape_type_mask=%lld.", op_desc->GetName().c_str(),
    input_orinal_format, input_format, reshape_type_mask);
  if (!transformer::TransferShapeUtils::TransferDims(transfer_dims_info, axis_index_mapping)) {
    GELOGD("[%s] notask transfer dims failed.", op_desc->GetName().c_str());
    return false;
  }
  PrintTransferDims("src_to_dst_transfer_dims", axis_index_mapping.src_to_dst_transfer_dims);
  PrintTransferDims("dst_to_src_transfer_dims", axis_index_mapping.dst_to_src_transfer_dims);
  GE_ASSERT_TRUE(axis_index_mapping.src_to_dst_transfer_dims.size() == src_shape.GetDimNum());
  GE_ASSERT_TRUE(axis_index_mapping.dst_to_src_transfer_dims.size() == input_tensor.GetShape().GetDimNum());

  return true;
}

bool NotaskPassBase::GetAlignedShape(const ge::OpDescPtr &op_desc, const gert::Shape &src_shape,
  const int64_t &reshape_type_mask,
  const ge::GeTensorDesc &input_tensor, gert::Shape &align_shape) const {
  const auto input_format = input_tensor.GetFormat();
  const ge::Format input_orinal_format = input_tensor.GetOriginFormat();

  GELOGD("[%s] original_format=%d, format=%d, data_type=%d, reshape_type_mask=%lld.",
    op_desc->GetName().c_str(), input_orinal_format, input_format, input_tensor.GetDataType(), reshape_type_mask);
  transformer::AlignShapeInfo align_shape_info;
  align_shape_info.src_format = input_orinal_format;
  align_shape_info.dst_format = input_format;
  align_shape_info.src_shape = src_shape;
  align_shape_info.data_type = input_tensor.GetDataType();
  align_shape_info.reshape_type_mask = reshape_type_mask;
  if (!transformer::TransferShapeUtils::GetAlignedShape(align_shape_info, align_shape)) {
    GELOGD("notask %s get align shape failed.", op_desc->GetName().c_str());
    return false;
  }
  PrintShape("align_shape", align_shape);
  GE_ASSERT_TRUE(align_shape.GetDimNum() == src_shape.GetDimNum());
  return true;
}

bool NotaskPassBase::IsUnknownShapeOp(const ge::OpDescPtr &op_desc) const {
  for (auto &tenosr_desc_ptr : op_desc->GetAllInputsDescPtr()) {
    if ((tenosr_desc_ptr != nullptr) && (tenosr_desc_ptr->GetShape().IsUnknownShape())) {
      GELOGD("notask input tensor is unknown shape.");
      return true;
    }
  }

  for (auto &tenosr_desc_ptr : op_desc->GetAllOutputsDescPtr()) {
    if ((tenosr_desc_ptr != nullptr) && (tenosr_desc_ptr->GetShape().IsUnknownShape())) {
      GELOGD("notask output tensor is unknown shape.");
      return true;
    }
  }
  return false;
}

bool NotaskPassBase::OutputCheck(const ge::NodePtr &node) const {
  for (auto &output_anchor : node->GetAllOutDataAnchors()) {
    for (size_t i = 0; i < output_anchor->GetPeerInDataAnchors().size(); i++) {
      auto peerAnchor = output_anchor->GetPeerInDataAnchors().at(i);
      GE_ASSERT_TRUE(peerAnchor != nullptr);
      auto next_node = peerAnchor->GetOwnerNode();
      const auto output_nodes = next_node->GetOutDataNodes();
      if ((next_node->GetType() == RESHAPE) && (!output_nodes.empty())) {
        next_node = output_nodes.at(0);
      }
      ge::OpDescPtr next_node_desc = next_node->GetOpDesc();
      string next_node_name = next_node_desc->GetName();
      bool no_task = false;
      bool output_reuse_input = false;
      bool no_padding_continuous_input = false;
      (void)ge::AttrUtils::GetBool(next_node_desc, ge::ATTR_NAME_NOTASK, no_task);
      (void)ge::AttrUtils::GetBool(next_node_desc, ge::ATTR_NAME_OUTPUT_REUSE_INPUT, output_reuse_input);
      (void)ge::AttrUtils::GetBool(next_node_desc, ge::ATTR_NAME_NOPADDING_CONTINUOUS_INPUT,
                                   no_padding_continuous_input);
      const bool is_virtual_op = no_task || output_reuse_input || no_padding_continuous_input;
      if (is_virtual_op) {
        GELOGD("Next node %s has _no_task attribute, %s can't optimize.", next_node_name.c_str(),
                node->GetName().c_str());
        return false;
      }
    }
  }
  return true;
}

bool NotaskPassBase::IsOwnerGraphUnknown(const ge::NodePtr &node) const {
  bool is_dynamic = false;
  const auto &owner_graph = node->GetOwnerComputeGraph();
  if (owner_graph != nullptr) {
    (void)AttrUtils::GetBool(owner_graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, is_dynamic);
    is_dynamic = (is_dynamic || owner_graph->GetGraphUnknownFlag());
  }

  return is_dynamic;
}

bool NotaskPassBase::LxFusionCheck(const ge::NodePtr &node) const {
  const auto op_desc = node->GetOpDesc();
  return !IsLxFusionMem(op_desc) && !IsLxFusionOp(node);
}

bool NotaskPassBase::IsLxFusionMem(const ge::OpDescPtr &op_desc) const {
  std::vector<uint32_t> input_mem_type;
  (void)ge::AttrUtils::GetListInt(op_desc, ge::ATTR_NAME_INPUT_MEM_TYPE_LIST, input_mem_type);
  std::vector<uint32_t> output_mem_type;
  (void)ge::AttrUtils::GetListInt(op_desc, ge::ATTR_NAME_OUTPUT_MEM_TYPE_LIST, output_mem_type);
  for (auto mem_type : input_mem_type) {
    if ((mem_type == RT_MEMORY_L1) || (mem_type == RT_MEMORY_L2) || (mem_type == kRtMemoryUB)) {
      GELOGD("Node [%s] has lx addr input, not optimize.", op_desc->GetName().c_str());
      return true;
    }
  }
  for (auto mem_type : output_mem_type) {
    if ((mem_type == RT_MEMORY_L1) || (mem_type == RT_MEMORY_L2) || (mem_type == kRtMemoryUB)) {
      GELOGD("Node [%s] has lx addr output, not optimize.", op_desc->GetName().c_str());
      return true;
    }
  }
  return false;
}

bool NotaskPassBase::IsLxFusionOp(const ge::NodePtr &node) const {
  std::string op_name = node->GetName();
  size_t pos = op_name.find(kNotaskLxSlice);
  if (pos != std::string::npos) {
    GELOGD("Node [%s] is lxfusion op, can not optimize.", node->GetName().c_str());
    return true;
  }
  return false;
}

void NotaskPassBase::SetNotaskAttr(const ge::NodePtr &node) const {
  const auto op_desc = node->GetOpDesc();
  GELOGI("success to set notask attribute for node [%s]", op_desc->GetName().c_str());
  (void)ge::AttrUtils::SetBool(op_desc, ge::ATTR_NAME_NOTASK, true);
  (void)ge::AttrUtils::SetBool(op_desc, ge::ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
  (void)ge::AttrUtils::SetBool(op_desc, ge::ATTR_NAME_OUTPUT_REUSE_INPUT, true);
  (void)ge::AttrUtils::SetInt(op_desc, ge::ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 0);

  const auto input_size = node->GetAllInDataAnchorsSize();
  for (uint32_t index = 0; index < input_size; ++index) {
    auto input_anchor = node->GetInDataAnchor(index);
    if (input_anchor == nullptr) {
      continue;
    }
    auto peer_out_anchor = input_anchor->GetPeerOutAnchor();
    if (peer_out_anchor == nullptr) {
      continue;
    }
    auto output_idx = peer_out_anchor->GetIdx();
    auto peer_node = peer_out_anchor->GetOwnerNode();
    auto output_tensor_desc = peer_node->GetOpDesc()->MutableOutputDesc(output_idx);
    if (output_tensor_desc != nullptr) {
      ge::AttrUtils::SetBool(output_tensor_desc, lock_attr_name_, false);
    }
  }
}

bool NotaskPassBase::InputCheck(const ge::NodePtr &node) {
  std::set<ge::OutDataAnchorPtr> src_anchors;
  std::set<int64_t> mem_types;
  for (size_t i = 0U; i < node->GetAllInDataAnchors().size(); i++) {
    const auto in_anchor = node->GetAllInDataAnchors().at(i);
    GE_CHECK_NOTNULL(in_anchor);
    const auto pre_out_anchor = in_anchor->GetPeerOutAnchor();
    if (pre_out_anchor == nullptr) {
      continue;
    }
    auto output_idx = pre_out_anchor->GetIdx();
    auto pre_node = pre_out_anchor->GetOwnerNode();
    auto pre_op_desc = pre_node->GetOpDesc();

    if (IsScalarInput(node, i)) {
      GELOGD("Node [%s] has scalar input[%zu] which does not meet optimize condition.", cur_pro_node_name_.c_str(), i);
      return false;
    }

    if (!CheckTensorAlign(node, i)) {
      GELOGD("node [%s] check tensor align failed.", node->GetName().c_str());
      return false;
    }

    if (HasSameSourceAnchor(in_anchor, src_anchors)) {
      GELOGD("node [%s] has same source anchor.", node->GetName().c_str());
      return false;
    }

    if (!IsPreNodeTypeValid(in_anchor)) {
      return false;
    }

    if (IsPreNodeWithSubgraph(in_anchor)) {
      GELOGD("Pre node [%s] has subgraph, [%s] can't optimize.", pre_node->GetName().c_str(), node->GetName().c_str());
      return false;
    }

    if (!IsPreOutAnchorCanReuse(pre_out_anchor)) {
      GELOGD("node [%s] pre node [%s] can not reused.", node->GetName().c_str(), pre_node->GetName().c_str());
      return false;
    }

    if (!IsPreOutAnchorValidMultiRef(pre_out_anchor)) {
      GELOGD("Previous node [%s] connect to netoutput, [%s] can't optimize.", pre_node->GetName().c_str(),
        cur_pro_node_name_.c_str());
      return false;
    }

    if (!IsPreNodeAttrValid(pre_op_desc)) {
      return false;
    }

    if (!IsSameInputMemType(pre_op_desc, output_idx, mem_types)) {
      GELOGD("Input mem type is not same, [%s] can't optimize.", cur_pro_node_name_.c_str());
      return false;
    }
  }
  return true;
}

bool NotaskPassBase::IsScalarInput(const ge::NodePtr &node, const size_t input_index) const {
  const auto td = node->GetOpDesc()->GetInputDesc(input_index);
  return td.GetOriginShape().GetDimNum() == 0;
}

bool NotaskPassBase::CheckTensorAlign(const ge::NodePtr &node, const size_t input_index) const {
  if (node->GetAllInDataAnchorsSize() == 1) {
    return true;
  }

  const auto td = node->GetOpDesc()->GetInputDesc(input_index);
  const auto shape_size = td.GetShape().GetShapeSize();
  if (ge::GetSizeByDataType(td.GetDataType()) < 0) {
    GELOGI("Get data type[%s] size less than zero.",
           ge::TypeUtils::DataTypeToSerialString(td.GetDataType()).c_str());
    return false;
  }
  const auto tensor_size = ge::GetSizeInBytes(shape_size, td.GetDataType());
  return ((tensor_size > 0) && (tensor_size % NOTASK_TENSOR_ALIGN_SIZE == 0));
}

bool NotaskPassBase::HasSameSourceAnchor(const ge::InDataAnchorPtr &in_anchor,
                         std::set<ge::OutDataAnchorPtr> &src_anchors) const {
  ge::OutDataAnchorPtr src_anchor = nullptr;
  GetFirstOutAnchorNotInRefNode(in_anchor, src_anchor, 0);
  const bool has_same_src_anchor = (src_anchors.count(src_anchor) == 1U);
  src_anchors.insert(src_anchor);
  return has_same_src_anchor;
}

bool NotaskPassBase::IsPreNodeWithSubgraph(const ge::InDataAnchorPtr &in_anchor) const {
  ge::NodePtr node = nullptr;

  GetFirstNotRefNode(in_anchor, node);
  if (node == nullptr) {
    return false;
  }
  const auto op_desc = node->GetOpDesc();
  return (op_desc != nullptr) ? (!op_desc->GetSubgraphInstanceNames().empty()) : false;
}

bool NotaskPassBase::IsPreNodeTypeValid(const ge::InDataAnchorPtr &in_anchor) {
  ge::NodePtr node = nullptr;

  GetFirstNotRefNode(in_anchor, node);
  if (node == nullptr) {
    return false;
  }
  const std::string op_type = node->GetType();
  static std::set<std::string> not_support_type = {DATA, REFDATA, VARIABLE, CONSTANTOP, CONSTANT};
  if (not_support_type.count(op_type) != 0U) {
    GELOGD("node [%s] pre node [%s] opType is %s.", cur_pro_node_name_.c_str(),
      node->GetName().c_str(), op_type.c_str());
    return false;
  }

  return true;
}

bool NotaskPassBase::IsPreOutAnchorCanReuse(const ge::OutDataAnchorPtr out_anchor) const {
  auto peer_node = out_anchor->GetOwnerNode();
  auto output_idx = out_anchor->GetIdx();
  auto output_tensor_desc = peer_node->GetOpDesc()->MutableOutputDesc(output_idx);
  if (output_tensor_desc == nullptr) {
    return false;
  }
  bool can_reuse = true;
  (void)ge::AttrUtils::GetBool(output_tensor_desc, lock_attr_name_, can_reuse);
  return can_reuse;
}

bool NotaskPassBase::IsPreOutAnchorValidMultiRef(const ge::OutDataAnchorPtr out_anchor) const {
  auto in_anchors = out_anchor->GetPeerInDataAnchors();
  if (in_anchors.size() == 1U) {
    return true;
  }

  for (const auto &anchor : in_anchors) {
    if (anchor->GetOwnerNode()->GetType() == NETOUTPUT) {
      return false;
    }
  }
  return true;
}

bool NotaskPassBase::IsPreNodeAttrValid(const ge::OpDescPtr &pre_op_desc) {
  string pre_node_name = pre_op_desc->GetName();
  bool is_continous_input = false;
  bool is_continous_output = false;
  bool is_ref = false;
  bool no_task = false;
  bool output_reuse_input = false;
  bool no_padding_continuous_input = false;
  vector<int64_t> output_index;
  (void)ge::AttrUtils::GetBool(pre_op_desc, ge::ATTR_NAME_CONTINUOUS_INPUT, is_continous_input);
  (void)ge::AttrUtils::GetBool(pre_op_desc, ge::ATTR_NAME_CONTINUOUS_OUTPUT, is_continous_output);
  (void)ge::AttrUtils::GetBool(pre_op_desc, ge::ATTR_NAME_REFERENCE, is_ref);
  (void)ge::AttrUtils::GetListInt(pre_op_desc, ge::ATOMIC_ATTR_OUTPUT_INDEX, output_index);
  (void)ge::AttrUtils::GetBool(pre_op_desc, ge::ATTR_NAME_NOTASK, no_task);
  (void)ge::AttrUtils::GetBool(pre_op_desc, ge::ATTR_NAME_OUTPUT_REUSE_INPUT, output_reuse_input);
  (void)ge::AttrUtils::GetBool(pre_op_desc, ge::ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, no_padding_continuous_input);

  if (is_continous_input || is_continous_output || is_ref) {
    GELOGD("Previous node %s attribute: continuous_input %s, continuous_output %s,"
      " reference %s, node %s can't optimize.",
      pre_node_name.c_str(), is_continous_input ? "true" : "false", is_continous_output ? "true" : "false",
      is_ref ? "true" : "false", cur_pro_node_name_.c_str());
    return false;
  }

  bool is_virtual_op = no_task || output_reuse_input || no_padding_continuous_input;
  if (is_virtual_op) {
    GELOGD("Previous node %s has _no_task attribute, %s can't optimize.", pre_node_name.c_str(),
           cur_pro_node_name_.c_str());
    return false;
  }
  if (!output_index.empty()) {
    GELOGD("Previous node %s has atomic output, %s can not optimize.", pre_node_name.c_str(),
           cur_pro_node_name_.c_str());
    return false;
  }

  return true;
}

bool NotaskPassBase::IsSameInputMemType(const ge::OpDescPtr &pre_op_desc, const size_t output_idx,
                                        std::set<int64_t> &mem_types) const {
  std::vector<int64_t> output_mem_type;
  int64_t mem_type = RT_MEMORY_HBM;
  (void)ge::AttrUtils::GetListInt(pre_op_desc, ge::ATTR_NAME_OUTPUT_MEM_TYPE_LIST, output_mem_type);
  if (output_idx < output_mem_type.size()) {
    if (MemTypeUtils::IsMemoryTypeSpecial(output_mem_type[output_idx])) {
      mem_type = output_mem_type[output_idx];
    }
  }
  mem_types.insert(mem_type);

  return (mem_types.size() == 1);
}

void NotaskPassBase::GetFirstOutAnchorNotInRefNode(const ge::InDataAnchorPtr &input_anchor, ge::OutDataAnchorPtr &src_anchor,
                                                   int32_t current_deep) const {
  if (current_deep >= kNotaskDeepth) {
    return;
  }
  auto peer_out_anchor = input_anchor->GetPeerOutAnchor();
  if (peer_out_anchor == nullptr) {
      return;
  }
  auto peer_node = peer_out_anchor->GetOwnerNode();
  if (peer_node == nullptr) {
      return;
  }
  int32_t reuse_in_index = -1;
  const bool reuse_input_flag = GraphUtils::IsRefFromInput(peer_out_anchor, reuse_in_index);
  if (reuse_input_flag) {
    auto in_anchor = peer_node->GetInDataAnchor(reuse_in_index);
    if (in_anchor == nullptr) {
      return;
    }
    GetFirstOutAnchorNotInRefNode(in_anchor, src_anchor, current_deep + 1);
  } else {
    src_anchor = peer_out_anchor;
  }
  return;
}

void NotaskPassBase::GetFirstNotRefNode(const ge::InDataAnchorPtr &input_anchor,
                                      ge::NodePtr &node) const {
  ge::OutDataAnchorPtr src_anchor = nullptr;
  GetFirstOutAnchorNotInRefNode(input_anchor, src_anchor, 0);
  node = (src_anchor != nullptr) ? src_anchor->GetOwnerNode() : nullptr;
  return;
}

bool NotaskPassBase::CheckDimForInput(const ge::OpDescPtr &op_desc, int64_t check_dim,
  size_t input_idx) const {
  ge::GeTensorDesc input_tensor = op_desc->GetInputDesc(input_idx);
  ge::GeShape input_orinal_shape = input_tensor.GetOriginShape();
  gert::Shape src_shape;
  src_shape.SetDimNum(input_orinal_shape.GetDimNum());
  for (size_t j = 0; j < src_shape.GetDimNum(); j++) {
    src_shape[j] = input_orinal_shape.GetDim(j);
  }
  PrintShape("src_shape", src_shape);
  int64_t reshape_type_mask = 0;
  (void)ge::AttrUtils::GetInt(input_tensor, ge::ATTR_NAME_RESHAPE_TYPE_MASK, reshape_type_mask);

  gert::Shape align_shape;
  if (!GetAlignedShape(op_desc, src_shape, reshape_type_mask, input_tensor, align_shape)) {
    return false;
  }

  transformer::AxisIndexMapping axis_index_mapping;
  if (!GetTransferDims(op_desc, src_shape, reshape_type_mask, input_tensor, axis_index_mapping)) {
    return false;
  }

  if (!CheckRealDim(align_shape, src_shape, axis_index_mapping, check_dim, input_tensor)) {
    GELOGD("[%s] notask check real dim failed, dim = %lld.", op_desc->GetName().c_str(), check_dim);
    return false;
  }

  if (!CheckDimAlignment(op_desc, align_shape, check_dim, input_orinal_shape)) {
    GELOGD("[%s] notask check dim alignment failed, dim = %lld.", op_desc->GetName().c_str(), check_dim);
    return false;
  }

  return true;
}
}  // namespace ge
