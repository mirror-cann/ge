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
#include <vector>
#include "common/om2/codegen/task_code_builder/rts/stream_switch_task_code_builder.h"
#include "common/om2/codegen/ast/ast_nodes.h"
#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/ast/ast_context.h"

namespace ge {

class UtestStreamSwitchTaskCodeBuilder : public ::testing::Test {
 protected:
  void SetUp() override {
    builder_ = std::make_unique<StreamSwitchTaskCodeBuilder>(ast_);
  }

  void TearDown() override {
    // 清理测试残留
  }

  AstContext ctx_;
  AstBuildContext ast_{ctx_};
  std::unique_ptr<StreamSwitchTaskCodeBuilder> builder_;
};

// 覆盖 RenderDistHelper 正常执行路径
TEST_F(UtestStreamSwitchTaskCodeBuilder, RenderDistHelperSuccess) {
  std::vector<DeclNode *> items;

  // 1. 调用目标函数
  Status ret = builder_->RenderDistHelper(items);

  // 2. 验证返回值
  EXPECT_EQ(ret, SUCCESS);
  
  // 3. 验证生成了且仅生成了一个函数定义节点
  ASSERT_EQ(items.size(), 1U);
  ASSERT_NE(items[0], nullptr);
  items.clear();
}

}  // namespace ge
