/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_LABEL_GOTO_EX_TASK_CODE_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_LABEL_GOTO_EX_TASK_CODE_BUILDER_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"

namespace ge {
constexpr uint8_t kGotoBranchMax = 1U;
class LabelGotoExTaskCodeBuilder : public TaskCodeBuilder {
 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderInitResource(std::vector<BodyItem> &items) override;
  Status RenderDistribution(std::vector<BodyItem> &items) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;

 private:
  uint32_t label_index_ = 0U;
  uint32_t memory_type_{0U};
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_LABEL_GOTO_EX_TASK_CODE_BUILDER_H_
