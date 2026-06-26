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

#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "graph/custom_op.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op_load_context.h"
#include "graph/custom_op_pull_registry.h"

using namespace ge;
namespace {
class PullRegistryTestOp : public BaseCustomOp {};
class PullRegistryCreatorOp : public BaseCustomOp {};
class PullRegistryMacroOp : public BaseCustomOp {};

BaseCustomOp *CreatePullRegistryCreatorOp() {
  return new PullRegistryCreatorOp();
}

bool HasRegisteredCreator(const char *op_type) {
  std::vector<CustomOpTypeToCreator> creators(GetRegisteredCustomOpCreatorNum());
  if (!creators.empty()) {
    const auto ret = GetRegisteredCustomOpCreators(creators.data(), creators.size(), sizeof(CustomOpTypeToCreator));
    EXPECT_EQ(0, ret);
  }
  return std::any_of(creators.begin(), creators.end(), [op_type](const CustomOpTypeToCreator &creator) {
    return (creator.op_type != nullptr) && (std::string(creator.op_type) == op_type) && (creator.creator != nullptr);
  });
}
}  // namespace

REG_AUTO_MAPPING_OP(PullRegistryMacroOp);

TEST(UtestCustomOpPullRegistry, creator_register_enters_global_registry_in_normal_mode) {
  CustomOpCreatorRegister("PullRegistryNormalGlobalOp",
                          []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<PullRegistryTestOp>(); });

  EXPECT_EQ(true, CustomOpFactory::IsExistOp("PullRegistryNormalGlobalOp"));
  EXPECT_NE(nullptr, CustomOpFactory::CreateOrGetCustomOp("PullRegistryNormalGlobalOp"));
}

TEST(UtestCustomOpPullRegistry, offline_guard_suppresses_global_registry_push) {
  {
    ScopedOfflineCustomOpSoLoadGuard guard;
    CustomOpCreatorRegister("PullRegistryOfflineSuppressedOp",
                            []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<PullRegistryTestOp>(); });
  }

  EXPECT_EQ(false, CustomOpFactory::IsExistOp("PullRegistryOfflineSuppressedOp"));
  EXPECT_EQ(nullptr, CustomOpFactory::CreateOrGetCustomOp("PullRegistryOfflineSuppressedOp"));
}

TEST(UtestCustomOpPullRegistry, nested_offline_guards_keep_suppress_until_outer_guard_exits) {
  EXPECT_EQ(false, IsOfflineCustomOpSoLoading());
  {
    ScopedOfflineCustomOpSoLoadGuard outer_guard;
    EXPECT_EQ(true, IsOfflineCustomOpSoLoading());
    {
      ScopedOfflineCustomOpSoLoadGuard inner_guard;
      EXPECT_EQ(true, IsOfflineCustomOpSoLoading());
    }
    EXPECT_EQ(true, IsOfflineCustomOpSoLoading());
  }
  EXPECT_EQ(false, IsOfflineCustomOpSoLoading());
}

TEST(UtestCustomOpPullRegistry, offline_guard_is_thread_local) {
  ScopedOfflineCustomOpSoLoadGuard guard;
  bool is_loading_in_another_thread = true;
  std::thread query_thread(
      [&is_loading_in_another_thread]() { is_loading_in_another_thread = IsOfflineCustomOpSoLoading(); });
  query_thread.join();

  EXPECT_EQ(true, IsOfflineCustomOpSoLoading());
  EXPECT_EQ(false, is_loading_in_another_thread);
}

TEST(UtestCustomOpPullRegistry, local_pull_registration_is_returned_by_c_abi) {
  const auto before_num = GetRegisteredCustomOpCreatorNum();
  CustomOpCreatorRegister register_creator("PullRegistryLocalOp", CreatePullRegistryCreatorOp);

  EXPECT_EQ(kCustomOpCreatorPullAbiVersion, GetRegisteredCustomOpCreatorAbiVersion());
  EXPECT_EQ(before_num + 1U, GetRegisteredCustomOpCreatorNum());
  EXPECT_EQ(true, HasRegisteredCreator("PullRegistryLocalOp"));
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("PullRegistryLocalOp"));
}

TEST(UtestCustomOpPullRegistry, offline_guard_does_not_suppress_local_pull_registration) {
  const auto before_num = GetRegisteredCustomOpCreatorNum();
  {
    ScopedOfflineCustomOpSoLoadGuard guard;
    CustomOpCreatorRegister register_creator("PullRegistryOfflineLocalOp", CreatePullRegistryCreatorOp);
    EXPECT_EQ(before_num + 1U, GetRegisteredCustomOpCreatorNum());
    EXPECT_EQ(true, HasRegisteredCreator("PullRegistryOfflineLocalOp"));
  }
  EXPECT_EQ(false, CustomOpFactory::IsExistOp("PullRegistryOfflineLocalOp"));
}

TEST(UtestCustomOpPullRegistry, pull_creator_can_create_instance) {
  CustomOpCreatorRegister register_creator("PullRegistryCreatesInstanceOp", CreatePullRegistryCreatorOp);
  std::vector<CustomOpTypeToCreator> creators(GetRegisteredCustomOpCreatorNum());
  ASSERT_EQ(0, GetRegisteredCustomOpCreators(creators.data(), creators.size(), sizeof(CustomOpTypeToCreator)));
  const auto iter = std::find_if(creators.begin(), creators.end(), [](const CustomOpTypeToCreator &creator) {
    return (creator.op_type != nullptr) && (std::string(creator.op_type) == "PullRegistryCreatesInstanceOp");
  });
  ASSERT_NE(creators.end(), iter);
  ASSERT_NE(nullptr, iter->creator);
  std::unique_ptr<BaseCustomOp> op(iter->creator());
  EXPECT_NE(nullptr, dynamic_cast<PullRegistryCreatorOp *>(op.get()));
}

TEST(UtestCustomOpPullRegistry, macro_registers_global_and_pull_entries_in_normal_mode) {
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("PullRegistryMacroOp"));
  EXPECT_NE(nullptr, dynamic_cast<PullRegistryMacroOp *>(CustomOpFactory::CreateOrGetCustomOp("PullRegistryMacroOp")));
  EXPECT_EQ(true, HasRegisteredCreator("PullRegistryMacroOp"));
}
