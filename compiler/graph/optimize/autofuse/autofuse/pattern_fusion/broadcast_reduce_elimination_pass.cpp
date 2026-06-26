/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "broadcast_reduce_elimination_pass.h"
#include <vector>
#include <unordered_set>
#include "graph/utils/graph_utils.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "operator_reg.h"
#include "common/checker.h"
#include "debug/ge_util.h"

namespace ge {
namespace {
// 广播操作类型
constexpr auto kOpTypeBroadcastTo = "BroadcastTo";
constexpr auto kOpTypeFill = "Fill";
constexpr auto kOpTypeTile = "Tile";
constexpr auto kOpTypeTileD = "TileD";

// 归约操作类型（仅支持直接消除的操作，避免数值精度问题）
// 注意：ReduceSum 和 ReduceProd 不支持，因为：
// - Sum -> Mul: broadcast_size 很大时 x*size 可能溢出，而原始累加器更鲁棒
// - Prod -> Pow: Pow 运算比连乘更昂贵且精度波动更大
const std::unordered_set<std::string> kReduceOpTypes = {"ReduceMax",  "ReduceMin",  "ReduceMean",
                                                        "ReduceMaxD", "ReduceMinD", "ReduceMeanD"};

constexpr auto kAttrNameAxes = "axes";
constexpr auto kAttrNameShape = "shape";
constexpr auto kAttrNameMultiples = "multiples";
constexpr auto kAttrNameKeepDims = "keep_dims";

bool IsReduceNode(const NodePtr &node) {
  GE_ASSERT_NOTNULL(node);
  return kReduceOpTypes.count(node->GetType()) > 0UL;
}

bool IsBroadcastNode(const NodePtr &node) {
  GE_ASSERT_NOTNULL(node);
  const auto &op_type = node->GetType();
  return op_type == kOpTypeBroadcastTo || op_type == kOpTypeFill || op_type == kOpTypeTile || op_type == kOpTypeTileD;
}

// 标准化轴索引（处理负数轴）
int64_t NormalizeAxis(int64_t axis, int64_t rank) {
  if (axis < 0) {
    return axis + rank;
  }
  return axis;
}

// 获取 BroadcastTo 操作的广播轴（通过 shape 推导）
// 使用标准广播语义：从右向左对齐
bool GetBroadcastToAxes(const NodePtr &brc_node, std::vector<int64_t> &brc_axes) {
  brc_axes.clear();
  const auto &op_desc = brc_node->GetOpDesc();
  GE_ASSERT_NOTNULL(op_desc);

  if (op_desc->GetInputsSize() == 0) {
    return false;
  }

  const auto &input_shape = op_desc->GetInputDesc(0).GetShape().GetDims();
  const auto &output_shape = op_desc->GetOutputDesc(0).GetShape().GetDims();
  GE_ASSERT_TRUE(input_shape.size() <= output_shape.size(), "BroadcastTo %s: input rank > output rank",
                 brc_node->GetNamePtr());

  // 检查输入 shape 是否有动态维度（输出可以有 -1）
  // 输入有 -1 时无法确定是否是广播轴，不能优化
  for (auto dim : input_shape) {
    if (dim == -1) {
      GELOGW("BroadcastTo %s has dynamic input shape", brc_node->GetNamePtr());
      return false;
    }
  }

  // 从右向左对齐（标准广播语义）
  // 例如：input [c, d] -> output [a, b, c, d]
  // 实际上是 [1, 1, c, d] -> [a, b, c, d]
  auto input_rank = static_cast<int64_t>(input_shape.size());
  auto output_rank = static_cast<int64_t>(output_shape.size());
  int64_t rank_diff = output_rank - input_rank;

  for (int64_t i = 0; i < output_rank; ++i) {
    int64_t in_dim = (i >= rank_diff) ? input_shape[i - rank_diff] : 1;
    // in_dim=1 且 output_dim!=1 时是广播轴（output_dim 可以是 -1，表示动态广播）
    if (in_dim == 1 && output_shape[i] != 1) {
      brc_axes.push_back(i);
    }
  }

  return !brc_axes.empty();
}

// 获取 Fill 的广播轴（所有输出维度都是广播轴）
bool GetFillBroadcastAxes(const NodePtr &fill_node, std::vector<int64_t> &brc_axes) {
  brc_axes.clear();
  const auto &op_desc = fill_node->GetOpDesc();
  GE_ASSERT_NOTNULL(op_desc);

  if (op_desc->GetInputsSize() < 2UL) {
    return false;
  }

  // 检查 value 输入（index 1）是否有动态 shape
  // 如果 value shape 包含 -1，无法确定 reshape 目标，不能优化
  const auto &value_shape = op_desc->GetInputDesc(1).GetShape().GetDims();
  for (auto dim : value_shape) {
    if (dim == -1) {
      GELOGW("Fill %s has dynamic value shape, cannot optimize", fill_node->GetNamePtr());
      return false;
    }
  }

  const auto &output_shape = op_desc->GetOutputDesc(0).GetShape().GetDims();
  for (size_t i = 0; i < output_shape.size(); ++i) {
    brc_axes.push_back(static_cast<int64_t>(i));
  }

  return !brc_axes.empty();
}

bool ReadIntVecFromInput(const NodePtr &node, const std::string &input_name, std::vector<int64_t> &values) {
  const auto op = ge::OpDescUtils::CreateOperatorFromNode(node);
  ge::Tensor tensor;
  if (op.GetInputConstData(input_name.c_str(), tensor) != ge::SUCCESS) {
    GELOGD("ReadIntVecFromInput %s: GetInputConstData failed for input '%s'", node->GetNamePtr(), input_name.c_str());
    return false;
  }
  if (tensor.GetData() == nullptr) {
    GELOGD("ReadIntVecFromInput %s: tensor data is null for input '%s'", node->GetNamePtr(), input_name.c_str());
    return false;
  }
  const auto &dims = tensor.GetTensorDesc().GetShape().GetDims();
  if (dims.size() > 1U) {
    GELOGD("ReadIntVecFromInput %s: input '%s' dims size %zu is not scalar or 1D", node->GetNamePtr(),
           input_name.c_str(), dims.size());
    return false;
  }
  const int64_t num_elems = dims.empty() ? 1L : dims[0];
  const auto dtype = tensor.GetTensorDesc().GetDataType();
  for (int64_t i = 0L; i < num_elems; ++i) {
    if (dtype == ge::DT_INT32) {
      values.push_back(reinterpret_cast<const int32_t *>(tensor.GetData())[i]);
    } else if (dtype == ge::DT_INT64) {
      values.push_back(reinterpret_cast<const int64_t *>(tensor.GetData())[i]);
    } else {
      GELOGW("ReadIntVecFromInput %s: unsupported dtype=%d for input '%s'", node->GetNamePtr(),
             static_cast<int32_t>(dtype), input_name.c_str());
      return false;
    }
  }
  return true;
}

// 获取 Tile 的 multiples（优先属性，fallback 到输入 tensor）
bool GetTileMultiples(const NodePtr &tile_node, std::vector<int64_t> &multiples) {
  const auto &op_desc = tile_node->GetOpDesc();
  if (AttrUtils::GetListInt(op_desc, kAttrNameMultiples, multiples)) {
    return true;
  }
  return ReadIntVecFromInput(tile_node, kAttrNameMultiples, multiples);
}

// 获取 Tile 的广播轴（输入维度=1 且 multiples>1 的轴）
bool GetTileBroadcastAxes(const NodePtr &tile_node, std::vector<int64_t> &brc_axes) {
  brc_axes.clear();
  const auto &op_desc = tile_node->GetOpDesc();
  GE_ASSERT_NOTNULL(op_desc);

  std::vector<int64_t> multiples;
  if (!GetTileMultiples(tile_node, multiples)) {
    GELOGD("Tile %s: cannot get multiples from attr or input", tile_node->GetNamePtr());
    return false;
  }

  const auto &input_shape = op_desc->GetInputDesc(0).GetShape().GetDims();
  if (multiples.size() != input_shape.size()) {
    GELOGW("Tile %s: multiples.size() != input_shape.size()", tile_node->GetNamePtr());
    return false;
  }

  // 检查动态 shape
  for (auto dim : input_shape) {
    if (dim == -1) {
      GELOGW("Tile %s has dynamic input shape", tile_node->GetNamePtr());
      return false;
    }
  }
  for (auto mult : multiples) {
    if (mult == -1) {
      GELOGW("Tile %s has dynamic multiples", tile_node->GetNamePtr());
      return false;
    }
  }

  // 找出广播轴（输入维度=1 且 multiples>1 的轴）
  for (size_t i = 0; i < input_shape.size(); ++i) {
    if (input_shape[i] == 1 && multiples[i] > 1) {
      brc_axes.push_back(static_cast<int64_t>(i));
    }
  }

  return !brc_axes.empty();
}

bool GetReduceAttrs(const NodePtr &reduce_node, std::vector<int64_t> &reduce_axes, bool &keep_dims) {
  reduce_axes.clear();
  keep_dims = false;
  const auto &op_desc = reduce_node->GetOpDesc();
  GE_ASSERT_NOTNULL(op_desc);
  GE_ASSERT_TRUE(AttrUtils::GetBool(op_desc, kAttrNameKeepDims, keep_dims));

  if (AttrUtils::GetListInt(op_desc, kAttrNameAxes, reduce_axes)) {
    return true;
  }
  // 非 D 变体：axes 是第二个输入 tensor，动态获取输入名
  if (reduce_node->GetAllInDataAnchorsSize() >= 2U) {
    std::string input_name = op_desc->GetInputNameByIndex(1U);
    if (ReadIntVecFromInput(reduce_node, input_name, reduce_axes)) {
      return !reduce_axes.empty();
    }
  }

  GELOGW("GetReduceAttrs %s: failed to get reduce axes from attr or input", reduce_node->GetNamePtr());
  return false;
}

// 按 broadcast 类型获取广播轴
bool GetBroadcastAxes(const NodePtr &brc_node, std::vector<int64_t> &brc_axes) {
  const auto &brc_type = brc_node->GetType();

  if (brc_type == kOpTypeBroadcastTo) {
    return GetBroadcastToAxes(brc_node, brc_axes);
  } else if (brc_type == kOpTypeFill) {
    return GetFillBroadcastAxes(brc_node, brc_axes);
  } else if (brc_type == kOpTypeTile || brc_type == kOpTypeTileD) {
    return GetTileBroadcastAxes(brc_node, brc_axes);
  }

  return false;
}

// 优化类型
enum class EliminationType { kFullElimination, kNoElimination };

EliminationType AnalyzeBroadcastReduce(const std::vector<int64_t> &brc_axes, const std::vector<int64_t> &reduce_axes) {
  // broadcast 轴和 reduce 轴必须完全一致：
  // - reduce 不能覆盖非广播轴（会丢失真实数据计算）
  // - 所有广播轴都必须被 reduce（否则输出仍含广播维度）
  const std::unordered_set<int64_t> brc_set(brc_axes.begin(), brc_axes.end());
  const std::unordered_set<int64_t> reduce_set(reduce_axes.begin(), reduce_axes.end());
  if (brc_set != reduce_set) {
    return EliminationType::kNoElimination;
  }
  return EliminationType::kFullElimination;
}

// 检查是否有其他消费者
bool HasOtherConsumers(const NodePtr &brc_node, const NodePtr &reduce_node) {
  for (const auto &out_anchor : brc_node->GetAllOutDataAnchors()) {
    for (const auto &in_anchor : out_anchor->GetPeerInDataAnchors()) {
      if (in_anchor->GetOwnerNode() != reduce_node) {
        return true;
      }
    }
  }
  return false;
}

// 计算 Reduce 输出 shape
std::vector<int64_t> ComputeReduceOutputShape(const std::vector<int64_t> &broadcast_output_dims,
                                              const std::vector<int64_t> &reduce_axes, bool keep_dims) {
  const std::unordered_set<int64_t> reduce_set(reduce_axes.begin(), reduce_axes.end());
  std::vector<int64_t> output_dims;
  for (size_t i = 0; i < broadcast_output_dims.size(); ++i) {
    if (reduce_set.count(static_cast<int64_t>(i)) > 0) {
      if (keep_dims) {
        output_dims.push_back(1);
      }
    } else {
      output_dims.push_back(broadcast_output_dims[i]);
    }
  }
  return output_dims;
}

// 替换节点并清理
Status ReplaceAndCleanup(const ComputeGraphPtr &graph, const NodePtr &brc_node, const NodePtr &reduce_node,
                         const NodePtr &replacement_node) {
  // 迁移数据边
  for (const auto &out_anchor : reduce_node->GetAllOutDataAnchors()) {
    for (const auto &in_anchor : out_anchor->GetPeerInDataAnchors()) {
      GE_ASSERT_GRAPH_SUCCESS(GraphUtils::RemoveEdge(out_anchor, in_anchor));
      GE_ASSERT_GRAPH_SUCCESS(GraphUtils::AddEdge(replacement_node->GetOutDataAnchor(0), in_anchor));
    }
  }

  // 迁移 Reduce 节点的入控制边
  for (const auto &ctrl_in : reduce_node->GetInControlNodes()) {
    GE_ASSERT_GRAPH_SUCCESS(GraphUtils::RemoveEdge(ctrl_in->GetOutControlAnchor(), reduce_node->GetInControlAnchor()));
    GE_ASSERT_GRAPH_SUCCESS(
        GraphUtils::AddEdge(ctrl_in->GetOutControlAnchor(), replacement_node->GetInControlAnchor()));
  }

  // 迁移 Broadcast 节点的入控制边（保持执行顺序依赖）
  for (const auto &ctrl_in : brc_node->GetInControlNodes()) {
    GE_ASSERT_GRAPH_SUCCESS(GraphUtils::RemoveEdge(ctrl_in->GetOutControlAnchor(), brc_node->GetInControlAnchor()));
    GE_ASSERT_GRAPH_SUCCESS(
        GraphUtils::AddEdge(ctrl_in->GetOutControlAnchor(), replacement_node->GetInControlAnchor()));
  }

  NodeUtils::UnlinkAll(*brc_node);
  NodeUtils::UnlinkAll(*reduce_node);
  GE_ASSERT_GRAPH_SUCCESS(graph->RemoveNode(brc_node));
  GE_ASSERT_GRAPH_SUCCESS(graph->RemoveNode(reduce_node));
  return ge::SUCCESS;
}

// 创建 Reshape 节点
NodePtr CreateReshapeNode(const ComputeGraphPtr &graph, const NodePtr &input, const std::vector<int64_t> &target_shape,
                          const std::string &name) {
  auto op_desc = std::make_shared<OpDesc>(name, "Reshape");
  GE_ASSERT_NOTNULL(op_desc);

  GeTensorDesc input_desc = input->GetOpDesc()->GetOutputDesc(0);
  GE_ASSERT_GRAPH_SUCCESS(op_desc->AddInputDesc(input_desc));

  // 设置输出 shape
  GeTensorDesc output_desc(input_desc);
  output_desc.SetShape(GeShape(target_shape));
  GE_ASSERT_GRAPH_SUCCESS(op_desc->AddOutputDesc(output_desc));

  // 设置 shape 属性
  GE_ASSERT_TRUE(AttrUtils::SetListInt(op_desc, kAttrNameShape, target_shape));

  auto reshape_node = graph->AddNode(op_desc);
  GE_ASSERT_NOTNULL(reshape_node);
  GE_ASSERT_GRAPH_SUCCESS(GraphUtils::AddEdge(input->GetOutDataAnchor(0), reshape_node->GetInDataAnchor(0)),
                          "Failed to add edge to Reshape node %s", reshape_node->GetNamePtr());

  return reshape_node;
}

// 创建替换节点
NodePtr CreateReplacementNode(const ComputeGraphPtr &graph, const NodePtr &input,
                              const std::vector<int64_t> &target_shape, const std::string &base_name) {
  auto input_dims = input->GetOpDesc()->GetOutputDesc(0).GetShape().GetDims();
  // shape 完全相同，直接用原始输入
  if (input_dims == target_shape) {
    return input;
  }

  // shape 不同，用 Reshape 节点
  return CreateReshapeNode(graph, input, target_shape, base_name + "_reshape");
}

// 获取 broadcast 的数据输入节点
NodePtr GetBroadcastInput(const NodePtr &brc_node) {
  if (brc_node->GetType() == kOpTypeFill) {
    return NodeUtils::GetInDataNodeByIndex(*brc_node, 1);
  }
  return NodeUtils::GetInDataNodeByIndex(*brc_node, 0);
}

// 处理 Broadcast + Reduce 优化
graphStatus ProcessBroadcastReduce(const NodePtr &brc_node, const NodePtr &reduce_node, const ComputeGraphPtr &graph,
                                   bool &changed) {
  // 1. 获取广播轴
  std::vector<int64_t> brc_axes;
  if (!GetBroadcastAxes(brc_node, brc_axes)) {
    return GRAPH_SUCCESS;
  }

  // 2. 获取归约属性并标准化
  std::vector<int64_t> reduce_axes;
  bool keep_dims = false;
  if (!GetReduceAttrs(reduce_node, reduce_axes, keep_dims)) {
    return GRAPH_SUCCESS;
  }

  int64_t output_rank = brc_node->GetOpDesc()->GetOutputDesc(0).GetShape().GetDims().size();
  for (auto &axis : reduce_axes) {
    axis = NormalizeAxis(axis, output_rank);
  }

  // 3. 检查是否可优化
  if (AnalyzeBroadcastReduce(brc_axes, reduce_axes) == EliminationType::kNoElimination) {
    return GRAPH_SUCCESS;
  }

  // 4. 获取广播输入
  auto brc_input = GetBroadcastInput(brc_node);
  GE_ASSERT_NOTNULL(brc_input);

  // 5. 创建替换节点并执行消除
  auto broadcast_output_dims = brc_node->GetOpDesc()->GetOutputDesc(0).GetShape().GetDims();
  auto reduce_output_dims = ComputeReduceOutputShape(broadcast_output_dims, reduce_axes, keep_dims);
  auto replacement_node = CreateReplacementNode(graph, brc_input, reduce_output_dims,
                                                std::string(brc_input->GetNamePtr()) + "_" + reduce_node->GetNamePtr());
  if (!replacement_node) {
    GELOGW("Failed to create replacement node for %s + %s", brc_node->GetNamePtr(), reduce_node->GetNamePtr());
    return GRAPH_SUCCESS;
  }

  GELOGD("BroadcastReduceElimination: eliminating %s + %s (keep_dims=%d)", brc_node->GetType().c_str(),
         reduce_node->GetType().c_str(), keep_dims);
  GE_ASSERT_SUCCESS(ReplaceAndCleanup(graph, brc_node, reduce_node, replacement_node));
  changed = true;
  return GRAPH_SUCCESS;
}
}  // namespace

graphStatus BroadcastReduceEliminationPass::Run(const ComputeGraphPtr &graph, bool &changed) const {
  GE_ASSERT_NOTNULL(graph);
  std::vector<std::pair<NodePtr, NodePtr> > pairs_to_process;
  for (const auto &node : graph->GetDirectNode()) {
    GE_ASSERT_NOTNULL(node);

    if (!IsReduceNode(node)) {
      continue;
    }

    auto input_node = NodeUtils::GetInDataNodeByIndex(*node, 0);
    if ((input_node == nullptr) || !IsBroadcastNode(input_node)) {
      continue;
    }

    if (HasOtherConsumers(input_node, node)) {
      continue;
    }

    pairs_to_process.emplace_back(input_node, node);
  }

  uint32_t optimized_count = 0U;
  for (const auto &pair : pairs_to_process) {
    bool cur_changed = false;
    auto ret = ProcessBroadcastReduce(pair.first, pair.second, graph, cur_changed);
    if (ret != GRAPH_SUCCESS) {
      GELOGW("Failed to process broadcast-reduce pattern for %s + %s", pair.first->GetNamePtr(),
             pair.second->GetNamePtr());
      continue;
    }

    if (cur_changed) {
      optimized_count++;
    }
  }

  if (optimized_count > 0U) {
    changed = true;
    GELOGI("BroadcastReduceEliminationPass: optimized %u patterns", optimized_count);
  }

  return GRAPH_SUCCESS;
}
}  // namespace ge
