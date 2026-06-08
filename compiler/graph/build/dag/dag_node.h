/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_MINIDAG_DAG_NODE_H_
#define GE_GRAPH_BUILD_DAG_MINIDAG_DAG_NODE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "graph/build/dag/dag_types.h"
#include "graph/build/dag/dag_edge.h"

namespace minidag {
struct NodeCost {
  size_t compute_cycles = 0;
  size_t memory_usage = 0;
  size_t bandwidth_usage = 0;
  float execution_time = 0.0f;
  size_t cube_block_num = 0;
  size_t vec_block_num = 0;
};

class DAGNode : public std::enable_shared_from_this<DAGNode> {
 public:
  explicit DAGNode(const std::string &name, const std::string &type);
  std::string GetName() const;
  std::string GetType() const;
  int64_t GetStreamId() const;
  void SetStreamId(int64_t stream_id);
  int64_t GetTopoId() const;
  void SetTopoId(int64_t topo_id);
  size_t GetInputCount() const;
  size_t GetOutputCount() const;
  std::vector<std::shared_ptr<DAGEdge>> GetInputEdges() const;
  std::vector<std::shared_ptr<DAGEdge>> GetOutputEdges() const;
  std::vector<std::shared_ptr<DAGNode>> GetInputNodes() const;
  std::vector<std::shared_ptr<DAGNode>> GetOutputNodes() const;
  const NodeCost& GetCost() const;
  void SetCost(const NodeCost &cost);
  void SetSerialFlag(const std::string &flag);
  const std::string& GetSerialFlag() const;

 private:
  std::string name_;
  std::string type_;
  int64_t stream_id_ = INVALID_STREAM_ID;
  int64_t topo_id_ = INVALID_TOPO_ID;
  NodeCost cost_;
  std::string serial_flag_;
  std::vector<std::weak_ptr<DAGEdge>> in_edges_;
  std::vector<std::weak_ptr<DAGEdge>> out_edges_;
  friend class DAGGraph;
  void AddInputEdge(const std::shared_ptr<DAGEdge> &edge);
  void AddOutputEdge(const std::shared_ptr<DAGEdge> &edge);
};
}  // namespace minidag
#endif  // GE_GRAPH_BUILD_DAG_MINIDAG_DAG_NODE_H_