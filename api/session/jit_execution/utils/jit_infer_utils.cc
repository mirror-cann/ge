/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "jit_infer_utils.h"
#include "common/memory/tensor_trans_utils.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_shape_inference.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_info_pre_processor.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"
#include "graph/optimize/symbolic/symbolic_kernel_factory.h"
#include "graph/utils/op_type_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/utils/constant_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "attribute_group/attr_group_symbolic_desc.h"
#include "base/registry/op_impl_space_registry_v2.h"
#include "opt_info/ge_opt_info.h"
#include "graph/optimize/autofuse/autofuse/lowering/lowering_query.h"

#include <unordered_set>

namespace ge {
namespace {
/**
 * 判断node是否为uninfered node, uninfered node会被切到下一张图
 *
 * 1、if/case节点, 符号化暂不支持对子图的推导, 因此if和case的输出必没有符号, 切到下一张图后可以根据上一张图的输出把cond构造成const,
 *    然后对if/case做剪枝, 消除if/case节点, 把子图展平到根图上, 达成对if/case做符号化推导的目的
 * 2、存在没有符号的输入且所有输出都没符号
 *
 * @param node
 * @return
 */
bool IsUnInferredNode(const NodePtr &node) {
  const auto op_desc = node->GetOpDesc();
  if (OpTypeUtils::IsGraphInputNode(op_desc->GetType())) {
    return false;
  }
  // 算子的任何一个输出有符号，说明推导成功，需要切到ep中执行
  const auto &outputs = op_desc->GetAllOutputsDescPtr();
  const bool has_valid_output = std::any_of(outputs.begin(), outputs.end(),
      [](const GeTensorDescPtr &desc) { return desc->GetAttrsGroup<SymbolicDescAttr>() != nullptr; });
  if (has_valid_output) {
    GELOGD("[%s][%s] is inferred successfully.", op_desc->GetNamePtr(), op_desc->GetTypePtr());
    return false;
  }
  // 存在空输入，说明是受上游算子影响，切到下一张图
  const auto &inputs = op_desc->GetAllInputsDescPtr();
  const bool has_empty_input = std::any_of(inputs.begin(), inputs.end(),
      [](const GeTensorDescPtr &desc) { return desc->GetAttrsGroup<SymbolicDescAttr>() == nullptr; });
  if (has_empty_input) {
    return true;
  }
  // 所有输入都有符号，但是所有输出都没有符号
  // if/case节点，且条件输入不是data，切到下一张图
  const auto cond_input = SymbolicInferUtil::GetCondInput(node);
  if (cond_input != nullptr && !OpTypeUtils::IsDataNode(cond_input->GetType())) {
    GELOGD("[%s][%s] is cond type without data input.", node->GetNamePtr(), node->GetTypePtr());
    return true;
  }
  // 可能就是断点，需要放在ep中执行
  GELOGI("[%s][%s] may be the breakpoint.", node->GetNamePtr(), node->GetTypePtr());
  return false;
}

bool ParentNodeInferred(const NodePtr &node, std::vector<NodePtr> &inferred_nodes) {
  // check parents data nodes
  const auto in_inferred = [&inferred_nodes](const NodePtr &n) {
    return std::find(inferred_nodes.begin(), inferred_nodes.end(), n) != inferred_nodes.end();
  };
  const auto &data_nodes = node->GetInDataNodes();
  // check parents control nodes
  const auto &ctrl_nodes = node->GetInControlNodes();
  return std::all_of(data_nodes.begin(), data_nodes.end(), in_inferred) &&
         std::all_of(ctrl_nodes.begin(), ctrl_nodes.end(), in_inferred);
}

void DeleteNodesWithoutParentNode(std::vector<NodePtr> &inferred_nodes) {
  // delete nodes whose parents node not infered
  for (auto it = inferred_nodes.begin(); it != inferred_nodes.end();) {
    if (!ParentNodeInferred(*it, inferred_nodes)) {
      GELOGD("Infer node:%s. Parent node uninfered", (*it)->GetName().c_str());
      it = inferred_nodes.erase(it);
    } else {
      ++it;
    }
  }
}

bool HasUnsuppliableInput(const NodePtr &node, const OpDescPtr &op_desc,
                          const gert::OpImplKernelRegistry::OpImplFunctionsV2 &func) {
  size_t invalid_index_num = 0UL;
  for (size_t i = 0UL; i < op_desc->GetAllInputsSize(); ++i) {
    const auto in_anchor = node->GetInDataAnchor(static_cast<int32_t>(i));
    if (in_anchor == nullptr || in_anchor->GetPeerOutAnchor() == nullptr) {
      invalid_index_num++;
      continue;
    }
    size_t ir_index = 0;
    const bool has_ir = (OpDescUtils::GetInputIrIndexByInstanceIndex(op_desc, i - invalid_index_num, ir_index) == SUCCESS);
    if (!has_ir || !func.IsInputDataDependency(ir_index)) { continue; }
    const auto source_node = in_anchor->GetPeerOutAnchor()->GetOwnerNode();
    if (source_node == nullptr || ConstantUtils::IsConstant(source_node) ||
        SymbolicKernelFactory::GetInstance().Create(source_node->GetType()) != nullptr) {
      continue;
    }
    return true;
  }
  return false;
}

bool ShouldKeepAsCutBoundary(const NodePtr &node, const OpDescPtr &op_desc) {
  if (!op_desc->GetSubgraphInstanceNames().empty()) {
    GELOGD("[%s][%s] has subgraph, keep it as cut boundary.", op_desc->GetNamePtr(), op_desc->GetTypePtr());
    return true;
  }
  const auto cond_input = SymbolicInferUtil::GetCondInput(node);
  if (cond_input != nullptr && !OpTypeUtils::IsDataNode(cond_input->GetType())) {
    GELOGD("[%s][%s] cond input is not data, keep it as cut boundary.", op_desc->GetNamePtr(), op_desc->GetTypePtr());
    return true;
  }
  return false;
}

struct PullableCategories {
  std::unordered_set<NodePtr> no_callback;        // 没注册回调（无compute且无infer）
  std::unordered_set<NodePtr> unsuppliable_input;  // 值依赖无法满足
  std::unordered_set<NodePtr> no_lowering;         // 没注册lowering

  bool IsPullable(const NodePtr &node) const {
    return no_callback.count(node) || unsuppliable_input.count(node) || no_lowering.count(node);
  }
};

void ClassifyPullable(const NodePtr &node, PullableCategories &categories) {
  const auto op_desc = node->GetOpDesc();
  if (op_desc == nullptr || OpTypeUtils::IsGraphOutputNode(op_desc->GetType())) {
    return;
  }
  if (ShouldKeepAsCutBoundary(node, op_desc)) {
    return;
  }
  // 优先级1: 没注册lowering — 这是最根本的原因，短路后续判断
  if (!HasLoweringRegistered(op_desc->GetType())) {
    categories.no_lowering.insert(node);
    return;
  }
  const auto &outputs = op_desc->GetAllOutputsDescPtr();
  const bool all_static = std::all_of(outputs.begin(), outputs.end(),
      [](const GeTensorDescPtr &desc) {
        return desc->IsOriginShapeInitialized() && !desc->GetOriginShape().IsUnknownShape();
      });
  if (all_static) {
    return;
  }
  // 优先级2: 没注册回调（无compute且无infer）
  const bool has_compute = (SymbolicKernelFactory::GetInstance().Create(op_desc->GetType()) != nullptr);
  const auto infer_funcs = gert::OpImplInferSymbolShapeRegistry::GetInstance().GetOpImpl(op_desc->GetType().c_str());
  const bool has_infer = (infer_funcs != nullptr && infer_funcs->infer_symbol_shape != nullptr);
  if (!has_compute && !has_infer) {
    categories.no_callback.insert(node);
    return;
  }
  // 优先级3: 值依赖无法满足
  const auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  if (space_registry != nullptr) {
    const auto *func = space_registry->GetOpImpl(op_desc->GetType().c_str());
    if (func != nullptr && HasUnsuppliableInput(node, op_desc, *func)) {
      categories.unsuppliable_input.insert(node);
    }
  }
}

void ClassifyNodes(const ComputeGraphPtr &graph, std::vector<NodePtr> &inferred_nodes,
                   std::vector<NodePtr> &uninferred_nodes, PullableCategories &pullable_categories,
                   std::vector<NodePtr> &net_output_nodes) {
  for (const auto &node : graph->GetDirectNode()) {
    if (OpTypeUtils::IsGraphOutputNode(node->GetType())) {
      net_output_nodes.push_back(node);
      continue;
    }
    if (IsUnInferredNode(node)) {
      uninferred_nodes.push_back(node);
      ClassifyPullable(node, pullable_categories);
    } else {
      inferred_nodes.push_back(node);
    }
  }
}

void ClearInferredNodesWithAllDataNodes(std::vector<NodePtr> &inferred_nodes) {
  for (const auto &node : inferred_nodes) {
    if (!OpTypeUtils::IsGraphInputNode(node->GetType())) {
      return;
    }
  }
  inferred_nodes.clear();
}

void AssignNetOutputNodes(std::vector<NodePtr> &net_output_nodes, std::vector<NodePtr> &inferred_nodes,
                          std::vector<NodePtr> &uninferred_nodes) {
  for (const auto &node : net_output_nodes) {
    if (ParentNodeInferred(node, inferred_nodes)) {
      GELOGD("NetOutput %s moved to inferred nodes.", node->GetName().c_str());
      inferred_nodes.push_back(node);
    } else {
      uninferred_nodes.push_back(node);
    }
  }
}

void PropagatePullable(std::vector<NodePtr> &uninferred_nodes, std::vector<NodePtr> &inferred_nodes,
                       const PullableCategories &pullable_categories) {
  // 维护inferred_set做O(1)父节点查询，避免ParentNodeInferred每轮线性扫描inferred_nodes
  std::unordered_set<NodePtr> inferred_set(inferred_nodes.begin(), inferred_nodes.end());
  const auto parent_inferred = [&inferred_set](const NodePtr &node) {
    const auto in_set = [&inferred_set](const NodePtr &n) { return inferred_set.count(n) > 0; };
    const auto &data_nodes = node->GetInDataNodes();
    const auto &ctrl_nodes = node->GetInControlNodes();
    return std::all_of(data_nodes.begin(), data_nodes.end(), in_set) &&
           std::all_of(ctrl_nodes.begin(), ctrl_nodes.end(), in_set);
  };
  while (true) {
    bool changed = false;
    for (auto it = uninferred_nodes.begin(); it != uninferred_nodes.end();) {
      auto &node = *it;
      if (!pullable_categories.IsPullable(node) || !parent_inferred(node)) {
        ++it;
        continue;
      }
      // 分类日志: 记录节点被拉入ep的原因
      const char *reason = "unknown";
      if (pullable_categories.no_callback.count(node)) {
        reason = "no callback registered (no compute and no infer)";
      } else if (pullable_categories.unsuppliable_input.count(node)) {
        reason = "value dependency unsuppliable";
      } else if (pullable_categories.no_lowering.count(node)) {
        reason = "no lowering registered";
      }
      GELOGI("[%s][%s] pulled into current EP, reason: %s.", node->GetNamePtr(), node->GetTypePtr(), reason);
      inferred_nodes.push_back(node);
      inferred_set.insert(node);
      it = uninferred_nodes.erase(it);
      changed = true;
    }
    if (!changed) { break; }
  }
}
}  // namespace

Status JitInferUtils::InferSymbolForGraph(const ComputeGraphPtr &graph, const std::vector<GeTensor> &inputs,
                                          std::vector<NodePtr> &inferred_nodes) {
  GE_ASSERT_SUCCESS(PrepareBeforeInferSymbol(graph, inputs));
  GE_ASSERT_SUCCESS(InferGraphAndGetInferredNodes(graph, inputs, inferred_nodes));
  return SUCCESS;
}

Status JitInferUtils::PrepareBeforeInferSymbol(const ComputeGraphPtr &graph, const std::vector<GeTensor> &inputs) {
  GE_ASSERT_SUCCESS(GeOptInfo::SetOptInfo());

  GraphNodePtr graph_node = make_shared<GraphNode>(0);
  graph_node->SetComputeGraph(graph);
  GraphPtr graph_ptr = std::make_shared<Graph>(GraphUtilsEx::CreateGraphFromComputeGraph(graph));
  graph_node->SetGraph(graph_ptr);

  GraphPrepare graph_prepare;
  GraphOptimize graph_optimize;
  GE_ASSERT_SUCCESS(graph_prepare.PrepareInit(graph_node, graph->GetSessionID()));
  GE_ASSERT_SUCCESS(graph_optimize.HandleSummaryOp(graph));
  graph_prepare.SetGraphNormalized(true);  // to skip GraphOptimize::OptimizeAfterGraphNormalization
  GE_ASSERT_SUCCESS(graph_prepare.NormalizeGraph(graph, graph_node->GetOptions(), inputs));
  GE_ASSERT_SUCCESS(graph_prepare.PrepareDynShape());

  return SUCCESS;
}

Status JitInferUtils::InferGraphAndGetInferredNodes(const ComputeGraphPtr &graph, const std::vector<GeTensor> &inputs,
                                                    std::vector<NodePtr> &inferred_nodes) {
  bool is_single_op_scene = false;
  ge::AttrUtils::GetBool(graph, ge::ATTR_SINGLE_OP_SCENE, is_single_op_scene);
  if (is_single_op_scene) {
    GELOGI("Skip auto fuse for single op scene.");
    return SUCCESS;
  }
  GE_ASSERT_GRAPH_SUCCESS(SymbolicShapeSymbolizer::Symbolize(graph, inputs), "Symbolize graph input failed, graph %s",
                          graph->GetName().c_str());
  SymbolicShapeInference symbolic_shape_inference;
  GE_ASSERT_SUCCESS(symbolic_shape_inference.Infer(graph));
  std::vector<NodePtr> uninferred_nodes;
  PullableCategories pullable_categories;
  std::vector<NodePtr> net_output_nodes;
  // 1. 对节点进行分类
  ClassifyNodes(graph, inferred_nodes, uninferred_nodes, pullable_categories, net_output_nodes);
  // 2. ep会拉入一些推导失败的节点，尽可能减少整张图产生的eo数量
  PropagatePullable(uninferred_nodes, inferred_nodes, pullable_categories);
  // 3. netoutput节点单独处理
  AssignNetOutputNodes(net_output_nodes, inferred_nodes, uninferred_nodes);
  // 4. data节点单独处理
  ClearInferredNodesWithAllDataNodes(inferred_nodes);
  // 5. 去除悬空节点
  DeleteNodesWithoutParentNode(inferred_nodes);
  GELOGD("Infer node size: %zu", inferred_nodes.size());
  return SUCCESS;
}
}  // namespace ge