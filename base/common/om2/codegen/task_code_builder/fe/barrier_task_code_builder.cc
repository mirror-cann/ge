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

#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/ge_common/debug/ge_log.h"

namespace ge {
Status BarrierTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);

  const domi::CmoBarrierTaskDef &barrier_task_def = context.task_def.cmo_barrier_task();

  barrier_task_info_.logicIdNum = static_cast<uint8_t>(barrier_task_def.logic_id_num());
  barrier_info_count_ = barrier_task_def.barrier_info_size();
  if (barrier_info_count_ > static_cast<int32_t>(RT_CMO_MAX_BARRIER_NUM)) {
    GELOGE(PARAM_INVALID, "Barrier info size %d exceeds max %d", barrier_info_count_,
           static_cast<int32_t>(RT_CMO_MAX_BARRIER_NUM));
    return PARAM_INVALID;
  }

  for (int32_t index = 0; index < barrier_info_count_; ++index) {
    const domi::CmoBarrierInfoDef &barrier_info_def = barrier_task_def.barrier_info(index);
    barrier_task_info_.cmoInfo[index].cmoType = static_cast<uint16_t>(barrier_info_def.cmo_type());
    barrier_task_info_.cmoInfo[index].logicId = barrier_info_def.logic_id();
  }

  GELOGI("BarrierTaskCodeBuilder: logicIdNum[%u], barrier_info_count[%d], stream_id[%u]",
         barrier_task_info_.logicIdNum, barrier_info_count_, header_.stream_id);

  return SUCCESS;
}

Status BarrierTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(
      ast_.Comment("============================= " + header_.op_name + " (Barrier) ==============================="));

  std::vector<Arg> cmo_info_init;
  cmo_info_init.reserve(barrier_info_count_);
  for (int32_t i = 0; i < barrier_info_count_; ++i) {
    cmo_info_init.push_back(ast_.InitList(
        {ast_.StaticCast("uint16_t", barrier_task_info_.cmoInfo[i].cmoType),
         ast_.UInt(barrier_task_info_.cmoInfo[i].logicId)}));
  }

  auto barrier_info_var = ast_.Var("rtBarrierTaskInfo_t", "barrier_info");
  items.push_back(
      ast_.VarDecl(barrier_info_var,
                   ast_.InitList({ast_.StaticCast("uint8_t", barrier_task_info_.logicIdNum),
                                  ast_.InitList(cmo_info_init)})));

  items.push_back(
      ChkStatus(ast_.Call("KernelBarrierTaskDistribute",
                          {barrier_info_var, stream_list_[static_cast<int>(header_.stream_id)], ast_.UInt(0)})));

  return SUCCESS;
}

Status BarrierTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto barrier_info = ast_.Var("const rtBarrierTaskInfo_t &", "barrier_info");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto flag = ast_.Var("uint32_t", "flag");

  auto inputs = ast_.Var("std::array<uintptr_t, 3U>", "inputs");
  items.push_back(ast_.DefineFunction(
      "KernelBarrierTaskDistribute", {barrier_info, stream, flag}, "aclError",
      {ast_.VarDecl(inputs,
                    ast_.InitList({ast_.ReinterpretCast("uintptr_t", barrier_info.Addr()),
                                   ast_.ReinterpretCast("uintptr_t", stream), ast_.StaticCast("uintptr_t", flag)})),
       ChkRt(RtGeneralCtrl(inputs[0].Addr(), ast_.StaticCast("uint32_t", 3),
                         "RT_GNL_CTRL_TYPE_BARRIER_TSK")),
       ast_.Return("ACL_SUCCESS")}));

  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_BARRIER, BarrierTaskCodeBuilder);
}  // namespace ge