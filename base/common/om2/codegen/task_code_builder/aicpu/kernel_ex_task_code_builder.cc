/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "kernel_ex_task_code_builder.h"
#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {
Status KernelExTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  (void)items;
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  (void)items;
  return SUCCESS;
}

int64_t KernelExTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const auto &kernel_ex_def = task_def.kernel_ex();
  return static_cast<int64_t>(kernel_ex_def.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_KERNEL_EX, KernelExTaskCodeBuilder);
}  // namespace ge
