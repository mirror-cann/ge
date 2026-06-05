/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <algorithm>
#include <fstream>
#include <unordered_set>
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/op_type_utils.h"
#include "register/graph_optimizer/fusion_common/graph_pass_util.h"
#include "graph/ge_context.h"
#include "common/util.h"
#include "common/plugin/ge_make_unique_util.h"
#include "common/framework_types_internal.h"
#include "nlohmann/json.hpp"
#include "fusion_utils.h"

namespace ge {
namespace fusion {
namespace {
constexpr char_t const *kGraphFusionKey = "GraphFusion";
constexpr char_t const *kSwitchKey = "Switch";
constexpr char_t const *kSwitchOn = "on";
constexpr char_t const *kSwitchOff = "off";

std::string SwitchToString(const std::map<string, bool> &pass_name_2_switches) {
  std::stringstream ss;
  for (const auto &pass_name_2_switch : pass_name_2_switches) {
    ss << "[" << pass_name_2_switch.first << ",";
    std::string switch_str = pass_name_2_switch.second ? kSwitchOn : kSwitchOff;
    ss << switch_str << "]";
  }
  return ss.str();
}

Status ReadJsonFile(const std::string &file_real_path, nlohmann::json &json_obj) {
  std::ifstream file_stream(file_real_path);
  GE_MAKE_GUARD(file_guard, [&file_stream]() { file_stream.close(); });

  try {
    GE_WARN_ASSERT(file_stream.is_open(), "Failed to open json file[%s], File is already opened",
                   file_real_path.c_str());
    file_stream >> json_obj;
  } catch (const std::exception &e) {
    GELOGW("Failed to read json file[%s], err msg: %s", file_real_path.c_str(), e.what());
    return FAILED;
  }
  // check whether write file error.
  GE_CHK_BOOL_RET_STATUS(file_stream.good(), FAILED, "Failed to read json file[%s], error msg = %s",
                         file_real_path.c_str(), strerror(errno));
  GELOGD("Read json file[%s] success", file_real_path.c_str());
  return SUCCESS;
}

void CollectFusionSwitchMap(const nlohmann::json &fusion_switch_config_json,
                            std::map<std::string, bool> &fusion_switch_map) {
  if (fusion_switch_config_json == nullptr ||
      fusion_switch_config_json.find(kSwitchKey) == fusion_switch_config_json.end() ||
      fusion_switch_config_json[kSwitchKey].find(kGraphFusionKey) == fusion_switch_config_json[kSwitchKey].end()) {
    return;
  }
  if (!fusion_switch_config_json[kSwitchKey][kGraphFusionKey].is_object()) {
    GELOGW("The third level of json file should be object.");
    return;
  }
  for (auto &item_json : fusion_switch_config_json[kSwitchKey][kGraphFusionKey].items()) {
    string key_inner = item_json.key();
    string value_inner = item_json.value();
    if (!key_inner.empty()) {
      fusion_switch_map.emplace(key_inner, value_inner == kSwitchOn);
    }
  }
}

Status ParseFusionSwitchFile(const string &fusion_switch_json_file, std::map<string, bool> &pass_name_2_switch) {
  nlohmann::json fusion_config_json;
  GE_WARN_ASSERT(ReadJsonFile(fusion_switch_json_file, fusion_config_json) == SUCCESS,
                 "Failed to read json from file %s.", fusion_switch_json_file.c_str());

  if (fusion_config_json != nullptr && !fusion_config_json.is_object()) {
    GELOGE(GRAPH_FAILED, "[GraphOpt][Init][CheckCfgFileFormat] Top level of fusion config file should be object.");
    return FAILED;
  }
  // no need check json format, it was check in fe init stage
  CollectFusionSwitchMap(fusion_config_json, pass_name_2_switch);
  return SUCCESS;
}

std::vector<NodePtr> ToNodePtrs(const std::vector<GNode> &nodes) {
  std::vector<NodePtr> node_ptrs;
  for (const auto &node : nodes) {
    node_ptrs.emplace_back(NodeAdapter::GNode2Node(node));
  }
  return node_ptrs;
}

// 公共实现：过滤无效节点后做局部 BFS 判环。
// matched_nodes 需已确保非空且均为有效节点（非 nullptr 且有 owner_graph）。
static bool WillCauseCycleIfFuseImpl(const std::vector<NodePtr> &matched_nodes) {
  // 把 matched_nodes 集合 S 融成一个节点是否会成环，等价于：从 S 的某个"外部后继"出发，
  // 沿有向边（数据+控制后继）正向能否回到 S。能回到即成环。无状态局部 BFS，O(V+E)，
  // 不依赖任何缓存/连通矩阵，多线程各判各的，天然并发安全。
  std::unordered_set<const Node *> fused;
  for (const auto &node : matched_nodes) {
    fused.insert(node.get());
  }
  std::unordered_set<const Node *> visited;
  std::queue<const Node *> to_visit;
  // 起点：S 各节点的外部直接后继（跳过 S 内部边）
  for (const auto &node : matched_nodes) {
    for (const auto *const out_node : node->GetOutNodesPtr()) {
      if ((out_node == nullptr) || (fused.count(out_node) > 0)) {
        continue;
      }
      if (visited.insert(out_node).second) {
        to_visit.push(out_node);
      }
    }
  }
  // 从外部后继正向 BFS；若回到 S 内任一节点，说明存在经过外部节点又绕回 S 的路径，融合后成环
  while (!to_visit.empty()) {
    const auto *const cur = to_visit.front();
    to_visit.pop();
    for (const auto *const out_node : cur->GetOutNodesPtr()) {
      if (out_node == nullptr) {
        continue;
      }
      if (fused.count(out_node) > 0) {
        return true;
      }
      if (visited.insert(out_node).second) {
        to_visit.push(out_node);
      }
    }
  }
  return false;
}
}  // namespace
std::unique_ptr<SubgraphBoundary> FusionUtils::BuildSubgraphBoundaryFromNode(const NodePtr &node) {
  GE_ASSERT_NOTNULL(node);
  std::vector<SubgraphInput> subgraph_inputs;
  std::map<OutDataAnchorPtr, size_t> out_anchor_2_idx;
  for (const auto &in_anchor : node->GetAllInDataAnchors()) {
    GE_ASSERT_NOTNULL(in_anchor);
    const auto peer_out_anchor = in_anchor->GetPeerOutAnchor();
    if (peer_out_anchor == nullptr) {
      continue;
    }
    NodeIo node_input = {NodeAdapter::Node2GNode(node), in_anchor->GetIdx()};
    const auto iter = out_anchor_2_idx.find(peer_out_anchor);
    if (iter == out_anchor_2_idx.cend()) {
      SubgraphInput subgraph_input({node_input});
      subgraph_inputs.emplace_back(subgraph_input);
      out_anchor_2_idx.emplace(peer_out_anchor, subgraph_inputs.size() - 1);
    } else {
      auto idx = iter->second;
      GE_ASSERT_TRUE(idx < subgraph_inputs.size());
      subgraph_inputs[idx].AddInput(node_input);
    }
  }
  std::vector<SubgraphOutput> subgraph_outputs;
  for (const auto &out_anchor : node->GetAllOutDataAnchors()) {
    GE_ASSERT_NOTNULL(out_anchor);
    NodeIo node_output = {NodeAdapter::Node2GNode(node), out_anchor->GetIdx()};
    subgraph_outputs.emplace_back(node_output);
  }
  return ge::MakeUnique<SubgraphBoundary>(subgraph_inputs, subgraph_outputs);
}

Status FusionUtils::MarkPassNameOnReplacementNodes(const std::unique_ptr<Graph> &replacement,
                                                   const std::unique_ptr<SubgraphBoundary> &subgraph,
                                                   const std::string &pass_name) {
  GE_ASSERT_NOTNULL(replacement);
  GE_ASSERT_NOTNULL(subgraph);
  const auto replacement_graph = GraphUtilsEx::GetComputeGraph(*replacement);
  GE_ASSERT_NOTNULL(replacement_graph);
  InnerSubgraphBoundary inner_boundary;
  std::string boundary_invalid_reason;
  GE_ASSERT_SUCCESS(inner_boundary.Init(*subgraph, boundary_invalid_reason), boundary_invalid_reason.c_str());
  for (const auto &node : replacement_graph->GetAllNodes()) {
    GE_ASSERT_NOTNULL(node);
    const auto op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc, "Node[%s][%s] has null op_desc.", node->GetNamePtr(), node->GetTypePtr());
    GE_ASSERT_SUCCESS(
        fe::GraphPassUtil::StoreAndUpdataOriginFusionPassName(op_desc, inner_boundary.GetNodes(), pass_name),
        "Failed to mark pass name[%s] on node[%s][%s].", pass_name.c_str(), node->GetNamePtr(), node->GetTypePtr());
  }
  return SUCCESS;
}

std::string FusionUtils::ToString(const std::unique_ptr<Graph> &graph) {
  std::stringstream ss;
  auto compute_grpah = GraphUtilsEx::GetComputeGraph(*graph);
  ss << "[";
  for (const auto &node : compute_grpah->GetDirectNode()) {
    if (OpTypeUtils::IsDataNode(node->GetType()) || OpTypeUtils::IsGraphOutputNode(node->GetType())) {
      continue;
    }
    ss << "{" << node->GetTypePtr() << "}";
  }
  ss << "]";
  return ss.str();
}

std::string FusionUtils::GetFusionSwitchFileFromOption() {
  std::string fusion_switch_file_path;
  ge::graphStatus status = GetContext().GetOption(FUSION_SWITCH_FILE, fusion_switch_file_path);
  if (status != ge::GRAPH_SUCCESS) {
    GELOGD("Cannot get option value [%s].", FUSION_SWITCH_FILE.c_str());
    return "";
  }
  GELOGD("The [%s] in ge context is %s.", FUSION_SWITCH_FILE.c_str(), fusion_switch_file_path.c_str());
  return fusion_switch_file_path;
}

std::map<string, bool> FusionUtils::ParseFusionSwitch() {
  const auto fusion_switch_file_path = GetFusionSwitchFileFromOption();
  if (fusion_switch_file_path.empty()) {
    return {};
  }
  const auto fusion_switch_real_path = RealPath(fusion_switch_file_path.c_str());
  if (fusion_switch_real_path.empty()) {
    GELOGD("Fusion switch config real path of %s is empty", fusion_switch_file_path.c_str());
    return {};
  }
  GELOGD("Fusion switch config real path is %s", fusion_switch_real_path.c_str());
  std::map<string, bool> pass_name_2_switches;
  // 可能存在一种较老的配置格式导致解析失败，因此处不再考虑兼容老的配置格式，因此返回warning
  GE_WARN_ASSERT(ParseFusionSwitchFile(fusion_switch_real_path, pass_name_2_switches) == SUCCESS);
  GELOGD("[FusionSwitch] is %s", SwitchToString(pass_name_2_switches).c_str());
  return pass_name_2_switches;
}

bool FusionUtils::WillCauseCycleIfFuse(const std::unique_ptr<MatchResult> &match_result) {
  if (match_result == nullptr) {
    return false;
  }
  auto matched_nodes = ToNodePtrs(match_result->GetMatchedNodes());
  // 过滤 ToNodePtrs 可能产生的 null（GNode2Node 转换失败、节点已被孤立等），以及 owner_graph 已被
  // 清空的"半死"节点——上一次 SubgraphRewriter::Replace 的 PruneUnusedNodes 会调用 ClearOwnerGraph(nullptr)
  matched_nodes.erase(std::remove_if(matched_nodes.begin(), matched_nodes.end(),
                                     [](const NodePtr &node) {
                                       return node == nullptr || node->GetOwnerComputeGraph() == nullptr;
                                     }),
                      matched_nodes.end());
  if (matched_nodes.empty()) {
    return false;
  }
  return WillCauseCycleIfFuseImpl(matched_nodes);
}

bool FusionUtils::WillCauseCycleIfFuse(const std::vector<NodePtr> &nodes) {
  // 过滤 nullptr 及 owner_graph 已被清空的"半死"节点
  std::vector<NodePtr> valid_nodes;
  valid_nodes.reserve(nodes.size());
  for (const auto &node : nodes) {
    if (node != nullptr && node->GetOwnerComputeGraph() != nullptr) {
      valid_nodes.push_back(node);
    }
  }
  if (valid_nodes.empty()) {
    return false;
  }
  return WillCauseCycleIfFuseImpl(valid_nodes);
}

void FusionUtils::RecordFusionStatistic(const uint64_t session_id, const std::string graph_id, const std::string pass_name,
  const int match_times, const int effect_times) {
  fe::FusionStatisticRecorder &fusion_statistic_instance = fe::FusionStatisticRecorder::Instance();
  auto fusion_info = fe::FusionInfo(session_id, graph_id, pass_name, match_times, effect_times);
  fusion_statistic_instance.UpdateGraphFusionMatchTimes(fusion_info);
  fusion_statistic_instance.UpdateGraphFusionEffectTimes(fusion_info);
}

} // namespace fusion
}  // namespace ge