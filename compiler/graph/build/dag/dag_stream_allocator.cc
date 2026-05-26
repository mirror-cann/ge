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
#include "graph/build/dag/dag_log.h"
#include "graph/build/dag/dag_stream_divide.h"
#include "graph/build/dag/dag_stream_merger.h"
#include <map>

namespace minidag {
namespace {

constexpr int32_t kDefaultMaxPhysicalStreams = 8;
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
}  // namespace

void DagStreamAllocator::ByPathCover(DAGGraph &graph, StreamAllocConfig &config) {
  MINIDAG_LOG_INFO("ByPathCover start: graph=%s, nodes=%zu, max_stream_id=%ld, base_stream_id=%ld",
                   graph.GetName().c_str(), graph.GetNodeCount(),
                   config.max_stream_id, config.base_stream_id);

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
  if (merger.Merge(graph, index_routes, logical_to_physical) != graphStatus::SUCCESS) {
    MINIDAG_LOG_ERROR("StreamMerger failed, ByPathCover abort");
    return;
  }

  AssignStreamIds(routes, logical_to_physical, id_to_node, config);
  MINIDAG_LOG_INFO("cv logical stream num:%zu, merged physical stream num:%ld",
         routes.size(), config.required_streams);
}
}  // namespace minidag