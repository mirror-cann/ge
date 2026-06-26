/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "event_wait_task_code_builder.h"

#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {
Status EventWaitTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  event_id_ = context.task_def.event_id();
  GE_ASSERT_TRUE(event_id_ < context.runtime->event_num, "[OM2][Check][Param] event list size:%u, cur:%u!",
                 context.runtime->event_num, event_id_);
  return SUCCESS;
}

Status EventWaitTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(
      ast_.Comment("============================= " + header_.op_name + " ==============================="));
  items.push_back(
      ChkStatus(ast_.Call("KernelEventWaitDistribute", {
                                                           ast_.Str(header_.op_name),
                                                           event_list_[static_cast<int32_t>(event_id_)],
                                                           stream_list_[static_cast<int32_t>(header_.stream_id)],
                                                       })));
  return SUCCESS;
}

Status EventWaitTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto event = ast_.Var("aclrtEvent", "event");
  auto stream = ast_.Var("aclrtStream", "stream");
  items.push_back(ast_.DefineFunction("KernelEventWaitDistribute", {op_name, event, stream}, "aclError",
                                      {
                                          ChkRt(RtSetTaskTag(op_name)),
                                          ChkRt(RtStreamWaitEvent(stream, event)),
                                          ChkRt(RtSetTaskTag(op_name)),
                                          ChkStatus(AclrtResetEvent(event, stream)),
                                          ast_.Return("ACL_SUCCESS"),
                                      }));
  return SUCCESS;
}

int64_t EventWaitTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  if (!task_def.has_event_ex()) {
    return kInvalidOpIndex;
  }
  return static_cast<int64_t>(task_def.event_ex().op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_EVENT_WAIT, EventWaitTaskCodeBuilder);
}  // namespace ge
