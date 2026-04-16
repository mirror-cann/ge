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

#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {
Status NotifyRecordTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  notify_id_ = context.task_def.notify_id();
  GE_ASSERT_TRUE(notify_id_ < context.runtime->notify_num,
                 "[OM2][Check][Param] notify list size:%u, cur:%u!", context.runtime->notify_num, notify_id_);
  return SUCCESS;
}

Status NotifyRecordTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(ast_.Comment("============================= " + header_.op_name + " ==============================="));
  items.push_back(ChkStatus(ast_.Call("KernelNotifyRecordDistribute", {
      ast_.Str(header_.op_name),
      notify_list_[static_cast<int>(notify_id_)],
      stream_list_[static_cast<int>(header_.stream_id)],
  })));
  return SUCCESS;
}

Status NotifyRecordTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto notify = ast_.Var("aclrtNotify", "notify");
  auto stream = ast_.Var("aclrtStream", "stream");
  items.push_back(ast_.DefineFunction("KernelNotifyRecordDistribute", {op_name, notify, stream}, "aclError", {
      ChkRt(RtSetTaskTag(op_name)),
      ChkStatus(AclrtRecordNotify(notify, stream)),
      ast_.Return("ACL_SUCCESS"),
  }));
  return SUCCESS;
}

int64_t NotifyRecordTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  return static_cast<int64_t>(task_def.id());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_NOTIFY_RECORD, NotifyRecordTaskCodeBuilder);
}  // namespace ge
