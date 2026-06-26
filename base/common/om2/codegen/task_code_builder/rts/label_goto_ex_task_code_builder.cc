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
#include <cinttypes>

#include "acl/acl_rt.h"
#include "common/om2/codegen/task_code_builder_factory.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"
#include "common/om2/codegen/om2_codegen_utils.h"

namespace ge {
Status LabelGotoExTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_TRUE(AttrUtils::GetInt(context.op_desc, ATTR_NAME_LABEL_SWITCH_INDEX, label_index_),
                 "[OM2][Get][Attr] %s in op:%s(%s) fail.", ATTR_NAME_LABEL_SWITCH_INDEX.c_str(),
                 header_.op_name.c_str(), context.op_desc->GetType().c_str());
  GE_ASSERT_TRUE(label_index_ < context.runtime->label_num,
                 "[OM2][Check][Param] label list size:%u, cur:%u, op:%s(%s).", context.runtime->label_num, label_index_,
                 context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
  // memory type
  memory_type_ = rtGetTsMemType(MEM_REQUEST_FEATURE_DEFAULT, static_cast<uint32_t>(sizeof(uint64_t)));
  // stream
  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);

  GELOGI("memory_type: %u, stream_id %u, op_index %ld", memory_type_, header_.stream_id, header_.op_index);
  return SUCCESS;
}

Status LabelGotoExTaskCodeBuilder::RenderInitResource(std::vector<BodyItem> &items) {
  items.push_back(
      ChkStatus(ast_.Call("CreateLabelListForLabelGotoEx", {header_.op_index, static_cast<int64_t>(label_index_)})));
  return SUCCESS;
}

Status LabelGotoExTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  const std::string op_head = "op" + std::to_string(header_.op_index) + "_";
  auto index_value = ast_.Var("void *", op_head + "index_value");
  auto branch_index = ast_.Var("constexpr uint64_t", op_head + "branch_index");
  auto label_goto_ex_label_list = ast_.Var("auto", "label_goto_ex_label_list_");
  items.push_back(
      ast_.Comment("============================= " + header_.op_name + " ==============================="));
  items.push_back(ast_.VarDecl(index_value, nullptr));
  items.push_back(
      ChkStatus(ast_.Call("MallocDeviceMemory", {index_value, ast_.Sizeof("uint64_t"),
                                                 static_cast<int64_t>(memory_type_), dev_dynamic_mem_ptrs_})));
  items.push_back(ast_.VarDecl(branch_index, 0));
  items.push_back(ChkStatus(AclrtMemcpy(index_value, ast_.Sizeof("uint64_t"), branch_index.Addr(),
                                        ast_.Sizeof("uint64_t"), "ACL_MEMCPY_HOST_TO_DEVICE")));
  items.push_back(ChkStatus(
      ast_.Call("KernelLabelGotoExDistribute", {
                                                   index_value,
                                                   1,
                                                   label_goto_ex_label_list[static_cast<int32_t>(header_.op_index)],
                                                   stream_list_[static_cast<int32_t>(header_.stream_id)],
                                               })));
  return SUCCESS;
}

Status LabelGotoExTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto ptr = ast_.Var("void *", "ptr");
  auto max_value = ast_.Var("uint32_t", "maxValue");
  auto label_list = ast_.Var("aclrtLabelList", "labelList");
  auto stream = ast_.Var("aclrtStream", "stream");
  items.push_back(ast_.DefineFunction("KernelLabelGotoExDistribute", {ptr, max_value, label_list, stream}, "aclError",
                                      {
                                          ChkStatus(AclrtSwitchLabelByIndex(ptr, max_value, label_list, stream)),
                                          ast_.Return("ACL_SUCCESS"),
                                      }));
  return SUCCESS;
}

int64_t LabelGotoExTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::LabelGotoExDef &label_goto = task_def.label_goto_ex();
  return static_cast<int64_t>(label_goto.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_STREAM_LABEL_GOTO, LabelGotoExTaskCodeBuilder);
}  // namespace ge
