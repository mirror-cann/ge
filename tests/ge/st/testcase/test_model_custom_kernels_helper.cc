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
#include <cstdint>
#include <cstring>
#include <elf.h>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/model/ge_model.h"
#define private public
#include "common/helper/custom_op_so_loader.h"
#include "common/model/ge_root_model.h"
#undef private
#include "common/plugin/plugin_manager.h"
#include "framework/common/helper/model_helper.h"
#include "framework/common/helper/om_file_helper.h"
#include "framework/common/framework_types_internal.h"
#include "graph/ge_local_context.h"
#include "graph/custom_op.h"
#include "graph/custom_op_factory.h"
#include "mmpa/mmpa_api.h"

namespace ge {
namespace {
constexpr const char *kCollectRootType = "StModelHelperCollectRootPortableOp";
constexpr const char *kCollectSubType = "StModelHelperCollectSubPortableOp";
constexpr const char *kSerializeNonPortableType = "StModelHelperSerializeNonPortableOp";
constexpr const char *kSerializeFailType = "StModelHelperSerializeFailPortableOp";
constexpr const char *kSerializeEmptyType = "StModelHelperSerializeEmptyPortableOp";
constexpr const char *kSerializeSuccessType = "StModelHelperSerializeSuccessPortableOp";
constexpr const char *kSaveEmptyRootType = "StModelHelperSaveEmptyRootPortableOp";
constexpr const char *kSaveEmptySubType = "StModelHelperSaveEmptySubPortableOp";
constexpr const char *kSaveSuccessRootType = "StModelHelperSaveSuccessRootPortableOp";
constexpr const char *kSaveSuccessSubType = "StModelHelperSaveSuccessSubPortableOp";
constexpr const char *kLoadSuccessType = "StModelHelperLoadSuccessPortableOp";

const std::vector<uint8_t> kPortableKernelBin = {0x11U, 0x22U, 0x33U};
size_t g_deserialize_called_count = 0U;
size_t g_last_deserialize_bin_size = 0U;

class TestableModelHelper : public ModelHelper {
 public:
  using ModelHelper::CollectUsedCustomOpTypes;
  using ModelHelper::LoadCustomOps;
  using ModelHelper::SaveCustomOpsPartition;
  using ModelHelper::SerializeCustomOpKernel;
};

class PortableOpForSerializeSuccess : public EagerExecuteOp, public PortableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }

  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer = kPortableKernelBin;
    return GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return GRAPH_SUCCESS;
  }
};

class PortableOpForSerializeEmpty : public EagerExecuteOp, public PortableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }

  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer.clear();
    return GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return GRAPH_SUCCESS;
  }
};

class PortableOpForSerializeFail : public EagerExecuteOp, public PortableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }

  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return GRAPH_FAILED;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return GRAPH_SUCCESS;
  }
};

class NonPortableCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }
};

class PortableOpForDeserializeRecord : public EagerExecuteOp, public PortableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }

  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer = kPortableKernelBin;
    return GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    g_deserialize_called_count++;
    g_last_deserialize_bin_size = buffer.size();
    return GRAPH_SUCCESS;
  }
};

static void RegisterCustomOpCreatorForSt(const std::string &op_type, const BaseOpCreator &creator) {
  const auto ret = CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), creator);
  EXPECT_TRUE((ret == GRAPH_SUCCESS) || (ret == GRAPH_FAILED));
}

static NodePtr AddNodeWithType(const ComputeGraphPtr &graph, const std::string &node_name, const std::string &op_type) {
  auto op_desc = std::make_shared<OpDesc>(node_name, op_type);
  if (op_desc == nullptr) {
    return nullptr;
  }
  (void)op_desc->AddDynamicInputDesc("x", 1);
  (void)op_desc->AddDynamicOutputDesc("y", 1);
  return graph->AddNode(op_desc);
}

static GeRootModelPtr CreateRootModelForCustomOps(const std::string &root_op_type,
                                                       const std::string &sub_op_type,
                                                       const bool with_null_sub_model,
                                                       const bool with_null_sub_graph) {
  auto root_graph = std::make_shared<ComputeGraph>("st_root_graph_for_custom_kernels_helper");
  if (!root_op_type.empty()) {
    (void)AddNodeWithType(root_graph, "root_custom_node", root_op_type);
  }
  (void)AddNodeWithType(root_graph, "root_builtin_node", "Data");

  auto ge_root_model = std::make_shared<GeRootModel>();
  if (ge_root_model->Initialize(root_graph) != SUCCESS) {
    return nullptr;
  }

  auto sub_graph = std::make_shared<ComputeGraph>("st_sub_graph_for_custom_kernels_helper");
  if (!sub_op_type.empty()) {
    (void)AddNodeWithType(sub_graph, "sub_custom_node", sub_op_type);
  }
  auto sub_model = std::make_shared<GeModel>();
  sub_model->SetGraph(sub_graph);
  ge_root_model->SetSubgraphInstanceNameToModel("st_sub_graph_with_custom_node", sub_model);

  if (with_null_sub_model) {
    ge_root_model->SetSubgraphInstanceNameToModel("st_sub_graph_null_model", nullptr);
  }
  if (with_null_sub_graph) {
    auto null_graph_model = std::make_shared<GeModel>();
    ge_root_model->SetSubgraphInstanceNameToModel("st_sub_graph_null_graph", null_graph_model);
  }
  return ge_root_model;
}

static std::vector<uint8_t> BuildCustomKernelPartitionPayload(const std::string &op_type,
                                                              const std::vector<uint8_t> &bin) {
  CustomKernelItemHeader header{kCustomKernelItemMagic, static_cast<uint32_t>(op_type.size()),
                                static_cast<uint32_t>(bin.size())};
  std::vector<uint8_t> payload(sizeof(CustomKernelItemHeader) + op_type.size() + bin.size(), 0U);
  std::copy_n(reinterpret_cast<const uint8_t *>(&header), sizeof(CustomKernelItemHeader), payload.data());
  std::copy_n(reinterpret_cast<const uint8_t *>(op_type.data()), op_type.size(),
              payload.data() + sizeof(CustomKernelItemHeader));
  if (!bin.empty()) {
    std::copy_n(bin.data(), bin.size(), payload.data() + sizeof(CustomKernelItemHeader) + op_type.size());
  }
  return payload;
}

class ScopedEnvVarForSt {
 public:
  explicit ScopedEnvVarForSt(std::string env_name) : env_name_(std::move(env_name)) {
    const char *env_value = std::getenv(env_name_.c_str());
    if (env_value != nullptr) {
      has_old_value_ = true;
      old_value_ = env_value;
    }
  }

  ~ScopedEnvVarForSt() {
    if (has_old_value_) {
      (void)mmSetEnv(env_name_.c_str(), old_value_.c_str(), 1);
      return;
    }
    (void)mmSetEnv(env_name_.c_str(), "", 1);
  }

 private:
  std::string env_name_;
  std::string old_value_;
  bool has_old_value_ = false;
};

class ScopedTmpDirCleanerForSt {
 public:
  explicit ScopedTmpDirCleanerForSt(std::string dir_path) : dir_path_(std::move(dir_path)) {
    Cleanup();
  }

  ~ScopedTmpDirCleanerForSt() {
    Cleanup();
  }

 private:
  void Cleanup() const {
    if (!dir_path_.empty()) {
      (void)system(("rm -rf " + dir_path_).c_str());
    }
  }

  std::string dir_path_;
};

class ScopedHostEnvOptionForSt {
 public:
  ScopedHostEnvOptionForSt(const std::string &host_env_os, const std::string &host_env_cpu) {
    (void)GetThreadLocalContext().GetOption(OPTION_HOST_ENV_OS, old_host_env_os_);
    (void)GetThreadLocalContext().GetOption(OPTION_HOST_ENV_CPU, old_host_env_cpu_);
    std::map<std::string, std::string> options;
    options[OPTION_HOST_ENV_OS] = host_env_os;
    options[OPTION_HOST_ENV_CPU] = host_env_cpu;
    (void)GetThreadLocalContext().SetGlobalOption(options);
  }

  ~ScopedHostEnvOptionForSt() {
    std::map<std::string, std::string> options;
    options[OPTION_HOST_ENV_OS] = old_host_env_os_;
    options[OPTION_HOST_ENV_CPU] = old_host_env_cpu_;
    (void)GetThreadLocalContext().SetGlobalOption(options);
  }

 private:
  std::string old_host_env_os_;
  std::string old_host_env_cpu_;
};

static std::string BuildTmpDirForSt(const std::string &dir_suffix) {
  return "/tmp/st_custom_op_so_loader_ge_root_model_" + std::to_string(static_cast<unsigned int>(getpid())) + "_" +
         dir_suffix;
}

static bool FindReadableSystemSoPathForSt(std::string &so_path) {
  const std::vector<std::string> so_dirs = {
      "/lib/aarch64-linux-gnu", "/usr/lib/aarch64-linux-gnu", "/lib/x86_64-linux-gnu",
      "/usr/lib/x86_64-linux-gnu", "/lib64", "/usr/lib64"};
  const std::vector<std::string> so_names = {"libz.so.1", "liblzma.so.5", "libbz2.so.1.0",
                                             "libuuid.so.1", "libffi.so.8", "libcrypt.so.1"};
  for (const auto &so_dir : so_dirs) {
    for (const auto &so_name : so_names) {
      const std::string candidate = so_dir + "/" + so_name;
      if (mmAccess2(candidate.c_str(), M_R_OK) == EN_OK) {
        so_path = candidate;
        return true;
      }
    }
  }
  return false;
}

static bool ReadSoDataForSt(const std::string &so_path, std::vector<char_t> &so_data) {
  std::ifstream file(so_path, std::ios::binary | std::ios::ate);
  if (!file.good()) {
    return false;
  }
  const std::streamsize file_size = file.tellg();
  if (file_size <= 0) {
    return false;
  }
  file.seekg(0, std::ios::beg);
  so_data.resize(static_cast<size_t>(file_size));
  return file.read(so_data.data(), file_size).good();
}

static OpSoBinPtr BuildCustomOpSoBinForSt(const std::string &so_name, const std::string &vendor_name,
                                          const std::vector<char_t> &so_data) {
  if (so_data.empty() || so_data.size() > static_cast<size_t>(UINT32_MAX)) {
    return nullptr;
  }
  auto data = std::unique_ptr<char_t[]>(new (std::nothrow) char_t[so_data.size()]);
  if (data == nullptr) {
    return nullptr;
  }
  std::copy_n(so_data.data(), so_data.size(), data.get());
  return std::make_shared<OpSoBin>(so_name, vendor_name, std::move(data), static_cast<uint32_t>(so_data.size()),
                                   SoBinType::kCustomOp);
}

static bool WriteElfHeaderForSt(const std::string &so_path, const uint16_t e_machine) {
  std::vector<uint8_t> elf_header(64U, 0U);
  elf_header[0] = 0x7FU;
  elf_header[1] = static_cast<uint8_t>('E');
  elf_header[2] = static_cast<uint8_t>('L');
  elf_header[3] = static_cast<uint8_t>('F');
  elf_header[4] = 2U;
  elf_header[5] = 1U;
  elf_header[6] = 1U;
  elf_header[16] = 3U;
  elf_header[18] = static_cast<uint8_t>(e_machine & 0xFFU);
  elf_header[19] = static_cast<uint8_t>((e_machine >> 8U) & 0xFFU);

  std::ofstream file(so_path, std::ios::binary | std::ios::trunc);
  if (!file.good()) {
    return false;
  }
  file.write(reinterpret_cast<const char *>(elf_header.data()), static_cast<std::streamsize>(elf_header.size()));
  return file.good();
}

static bool WriteTextFileForSt(const std::string &path, const std::string &content) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.good()) {
    return false;
  }
  file << content;
  return file.good();
}

static void GetCurrentEnvWithFallbackForSt(std::string &current_env_os, std::string &current_env_cpu) {
  PluginManager::GetCurEnvPackageOsAndCpuType(current_env_os, current_env_cpu);
  if (current_env_os.empty()) {
    current_env_os = "linux";
  }
  if (current_env_cpu.empty()) {
#if defined(__aarch64__) || defined(__arm64__)
    current_env_cpu = "aarch64";
#elif defined(__x86_64__) || defined(__amd64__)
    current_env_cpu = "x86_64";
#endif
  }
}

}  // namespace

class TestModelCustomOpsHelper : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(TestModelCustomOpsHelper, collect_used_custom_op_types_collects_root_subgraph_and_skips_invalid_subgraphs) {
  RegisterCustomOpCreatorForSt(kCollectRootType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<PortableOpForSerializeSuccess>();
  });
  RegisterCustomOpCreatorForSt(kCollectSubType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<PortableOpForSerializeSuccess>();
  });

  const auto ge_root_model = CreateRootModelForCustomOps(kCollectRootType, kCollectSubType, true, true);
  ASSERT_NE(ge_root_model, nullptr);

  TestableModelHelper model_helper;
  std::set<std::string> used_custom_op_types;
  EXPECT_EQ(model_helper.CollectUsedCustomOpTypes(ge_root_model, used_custom_op_types), SUCCESS);
  EXPECT_EQ(used_custom_op_types.size(), 2U);
  EXPECT_EQ(used_custom_op_types.count(kCollectRootType), 1U);
  EXPECT_EQ(used_custom_op_types.count(kCollectSubType), 1U);
}

TEST_F(TestModelCustomOpsHelper, serialize_custom_op_kernel_covers_all_target_branches) {
  TestableModelHelper model_helper;

  std::vector<uint8_t> create_fail_merged_kernels = {0xA1U};
  EXPECT_EQ(model_helper.SerializeCustomOpKernel(nullptr, "StModelHelperSerializeCreateFailPortableOp", create_fail_merged_kernels),
            FAILED);
  EXPECT_EQ(create_fail_merged_kernels, std::vector<uint8_t>({0xA1U}));

  std::vector<uint8_t> serialize_fail_merged_kernels = {0xC3U};
  PortableOpForSerializeFail portable_op_fail;
  EXPECT_EQ(model_helper.SerializeCustomOpKernel(&portable_op_fail, kSerializeFailType, serialize_fail_merged_kernels),
            GRAPH_FAILED);
  EXPECT_EQ(serialize_fail_merged_kernels, std::vector<uint8_t>({0xC3U}));

  std::vector<uint8_t> empty_bin_merged_kernels = {0xD4U};
  PortableOpForSerializeEmpty portable_op_empty;
  EXPECT_EQ(model_helper.SerializeCustomOpKernel(&portable_op_empty, kSerializeEmptyType, empty_bin_merged_kernels), SUCCESS);
  EXPECT_EQ(empty_bin_merged_kernels, std::vector<uint8_t>({0xD4U}));

  std::vector<uint8_t> success_merged_kernels = {0xE5U};
  const auto old_size = success_merged_kernels.size();
  PortableOpForSerializeSuccess portable_op_success;
  EXPECT_EQ(model_helper.SerializeCustomOpKernel(&portable_op_success, kSerializeSuccessType, success_merged_kernels), SUCCESS);

  const auto expected_size =
      old_size + sizeof(CustomKernelItemHeader) + strlen(kSerializeSuccessType) + kPortableKernelBin.size();
  ASSERT_EQ(success_merged_kernels.size(), expected_size);

  CustomKernelItemHeader header{};
  std::copy_n(success_merged_kernels.data() + old_size, sizeof(CustomKernelItemHeader),
              reinterpret_cast<uint8_t *>(&header));
  EXPECT_EQ(header.magic, kCustomKernelItemMagic);
  EXPECT_EQ(header.name_len, strlen(kSerializeSuccessType));
  EXPECT_EQ(header.bin_len, kPortableKernelBin.size());
}

TEST_F(TestModelCustomOpsHelper, save_custom_kernels_partition_skips_when_merged_kernels_empty_after_loop) {
  RegisterCustomOpCreatorForSt(kSaveEmptyRootType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<PortableOpForSerializeEmpty>();
  });
  RegisterCustomOpCreatorForSt(kSaveEmptySubType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<PortableOpForSerializeEmpty>();
  });

  const auto ge_root_model = CreateRootModelForCustomOps(kSaveEmptyRootType, kSaveEmptySubType, false, false);
  ASSERT_NE(ge_root_model, nullptr);

  auto om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  TestableModelHelper model_helper;
  EXPECT_EQ(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);
}

TEST_F(TestModelCustomOpsHelper, save_custom_kernels_partition_success_with_non_empty_used_custom_op_types) {
  RegisterCustomOpCreatorForSt(kSaveSuccessRootType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<PortableOpForSerializeSuccess>();
  });
  RegisterCustomOpCreatorForSt(kSaveSuccessSubType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<PortableOpForSerializeSuccess>();
  });

  const auto ge_root_model = CreateRootModelForCustomOps(kSaveSuccessRootType, kSaveSuccessSubType, false, false);
  ASSERT_NE(ge_root_model, nullptr);

  auto om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  TestableModelHelper model_helper;
  EXPECT_EQ(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);
}

TEST_F(TestModelCustomOpsHelper, load_custom_kernels_returns_success_when_partition_not_found) {
  TestableModelHelper model_helper;
  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;
  load_helper.model_contexts_.emplace_back(OmFileContext{});
  EXPECT_EQ(model_helper.LoadCustomOps(load_helper), SUCCESS);
}

TEST_F(TestModelCustomOpsHelper, load_custom_kernels_loads_non_empty_partition_successfully) {
  RegisterCustomOpCreatorForSt(kLoadSuccessType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<PortableOpForDeserializeRecord>();
  });

  const std::vector<uint8_t> serialized_bin = {0x66U, 0x77U};
  const auto payload = BuildCustomKernelPartitionPayload(kLoadSuccessType, serialized_bin);

  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;
  OmFileContext context;
  ModelPartition custom_partition;
  custom_partition.type = ModelPartitionType::CUSTOM_OPS;
  custom_partition.data = payload.data();
  custom_partition.size = payload.size();
  context.partition_datas_.push_back(custom_partition);
  load_helper.model_contexts_.push_back(context);

  g_deserialize_called_count = 0U;
  g_last_deserialize_bin_size = 0U;
  TestableModelHelper model_helper;
  EXPECT_EQ(model_helper.LoadCustomOps(load_helper), SUCCESS);
  EXPECT_EQ(g_deserialize_called_count, 1U);
  EXPECT_EQ(g_last_deserialize_bin_size, serialized_bin.size());
}

TEST_F(TestModelCustomOpsHelper, custom_op_so_loader_load_success_repeat_and_conflict_are_stable) {
  std::string source_so_path;
  ASSERT_TRUE(FindReadableSystemSoPathForSt(source_so_path));
  std::vector<char_t> so_data;
  ASSERT_TRUE(ReadSoDataForSt(source_so_path, so_data));
  ASSERT_FALSE(so_data.empty());

  CustomOpSoLoader loader;
  const auto so_bin = BuildCustomOpSoBinForSt("libst_custom_loader_conflict.so", "st_vendor", so_data);
  ASSERT_NE(so_bin, nullptr);
  ASSERT_EQ(loader.LoadCustomOpSoBins({so_bin}), SUCCESS);
  ASSERT_EQ(loader.loaded_states_.size(), 1U);

  EXPECT_EQ(loader.LoadCustomOpSoBins({so_bin}), SUCCESS);
  EXPECT_EQ(loader.loaded_states_.size(), 1U);

  std::vector<char_t> modified_so_data = so_data;
  modified_so_data[0U] = static_cast<char_t>(modified_so_data[0U] ^ 0x1);
  const auto modified_so_bin = BuildCustomOpSoBinForSt("libst_custom_loader_conflict.so", "st_vendor",
                                                        modified_so_data);
  ASSERT_NE(modified_so_bin, nullptr);
  EXPECT_NE(loader.LoadCustomOpSoBins({modified_so_bin}), SUCCESS);
  EXPECT_EQ(loader.loaded_states_.size(), 1U);

  loader.Cleanup();
  EXPECT_TRUE(loader.loaded_states_.empty());
}

TEST_F(TestModelCustomOpsHelper, custom_op_so_loader_get_instance_and_empty_input_success) {
  auto &loader = CustomOpSoLoader::GetInstance();
  EXPECT_EQ(loader.LoadCustomOpSoBins({}), SUCCESS);
  loader.Cleanup();
}

TEST_F(TestModelCustomOpsHelper, ge_root_model_check_so_arch_matches_target_covers_core_paths) {
  GeRootModel ge_root_model;
  const std::string tmp_dir = BuildTmpDirForSt("arch_check");
  ScopedTmpDirCleanerForSt tmp_guard(tmp_dir);
  ASSERT_EQ(system(("mkdir -p " + tmp_dir).c_str()), 0);

  EXPECT_NE(ge_root_model.CheckSoArchMatchesTarget(tmp_dir + "/not_exist.so", "x86_64"), SUCCESS);

  const std::string not_elf_so_path = tmp_dir + "/not_elf.so";
  ASSERT_TRUE(WriteTextFileForSt(not_elf_so_path, "not an elf"));
  EXPECT_NE(ge_root_model.CheckSoArchMatchesTarget(not_elf_so_path, "x86_64"), SUCCESS);

  const std::string good_elf_so_path = tmp_dir + "/good_x86_64.so";
  ASSERT_TRUE(WriteElfHeaderForSt(good_elf_so_path, EM_X86_64));
  EXPECT_EQ(ge_root_model.CheckSoArchMatchesTarget(good_elf_so_path, "x86_64"), SUCCESS);

  const std::string mismatch_elf_so_path = tmp_dir + "/mismatch_aarch64.so";
  ASSERT_TRUE(WriteElfHeaderForSt(mismatch_elf_so_path, EM_AARCH64));
  EXPECT_NE(ge_root_model.CheckSoArchMatchesTarget(mismatch_elf_so_path, "x86_64"), SUCCESS);

  EXPECT_EQ(ge_root_model.CheckSoArchMatchesTarget(not_elf_so_path, "unsupported_cpu"), SUCCESS);
}

TEST_F(TestModelCustomOpsHelper, ge_root_model_collect_custom_op_so_from_custom_opp_path_success) {
  ScopedEnvVarForSt custom_opp_env_guard("ASCEND_CUSTOM_OPP_PATH");
  GeRootModel ge_root_model;

  const std::string tmp_dir = BuildTmpDirForSt("collect_so_success");
  ScopedTmpDirCleanerForSt tmp_guard(tmp_dir);
  const std::string root1 = tmp_dir + "/root1";
  const std::string root2 = tmp_dir + "/root2";
  const std::string op_graph_dir = root1 + "/op_graph/lib/linux/x86_64";
  const std::string ignored_dir = root1 + "/op_graph/lib/linux/aarch64";
  ASSERT_EQ(system(("mkdir -p " + op_graph_dir).c_str()), 0);
  ASSERT_EQ(system(("mkdir -p " + ignored_dir).c_str()), 0);
  ASSERT_EQ(system(("mkdir -p " + root2).c_str()), 0);

  const std::string so0 = op_graph_dir + "/libst_collect_0.so";
  const std::string ignored_so = ignored_dir + "/libst_ignore.so";
  ASSERT_TRUE(WriteElfHeaderForSt(so0, EM_X86_64));
  ASSERT_TRUE(WriteElfHeaderForSt(ignored_so, EM_X86_64));

  const std::string custom_opp_path = ":" + root1 + "::" + root2 + ":";
  ASSERT_EQ(mmSetEnv("ASCEND_CUSTOM_OPP_PATH", custom_opp_path.c_str(), 1), EN_OK);
  ASSERT_EQ(ge_root_model.CollectCustomOpSoFromCustomOppPath("linux", "x86_64"), SUCCESS);
  const auto &custom_op_so_set = ge_root_model.GetCustomOpSoSet();
  ASSERT_EQ(custom_op_so_set.size(), 1U);
  EXPECT_EQ(custom_op_so_set.count(so0), 1U);
  EXPECT_EQ(custom_op_so_set.count(ignored_so), 0U);
}

TEST_F(TestModelCustomOpsHelper, ge_root_model_collect_custom_op_so_from_custom_opp_path_arch_mismatch_fail) {
  ScopedEnvVarForSt custom_opp_env_guard("ASCEND_CUSTOM_OPP_PATH");
  GeRootModel ge_root_model;

  const std::string tmp_dir = BuildTmpDirForSt("collect_so_arch_mismatch");
  ScopedTmpDirCleanerForSt tmp_guard(tmp_dir);
  const std::string target_dir = tmp_dir + "/root/op_proto/lib/linux/x86_64";
  ASSERT_EQ(system(("mkdir -p " + target_dir).c_str()), 0);
  const std::string mismatch_so_path = target_dir + "/libst_arch_mismatch.so";
  ASSERT_TRUE(WriteElfHeaderForSt(mismatch_so_path, EM_AARCH64));
  ASSERT_EQ(mmSetEnv("ASCEND_CUSTOM_OPP_PATH", (tmp_dir + "/root").c_str(), 1), EN_OK);
  EXPECT_NE(ge_root_model.CollectCustomOpSoFromCustomOppPath("linux", "x86_64"), SUCCESS);
}

TEST_F(TestModelCustomOpsHelper, ge_root_model_target_host_env_and_cross_compile_decision) {
  std::string current_env_os;
  std::string current_env_cpu;
  GetCurrentEnvWithFallbackForSt(current_env_os, current_env_cpu);
  ASSERT_FALSE(current_env_os.empty());
  ASSERT_FALSE(current_env_cpu.empty());

  ScopedHostEnvOptionForSt host_env_guard("linux", "x86_64");
  GeRootModel ge_root_model;
  std::string target_os;
  std::string target_cpu;
  ASSERT_EQ(ge_root_model.GetTargetHostEnv(target_os, target_cpu), SUCCESS);
  EXPECT_EQ(target_os, "linux");
  EXPECT_EQ(target_cpu, "x86_64");

  EXPECT_FALSE(ge_root_model.IsCrossCompileTarget(current_env_os, current_env_cpu));
  const std::string mismatch_cpu = (current_env_cpu == "x86_64") ? "aarch64" : "x86_64";
  EXPECT_TRUE(ge_root_model.IsCrossCompileTarget(current_env_os, mismatch_cpu));
}
}  // namespace ge
