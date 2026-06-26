/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/dag/dag_node.h"
#include "graph/build/dag/dag_edge.h"

namespace minidag {
DAGNode::DAGNode(const std::string &name, const std::string &type) : name_(name), type_(type) {}

std::string DAGNode::GetName() const {
  return name_;
}

std::string DAGNode::GetType() const {
  return type_;
}

int64_t DAGNode::GetStreamId() const {
  return stream_id_;
}

void DAGNode::SetStreamId(int64_t stream_id) {
  stream_id_ = stream_id;
}

int64_t DAGNode::GetTopoId() const {
  return topo_id_;
}

void DAGNode::SetTopoId(int64_t topo_id) {
  topo_id_ = topo_id;
}

size_t DAGNode::GetInputCount() const {
  return in_edges_.size();
}

size_t DAGNode::GetOutputCount() const {
  return out_edges_.size();
}

std::vector<std::shared_ptr<DAGEdge>> DAGNode::GetInputEdges() const {
  std::vector<std::shared_ptr<DAGEdge>> edges;
  for (const auto &weak_edge : in_edges_) {
    auto edge = weak_edge.lock();
    if (edge != nullptr) {
      edges.push_back(edge);
    }
  }
  return edges;
}

std::vector<std::shared_ptr<DAGEdge>> DAGNode::GetOutputEdges() const {
  std::vector<std::shared_ptr<DAGEdge>> edges;
  for (const auto &weak_edge : out_edges_) {
    auto edge = weak_edge.lock();
    if (edge != nullptr) {
      edges.push_back(edge);
    }
  }
  return edges;
}

std::vector<std::shared_ptr<DAGNode>> DAGNode::GetInputNodes() const {
  std::vector<std::shared_ptr<DAGNode>> nodes;
  for (const auto &edge : GetInputEdges()) {
    auto node = edge->GetSrcNode();
    if (node != nullptr) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

std::vector<std::shared_ptr<DAGNode>> DAGNode::GetOutputNodes() const {
  std::vector<std::shared_ptr<DAGNode>> nodes;
  for (const auto &edge : GetOutputEdges()) {
    auto node = edge->GetDstNode();
    if (node != nullptr) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

const NodeCost &DAGNode::GetCost() const {
  return cost_;
}

void DAGNode::SetCost(const NodeCost &cost) {
  cost_ = cost;
}

void DAGNode::AddInputEdge(const std::shared_ptr<DAGEdge> &edge) {
  in_edges_.push_back(edge);
}

void DAGNode::AddOutputEdge(const std::shared_ptr<DAGEdge> &edge) {
  out_edges_.push_back(edge);
}

void DAGNode::SetSerialFlag(const std::string &flag) {
  serial_flag_ = flag;
}

const std::string &DAGNode::GetSerialFlag() const {
  return serial_flag_;
}
}  // namespace minidag
