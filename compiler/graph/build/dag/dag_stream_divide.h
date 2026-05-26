/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_DAG_STREAM_DIVIDE_H_
#define GE_GRAPH_BUILD_DAG_DAG_STREAM_DIVIDE_H_

#include <algorithm>
#include <cstdint>
#include <queue>
#include <vector>

namespace minidag {

constexpr int kInf = 0x3f3f3f3f;
constexpr int64_t kMaxNode = 1000000;
constexpr int kSourceNodeId = 0;
constexpr int kNodeSplitFactor = 2;
constexpr int kSinkOffset = 1;
constexpr int kEdgePairSize = 2;
constexpr int kForwardEdgeOffset = 2;
constexpr int kBackwardEdgeOffset = 1;

struct Edge {
  int from;
  int to;
  int cap;  // capacity: maximum flow allowed through this edge
  int flow; // current flow through this edge
  Edge(int u, int v, int c, int f) : from(u), to(v), cap(c), flow(f) {}
};

class HopcroftKarp {
public:
  explicit HopcroftKarp(int node_count) {
    node_num_ = node_count;
    total_node_count_ = node_num_ * kNodeSplitFactor + kEdgePairSize;
    source_node_ = kSourceNodeId;
    sink_node_ = node_num_ * kNodeSplitFactor + kSinkOffset;
    edges_.clear();
    adjacency_list_.resize(total_node_count_ + 1);
    visited_.resize(total_node_count_ + 1, false);
    distance_.resize(total_node_count_ + 1, 0);
    current_edge_idx_.resize(total_node_count_ + 1, 0);
    for (int i = 1; i <= node_num_; i++) {
      AddEdge(source_node_, i, 1);
      AddEdge(i + node_num_, sink_node_, 1);
    }
  }

  void AddEdge(int from, int to, int cap) {
    edges_.push_back(Edge(from, to, cap, 0));
    edges_.push_back(Edge(to, from, 0, 0));

    edge_num_ = edges_.size();
    adjacency_list_[from].push_back(edge_num_ - kForwardEdgeOffset);
    adjacency_list_[to].push_back(edge_num_ - kBackwardEdgeOffset);
  }

  bool BFS() {
    std::fill(visited_.begin(), visited_.end(), false);
    std::queue<int> Q;
    distance_[source_node_] = 0;
    visited_[source_node_] = true;
    Q.push(source_node_);
    while (!Q.empty()) {
      int x = Q.front(); Q.pop();
      for (int i = 0; i < static_cast<int>(adjacency_list_[x].size()); i++) {
        Edge& e = edges_[adjacency_list_[x][i]];
        if (!visited_[e.to] && e.cap > e.flow) {
          visited_[e.to] = true;
          distance_[e.to] = distance_[x] + 1;
          Q.push(e.to);
        }
      }
    }
    return visited_[sink_node_];
  }

  int DFS(int current_node, int remaining_flow) {
    if (current_node == sink_node_ || remaining_flow == 0) return remaining_flow;
    int flow = 0, f;
    for (int& i = current_edge_idx_[current_node]; i < static_cast<int>(adjacency_list_[current_node].size()); i++) {
      Edge& e = edges_[adjacency_list_[current_node][i]];
      if (distance_[current_node] + 1 == distance_[e.to] &&
          (f = DFS(e.to, std::min(remaining_flow, e.cap - e.flow))) > 0) {
        e.flow += f;
        edges_[adjacency_list_[current_node][i]^1].flow -= f;
        flow += f;
        remaining_flow -= f;
        if (remaining_flow == 0) break;
      }
    }
    return flow;
  }

  int MinStreamCover() {
    int flow = 0;
    while (BFS()) {
      std::fill(current_edge_idx_.begin(), current_edge_idx_.end(), 0);
      flow += DFS(source_node_, kInf);
    }
    std::vector<int> to(total_node_count_ + 1, 0);
    std::vector<int> from(total_node_count_ + 1, 0);
    for (int i = 1; i <= node_num_; i++) {
      for (int j = 0; j < static_cast<int>(adjacency_list_[i].size()); j++) {
        Edge& e = edges_[adjacency_list_[i][j]];
        if (e.flow == 1) {
          int nxt = e.to - node_num_;
          if (nxt >= 1 && nxt <= node_num_) {
            to[i] = nxt;
            from[nxt] = i;
          }
          break;
        }
      }
    }
    routes_.clear();

    std::vector<bool> v(total_node_count_ + 1, false);
    for (int i = 1; i <= node_num_; i++) {
      if (from[i] == 0 && !v[i]) {
        int x = i;
        routes_.push_back({});
        while (x) {
          if (v[x]) break;
          routes_.back().push_back(x);
          v[x] = true;
          x = to[x];
        }
      }
    }
    for (int i = 1; i <= node_num_; i++) {
      if (!v[i]) {
        routes_.push_back({i});
        v[i] = true;
      }
    }
    return node_num_ - flow;
  }

  std::vector<std::vector<int>> GetRoutes() const {
    return routes_;
  }

private:
  int node_num_ = 0;
  int total_node_count_ = 0;
  int edge_num_ = 0;
  int source_node_ = 0;
  int sink_node_ = 0;
  std::vector<Edge> edges_;
  std::vector<std::vector<int>> adjacency_list_;
  std::vector<bool> visited_;
  std::vector<int> distance_;
  std::vector<int> current_edge_idx_;
  std::vector<std::vector<int>> routes_;
};

}  // namespace minidag

#endif  // GE_GRAPH_BUILD_DAG_DAG_STREAM_DIVIDE_H_