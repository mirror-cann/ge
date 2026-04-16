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
    internal_index = 0;
    (*context.op_index_to_count_map)[op_index] = 1;
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
  GE_ASSERT_TRUE(active_stream_index_list_[internal_index] < context.runtime->stream_num,
                "[OM2][Check][Param] active_stream_index:%u in op:%s(%s) >= stream size:%u in model",
                active_stream_index_list_[internal_index], context.op_desc->GetName().c_str(),
                context.op_desc->GetType().c_str(), context.runtime->stream_num);

  // stream
  const uint32_t stream_id = context.task_def.stream_id();
  GE_ASSERT_TRUE(stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] stream list size:%u, cur:%u!", context.runtime->stream_num, stream_id);

  GELOGI("Stream Active Task Codegen: op[%s], internal index[%u], active stream id[%u], stream id[%u].",
        context.op_desc->GetName().c_str(), internal_index, active_stream_index_list_[internal_index], stream_id);
  active_stream_id_ = active_stream_index_list_[internal_index];
  return SUCCESS;
}

Status StreamActiveTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(ast_.Comment("============================= " + header_.op_name + " ==============================="));
  items.push_back(ChkStatus(ast_.Call("KernelStreamActiveDistribute", {
      ast_.Str(header_.op_name),
      stream_list_[static_cast<int>(active_stream_id_)],
      stream_list_[static_cast<int>(header_.stream_id)],
  })));
  return SUCCESS;
}

Status StreamActiveTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto active_stream = ast_.Var("aclrtStream", "active_stream");
  auto stream = ast_.Var("aclrtStream", "stream");
  items.push_back(ast_.DefineFunction("KernelStreamActiveDistribute", {op_name, active_stream, stream}, "aclError", {
      ChkRt(RtSetTaskTag(op_name)),
      ChkStatus(AclrtActiveStream(active_stream, stream)),
      ast_.Return("ACL_SUCCESS"),
  }));
  return SUCCESS;
}

int64_t StreamActiveTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  return static_cast<int64_t>(task_def.stream_active().op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_STREAM_ACTIVE, StreamActiveTaskCodeBuilder);
}  // namespace ge
