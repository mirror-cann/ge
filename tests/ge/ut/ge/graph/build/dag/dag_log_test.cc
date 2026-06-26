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

#include "graph/build/dag/dag_log.h"

namespace minidag {

class MinidagLogTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

/**
 * 场景 1: 日志宏编译验证
 * 验证 MINIDAG_LOG_xxx 宏可正常编译调用
 */
TEST_F(MinidagLogTest, MacroBasic) {
  MINIDAG_LOG_INFO("Info log test");
  MINIDAG_LOG_WARN("Warn log test");
  MINIDAG_LOG_ERROR("Error log test");
  MINIDAG_LOG_DEBUG("Debug log test - output depends on ASCEND_GLOBAL_LOG_LEVEL");
  SUCCEED();
}

/**
 * 场景 2: 日志宏格式化参数验证
 * 验证宏支持可变参数格式化
 */
TEST_F(MinidagLogTest, MacroFormatArguments) {
  MINIDAG_LOG_INFO("Value: %d, String: %s", 42, "test");
  MINIDAG_LOG_WARN("Float: %.2f, Hex: 0x%x", 3.14, 255);
  MINIDAG_LOG_ERROR("Error code: %d, Message: %s", -1, "failed");
  SUCCEED();
}

/**
 * 场景 3: GetTid 辅助函数验证
 * 验证 GetTid 返回有效线程 ID
 */
TEST_F(MinidagLogTest, GetTidValid) {
  uint64_t tid = GetTid();
  EXPECT_GT(tid, 0U);
}

/**
 * 场景 4: kModuleId 验证
 * 验证模块 ID 为 GE（45）
 */
TEST_F(MinidagLogTest, ModuleIdIsGE) {
  EXPECT_EQ(kModuleId, static_cast<int32_t>(GE));
}

}  // namespace minidag
