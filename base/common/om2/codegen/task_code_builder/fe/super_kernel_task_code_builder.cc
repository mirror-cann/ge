/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "super_kernel_task_code_builder.h"
#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {

std::string SuperKernelV2TaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status SuperKernelV2TaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  (void)items;
  return SUCCESS;
}

int64_t SuperKernelV2TaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::KernelDef &kernel_def = task_def.kernel();
  domi::KernelContext context = kernel_def.context();
  return static_cast<int64_t>(context.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_SUPER_KERNEL, SuperKernelV2TaskCodeBuilder);
}  // namespace ge
