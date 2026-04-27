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

#include <algorithm>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

#include "framework/common/types.h"
#include "graph/custom_op.h"
#include "graph/custom_op_factory.h"

namespace ge {
namespace {
class TestPortableOpForPartition : public EagerExecuteOp, public PortableOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_SUCCESS;
  }
};

class TestNonPortableOpForPartition : public EagerExecuteOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }
};

class TestPortableDeserializeFailOpForPartition : public EagerExecuteOp, public PortableOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_PARAM_INVALID;
  }
};

std::vector<uint8_t> BuildCustomKernelPartitionWithHeader(const CustomKernelItemHeader &header,
                                                          const std::string &op_type,
                                                          const std::vector<uint8_t> &bin) {
  std::vector<uint8_t> payload(sizeof(CustomKernelItemHeader) + op_type.size() + bin.size(), 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(CustomKernelItemHeader));
  if (!op_type.empty()) {
    (void)memcpy_s(payload.data() + sizeof(CustomKernelItemHeader), payload.size() - sizeof(CustomKernelItemHeader),
                   op_type.data(), op_type.size());
  }
  if (!bin.empty()) {
    (void)memcpy_s(payload.data() + sizeof(CustomKernelItemHeader) + op_type.size(),
                   payload.size() - sizeof(CustomKernelItemHeader) - op_type.size(), bin.data(), bin.size());
  }
  return payload;
}

std::vector<uint8_t> BuildCustomKernelPartition(const std::string &op_type, const std::vector<uint8_t> &bin) {
  CustomKernelItemHeader header{kCustomKernelItemMagic, static_cast<uint32_t>(op_type.size()),
                                static_cast<uint32_t>(bin.size())};
  return BuildCustomKernelPartitionWithHeader(header, op_type, bin);
}
}  // namespace

class TestCustomOpsPartition : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TestCustomOpsPartition, load_custom_kernels_partition_success) {
  const std::string op_type = "StPortableOpForPartition";
  const auto reg_ret = CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestPortableOpForPartition>();
  });
  EXPECT_TRUE((reg_ret == GRAPH_SUCCESS) || (reg_ret == GRAPH_FAILED));

  const std::vector<uint8_t> fake_bin = {0x1U, 0x2U, 0x3U};
  const auto payload = BuildCustomKernelPartition(op_type, fake_bin);
  EXPECT_EQ(GRAPH_SUCCESS, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST_F(TestCustomOpsPartition, load_custom_kernels_partition_unregistered_op_failed) {
  const std::string op_type = "StNoSuchPortableOpForPartition";
  const std::vector<uint8_t> fake_bin = {0x5AU};
  const auto payload = BuildCustomKernelPartition(op_type, fake_bin);

  EXPECT_EQ(GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST_F(TestCustomOpsPartition, register_custom_op_creator_nullptr_failed) {
  EXPECT_EQ(GRAPH_PARAM_INVALID,
            CustomOpFactory::RegisterCustomOpCreator("StNullCreatorForPartition", BaseOpCreator{}));
}

TEST_F(TestCustomOpsPartition, load_custom_kernels_partition_invalid_data_failed) {
  uint8_t payload[1] = {0U};
  EXPECT_EQ(GRAPH_PARAM_INVALID, CustomOpFactory::LoadCustomOpsPartition(nullptr, sizeof(payload)));
  EXPECT_EQ(GRAPH_PARAM_INVALID, CustomOpFactory::LoadCustomOpsPartition(payload, 0U));
}

TEST_F(TestCustomOpsPartition, load_custom_kernels_partition_header_too_short_failed) {
  std::vector<uint8_t> payload(sizeof(CustomKernelItemHeader) - 1U, 0U);
  EXPECT_EQ(GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST_F(TestCustomOpsPartition, load_custom_kernels_partition_invalid_magic_failed) {
  const std::string op_type = "StPortableOpInvalidMagicForPartition";
  const std::vector<uint8_t> fake_bin = {0x1U, 0x2U, 0x3U};
  const CustomKernelItemHeader header{0x12345678U, static_cast<uint32_t>(op_type.size()),
                                      static_cast<uint32_t>(fake_bin.size())};
  const auto payload = BuildCustomKernelPartitionWithHeader(header, op_type, fake_bin);
  EXPECT_EQ(GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST_F(TestCustomOpsPartition, load_custom_kernels_partition_entry_size_overflow_failed) {
  const CustomKernelItemHeader header{kCustomKernelItemMagic, std::numeric_limits<uint32_t>::max(),
                                      std::numeric_limits<uint32_t>::max()};
  std::vector<uint8_t> payload(sizeof(CustomKernelItemHeader), 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(CustomKernelItemHeader));
  EXPECT_EQ(GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST_F(TestCustomOpsPartition, load_custom_kernels_partition_non_portable_op_failed) {
  const std::string op_type = "StNonPortableOpForPartition";
  const auto reg_ret = CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestNonPortableOpForPartition>();
  });
  EXPECT_TRUE((reg_ret == GRAPH_SUCCESS) || (reg_ret == GRAPH_FAILED));

  const std::vector<uint8_t> fake_bin = {0x9U};
  const auto payload = BuildCustomKernelPartition(op_type, fake_bin);
  EXPECT_EQ(GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST_F(TestCustomOpsPartition, load_custom_kernels_partition_deserialize_failed_return_failed_code) {
  const std::string op_type = "StPortableDeserializeFailOpForPartition";
  const auto reg_ret = CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestPortableDeserializeFailOpForPartition>();
  });
  EXPECT_TRUE((reg_ret == GRAPH_SUCCESS) || (reg_ret == GRAPH_FAILED));

  const std::vector<uint8_t> fake_bin = {0xEEU};
  const auto payload = BuildCustomKernelPartition(op_type, fake_bin);
  EXPECT_EQ(GRAPH_PARAM_INVALID, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}
}  // namespace ge
