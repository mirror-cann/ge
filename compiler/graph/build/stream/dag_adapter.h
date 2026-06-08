/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_STREAM_DAG_ADAPTER_H_
#define GE_GRAPH_BUILD_STREAM_DAG_ADAPTER_H_

#include <memory>

#include "graph/build/dag/dag_graph.h"
#include "external/register/register_custom_pass.h"

namespace ge {
class DAGAdapter {
 public:
  static graphStatus ToGEStatus(minidag::graphStatus status);
  static graphStatus FromGEGraph(const ConstGraphPtr &ge_graph,
                                      std::shared_ptr<minidag::DAGGraph> &dag);
  static graphStatus RefreshStreamIdsToGE(
      const minidag::DAGGraph &dag,
      const ConstGraphPtr &ge_graph,
      StreamPassContext &context);
  DAGAdapter() = delete;

 private:
  static graphStatus ConvertNodes(const ConstGraphPtr &ge_graph, minidag::DAGGraph &dag);
  static graphStatus ConvertEdges(const ConstGraphPtr &ge_graph, minidag::DAGGraph &dag);
  static graphStatus ConvertDataEdgesForNode(
      const GNode &gnode,
      const std::shared_ptr<minidag::DAGNode> &src_node,
      minidag::DAGGraph &dag,
      int64_t &edge_count);
  static graphStatus ConvertControlEdgesForNode(
      const GNode &gnode,
      const std::shared_ptr<minidag::DAGNode> &src_node,
      minidag::DAGGraph &dag,
      int64_t &edge_count);
};
}  // namespace ge
#endif  // GE_GRAPH_BUILD_STREAM_DAG_ADAPTER_H_
