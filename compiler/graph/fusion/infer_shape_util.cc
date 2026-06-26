/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ge/fusion/infer_shape_util.h"

#include "graph/utils/node_adapter.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/op_type_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/shape_refiner.h"
#include "graph/passes/base_pass.h"
#include "graph/ir_definitions_recover.h"
#include "graph/passes/shape_optimize/infershape_pass.h"
#include "graph/fusion/fusion_utils.h"
#include "common/checker.h"

namespace ge {
namespace fusion {
namespace {
bool IsGraphInput(const NodePtr &node, int64_t &input_index) {
  if (!OpTypeUtils::IsDataNode(node->GetType())) {
    return false;
  }
  if (!AttrUtils::GetInt(node->GetOpDesc(), ATTR_NAME_INDEX, input_index)) {
    return false;
  }
  return true;
}

Status RunInferShapePass(ComputeGraphPtr &compute_graph) {
  GEPass ge_passes(compute_graph);
  NamesToPass names_to_passes;
  InferShapePass infer_shape_pass(nullptr, nullptr);
  names_to_passes.emplace_back("InferShapePass", &infer_shape_pass);
  const Status ret = ge_passes.Run(names_to_passes, false);
  ShapeRefiner::ClearContextMap();
  return ret;
}

void CopyTensorDescMeta(const GeTensorDesc &src, GeTensorDescPtr &dst) {
  dst->SetShape(src.GetShape());
  dst->SetOriginShape(src.GetOriginShape());
  dst->SetDataType(src.GetDataType());
  dst->SetOriginDataType(src.GetOriginDataType());
  dst->SetFormat(src.GetFormat());
  dst->SetOriginFormat(src.GetOriginFormat());
}

Status PropagateConstValueToPeers(const NodePtr &node, const GeTensor &weight) {
  for (const auto &out_anchor : node->GetAllOutDataAnchors()) {
    for (const auto &peer_in_anchor : out_anchor->GetPeerInDataAnchors()) {
      const auto peer_node = peer_in_anchor->GetOwnerNode();
      auto peer_input_desc = peer_node->GetOpDesc()->MutableInputDesc(static_cast<uint32_t>(peer_in_anchor->GetIdx()));
      if (peer_input_desc == nullptr) {
        continue;
      }
      GE_ASSERT_TRUE(AttrUtils::SetShareTensor(peer_input_desc, ATTR_NAME_VALUE, weight),
                     "Failed to set ATTR_NAME_VALUE on node[%s][%s] input[%d]", peer_node->GetNamePtr(),
                     peer_node->GetTypePtr(), peer_in_anchor->GetIdx());
      GELOGI("Set ATTR_NAME_VALUE on node[%s][%s] input[%d] for value-dependent infershape", peer_node->GetNamePtr(),
             peer_node->GetTypePtr(), peer_in_anchor->GetIdx());
    }
  }
  return SUCCESS;
}

Status RefreshReplacementInputDescs(ComputeGraphPtr &compute_graph,
                                    const std::map<int64_t, GeTensorDesc> &index_2_input_desc,
                                    const std::map<int64_t, const GeTensor *> &index_2_const_weight) {
  for (const auto &node : compute_graph->GetDirectNode()) {
    if (!OpTypeUtils::IsDataNode(node->GetType())) {
      continue;
    }
    int64_t input_index = 0;
    GE_ASSERT_TRUE(IsGraphInput(node, input_index), "Data node[%s][%s] missing index attr", node->GetNamePtr(),
                   node->GetTypePtr());
    const auto iter = index_2_input_desc.find(input_index);
    GE_ASSERT_TRUE(iter != index_2_input_desc.cend(), "Data node[%s][%s] index[%ld] not found in boundary inputs",
                   node->GetNamePtr(), node->GetTypePtr(), input_index);
    const auto &tensor_desc = iter->second;
    auto input_desc = node->GetOpDesc()->MutableInputDesc(0U);
    auto output_desc = node->GetOpDesc()->MutableOutputDesc(0U);
    GE_ASSERT_NOTNULL(input_desc);
    GE_ASSERT_NOTNULL(output_desc);
    CopyTensorDescMeta(tensor_desc, input_desc);
    CopyTensorDescMeta(tensor_desc, output_desc);
    GELOGI("Update replacement Data node[%s][%s] index[%ld]: shape[%s] dtype[%d] format[%d]", node->GetNamePtr(),
           node->GetTypePtr(), input_index, tensor_desc.GetShape().ToString().c_str(),
           static_cast<int32_t>(tensor_desc.GetDataType()), static_cast<int32_t>(tensor_desc.GetFormat()));

    const auto weight_iter = index_2_const_weight.find(input_index);
    if (weight_iter == index_2_const_weight.cend() || weight_iter->second == nullptr) {
      continue;
    }
    GE_ASSERT_SUCCESS(PropagateConstValueToPeers(node, *weight_iter->second));
  }
  return SUCCESS;
}
}  // namespace

Status InferShapeUtil::InferShape(const Graph &replacement_graph, const SubgraphBoundary &subgraph_boundary) {
  auto compute_graph = GraphUtilsEx::GetComputeGraph(replacement_graph);
  GE_ASSERT_NOTNULL(compute_graph);
  GE_ASSERT_GRAPH_SUCCESS(ge::RecoverIrDefinitions(compute_graph), "Failed to recover ir definitions");

  std::vector<SubgraphInput> all_inputs;
  GE_ASSERT_SUCCESS(subgraph_boundary.GetAllInputs(all_inputs));

  std::map<int64_t, GeTensorDesc> index_2_input_desc;
  std::map<int64_t, const GeTensor *> index_2_const_weight;
  for (int64_t i = 0; i < static_cast<int64_t>(all_inputs.size()); ++i) {
    const auto node_inputs = all_inputs[static_cast<size_t>(i)].GetAllInputs();
    GE_ASSERT_TRUE(!node_inputs.empty(), "Boundary input[%ld] has no associated node inputs", i);
    const auto node_ptr = NodeAdapter::GNode2Node(node_inputs[0U].node);
    GE_ASSERT_NOTNULL(node_ptr);
    const auto in_anchor = node_ptr->GetInDataAnchor(static_cast<int32_t>(node_inputs[0U].index));
    GE_ASSERT_NOTNULL(in_anchor);
    const auto producer_anchor = in_anchor->GetPeerOutAnchor();
    GE_ASSERT_NOTNULL(producer_anchor, "Boundary input[%ld] has no tensor producer", i);
    const auto producer_node = producer_anchor->GetOwnerNode();
    GE_ASSERT_NOTNULL(producer_node);
    index_2_input_desc[i] = producer_node->GetOpDesc()->GetOutputDesc(static_cast<uint32_t>(producer_anchor->GetIdx()));
    Operator boundary_op = OpDescUtils::CreateOperatorFromNode(node_ptr);
    const auto weight = OpDescUtils::GetInputConstData(boundary_op, static_cast<uint32_t>(node_inputs[0U].index));
    if (weight != nullptr) {
      index_2_const_weight[i] = weight;
    }
  }

  GE_ASSERT_SUCCESS(RefreshReplacementInputDescs(compute_graph, index_2_input_desc, index_2_const_weight));
  return RunInferShapePass(compute_graph);
}

Status InferShapeUtil::InferShape(const Graph &replacement_graph, const MatchResult &match_result) {
  auto boundary = match_result.ToSubgraphBoundary();
  GE_ASSERT_NOTNULL(boundary);
  return InferShape(replacement_graph, *boundary);
}

Status InferShapeUtil::InferShape(const Graph &replacement_graph, const GNode &matched_node) {
  auto node_ptr = NodeAdapter::GNode2Node(matched_node);
  GE_ASSERT_NOTNULL(node_ptr);
  auto boundary = FusionUtils::BuildSubgraphBoundaryFromNode(node_ptr);
  GE_ASSERT_NOTNULL(boundary);
  return InferShape(replacement_graph, *boundary);
}
}  // namespace fusion
}  // namespace ge
