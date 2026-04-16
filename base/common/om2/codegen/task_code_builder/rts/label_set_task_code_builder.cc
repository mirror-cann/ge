/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "label_set_task_code_builder.h"

#include "common/om2/codegen/task_code_builder_factory.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"

namespace ge {
Status LabelSetTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] stream list size:%u, cur:%u!", context.runtime->stream_num, header_.stream_id);
  int64_t label_index = 0;
  GE_ASSERT_TRUE(AttrUtils::GetInt(context.op_desc, ATTR_NAME_LABEL_SWITCH_INDEX, label_index),
                 "[OM2][Get][Attr] %s attr %s does not exist.", context.op_desc->GetName().c_str(),
                 ATTR_NAME_LABEL_SWITCH_INDEX.c_str());
  GE_ASSERT_TRUE(label_index >= 0, "[OM2][Check][Param] label index %" PRId64 " is invalid.", label_index);
  label_index_ = static_cast<uint32_t>(label_index);
  GE_ASSERT_TRUE(label_index_ < context.runtime->label_num,
                 "[OM2][Check][Param] label list size:%u, cur:%u, op:%s(%s).", context.runtime->label_num,
                 label_index_, context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
  return SUCCESS;
}

Status LabelSetTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(ast_.Comment("============================= " + header_.op_name + " ==============================="));
  items.push_back(ChkStatus(ast_.Call("KernelLabelSetDistribute", {
      label_list_[static_cast<int>(label_index_)],
      stream_list_[static_cast<int>(header_.stream_id)],
  })));
  return SUCCESS;
}

Status LabelSetTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto label = ast_.Var("aclrtLabel", "label");
  auto stream = ast_.Var("aclrtStream", "stream");
  items.push_back(ast_.DefineFunction("KernelLabelSetDistribute", {label, stream}, "aclError", {
      ChkStatus(AclrtSetLabel(label, stream)),
      ast_.Return("ACL_SUCCESS"),
  }));
  return SUCCESS;
}

int64_t LabelSetTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  return static_cast<int64_t>(task_def.label_set().op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_LABEL_SET, LabelSetTaskCodeBuilder);
}  // namespace ge
