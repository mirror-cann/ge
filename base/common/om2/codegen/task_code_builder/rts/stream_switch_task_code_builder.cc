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
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"

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
  GE_ASSERT_TRUE(AttrUtils::GetInt(context.op_desc, ATTR_NAME_STREAM_SWITCH_COND, build_data_.cond),
                 "[Get][Attr] %s in op:%s(%s) fail", ATTR_NAME_STREAM_SWITCH_COND.c_str(),
                 context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());

  // true_stream
  std::vector<uint32_t> active_stream_list;
  GE_ASSERT_TRUE(AttrUtils::GetListInt(context.op_desc, ATTR_NAME_ACTIVE_STREAM_LIST, active_stream_list) &&
                     active_stream_list.size() == kTrueBranchStreamNum_1,
                 "[Get][Attr] %s in op:%s fail, active_stream_list.size():%zu", ATTR_NAME_ACTIVE_STREAM_LIST.c_str(),
                 context.op_desc->GetName().c_str(), active_stream_list.size());
  build_data_.true_stream_id = active_stream_list.front();
  GE_ASSERT_TRUE(build_data_.true_stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] active_stream_index:%zu in op:%s(%s) >= stream list size:%u in model",
                 static_cast<size_t>(build_data_.true_stream_id), context.op_desc->GetName().c_str(),
                 context.op_desc->GetType().c_str(), context.runtime->stream_num);
  // stream
  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);

  // data_type
  if (context.op_desc->HasAttr(ATTR_NAME_SWITCH_DATA_TYPE) &&
      !AttrUtils::GetInt(context.op_desc, ATTR_NAME_SWITCH_DATA_TYPE, build_data_.data_type)) {
    REPORT_INNER_ERR_MSG("E19999", "Get Attr:%s in op:%s(%s) fail, attribute value not int",
                         ATTR_NAME_SWITCH_DATA_TYPE.c_str(), context.op_desc->GetName().c_str(),
                         context.op_desc->GetType().c_str());
    GELOGE(FAILED, "[Get][Attr] %s in op:%s(%s) fail, attribute value not int", ATTR_NAME_SWITCH_DATA_TYPE.c_str(),
           context.op_desc->GetName().c_str(), context.op_desc->GetType().c_str());
    return FAILED;
  }
  GELOGI("Stream Switch Task Codegen: op[%s], cond_[%u], true stream id[%lu], stream id[%u], data type[%ld].",
         context.op_desc->GetName().c_str(), build_data_.cond, static_cast<unsigned long>(build_data_.true_stream_id),
         header_.stream_id, build_data_.data_type);
  build_data_.stream_id = header_.stream_id;
  for (const auto &addr : input_addr_nodes_) {
    auto arg = TaskCodeBuilderUtil::ConvertAddrDesc(addr);
    arg.has_tensor_info = false;
    build_data_.ordered_args.push_back(std::move(arg));
  }
  return SUCCESS;
}

Status StreamSwitchTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> dispatch_body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  dispatch_body.push_back(ChkRt(RtSetTaskTag(op.Arrow("op_name"))));
  dispatch_body.push_back(ChkRt(AclrtSwitchStream(
      ast_.Call("ResolveOpAddr",
                {op.Arrow("dispatch_info").Attr("stream_switch").Attr("args_info")[0].Attr("addr").Attr("mem_src"),
                 op.Arrow("dispatch_info").Attr("stream_switch").Attr("args_info")[0].Attr("addr").Attr("offset"),
                 ctx.Attr("total_dev_mem_ptr"), ctx.Attr("session_scope_mem_ptr"), ctx.Attr("constants")}),
      ast_.StaticCast("aclrtCondition", op.Arrow("dispatch_info").Attr("stream_switch").Attr("cond")),
      ast_.Call("ResolveOpAddr",
                {op.Arrow("dispatch_info").Attr("stream_switch").Attr("args_info")[1].Attr("addr").Attr("mem_src"),
                 op.Arrow("dispatch_info").Attr("stream_switch").Attr("args_info")[1].Attr("addr").Attr("offset"),
                 ctx.Attr("total_dev_mem_ptr"), ctx.Attr("session_scope_mem_ptr"), ctx.Attr("constants")}),
      ast_.StaticCast("aclrtCompareDataType", op.Arrow("dispatch_info").Attr("stream_switch").Attr("data_type")),
      ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("stream_switch").Attr("true_stream_id")],
      ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("stream_switch").Attr("stream_id")])));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, dispatch_body, items));
  return SUCCESS;
}

int64_t StreamSwitchTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const auto &stream_switch_def = task_def.stream_switch();
  return static_cast<int64_t>(stream_switch_def.op_index());
}

std::string StreamSwitchTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status StreamSwitchTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  fields.push_back(
      {"dispatch_info",
       ast_.DesignatedInit(
           {{"stream_switch", ast_.InitList({TaskCodeBuilderUtil::RenderOpArgDesc(ast_, build_data_.ordered_args),
                                             build_data_.true_stream_id, build_data_.stream_id, build_data_.cond,
                                             build_data_.data_type})}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_STREAM_SWITCH, StreamSwitchTaskCodeBuilder);
}  // namespace ge
