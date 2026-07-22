/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

#include "graph/bin_cache/node_bin_selector_factory.h"
#include "hybrid/common/bin_cache/one_node_single_bin_selector.h"

namespace ge {
class UtestOneNodeSingleBinSelector : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UtestOneNodeSingleBinSelector, do_select_bin_succces) {
  auto selector = NodeBinSelectorFactory::GetInstance().GetNodeBinSelector(fuzz_compile::kOneNodeSingleBinMode);
  std::vector<domi::TaskDef> task_defs;
  EXPECT_NE(selector->SelectBin(nullptr, nullptr, task_defs), nullptr);
  EXPECT_EQ(selector->SelectBin(nullptr, nullptr, task_defs)->GetCacheItemId(), std::numeric_limits<uint64_t>::max());
}

TEST_F(UtestOneNodeSingleBinSelector, get_instance_returns_same_reference) {
  auto &factory1 = NodeBinSelectorFactory::GetInstance();
  auto &factory2 = NodeBinSelectorFactory::GetInstance();
  EXPECT_EQ(&factory1, &factory2);
}

TEST_F(UtestOneNodeSingleBinSelector, get_selector_one_node_multiple_bins_mode) {
  auto selector = NodeBinSelectorFactory::GetInstance().GetNodeBinSelector(fuzz_compile::kOneNodeMultipleBinsMode);
  EXPECT_NE(selector, nullptr);
}

TEST_F(UtestOneNodeSingleBinSelector, get_selector_out_of_range) {
  auto selector = NodeBinSelectorFactory::GetInstance().GetNodeBinSelector(fuzz_compile::kNodeBinModeEnd);
  EXPECT_EQ(selector, nullptr);
}

TEST_F(UtestOneNodeSingleBinSelector, get_selector_invalid_mode) {
  auto selector = NodeBinSelectorFactory::GetInstance().GetNodeBinSelector(
      static_cast<fuzz_compile::NodeBinMode>(fuzz_compile::kNodeBinModeEnd + 1));
  EXPECT_EQ(selector, nullptr);
}

TEST_F(UtestOneNodeSingleBinSelector, get_selector_valid_mode_one_node_single) {
  auto selector = NodeBinSelectorFactory::GetInstance().GetNodeBinSelector(fuzz_compile::kOneNodeSingleBinMode);
  EXPECT_NE(selector, nullptr);
}
}  // namespace ge
