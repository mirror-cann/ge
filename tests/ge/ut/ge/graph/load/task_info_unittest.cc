/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/load/model_manager/task_info/task_info.h"
#include <gtest/gtest.h>
#include "graph/custom_op.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op_registry.h"
#include "graph/load/model_manager/davinci_model.h"
#include "graph/load/model_manager/task_info/ge/custom_task_info.h"
namespace ge {
class TaskInfoUT : public testing::Test {};
TEST_F(TaskInfoUT, GetArgsPlacementStr_ReturnUnknown_OutOfRange) {
  ASSERT_STREQ(GetArgsPlacementStr(ArgsPlacement::kEnd), "unknown");
  ASSERT_STREQ(GetArgsPlacementStr(static_cast<ArgsPlacement>(100)), "unknown");
}

class TaskInfoRegistryOnlyCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return SUCCESS;
  }
};

TEST_F(TaskInfoUT, CustomTaskInfoDistributeDoesNotFallbackToGlobalRegistry) {
  const std::string op_type = "GlobalOnlyCustomTaskOp";
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(
                op_type.c_str(),
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TaskInfoRegistryOnlyCustomOp>(); }),
            GRAPH_SUCCESS);
  ASSERT_NE(CustomOpFactory::CreateOrGetCustomOp(op_type.c_str()), nullptr);

  DavinciModel davinci_model(0, nullptr);
  davinci_model.SetCustomOpRegistry(MakeShared<CustomOpRegistry>());
  auto op_desc = MakeShared<OpDesc>("custom_task", op_type);

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &davinci_model;
  task_info.op_desc_ = op_desc;

  EXPECT_NE(task_info.Distribute(), GRAPH_SUCCESS);
}

TEST_F(TaskInfoUT, CustomTaskInfoDistributeUsesGlobalRegistry) {
  const std::string op_type = "OnlineGlobalCustomTaskOp";
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(
                op_type.c_str(),
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TaskInfoRegistryOnlyCustomOp>(); }),
            GRAPH_SUCCESS);
  auto *global_op = CustomOpFactory::CreateOrGetCustomOp(op_type.c_str());
  ASSERT_NE(global_op, nullptr);

  DavinciModel davinci_model(0, nullptr);
  davinci_model.SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  auto op_desc = MakeShared<OpDesc>("custom_task_online", op_type);

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &davinci_model;
  task_info.op_desc_ = op_desc;

  EXPECT_EQ(task_info.Distribute(), GRAPH_SUCCESS);
}
}  // namespace ge
