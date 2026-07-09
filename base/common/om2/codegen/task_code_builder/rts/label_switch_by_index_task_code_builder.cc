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
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"

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
  build_data_.branch_max = label_switch.label_max();

  GE_ASSERT_TRUE(AttrUtils::GetListInt(context.op_desc, ATTR_NAME_LABEL_SWITCH_LIST, build_data_.label_indices),
                 "[Get][Attr] %s in op:%s(%s) fail, attribute value not set", ATTR_NAME_LABEL_SWITCH_LIST.c_str(),
                 context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());

  const auto validate_label_id = [&context](const std::vector<uint32_t> &ids) -> bool {
    return std::all_of(ids.cbegin(), ids.cend(),
                       [&context](const uint32_t id) -> bool { return id < context.runtime->label_num; });
  };
  GE_ASSERT_TRUE(!build_data_.label_indices.empty() && build_data_.label_indices.size() == build_data_.branch_max &&
                     validate_label_id(build_data_.label_indices),
                 "[Check][Param] %s(%s) label index size:%zu, task branch max:%u.", context.op_desc->GetName().c_str(),
                 context.op_desc->GetType().c_str(), build_data_.label_indices.size(), build_data_.branch_max);

  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveInputAddrs(context, input_addr_nodes_));

  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);
  build_data_.stream_id = header_.stream_id;
  for (const auto &addr : input_addr_nodes_) {
    auto arg = TaskCodeBuilderUtil::ConvertAddrDesc(addr);
    arg.has_tensor_info = false;
    build_data_.ordered_args.push_back(std::move(arg));
  }
  return SUCCESS;
}

Status LabelSwitchByIndexTaskCodeBuilder::RenderInitResource(std::vector<BodyItem> &items) {
  std::vector<Arg> label_args;
  label_args.reserve(build_data_.label_indices.size());
  for (const auto label_id : build_data_.label_indices) {
    (void)label_args.emplace_back(static_cast<int64_t>(label_id));
  }
  (void)items.push_back(
      ChkStatus(ast_.Call("CreateLabelListForLabelSwitch", {header_.op_index, ast_.InitList(label_args)})));
  return SUCCESS;
}

Status LabelSwitchByIndexTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  (void)body.push_back(ChkStatus(AclrtSwitchLabelByIndex(
      ast_.Call("ResolveOpAddr",
                {op.Arrow("dispatch_info").Attr("label_switch").Attr("args_info")[0].Attr("addr").Attr("mem_src"),
                 op.Arrow("dispatch_info").Attr("label_switch").Attr("args_info")[0].Attr("addr").Attr("offset"),
                 ctx.Attr("total_dev_mem_ptr"), ctx.Attr("session_scope_mem_ptr"), ctx.Attr("constants")}),
      op.Arrow("dispatch_info").Attr("label_switch").Attr("branch_max"),
      ctx.Attr("label_switch_label_list")[op.Arrow("dispatch_info").Attr("label_switch").Attr("op_idx")],
      ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("label_switch").Attr("stream_id")])));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

int64_t LabelSwitchByIndexTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::LabelSwitchByIndexDef &label_switch = task_def.label_switch_by_index();
  return static_cast<int64_t>(label_switch.op_index());
}

std::string LabelSwitchByIndexTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status LabelSwitchByIndexTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  fields.push_back(
      {"dispatch_info",
       ast_.DesignatedInit(
           {{"label_switch", ast_.InitList({TaskCodeBuilderUtil::RenderOpArgDesc(ast_, build_data_.ordered_args),
                                            static_cast<int64_t>(header_.op_index), build_data_.branch_max,
                                            build_data_.stream_id})}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_STREAM_LABEL_SWITCH_BY_INDEX, LabelSwitchByIndexTaskCodeBuilder);
}  // namespace ge
