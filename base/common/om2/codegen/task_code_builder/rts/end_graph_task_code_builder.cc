/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "end_graph_task_code_builder.h"
#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {
Status EndGraphTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  return SUCCESS;
}

Status EndGraphTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  auto model_handle = ast_.Var("aclmdlRI", "model_handle_");
  auto stream_list = ast_.Var("std::vector<aclrtStream>", "stream_list_");
  items.push_back(ast_.Comment("EndGraph"));
  items.push_back(ChkStatus(ast_.Call("EndGraphTaskDistribute", {model_handle, stream_list[header_.stream_id]})));
  return SUCCESS;
}

Status EndGraphTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto mdl = ast_.Var("aclmdlRI", "mdl");
  auto stream = ast_.Var("aclrtStream", "stream");
  items.push_back(ast_.DefineFunction("EndGraphTaskDistribute", {mdl, stream}, "aclError", {
      ChkStatus(AclmdlRIEndTask(mdl, stream)),
      ast_.Return("ACL_SUCCESS"),
  }));
  return SUCCESS;
}
REGISTER_TASK_CODE_BUILDER(MODEL_TASK_END_GRAPH, EndGraphTaskCodeBuilder);
}  // namespace ge
