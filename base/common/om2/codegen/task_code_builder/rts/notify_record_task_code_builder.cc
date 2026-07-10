/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "notify_record_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"

#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {
Status NotifyRecordTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  build_data_.notify_id = context.task_def.notify_id();
  GE_ASSERT_TRUE(build_data_.notify_id < context.runtime->notify_num,
                 "[OM2][Check][Param] notify list size:%u, cur:%u!", context.runtime->notify_num,
                 build_data_.notify_id);
  build_data_.stream_id = header_.stream_id;
  return SUCCESS;
}

Status NotifyRecordTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  body.push_back(ChkRt(RtSetTaskTag(op.Arrow("op_name"))));
  body.push_back(ChkStatus(
      AclrtRecordNotify(ctx.Attr("notify_list")[op.Arrow("dispatch_info").Attr("notify").Attr("notify_id")],
                        ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("notify").Attr("stream_id")])));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

int64_t NotifyRecordTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  return static_cast<int64_t>(task_def.id());
}

std::string NotifyRecordTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status NotifyRecordTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  fields.push_back({"dispatch_info",
                    ast_.DesignatedInit({{"notify", ast_.InitList({build_data_.notify_id, build_data_.stream_id})}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_NOTIFY_RECORD, NotifyRecordTaskCodeBuilder);
}  // namespace ge
