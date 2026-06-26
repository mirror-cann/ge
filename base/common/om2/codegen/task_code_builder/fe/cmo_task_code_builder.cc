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

#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "common/ge_common/debug/ge_log.h"

namespace ge {
Status CmoTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);

  const domi::CmoTaskDef &cmo_task_def = context.task_def.cmo_task();

  cmo_task_info_.cmoType = static_cast<uint16_t>(cmo_task_def.cmo_type());
  cmo_task_info_.logicId = cmo_task_def.logic_id();
  cmo_task_info_.opCode = static_cast<uint16_t>(cmo_task_def.op_code());
  cmo_task_info_.qos = static_cast<uint8_t>(cmo_task_def.qos());
  cmo_task_info_.partId = static_cast<uint8_t>(cmo_task_def.part_id());
  cmo_task_info_.pmg = static_cast<uint8_t>(cmo_task_def.pmg());
  cmo_task_info_.numInner = static_cast<uint16_t>(cmo_task_def.num_inner());
  cmo_task_info_.numOuter = static_cast<uint16_t>(cmo_task_def.num_outer());
  cmo_task_info_.lengthInner = cmo_task_def.length_inner();
  cmo_task_info_.striderOuter = cmo_task_def.strider_outer();
  cmo_task_info_.striderInner = cmo_task_def.strider_inner();

  GE_ASSERT_SUCCESS(
      Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(cmo_task_def.source_addr()), source_addr_, true, 0U));

  GELOGI(
      "CmoTaskCodeBuilder: cmoType[%u], logicId[%u], opCode[%u], qos[%u], partId[%u], pmg[%u], "
      "numInner[%u], numOuter[%u], lengthInner[%u], source_addr[0x%" PRIx64
      "], "
      "striderOuter[%u], striderInner[%u], stream_id[%u]",
      cmo_task_info_.cmoType, cmo_task_info_.logicId, cmo_task_info_.opCode, cmo_task_info_.qos, cmo_task_info_.partId,
      cmo_task_info_.pmg, cmo_task_info_.numInner, cmo_task_info_.numOuter, cmo_task_info_.lengthInner,
      source_addr_.mem_offset, cmo_task_info_.striderOuter, cmo_task_info_.striderInner, header_.stream_id);

  return SUCCESS;
}

Status CmoTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(
      ast_.Comment("============================= " + header_.op_name + " (CMO) ==============================="));

  Arg source_addr_expr = ast_.ULong(0U);
  if (source_addr_.kind == AddrValueKind::kInputInstance || source_addr_.kind == AddrValueKind::kOutputInstance) {
    source_addr_expr = ast_.Call("PtrToValue", {GetAddr(total_dev_mem_ptr_, source_addr_.mem_offset)});
  } else if (source_addr_.kind == AddrValueKind::kConstTensor) {
    source_addr_expr = ast_.Call("PtrToValue", {ast_.Var("auto", source_addr_.symbol_hint)});
  } else {
    GELOGE(FAILED, "[OM2] CMO source addr unsupported kind %d.", static_cast<int32_t>(source_addr_.kind));
    return FAILED;
  }

  auto cmo_info_var = ast_.Var("rtCmoTaskInfo_t", "cmo_info");
  items.push_back(ast_.VarDecl(
      cmo_info_var,
      ast_.InitList({ast_.StaticCast("uint16_t", cmo_task_info_.cmoType),
                     ast_.UInt(static_cast<uint64_t>(cmo_task_info_.logicId)),
                     ast_.StaticCast("uint8_t", cmo_task_info_.qos), ast_.StaticCast("uint8_t", cmo_task_info_.partId),
                     ast_.StaticCast("uint8_t", cmo_task_info_.pmg), ast_.StaticCast("uint16_t", cmo_task_info_.opCode),
                     ast_.StaticCast("uint16_t", cmo_task_info_.numInner),
                     ast_.StaticCast("uint16_t", cmo_task_info_.numOuter),
                     ast_.UInt(static_cast<uint64_t>(cmo_task_info_.lengthInner)), source_addr_expr,
                     ast_.UInt(static_cast<uint64_t>(cmo_task_info_.striderOuter)),
                     ast_.UInt(static_cast<uint64_t>(cmo_task_info_.striderInner))})));

  items.push_back(ChkStatus(ast_.Call(
      "KernelCmoTaskDistribute", {cmo_info_var, stream_list_[static_cast<int32_t>(header_.stream_id)], ast_.UInt(0)})));

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

  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_CMO, CmoTaskCodeBuilder);
}  // namespace ge
