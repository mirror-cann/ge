/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_MINIDAG_DAG_EDGE_H_
#define GE_GRAPH_BUILD_DAG_MINIDAG_DAG_EDGE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "graph/build/dag/dag_types.h"

namespace minidag {

class DAGNode;

class DAGEdge {
 public:
  DAGEdge(std::shared_ptr<DAGNode> src_node, int32_t src_port, std::shared_ptr<DAGNode> dst_node, int32_t dst_port);
  std::shared_ptr<DAGNode> GetSrcNode() const;
  int32_t GetSrcPort() const;
  std::shared_ptr<DAGNode> GetDstNode() const;
  int32_t GetDstPort() const;

 private:
  std::weak_ptr<DAGNode> src_node_;
  std::weak_ptr<DAGNode> dst_node_;
  int32_t src_port_;
  int32_t dst_port_;
};

}  // namespace minidag

#endif  // GE_GRAPH_BUILD_DAG_MINIDAG_DAG_EDGE_H_
