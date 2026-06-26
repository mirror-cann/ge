/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/build/dag/dag_edge.h"
#include "graph/build/dag/dag_node.h"

namespace minidag {

DAGEdge::DAGEdge(std::shared_ptr<DAGNode> src_node, int32_t src_port, std::shared_ptr<DAGNode> dst_node,
                 int32_t dst_port)
    : src_node_(src_node), dst_node_(dst_node), src_port_(src_port), dst_port_(dst_port) {}

std::shared_ptr<DAGNode> DAGEdge::GetSrcNode() const {
  return src_node_.lock();
}

int32_t DAGEdge::GetSrcPort() const {
  return src_port_;
}

std::shared_ptr<DAGNode> DAGEdge::GetDstNode() const {
  return dst_node_.lock();
}

int32_t DAGEdge::GetDstPort() const {
  return dst_port_;
}

}  // namespace minidag
