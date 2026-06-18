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

#include <atomic>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "common/env_path.h"
#include "common/path_utils.h"
#include "depends/ascendcl/src/ascendcl_stub.h"
#include "graph/utils/file_utils.h"
#include "mmpa/mmpa_api.h"
#include "rt_external_mem.h"
#include "file_const_loader.h"
#include "om2_external_weight_manager.h"
#include "om2_file_utils.h"
#include "om2_malloc_helper.h"
#include "om2_thread_pool.h"
#include "om2_var_manager.h"

namespace ge {
namespace {
constexpr size_t kCombinedConstCopyBlockSize = 10485760U;

std::string GetParentDir(const std::string &file_path) {
  size_t slash = file_path.find_last_of('/');
  if (slash == std::string::npos) {
    return "";
  }
  if (slash == 0U) {
    return "/";
  }
  return file_path.substr(0U, slash);
}

void WriteBinaryFile(const std::string &file_path, const std::vector<uint8_t> &content) {
  const auto parent_dir = GetParentDir(file_path);
  ASSERT_EQ(CreateDir(parent_dir), 0);
  std::ofstream ofs(file_path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(ofs.is_open());
  ofs.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size()));
  ASSERT_TRUE(ofs.good());
}

class AclRuntimeStubGuard {
 public:
  explicit AclRuntimeStubGuard(AclRuntimeStub *stub) : stub_(stub) {
    AclRuntimeStub::Install(stub_);
  }

  ~AclRuntimeStubGuard() {
    AclRuntimeStub::UnInstall(stub_);
  }

 private:
  AclRuntimeStub *stub_;
};

class RecordingAclRuntimeStub : public AclRuntimeStub {
 public:
  struct MemcpyRecord {
    size_t dest_max = 0U;
    size_t count = 0U;
    aclrtMemcpyKind kind = ACL_MEMCPY_HOST_TO_DEVICE;
    uint8_t first_byte = 0U;
    uint8_t last_byte = 0U;
  };

  struct MallocRecord {
    std::string api_name;
    size_t size = 0U;
    aclrtMemMallocPolicy policy = ACL_MEM_MALLOC_HUGE_FIRST;
    uint16_t module_id = 0U;
  };

  aclError aclrtMallocWithCfg(void **devPtr, size_t size, aclrtMemMallocPolicy policy,
                              aclrtMallocConfig *cfg) override {
    malloc_records.push_back({"aclrtMallocWithCfg", size, policy, GetModuleId(cfg)});
    return AclRuntimeStub::aclrtMallocWithCfg(devPtr, size, policy, cfg);
  }

  aclError aclrtMallocHostWithCfg(void **hostPtr, size_t size, aclrtMallocConfig *cfg) override {
    malloc_records.push_back({"aclrtMallocHostWithCfg", size, ACL_MEM_MALLOC_HUGE_FIRST, GetModuleId(cfg)});
    return AclRuntimeStub::aclrtMallocHostWithCfg(hostPtr, size, cfg);
  }

  aclError aclrtMallocForTaskScheduler(void **devPtr, size_t size, aclrtMemMallocPolicy policy,
                                       aclrtMallocConfig *cfg) override {
    malloc_records.push_back({"aclrtMallocForTaskScheduler", size, policy, GetModuleId(cfg)});
    return AclRuntimeStub::aclrtMallocForTaskScheduler(devPtr, size, policy, cfg);
  }

  aclError aclrtMemcpy(void *dst, size_t dest_max, const void *src, size_t count, aclrtMemcpyKind kind) override {
    MemcpyRecord record;
    record.dest_max = dest_max;
    record.count = count;
    record.kind = kind;
    if ((src != nullptr) && (count > 0U)) {
      const auto *src_bytes = static_cast<const uint8_t *>(src);
      record.first_byte = src_bytes[0];
      record.last_byte = src_bytes[count - 1U];
    }
    memcpy_records.push_back(record);
    return AclRuntimeStub::aclrtMemcpy(dst, dest_max, src, count, kind);
  }

  std::vector<MemcpyRecord> memcpy_records;
  std::vector<MallocRecord> malloc_records;

 private:
  static uint16_t GetModuleId(const aclrtMallocConfig *cfg) {
    if ((cfg == nullptr) || (cfg->attrs == nullptr) || (cfg->numAttrs == 0U)) {
      return 0U;
    }
    return cfg->attrs[0U].value.moduleId;
  }
};
}  // namespace

class FileConstLoaderUt : public testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = EnvPath().GetOrCreateCaseTmpPath("FileConstLoaderUt");
  }

  void TearDown() override {
    EnvPath().RemoveRfCaseTmpPath("FileConstLoaderUt");
  }

  std::string test_dir_;
};

TEST_F(FileConstLoaderUt, om2_file_utils_real_path_matches_public_impl) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weights_private"});
  ASSERT_EQ(CreateDir(weight_dir), 0);
  EXPECT_EQ(ge::om2::RealPath(weight_dir.c_str()), ge::RealPath(weight_dir.c_str()));
}

TEST_F(FileConstLoaderUt, om2_file_utils_split_file_path_matches_public_impl) {
  const auto file_path = PathUtils::Join({test_dir_, "weight", "fc.bin"});
  std::string public_dir;
  std::string public_name;
  std::string om2_dir;
  std::string om2_name;
  ge::SplitFilePath(file_path, public_dir, public_name);
  ge::om2::SplitFilePath(file_path, om2_dir, om2_name);
  EXPECT_EQ(om2_dir, public_dir);
  EXPECT_EQ(om2_name, public_name);
}

TEST_F(FileConstLoaderUt, om2_thread_pool_executes_all_tasks) {
  ge::om2::ThreadPool pool("om2ut", 2U);
  std::atomic<int32_t> counter = 0;
  auto future0 = pool.commit([&counter]() {
    ++counter;
    return 1;
  });
  auto future1 = pool.commit([&counter]() {
    ++counter;
    return 2;
  });
  EXPECT_EQ(future0.get(), 1);
  EXPECT_EQ(future1.get(), 2);
  EXPECT_EQ(counter.load(), 2);
}

TEST_F(FileConstLoaderUt, om2_thread_pool_zero_size_falls_back_to_one_thread) {
  ge::om2::ThreadPool pool("om2ut", 0U);
  auto future = pool.commit([]() { return 7; });
  EXPECT_EQ(future.get(), 7);
}

TEST_F(FileConstLoaderUt, resolve_weight_dir_prefers_weight_path) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weights"});
  ASSERT_EQ(CreateDir(weight_dir), 0);
  ge::ModelData model_data;
  model_data.weight_path = weight_dir;
  model_data.om_path = PathUtils::Join({test_dir_, "model.om2"});
  std::string resolved_weight_dir;
  ASSERT_EQ(gert::ResolveFileConstWeightDir(model_data, resolved_weight_dir), SUCCESS);
  EXPECT_EQ(resolved_weight_dir, ge::RealPath(weight_dir.c_str()) + "/");
}

TEST_F(FileConstLoaderUt, resolve_weight_dir_from_om_path) {
  const auto om_path = PathUtils::Join({test_dir_, "test.om2"});
  WriteBinaryFile(om_path, {1U, 2U, 3U, 4U});
  ge::ModelData model_data;
  model_data.om_path = om_path;
  std::string resolved_weight_dir;
  ASSERT_EQ(gert::ResolveFileConstWeightDir(model_data, resolved_weight_dir), SUCCESS);
  EXPECT_EQ(resolved_weight_dir, test_dir_ + "/weight/");
}

TEST_F(FileConstLoaderUt, build_user_map_and_prepare_individual_from_user_mem_ok) {
  std::vector<uint8_t> user_mem{7U, 8U, 9U, 10U};
  std::vector<ge::FileConstantMem> mems{{"fc.bin", user_mem.data(), user_mem.size()}};
  std::map<std::string, ge::FileConstantMem> file_name_to_mem;
  ASSERT_EQ(gert::BuildUserFileConstMemMap(mems, file_name_to_mem), SUCCESS);
  gert::Om2ConstItem const_item;
  const_item.type = "INDIVIDUAL";
  const_item.file_name = "fc.bin";
  const_item.offset = 1U;
  const_item.size = 2U;
  gert::FileConstContext ctx;
  ctx.user_file_const_mems = &file_name_to_mem;
  void *const_addr = nullptr;
  ASSERT_EQ(gert::PrepareIndividualConst(const_item, ctx, const_addr), SUCCESS);
  ASSERT_NE(const_addr, nullptr);
  EXPECT_EQ(*(static_cast<uint8_t *>(const_addr)), 8U);
}

TEST_F(FileConstLoaderUt, prepare_individual_consts_from_user_mem_ok) {
  std::vector<uint8_t> user_mem{7U, 8U, 9U, 10U};
  std::vector<ge::FileConstantMem> mems{{"fc.bin", user_mem.data(), user_mem.size()}};
  std::map<std::string, ge::FileConstantMem> file_name_to_mem;
  ASSERT_EQ(gert::BuildUserFileConstMemMap(mems, file_name_to_mem), SUCCESS);
  gert::Om2ConstItem const_item;
  const_item.index = 0U;
  const_item.type = "INDIVIDUAL";
  const_item.file_name = "fc.bin";
  const_item.offset = 1U;
  const_item.size = 2U;
  gert::FileConstContext ctx;
  ctx.user_file_const_mems = &file_name_to_mem;
  std::vector<void *> constants(1U, nullptr);
  ASSERT_EQ(gert::PrepareIndividualConsts({const_item}, ctx, 0, constants), SUCCESS);
  ASSERT_NE(constants[0], nullptr);
  EXPECT_EQ(*(static_cast<uint8_t *>(constants[0])), 8U);
}

TEST_F(FileConstLoaderUt, prepare_individual_from_file_ok) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weight"});
  const auto file_path = PathUtils::Join({weight_dir, "fc.bin"});
  WriteBinaryFile(file_path, {11U, 12U, 13U, 14U});
  gert::Om2ConstItem const_item;
  const_item.type = "INDIVIDUAL";
  const_item.file_name = "fc.bin";
  const_item.offset = 1U;
  const_item.size = 2U;
  std::vector<void *> owned_buffers;
  gert::FileConstContext ctx;
  ctx.weight_dir = weight_dir + "/";
  ctx.owned_buffers = &owned_buffers;
  void *const_addr = nullptr;
  ASSERT_EQ(gert::PrepareIndividualConst(const_item, ctx, const_addr), SUCCESS);
  ASSERT_NE(const_addr, nullptr);
  ASSERT_EQ(owned_buffers.size(), 1U);
  EXPECT_EQ(*(static_cast<uint8_t *>(const_addr)), 12U);
  (void)aclrtFree(owned_buffers[0]);
}

TEST_F(FileConstLoaderUt, prepare_individual_consts_reuses_same_file_and_offset_only) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weight"});
  const auto file_path = PathUtils::Join({weight_dir, "fc.bin"});
  WriteBinaryFile(file_path, {101U, 102U, 103U, 104U, 105U});

  gert::Om2ConstItem const_item0;
  const_item0.index = 0U;
  const_item0.type = "INDIVIDUAL";
  const_item0.file_name = "fc.bin";
  const_item0.offset = 1U;
  const_item0.size = 2U;

  gert::Om2ConstItem const_item1 = const_item0;
  const_item1.index = 1U;

  gert::Om2ConstItem const_item2 = const_item0;
  const_item2.index = 2U;
  const_item2.offset = 2U;

  std::vector<void *> owned_buffers;
  gert::FileConstContext ctx;
  ctx.weight_dir = weight_dir + "/";
  ctx.owned_buffers = &owned_buffers;
  ctx.session_id = 1001U;
  ctx.device_id = 0;

  std::vector<void *> constants(3U, nullptr);
  ASSERT_EQ(gert::PrepareIndividualConsts({const_item0, const_item1, const_item2}, ctx, 0, constants), SUCCESS);
  ASSERT_NE(constants[0], nullptr);
  ASSERT_NE(constants[2], nullptr);
  EXPECT_EQ(constants[0], constants[1]);
  EXPECT_NE(constants[0], constants[2]);
  EXPECT_EQ(*(static_cast<uint8_t *>(constants[0])), 102U);
  EXPECT_EQ(*(static_cast<uint8_t *>(constants[2])), 103U);

  EXPECT_TRUE(owned_buffers.empty());
  for (auto *buffer : owned_buffers) {
    (void)aclrtFree(buffer);
  }
  gert::Om2VarManagerPool::Instance().RemoveManager(ctx.session_id);
  gert::Om2ExternalWeightManagerPool::Instance().RemoveManager(ctx.session_id);
}

TEST_F(FileConstLoaderUt, prepare_individual_consts_rejects_same_key_with_different_size) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weight"});
  const auto file_path = PathUtils::Join({weight_dir, "fc.bin"});
  WriteBinaryFile(file_path, {101U, 102U, 103U, 104U, 105U});

  gert::Om2ConstItem const_item0;
  const_item0.index = 0U;
  const_item0.type = "INDIVIDUAL";
  const_item0.file_name = "fc.bin";
  const_item0.offset = 1U;
  const_item0.size = 2U;

  gert::Om2ConstItem const_item1 = const_item0;
  const_item1.index = 1U;
  const_item1.size = 3U;

  std::vector<void *> owned_buffers;
  gert::FileConstContext ctx;
  ctx.weight_dir = weight_dir + "/";
  ctx.owned_buffers = &owned_buffers;
  ctx.session_id = 1002U;
  ctx.device_id = 0;

  std::vector<void *> constants(2U, nullptr);
  EXPECT_NE(gert::PrepareIndividualConsts({const_item0, const_item1}, ctx, 0, constants), SUCCESS);
  for (auto *buffer : owned_buffers) {
    (void)aclrtFree(buffer);
  }
  gert::Om2VarManagerPool::Instance().RemoveManager(ctx.session_id);
  gert::Om2ExternalWeightManagerPool::Instance().RemoveManager(ctx.session_id);
}

TEST_F(FileConstLoaderUt, prepare_individual_consts_supports_zero_size_file_const) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weight"});
  const auto file_path = PathUtils::Join({weight_dir, "fc.bin"});
  WriteBinaryFile(file_path, {101U, 102U, 103U});

  gert::Om2ConstItem const_item;
  const_item.index = 0U;
  const_item.type = "INDIVIDUAL";
  const_item.file_name = "fc.bin";
  const_item.offset = 1U;
  const_item.size = 0U;

  std::vector<void *> owned_buffers;
  gert::FileConstContext ctx;
  ctx.weight_dir = weight_dir + "/";
  ctx.owned_buffers = &owned_buffers;
  ctx.session_id = 1003U;
  ctx.device_id = 0;

  std::vector<void *> constants(1U, reinterpret_cast<void *>(0x1UL));
  EXPECT_EQ(gert::PrepareIndividualConsts({const_item}, ctx, 0, constants), SUCCESS);
  EXPECT_EQ(constants[0], nullptr);
  EXPECT_TRUE(owned_buffers.empty());
  for (auto *buffer : owned_buffers) {
    (void)aclrtFree(buffer);
  }
  gert::Om2VarManagerPool::Instance().RemoveManager(ctx.session_id);
  gert::Om2ExternalWeightManagerPool::Instance().RemoveManager(ctx.session_id);
}

TEST_F(FileConstLoaderUt, prepare_combined_from_user_mem_ok) {
  std::vector<uint8_t> user_mem{17U, 18U, 19U, 20U};
  std::vector<ge::FileConstantMem> mems{{"combined.bin", user_mem.data(), user_mem.size()}};
  std::map<std::string, ge::FileConstantMem> file_name_to_mem;
  ASSERT_EQ(gert::BuildUserFileConstMemMap(mems, file_name_to_mem), SUCCESS);
  gert::Om2ConstItem const_item;
  const_item.type = "COMBINED";
  const_item.file_name = "combined.bin";
  const_item.offset = 1U;
  const_item.size = 2U;
  gert::FileConstContext ctx;
  ctx.user_file_const_mems = &file_name_to_mem;
  std::vector<void *> constants(1U, nullptr);
  ASSERT_EQ(gert::PrepareCombinedConsts({const_item}, ctx, constants), SUCCESS);
  ASSERT_NE(constants[0], nullptr);
  EXPECT_EQ(*(static_cast<uint8_t *>(constants[0])), 18U);
}

TEST_F(FileConstLoaderUt, prepare_combined_from_file_loads_once_and_maps_offsets_ok) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weight"});
  const auto file_path = PathUtils::Join({weight_dir, "combined.bin"});
  WriteBinaryFile(file_path, {21U, 22U, 23U, 24U});
  std::vector<void *> owned_buffers;
  gert::FileConstContext ctx;
  ctx.weight_dir = weight_dir + "/";
  ctx.owned_buffers = &owned_buffers;

  gert::Om2ConstItem const_item0;
  const_item0.index = 0U;
  const_item0.type = "COMBINED";
  const_item0.file_name = "combined.bin";
  const_item0.offset = 0U;
  const_item0.size = 2U;

  gert::Om2ConstItem const_item1;
  const_item1.index = 1U;
  const_item1.type = "COMBINED";
  const_item1.file_name = "combined.bin";
  const_item1.offset = 1U;
  const_item1.size = 2U;

  std::vector<void *> constants(2U, nullptr);
  ASSERT_EQ(gert::PrepareCombinedConsts({const_item0, const_item1}, ctx, constants), SUCCESS);
  ASSERT_NE(constants[0], nullptr);
  ASSERT_NE(constants[1], nullptr);
  ASSERT_EQ(owned_buffers.size(), 1U);
  EXPECT_EQ(*(static_cast<uint8_t *>(constants[0])), 21U);
  EXPECT_EQ(*(static_cast<uint8_t *>(constants[1])), 22U);
  EXPECT_EQ(static_cast<uint8_t *>(constants[1]) - static_cast<uint8_t *>(constants[0]), 1);
  (void)aclrtFree(owned_buffers[0]);
}

TEST_F(FileConstLoaderUt, prepare_combined_from_large_file_copies_by_block_and_maps_offsets_ok) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weight"});
  const auto file_path = PathUtils::Join({weight_dir, "large_combined.bin"});
  const size_t tail_size = 17U;
  std::vector<uint8_t> content(kCombinedConstCopyBlockSize + tail_size);
  for (size_t i = 0U; i < content.size(); ++i) {
    content[i] = static_cast<uint8_t>((i * 37U + 11U) & 0xFFU);
  }
  WriteBinaryFile(file_path, content);

  RecordingAclRuntimeStub runtime_stub;
  AclRuntimeStubGuard runtime_stub_guard(&runtime_stub);
  std::vector<void *> owned_buffers;
  gert::FileConstContext ctx;
  ctx.weight_dir = weight_dir + "/";
  ctx.owned_buffers = &owned_buffers;

  gert::Om2ConstItem const_item0;
  const_item0.index = 0U;
  const_item0.type = "COMBINED";
  const_item0.file_name = "large_combined.bin";
  const_item0.offset = kCombinedConstCopyBlockSize - 1U;
  const_item0.size = 1U;

  gert::Om2ConstItem const_item1 = const_item0;
  const_item1.index = 1U;
  const_item1.offset = kCombinedConstCopyBlockSize;

  std::vector<void *> constants(2U, nullptr);
  ASSERT_EQ(gert::PrepareCombinedConsts({const_item0, const_item1}, ctx, constants), SUCCESS);
  ASSERT_EQ(owned_buffers.size(), 1U);
  ASSERT_NE(constants[0], nullptr);
  ASSERT_NE(constants[1], nullptr);
  EXPECT_EQ(*(static_cast<uint8_t *>(constants[0])), content[kCombinedConstCopyBlockSize - 1U]);
  EXPECT_EQ(*(static_cast<uint8_t *>(constants[1])), content[kCombinedConstCopyBlockSize]);
  EXPECT_EQ(static_cast<uint8_t *>(constants[1]) - static_cast<uint8_t *>(constants[0]), 1);

  ASSERT_EQ(runtime_stub.memcpy_records.size(), 2U);
  EXPECT_EQ(runtime_stub.memcpy_records[0U].count, kCombinedConstCopyBlockSize);
  EXPECT_EQ(runtime_stub.memcpy_records[0U].dest_max, content.size());
  EXPECT_EQ(runtime_stub.memcpy_records[0U].kind, ACL_MEMCPY_HOST_TO_DEVICE);
  EXPECT_EQ(runtime_stub.memcpy_records[0U].first_byte, content.front());
  EXPECT_EQ(runtime_stub.memcpy_records[0U].last_byte, content[kCombinedConstCopyBlockSize - 1U]);
  EXPECT_EQ(runtime_stub.memcpy_records[1U].count, tail_size);
  EXPECT_EQ(runtime_stub.memcpy_records[1U].dest_max, tail_size);
  EXPECT_EQ(runtime_stub.memcpy_records[1U].kind, ACL_MEMCPY_HOST_TO_DEVICE);
  EXPECT_EQ(runtime_stub.memcpy_records[1U].first_byte, content[kCombinedConstCopyBlockSize]);
  EXPECT_EQ(runtime_stub.memcpy_records[1U].last_byte, content.back());
  (void)aclrtFree(owned_buffers[0]);
}

TEST_F(FileConstLoaderUt, om2_malloc_dispatches_to_expected_acl_allocator) {
  struct MallocCase {
    uint32_t mem_type;
    std::string api_name;
    aclrtMemMallocPolicy policy;
    bool check_policy;
    bool host_mem;
  };
  const uint16_t module_id = 19U;
  const size_t alloc_size = 64U;
  const std::vector<MallocCase> cases = {
      {RT_MEMORY_HBM, "aclrtMallocWithCfg", ACL_MEM_TYPE_HIGH_BAND_WIDTH, true, false},
      {RT_MEMORY_DDR, "aclrtMallocWithCfg", ACL_MEM_TYPE_LOW_BAND_WIDTH, true, false},
      {RT_MEMORY_P2P_HBM, "aclrtMallocWithCfg", ACL_MEM_MALLOC_HUGE_FIRST_P2P, true, false},
      {RT_MEMORY_TS, "aclrtMallocForTaskScheduler", ACL_MEM_MALLOC_HUGE_FIRST, true, false},
      {RT_MEMORY_HOST, "aclrtMallocHostWithCfg", ACL_MEM_MALLOC_HUGE_FIRST, false, true},
      {0xFEEDU, "aclrtMallocWithCfg", ACL_MEM_TYPE_HIGH_BAND_WIDTH, true, false},
  };

  RecordingAclRuntimeStub runtime_stub;
  AclRuntimeStubGuard runtime_stub_guard(&runtime_stub);
  for (const auto &test_case : cases) {
    void *ptr = nullptr;
    ASSERT_EQ(gert::Om2Malloc(&ptr, alloc_size, test_case.mem_type, module_id), ACL_SUCCESS);
    ASSERT_NE(ptr, nullptr);
    ASSERT_EQ(runtime_stub.malloc_records.size(), 1U);
    const auto &record = runtime_stub.malloc_records.back();
    EXPECT_EQ(record.api_name, test_case.api_name);
    EXPECT_EQ(record.size, alloc_size);
    EXPECT_EQ(record.module_id, module_id);
    if (test_case.check_policy) {
      EXPECT_EQ(record.policy, test_case.policy);
    }
    if (test_case.host_mem) {
      (void)aclrtFreeHost(ptr);
    } else {
      (void)aclrtFree(ptr);
    }
    runtime_stub.malloc_records.clear();
  }
}

TEST_F(FileConstLoaderUt, om2_malloc_checks_ptr_and_size) {
  RecordingAclRuntimeStub runtime_stub;
  AclRuntimeStubGuard runtime_stub_guard(&runtime_stub);
  EXPECT_EQ(gert::Om2Malloc(nullptr, 64U, RT_MEMORY_HBM, 0U), ACL_ERROR_RT_PARAM_INVALID);
  EXPECT_TRUE(runtime_stub.malloc_records.empty());

  void *ptr = reinterpret_cast<void *>(0x1UL);
  EXPECT_EQ(gert::Om2Malloc(&ptr, 0U, RT_MEMORY_HBM, 0U), ACL_SUCCESS);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_TRUE(runtime_stub.malloc_records.empty());
}

TEST_F(FileConstLoaderUt, prepare_combined_consts_rejects_different_file_names) {
  const auto weight_dir = PathUtils::Join({test_dir_, "weight"});
  WriteBinaryFile(PathUtils::Join({weight_dir, "combined0.bin"}), {31U, 32U, 33U, 34U});
  WriteBinaryFile(PathUtils::Join({weight_dir, "combined1.bin"}), {41U, 42U, 43U, 44U});

  gert::Om2ConstItem const_item0;
  const_item0.index = 0U;
  const_item0.type = "COMBINED";
  const_item0.file_name = "combined0.bin";
  const_item0.offset = 0U;
  const_item0.size = 2U;

  gert::Om2ConstItem const_item1;
  const_item1.index = 1U;
  const_item1.type = "COMBINED";
  const_item1.file_name = "combined1.bin";
  const_item1.offset = 0U;
  const_item1.size = 2U;

  std::vector<void *> constants(2U, nullptr);
  std::vector<void *> owned_buffers;
  gert::FileConstContext ctx;
  ctx.weight_dir = weight_dir + "/";
  ctx.owned_buffers = &owned_buffers;
  EXPECT_NE(gert::PrepareCombinedConsts({const_item0, const_item1}, ctx, constants), SUCCESS);
  EXPECT_TRUE(owned_buffers.empty());
}
}  // namespace ge
