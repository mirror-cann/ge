/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "barrier_task_code_builder.h"

#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"
#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/ge_common/debug/ge_log.h"

namespace ge {
constexpr uint32_t kBarrierRtGeneralCtrlInputCnt = 3U;

Status BarrierTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);

  const domi::CmoBarrierTaskDef &barrier_task_def = context.task_def.cmo_barrier_task();

  int32_t barrier_info_count = barrier_task_def.barrier_info_size();
  build_data_.barrier_task_info.logicIdNum = static_cast<uint8_t>(barrier_task_def.logic_id_num());
  if ((build_data_.barrier_task_info.logicIdNum != static_cast<uint8_t>(barrier_info_count)) ||
      (barrier_info_count > static_cast<int32_t>(RT_CMO_MAX_BARRIER_NUM))) {
    GELOGE(PARAM_INVALID, "Invalid barrier task: logicIdNum %u, barrier_info_count %d, max %d",
           build_data_.barrier_task_info.logicIdNum, barrier_info_count, static_cast<int32_t>(RT_CMO_MAX_BARRIER_NUM));
    return PARAM_INVALID;
  }

  for (int32_t index = 0; index < barrier_info_count; ++index) {
    const domi::CmoBarrierInfoDef &barrier_info_def = barrier_task_def.barrier_info(index);
    build_data_.barrier_task_info.cmoInfo[index].cmoType = static_cast<uint16_t>(barrier_info_def.cmo_type());
    build_data_.barrier_task_info.cmoInfo[index].logicId = barrier_info_def.logic_id();
  }
  build_data_.stream_id = header_.stream_id;

  GELOGI("BarrierTaskCodeBuilder: logicIdNum[%u], barrier_info_count[%d], stream_id[%u]",
         build_data_.barrier_task_info.logicIdNum, barrier_info_count, build_data_.stream_id);

  return SUCCESS;
}

Status BarrierTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  auto inputs = ast_.Var("std::array<uintptr_t, 3U>", "inputs");
  body.push_back(ast_.VarDecl(
      inputs,
      ast_.InitList(
          {ast_.ReinterpretCast("uintptr_t", op.Arrow("dispatch_info").Attr("barrier").Attr("info").Addr()),
           ast_.ReinterpretCast("uintptr_t",
                                ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("barrier").Attr("stream_id")]),
           ast_.StaticCast("uintptr_t", ast_.UInt(0))})));
  body.push_back(ChkRt(RtGeneralCtrl(inputs[0].Addr(), ast_.StaticCast("uint32_t", kBarrierRtGeneralCtrlInputCnt),
                                     "RT_GNL_CTRL_TYPE_BARRIER_TSK")));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

Status BarrierTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  std::vector<Arg> cmo_info;
  for (int32_t i = 0; i < build_data_.barrier_task_info.logicIdNum; ++i) {
    cmo_info.push_back(ast_.InitList({static_cast<int64_t>(build_data_.barrier_task_info.cmoInfo[i].cmoType),
                                      static_cast<int64_t>(build_data_.barrier_task_info.cmoInfo[i].logicId)}));
  }
  fields.push_back(
      {"dispatch_info",
       ast_.DesignatedInit(
           {{"barrier", ast_.InitList({build_data_.stream_id, ast_.InitList({build_data_.barrier_task_info.logicIdNum,
                                                                             ast_.InitList(cmo_info)})})}})});
  return SUCCESS;
}

std::string BarrierTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_BARRIER, BarrierTaskCodeBuilder);
}  // namespace ge
