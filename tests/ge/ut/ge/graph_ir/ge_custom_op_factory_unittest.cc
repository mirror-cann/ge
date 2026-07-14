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
#include <new>
#include <string>
#include <vector>

#include "framework/common/framework_types_internal.h"
#include "../graph/custom_ops_stub.h"
#include "graph/custom_op/cast.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op_registry.h"
#include "runtime/custom_op/python_custom_op_adapter.h"
#include "ge/ge_api_error_codes.h"
#include "macro_utils/dt_public_scope.h"
#include "macro_utils/dt_public_unscope.h"
#include "securec.h"

using namespace ge;
using namespace ge::custom_op;
namespace {
class RegistryTestOp : public BaseCustomOp {};
class RegistryDuplicateReplacementOp : public BaseCustomOp {};
class RegistryDestructCountOp : public BaseCustomOp {
 public:
  explicit RegistryDestructCountOp(size_t *destruct_count) : destruct_count_(destruct_count) {}
  ~RegistryDestructCountOp() override {
    if (destruct_count_ != nullptr) {
      ++(*destruct_count_);
    }
  }

 private:
  size_t *destruct_count_;
};

class CastEagerOnlyOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }
};

class RegistryPortableOp : public PortableOp {
 public:
  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer = {1U, 2U, 3U};
    return GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    deserialized_buffer = buffer;
    return GRAPH_SUCCESS;
  }

  std::vector<uint8_t> deserialized_buffer;
};

class FactoryCallbackPortableOp : public PortableOp {
 public:
  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer.clear();
    return GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    const auto dependency_op = CustomOpFactory::CreateOrGetCustomOp("FactoryCallbackDependencyOp");
    return (dependency_op == nullptr) ? GRAPH_FAILED : GRAPH_SUCCESS;
  }
};

struct MockPythonCustomOpHolder {
  bool executed{false};
};

void *CreateMockPythonCustomOpHolder(const PythonCustomOpDescriptor *desc) {
  if (desc == nullptr) {
    return nullptr;
  }
  return new (std::nothrow) MockPythonCustomOpHolder();
}

void DestroyMockPythonCustomOpHolder(void *holder) {
  delete static_cast<MockPythonCustomOpHolder *>(holder);
}

graphStatus ExecuteMockPythonCustomOp(const void *holder, gert::EagerOpExecutionContext *ctx) {
  (void)ctx;
  auto *mock_holder = const_cast<MockPythonCustomOpHolder *>(static_cast<const MockPythonCustomOpHolder *>(holder));
  if (mock_holder == nullptr) {
    return GRAPH_FAILED;
  }
  mock_holder->executed = true;
  return GRAPH_SUCCESS;
}

std::vector<uint8_t> BuildCustomOpPartition(const std::string &name, const std::vector<uint8_t> &bin) {
  ge::CustomKernelItemHeader header{ge::kCustomKernelItemMagic, static_cast<uint32_t>(name.size()),
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

class UtestCustomOpFactory : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}
};

TEST(UtestCustomOpRegistry, rejects_null_creator) {
  CustomOpRegistry registry;
  EXPECT_EQ(ge::GRAPH_PARAM_INVALID, registry.RegisterCreator("RegistryNullCreator", nullptr));
  EXPECT_EQ(false, registry.HasCreator("RegistryNullCreator"));
}

TEST(UtestCustomOpRegistry, rejects_duplicate_creator) {
  CustomOpRegistry registry;
  const auto creator = []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<RegistryTestOp>(); };
  EXPECT_EQ(ge::GRAPH_SUCCESS, registry.RegisterCreator("RegistryDuplicateCreator", creator));
  EXPECT_EQ(ge::GRAPH_FAILED, registry.RegisterCreator("RegistryDuplicateCreator", creator));
}

TEST(UtestCustomOpRegistry, duplicate_creator_keeps_original_creator) {
  CustomOpRegistry registry;
  EXPECT_EQ(ge::GRAPH_SUCCESS,
            registry.RegisterCreator("RegistryDuplicateKeepsOriginal", []() -> std::unique_ptr<BaseCustomOp> {
              return std::make_unique<RegistryTestOp>();
            }));
  EXPECT_EQ(ge::GRAPH_FAILED,
            registry.RegisterCreator("RegistryDuplicateKeepsOriginal", []() -> std::unique_ptr<BaseCustomOp> {
              return std::make_unique<RegistryDuplicateReplacementOp>();
            }));

  const auto op = registry.CreateOrGetCustomOp("RegistryDuplicateKeepsOriginal");
  EXPECT_NE(nullptr, dynamic_cast<RegistryTestOp *>(op));
  EXPECT_EQ(nullptr, dynamic_cast<RegistryDuplicateReplacementOp *>(op));
}

TEST(UtestCustomOpRegistry, create_or_get_returns_same_instance) {
  CustomOpRegistry registry;
  EXPECT_EQ(ge::GRAPH_SUCCESS, registry.RegisterCreator("RegistryCreateOnce", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<RegistryTestOp>();
  }));

  const auto first = registry.CreateOrGetCustomOp("RegistryCreateOnce");
  const auto second = registry.CreateOrGetCustomOp("RegistryCreateOnce");
  EXPECT_NE(nullptr, first);
  EXPECT_EQ(first, second);
}

TEST(UtestCustomOpRegistry, find_custom_op_does_not_create_implicitly) {
  CustomOpRegistry registry;
  EXPECT_EQ(ge::GRAPH_SUCCESS, registry.RegisterCreator("RegistryFindNoCreate", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<RegistryTestOp>();
  }));

  EXPECT_EQ(nullptr, registry.FindCustomOp("RegistryFindNoCreate"));
  EXPECT_EQ(false, registry.HasCustomOp("RegistryFindNoCreate"));
}

TEST(UtestCustomOpRegistry, find_custom_op_returns_created_instance) {
  CustomOpRegistry registry;
  EXPECT_EQ(ge::GRAPH_SUCCESS,
            registry.RegisterCreator("RegistryFindAfterCreate", []() -> std::unique_ptr<BaseCustomOp> {
              return std::make_unique<RegistryTestOp>();
            }));

  const auto created = registry.CreateOrGetCustomOp("RegistryFindAfterCreate");
  EXPECT_NE(nullptr, created);
  EXPECT_EQ(created, registry.FindCustomOp("RegistryFindAfterCreate"));
  EXPECT_EQ(true, registry.HasCustomOp("RegistryFindAfterCreate"));
}

TEST(UtestCustomOpRegistry, remove_custom_ops_erases_creator_and_created_instance) {
  size_t destruct_count = 0U;
  CustomOpRegistry registry;
  EXPECT_EQ(ge::GRAPH_SUCCESS,
            registry.RegisterCreator("RegistryRemoveCustomOp", [&destruct_count]() -> std::unique_ptr<BaseCustomOp> {
              return std::make_unique<RegistryDestructCountOp>(&destruct_count);
            }));

  EXPECT_EQ(true, registry.HasCreator("RegistryRemoveCustomOp"));
  EXPECT_NE(nullptr, registry.CreateOrGetCustomOp("RegistryRemoveCustomOp"));
  EXPECT_EQ(true, registry.HasCustomOp("RegistryRemoveCustomOp"));

  registry.RemoveCustomOps({AscendString("RegistryRemoveCustomOp")});
  EXPECT_EQ(1U, destruct_count);
  EXPECT_EQ(false, registry.HasCreator("RegistryRemoveCustomOp"));
  EXPECT_EQ(false, registry.HasCustomOp("RegistryRemoveCustomOp"));
  EXPECT_EQ(nullptr, registry.CreateOrGetCustomOp("RegistryRemoveCustomOp"));
}

TEST(UtestCustomOpCast, falls_back_to_dynamic_cast_for_cpp_custom_op) {
  CastEagerOnlyOp op;
  BaseCustomOp *base = &op;

  EXPECT_EQ(static_cast<EagerExecuteOp *>(&op), CustomOpCast<EagerExecuteOp>(base));
  EXPECT_EQ(nullptr, CustomOpCast<CompilableOp>(base));
  EXPECT_EQ(nullptr, CustomOpCast<ShapeInferOp>(base));
}

TEST(UtestCustomOpCast, filters_python_adapter_by_capability) {
  PythonCustomOpDescriptor desc;
  desc.descriptor_key = "python_adapter_eager_only";
  desc.op_type = "PythonAdapterEagerOnly";
  AddCustomOpCapability(desc.capabilities, CustomOpCapability::kEagerExecute);

  PythonCustomOpCallbacks callbacks;
  callbacks.create = CreateMockPythonCustomOpHolder;
  callbacks.destroy = DestroyMockPythonCustomOpHolder;
  callbacks.execute = ExecuteMockPythonCustomOp;

  ASSERT_TRUE(PythonCustomOpRuntimeRegistry::Register(desc, callbacks));
  {
    PythonCustomOpAdapter adapter(desc);
    EXPECT_TRUE(adapter.IsValid());

    BaseCustomOp *base = &adapter;
    EXPECT_NE(nullptr, CustomOpCast<EagerExecuteOp>(base));
    EXPECT_EQ(nullptr, CustomOpCast<CompilableOp>(base));
    EXPECT_EQ(nullptr, CustomOpCast<ShapeInferOp>(base));
    EXPECT_EQ(nullptr, CustomOpCast<PortableOp>(base));
    EXPECT_EQ(nullptr, CustomOpCast<ArgsUpdater>(base));

    EXPECT_EQ(GRAPH_SUCCESS, CustomOpCast<EagerExecuteOp>(base)->Execute(nullptr));
    EXPECT_EQ(GRAPH_FAILED, adapter.Compile(nullptr));
    EXPECT_FALSE(PythonCustomOpRuntimeRegistry::Unregister(desc.descriptor_key));
    PythonCustomOpRuntimeRegistry::Clear();
  }
  EXPECT_FALSE(PythonCustomOpRuntimeRegistry::Unregister(desc.descriptor_key));
}

TEST(UtestCustomOpCast, rejects_unsupported_python_adapter_capability) {
  PythonCustomOpDescriptor desc;
  desc.descriptor_key = "python_adapter_shape_unsupported";
  desc.op_type = "PythonAdapterShapeUnsupported";
  AddCustomOpCapability(desc.capabilities, CustomOpCapability::kShapeInfer);

  PythonCustomOpCallbacks callbacks;
  callbacks.create = CreateMockPythonCustomOpHolder;
  callbacks.destroy = DestroyMockPythonCustomOpHolder;

  EXPECT_FALSE(PythonCustomOpRuntimeRegistry::Register(desc, callbacks));
}

TEST(UtestCustomOpRegistry, load_custom_ops_partition_deserializes_registered_portable_op) {
  CustomOpRegistry registry;
  EXPECT_EQ(ge::GRAPH_SUCCESS,
            registry.RegisterCreator("RegistryPortablePartition", []() -> std::unique_ptr<BaseCustomOp> {
              return std::make_unique<RegistryPortableOp>();
            }));
  const std::vector<uint8_t> kernel_bin = {0x1U, 0x2U, 0x3U};
  const auto payload = BuildCustomOpPartition("RegistryPortablePartition", kernel_bin);

  EXPECT_EQ(ge::GRAPH_SUCCESS, registry.LoadCustomOpsPartition(payload.data(), payload.size()));
  const auto *op = dynamic_cast<RegistryPortableOp *>(registry.FindCustomOp("RegistryPortablePartition"));
  ASSERT_NE(nullptr, op);
  EXPECT_EQ(kernel_bin, op->deserialized_buffer);
}

TEST(UtestCustomOpFactory, facade_registers_and_creates_through_global_registry) {
  const auto ret = CustomOpFactory::RegisterCustomOpCreator(
      "FactoryGlobalRegistryOp", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<RegistryTestOp>(); });
  EXPECT_TRUE((ret == ge::GRAPH_SUCCESS) || (ret == ge::GRAPH_FAILED));

  EXPECT_EQ(true, CustomOpFactory::IsExistOp("FactoryGlobalRegistryOp"));
  EXPECT_NE(nullptr, CustomOpFactory::CreateOrGetCustomOp("FactoryGlobalRegistryOp"));
}

TEST(UtestCustomOpFactory, load_custom_ops_partition_allows_factory_callback_from_deserialize) {
  const auto dependency_ret = CustomOpFactory::RegisterCustomOpCreator(
      "FactoryCallbackDependencyOp",
      []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<RegistryTestOp>(); });
  EXPECT_TRUE((dependency_ret == ge::GRAPH_SUCCESS) || (dependency_ret == ge::GRAPH_FAILED));

  const auto callback_ret = CustomOpFactory::RegisterCustomOpCreator(
      "FactoryCallbackPortableOp",
      []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<FactoryCallbackPortableOp>(); });
  EXPECT_TRUE((callback_ret == ge::GRAPH_SUCCESS) || (callback_ret == ge::GRAPH_FAILED));

  const auto payload = BuildCustomOpPartition("FactoryCallbackPortableOp", {0x1U});
  EXPECT_EQ(ge::GRAPH_SUCCESS, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}

TEST(UtestCustomOpFactory, create_or_get_custom_op) {
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("MyEagerExecuteOp"));
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("MyPortableOp"));
  const auto op = CustomOpFactory::CreateOrGetCustomOp("MyEagerExecuteOp");
  const auto op2 = CustomOpFactory::CreateOrGetCustomOp("NonExists");
  EXPECT_EQ(true, op != nullptr);
  EXPECT_EQ(true, op2 == nullptr);
}

TEST(UtestCustomOpFactory, get_all_ops) {
  std::vector<AscendString> all_registered_ops;
  const auto ret = CustomOpFactory::GetAllRegisteredOps(all_registered_ops);
  EXPECT_EQ(ge::SUCCESS, ret);
  const auto has_my_custom_op = std::any_of(all_registered_ops.begin(), all_registered_ops.end(),
                                            [](const AscendString &op_name) { return op_name == "MyEagerExecuteOp"; });
  const auto has_my_serializable_custom_op =
      std::any_of(all_registered_ops.begin(), all_registered_ops.end(),
                  [](const AscendString &op_name) { return op_name == "MyPortableOp"; });
  EXPECT_EQ(true, has_my_custom_op);
  EXPECT_EQ(true, has_my_serializable_custom_op);
  EXPECT_GE(all_registered_ops.size(), 4U);
}

TEST(UtestCustomOpFactory, register_custom_op_creator) {
  const auto reg_ret = CustomOpFactory::RegisterCustomOpCreator(
      "MyCustomOp3", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MyEagerExecuteOp>(); });
  const auto reg_null_ret = CustomOpFactory::RegisterCustomOpCreator("MyCustomOp4", nullptr);
  EXPECT_TRUE((reg_ret == ge::SUCCESS) || (reg_ret == ge::GRAPH_FAILED));
  EXPECT_EQ(ge::GRAPH_PARAM_INVALID, reg_null_ret);
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("MyCustomOp3"));
  EXPECT_EQ(false, CustomOpFactory::IsExistOp("MyCustomOp4"));
}

TEST(UtestCustomOpFactory, creator_register) {
  CustomOpCreatorRegister("MyCustomOp5", []() -> std::unique_ptr<BaseCustomOp> { return nullptr; });
  CustomOpCreatorRegister("MyCustomOp5", []() -> std::unique_ptr<BaseCustomOp> { return nullptr; });
  EXPECT_EQ(true, CustomOpFactory::IsExistOp("MyCustomOp5"));
  EXPECT_EQ(nullptr, CustomOpFactory::CreateOrGetCustomOp("MyCustomOp5"));
}

TEST(UtestCustomOpFactory, test_compilable_op) {
  const auto my_custom_op_2 = CustomOpFactory::CreateOrGetCustomOp("MyEagerExecuteOp");
  const auto my_custom_op_3 = CustomOpFactory::CreateOrGetCustomOp("MyCompilableOp");
  const auto compilable_op_2 = dynamic_cast<CompilableOp *>(my_custom_op_2);
  const auto compilable_op_3 = dynamic_cast<CompilableOp *>(my_custom_op_3);
  EXPECT_EQ(true, compilable_op_2 == nullptr);
  EXPECT_EQ(true, compilable_op_3 != nullptr);
  compilable_op_3->Compile(nullptr);
  const auto casted_my_custom_op_3 = dynamic_cast<MyCompilableOp *>(my_custom_op_3);
  std::string res;
  casted_my_custom_op_3->GetStubCompiledPath(res);
  EXPECT_EQ("stub_compiled_path", res);
}

TEST(UtestCustomOpFactory, create_or_get_returns_same_instance) {
  const auto base_custom_op = CustomOpFactory::CreateOrGetCustomOp("MyCompilableOp");
  const auto base_custom_op2 = CustomOpFactory::CreateOrGetCustomOp("MyCompilableOp");
  EXPECT_EQ(true, base_custom_op == base_custom_op2);
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
      "MyPortableOpFailedForPartition",
      []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MyPortableOpFailed>(); });
  EXPECT_TRUE((reg_ret == ge::SUCCESS) || (reg_ret == ge::GRAPH_FAILED));

  const std::string name = "MyPortableOpFailedForPartition";
  ge::CustomKernelItemHeader header{ge::kCustomKernelItemMagic, static_cast<uint32_t>(name.size()), 1U};
  std::vector<uint8_t> payload(sizeof(header) + name.size() + 1U, 0U);
  (void)memcpy_s(payload.data(), payload.size(), &header, sizeof(header));
  (void)memcpy_s(payload.data() + sizeof(header), payload.size() - sizeof(header), name.data(), name.size());
  payload.back() = 0x9U;
  EXPECT_EQ(ge::GRAPH_FAILED, CustomOpFactory::LoadCustomOpsPartition(payload.data(), payload.size()));
}
