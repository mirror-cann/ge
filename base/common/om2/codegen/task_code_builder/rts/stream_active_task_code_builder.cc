/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "stream_active_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"

#include "common/om2/codegen/task_code_builder_factory.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"

namespace ge {
Status StreamActiveTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.op_index_to_count_map);
  // active stream
  uint32_t internal_index = 0;
  const uint32_t op_index = context.task_def.stream_active().op_index();
  auto it = context.op_index_to_count_map->find(op_index);
  if (it == context.op_index_to_count_map->end()) {
    internal_index = 0U;
    (*context.op_index_to_count_map)[op_index] = 1U;
  } else {
    internal_index = it->second;
    ++(*context.op_index_to_count_map)[op_index];
  }

  GE_ASSERT_TRUE(AttrUtils::GetListInt(context.op_desc, ATTR_NAME_ACTIVE_STREAM_LIST, active_stream_index_list_),
                 "[Get][Attr] %s in op:%s(%s) fail", ATTR_NAME_ACTIVE_STREAM_LIST.c_str(),
                 context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
  GE_ASSERT_TRUE(internal_index < active_stream_index_list_.size(),
                 "[OM2][Check][Param] stream id index invalid. index:%u, list size:%zu, op:%s", internal_index,
                 active_stream_index_list_.size(), context.op_desc->GetName().c_str());
  GE_ASSERT_TRUE(active_stream_index_list_[static_cast<size_t>(internal_index)] < context.runtime->stream_num,
                 "[OM2][Check][Param] active_stream_index:%u in op:%s(%s) >= stream size:%u in model",
                 active_stream_index_list_[static_cast<size_t>(internal_index)], context.op_desc->GetName().c_str(),
                 context.op_desc->GetType().c_str(), context.runtime->stream_num);

  // stream
  const uint32_t stream_id = context.task_def.stream_id();
  GE_ASSERT_TRUE(stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, stream_id);

  GELOGI("Stream Active Task Codegen: op[%s], internal index[%u], active stream id[%u], stream id[%u].",
         context.op_desc->GetName().c_str(), internal_index,
         active_stream_index_list_[static_cast<size_t>(internal_index)], stream_id);
  build_data_.active_stream_id = active_stream_index_list_[static_cast<size_t>(internal_index)];
  build_data_.stream_id = header_.stream_id;
  return SUCCESS;
}

Status StreamActiveTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  body.push_back(ChkRt(RtSetTaskTag(op.Arrow("op_name"))));
  body.push_back(ChkStatus(AclrtActiveStream(
      ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("stream_active").Attr("active_stream_id")],
      ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("stream_active").Attr("stream_id")])));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

int64_t StreamActiveTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  return static_cast<int64_t>(task_def.stream_active().op_index());
}

std::string StreamActiveTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status StreamActiveTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  fields.push_back(
      {"dispatch_info",
       ast_.DesignatedInit({{"stream_active", ast_.InitList({build_data_.active_stream_id, build_data_.stream_id})}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_STREAM_ACTIVE, StreamActiveTaskCodeBuilder);
}  // namespace ge
