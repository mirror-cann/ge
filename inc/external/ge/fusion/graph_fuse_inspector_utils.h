/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_EXTERNAL_GE_GRAPH_FUSION_FUSION_PASS_UTILS_H
#define INC_EXTERNAL_GE_GRAPH_FUSION_FUSION_PASS_UTILS_H

#include <vector>
#include "graph/graph.h"
#include "ge_common/ge_common_api_types.h"
#include "register/register_custom_pass.h"

namespace ge {
namespace fusion {
class GraphFuseInspectorUtils {
 public:
  /**
   * 判断待融合节点集合是否可融合
   * 判断条件：
   * 1.节点上的stream lable表达图上的分流策略，当传入节点stream label不一致时，
   *   无法确定用户意图，即无法确定新节点该继承谁的标签，因此传入节点列表的stream label
   *   不一致时，判断为无法融合。
   * 2.如果传入节点列表融合成单个节点出现环，判断为无法融合，
   *   注意传入节点替换为多个节点等其他场景不在此处的成环检测范围内。
   *
   * @param nodes_before_fuse 融合前节点列表（列表内所有节点需连通）
   * @param failed_reason 不支持融合的原因
   * @return true: 可融合; false: 不可融合
   * @since 9.1.0(2026-06)
   */
  static bool CanFuse(const std::vector<GNode> &nodes_before_fuse, AscendString &failed_reason);

  /**
   * 上报融合结果
   * 在完成对图的修改后，需要上报融合结果以完成图连接矩阵的更新、维测信息记录等操作。
   * 约束：该接口必须在改图后且释放删除节点前调用。
   *       nodes_after_fuse 为空表示删除但不新增节点的场景。
   * 行为：
   * 1.新节点的opdesc记录pass name。
   * 2.更新在CanFuse中用于检测成环的连接矩阵。
   * 3.记录匹配次数与生效次数，对应信息落盘至fusion_result.json。
   *
   * @param nodes_before_fuse 融合前节点列表（列表内所有节点需连通）
   * @param nodes_after_fuse 融合后新节点列表（列表内所有节点需连通）
   * @param ctx pass上下文，使用ctx.GetPassName()记录pass name
   * @return SUCCESS: 上报成功; FAILED: 上报失败
   * @since 9.1.0(2026-06)
   */
  static Status ReportFuse(const std::vector<GNode> &nodes_before_fuse,
                           const std::vector<GNode> &nodes_after_fuse,
                           CustomPassContext &ctx);
};
}  // namespace fusion
}  // namespace ge

#endif  // INC_EXTERNAL_GE_GRAPH_FUSION_FUSION_PASS_UTILS_H
