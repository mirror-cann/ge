/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "lower_concat_helper.h"

#include "backend/backend_spec.h"
#include "graph/utils/node_utils.h"
#include "can_fuse/backend/backend_utils.h"
#include "graph/debug/ge_op_types.h"
#include "symbolizer/symbolic_utils.h"

namespace ge {
namespace {
constexpr int64_t kAlignment = 32;
constexpr int32_t kAlgTranspose = 0;
constexpr int32_t kAlgScatter = 1;
const Expression kOne = Symbol(1);
}  // namespace
LowerConcatHelper::LowerConcatHelper(NodePtr fused_asc_backend_node)
    : fused_asc_backend_node_(std::move(fused_asc_backend_node)) {}

graphStatus LowerConcatHelper::LiftingPoorPerfFusedAscBackendOps(const ComputeGraphPtr &graph) {
  for (const auto &node : graph->GetAllNodes()) {
    if (node->GetType() == kFusedAscBackendType) {
      LowerConcatHelper lower_concat_helper(node);
      GE_ASSERT_GRAPH_SUCCESS(lower_concat_helper.LiftingPoorPerfFusedAscBackendOp(), "Failed to process %s",
                              node->GetNamePtr());
    }
  }
  return ge::GRAPH_SUCCESS;
}

graphStatus LowerConcatHelper::LiftingPoorPerfFusedAscBackendOp() {
  GE_CHK_BOOL_RET_SPECIAL_STATUS(!FindConcatAscBackendNode(), GRAPH_SUCCESS, "concat node not found");
  bool need_lifting = false;
  GE_ASSERT_SUCCESS(NeedLifting(need_lifting));
  if (need_lifting) {
    GE_ASSERT_SUCCESS(UnfoldFusedAscBackend());
  }
  return GRAPH_SUCCESS;
}

bool LowerConcatHelper::FindConcatAscBackendNode() {
  bool found = false;
  GE_ASSERT_NOTNULL(fused_asc_backend_node_->GetOpDescBarePtr());
  const auto attr = fused_asc_backend_node_->GetOpDescBarePtr()->GetAttrsGroup<AutoFuseAttrs>();
  if (attr != nullptr) {
    graph_ = attr->GetFuseComputeGraph();
    GE_ASSERT_NOTNULL(graph_);
    for (const auto &node : graph_->GetAllNodes()) {
      GE_ASSERT_NOTNULL(node->GetOpDescBarePtr());
      const auto sub_attr = node->GetOpDescBarePtr()->GetAttrsGroup<AutoFuseAttrs>();
      if ((sub_attr != nullptr) && (sub_attr->GetFuseType() == loop::FuseType::kConcat)) {
        concat_asc_backend_node_ = node;
        const auto &asc_graph = sub_attr->GetAscGraph();
        GE_ASSERT_NOTNULL(asc_graph);
        concat_node_ = FindConcatNode(*asc_graph);
        GE_ASSERT_NOTNULL(concat_node_);
        found = true;
        break;
      }
    }
  }
  return found;
}

AscNodePtr LowerConcatHelper::FindConcatNode(const AscGraph &asc_graph) {
  AscNodePtr concat_node;
  for (const auto &n : asc_graph.GetAllNodes()) {
    if (n->GetType() == "Concat") {
      concat_node = n;
      break;
    }
  }
  return concat_node;
}

graphStatus LowerConcatHelper::ParseConcatNode() {
  bool found = false;
  for (size_t i = 0U; i < concat_node_->inputs.Size(); ++i) {
    input_shapes_.emplace_back(concat_node_->inputs[i].attr.repeats);
  }
  output_shape_ = concat_node_->outputs[0].attr.repeats;
  GE_ASSERT_EQ(input_shapes_.front().size(), output_shape_.size());

  size_t non_one_count = 0U;
  for (size_t i = 0U; i < output_shape_.size(); ++i) {
    if (input_shapes_.front()[i] != output_shape_[i]) {
      concat_dim_ = i;
      is_first_dim_ = (non_one_count == 0);
      found = true;
      break;
    }
    if (ge::SymbolicUtils::StaticCheckEq(output_shape_[i], kOne) != ge::TriBool::kTrue) {
      ++non_one_count;
    }
  }
  GE_ASSERT_TRUE(found, "[%s] failed to find concat dim, input_shape[0] = %s, output_shape = %s",
                 fused_asc_backend_node_->GetNamePtr(), ToString(input_shapes_.front()).c_str(),
                 ToString(output_shape_).c_str());
  return GRAPH_SUCCESS;
}

graphStatus LowerConcatHelper::ParseConcatCase() {
  bool is_unknown_shape = false;
  stride_ = CalcConcatAxisStride(is_unknown_shape);
  size_t num_aligned = 0;
  std::set<std::pair<Node *, int32_t>> unique_srcs;
  bool can_be_no_task = is_first_dim_ && (!is_unknown_shape);
  GELOGD("is first dim concat = %d, is known shape = %d", static_cast<int32_t>(is_first_dim_),
         static_cast<int32_t>(!is_unknown_shape));
  for (const auto in_anchor : concat_asc_backend_node_->GetAllInDataAnchorsPtr()) {
    if (in_anchor != nullptr) {
      GE_ASSERT_TRUE(static_cast<size_t>(in_anchor->GetIdx()) < input_shapes_.size());
      auto &input_shape = input_shapes_[in_anchor->GetIdx()];
      GE_ASSERT_EQ(input_shape.size(), output_shape_.size());
      const auto dim_size = input_shape[concat_dim_];
      GE_CHK_BOOL_RET_SPECIAL_STATUS((!dim_size.IsConstExpr()), ge::SUCCESS, "contains non-const dim: %s",
                                     SymbolicUtils::ToString(dim_size).c_str());
      int64_t dim_size_val = -1;
      (void)dim_size.GetConstValue(dim_size_val);
      GE_CHK_BOOL_RET_SPECIAL_STATUS(dim_size_val < 0, ge::SUCCESS, "input[%zu] contains %ld dim", in_anchor->GetIdx(),
                                     dim_size_val);
      auto peer_out_anchor = in_anchor->GetPeerOutAnchor();
      GE_ASSERT_NOTNULL(peer_out_anchor);
      const auto peer_node = peer_out_anchor->GetOwnerNodeBarePtr();
      GE_ASSERT_NOTNULL(peer_node);
      GELOGI("input[%d] connected to %s(%s)", in_anchor->GetIdx(), peer_node->GetNamePtr(), peer_node->GetTypePtr());
      if ((peer_node->GetType() == kAscBackendType) &&
          unique_srcs.emplace(peer_node, peer_out_anchor->GetIdx()).second) {
        total_fused_dim_size_ += dim_size_val;
      } else {
        if (can_be_no_task) {
          GE_ASSERT_SUCCESS(IsPeerNodeValidForNoTask(peer_node, can_be_no_task));
        }
      }
      num_aligned += static_cast<int64_t>(stride_ * dim_size_val % kAlignment == 0);
    }
  }
  const auto in_num = concat_asc_backend_node_->GetInDataNodesSize();
  const auto all_aligned = (num_aligned == in_num);
  const auto none_reuse = (input_shapes_.size() == in_num);
  GELOGI("can_be_no_task = %d, all_aligned = %d, none_reuse = %d", static_cast<int32_t>(can_be_no_task),
         static_cast<int32_t>(all_aligned), static_cast<int32_t>(none_reuse));
  if (can_be_no_task && all_aligned && none_reuse) {
    case_ = ParseConcatCaseForNoTask();
    return GRAPH_SUCCESS;
  }
  case_ = is_first_dim_ ? ConcatCase::kFirstDim : (all_aligned ? ConcatCase::kAllAligned : ConcatCase::kOther);
  return GRAPH_SUCCESS;
}

LowerConcatHelper::ConcatCase LowerConcatHelper::ParseConcatCaseForNoTask() const {
  constexpr int64_t kInputSizeLarge = 8 * 1024 * 1024;
  constexpr int64_t kInputSizeMid = 1024 * 1024;
  constexpr int64_t kInputSizeSmall = 512 * 1024;
  const int64_t avg_input_size = stride_ * output_dim_size_ / num_inputs_;  // checked num_inputs_ > 0
  GELOGD("avg_input_size = %ld bytes", avg_input_size);
  if (avg_input_size > kInputSizeLarge) {
    return ConcatCase::kFirstDimNoTaskLarge;  // 100%
  }
  if (avg_input_size > kInputSizeMid) {
    return ConcatCase::kFirstDimNoTaskMid;  // 80%
  }
  if (avg_input_size > kInputSizeSmall) {
    return ConcatCase::kFirstDimNoTaskSmall;  // 50%
  }
  return ConcatCase::kFirstDimNoTaskTiny;  // 33%
}

graphStatus LowerConcatHelper::NeedLifting(bool &need_lifting) {
  num_inputs_ = concat_node_->GetAllInDataAnchorsSize();
  GE_ASSERT_TRUE(num_inputs_ > 0);
  GE_CHK_BOOL_RET_SPECIAL_STATUS(HasBackwardFusion(), GRAPH_SUCCESS, "has backward fusion, do not lifting");
  auto backend_spec = optimize::BackendSpec::GetInstance();
  GE_ASSERT_NOTNULL(backend_spec);
  if (!IsTile()) {
    GE_CHK_BOOL_RET_SPECIAL_STATUS(num_inputs_ > backend_spec->concat_max_input_num, GRAPH_SUCCESS,
                                   "num_inputs = %zu, do not lifting", num_inputs_);
    GE_CHK_BOOL_RET_SPECIAL_STATUS(num_inputs_ == 1U, GRAPH_SUCCESS, "single input, do not lifting");
  }
  GE_ASSERT_SUCCESS(ParseConcatNode());
  // 暂不处理concat_dim后为动态shape的场景
  GE_CHK_BOOL_RET_SPECIAL_STATUS(!output_shape_[concat_dim_].IsConstExpr(), GRAPH_SUCCESS,
                                 "concat dim size is non-const");
  (void)output_shape_[concat_dim_].GetConstValue(output_dim_size_);
  GE_CHK_BOOL_RET_SPECIAL_STATUS(output_dim_size_ <= 0, GRAPH_SUCCESS, "concat dim size is not positive");
  GE_ASSERT_SUCCESS(ParseConcatCase());
  GE_CHK_BOOL_RET_SPECIAL_STATUS(case_ == ConcatCase::kNoLifting, GRAPH_SUCCESS, "No need for lifting");
  auto buffer_ratio = static_cast<float64_t>(total_fused_dim_size_) / static_cast<float64_t>(output_dim_size_);
  auto threshold = GetThreshold(backend_spec->concat_alg, case_);
  need_lifting = buffer_ratio < threshold;
  GELOGI("FusedAscBackend: %s, concat: %s, case = %s, ratio = %ld/%ld = %.15f, threshold = %f, need_lifting = %d",
         fused_asc_backend_node_->GetNamePtr(), concat_node_->GetNamePtr(), CaseName(case_).c_str(),
         total_fused_dim_size_, output_dim_size_, buffer_ratio, threshold, need_lifting);
  return GRAPH_SUCCESS;
}

graphStatus LowerConcatHelper::UnfoldFusedAscBackend() const {
  auto is_valid_graph = CheckGraph();
  GE_LOGW_IF(!is_valid_graph, "[%s] skip unfold, graph is abnormal", fused_asc_backend_node_->GetNamePtr());
  if (is_valid_graph) {
    GELOGI("FusedAscBackend node name = [%s], AscBackend node name = [%s], concat node name = [%s]",
           fused_asc_backend_node_->GetNamePtr(), concat_asc_backend_node_->GetNamePtr(), concat_node_->GetNamePtr());
    auto parent_graph = fused_asc_backend_node_->GetOwnerComputeGraph();
    GE_ASSERT_GRAPH_SUCCESS(GraphUtils::UnfoldGraph(graph_, parent_graph, fused_asc_backend_node_, nullptr),
                            "Failed to unfold graph");
    GE_ASSERT_GRAPH_SUCCESS(parent_graph->TopologicalSorting());
  }
  return GRAPH_SUCCESS;
}

bool LowerConcatHelper::CheckGraph() const {
  auto output_node = graph_->FindFirstNodeMatchType(NETOUTPUT);
  GE_WARN_ASSERT(output_node != nullptr);
  auto in_num = graph_->GetInputNodes().size();
  auto parent_in_num = static_cast<size_t>(fused_asc_backend_node_->GetAllInDataAnchorsSize());
  GE_WARN_ASSERT(in_num == parent_in_num, "[%s] input not match, in_num = %u, parent_node in_num = %u",
                 fused_asc_backend_node_->GetNamePtr(), in_num, parent_in_num);

  auto out_num = output_node->GetAllInDataAnchorsSize();
  auto parent_out_num = fused_asc_backend_node_->GetAllOutDataAnchorsSize();
  GE_WARN_ASSERT(out_num == parent_out_num, "[%s] output not match, out_num = %u, parent out_num = %u",
                 fused_asc_backend_node_->GetNamePtr(), out_num, parent_out_num);
  return true;
}

bool LowerConcatHelper::HasBackwardFusion() const {
  const auto out_data_nodes = concat_asc_backend_node_->GetOutDataNodes();
  return std::any_of(out_data_nodes.begin(), out_data_nodes.end(),
                     [](const ge::NodePtr &peer_node) -> bool { return peer_node->GetType() == kAscBackendType; });
}

bool LowerConcatHelper::IsTile() const {
  return (concat_node_->GetInDataNodesSize() > 1UL) && (concat_asc_backend_node_->GetInDataNodesSize() == 1UL);
}

graphStatus LowerConcatHelper::IsPeerNodeValidForNoTask(const Node *node, bool &is_valid) const {
  static std::set<std::string> kUnsupportedTypes = {"Data", "RefData", "Variable", "Constant", "Const"};
  auto peer_node = node;
  if (peer_node->GetType() == "Data") {
    int32_t index = -1;
    GE_ASSERT_TRUE(AttrUtils::GetInt(node->GetOpDesc(), ATTR_NAME_PARENT_NODE_INDEX, index));
    const auto in_anchor = fused_asc_backend_node_->GetInDataAnchor(index);
    GE_ASSERT_NOTNULL(in_anchor);
    const auto out_anchor = in_anchor->GetPeerOutAnchor();
    GE_ASSERT_NOTNULL(out_anchor);
    peer_node = out_anchor->GetOwnerNodeBarePtr();
    GE_ASSERT_NOTNULL(peer_node);
  }
  if (kUnsupportedTypes.find(peer_node->GetType()) != kUnsupportedTypes.cend()) {
    is_valid = false;
    GELOGI("peer node = %s(%s), do not support no task", peer_node->GetNamePtr(), peer_node->GetTypePtr());
  }
  return SUCCESS;
}

int64_t LowerConcatHelper::CalcConcatAxisStride(bool &is_unknown_shape) const {
  int64_t stride = ge::GetSizeByDataType(concat_node_->GetOpDesc()->GetOutputDesc(0).GetDataType());
  for (size_t i = concat_dim_ + 1U; i < output_shape_.size(); ++i) {
    auto &dim_expr = output_shape_[i];
    if (dim_expr.IsConstExpr()) {
      int64_t dim_size = -1;
      dim_expr.GetConstValue(dim_size);
      if (dim_size >= 0) {
        stride *= dim_size;
      }
    } else {
      is_unknown_shape = true;
    }
  }
  return stride;
}

const std::string &LowerConcatHelper::CaseName(ConcatCase concat_case) {
  static const std::map<ConcatCase, std::string> kCaseToName{{ConcatCase::kFirstDim, "first_dim"},
                                                             {ConcatCase::kAllAligned, "aligned"},
                                                             {ConcatCase::kOther, "other"},
                                                             {ConcatCase::kFirstDimNoTaskLarge, "may_be_no_task_large"},
                                                             {ConcatCase::kFirstDimNoTaskMid, "may_be_no_task_mid"},
                                                             {ConcatCase::kFirstDimNoTaskSmall, "may_be_no_task_small"},
                                                             {ConcatCase::kFirstDimNoTaskTiny, "may_be_no_task"}};
  return kCaseToName.at(concat_case);
}

float64_t LowerConcatHelper::GetThreshold(int32_t alg, ConcatCase concat_case) {
  static const std::map<int32_t, std::map<ConcatCase, ge::float64_t>> kAlgToCaseToRatio{
      {kAlgTranspose,
       std::map<ConcatCase, ge::float64_t>{
           {ConcatCase::kFirstDim, 0.0},
           {ConcatCase::kFirstDimNoTaskLarge, 1.0},
           {ConcatCase::kFirstDimNoTaskMid, 0.8},
           {ConcatCase::kFirstDimNoTaskSmall, 0.5},
           {ConcatCase::kFirstDimNoTaskTiny, 0.3333},
           {ConcatCase::kAllAligned, 0.3333},
           {ConcatCase::kOther, 0.3333},
       }},
      {kAlgScatter,
       std::map<ConcatCase, ge::float64_t>{
           {ConcatCase::kFirstDim, 0.0},
           {ConcatCase::kFirstDimNoTaskLarge, 1.0},
           {ConcatCase::kFirstDimNoTaskMid, 0.8},
           {ConcatCase::kFirstDimNoTaskSmall, 0.5},
           {ConcatCase::kFirstDimNoTaskTiny, 0.3333},
           {ConcatCase::kAllAligned, 0.1666},
           {ConcatCase::kOther, 0.1666},
       }},
  };
  return kAlgToCaseToRatio.at(alg).at(concat_case);
}
}  // namespace ge
