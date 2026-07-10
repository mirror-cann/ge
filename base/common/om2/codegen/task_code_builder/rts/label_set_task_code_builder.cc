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
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"

#include "common/om2/codegen/task_code_builder_factory.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"

namespace ge {
Status LabelSetTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);
  GE_ASSERT_TRUE(AttrUtils::GetInt(context.op_desc, ATTR_NAME_LABEL_SWITCH_INDEX, build_data_.label_index),
                 "[OM2][Get][Attr] %s attr %s does not exist.", context.op_desc->GetName().c_str(),
                 ATTR_NAME_LABEL_SWITCH_INDEX.c_str());
  GE_ASSERT_TRUE(build_data_.label_index >= 0, "[OM2][Check][Param] label index %" PRId64 " is invalid.",
                 build_data_.label_index);
  GE_ASSERT_TRUE(build_data_.label_index < static_cast<int64_t>(context.runtime->label_num),
                 "[OM2][Check][Param] label list size:%u, cur:%" PRId64 ", op:%s(%s).", context.runtime->label_num,
                 build_data_.label_index, context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
  build_data_.stream_id = header_.stream_id;
  return SUCCESS;
}

Status LabelSetTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  body.push_back(
      ChkStatus(AclrtSetLabel(ctx.Attr("label_list")[op.Arrow("dispatch_info").Attr("label_set").Attr("label_index")],
                              ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("label_set").Attr("stream_id")])));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

int64_t LabelSetTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  return static_cast<int64_t>(task_def.label_set().op_index());
}

std::string LabelSetTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status LabelSetTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  fields.push_back(
      {"dispatch_info",
       ast_.DesignatedInit({{"label_set", ast_.InitList({build_data_.label_index, build_data_.stream_id})}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_LABEL_SET, LabelSetTaskCodeBuilder);
}  // namespace ge
