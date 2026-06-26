/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_MINIDAG_DAG_GRAPH_H_
#define GE_GRAPH_BUILD_DAG_MINIDAG_DAG_GRAPH_H_

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "graph/build/dag/dag_types.h"
#include "graph/build/dag/dag_node.h"
#include "graph/build/dag/dag_edge.h"

namespace minidag {
class DAGGraph {
 public:
  explicit DAGGraph(const std::string &name);
  std::string GetName() const;
  size_t GetNodeCount() const;
  size_t GetEdgeCount() const;
  std::shared_ptr<DAGNode> AddNode(const std::string &name, const std::string &type);
  std::shared_ptr<DAGNode> FindNode(const std::string &name) const;
  std::vector<std::shared_ptr<DAGNode>> GetAllNodes() const;
  graphStatus AddEdge(std::shared_ptr<DAGNode> src, int32_t src_port, std::shared_ptr<DAGNode> dst, int32_t dst_port);
  std::vector<std::shared_ptr<DAGEdge>> GetAllEdges() const;
  void SetDeviceResource(const DeviceResourceInfo &resource);
  const DeviceResourceInfo &GetDeviceResource() const;

 private:
  std::string name_;
  std::unordered_map<std::string, std::shared_ptr<DAGNode>> nodes_;
  std::vector<std::shared_ptr<DAGEdge>> edges_;
  std::vector<std::string> node_order_;
  DeviceResourceInfo device_resource_;
};

}  // namespace minidag

#endif  // GE_GRAPH_BUILD_DAG_MINIDAG_DAG_GRAPH_H_
