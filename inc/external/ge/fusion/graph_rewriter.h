/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_EXTERNAL_GE_GRAPH_FUSION_GRAPH_REWRITER_H
#define INC_EXTERNAL_GE_GRAPH_FUSION_GRAPH_REWRITER_H

#include "graph/graph.h"
#include "subgraph_boundary.h"
#include "ge_common/ge_common_api_types.h"
#include "register/register_custom_pass.h"

namespace ge {
namespace fusion {
/**
 * @since 8.5.0(2025-12)
 */
class SubgraphRewriter {
 public:
  /**
   * 给定SubgraphBoundary，将边界内算子替换为replacement
   * @param subgraph
   * @param replacement
   * @return
   * @since 8.5.0(2025-12)
   */
  static Status Replace(const SubgraphBoundary &subgraph, const Graph &replacement);

  /**
   * 给定SubgraphBoundary，将边界内算子替换为replacement
   * @param subgraph
   * @param replacement
   * @return
   * @since 8.5.0(2025-12)
   */
  static Status Replace(const SubgraphBoundary &subgraph, Graph &&replacement);

  /**
   * 给定SubgraphBoundary，将边界内算子替换为replacement。
   * 在替换过程中会自动执行可融合检查与融合结果上报。
   * @param subgraph
   * @param replacement
   * @param ctx 用于获取pass name并执行融合上报
   * @return
   * @since 9.1.0(2026-06)
   */
  static Status Replace(const SubgraphBoundary &subgraph, Graph &&replacement, CustomPassContext &ctx);

  /**
   * 给定SubgraphBoundary，将边界内算子替换为replacement。
   * 在替换过程中会自动执行可融合检查与融合结果上报。
   * @param subgraph
   * @param replacement
   * @param ctx 用于获取pass name并执行融合上报
   * @return
   * @since 9.1.0(2026-06)
   */
  static Status Replace(const SubgraphBoundary &subgraph, const Graph &replacement, CustomPassContext &ctx);
};
}  // namespace fusion
}  // namespace ge

#endif  // INC_EXTERNAL_GE_GRAPH_FUSION_PATTERN_MATCHER_H
