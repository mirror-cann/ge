/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "utils/auto_fuse_config.h"
#include "can_fuse/backend/backend_utils.h"
#include "fusion/autofuse_attrs.h"
#include "can_fuse/strategy/fusion_strategy_registry.h"
#include "split_fusion_strategy.h"
#include "utils/not_fuse_reason_code.h"
#include "graph/symbolizer/symbolic_utils.h"
#include "graph/attribute_group/attr_group_symbolic_desc.h"

namespace ge {
namespace {
// 检查Reshape是否只是削减了shape为1的轴（即squeeze操作）
// split_output_shape: Split的输出shape（即Reshape的输入shape）
// reshape_output_shape: Reshape的输出shape
// 返回true表示只是削减了shape为1的轴，false表示有其他变换
bool IsReshapeOnlySqueezeDims(const std::vector<ge::Expression> &split_output_shape,
                              const std::vector<ge::Expression> &reshape_output_shape) {
  // 如果输出维度大于输入维度，不是squeeze
  if (reshape_output_shape.size() > split_output_shape.size()) {
    return false;
  }

  // 检查输出shape是否是输入shape去掉dim=1的轴后的结果
  size_t split_idx = 0U;
  size_t reshape_idx = 0U;
  const auto kSymbolOne = ge::Symbol(1);

  while (split_idx < split_output_shape.size() && reshape_idx < reshape_output_shape.size()) {
    // 如果Split输出和Reshape输出维度相等，继续检查下一个
    if (SymbolicUtils::StaticCheckEq(split_output_shape[split_idx], reshape_output_shape[reshape_idx]) ==
        ge::TriBool::kTrue) {
      split_idx++;
      reshape_idx++;
      continue;
    }
    // 如果Split输出维度为1，跳过（这是被squeeze掉的维度）
    if (SymbolicUtils::StaticCheckEq(split_output_shape[split_idx], kSymbolOne) == ge::TriBool::kTrue) {
      split_idx++;
      continue;
    }
    // 不匹配，不是纯squeeze
    return false;
  }

  // 检查剩余的Split输出维度是否都为1
  while (split_idx < split_output_shape.size()) {
    if (SymbolicUtils::StaticCheckEq(split_output_shape[split_idx], kSymbolOne) != ge::TriBool::kTrue) {
      return false;
    }
    split_idx++;
  }

  // Reshape输出shape应该已经遍历完
  return reshape_idx == reshape_output_shape.size();
}

// 检查Reshape节点的下一个节点是否能被融合
// 只要有任意一个后继节点能被融合，就返回true
// split_node: Split节点，用于判断是否能与next_node融合
// reshape_node: Reshape节点，其后继节点需要被检查
bool CanReshapeNextNodeFuse(const NodePtr &reshape_node) {
  const auto &out_nodes = reshape_node->GetOutDataNodes();
  // 如果没有输出节点，无法判断，不允许融合
  if (out_nodes.empty()) {
    GELOGI("Reshape node %s has no output nodes, cannot check next node fusion.",
           reshape_node->GetName().c_str());
    return false;
  }

  // 检查是否有任意一个后继节点能被融合
  for (const auto &next_node : out_nodes) {
    GE_ASSERT_NOTNULL(next_node);

    // 使用节点的FuseType判断是否能融合
    const auto next_attr = BackendUtils::GetNodeAutoFuseAttr(next_node);
    if (next_attr == nullptr) {
      continue;
    }
    // kReduction、kExtern、kSplit、kSliceSplit不能融合
    if (next_attr->HasFuseType(loop::FuseType::kReduction) ||
        next_attr->HasFuseType(loop::FuseType::kExtern) ||
        next_attr->HasFuseType(loop::FuseType::kSplit) ||
        next_attr->HasFuseType(loop::FuseType::kSliceSplit)) {
      continue;
    }

    GELOGI("Reshape node %s: next node %s(%s) can fuse with split.",
           reshape_node->GetName().c_str(), next_node->GetName().c_str(), next_node->GetType().c_str());
    return true;
  }

  GELOGI("Reshape node %s: none of next nodes can fuse.", reshape_node->GetName().c_str());
  return false;
}

// 获取节点的符号shape
graphStatus GetNodeSymbolicOutputShape(const NodePtr &node, std::vector<ge::Expression> &shape) {
  const auto op_desc = node->GetOpDesc();
  GE_WARN_ASSERT(op_desc != nullptr, "node %s op_desc is nullptr", node->GetName().c_str());
  const auto output_desc = op_desc->GetOutputDescPtr(0);
  GE_WARN_ASSERT(output_desc != nullptr, "node %s output_desc is nullptr", node->GetName().c_str());
  const auto sym_attr = output_desc->GetAttrsGroup<SymbolicDescAttr>();
  GE_WARN_ASSERT(sym_attr != nullptr, "node %s sym_attr is nullptr", node->GetName().c_str());
  shape = sym_attr->symbolic_tensor.GetOriginSymbolShape().GetDims();
  return GRAPH_SUCCESS;
}

// 检查Split与Reshape融合的特殊场景
// 当Split与Reshape节点融合时，如果Reshape只是削减了shape为1的轴，
// 且Reshape的下一个节点不能被融合，则Split不与Reshape融合
bool CheckSplitReshapeFusion(const NodePtr &split_node, const NodePtr &reshape_node) {
  // 检查reshape_node是否只包含Reshape操作
  if (!BackendUtils::OnlyHasTypesInAscgraph(reshape_node, {kReshapeType})) {
    return true; // 不是纯Reshape节点，允许融合
  }

  // 获取Split和Reshape的输出shape
  std::vector<ge::Expression> split_output_shape;
  std::vector<ge::Expression> reshape_output_shape;
  GE_ASSERT_SUCCESS(GetNodeSymbolicOutputShape(split_node, split_output_shape),
                          "Cannot get Split node %s output shape", split_node->GetName().c_str());
  GE_ASSERT_SUCCESS(GetNodeSymbolicOutputShape(reshape_node, reshape_output_shape),
                          "Cannot get Reshape node %s output shape", reshape_node->GetName().c_str());

  GELOGI("Checking Split %s and Reshape %s for squeeze fusion.",
         split_node->GetName().c_str(), reshape_node->GetName().c_str());

  // 检查是否只是削减了shape为1的轴
  if (!IsReshapeOnlySqueezeDims(split_output_shape, reshape_output_shape)) {
    GELOGI("Reshape node %s is not just squeezing dims with size 1, allow fusion.",
           reshape_node->GetName().c_str());
    return true; // 不是纯squeeze，允许融合
  }

  GELOGI("Reshape node %s is only squeezing dims with size 1, checking next node fusion.",
         reshape_node->GetName().c_str());

  // 检查Reshape的下一个节点是否能被融合
  if (CanReshapeNextNodeFuse(reshape_node)) {
    GELOGI("Reshape next node can fuse, allow Split %s to fuse with Reshape node %s.",
           split_node->GetName().c_str(), reshape_node->GetName().c_str());
    return true;
  }

  GELOGI("Split %s cannot fuse with Reshape node %s: Reshape only squeezes dims with size 1 "
         "and next node cannot fuse.",
         split_node->GetName().c_str(), reshape_node->GetName().c_str());
  return false;
}

// 检查两个Split节点是否能融合
bool CheckSplitSplitFusion(const NodePtr &node1, const NodePtr &node2,
                           AutoFuseAttrs *attr1,
                           AutoFuseAttrs *attr2) {
  if (attr1->GetSplitGlobalId() != attr2->GetSplitGlobalId()) {
    GELOGI("node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s][node1 and node2 are horizontal fuse, "
           "node1 is split, node2 is split, and they are different split nodes before lowering.]",
           node1->GetName().c_str(), node1->GetType().c_str(), node2->GetName().c_str(), node2->GetType().c_str(),
           ge::NotFuseReasonCode(ge::NotFuseReason::kSplitCanNotFuseSplitHorizontal));
    return false;
  }
  // 前端必须保证同一个Ascend IR split lowering成的N个split AscBackend在canfuse阶段融合到同一个AscBackend内
  return true;
}

// 检查Split低融合比例
bool CheckSplitLowFusionRatio(const NodePtr &node1, const NodePtr &node2,
                              AutoFuseAttrs *attr1) {
  if (attr1->GetSplitFusionRatioRequirementState() == SplitFusionRatioRequirementState::NOT_SATISFIED) {
    GELOGI("node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s][node1 or node2 has low fuse ratio]",
           node1->GetName().c_str(), node1->GetType().c_str(), node2->GetName().c_str(), node2->GetType().c_str(),
           ge::NotFuseReasonCode(ge::NotFuseReason::kSplitLowFuseRatio));
    return false;
  }
  return true;
}

// 检查Split与Reduction融合
bool CheckSplitReductionFusion(const NodePtr &node1, const NodePtr &node2,
                               const AutoFuseAttrs *attr1,
                               const AutoFuseAttrs *attr2) {
  if ((attr1->HasFuseType(loop::FuseType::kSplit) && attr2->HasFuseType(loop::FuseType::kReduction)) ||
      (attr1->HasFuseType(loop::FuseType::kReduction) && attr2->HasFuseType(loop::FuseType::kSplit))) {
    GELOGI("node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s][split cannot fuse reduction]",
           node1->GetName().c_str(), node1->GetType().c_str(), node2->GetName().c_str(), node2->GetType().c_str(),
           ge::NotFuseReasonCode(ge::NotFuseReason::kSplitCanNotFuseReduction));
    return false;
  }
  return true;
}

// 检查Split前向融合
bool CheckSplitForwardFusion(const NodePtr &node1, const NodePtr &node2,
                             const AutoFuseAttrs *attr2) {
  if (BackendUtils::IsVertical(node1, node2) && attr2->HasFuseType(loop::FuseType::kSplit)) {
    GELOGI("node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s][node1 and node2 are vertical fuse, "
           "and node2 is split, split cannot fuse forward]", node1->GetName().c_str(), node1->GetType().c_str(),
           node2->GetName().c_str(), node2->GetType().c_str(),
           ge::NotFuseReasonCode(ge::NotFuseReason::kSplitCanNotFuseForward));
    return false;
  }
  return true;
}

// 检查Split水平融合
bool CheckSplitHorizontalFusion(const NodePtr &node1, const NodePtr &node2) {
  if (!BackendUtils::IsVertical(node1, node2)) {
    GELOGI("node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s][split cannot fuse other node horizontal.]",
           node1->GetName().c_str(), node1->GetType().c_str(), node2->GetName().c_str(), node2->GetType().c_str(),
           ge::NotFuseReasonCode(ge::NotFuseReason::kSplitCanNotFuseHorizontal));
    return false;
  }
  return true;
}
} // namespace

bool SplitFusionStrategy::CanFuse(const NodePtr &node1, const NodePtr &node2) {
  const auto attr1 = BackendUtils::GetNodeAutoFuseAttr(node1);
  GE_ASSERT_NOTNULL(attr1);
  const auto attr2 = BackendUtils::GetNodeAutoFuseAttr(node2);
  GE_ASSERT_NOTNULL(attr2);

  // 两个Split节点融合检查
  if (attr1->HasFuseType(loop::FuseType::kSplit) && attr2->HasFuseType(loop::FuseType::kSplit)) {
    GELOGI("node1 %s(%s) has split global id %d, node2 %s(%s) has split global id %d.",
           node1->GetName().c_str(), node1->GetType().c_str(), attr1->GetSplitGlobalId(),
           node2->GetName().c_str(), node2->GetType().c_str(), attr2->GetSplitGlobalId());
    return CheckSplitSplitFusion(node1, node2, attr1, attr2);
  }

  // 检查低融合比例
  if (!CheckSplitLowFusionRatio(node1, node2, attr1)) {
    return false;
  }

  // split不与reduction融合
  if (!CheckSplitReductionFusion(node1, node2, attr1, attr2)) {
    return false;
  }

  // node2 为 Split 类型时，不支持向前融合
  if (!CheckSplitForwardFusion(node1, node2, attr2)) {
    return false;
  }

  // split不做水平融合
  if (!CheckSplitHorizontalFusion(node1, node2)) {
    return false;
  }

  // 检查Split与Reshape融合的特殊场景
  if (attr1->HasFuseType(loop::FuseType::kSplit) && !CheckSplitReshapeFusion(node1, node2)) {
    GELOGI("node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s]"
           "[split cannot fuse reshape that only squeezes dims with size 1 when next node cannot fuse]",
           node1->GetName().c_str(), node1->GetType().c_str(), node2->GetName().c_str(), node2->GetType().c_str(),
           ge::NotFuseReasonCode(ge::NotFuseReason::kSplitCanNotFuseReshapeSqueeze));
    return false;
  }

  return true;
}

uint64_t SplitFusionStrategy::GetMaxFusionNodesSize(const NodePtr &node1, const NodePtr &node2) {
  const auto &config = autofuse::AutoFuseConfig::Config().GetFusionStrategySolver();
  uint64_t max_fusion_size = config.max_fusion_size;
  // split和输出融合时不限制个数
  if (BackendUtils::IsVertical(node1, node2)) {
    const auto attr = BackendUtils::GetNodeAutoFuseAttr(node1);
    GE_ASSERT_NOTNULL(attr);
    if (attr->HasFuseType(loop::FuseType::kSplit)) {
      max_fusion_size = std::numeric_limits<uint64_t>::max();
    }
  } else if (BackendUtils::IsHorizontal(node1, node2)) {
    const auto attr1 = BackendUtils::GetNodeAutoFuseAttr(node1);
    GE_ASSERT_NOTNULL(attr1);
    const auto attr2 = BackendUtils::GetNodeAutoFuseAttr(node2);
    GE_ASSERT_NOTNULL(attr2);
    if (attr1->HasFuseType(loop::FuseType::kSplit) && attr2->HasFuseType(loop::FuseType::kSplit)) {
      max_fusion_size = std::numeric_limits<uint64_t>::max();
    }
  }
  GELOGI("node1 %s(*) and node2 %s(*) max_fusion_nodes_size: %lu.", node1->GetName().c_str(), node2->GetName().c_str(),
         max_fusion_size);
  return max_fusion_size;
}

FusionPriority SplitFusionStrategy::GetFusionPairPriority(const NodePtr &node1, const NodePtr &node2) {
  const auto attr1 = BackendUtils::GetNodeAutoFuseAttr(node1);
  GE_ASSERT_NOTNULL(attr1);
  const auto attr2 = BackendUtils::GetNodeAutoFuseAttr(node2);
  GE_ASSERT_NOTNULL(attr2);
  auto fusion_priority = FusionPriority::HIGHER;
  if ((attr1->GetFuseType() == loop::FuseType::kSplit) && (attr2->GetFuseType() == loop::FuseType::kSplit)) {
    fusion_priority = FusionPriority::HIGHEST;
    GELOGI("node1 %s(%s) and node2 %s(%s) priority:%u.", node1->GetName().c_str(), node1->GetType().c_str(),
      node2->GetName().c_str(), node2->GetType().c_str(), fusion_priority);
  }
  return fusion_priority;
}

REGISTER_FUSION_STRATEGY(SplitFusionStrategy, loop::FuseType::kSplit);
} // namespace ge