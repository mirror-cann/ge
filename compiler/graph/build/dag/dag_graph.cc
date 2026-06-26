/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/dag/dag_graph.h"
#include "graph/build/dag/dag_log.h"

namespace minidag {
DAGGraph::DAGGraph(const std::string &name) : name_(name) {}

std::string DAGGraph::GetName() const {
  return name_;
}

size_t DAGGraph::GetNodeCount() const {
  return nodes_.size();
}

size_t DAGGraph::GetEdgeCount() const {
  return edges_.size();
}

void DAGGraph::SetDeviceResource(const DeviceResourceInfo &resource) {
  device_resource_ = resource;
}

const DeviceResourceInfo &DAGGraph::GetDeviceResource() const {
  return device_resource_;
}

std::shared_ptr<DAGNode> DAGGraph::AddNode(const std::string &name, const std::string &type) {
  if (nodes_.find(name) != nodes_.end()) {
    MINIDAG_LOG_WARN("Node already exists: %s, type: %s", name.c_str(), type.c_str());
    return nullptr;  // 节点已存在，返回nullptr
  }

  auto node = std::make_shared<DAGNode>(name, type);
  nodes_[name] = node;
  node_order_.push_back(name);
  MINIDAG_LOG_DEBUG("Add node success: %s, type: %s, total nodes: %zu", name.c_str(), type.c_str(), nodes_.size());
  return node;
}

std::shared_ptr<DAGNode> DAGGraph::FindNode(const std::string &name) const {
  auto it = nodes_.find(name);
  if (it != nodes_.end()) {
    return it->second;
  }
  return nullptr;
}

std::vector<std::shared_ptr<DAGNode>> DAGGraph::GetAllNodes() const {
  std::vector<std::shared_ptr<DAGNode>> result;
  for (const auto &name : node_order_) {
    auto it = nodes_.find(name);
    if (it != nodes_.end()) {
      result.push_back(it->second);
    }
  }
  return result;
}

graphStatus DAGGraph::AddEdge(std::shared_ptr<DAGNode> src, int32_t src_port, std::shared_ptr<DAGNode> dst,
                              int32_t dst_port) {
  if (src == nullptr || dst == nullptr) {
    MINIDAG_LOG_WARN("Add edge failed: src or dst is null");
    return graphStatus::INVALID_NODE;
  }

  auto edge = std::make_shared<DAGEdge>(src, src_port, dst, dst_port);
  edges_.push_back(edge);

  src->AddOutputEdge(edge);
  dst->AddInputEdge(edge);

  MINIDAG_LOG_DEBUG("Add edge: %s[port %d] -> %s[port %d], total edges: %zu", src->GetName().c_str(), src_port,
                    dst->GetName().c_str(), dst_port, edges_.size());
  return graphStatus::SUCCESS;
}

std::vector<std::shared_ptr<DAGEdge>> DAGGraph::GetAllEdges() const {
  return edges_;
}

}  // namespace minidag
