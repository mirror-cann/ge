/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "ffts_plus_task_code_builder.h"
#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {

std::string FftsPlusTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status FftsPlusTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  (void)items;
  return SUCCESS;
}

int64_t FftsPlusTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::FftsPlusTaskDef &ffts_plus_task_def = task_def.ffts_plus_task();
  return static_cast<int64_t>(ffts_plus_task_def.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_FFTS_PLUS, FftsPlusTaskCodeBuilder);
}  // namespace ge
