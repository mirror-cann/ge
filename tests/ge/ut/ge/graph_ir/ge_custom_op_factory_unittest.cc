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
#include <string>
#include <vector>

#include "framework/common/types.h"
#include "../graph/custom_ops_stub.h"
#include "graph/custom_op_factory.h"
#include "ge/ge_api_error_codes.h"
#include "macro_utils/dt_public_scope.h"
#include "macro_utils/dt_public_unscope.h"
#include "securec.h"

using namespace ge;
class UtestCustomOpFactory : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}
};


TEST(UtestCustomOpFactory, create_custom_op) {
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("MyCustomOp"));
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("MyPortableOp"));
  const auto op = CustomOpFactory::CreateCustomOp("MyCustomOp");
  const auto op2 = CustomOpFactory::CreateCustomOp("NonExists");
  EXPECT_EQ(true, op != nullptr);
  EXPECT_EQ(true, op2 == nullptr);
}

TEST(UtestCustomOpFactory, get_all_ops) {
  std::vector<AscendString> all_registered_ops;
  const auto ret = CustomOpFactory::GetAllRegisteredOps(all_registered_ops);
  EXPECT_EQ(ge::SUCCESS, ret);
  const auto has_my_custom_op = std::any_of(all_registered_ops.begin(), all_registered_ops.end(),
      [](const AscendString &op_name) { return op_name == "MyCustomOp"; });
  const auto has_my_serializable_custom_op = std::any_of(all_registered_ops.begin(), all_registered_ops.end(),
      [](const AscendString &op_name) { return op_name == "MyPortableOp"; });
  EXPECT_EQ(true, has_my_custom_op);
  EXPECT_EQ(true, has_my_serializable_custom_op);
}

TEST(UtestCustomOpFactory, register_custom_op_creator) {
  const auto reg_ret = CustomOpFactory::RegisterCustomOpCreator(
      "MyCustomOp3", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MyCustomOp>(); });
  const auto reg_null_ret = CustomOpFactory::RegisterCustomOpCreator("MyCustomOp4", nullptr);
  EXPECT_TRUE((reg_ret == ge::SUCCESS) || (reg_ret == ge::GRAPH_FAILED));
  EXPECT_EQ(ge::GRAPH_PARAM_INVALID, reg_null_ret);
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("MyCustomOp3"));
  EXPECT_EQ(false, CustomOpFactory::IsExistOp("MyCustomOp4"));
}

TEST(UtestCustomOpFactory, creator_register) {
  CustomOpCreatorRegister(
      "MyCustomOp5", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MyCustomOp>(); });
  CustomOpCreatorRegister(
      "MyCustomOp5", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MyCustomOp>(); });
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("MyCustomOp5"));
}

TEST(UtestCustomOpFactory, load_custom_kernels_partition_invalid_data) {
  uint8_t payload[1] = {0U};
  EXPECT_EQ(ge::GRAPH_PARAM_INVALID, CustomOpFactory::LoadCustomOpsPartition(nullptr, sizeof(payload)));
  EXPECT_EQ(ge::GRAPH_PARAM_INVALID, CustomOpFactory::LoadCustomOpsPartition(payload, 0U));
}

TEST(UtestCustomOpFactory, load_custom_kernels_partition_header_too_short_fail) {
  uint8_t payload[2] = {0x1U, 0x2U};
  EXPECT_EQ(ge::GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload, sizeof(payload)));
}

TEST(UtestCustomOpFactory, load_custom_kernels_partition_invalid_magic_fail) {
  const std::string name = "MyPortableOp";
  ge::CustomKernelItemHeader header{0x12345678U, static_cast<uint32_t>(name.size()), 3U};
  std::vector<uint8_t> payload(sizeof(header) + name.size() + 3U, 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(header));
  (void)memcpy_s(payload.data() + sizeof(header), payload.size() - sizeof(header), name.data(), name.size());
  EXPECT_EQ(ge::GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST(UtestCustomOpFactory, load_custom_kernels_partition_unregistered_op_fail) {
  const std::string name = "NoSuchCustomOpX";
  ge::CustomKernelItemHeader header{ge::kCustomKernelItemMagic, static_cast<uint32_t>(name.size()), 1U};
  std::vector<uint8_t> payload(sizeof(header) + name.size() + 1U, 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(header));
  (void)memcpy_s(payload.data() + sizeof(header), payload.size() - sizeof(header), name.data(), name.size());
  payload.back() = 0x1U;
  EXPECT_EQ(ge::GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST(UtestCustomOpFactory, load_custom_kernels_partition_not_portable_op_fail) {
  const std::string name = "MyCustomOp";
  ge::CustomKernelItemHeader header{ge::kCustomKernelItemMagic, static_cast<uint32_t>(name.size()), 1U};
  std::vector<uint8_t> payload(sizeof(header) + name.size() + 1U, 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(header));
  (void)memcpy_s(payload.data() + sizeof(header), payload.size() - sizeof(header), name.data(), name.size());
  payload.back() = 0x1U;
  EXPECT_EQ(ge::GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST(UtestCustomOpFactory, load_custom_kernels_partition_success) {
  const std::string name = "MyPortableOp";
  ge::CustomKernelItemHeader header{ge::kCustomKernelItemMagic, static_cast<uint32_t>(name.size()), 3U};
  std::vector<uint8_t> payload(sizeof(header) + name.size() + 3U, 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(header));
  (void)memcpy_s(payload.data() + sizeof(header), payload.size() - sizeof(header), name.data(), name.size());
  payload[sizeof(header) + name.size() + 0U] = 0x1U;
  payload[sizeof(header) + name.size() + 1U] = 0x2U;
  payload[sizeof(header) + name.size() + 2U] = 0x3U;
  EXPECT_EQ(ge::SUCCESS, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST(UtestCustomOpFactory, load_custom_kernels_partition_deserialize_fail_propagate) {
  const auto reg_ret = CustomOpFactory::RegisterCustomOpCreator(
      "MyPortableOpFailedForPartition", []() -> std::unique_ptr<BaseCustomOp> {
        return std::make_unique<MyPortableOpFailed>();
      });
  EXPECT_TRUE((reg_ret == ge::SUCCESS) || (reg_ret == ge::GRAPH_FAILED));

  const std::string name = "MyPortableOpFailedForPartition";
  ge::CustomKernelItemHeader header{ge::kCustomKernelItemMagic, static_cast<uint32_t>(name.size()), 1U};
  std::vector<uint8_t> payload(sizeof(header) + name.size() + 1U, 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(header));
  (void)memcpy_s(payload.data() + sizeof(header), payload.size() - sizeof(header), name.data(), name.size());
  payload.back() = 0x9U;
  EXPECT_EQ(ge::GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}
