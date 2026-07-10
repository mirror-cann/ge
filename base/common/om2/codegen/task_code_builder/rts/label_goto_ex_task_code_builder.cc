/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "label_goto_ex_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"
#include <cinttypes>

#include "acl/acl_rt.h"
#include "common/om2/codegen/task_code_builder_factory.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"
#include "common/om2/codegen/om2_codegen_utils.h"
#include "rt_external_device.h"

namespace ge {
Status LabelGotoExTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_TRUE(AttrUtils::GetInt(context.op_desc, ATTR_NAME_LABEL_SWITCH_INDEX, build_data_.label_index),
                 "[OM2][Get][Attr] %s in op:%s(%s) fail.", ATTR_NAME_LABEL_SWITCH_INDEX.c_str(),
                 header_.op_name.c_str(), context.op_desc->GetType().c_str());
  GE_ASSERT_TRUE(build_data_.label_index < context.runtime->label_num,
                 "[OM2][Check][Param] label list size:%u, cur:%u, op:%s(%s).", context.runtime->label_num,
                 build_data_.label_index, context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
  // memory type
  build_data_.memory_type = rtGetTsMemType(MEM_REQUEST_FEATURE_DEFAULT, static_cast<uint32_t>(sizeof(uint64_t)));
  // stream
  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);
  build_data_.stream_id = header_.stream_id;

  GELOGI("memory_type: %u, stream_id %u, op_index %ld", build_data_.memory_type, build_data_.stream_id,
         header_.op_index);
  return SUCCESS;
}

Status LabelGotoExTaskCodeBuilder::RenderInitResource(std::vector<BodyItem> &items) {
  items.push_back(ChkStatus(
      ast_.Call("CreateLabelListForLabelGotoEx", {header_.op_index, static_cast<int64_t>(build_data_.label_index)})));
  return SUCCESS;
}

Status LabelGotoExTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  body.push_back(ast_.VarDecl(ast_.Var("void *", "index_value"), Arg(nullptr)));
  body.push_back(ast_.VarDecl(ast_.Var("uint64_t", "branch_index"), Arg(0)));
  body.push_back(
      ChkStatus(ast_.Call("MallocDeviceMemory", {
                                                    ast_.Var("", "index_value"),
                                                    ast_.Sizeof("uint64_t"),
                                                    op.Arrow("dispatch_info").Attr("label_goto").Attr("memory_type"),
                                                    ctx.Attr("dev_dynamic_mem_ptrs"),
                                                })));
  body.push_back(
      ChkStatus(AclrtMemcpy(ast_.Var("", "index_value"), ast_.Sizeof("uint64_t"), ast_.Var("", "branch_index").Addr(),
                            ast_.Sizeof("uint64_t"), "ACL_MEMCPY_HOST_TO_DEVICE")));
  body.push_back(ChkStatus(AclrtSwitchLabelByIndex(
      ast_.Var("", "index_value"), ast_.UInt(1),
      ctx.Attr("label_goto_ex_label_list")[op.Arrow("dispatch_info").Attr("label_goto").Attr("op_idx")],
      ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("label_goto").Attr("stream_id")])));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

int64_t LabelGotoExTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::LabelGotoExDef &label_goto = task_def.label_goto_ex();
  return static_cast<int64_t>(label_goto.op_index());
}

std::string LabelGotoExTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status LabelGotoExTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  fields.push_back(
      {"dispatch_info",
       ast_.DesignatedInit({{"label_goto", ast_.InitList({static_cast<int64_t>(header_.op_index), build_data_.stream_id,
                                                          build_data_.memory_type})}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_STREAM_LABEL_GOTO, LabelGotoExTaskCodeBuilder);
}  // namespace ge
