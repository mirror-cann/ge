/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/passes/variable_optimize/ref_identity_delete_op_pass.h"

namespace ge {
// RefIdentity 由 VariablePrepareOpPass 插入，仅作为 VariableRef 的挂载桥梁。
// 其输出对端包括 VariableRef 以及 RefOp（精度模式下中间可能被 FE 插入 Cast/TransData）。
// IsolateNode 会将 RefIdentity 的输入(Variable)直连到所有输出对端，保留原有数据流和控制边，
// 因此无需校验下游是否为 RefOp，直接旁路并删除即可。
Status RefIdentityDeleteOpPass::Run(ComputeGraphPtr graph) {
  GE_CHECK_NOTNULL(graph);
  for (auto &node : graph->GetDirectNode()) {
    if (node->GetType() != REFIDENTITY) {
      continue;
    }
    CHECK_FALSE_EXEC(
        DealNoOutputRef(node, graph) == SUCCESS, REPORT_INNER_ERR_MSG("E19999", "Deal RefIdentity node:%s(%s) failed",
                                                                      node->GetName().c_str(), node->GetType().c_str());
        GELOGE(FAILED, "[Deal][RefIdentity] node:%s(%s) failed", node->GetName().c_str(), node->GetType().c_str());
        return FAILED);
  }
  return SUCCESS;
}

Status RefIdentityDeleteOpPass::DealNoOutputRef(const NodePtr &ref_identity, const ComputeGraphPtr &graph) const {
  // remove ref identity
  if (GraphUtils::IsolateNode(ref_identity, {0}) != GRAPH_SUCCESS) {
    REPORT_INNER_ERR_MSG("E19999", "Isolate op:%s(%s) failed",
                      ref_identity->GetName().c_str(), ref_identity->GetType().c_str());
    GELOGE(INTERNAL_ERROR, "[Isolate][Node] %s, type:%s failed", ref_identity->GetName().c_str(),
           ref_identity->GetType().c_str());
    return FAILED;
  }
  if (GraphUtils::RemoveNodeWithoutRelink(graph, ref_identity) != GRAPH_SUCCESS) {
    REPORT_INNER_ERR_MSG("E19999", "Remove node:%s(%s) without relink in graph:%s failed",
                      ref_identity->GetName().c_str(), ref_identity->GetType().c_str(), graph->GetName().c_str());
    GELOGE(INTERNAL_ERROR, "[Remove][Node] %s, type:%s without relink in graph:%s failed",
           ref_identity->GetName().c_str(), ref_identity->GetType().c_str(), graph->GetName().c_str());
    return FAILED;
  }
  GELOGI("Successfully removed node[%s] from graph[%s]", ref_identity->GetName().c_str(), graph->GetName().c_str());

  return SUCCESS;
}

REG_PASS_OPTION("RefIdentityDeleteOpPass").LEVELS(OoLevel::kO0);
}  // namespace ge
