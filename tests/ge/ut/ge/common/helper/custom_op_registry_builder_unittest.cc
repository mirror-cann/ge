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
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "common/helper/custom_op_so_loader.h"
#include "graph/custom_op_registry.h"
#define private public
#include "common/helper/custom_op_registry_builder.h"
#undef private
#include "depends/mmpa/src/mmpa_stub.h"
#include "framework/common/helper/model_helper.h"
#include "framework/common/framework_types_internal.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op_pull_registry.h"
#include "securec.h"

namespace ge {
namespace {
constexpr const char *kSymbolAbiVersion = "GetRegisteredCustomOpCreatorAbiVersion";
constexpr const char *kSymbolCreatorNum = "GetRegisteredCustomOpCreatorNum";
constexpr const char *kSymbolCreators = "GetRegisteredCustomOpCreators";
constexpr const char *kBuilderOpA = "Task4BuilderOpA";
constexpr const char *kBuilderOpB = "Task4BuilderOpB";
constexpr const char *kPartitionOnlyOp = "Task4PartitionOnlyOp";

struct FakeSoCreators {
  uint32_t abi_version = kCustomOpCreatorPullAbiVersion;
  std::vector<CustomOpTypeToCreator> creators;
};

FakeSoCreators g_fake_so_a;
FakeSoCreators g_fake_so_b;

class FakeSoHandleMmpaStub : public MmpaStubApiGe {
 public:
  int32_t DlClose(void *handle) override {
    if ((handle == reinterpret_cast<void *>(0xA001U)) || (handle == reinterpret_cast<void *>(0xB001U))) {
      return 0;
    }
    return MmpaStubApiGe::DlClose(handle);
  }
};

class BuilderTestOpA : public BaseCustomOp {};
class BuilderTestOpB : public BaseCustomOp {};

class BuilderPortableOp : public PortableOp {
 public:
  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer.clear();
    return GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    deserialized_buffer = buffer;
    return GRAPH_SUCCESS;
  }

  std::vector<uint8_t> deserialized_buffer;
};

BaseCustomOp *CreateBuilderOpA() {
  return new BuilderTestOpA();
}

BaseCustomOp *CreateBuilderOpB() {
  return new BuilderTestOpB();
}

BaseCustomOp *CreateBuilderPortableOp() {
  return new BuilderPortableOp();
}

uint32_t GetFakeAbiVersionA() {
  return g_fake_so_a.abi_version;
}

uint32_t GetFakeAbiVersionB() {
  return g_fake_so_b.abi_version;
}

size_t GetFakeCreatorNumA() {
  return g_fake_so_a.creators.size();
}

size_t GetFakeCreatorNumB() {
  return g_fake_so_b.creators.size();
}

int32_t CopyFakeCreators(const FakeSoCreators &fake_so, CustomOpTypeToCreator *creators, const size_t creator_num,
                         const size_t creator_struct_size) {
  if ((creator_num < fake_so.creators.size()) || ((creator_num > 0U) && (creators == nullptr)) ||
      (creator_struct_size < sizeof(CustomOpTypeToCreator))) {
    return -1;
  }
  for (size_t i = 0U; i < fake_so.creators.size(); ++i) {
    auto *creator_addr = reinterpret_cast<uint8_t *>(creators) + (i * creator_struct_size);
    (void)memcpy_s(creator_addr, creator_struct_size, &fake_so.creators[i], sizeof(CustomOpTypeToCreator));
  }
  return 0;
}

int32_t GetFakeCreatorsA(CustomOpTypeToCreator *creators, size_t creator_num, size_t creator_struct_size) {
  return CopyFakeCreators(g_fake_so_a, creators, creator_num, creator_struct_size);
}

int32_t GetFakeCreatorsB(CustomOpTypeToCreator *creators, size_t creator_num, size_t creator_struct_size) {
  return CopyFakeCreators(g_fake_so_b, creators, creator_num, creator_struct_size);
}

void *ResolveFakeSymbols(void *handle, const char *symbol) {
  if ((handle == reinterpret_cast<void *>(0xA001U)) && (std::strcmp(symbol, kSymbolAbiVersion) == 0)) {
    return reinterpret_cast<void *>(&GetFakeAbiVersionA);
  }
  if ((handle == reinterpret_cast<void *>(0xA001U)) && (std::strcmp(symbol, kSymbolCreatorNum) == 0)) {
    return reinterpret_cast<void *>(&GetFakeCreatorNumA);
  }
  if ((handle == reinterpret_cast<void *>(0xA001U)) && (std::strcmp(symbol, kSymbolCreators) == 0)) {
    return reinterpret_cast<void *>(&GetFakeCreatorsA);
  }
  if ((handle == reinterpret_cast<void *>(0xB001U)) && (std::strcmp(symbol, kSymbolAbiVersion) == 0)) {
    return reinterpret_cast<void *>(&GetFakeAbiVersionB);
  }
  if ((handle == reinterpret_cast<void *>(0xB001U)) && (std::strcmp(symbol, kSymbolCreatorNum) == 0)) {
    return reinterpret_cast<void *>(&GetFakeCreatorNumB);
  }
  if ((handle == reinterpret_cast<void *>(0xB001U)) && (std::strcmp(symbol, kSymbolCreators) == 0)) {
    return reinterpret_cast<void *>(&GetFakeCreatorsB);
  }
  return nullptr;
}

void *ResolveMissingCreatorNum(void *handle, const char *symbol) {
  if (std::strcmp(symbol, kSymbolCreatorNum) == 0) {
    return nullptr;
  }
  return ResolveFakeSymbols(handle, symbol);
}

CustomOpSoHandlePtr MakeFakeSoHandle(void *handle, const std::string &name) {
  return std::make_shared<CustomOpSoHandle>(name, handle, name, 0U, -1);
}

CustomOpTypeToCreator MakeCreator(const char *op_type, const CustomOpCreateFunc creator) {
  return CustomOpTypeToCreator{sizeof(CustomOpTypeToCreator), op_type, creator};
}

std::vector<uint8_t> BuildCustomOpPartition(const std::string &name, const std::vector<uint8_t> &bin) {
  CustomKernelItemHeader header{kCustomKernelItemMagic, static_cast<uint32_t>(name.size()),
                                static_cast<uint32_t>(bin.size())};
  std::vector<uint8_t> payload(sizeof(header) + name.size() + bin.size(), 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(header));
  (void)memcpy_s(payload.data() + sizeof(header), payload.size() - sizeof(header), name.data(), name.size());
  if (!bin.empty()) {
    (void)memcpy_s(payload.data() + sizeof(header) + name.size(), payload.size() - sizeof(header) - name.size(),
                   bin.data(), bin.size());
  }
  return payload;
}
}  // namespace

class UtestCustomOpRegistryBuilder : public testing::Test {
 protected:
  void SetUp() override {
    g_fake_so_a = FakeSoCreators{};
    g_fake_so_b = FakeSoCreators{};
    MmpaStub::GetInstance().SetImpl(std::make_shared<FakeSoHandleMmpaStub>());
  }

  void TearDown() override {
    g_fake_so_a = FakeSoCreators{};
    g_fake_so_b = FakeSoCreators{};
    MmpaStub::GetInstance().Reset();
  }
};

TEST_F(UtestCustomOpRegistryBuilder, add_creators_from_so_handles_registers_valid_pull_creators) {
  g_fake_so_a.creators = {MakeCreator(kBuilderOpA, CreateBuilderOpA), MakeCreator(kBuilderOpB, CreateBuilderOpB)};
  auto registry = std::make_shared<CustomOpRegistry>();
  std::vector<CustomOpSoHandlePtr> so_handles = {MakeFakeSoHandle(reinterpret_cast<void *>(0xA001U), "fake_a")};

  EXPECT_EQ(SUCCESS, CustomOpRegistryBuilder::AddCreatorsFromSoHandles(so_handles, registry, ResolveFakeSymbols));

  EXPECT_TRUE(registry->HasCreator(kBuilderOpA));
  EXPECT_TRUE(registry->HasCreator(kBuilderOpB));
  EXPECT_NE(nullptr, dynamic_cast<BuilderTestOpA *>(registry->CreateOrGetCustomOp(kBuilderOpA)));
  EXPECT_NE(nullptr, dynamic_cast<BuilderTestOpB *>(registry->CreateOrGetCustomOp(kBuilderOpB)));
  registry.reset();
  so_handles.clear();
}

TEST_F(UtestCustomOpRegistryBuilder, add_creators_from_so_handles_fails_when_required_symbol_missing) {
  g_fake_so_a.creators = {MakeCreator(kBuilderOpA, CreateBuilderOpA)};
  auto registry = std::make_shared<CustomOpRegistry>();
  std::vector<CustomOpSoHandlePtr> so_handles = {MakeFakeSoHandle(reinterpret_cast<void *>(0xA001U), "fake_a")};

  EXPECT_NE(SUCCESS, CustomOpRegistryBuilder::AddCreatorsFromSoHandles(so_handles, registry, ResolveMissingCreatorNum));
  EXPECT_FALSE(registry->HasCreator(kBuilderOpA));
  so_handles.clear();
}

TEST_F(UtestCustomOpRegistryBuilder, add_creators_from_so_handles_fails_on_abi_version_mismatch) {
  g_fake_so_a.abi_version = kCustomOpCreatorPullAbiVersion + 1U;
  g_fake_so_a.creators = {MakeCreator(kBuilderOpA, CreateBuilderOpA)};
  auto registry = std::make_shared<CustomOpRegistry>();
  std::vector<CustomOpSoHandlePtr> so_handles = {MakeFakeSoHandle(reinterpret_cast<void *>(0xA001U), "fake_a")};

  EXPECT_NE(SUCCESS, CustomOpRegistryBuilder::AddCreatorsFromSoHandles(so_handles, registry, ResolveFakeSymbols));
  EXPECT_FALSE(registry->HasCreator(kBuilderOpA));
  so_handles.clear();
}

TEST_F(UtestCustomOpRegistryBuilder, add_creators_from_so_handles_fails_on_invalid_creator_entry) {
  auto registry = std::make_shared<CustomOpRegistry>();
  const std::vector<CustomOpTypeToCreator> invalid_creators = {
      MakeCreator(nullptr, CreateBuilderOpA), MakeCreator("", CreateBuilderOpA), MakeCreator(kBuilderOpA, nullptr),
      CustomOpTypeToCreator{sizeof(CustomOpTypeToCreator) - 1U, kBuilderOpA, CreateBuilderOpA}};

  for (const auto &invalid_creator : invalid_creators) {
    g_fake_so_a.creators = {invalid_creator};
    std::vector<CustomOpSoHandlePtr> so_handles = {MakeFakeSoHandle(reinterpret_cast<void *>(0xA001U), "fake_a")};
    EXPECT_NE(SUCCESS, CustomOpRegistryBuilder::AddCreatorsFromSoHandles(so_handles, registry, ResolveFakeSymbols));
    EXPECT_FALSE(registry->HasCreator(kBuilderOpA));
    so_handles.clear();
  }
}

TEST_F(UtestCustomOpRegistryBuilder, add_creators_from_so_handles_fails_on_duplicate_op_type_across_so_handles) {
  g_fake_so_a.creators = {MakeCreator(kBuilderOpA, CreateBuilderOpA)};
  g_fake_so_b.creators = {MakeCreator(kBuilderOpA, CreateBuilderOpB)};
  auto registry = std::make_shared<CustomOpRegistry>();
  std::vector<CustomOpSoHandlePtr> so_handles = {MakeFakeSoHandle(reinterpret_cast<void *>(0xA001U), "fake_a"),
                                                 MakeFakeSoHandle(reinterpret_cast<void *>(0xB001U), "fake_b")};

  EXPECT_NE(SUCCESS, CustomOpRegistryBuilder::AddCreatorsFromSoHandles(so_handles, registry, ResolveFakeSymbols));
  EXPECT_FALSE(registry->HasCreator(kBuilderOpA));
  so_handles.clear();
}

TEST_F(UtestCustomOpRegistryBuilder, load_custom_ops_to_registry_uses_given_registry_without_global_pollution) {
  const std::vector<uint8_t> serialized_bin = {0x21U, 0x22U, 0x23U};
  auto registry = std::make_shared<CustomOpRegistry>();
  ASSERT_EQ(GRAPH_SUCCESS, registry->RegisterCreator(kPartitionOnlyOp, []() -> std::unique_ptr<BaseCustomOp> {
    return std::unique_ptr<BaseCustomOp>(CreateBuilderPortableOp());
  }));
  ASSERT_FALSE(CustomOpFactory::IsExistOp(kPartitionOnlyOp));

  const auto payload = BuildCustomOpPartition(kPartitionOnlyOp, serialized_bin);
  EXPECT_EQ(SUCCESS, LoadCustomOpsToRegistry(payload.data(), payload.size(), registry));

  const auto *op = dynamic_cast<BuilderPortableOp *>(registry->FindCustomOp(kPartitionOnlyOp));
  ASSERT_NE(nullptr, op);
  EXPECT_EQ(serialized_bin, op->deserialized_buffer);
  EXPECT_FALSE(CustomOpFactory::IsExistOp(kPartitionOnlyOp));
}
}  // namespace ge
