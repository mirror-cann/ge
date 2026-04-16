/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "stream_switch_task_code_builder.h"

#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"

namespace ge {
Status StreamSwitchTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  // input_ptr, value_ptr
  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveInputAddrs(context, input_addr_nodes_));
  GE_ASSERT_TRUE(input_addr_nodes_.size() >= 2U, "[OM2][Check][Param] %s(%s) input addr size:%zu is invalid.",
                 context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str(), input_addr_nodes_.size());

  // cond
  GE_ASSERT_TRUE(AttrUtils::GetInt(context.op_desc, ATTR_NAME_STREAM_SWITCH_COND, cond_),
                "[Get][Attr] %s in op:%s(%s) fail", ATTR_NAME_STREAM_SWITCH_COND.c_str(),
                context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());

  // true_stream
  std::vector<uint32_t> active_stream_list;
  GE_ASSERT_TRUE(AttrUtils::GetListInt(context.op_desc, ATTR_NAME_ACTIVE_STREAM_LIST, active_stream_list) &&
                active_stream_list.size() == kTrueBranchStreamNum_1,
                "[Get][Attr] %s in op:%s fail, active_stream_list.size():%zu", ATTR_NAME_ACTIVE_STREAM_LIST.c_str(),
                context.op_desc->GetName().c_str(), active_stream_list.size());
  true_stream_id_ = active_stream_list.front();
  GE_ASSERT_TRUE(true_stream_id_ < context.runtime->stream_num,
                "[OM2][Check][Param] active_stream_index:%zu in op:%s(%s) >= stream list size:%u in model",
                static_cast<size_t>(true_stream_id_), context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str(),
                context.runtime->stream_num);
  // stream
  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] stream list size:%u, cur:%u!", context.runtime->stream_num, header_.stream_id);

  // data_type
  if (context.op_desc->HasAttr(ATTR_NAME_SWITCH_DATA_TYPE) &&
      !AttrUtils::GetInt(context.op_desc, ATTR_NAME_SWITCH_DATA_TYPE, data_type_)) {
    REPORT_INNER_ERR_MSG("E19999", "Get Attr:%s in op:%s(%s) fail, attribute value not int",
                        ATTR_NAME_SWITCH_DATA_TYPE.c_str(), context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
    GELOGE(FAILED, "[Get][Attr] %s in op:%s(%s) fail, attribute value not int",
            ATTR_NAME_SWITCH_DATA_TYPE.c_str(), context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
    return FAILED;
  }
  GELOGI("Stream Switch Task Codegen: op[%s], cond_[%u], true stream id[%lu], stream id[%u], data type[%ld].",
         context.op_desc->GetName().c_str(), cond_, static_cast<unsigned long>(true_stream_id_), header_.stream_id,
         data_type_);
  return SUCCESS;
}

Status StreamSwitchTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(ast_.Comment("============================= " + header_.op_name + " ==============================="));
  for (const auto &input_addr_node : input_addr_nodes_) {
    if (input_addr_node.is_reused_from_upstream) {
      continue;
    }
    items.push_back(ast_.VarDecl("auto", input_addr_node.symbol_hint,
                                 GetAddr(total_dev_mem_ptr_, input_addr_node.mem_offset)));
  }
  items.push_back(ChkStatus(ast_.Call("KernelStreamSwitchDistribute", {
      ast_.Str(header_.op_name),
      input_addr_nodes_[0].symbol_hint,
      ast_.StaticCast("rtCondition_t", static_cast<int64_t>(cond_)),
      input_addr_nodes_[1].symbol_hint,
      stream_list_[static_cast<int>(true_stream_id_)],
      stream_list_[static_cast<int>(header_.stream_id)],
      ast_.StaticCast("rtSwitchDataType_t", data_type_),
  })));
  return SUCCESS;
}

Status StreamSwitchTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto input_ptr = ast_.Var("void *", "input_ptr");
  auto cond = ast_.Var("rtCondition_t", "cond");
  auto value_ptr = ast_.Var("void *", "value_ptr");
  auto true_stream = ast_.Var("aclrtStream", "true_stream");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto data_type = ast_.Var("rtSwitchDataType_t", "data_type");
  items.push_back(ast_.DefineFunction("KernelStreamSwitchDistribute",
      {op_name, input_ptr, cond, value_ptr, true_stream, stream, data_type}, "aclError", {
          ChkRt(RtSetTaskTag(op_name)),
          ChkRt(RtStreamSwitchEx(input_ptr, cond, value_ptr, true_stream, stream, data_type)),
          ast_.Return("ACL_SUCCESS"),
      }));
  return SUCCESS;
}

int64_t StreamSwitchTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const auto &stream_switch_def = task_def.stream_switch();
  return static_cast<int64_t>(stream_switch_def.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_STREAM_SWITCH, StreamSwitchTaskCodeBuilder);
}  // namespace ge
