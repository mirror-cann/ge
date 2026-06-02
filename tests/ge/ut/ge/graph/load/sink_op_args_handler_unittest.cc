/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "graph/load/model_manager/task_info/ge/sink_op_args_handler.h"
#include "graph/load/model_manager/task_info/ge/custom_task_info.h"

namespace ge {

TEST(SinkOpArgsHandlerTest, NullTaskInfoReturnsNullptr) {
  SinkOpArgsHandler handler(nullptr);
  EXPECT_EQ(handler.MallocReadOnlyDevArgs(nullptr, 0), nullptr);
}

TEST(SinkOpArgsHandlerTest, GetKernelArgsWithNullTaskInfoHost) {
  SinkOpArgsHandler handler(nullptr);
  const auto& args = handler.GetKernelArgs(gert::Placement::kPlacementHost);
  EXPECT_EQ(args.size(), 0);
}

TEST(SinkOpArgsHandlerTest, GetKernelArgsWithNullTaskInfoDevice) {
  SinkOpArgsHandler handler(nullptr);
  const auto& args = handler.GetKernelArgs(gert::Placement::kPlacementDevice);
  EXPECT_EQ(args.size(), 0);
}

TEST(SinkOpArgsHandlerTest, GetKernelArgsDelegatesToTaskInfoHost) {
  CustomTaskInfo task_info;
  SinkOpArgsHandler handler(&task_info);
  const auto& args = handler.GetKernelArgs(gert::Placement::kPlacementHost);
  EXPECT_EQ(args.size(), 0);
}

TEST(SinkOpArgsHandlerTest, GetKernelArgsDelegatesToTaskInfoDevice) {
  CustomTaskInfo task_info;
  SinkOpArgsHandler handler(&task_info);
  const auto& args = handler.GetKernelArgs(gert::Placement::kPlacementDevice);
  EXPECT_EQ(args.size(), 0);
}

}  // namespace ge