/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/dag/dag_stream_allocator.h"
#include "graph/build/dag/dag_checker.h"
#include "graph/build/dag/dag_log.h"
#include "graph/build/dag/dag_stream_divide.h"
#include "graph/build/dag/dag_stream_merger.h"
#include <algorithm>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace minidag {
namespace {

constexpr int32_t kDefaultMaxPhysicalStreams = 8;

using DAGNodePtr = std::shared_ptr<DAGNode>;

struct SerialGraphSplit {
  std::shared_ptr<DAGGraph> residual_graph;
  std::map<std::string, std::vector<DAGNodePtr>> serial_groups;
  std::vector<std::string> serial_label_order;
};

bool IsSerialNode(const DAGNodePtr &node) {
  return (node != nullptr) && (!node->GetSerialFlag().empty());
}

void SortByTopoId(std::vector<DAGNodePtr> &nodes) {
  std::stable_sort(nodes.begin(), nodes.end(), [](const DAGNodePtr &lhs, const DAGNodePtr &rhs) {
    if (lhs == nullptr) {
      return rhs != nullptr;
    }
    if (rhs == nullptr) {
      return false;
    }
    return lhs->GetTopoId() < rhs->GetTopoId();
  });
}

void BuildNodeIdMaps(DAGGraph &graph,
                      std::map<int32_t, std::shared_ptr<DAGNode>> &id_to_node,
                      std::map<std::shared_ptr<DAGNode>, int32_t> &node_to_id) {
  auto all_nodes = graph.GetAllNodes();
  int32_t id = 1;
  for (auto &node : all_nodes) {
    id_to_node[id] = node;
    node_to_id[node] = id++;
  }
}

void BuildHopcroftKarpGraph(const std::map<std::shared_ptr<DAGNode>, int32_t> &node_to_id,
                             HopcroftKarp &hk, int32_t node_num) {
  for (const auto &entry : node_to_id) {
    const auto &src = entry.first;
    int32_t u = entry.second;
    for (const auto &dst : src->GetOutputNodes()) {
      auto it = node_to_id.find(dst);
      if (it != node_to_id.end()) {
        int32_t v = it->second;
        hk.AddEdge(u, v + node_num, 1);
      }
    }
  }
}

std::vector<std::vector<int32_t>> ConvertRoutesToIndexRoutes(
    const std::vector<std::vector<int32_t>> &routes) {
  std::vector<std::vector<int32_t>> index_routes;
  for (auto &route : routes) {
    std::vector<int32_t> converted_route;
    for (auto &node_id : route) {
      converted_route.push_back(node_id - 1);
    }
    index_routes.push_back(converted_route);
  }
  return index_routes;
}

void AssignStreamIds(const std::vector<std::vector<int32_t>> &routes,
                     const std::vector<int32_t> &logical_to_physical,
                     const std::map<int32_t, std::shared_ptr<DAGNode>> &id_to_node,
                     StreamAllocConfig &config) {
  for (size_t logical_stream_idx = 0; logical_stream_idx < routes.size(); ++logical_stream_idx) {
    int32_t physical_stream_idx = logical_to_physical[logical_stream_idx];
    int64_t new_stream_id = config.base_stream_id + physical_stream_idx;

    std::string node_ids_str;
    for (const auto node_id : routes[logical_stream_idx]) {
      auto it = id_to_node.find(node_id);
      if (it != id_to_node.end()) {
        const auto &node = it->second;
        node_ids_str += (std::to_string(node->GetTopoId()) + " ");
        node->SetStreamId(new_stream_id);
      }
    }
    MINIDAG_LOG_DEBUG("logical stream %zu merged to physical stream %d, set as %ld, nodes: %s",
           logical_stream_idx, physical_stream_idx, new_stream_id, node_ids_str.c_str());
  }

  int32_t max_stream = 0;
  for (const auto stream : logical_to_physical) {
    max_stream = std::max(max_stream, stream + 1);
  }
  config.required_streams = max_stream;
}

graphStatus AddResidualEdge(DAGGraph &residual_graph, const DAGNodePtr &src_node,
                            const DAGNodePtr &dst_node,
                            std::set<std::pair<std::string, std::string>> &added_edges) {
  MINIDAG_ASSERT_NOTNULL(src_node, "Add residual edge failed: src node is null.");
  MINIDAG_ASSERT_NOTNULL(dst_node, "Add residual edge failed: dst node is null.");
  if (src_node->GetName() == dst_node->GetName()) {
    return graphStatus::SUCCESS;
  }

  const auto edge_key = std::make_pair(src_node->GetName(), dst_node->GetName());
  if (!added_edges.insert(edge_key).second) {
    return graphStatus::SUCCESS;
  }

  auto residual_src = residual_graph.FindNode(src_node->GetName());
  auto residual_dst = residual_graph.FindNode(dst_node->GetName());
  MINIDAG_ASSERT_NOTNULL(residual_src, "Add residual edge failed: %s -> %s src not found in residual graph.",
                         src_node->GetName().c_str(), dst_node->GetName().c_str());
  MINIDAG_ASSERT_NOTNULL(residual_dst, "Add residual edge failed: %s -> %s dst not found in residual graph.",
                         src_node->GetName().c_str(), dst_node->GetName().c_str());
  MINIDAG_ASSERT_SUCCESS(residual_graph.AddEdge(residual_src, -1, residual_dst, -1),
                         "Add residual edge failed: %s -> %s.", src_node->GetName().c_str(),
                         dst_node->GetName().c_str());
  return graphStatus::SUCCESS;
}

graphStatus AddResidualEdgesFromNode(DAGGraph &residual_graph, const DAGNodePtr &src_node,
                                     std::set<std::pair<std::string, std::string>> &added_edges) {
  MINIDAG_ASSERT_NOTNULL(src_node, "Add residual edges failed: src node is null.");

  std::queue<DAGNodePtr> pending_serial_nodes;
  std::set<std::string> visited_serial_nodes;
  auto handle_dst_node = [&residual_graph, &src_node, &added_edges, &pending_serial_nodes, &visited_serial_nodes](
                             const DAGNodePtr &dst_node) {
    if (dst_node == nullptr) {
      return graphStatus::SUCCESS;
    }
    if (IsSerialNode(dst_node)) {
      if (visited_serial_nodes.insert(dst_node->GetName()).second) {
        pending_serial_nodes.push(dst_node);
      }
      return graphStatus::SUCCESS;
    }
    return AddResidualEdge(residual_graph, src_node, dst_node, added_edges);
  };

  for (const auto &edge : src_node->GetOutputEdges()) {
    auto ret = handle_dst_node(edge->GetDstNode());
    MINIDAG_ASSERT_SUCCESS(ret, "Add residual edge from %s failed.", src_node->GetName().c_str());
  }

  while (!pending_serial_nodes.empty()) {
    auto serial_node = pending_serial_nodes.front();
    pending_serial_nodes.pop();
    MINIDAG_ASSERT_NOTNULL(serial_node, "Pending serial node is null.");
    for (const auto &edge : serial_node->GetOutputEdges()) {
      auto ret = handle_dst_node(edge->GetDstNode());
      MINIDAG_ASSERT_SUCCESS(ret, "Add residual edge through serial node %s failed.",
                             serial_node->GetName().c_str());
    }
  }
  return graphStatus::SUCCESS;
}

graphStatus BuildResidualEdges(const DAGGraph &graph, DAGGraph &residual_graph) {
  std::set<std::pair<std::string, std::string>> added_edges;
  for (const auto &node : graph.GetAllNodes()) {
    MINIDAG_ASSERT_NOTNULL(node, "Build residual edges failed: graph contains null node.");
    if (IsSerialNode(node)) {
      continue;
    }
    auto ret = AddResidualEdgesFromNode(residual_graph, node, added_edges);
    MINIDAG_ASSERT_SUCCESS(ret, "Build residual edges failed from node %s.", node->GetName().c_str());
  }
  return graphStatus::SUCCESS;
}

graphStatus SplitSerialGraph(DAGGraph &graph, SerialGraphSplit &split) {
  split.residual_graph = std::make_shared<DAGGraph>(graph.GetName() + "_residual");
  MINIDAG_ASSERT_NOTNULL(split.residual_graph, "Create residual graph failed.");
  split.serial_groups.clear();
  split.serial_label_order.clear();

  auto all_nodes = graph.GetAllNodes();
  SortByTopoId(all_nodes);
  std::set<std::string> seen_labels;
  for (const auto &node : all_nodes) {
    MINIDAG_ASSERT_NOTNULL(node, "Split serial graph failed: graph contains null node.");
    if (IsSerialNode(node)) {
      const auto &serial_label = node->GetSerialFlag();
      split.serial_groups[serial_label].push_back(node);
      if (seen_labels.insert(serial_label).second) {
        split.serial_label_order.push_back(serial_label);
      }
      continue;
    }

    auto residual_node = split.residual_graph->AddNode(node->GetName(), node->GetType());
    MINIDAG_ASSERT_NOTNULL(residual_node, "Add residual node failed: %s, type: %s.",
                           node->GetName().c_str(), node->GetType().c_str());
    residual_node->SetTopoId(node->GetTopoId());
    residual_node->SetCost(node->GetCost());
  }

  for (auto &serial_group : split.serial_groups) {
    SortByTopoId(serial_group.second);
  }
  MINIDAG_ASSERT_SUCCESS(BuildResidualEdges(graph, *split.residual_graph), "Build residual edges failed.");
  return graphStatus::SUCCESS;
}

void CopyResidualStreamIdsToOriginal(const DAGGraph &residual_graph, DAGGraph &origin_graph) {
  for (const auto &residual_node : residual_graph.GetAllNodes()) {
    if (residual_node == nullptr) {
      continue;
    }
    auto origin_node = origin_graph.FindNode(residual_node->GetName());
    if (origin_node == nullptr) {
      MINIDAG_LOG_WARN("Residual node %s not found in origin graph.", residual_node->GetName().c_str());
      continue;
    }
    origin_node->SetStreamId(residual_node->GetStreamId());
  }
}

void AssignSerialStreamIds(const SerialGraphSplit &split, const int64_t first_serial_stream_id) {
  for (size_t label_idx = 0UL; label_idx < split.serial_label_order.size(); ++label_idx) {
    const auto &label = split.serial_label_order[label_idx];
    const auto group_iter = split.serial_groups.find(label);
    if (group_iter == split.serial_groups.end()) {
      continue;
    }

    const int64_t serial_stream_id = first_serial_stream_id + static_cast<int64_t>(label_idx);
    std::string node_ids_str;
    for (const auto &node : group_iter->second) {
      if (node == nullptr) {
        continue;
      }
      node->SetStreamId(serial_stream_id);
      node_ids_str += (std::to_string(node->GetTopoId()) + " ");
    }
    MINIDAG_LOG_DEBUG("serial label %s set as stream %ld, nodes: %s",
                      label.c_str(), serial_stream_id, node_ids_str.c_str());
  }
}

graphStatus ByPathCoverCore(DAGGraph &graph, StreamAllocConfig &config) {
  auto all_nodes = graph.GetAllNodes();
  int32_t node_num = static_cast<int32_t>(all_nodes.size());

  std::map<int32_t, std::shared_ptr<DAGNode>> id_to_node;
  std::map<std::shared_ptr<DAGNode>, int32_t> node_to_id;
  BuildNodeIdMaps(graph, id_to_node, node_to_id);

  HopcroftKarp hk(node_num);
  BuildHopcroftKarpGraph(node_to_id, hk, node_num);

  hk.MinStreamCover();
  auto routes = hk.GetRoutes();

  auto index_routes = ConvertRoutesToIndexRoutes(routes);

  StreamMergeOptions options;
  options.physical_stream_limit = (config.max_stream_id >= 0)
                                   ? static_cast<int32_t>(config.max_stream_id + 1) : kDefaultMaxPhysicalStreams;
  options.strategy = config.merge_strategy;
  StreamMerger merger(options);
  std::vector<int32_t> logical_to_physical;
  MINIDAG_ASSERT_SUCCESS(merger.Merge(graph, index_routes, logical_to_physical),
                         "StreamMerger failed, ByPathCover abort.");

  AssignStreamIds(routes, logical_to_physical, id_to_node, config);
  MINIDAG_LOG_INFO("Logical stream num:%zu, merged physical stream num:%ld",
         routes.size(), config.required_streams);
  return graphStatus::SUCCESS;
}

graphStatus ByPathCoverWithStatus(DAGGraph &graph, StreamAllocConfig &config) {
  SerialGraphSplit split;
  MINIDAG_ASSERT_SUCCESS(SplitSerialGraph(graph, split), "Split serial graph failed.");
  MINIDAG_ASSERT_NOTNULL(split.residual_graph, "Split serial graph generated null residual graph.");

  StreamAllocConfig residual_config = config;
  residual_config.required_streams = 0;
  MINIDAG_ASSERT_SUCCESS(ByPathCoverCore(*split.residual_graph, residual_config), "ByPathCover core failed.");
  CopyResidualStreamIdsToOriginal(*split.residual_graph, graph);

  AssignSerialStreamIds(split, config.base_stream_id + residual_config.required_streams);
  config.required_streams = residual_config.required_streams +
                            static_cast<int64_t>(split.serial_label_order.size());
  MINIDAG_LOG_INFO("ByPathCover with residual graph done: residual_nodes=%zu, serial_labels=%zu, required_streams=%ld",
                   split.residual_graph->GetNodeCount(), split.serial_label_order.size(), config.required_streams);
  return graphStatus::SUCCESS;
}
}  // namespace

void DagStreamAllocator::ByPathCover(DAGGraph &graph, StreamAllocConfig &config) {
  MINIDAG_LOG_INFO("ByPathCover start: graph=%s, nodes=%zu, max_stream_id=%ld, base_stream_id=%ld",
                   graph.GetName().c_str(), graph.GetNodeCount(),
                   config.max_stream_id, config.base_stream_id);

  const auto ret = ByPathCoverWithStatus(graph, config);
  if (ret != graphStatus::SUCCESS) {
    MINIDAG_LOG_ERROR("ByPathCover failed, ret=%d.", static_cast<int32_t>(ret));
    return;
  }
}
}  // namespace minidag
