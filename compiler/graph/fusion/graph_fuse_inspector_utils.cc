/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string>
#include <vector>
#include "common/ge_common/debug/ge_log.h"
#include "graph/fusion/fusion_utils.h"
#include "graph/utils/node_adapter.h"
#include "graph/utils/op_type_utils.h"
#include "ge/fusion/graph_fuse_inspector_utils.h"
#include "register/graph_optimizer/fusion_common/graph_pass_util.h"

namespace ge {
namespace fusion {
namespace {
bool ConvertGNodes(const std::vector<GNode> &gnodes, std::vector<NodePtr> &nodes, std::string &error_msg) {
  nodes.clear();
  if (gnodes.empty()) {
    error_msg = "nodes_before_fuse is empty";
    return false;
  }
  for (const auto &g_node : gnodes) {
    auto node = NodeAdapter::GNode2Node(g_node);
    if (node == nullptr) {
      error_msg = "failed to convert GNode to NodePtr";
      return false;
    }
    nodes.emplace_back(node);
  }
  return true;
}

bool CheckOwnerGraph(const std::vector<NodePtr> &nodes, ComputeGraphPtr &owner_graph, std::string &error_msg) {
  owner_graph = nodes.at(0)->GetOwnerComputeGraph();
  if (owner_graph == nullptr) {
    error_msg = "owner graph is null";
    return false;
  }
  for (const auto &node : nodes) {
    GE_ASSERT_NOTNULL(node);
    if (node->GetOwnerComputeGraph() != owner_graph) {
      error_msg = "nodes belong to different graphs, node: " + node->GetName();
      return false;
    }
  }
  return true;
}

bool MarkPassNameOnReplacementNodes(const std::vector<NodePtr> &before_nodes, const std::vector<NodePtr> &after_nodes,
                                    const std::string &pass_name) {
  for (const auto &node : after_nodes) {
    GE_ASSERT_NOTNULL(node);
    GE_ASSERT_NOTNULL(node->GetOpDesc());
    if (fe::GraphPassUtil::StoreAndUpdataOriginFusionPassName(node->GetOpDesc(), before_nodes, pass_name) != SUCCESS) {
      GELOGW("failed to mark pass name on node %s", node->GetName().c_str());
      return false;
    }
  }
  return true;
}
}  // namespace

bool GraphFuseInspectorUtils::CanFuse(const std::vector<GNode> &nodes_before_fuse, AscendString &failed_reason) {
  std::vector<NodePtr> nodes;
  std::string reason;
  if (!ConvertGNodes(nodes_before_fuse, nodes, reason)) {
    failed_reason = reason.c_str();
    return false;
  }

  ComputeGraphPtr owner_graph = nullptr;
  if (!CheckOwnerGraph(nodes, owner_graph, reason)) {
    failed_reason = reason.c_str();
    return false;
  }

  std::string reason_not_support;
  if (!owner_graph->IsSupportFuse(nodes, reason_not_support)) {
    failed_reason = reason_not_support.c_str();
    return false;
  }

  if (FusionUtils::WillCauseCycleIfFuse(nodes)) {
    failed_reason = "it will create cycle after fuse";
    return false;
  }
  return true;
}

Status GraphFuseInspectorUtils::ReportFuse(const std::vector<GNode> &nodes_before_fuse,
                                           const std::vector<GNode> &nodes_after_fuse, CustomPassContext &ctx) {
  std::string error_msg;
  std::vector<NodePtr> before_nodes;
  if (!ConvertGNodes(nodes_before_fuse, before_nodes, error_msg)) {
    GELOGW("%s", error_msg.c_str());
    return FAILED;
  }
  ComputeGraphPtr owner_graph = nullptr;
  if (!CheckOwnerGraph(before_nodes, owner_graph, error_msg)) {
    GELOGW("%s", error_msg.c_str());
    return FAILED;
  }
  std::vector<NodePtr> after_nodes;
  if (!nodes_after_fuse.empty() && !ConvertGNodes(nodes_after_fuse, after_nodes, error_msg)) {
    GELOGW("%s", error_msg.c_str());
    return FAILED;
  }
  if (!after_nodes.empty()) {
    ComputeGraphPtr after_owner_graph = nullptr;
    if (!CheckOwnerGraph(after_nodes, after_owner_graph, error_msg)) {
      GELOGW("%s", error_msg.c_str());
      return FAILED;
    }
    if (owner_graph != after_owner_graph) {
      GELOGW("nodes_after_fuse and nodes_before_fuse belong to different graphs");
      return FAILED;
    }
  }
  const auto pass_name = ctx.GetPassName();
  const auto *pass_name_cstr = pass_name.GetString();
  const std::string pass_name_str = (pass_name_cstr == nullptr) ? "" : pass_name_cstr;
  if (pass_name_str.empty()) {
    return FAILED;
  }
  if (!MarkPassNameOnReplacementNodes(before_nodes, after_nodes, pass_name_str)) {
    return FAILED;
  }
  FusionUtils::RecordFusionStatistic(owner_graph->GetSessionID(), std::to_string(owner_graph->GetGraphID()),
                                     pass_name_str, 1, 1);
  return SUCCESS;
}
}  // namespace fusion
}  // namespace ge
