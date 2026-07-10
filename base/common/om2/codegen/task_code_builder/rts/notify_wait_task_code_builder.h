/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_NOTIFY_WAIT_TASK_CODE_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_NOTIFY_WAIT_TASK_CODE_BUILDER_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"

namespace ge {

struct NotifyBuildData {
  uint32_t notify_id{0U};
  uint32_t stream_id{0U};
};

class NotifyWaitTaskCodeBuilder : public TaskCodeBuilder {
  static constexpr const char *kDispatchFuncName = "DispatchNotifyWait";
  static constexpr OpDispatchType::Value kDispatchType = OpDispatchType::DISPATCH_NOTIFY_WAIT;

 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  std::string GetFuncName() const override;
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;
  Status RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) override;

 private:
  NotifyBuildData build_data_;
};

}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_NOTIFY_WAIT_TASK_CODE_BUILDER_H_
