/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cmo_task_code_builder.h"

#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"
#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "common/ge_common/debug/ge_log.h"

namespace ge {
Status CmoTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);

  const domi::CmoTaskDef &cmo_task_def = context.task_def.cmo_task();

  build_data_.task_info.cmoType = static_cast<uint16_t>(cmo_task_def.cmo_type());
  build_data_.task_info.logicId = cmo_task_def.logic_id();
  build_data_.task_info.opCode = static_cast<uint16_t>(cmo_task_def.op_code());
  build_data_.task_info.qos = static_cast<uint8_t>(cmo_task_def.qos());
  build_data_.task_info.partId = static_cast<uint8_t>(cmo_task_def.part_id());
  build_data_.task_info.pmg = static_cast<uint8_t>(cmo_task_def.pmg());
  build_data_.task_info.numInner = static_cast<uint16_t>(cmo_task_def.num_inner());
  build_data_.task_info.numOuter = static_cast<uint16_t>(cmo_task_def.num_outer());
  build_data_.task_info.lengthInner = cmo_task_def.length_inner();
  build_data_.task_info.striderOuter = cmo_task_def.strider_outer();
  build_data_.task_info.striderInner = cmo_task_def.strider_inner();

  AddrSemantic source_addr;
  GE_ASSERT_SUCCESS(
      Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(cmo_task_def.source_addr()), source_addr, true, 0U));

  GELOGI(
      "CmoTaskCodeBuilder: cmoType[%u], logicId[%u], opCode[%u], qos[%u], partId[%u], pmg[%u], "
      "numInner[%u], numOuter[%u], lengthInner[%u], source_addr[0x%" PRIx64
      "], "
      "striderOuter[%u], striderInner[%u], stream_id[%u]",
      build_data_.task_info.cmoType, build_data_.task_info.logicId, build_data_.task_info.opCode,
      build_data_.task_info.qos, build_data_.task_info.partId, build_data_.task_info.pmg,
      build_data_.task_info.numInner, build_data_.task_info.numOuter, build_data_.task_info.lengthInner,
      source_addr.mem_offset, build_data_.task_info.striderOuter, build_data_.task_info.striderInner,
      header_.stream_id);

  build_data_.ordered_args.push_back(TaskCodeBuilderUtil::ConvertAddrDesc(source_addr));
  build_data_.stream_id = header_.stream_id;
  return SUCCESS;
}

Status CmoTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto cmo_info = ast_.Var("const rtCmoTaskInfo_t &", "cmo_info");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto flag = ast_.Var("uint32_t", "flag");

  auto inputs = ast_.Var("std::array<uintptr_t, 3U>", "inputs");
  items.push_back(ast_.DefineFunction(
      "KernelCmoTaskDistribute", {cmo_info, stream, flag}, "aclError",
      {ast_.VarDecl(inputs,
                    ast_.InitList({ast_.ReinterpretCast("uintptr_t", cmo_info.Addr()),
                                   ast_.ReinterpretCast("uintptr_t", stream), ast_.StaticCast("uintptr_t", flag)})),
       ChkRt(RtGeneralCtrl(inputs[0].Addr(), ast_.StaticCast("uint32_t", 3), "RT_GNL_CTRL_TYPE_CMO_TSK")),
       ast_.Return("ACL_SUCCESS")}));

  // dispatch function
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  auto d = op.Arrow("dispatch_info").Attr("cmo");
  body.push_back(ast_.VarDecl(ast_.Var("rtCmoTaskInfo_t", "cmo_info"), d.Attr("task_info")));
  body.push_back(ast_.Assign(
      ast_.Var("", "cmo_info").Attr("sourceAddr"),
      ast_.ReinterpretCast("uint64_t",
                           ast_.Call("ResolveOpAddr", {d.Attr("args_info")[ast_.UInt(0)].Attr("addr").Attr("mem_src"),
                                                       d.Attr("args_info")[ast_.UInt(0)].Attr("addr").Attr("offset"),
                                                       ctx.Attr("total_dev_mem_ptr"), ctx.Attr("session_scope_mem_ptr"),
                                                       ctx.Attr("constants")}))));
  body.push_back(ChkStatus(ast_.Call("KernelCmoTaskDistribute", {
                                                                    ast_.Var("", "cmo_info"),
                                                                    ctx.Attr("stream_list")[d.Attr("stream_id")],
                                                                    ast_.UInt(0),
                                                                })));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

Status CmoTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  fields.emplace_back(
      "dispatch_info",
      ast_.DesignatedInit(
          {{"cmo",
            ast_.DesignatedInit(
                {{"args_info", TaskCodeBuilderUtil::RenderOpArgDesc(ast_, build_data_.ordered_args)},
                 {"task_info", ast_.InitList({build_data_.task_info.qos, build_data_.task_info.partId,
                                              build_data_.task_info.pmg, ast_.UInt(0U), build_data_.task_info.cmoType,
                                              build_data_.task_info.opCode, build_data_.task_info.numInner,
                                              build_data_.task_info.numOuter, build_data_.task_info.logicId,
                                              build_data_.task_info.lengthInner, ast_.StaticCast("uint64_t", 0),
                                              build_data_.task_info.striderOuter, build_data_.task_info.striderInner})},
                 {"stream_id", build_data_.stream_id}})}}));
  return SUCCESS;
}

std::string CmoTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_CMO, CmoTaskCodeBuilder);
}  // namespace ge
