/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <memory>
#include "graph/graph.h"
#include "ge_common/ge_common_api_types.h"

#ifndef INC_EXTERNAL_GRAPH_ENGINE_GE_UTILS_H
#define INC_EXTERNAL_GRAPH_ENGINE_GE_UTILS_H

namespace ge {
class GeUtils {
 public:
  /**
   * 给定输入 shape，对传入的 graph 做全图 shape/dtype 推导。
   * 本接口只做推导，不对图做任何其他优化（如常量折叠、死边消除等）。
   * 注意：仅覆盖各输入 tensor 的 shape，dtype/format 沿用图中 Data 节点已有的值。
   *
   * @param graph        待推导的图
   * @param input_shape  各输入 tensor 的 shape，顺序与图中 Data 节点的 ATTR_NAME_INDEX 对应
   * @return SUCCESS 或错误码
   */
  static Status InferShape(const Graph &graph, const std::vector<Shape> &input_shape);

  /**
   * 對传入的node做校验，校验其是否支持在aicore上执行
   * @param node
   * @param unsupported_reason
   */
  static Status CheckNodeSupportOnAicore(const GNode &node, bool &is_supported, AscendString &unsupported_reason);
};
} // namespace ge
#endif  // INC_EXTERNAL_GRAPH_ENGINE_GE_UTILS_H
