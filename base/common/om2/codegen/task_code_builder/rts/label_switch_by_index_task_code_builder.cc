/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "label_switch_by_index_task_code_builder.h"

#include <algorithm>

#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"

namespace ge {
Status LabelSwitchByIndexTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  // labelList
  const domi::LabelSwitchByIndexDef &label_switch = context.task_def.label_switch_by_index();
  branch_max_ = label_switch.label_max();
 	
  GE_ASSERT_TRUE(AttrUtils::GetListInt(context.op_desc, ATTR_NAME_LABEL_SWITCH_LIST, label_idx_list_),
                "[Get][Attr] %s in op:%s(%s) fail, attribute value not set",
                ATTR_NAME_LABEL_SWITCH_LIST.c_str(), context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
  
  const auto validate_label_id = [&context](const std::vector<uint32_t> &ids) -> bool {
    return std::all_of(ids.cbegin(), ids.cend(),
                      [&context](const uint32_t id) -> bool { return id < context.runtime->label_num; });
  };
  GE_ASSERT_TRUE(!label_idx_list_.empty() && label_idx_list_.size() == branch_max_ && validate_label_id(label_idx_list_),
                "[Check][Param] %s(%s) label index size:%zu, task branch max:%u.",
                context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str(), label_idx_list_.size(), branch_max_);
  
  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveInputAddrs(context, input_addr_nodes_));

  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] stream list size:%u, cur:%u!", context.runtime->stream_num, header_.stream_id);
  return SUCCESS;
}

Status LabelSwitchByIndexTaskCodeBuilder::RenderInitResource(std::vector<BodyItem> &items) {
  std::vector<Arg> label_args;
  label_args.reserve(label_idx_list_.size());
  for (const auto label_id : label_idx_list_) {
    label_args.emplace_back(static_cast<int64_t>(label_id));
  }
  items.push_back(ChkStatus(ast_.Call("CreateLabelListForLabelSwitch",
                                      {header_.op_index, ast_.InitList(label_args)})));
  return SUCCESS;
}

Status LabelSwitchByIndexTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(ast_.Comment("============================= " + header_.op_name +
                               " ==============================="));
  for (const auto &input_addr_node : input_addr_nodes_) {
    if (input_addr_node.is_reused_from_upstream) {
      continue;
    }
    items.emplace_back(
          ast_.VarDecl("auto", input_addr_node.symbol_hint, GetAddr(total_dev_mem_ptr_, input_addr_node.mem_offset)));
  }
  items.push_back(ChkStatus(ast_.Call("KernelLabelSwitchByIndexDistribute", {
      input_addr_nodes_[0].symbol_hint,
      static_cast<int64_t>(branch_max_),
      label_switch_label_list_[static_cast<int>(header_.op_index)],
      stream_list_[static_cast<int>(header_.stream_id)],
  })));
  return SUCCESS;
}

Status LabelSwitchByIndexTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto ptr = ast_.Var("void *", "ptr");
  auto max_value = ast_.Var("uint32_t", "max_value");
  auto label_list = ast_.Var("aclrtLabelList", "label_list");
  auto stream = ast_.Var("aclrtStream", "stream");
  items.push_back(ast_.DefineFunction("KernelLabelSwitchByIndexDistribute",
                                      {ptr, max_value, label_list, stream}, "aclError", {
      ChkStatus(AclrtSwitchLabelByIndex(ptr, max_value, label_list, stream)),
      ast_.Return("ACL_SUCCESS"),
  }));
  return SUCCESS;
}

int64_t LabelSwitchByIndexTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::LabelSwitchByIndexDef &label_switch = task_def.label_switch_by_index();
  return static_cast<int64_t>(label_switch.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_STREAM_LABEL_SWITCH_BY_INDEX, LabelSwitchByIndexTaskCodeBuilder);
}  // namespace ge
