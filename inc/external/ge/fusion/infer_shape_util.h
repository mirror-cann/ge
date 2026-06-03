/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_EXTERNAL_GE_FUSION_INFER_SHAPE_UTIL_H_
#define INC_EXTERNAL_GE_FUSION_INFER_SHAPE_UTIL_H_
#include "graph/graph.h"
#include "ge_common/ge_api_types.h"
#include "ge/fusion/subgraph_boundary.h"
#include "ge/fusion/match_result.h"

namespace ge {
namespace fusion {
/**
 * 用于 fusion pass Replacement 阶段对 replacement graph 做 shape/dtype 推导的工具集合。
 */
class InferShapeUtil {
 public:
  /**
   * 对 replacement graph 执行 shape/dtype 推导。
   *
   * 适用于注册在 CustomPassStage::kAfterInferShape 及之后阶段的融合 pass。
   * PatternFusionPass/DecomposePass 等 pass 可在 Replacement() 中构建完
   * replacement graph 后调用本接口，补齐其中算子的 output desc，使后续
   * CheckSupport 等基于推导结果的判断可正常执行。
   *
   * 本接口会根据 subgraph_boundary 将原图边界输入的 shape/dtype/format
   * 同步到 replacement graph 对应的 Data 节点，并执行全图推导。
   *
   * 若边界输入来自原图 Const，本接口会提取常量值并写入 replacement graph 中
   * 下游消费者的 input desc（ATTR_NAME_VALUE 属性），使 Reshape 等值依赖
   * 算子可在 replacement graph 内完成静态 shape 推导。
   *
   * 调用方需保证 replacement_graph 是合法替换图：
   * - subgraph_boundary 输入按 index 0..N-1 连续编号；
   * - replacement_graph 中 Data 节点数量与 boundary 输入数量一致；
   * - 每个 Data 节点携带 ATTR_NAME_INDEX，并与 boundary input index 对应。
   *
   * @param replacement_graph 待推导的 replacement graph。
   * @param subgraph_boundary 原图子图边界，用于提供边界输入的 desc 及常量值。
   * @return SUCCESS 表示推导成功，否则返回错误码。
   */
  static Status InferShape(const Graph &replacement_graph, const SubgraphBoundary &subgraph_boundary);

  /**
   * PatternFusionPass 场景的便利重载。
   * 内部对 match_result 调用 ToSubgraphBoundary() 后委托给本类
   * InferShape(Graph, SubgraphBoundary)，行为与前提条件见该重载。
   *
   * @param replacement_graph 待推导的 replacement graph。
   * @param match_result      匹配结果，用于构造子图边界。
   * @return SUCCESS 表示推导成功，否则返回错误码。
   */
  static Status InferShape(const Graph &replacement_graph, const MatchResult &match_result);

  /**
   * DecomposePass 场景的便利重载。
   * 以 matched_node 的入边/出边构造单节点 SubgraphBoundary 后委托给本类
   * InferShape(Graph, SubgraphBoundary)，行为与前提条件见该重载。
   *
   * @param replacement_graph 待推导的 replacement graph。
   * @param matched_node      原图中匹配到的节点，提供边界输入 desc。
   * @return SUCCESS 表示推导成功，否则返回错误码。
   */
  static Status InferShape(const Graph &replacement_graph, const GNode &matched_node);
};
}  // namespace fusion
}  // namespace ge
#endif  // INC_EXTERNAL_GE_FUSION_INFER_SHAPE_UTIL_H_
