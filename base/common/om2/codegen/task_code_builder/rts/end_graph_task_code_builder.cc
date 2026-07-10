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
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"
#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {
std::string EndGraphTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status EndGraphTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  return SUCCESS;
}

Status EndGraphTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  body.push_back(ChkStatus(
      AclmdlRIEndTask(ctx.Attr("model_handle"),
                      ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("end_graph").Attr("stream_id")])));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

Status EndGraphTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  fields.push_back({"dispatch_info", ast_.DesignatedInit({{"end_graph", ast_.InitList({header_.stream_id})}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_END_GRAPH, EndGraphTaskCodeBuilder);
}  // namespace ge
