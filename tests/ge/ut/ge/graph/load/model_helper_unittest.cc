/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <stdio.h>
#include <gtest/gtest.h>
#include <dlfcn.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <set>
#include <thread>
#include <utility>
#include "mmpa/mmpa_api.h"
#include "macro_utils/dt_public_scope.h"
#define private public
#include "common/model/ge_root_model.h"
#include "common/helper/custom_op_so_loader.h"
#undef private
#include "framework/common/helper/model_helper.h"
#include "hybrid/node_executor/aicore/aicore_op_task.h"
#include "framework/omg/ge_init.h"
#include "common/model/ge_model.h"
#include "common/helper/model_parser_base.h"
#include "graph/buffer/buffer_impl.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/file_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/ge_local_context.h"
#include "depends/runtime/src/runtime_stub.h"
#include "macro_utils/dt_public_unscope.h"
#include "faker/space_registry_faker.h"
#include "common/path_utils.h"
#include "common/plugin/plugin_manager.h"
#include "ge/framework/common/taskdown_common.h"
#include "stub/gert_runtime_stub.h"
#include "common/share_graph.h"
#include "proto/task.pb.h"
#include "faker/space_registry_faker.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "graph/custom_op.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op_registry.h"
#include "depends/mmpa/src/mmpa_stub.h"

using namespace std;
extern std::string g_runtime_stub_mock;

namespace ge {
using namespace hybrid;
namespace {
const char *const kEnvName = "ASCEND_OPP_PATH";
const char *const kEnvNameCustom = "ASCEND_CUSTOM_OPP_PATH";
const string kOpsProto = "libopsproto_rt2.0.so";
const string kOpMaster = "libopmaster_rt2.0.so";
const string kInner = "built-in";
const string kOpsProtoPath = "/op_proto/lib/linux/x86_64/";
const string kOpMasterPath = "/op_impl/ai_core/tbe/op_tiling/lib/linux/x86_64/";

constexpr const char *kPortableOpTypeForModelHelper = "ModelHelperPortableOpForUt";
constexpr const char *kPortableOpEmptyTypeForModelHelper = "ModelHelperPortableOpEmptyForUt";
constexpr const char *kPortableOpSerializeFailTypeForModelHelper = "ModelHelperPortableOpSerializeFailForUt";
constexpr const char *kNonPortableOpTypeForModelHelper = "ModelHelperNonPortableOpForUt";
constexpr const char *kUnregisteredPortableOpTypeForModelHelper = "ModelHelperUnregisteredPortableOpForUt";
constexpr const char *kRegistryOnlyPortableOpTypeForModelHelper = "ModelHelperRegistryOnlyPortableOpForUt";
const std::vector<uint8_t> kPortableKernelBinForModelHelper = {0x11U, 0x22U, 0x33U};
std::atomic<size_t> g_custom_op_loader_dlopen_count{0U};
std::mutex g_custom_op_loader_dlopen_mutex;
std::condition_variable g_custom_op_loader_dlopen_cv;
bool g_custom_op_loader_release_dlopen = false;

class BlockingDlopenMmpaStubForUt : public MmpaStubApiGe {
 public:
  void *DlOpen(const char *file_name, int32_t mode) override {
    {
      std::unique_lock<std::mutex> lock(g_custom_op_loader_dlopen_mutex);
      ++g_custom_op_loader_dlopen_count;
      g_custom_op_loader_dlopen_cv.notify_all();
      g_custom_op_loader_dlopen_cv.wait(lock, [] { return g_custom_op_loader_release_dlopen; });
    }
    return MmpaStubApiGe::DlOpen(file_name, mode);
  }
};

class ScopedCustomOpLoaderBlockingDlopenForUt {
 public:
  ScopedCustomOpLoaderBlockingDlopenForUt() {
    g_custom_op_loader_dlopen_count = 0U;
    g_custom_op_loader_release_dlopen = false;
    MmpaStub::GetInstance().SetImpl(std::make_shared<BlockingDlopenMmpaStubForUt>());
  }

  ~ScopedCustomOpLoaderBlockingDlopenForUt() {
    {
      std::lock_guard<std::mutex> lock(g_custom_op_loader_dlopen_mutex);
      g_custom_op_loader_release_dlopen = true;
    }
    g_custom_op_loader_dlopen_cv.notify_all();
    MmpaStub::GetInstance().Reset();
  }
};

static bool FindNotLoadedSystemSoForModelHelperUt(std::string &so_path) {
  const std::vector<std::string> so_dirs = {"/lib/aarch64-linux-gnu",
                                            "/usr/lib/aarch64-linux-gnu",
                                            "/lib/x86_64-linux-gnu",
                                            "/usr/lib/x86_64-linux-gnu",
                                            "/lib64",
                                            "/usr/lib64"};
  const std::vector<std::string> so_names = {"libz.so.1",    "liblzma.so.5", "libbz2.so.1.0",
                                             "libuuid.so.1", "libffi.so.8",  "libcrypt.so.1"};
  for (const auto &so_dir : so_dirs) {
    for (const auto &so_name : so_names) {
      const std::string candidate = so_dir + "/" + so_name;
      if (mmAccess2(candidate.c_str(), M_R_OK) != EN_OK) {
        continue;
      }
      so_path = candidate;
      return true;
    }
  }
  return false;
}

static bool FindTwoReadableSystemSosForModelHelperUt(std::string &first_so_path, std::string &second_so_path) {
  const std::vector<std::string> so_dirs = {"/lib/aarch64-linux-gnu",
                                            "/usr/lib/aarch64-linux-gnu",
                                            "/lib/x86_64-linux-gnu",
                                            "/usr/lib/x86_64-linux-gnu",
                                            "/lib64",
                                            "/usr/lib64"};
  const std::vector<std::string> so_names = {"libz.so.1",   "liblzma.so.5",  "libbz2.so.1.0", "libuuid.so.1",
                                             "libffi.so.8", "libcrypt.so.1", "libm.so.6"};
  std::set<std::string> found_paths;
  for (const auto &so_dir : so_dirs) {
    for (const auto &so_name : so_names) {
      const std::string candidate = so_dir + "/" + so_name;
      if ((found_paths.find(candidate) != found_paths.end()) || (mmAccess2(candidate.c_str(), M_R_OK) != EN_OK)) {
        continue;
      }
      if (first_so_path.empty()) {
        first_so_path = candidate;
      } else {
        second_so_path = candidate;
        return true;
      }
      (void)found_paths.emplace(candidate);
    }
  }
  return false;
}

class ScopedEnvVarForModelHelperUt {
 public:
  explicit ScopedEnvVarForModelHelperUt(std::string env_name) : env_name_(std::move(env_name)) {
    const char *env_value = std::getenv(env_name_.c_str());
    if (env_value != nullptr) {
      has_old_value_ = true;
      old_value_ = env_value;
    }
  }

  ~ScopedEnvVarForModelHelperUt() {
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

class ScopedHostEnvForModelHelperUt {
 public:
  ScopedHostEnvForModelHelperUt(const std::string &host_env_os, const std::string &host_env_cpu) {
    (void)GetThreadLocalContext().GetOption(OPTION_HOST_ENV_OS, old_host_env_os_);
    (void)GetThreadLocalContext().GetOption(OPTION_HOST_ENV_CPU, old_host_env_cpu_);
    std::map<std::string, std::string> options_map;
    options_map[OPTION_HOST_ENV_OS] = host_env_os;
    options_map[OPTION_HOST_ENV_CPU] = host_env_cpu;
    (void)GetThreadLocalContext().SetGlobalOption(options_map);
  }

  ~ScopedHostEnvForModelHelperUt() {
    std::map<std::string, std::string> restore_options;
    restore_options[OPTION_HOST_ENV_OS] = old_host_env_os_;
    restore_options[OPTION_HOST_ENV_CPU] = old_host_env_cpu_;
    (void)GetThreadLocalContext().SetGlobalOption(restore_options);
  }

 private:
  std::string old_host_env_os_;
  std::string old_host_env_cpu_;
};

class ScopedTmpDirCleanerForModelHelperUt {
 public:
  explicit ScopedTmpDirCleanerForModelHelperUt(std::string dir_path) : dir_path_(std::move(dir_path)) {
    Cleanup();
  }

  ~ScopedTmpDirCleanerForModelHelperUt() {
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

static bool ReadSoDataForModelHelperUt(const std::string &so_path, std::vector<char_t> &so_data) {
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
  if (!file.read(so_data.data(), file_size)) {
    so_data.clear();
    return false;
  }
  return true;
}

static bool WriteElfHeaderForModelHelperUt(const std::string &so_path, const uint16_t e_machine) {
  std::vector<uint8_t> elf_header(64U, 0U);
  elf_header[0] = 0x7FU;
  elf_header[1] = static_cast<uint8_t>('E');
  elf_header[2] = static_cast<uint8_t>('L');
  elf_header[3] = static_cast<uint8_t>('F');
  elf_header[4] = 2U;   // ELFCLASS64
  elf_header[5] = 1U;   // ELFDATA2LSB
  elf_header[6] = 1U;   // EV_CURRENT
  elf_header[16] = 3U;  // ET_DYN
  elf_header[18] = static_cast<uint8_t>(e_machine & 0xFFU);
  elf_header[19] = static_cast<uint8_t>((e_machine >> 8U) & 0xFFU);

  std::ofstream file(so_path, std::ios::binary | std::ios::trunc);
  if (!file.good()) {
    return false;
  }
  file.write(reinterpret_cast<const char *>(elf_header.data()), static_cast<std::streamsize>(elf_header.size()));
  return file.good();
}

static bool GetExpectedElfMachineForCpuModelHelperUt(const std::string &target_cpu, uint16_t &expected_machine) {
  std::string normalized_target_cpu = target_cpu;
  std::transform(normalized_target_cpu.begin(), normalized_target_cpu.end(), normalized_target_cpu.begin(),
                 [](char ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); });
  if ((normalized_target_cpu == "x86_64") || (normalized_target_cpu == "amd64")) {
    expected_machine = 62U;  // EM_X86_64
    return true;
  }
  if ((normalized_target_cpu == "aarch64") || (normalized_target_cpu == "arm64")) {
    expected_machine = 183U;  // EM_AARCH64
    return true;
  }
  return false;
}

static OpSoBinPtr BuildCustomOpSoBinForModelHelperUt(const std::string &so_name, const std::string &vendor_name,
                                                     const std::vector<char_t> &so_data) {
  if ((so_data.empty()) || (so_data.size() > static_cast<size_t>(UINT32_MAX))) {
    return nullptr;
  }
  auto so_data_buf = std::unique_ptr<char_t[]>(new (std::nothrow) char_t[so_data.size()]);
  if (so_data_buf == nullptr) {
    return nullptr;
  }
  if (memcpy_s(so_data_buf.get(), so_data.size(), so_data.data(), so_data.size()) != EOK) {
    return nullptr;
  }
  return std::make_shared<OpSoBin>(so_name, vendor_name, std::move(so_data_buf), static_cast<uint32_t>(so_data.size()),
                                   SoBinType::kCustomOp);
}

static OpSoBinPtr BuildOpSoBinForModelHelperUt(const std::string &so_name, const std::string &vendor_name,
                                               const std::vector<uint8_t> &so_data, const SoBinType so_bin_type) {
  if ((so_data.empty()) || (so_data.size() > static_cast<size_t>(UINT32_MAX))) {
    return nullptr;
  }
  auto so_data_buf = std::unique_ptr<char_t[]>(new (std::nothrow) char_t[so_data.size()]);
  if (so_data_buf == nullptr) {
    return nullptr;
  }
  if (memcpy_s(so_data_buf.get(), so_data.size(), so_data.data(), so_data.size()) != EOK) {
    return nullptr;
  }
  return std::make_shared<OpSoBin>(so_name, vendor_name, std::move(so_data_buf), static_cast<uint32_t>(so_data.size()),
                                   so_bin_type);
}

static bool BuildSoBinsPayloadForModelHelperUt(const std::vector<OpSoBinPtr> &so_bins, std::vector<uint8_t> &payload) {
  OpSoStore op_so_store;
  for (const auto &so_bin : so_bins) {
    if (so_bin == nullptr) {
      return false;
    }
    op_so_store.AddKernel(so_bin);
  }
  if (!op_so_store.Build()) {
    return false;
  }
  const auto *data = op_so_store.Data();
  const auto size = op_so_store.DataSize();
  if ((data == nullptr) || (size == 0U)) {
    return false;
  }
  payload.assign(data, data + size);
  return true;
}

static bool PrepareBuiltInOpMasterFilesForModelHelperUt(const std::string &opp_path, std::string &inner_op_master) {
  inner_op_master = opp_path + "built_in/op_master_device/lib/";
  if (system(("mkdir -p " + inner_op_master).c_str()) != 0) {
    return false;
  }
  inner_op_master += "Ascend-V7.6-libopmaster.so";
  if (system(("touch " + inner_op_master).c_str()) != 0) {
    return false;
  }
  if (system(("echo 'Ascend-V7.6-libopmaster' > " + inner_op_master).c_str()) != 0) {
    return false;
  }

  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  if (system(("mkdir -p " + inner_proto_path).c_str()) != 0) {
    return false;
  }
  inner_proto_path += kOpsProto;
  if (system(("touch " + inner_proto_path).c_str()) != 0) {
    return false;
  }
  if (system(("echo 'ops proto' > " + inner_proto_path).c_str()) != 0) {
    return false;
  }

  std::string inner_tiling_path = opp_path + kInner + kOpMasterPath;
  if (system(("mkdir -p " + inner_tiling_path).c_str()) != 0) {
    return false;
  }
  inner_tiling_path += kOpMaster;
  if (system(("touch " + inner_tiling_path).c_str()) != 0) {
    return false;
  }
  return system(("echo 'op tiling ' > " + inner_tiling_path).c_str()) == 0;
}

static bool AddAutofuseNodeAndSetBinFilePathForModelHelperUt(const ComputeGraphPtr &ge_root_graph,
                                                             const std::string &autofuse_stub_so) {
  if ((ge_root_graph == nullptr) || autofuse_stub_so.empty()) {
    return false;
  }
  OpDescBuilder op_desc_builder("test", "AscBackend");
  const auto &op_desc = op_desc_builder.Build();
  auto node = ge_root_graph->AddNode(op_desc);
  if (node == nullptr) {
    return false;
  }
  node->SetOwnerComputeGraph(ge_root_graph);
  for (const auto &n : ge_root_graph->GetAllNodesPtr()) {
    (void)ge::AttrUtils::SetStr(n->GetOpDesc(), "bin_file_path", autofuse_stub_so);
  }
  return true;
}

static bool BuildOfflineAutofuseSoBinsForModelHelperUt(OmFileLoadHelper &load_helper,
                                                       std::vector<uint8_t> &so_payload) {
  load_helper.is_inited_ = true;
  OmFileContext cur_ctx;
  ModelPartition so_patition;
  so_patition.type = ModelPartitionType::SO_BINS;
  const auto op_master_so_bin = BuildOpSoBinForModelHelperUt("Ascend-V7.6-libopmaster.so", "built_in", {0x11U, 0x22U},
                                                             SoBinType::kOpMasterDevice);
  const auto autofuse_so_bin = BuildOpSoBinForModelHelperUt("libautofuse_for_ut.so", "autofuse_vendor",
                                                            {0x33U, 0x44U, 0x55U}, SoBinType::kAutofuse);
  if ((op_master_so_bin == nullptr) || (autofuse_so_bin == nullptr)) {
    return false;
  }
  if (!BuildSoBinsPayloadForModelHelperUt({op_master_so_bin, autofuse_so_bin}, so_payload)) {
    return false;
  }
  so_patition.data = so_payload.data();
  so_patition.size = so_payload.size();
  cur_ctx.partition_datas_.push_back(so_patition);
  load_helper.model_contexts_.push_back(cur_ctx);
  return true;
}

struct CrossModeCustomOppPathsForModelHelperUt {
  std::string custom_opp_path;
  std::string target_graph_so_path;
  std::string non_target_graph_so_path;
};

static bool ResolveCrossModeTargetEnvForModelHelperUt(std::string &current_env_os, std::string &current_env_cpu,
                                                      std::string &target_env_cpu) {
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
  if (current_env_cpu.empty()) {
    return false;
  }
  target_env_cpu = current_env_cpu;
  std::transform(target_env_cpu.begin(), target_env_cpu.end(), target_env_cpu.begin(),
                 [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  if ((target_env_cpu == "aarch64") || (target_env_cpu == "arm64")) {
    target_env_cpu = "x86_64";
  } else {
    target_env_cpu = "aarch64";
  }
  return true;
}

static bool PrepareCrossModeCustomOppForModelHelperUt(const std::string &custom_opp_path,
                                                      const std::string &current_env_os,
                                                      const std::string &current_env_cpu,
                                                      const std::string &target_env_cpu,
                                                      CrossModeCustomOppPathsForModelHelperUt &so_paths) {
  so_paths.custom_opp_path = custom_opp_path;
  const std::string target_graph_so_dir = custom_opp_path + "/op_graph/lib/" + current_env_os + "/" + target_env_cpu;
  const std::string non_target_graph_so_dir =
      custom_opp_path + "/op_proto/lib/" + current_env_os + "/" + current_env_cpu;
  so_paths.target_graph_so_path = target_graph_so_dir + "/libcustom_unrelated_ut.so";
  so_paths.non_target_graph_so_path = non_target_graph_so_dir + "/libshould_not_collect_ut.so";

  if (system(("mkdir -p " + target_graph_so_dir).c_str()) != 0) {
    return false;
  }
  if (system(("mkdir -p " + non_target_graph_so_dir).c_str()) != 0) {
    return false;
  }
  uint16_t expected_machine = 0U;
  if (!GetExpectedElfMachineForCpuModelHelperUt(target_env_cpu, expected_machine)) {
    return false;
  }
  if (!WriteElfHeaderForModelHelperUt(so_paths.target_graph_so_path, expected_machine)) {
    return false;
  }
  return system(("touch " + so_paths.non_target_graph_so_path).c_str()) == 0;
}

static void PreparePortableCustomOpCrossModeForModelHelperUt(const std::string &custom_opp_path,
                                                             std::string &current_env_os, std::string &current_env_cpu,
                                                             std::string &target_env_cpu,
                                                             CrossModeCustomOppPathsForModelHelperUt &so_paths) {
  ASSERT_TRUE(ResolveCrossModeTargetEnvForModelHelperUt(current_env_os, current_env_cpu, target_env_cpu));
  ASSERT_TRUE(PrepareCrossModeCustomOppForModelHelperUt(custom_opp_path, current_env_os, current_env_cpu,
                                                        target_env_cpu, so_paths));
  ASSERT_EQ(mmSetEnv(kEnvNameCustom, custom_opp_path.c_str(), 1), EN_OK);
}

static std::string BuildProcFdPathForModelHelperUt(const int32_t fd) {
  return "/proc/self/fd/" + std::to_string(fd);
}

class ModelHelperPortableOpForUt : public EagerExecuteOp, public PortableOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer = kPortableKernelBinForModelHelper;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_SUCCESS;
  }
};

class ModelHelperPortableOpEmptyForUt : public EagerExecuteOp, public PortableOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer.clear();
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_SUCCESS;
  }
};

class ModelHelperPortableOpSerializeFailForUt : public EagerExecuteOp, public PortableOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_FAILED;
  }

  ge::graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_SUCCESS;
  }
};

class ModelHelperNonPortableOpForUt : public EagerExecuteOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }
};

static void RegisterCustomOpCreatorForModelHelperUt(const std::string &op_type, const BaseOpCreator &creator) {
  const auto ret = CustomOpFactory::RegisterCustomOpCreator(op_type.c_str(), creator);
  EXPECT_TRUE((ret == GRAPH_SUCCESS) || (ret == GRAPH_FAILED));
}

static NodePtr AddNodeWithOpTypeForModelHelperUt(const ComputeGraphPtr &graph, const std::string &node_name,
                                                 const std::string &op_type) {
  auto op_desc = std::make_shared<OpDesc>(node_name, op_type);
  if (op_desc == nullptr) {
    return nullptr;
  }
  (void)op_desc->AddDynamicInputDesc("x", 1);
  (void)op_desc->AddDynamicOutputDesc("y", 1);
  return graph->AddNode(op_desc);
}

static GeRootModelPtr CreateGeRootModelForModelHelperUt(const std::string &root_op_type,
                                                        const std::string &sub_op_type) {
  auto root_graph = std::make_shared<ComputeGraph>("root_graph_for_custom_kernels");
  if (!root_op_type.empty()) {
    (void)AddNodeWithOpTypeForModelHelperUt(root_graph, "root_node", root_op_type);
  }

  auto ge_root_model = std::make_shared<GeRootModel>();
  if (ge_root_model->Initialize(root_graph) != SUCCESS) {
    return nullptr;
  }
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());

  auto sub_graph = std::make_shared<ComputeGraph>("sub_graph_for_custom_kernels");
  if (!sub_op_type.empty()) {
    (void)AddNodeWithOpTypeForModelHelperUt(sub_graph, "sub_node", sub_op_type);
  }
  GeModelPtr sub_model = std::make_shared<GeModel>();
  sub_model->SetGraph(sub_graph);
  ge_root_model->subgraph_instance_name_to_model_["sub_graph_for_custom_kernels"] = sub_model;
  return ge_root_model;
}

static bool FindCustomOpsPartitionForModelHelperUt(const std::shared_ptr<OmFileSaveHelper> &om_file_save_helper,
                                                   ModelPartition &partition) {
  if (om_file_save_helper == nullptr || om_file_save_helper->model_contexts_.empty()) {
    return false;
  }
  for (const auto &saved_partition : om_file_save_helper->model_contexts_[0U].partition_datas_) {
    if (saved_partition.type == ModelPartitionType::CUSTOM_OPS) {
      partition = saved_partition;
      return true;
    }
  }
  return false;
}

static std::vector<uint8_t> BuildExpectedCustomOpsDataForModelHelperUt(const std::string &op_type,
                                                                       const std::vector<uint8_t> &kernel_bin) {
  CustomKernelItemHeader header{};
  header.magic = kCustomKernelItemMagic;
  header.name_len = static_cast<uint32_t>(op_type.size());
  header.bin_len = static_cast<uint32_t>(kernel_bin.size());

  std::vector<uint8_t> expected(sizeof(CustomKernelItemHeader));
  std::copy_n(reinterpret_cast<const uint8_t *>(&header), sizeof(CustomKernelItemHeader), expected.data());
  expected.insert(expected.end(), op_type.begin(), op_type.end());
  expected.insert(expected.end(), kernel_bin.begin(), kernel_bin.end());
  return expected;
}

static bool FindSoBinsPartitionForModelHelperUt(const std::shared_ptr<OmFileSaveHelper> &om_file_save_helper,
                                                ModelPartition &partition) {
  if (om_file_save_helper == nullptr || om_file_save_helper->model_contexts_.empty()) {
    return false;
  }
  for (const auto &saved_partition : om_file_save_helper->model_contexts_[0U].partition_datas_) {
    if (saved_partition.type == ModelPartitionType::SO_BINS) {
      partition = saved_partition;
      return true;
    }
  }
  return false;
}

static void FillModelTaskDef(GeModelPtr ge_model, ModelTaskType task_type = ModelTaskType::MODEL_TASK_ALL_KERNEL,
                             ccKernelType kernel_type = ccKernelType::TE, std::string so_name = "") {
  domi::ModelTaskDef model_task_def;
  std::shared_ptr<domi::ModelTaskDef> model_task_def_ptr = make_shared<domi::ModelTaskDef>(model_task_def);
  domi::TaskDef *task_def = model_task_def_ptr->add_task();
  ge_model->SetModelTaskDef(model_task_def_ptr);

  auto aicore_task = std::unique_ptr<hybrid::AiCoreOpTask>(new (std::nothrow) hybrid::AiCoreOpTask());
  task_def->set_type(static_cast<uint32_t>(task_type));
  domi::KernelDefWithHandle *kernel_with_handle = task_def->mutable_kernel_with_handle();
  kernel_with_handle->set_original_kernel_key("");
  kernel_with_handle->set_node_info("");
  kernel_with_handle->set_block_dim(32);
  kernel_with_handle->set_args_size(64);
  domi::KernelDef *kernel_def = task_def->mutable_kernel();
  kernel_def->set_block_dim(32);
  kernel_def->set_args_size(64);
  kernel_def->set_so_name(so_name);
  string args(64, '1');
  kernel_with_handle->set_args(args.data(), 64);
  domi::KernelContext *context = kernel_with_handle->mutable_context();
  context->set_op_index(1);
  context->set_kernel_type(static_cast<uint32_t>(kernel_type));  // ccKernelType::TE
  uint16_t args_offset[9] = {0};
  context->set_args_offset(args_offset, 9 * sizeof(uint16_t));
}

static GeRootModelPtr ConstructGeRootModel(bool is_dynamic_shape = true,
                                           ModelTaskType task_type = ModelTaskType::MODEL_TASK_KERNEL,
                                           ccKernelType kernel_type = ccKernelType::TE, std::string so_name = "") {
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, is_dynamic_shape);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;
  ge_model->SetGraph(graph);
  (void)ge::AttrUtils::SetStr(ge_model, ATTR_MODEL_HOST_ENV_OS, "linux");
  (void)ge::AttrUtils::SetStr(ge_model, ATTR_MODEL_HOST_ENV_CPU, "x86_64");

  GeModelPtr ge_model1 = std::make_shared<GeModel>();
  ge::ComputeGraphPtr graph1 = std::make_shared<ge::ComputeGraph>("graph1");
  ge_model1->SetGraph(graph1);
  ge_root_model->subgraph_instance_name_to_model_["graph1"] = ge_model1;
  FillModelTaskDef(ge_model, task_type, kernel_type, so_name);
  FillModelTaskDef(ge_model1, task_type, kernel_type, so_name);
  return ge_root_model;
}
}  // namespace

class UtestModelHelper : public testing::Test {
 protected:
  void SetUp() override {
    old_global_options_ = GetThreadLocalContext().GetAllGlobalOptions();
  }

  void TearDown() override {
    GetThreadLocalContext().SetGlobalOption(old_global_options_);
  }

  void RunConcurrentLoadSameFingerprint(CustomOpSoLoader &loader, const OpSoBinPtr &so_bin,
                                        std::vector<CustomOpSoHandlePtr> &first_handles,
                                        std::vector<CustomOpSoHandlePtr> &second_handles, Status &first_status,
                                        Status &second_status) {
    ScopedCustomOpLoaderBlockingDlopenForUt dlopen_guard;
    std::thread first_thread([&]() { first_status = loader.LoadCustomOpSoBins({so_bin}, first_handles); });
    bool first_dlopen_started = false;
    {
      std::unique_lock<std::mutex> lock(g_custom_op_loader_dlopen_mutex);
      first_dlopen_started = g_custom_op_loader_dlopen_cv.wait_for(
          lock, std::chrono::seconds(5), [] { return g_custom_op_loader_dlopen_count.load() == 1U; });
    }
    if (!first_dlopen_started) {
      {
        std::lock_guard<std::mutex> lock(g_custom_op_loader_dlopen_mutex);
        g_custom_op_loader_release_dlopen = true;
      }
      g_custom_op_loader_dlopen_cv.notify_all();
      first_thread.join();
      ASSERT_TRUE(first_dlopen_started);
    }
    std::thread second_thread([&]() { second_status = loader.LoadCustomOpSoBins({so_bin}, second_handles); });
    bool both_dlopen_started = false;
    {
      std::unique_lock<std::mutex> lock(g_custom_op_loader_dlopen_mutex);
      both_dlopen_started = g_custom_op_loader_dlopen_cv.wait_for(
          lock, std::chrono::seconds(5), [] { return g_custom_op_loader_dlopen_count.load() == 2U; });
      g_custom_op_loader_release_dlopen = true;
    }
    g_custom_op_loader_dlopen_cv.notify_all();
    first_thread.join();
    second_thread.join();
    ASSERT_TRUE(both_dlopen_started);
  }

 private:
  std::map<std::string, std::string> old_global_options_;
};

TEST_F(UtestModelHelper, save_size_to_modeldef) {
  GeModelPtr ge_model = ge::MakeShared<ge::GeModel>();
  std::shared_ptr<domi::ModelTaskDef> task = ge::MakeShared<domi::ModelTaskDef>();
  ge_model->SetModelTaskDef(task);
  ModelHelper model_helper;
  EXPECT_EQ(SUCCESS, model_helper.SaveSizeToModelDef(ge_model, 1));
  EXPECT_EQ(SUCCESS, model_helper.SaveSizeToModelDef(ge_model, 0));
}

TEST_F(UtestModelHelper, SaveModelPartitionInvalid) {
  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelPartitionType type = MODEL_DEF;
  uint8_t data[128] = {0};
  size_t size = 10000000000;
  size_t model_index = 0;
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index),
            PARAM_INVALID);
  type = WEIGHTS_DATA;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index), SUCCESS);
  type = TASK_INFO;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index),
            PARAM_INVALID);
  type = TBE_KERNELS;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index), SUCCESS);
  type = CUST_AICPU_KERNELS;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index), SUCCESS);
  type = MODEL_INOUT_INFO;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index),
            PARAM_INVALID);
  type = (ModelPartitionType)100;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index),
            PARAM_INVALID);
  type = SO_BINS;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index),
            PARAM_INVALID);
  size = 1024;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, nullptr, size, model_index), PARAM_INVALID);
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index), SUCCESS);
  ASSERT_FALSE(om_file_save_helper->model_contexts_.empty());
  om_file_save_helper->model_contexts_[0U].model_data_len_ = 4000000000U;
  model_index = 2000000000U;
  EXPECT_EQ(model_helper.SaveModelPartition(om_file_save_helper, type, (uint8_t *)data, size, model_index),
            PARAM_INVALID);
}

TEST_F(UtestModelHelper, SaveOriginalGraphToOmModel) {
  Graph graph("graph");
  std::string output_file = "";
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.SaveOriginalGraphToOmModel(graph, output_file), FAILED);
  output_file = "output.graph";
  EXPECT_EQ(model_helper.SaveOriginalGraphToOmModel(graph, output_file), FAILED);
  ge::OpDescPtr add_op(new ge::OpDesc("add1", "Add"));
  add_op->AddDynamicInputDesc("input", 2);
  add_op->AddDynamicOutputDesc("output", 1);
  std::shared_ptr<ge::ComputeGraph> compute_graph(new ge::ComputeGraph("test_graph"));
  auto add_node = compute_graph->AddNode(add_op);
  auto graph2 = ge::GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);
  EXPECT_EQ(model_helper.SaveOriginalGraphToOmModel(graph2, output_file), SUCCESS);
}

TEST_F(UtestModelHelper, GetGeModel) {
  ModelHelper model_helper;
  model_helper.model_ = nullptr;
  EXPECT_NE(model_helper.GetGeModel(), nullptr);
}

TEST_F(UtestModelHelper, LoadTask) {
  ModelHelper model_helper;
  OmFileLoadHelper om_load_helper;
  GeModelPtr cur_model = std::make_shared<GeModel>();
  size_t mode_index = 10;
  EXPECT_EQ(model_helper.LoadTask(om_load_helper, cur_model, mode_index), FAILED);
}

TEST_F(UtestModelHelper, LoadTaskByHelper) {
  ModelHelper model_helper;
  OmFileLoadHelper om_load_helper;
  om_load_helper.is_inited_ = false;
  EXPECT_EQ(model_helper.LoadTask(om_load_helper, model_helper.model_, 0U), FAILED);
  om_load_helper.is_inited_ = true;
  ModelPartition mp;
  mp.type = TASK_INFO;
  om_load_helper.model_contexts_.emplace_back(OmFileContext{});
  om_load_helper.model_contexts_[0U].partition_datas_.push_back(mp);
  model_helper.model_ = std::make_shared<GeModel>();
  EXPECT_EQ(model_helper.LoadTask(om_load_helper, model_helper.model_, 0U), SUCCESS);
}

TEST_F(UtestModelHelper, SaveToOmRootModel) {
  ModelHelper model_helper;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ge::ComputeGraphPtr subgraph = std::make_shared<ge::ComputeGraph>("subgraph");
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge_model->SetGraph(subgraph);
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;
  std::string output_file = "output.file";
  ModelBufferData model;
  bool is_unknown_shape = true;
  EXPECT_NE(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, is_unknown_shape), SUCCESS);
  ge::ComputeGraphPtr subgraph2 = std::make_shared<ge::ComputeGraph>("subgraph2");
  GeModelPtr ge_model2 = std::make_shared<GeModel>();
  ge_root_model->subgraph_instance_name_to_model_["graph2"] = ge_model2;
  EXPECT_NE(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, is_unknown_shape), SUCCESS);
}

TEST_F(UtestModelHelper, SaveModelDef) {
  ModelHelper model_helper;
  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge::Buffer model_buffer;
  model_buffer.impl_->buffer_ = nullptr;
  size_t model_index = 0;
  EXPECT_NE(model_helper.SaveModelDef(om_file_save_helper, ge_model, model_buffer, model_index), SUCCESS);
}

TEST_F(UtestModelHelper, SaveAllModelPartiton) {
  ModelHelper model_helper;
  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge::Buffer model_buffer;
  model_buffer.impl_->buffer_ = nullptr;
  ge::Buffer task_buffer;
  size_t model_index = 0;
  EXPECT_EQ(model_helper.SaveAllModelPartiton(om_file_save_helper, ge_model, model_buffer, task_buffer, model_index),
            FAILED);
}

TEST_F(UtestModelHelper, SaveToOmModel) {
  ModelHelper model_helper;
  GeModelPtr ge_model = std::make_shared<GeModel>();
  std::string output_file = "";
  ModelBufferData model;
  EXPECT_EQ(model_helper.SaveToOmModel(ge_model, output_file, model), FAILED);
}

TEST_F(UtestModelHelper, LoadFromFile_failed) {
  ModelParserBase base;
  ge::ModelData model_data;
  EXPECT_EQ(base.LoadFromFile("/tmp/123test", -1, model_data), ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID);
}

TEST_F(UtestModelHelper, SaveModelIntroduction) {
  ModelHelper model_helper;
  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  GeModelPtr ge_model = std::make_shared<GeModel>();
  size_t model_index = 0U;
  EXPECT_NE(model_helper.SaveModelIntroduction(om_file_save_helper, ge_model, model_index), SUCCESS);
}

TEST_F(UtestModelHelper, LoadPartInfoFromModel) {
  ModelHelper model_helper;
  ModelPartition partition;
  ModelBufferData model_buffer;
  ModelData model_data{model_buffer.data.get(), static_cast<uint32_t>(model_buffer.length), 0, "", "/tmp/a"};
  EXPECT_EQ(model_helper.LoadPartInfoFromModel(model_data, partition), ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID);
}

TEST_F(UtestModelHelper, GetWeightDataTest) {
  GeModel ge_model;
  uint8_t i = 0;
  DataBuffer weight_buf(&i, sizeof(i));
  ge_model.SetWeightDataBuf(weight_buf);
  const ge::Buffer weight = ge::Buffer::CopyFrom(&i, sizeof(i));
  ge_model.SetWeight(weight);
  EXPECT_TRUE((ge_model.GetWeightData() == &i));
  ge_model.ClearWeightDataBuf();
  EXPECT_FALSE((ge_model.GetWeightData() == &i));
}

TEST_F(UtestModelHelper, CheckOsCpuInfoAndOppVersion) {
  auto paths = gert::CreateSceneInfo();
  auto path = paths[0];
  system(("realpath " + path).c_str());

  ModelHelper model_helper;
  std::vector<char> data;
  data.resize(256);
  ModelFileHeader *file_header = reinterpret_cast<ModelFileHeader *>(data.data());
  file_header->need_check_os_cpu_info = static_cast<uint8_t>(OsCpuInfoCheckTyep::NEED_CHECK);
  model_helper.file_header_ = file_header;

  std::string host_env_os = "linux";
  std::string host_env_cpu = "x86_64";
  model_helper.model_ = std::make_shared<GeModel>();
  ge::AttrUtils::SetStr(*(model_helper.model_.get()), "host_env_os", host_env_os);
  ge::AttrUtils::SetStr(*(model_helper.model_.get()), "host_env_cpu", host_env_cpu);
  model_helper.root_model_ = std::make_shared<GeRootModel>();
  ASSERT_EQ(model_helper.CheckOsCpuInfoAndOppVersion(), SUCCESS);

  system(("rm -f " + path).c_str());
}

TEST_F(UtestModelHelper, SoBinSaveToOmRootModel) {
  ModelHelper model_helper;  // 默认为离线模型
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge::ComputeGraphPtr subgraph = std::make_shared<ge::ComputeGraph>("subgraph");
  ge_model->SetGraph(subgraph);
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;
  ge_model->SetGraph(graph);
  FillModelTaskDef(ge_model);

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  mmSetEnv(kEnvName, opp_path.c_str(), 1);

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_proto_path).c_str());
  inner_proto_path += kOpsProto;
  system(("touch " + inner_proto_path).c_str());
  system(("echo 'ops proto:123 ' > " + inner_proto_path).c_str());

  std::string inner_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_tiling_path).c_str());
  inner_tiling_path += kOpMaster;
  system(("touch " + inner_tiling_path).c_str());
  system(("echo 'op tiling:456 ' > " + inner_tiling_path).c_str());

  string cpu_info = "x86_64";
  string os_info = "linux";
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0x8000);
  EXPECT_EQ(model_helper.GetSoBinData(cpu_info, os_info), SUCCESS);

  std::string output_file = "outputfile.om";
  ModelBufferData model;
  bool is_unknown_shape = true;
  std::map<std::string, std::string> options_map;
  options_map["ge.host_env_os"] = os_info;
  options_map["ge.host_env_cpu"] = cpu_info;
  GetThreadLocalContext().SetGlobalOption(options_map);

  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, is_unknown_shape), SUCCESS);

  output_file += "_linux_x86_64.om";
  system(("rm -rf " + path_vendors).c_str());
  system(("rm -rf " + output_file).c_str());
  system(("rm -rf " + opp_path + kInner).c_str());
}

TEST_F(UtestModelHelper, RepackSoToOm) {
  ModelHelper model_helper;  // 默认为离线模型
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ge::ComputeGraphPtr subgraph = std::make_shared<ge::ComputeGraph>("subgraph");
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge_model->SetGraph(subgraph);
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;
  ge_model->SetGraph(graph);

  GeModelPtr ge_model1 = std::make_shared<GeModel>();
  ge::ComputeGraphPtr graph1 = std::make_shared<ge::ComputeGraph>("graph1");
  ge_model1->SetGraph(graph1);
  ge_root_model->subgraph_instance_name_to_model_["graph1"] = ge_model1;

  {
    domi::ModelTaskDef model_task_def;
    std::shared_ptr<domi::ModelTaskDef> model_task_def_ptr = make_shared<domi::ModelTaskDef>(model_task_def);
    domi::TaskDef *task_def = model_task_def_ptr->add_task();
    ge_model->SetModelTaskDef(model_task_def_ptr);
    ge_model1->SetModelTaskDef(model_task_def_ptr);

    auto aicore_task = std::unique_ptr<hybrid::AiCoreOpTask>(new (std::nothrow) hybrid::AiCoreOpTask());
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_ALL_KERNEL));
    domi::KernelDefWithHandle *kernel_with_handle = task_def->mutable_kernel_with_handle();
    kernel_with_handle->set_block_dim(32);
    kernel_with_handle->set_args_size(64);
    kernel_with_handle->set_original_kernel_key("");
    kernel_with_handle->set_node_info("");

    string args(64, '1');
    uint16_t args_offset[9] = {0};
    kernel_with_handle->set_args(args.data(), 64);
    domi::KernelContext *context = kernel_with_handle->mutable_context();
    context->set_kernel_type(2);
    context->set_op_index(1);
    context->set_args_offset(args_offset, 9 * sizeof(uint16_t));
  }

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  mmSetEnv(kEnvName, opp_path.c_str(), 1);

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_proto_path).c_str());
  inner_proto_path += kOpsProto;
  system(("touch " + inner_proto_path).c_str());
  system(("echo 'ops proto:123 ' > " + inner_proto_path).c_str());

  std::string inner_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_tiling_path).c_str());
  inner_tiling_path += kOpMaster;
  system(("touch " + inner_tiling_path).c_str());
  system(("echo 'op tiling:456 ' > " + inner_tiling_path).c_str());

  string cpu_info = "x86_64";
  string os_info = "linux";
  std::map<std::string, std::string> options_map;
  options_map["ge.host_env_os"] = os_info;
  options_map["ge.host_env_cpu"] = cpu_info;
  GetThreadLocalContext().SetGlobalOption(options_map);

  std::string output_file = "outputfile.om";
  {
    ModelBufferData model;
    ModelHelper model_helper;
    model_helper.SetSaveMode(false);
    GeRootModelPtr ge_root_model = ConstructGeRootModel();
    EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, true), SUCCESS);

    ModelData model_data;
    model_data.model_data = model.data.get();
    model_data.model_len = model.length;
    model_helper.SetRepackSoFlag(true);
    ModelBufferData buffer;
    ASSERT_EQ(model_helper.LoadRootModel(model_data), SUCCESS);
    EXPECT_EQ(model_helper.PackSoToModelData(model_data, output_file, buffer), SUCCESS);
    system("rm -rf outputfile_linux_x86_64.om");
  }

  {
    ModelBufferData model;
    ModelHelper model_helper;
    model_helper.SetSaveMode(false);
    GeRootModelPtr ge_root_model = ConstructGeRootModel(false);
    EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, false), SUCCESS);

    ModelData model_data;
    model_data.model_data = model.data.get();
    model_data.model_len = model.length;
    model_helper.SetRepackSoFlag(true);
    ModelBufferData buffer;
    EXPECT_EQ(model_helper.LoadRootModel(model_data), SUCCESS);
    EXPECT_EQ(model_helper.PackSoToModelData(model_data, output_file, buffer), SUCCESS);
    system(("rm -rf " + output_file).c_str());
  }
  system(("rm -rf " + path_vendors).c_str());
  system(("rm -rf " + opp_path + kInner).c_str());
}

TEST_F(UtestModelHelper, RepackSoToOmWithZeroCopyAddr) {
  ModelHelper helper;
  uint8_t data[64];
  ModelData model_data;
  model_data.model_data = data;
  model_data.model_len = 64;
  GeRootModelPtr ge_root_model = ConstructGeRootModel(false);
  ASSERT_NE(ge_root_model, nullptr);
  helper.root_model_ = ge_root_model;
  ModelBufferData buffer_data;
  EXPECT_EQ(helper.PackSoToModelData(model_data, "./tmp", buffer_data, false), SUCCESS);

  EXPECT_EQ(buffer_data.data.get(), data);
  EXPECT_EQ(buffer_data.length, 64);
}

TEST_F(UtestModelHelper, LoadRootModel_GetFileConstantWeightDirOK) {
  ModelBufferData model;
  ModelHelper model_helper;
  model_helper.SetRepackSoFlag(true);
  model_helper.SetSaveMode(true);
  GeRootModelPtr ge_root_model = ConstructGeRootModel(false);
  std::string output_file = "./outputfile.om";
  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, false), SUCCESS);

  model_helper.SetSaveMode(false);
  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, true), SUCCESS);
  ModelData model_data;
  model_data.model_data = model.data.get();
  model_data.model_len = model.length;
  model_data.om_path = output_file;
  EXPECT_EQ(model_helper.LoadRootModel(model_data), SUCCESS);
  const auto root_model = model_helper.GetGeRootModel();
  ASSERT_NE(root_model, nullptr);
  std::string real_om_path = ge::RealPath(output_file.c_str());
  ASSERT_TRUE(!real_om_path.empty());
  std::string weight_path_expected = real_om_path.substr(0, real_om_path.rfind("/") + 1) + "weight/";
  EXPECT_EQ(root_model->GetFileConstantWeightDir(), weight_path_expected);
  system(("rm -rf " + output_file).c_str());
}

TEST_F(UtestModelHelper, LoadRootModel_GetFileConstantWeightDirWithWrongWeightPath) {
  ModelBufferData model;
  ModelHelper model_helper;
  model_helper.SetRepackSoFlag(true);
  model_helper.SetSaveMode(true);
  GeRootModelPtr ge_root_model = ConstructGeRootModel(false);
  std::string output_file = "./outputfile.om";
  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, false), SUCCESS);

  model_helper.SetSaveMode(false);
  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, true), SUCCESS);
  ModelData model_data;
  model_data.model_data = model.data.get();
  model_data.model_len = model.length;
  model_data.weight_path = "/home";
  EXPECT_NE(model_helper.LoadRootModel(model_data), SUCCESS);
  system(("rm -rf " + output_file).c_str());
}

TEST_F(UtestModelHelper, SaveOutNodesFromRootGraph) {
  ModelHelper model_helper;  // 默认为离线模型
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ge::ComputeGraphPtr subgraph1 = std::make_shared<ge::ComputeGraph>("subgraph1");
  GeModelPtr ge_model1 = std::make_shared<GeModel>();
  ge_model1->SetGraph(subgraph1);
  ge_root_model->subgraph_instance_name_to_model_["subgraph1"] = ge_model1;

  ge::ComputeGraphPtr subgraph2 = std::make_shared<ge::ComputeGraph>("subgraph2");
  GeModelPtr ge_model2 = std::make_shared<GeModel>();
  ge_model2->SetGraph(subgraph2);
  ge_root_model->subgraph_instance_name_to_model_["subgraph2"] = ge_model2;

  GeModelPtr first_ge_model = std::make_shared<GeModel>();
  model_helper.SaveOutNodesFromRootGraph(ge_root_model, first_ge_model);
  std::vector<std::string> out_node_name_get;
  (void)ge::AttrUtils::GetListStr(first_ge_model, ge::ATTR_MODEL_OUT_NODES_NAME, out_node_name_get);
  EXPECT_TRUE(out_node_name_get.empty());

  std::vector<std::string> out_node_name_set;
  out_node_name_set.emplace_back("test");
  (void)ge::AttrUtils::SetListStr(ge_model1, ge::ATTR_MODEL_OUT_NODES_NAME, out_node_name_set);

  model_helper.SaveOutNodesFromRootGraph(ge_root_model, first_ge_model);
  (void)ge::AttrUtils::GetListStr(first_ge_model, ge::ATTR_MODEL_OUT_NODES_NAME, out_node_name_get);
  EXPECT_TRUE(!out_node_name_get.empty());
}

TEST_F(UtestModelHelper, SoBinSaveToOmModel) {
  ModelHelper model_helper;  // 默认为离线模型
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  ge::ComputeGraphPtr subgraph = std::make_shared<ge::ComputeGraph>("subgraph");
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge_model->SetGraph(subgraph);
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;
  ge_model->SetGraph(graph);

  {
    domi::ModelTaskDef model_task_def;
    std::shared_ptr<domi::ModelTaskDef> model_task_def_ptr = make_shared<domi::ModelTaskDef>(model_task_def);
    domi::TaskDef *task_def = model_task_def_ptr->add_task();
    ge_model->SetModelTaskDef(model_task_def_ptr);

    auto aicore_task = std::unique_ptr<hybrid::AiCoreOpTask>(new (std::nothrow) hybrid::AiCoreOpTask());
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_ALL_KERNEL));
    domi::KernelDefWithHandle *kernel_with_handle = task_def->mutable_kernel_with_handle();
    kernel_with_handle->set_original_kernel_key("");
    kernel_with_handle->set_node_info("");
    kernel_with_handle->set_block_dim(32);
    kernel_with_handle->set_args_size(64);
    string args(64, '1');
    kernel_with_handle->set_args(args.data(), 64);
    domi::KernelContext *context = kernel_with_handle->mutable_context();
    context->set_op_index(1);
    context->set_kernel_type(2);  // ccKernelType::TE
    uint16_t args_offset[9] = {0};
    context->set_args_offset(args_offset, 9 * sizeof(uint16_t));
  }

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  mmSetEnv(kEnvName, opp_path.c_str(), 1);

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_proto_path).c_str());
  inner_proto_path += kOpsProto;
  system(("touch " + inner_proto_path).c_str());
  system(("echo 'ops proto:123 ' > " + inner_proto_path).c_str());

  std::string inner_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_tiling_path).c_str());
  inner_tiling_path += kOpMaster;
  system(("touch " + inner_tiling_path).c_str());
  system(("echo 'op tiling:456 ' > " + inner_tiling_path).c_str());

  string cpu_info = "x86_64";
  string os_info = "linux";
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0x8000);
  EXPECT_EQ(model_helper.GetSoBinData(cpu_info, os_info), SUCCESS);

  std::string output_file = "static_outputfile.om";
  ModelBufferData model;
  bool is_unknown_shape = false;

  std::map<std::string, std::string> options_map;
  options_map["ge.host_env_os"] = os_info;
  options_map["ge.host_env_cpu"] = cpu_info;
  GetThreadLocalContext().SetGlobalOption(options_map);

  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, is_unknown_shape), SUCCESS);

  output_file += "_linux_x86_64.om";
  system(("rm -rf " + path_vendors).c_str());
  system(("rm -rf " + inner_proto_path).c_str());
  system(("rm -rf " + output_file).c_str());
}

TEST_F(UtestModelHelper, LoadOpSoBinSuccess) {
  OmFileLoadHelper load_helper;
  ModelHelper model_helper;  // 默认为离线
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);

  load_helper.is_inited_ = true;
  OmFileContext cur_ctx;
  ModelPartition so_patition;
  so_patition.type = ModelPartitionType::SO_BINS;
  std::vector<uint8_t> so_payload;
  const auto test_so_bin = BuildOpSoBinForModelHelperUt("libload_op_so_bin_success.so", "vendor_ut", {0x1U, 0x2U, 0x3U},
                                                        SoBinType::kAutofuse);
  ASSERT_NE(test_so_bin, nullptr);
  ASSERT_TRUE(BuildSoBinsPayloadForModelHelperUt({test_so_bin}, so_payload));
  so_patition.data = so_payload.data();
  so_patition.size = so_payload.size();
  cur_ctx.partition_datas_.push_back(so_patition);
  load_helper.model_contexts_.push_back(cur_ctx);
  std::vector<CustomOpSoHandlePtr> loaded_handles;
  EXPECT_EQ(model_helper.LoadOpSoBin(load_helper, ge_root_model, loaded_handles), SUCCESS);
}

TEST_F(UtestModelHelper, LoadTilingDataSuccess) {
  OmFileLoadHelper load_helper;
  ModelHelper model_helper;  // 默认为离线
  load_helper.is_inited_ = true;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);

  auto op_desc = std::make_shared<OpDesc>("Test", "Test");
  auto node = graph->AddNode(op_desc);
  ASSERT_TRUE(node != nullptr);

  std::string tiling_data = "hahahaha";
  std::shared_ptr<optiling::utils::OpRunInfo> add_run_info = std::make_shared<optiling::utils::OpRunInfo>();
  add_run_info->AddTilingData(tiling_data.data(), tiling_data.size());
  node->GetOpDescBarePtr()->SetExtAttr(ATTR_NAME_OP_RUN_INFO, add_run_info);

  HostResourceCenter center = HostResourceCenter();
  center.TakeOverHostResources(graph);

  uint8_t *data{data};
  std::size_t tiling_size;
  HostResourceSerializer serializer;
  ASSERT_EQ(serializer.SerializeTilingData(center, data, tiling_size), SUCCESS);

  OmFileContext cur_ctx;
  ModelPartition tiling_partition;
  tiling_partition.type = ModelPartitionType::TILING_DATA;
  tiling_partition.data = reinterpret_cast<uint8_t *>(data);
  tiling_partition.size = tiling_size;
  cur_ctx.partition_datas_.push_back(tiling_partition);
  load_helper.model_contexts_.push_back(cur_ctx);
  EXPECT_EQ(model_helper.LoadTilingData(load_helper, ge_root_model), SUCCESS);
}

TEST_F(UtestModelHelper, LoadCustomOpsPartitionMissingSuccess) {
  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;
  load_helper.model_contexts_.emplace_back(OmFileContext{});

  ModelHelper model_helper;
  EXPECT_EQ(model_helper.LoadCustomOps(load_helper, nullptr), SUCCESS);
}

TEST_F(UtestModelHelper, LoadCustomOpsEmptyPartitionSuccess) {
  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;

  OmFileContext cur_ctx;
  ModelPartition custom_partition;
  custom_partition.type = ModelPartitionType::CUSTOM_OPS;
  custom_partition.data = nullptr;
  custom_partition.size = 0U;
  cur_ctx.partition_datas_.push_back(custom_partition);
  load_helper.model_contexts_.push_back(cur_ctx);

  ModelHelper model_helper;
  EXPECT_EQ(model_helper.LoadCustomOps(load_helper, nullptr), SUCCESS);
}

TEST_F(UtestModelHelper, GeRootModelCustomOpRegistryShouldBeSharedByFork) {
  auto registry = std::make_shared<CustomOpRegistry>();
  GeRootModel root_model;
  root_model.SetCustomOpRegistry(registry);

  EXPECT_EQ(root_model.GetCustomOpRegistry(), registry);
  const auto forked_model = root_model.Fork();
  ASSERT_NE(forked_model, nullptr);
  EXPECT_EQ(forked_model->GetCustomOpRegistry(), registry);
}

TEST_F(UtestModelHelper, LoadCustomOpsWithRegistryShouldNotUseGlobalFactory) {
  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;

  const auto payload = BuildExpectedCustomOpsDataForModelHelperUt(kRegistryOnlyPortableOpTypeForModelHelper,
                                                                  kPortableKernelBinForModelHelper);
  OmFileContext cur_ctx;
  ModelPartition custom_partition;
  custom_partition.type = ModelPartitionType::CUSTOM_OPS;
  custom_partition.data = payload.data();
  custom_partition.size = payload.size();
  cur_ctx.partition_datas_.push_back(custom_partition);
  load_helper.model_contexts_.push_back(cur_ctx);

  auto registry = std::make_shared<CustomOpRegistry>();
  ASSERT_EQ(registry->RegisterCreator(
                kRegistryOnlyPortableOpTypeForModelHelper,
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<ModelHelperPortableOpForUt>(); }),
            GRAPH_SUCCESS);
  ASSERT_FALSE(CustomOpFactory::IsExistOp(kRegistryOnlyPortableOpTypeForModelHelper));

  ModelHelper model_helper;
  EXPECT_EQ(model_helper.LoadCustomOps(load_helper, registry), SUCCESS);
  EXPECT_TRUE(registry->HasCustomOp(kRegistryOnlyPortableOpTypeForModelHelper));
  EXPECT_FALSE(CustomOpFactory::IsExistOp(kRegistryOnlyPortableOpTypeForModelHelper));
}

TEST_F(UtestModelHelper, LoadCustomOpRegistryShouldCreateEmptyModelRegistryWhenNoSoBinsOrCustomOps) {
  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;
  load_helper.model_contexts_.emplace_back(OmFileContext{});

  auto graph = std::make_shared<ComputeGraph>("task5_no_custom_op_registry_graph");
  auto ge_root_model = std::make_shared<GeRootModel>();
  ASSERT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ASSERT_EQ(ge_root_model->GetCustomOpRegistry(), nullptr);

  ModelHelper model_helper;
  EXPECT_EQ(model_helper.LoadCustomOpRegistry(load_helper, ge_root_model), SUCCESS);
  ASSERT_NE(ge_root_model->GetCustomOpRegistry(), nullptr);
  EXPECT_NE(ge_root_model->GetCustomOpRegistry(), CustomOpFactory::GetGlobalRegistryPtr());
}

TEST_F(UtestModelHelper, LoadCustomOpRegistryShouldFailWhenCustomOpsExistWithoutSoBins) {
  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;

  const auto payload = BuildExpectedCustomOpsDataForModelHelperUt(kRegistryOnlyPortableOpTypeForModelHelper,
                                                                  kPortableKernelBinForModelHelper);
  OmFileContext cur_ctx;
  ModelPartition custom_partition;
  custom_partition.type = ModelPartitionType::CUSTOM_OPS;
  custom_partition.data = payload.data();
  custom_partition.size = payload.size();
  cur_ctx.partition_datas_.push_back(custom_partition);
  load_helper.model_contexts_.push_back(cur_ctx);

  auto graph = std::make_shared<ComputeGraph>("task5_custom_ops_without_so_bins_graph");
  auto ge_root_model = std::make_shared<GeRootModel>();
  ASSERT_EQ(ge_root_model->Initialize(graph), SUCCESS);

  ModelHelper model_helper;
  EXPECT_NE(model_helper.LoadCustomOpRegistry(load_helper, ge_root_model), SUCCESS);
  EXPECT_EQ(ge_root_model->GetCustomOpRegistry(), nullptr);
}

TEST_F(UtestModelHelper, LoadCustomOpRegistryShouldFailAndReleaseLeaseWhenPullSymbolsMissing) {
  CustomOpSoLoader::GetInstance().Cleanup();
  std::string source_so_path;
  ASSERT_TRUE(FindNotLoadedSystemSoForModelHelperUt(source_so_path));
  std::vector<char_t> so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(source_so_path, so_data));
  const auto custom_op_so_bin =
      BuildCustomOpSoBinForModelHelperUt("libtask5_missing_pull_symbols.so", "vendor_ut", so_data);
  ASSERT_NE(custom_op_so_bin, nullptr);

  std::vector<uint8_t> so_payload;
  ASSERT_TRUE(BuildSoBinsPayloadForModelHelperUt({custom_op_so_bin}, so_payload));
  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;
  OmFileContext cur_ctx;
  ModelPartition so_partition;
  so_partition.type = ModelPartitionType::SO_BINS;
  so_partition.data = so_payload.data();
  so_partition.size = so_payload.size();
  cur_ctx.partition_datas_.push_back(so_partition);
  load_helper.model_contexts_.push_back(cur_ctx);

  auto graph = std::make_shared<ComputeGraph>("task5_missing_pull_symbols_graph");
  auto ge_root_model = std::make_shared<GeRootModel>();
  ASSERT_EQ(ge_root_model->Initialize(graph), SUCCESS);

  ModelHelper model_helper;
  EXPECT_NE(model_helper.LoadCustomOpRegistry(load_helper, ge_root_model), SUCCESS);
  EXPECT_EQ(ge_root_model->GetCustomOpRegistry(), nullptr);
  ASSERT_EQ(CustomOpSoLoader::GetInstance().loaded_states_.size(), 1U);
  EXPECT_TRUE(CustomOpSoLoader::GetInstance().loaded_states_.begin()->second.expired());
  CustomOpSoLoader::GetInstance().Cleanup();
}

TEST_F(UtestModelHelper, ValidateCustomOpsDeserializedFailsWhenUsedCreatorHasNoInstance) {
  const auto ge_root_model = CreateGeRootModelForModelHelperUt(kRegistryOnlyPortableOpTypeForModelHelper, "");
  ASSERT_NE(ge_root_model, nullptr);
  auto registry = std::make_shared<CustomOpRegistry>();
  ASSERT_EQ(registry->RegisterCreator(
                kRegistryOnlyPortableOpTypeForModelHelper,
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<ModelHelperPortableOpForUt>(); }),
            GRAPH_SUCCESS);

  ModelHelper model_helper;
  EXPECT_NE(model_helper.ValidateCustomOpsDeserialized(ge_root_model, registry), SUCCESS);
}

TEST_F(UtestModelHelper, ValidateCustomOpsDeserializedSucceedsWhenUsedCreatorHasInstance) {
  const auto ge_root_model = CreateGeRootModelForModelHelperUt(kRegistryOnlyPortableOpTypeForModelHelper, "");
  ASSERT_NE(ge_root_model, nullptr);
  auto registry = std::make_shared<CustomOpRegistry>();
  ASSERT_EQ(registry->RegisterCreator(
                kRegistryOnlyPortableOpTypeForModelHelper,
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<ModelHelperPortableOpForUt>(); }),
            GRAPH_SUCCESS);
  ASSERT_NE(registry->CreateOrGetCustomOp(kRegistryOnlyPortableOpTypeForModelHelper), nullptr);

  ModelHelper model_helper;
  EXPECT_EQ(model_helper.ValidateCustomOpsDeserialized(ge_root_model, registry), SUCCESS);
}

TEST_F(UtestModelHelper, SerializeCustomOpKernelNullPortableOpReturnsFailed) {
  ModelHelper model_helper;
  std::vector<uint8_t> merged_kernels = {0xB0U};
  EXPECT_EQ(model_helper.SerializeCustomOpKernel(nullptr, kPortableOpTypeForModelHelper, merged_kernels), FAILED);
  EXPECT_EQ(merged_kernels, std::vector<uint8_t>({0xB0U}));
}

TEST_F(UtestModelHelper, SerializeCustomOpKernelPortableOpSerializeFailedPropagates) {
  ModelHelper model_helper;
  std::vector<uint8_t> merged_kernels = {0xCDU};
  ModelHelperPortableOpSerializeFailForUt portable_op;
  EXPECT_EQ(
      model_helper.SerializeCustomOpKernel(&portable_op, kPortableOpSerializeFailTypeForModelHelper, merged_kernels),
      GRAPH_FAILED);
  EXPECT_EQ(merged_kernels, std::vector<uint8_t>({0xCDU}));
}

TEST_F(UtestModelHelper, SerializeCustomOpKernelPortableOpSerializeEmptyReturnsFailed) {
  ModelHelper model_helper;
  std::vector<uint8_t> merged_kernels = {0xDEU};
  ModelHelperPortableOpEmptyForUt portable_op;
  EXPECT_NE(model_helper.SerializeCustomOpKernel(&portable_op, kPortableOpEmptyTypeForModelHelper, merged_kernels),
            SUCCESS);
  EXPECT_EQ(merged_kernels, std::vector<uint8_t>({0xDEU}));
}

TEST_F(UtestModelHelper, SerializeCustomOpKernelPortableOpSerializeSuccessAppendsKernelHeaderAndPayload) {
  ModelHelper model_helper;
  std::vector<uint8_t> merged_kernels = {0xEFU};
  const auto old_size = merged_kernels.size();
  ModelHelperPortableOpForUt portable_op;
  EXPECT_EQ(model_helper.SerializeCustomOpKernel(&portable_op, kPortableOpTypeForModelHelper, merged_kernels), SUCCESS);

  const auto expected_size = old_size + sizeof(CustomKernelItemHeader) + strlen(kPortableOpTypeForModelHelper) +
                             kPortableKernelBinForModelHelper.size();
  ASSERT_EQ(merged_kernels.size(), expected_size);

  CustomKernelItemHeader header{};
  std::copy_n(merged_kernels.data() + old_size, sizeof(CustomKernelItemHeader), reinterpret_cast<uint8_t *>(&header));
  EXPECT_EQ(header.magic, kCustomKernelItemMagic);
  EXPECT_EQ(header.name_len, strlen(kPortableOpTypeForModelHelper));
  EXPECT_EQ(header.bin_len, kPortableKernelBinForModelHelper.size());

  const size_t name_offset = old_size + sizeof(CustomKernelItemHeader);
  const std::string kernel_name(reinterpret_cast<const char *>(merged_kernels.data() + name_offset), header.name_len);
  EXPECT_EQ(kernel_name, kPortableOpTypeForModelHelper);

  const size_t bin_offset = name_offset + header.name_len;
  const std::vector<uint8_t> kernel_bin(merged_kernels.begin() + static_cast<ptrdiff_t>(bin_offset),
                                        merged_kernels.begin() + static_cast<ptrdiff_t>(bin_offset + header.bin_len));
  EXPECT_EQ(kernel_bin, kPortableKernelBinForModelHelper);
}

TEST_F(UtestModelHelper, SaveCustomOpsPartitionPortableOpsWithoutKernelBinReturnsFailed) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpEmptyTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpEmptyForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpEmptyTypeForModelHelper, kPortableOpEmptyTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelHelper model_helper;
  EXPECT_NE(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);
  EXPECT_TRUE(om_file_save_helper->model_contexts_.empty());
}

TEST_F(UtestModelHelper, SaveCustomOpsPartitionAnyPortableOpWithoutKernelBinReturnsFailed) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpEmptyTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpEmptyForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpEmptyTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelHelper model_helper;
  EXPECT_NE(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);
}

TEST_F(UtestModelHelper, SaveCustomOpsPartitionSerializeFailedReturnsFailed) {
  RegisterCustomOpCreatorForModelHelperUt(
      kPortableOpSerializeFailTypeForModelHelper,
      []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<ModelHelperPortableOpSerializeFailForUt>(); });
  const auto ge_root_model = CreateGeRootModelForModelHelperUt(kPortableOpSerializeFailTypeForModelHelper,
                                                               kPortableOpSerializeFailTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelHelper model_helper;
  EXPECT_NE(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);
}

TEST_F(UtestModelHelper, SaveCustomOpsPartitionCreateCustomOpFailedReturnsFailed) {
  RegisterCustomOpCreatorForModelHelperUt(kUnregisteredPortableOpTypeForModelHelper,
                                          []() -> std::unique_ptr<BaseCustomOp> { return nullptr; });

  const auto ge_root_model = CreateGeRootModelForModelHelperUt(kUnregisteredPortableOpTypeForModelHelper,
                                                               kUnregisteredPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), FAILED);
}

TEST_F(UtestModelHelper, SaveCustomOpsPartitionHasSerializablePortableOpsSavesPartition) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);

  ModelPartition custom_partition;
  ASSERT_TRUE(FindCustomOpsPartitionForModelHelperUt(om_file_save_helper, custom_partition));
  ASSERT_FALSE(om_file_save_helper->model_contexts_.empty());
  ASSERT_EQ(om_file_save_helper->model_contexts_[0U].owned_partitions_.size(), 1U);
  EXPECT_EQ(custom_partition.data, om_file_save_helper->model_contexts_[0U].owned_partitions_[0U]->data());

  const auto expected_partition_size =
      sizeof(CustomKernelItemHeader) + strlen(kPortableOpTypeForModelHelper) + kPortableKernelBinForModelHelper.size();
  ASSERT_EQ(custom_partition.size, expected_partition_size);
}

TEST_F(UtestModelHelper, SaveCustomOpsPartitionSaveModelToBufferBytesStable) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelHelper model_helper;
  ASSERT_EQ(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);
  ASSERT_FALSE(om_file_save_helper->model_contexts_.empty());
  ASSERT_EQ(om_file_save_helper->model_contexts_[0U].owned_partitions_.size(), 1U);

  const std::vector<uint8_t> expected_partition =
      BuildExpectedCustomOpsDataForModelHelperUt(kPortableOpTypeForModelHelper, kPortableKernelBinForModelHelper);

  // Overwrite allocator cache to stress payload lifetime before delayed SaveModel serialization.
  std::vector<uint8_t> memory_noise(expected_partition.size(), 0xABU);
  ASSERT_FALSE(memory_noise.empty());

  ModelBufferData model_buffer;
  ASSERT_EQ(om_file_save_helper->SaveModel("unused.om", model_buffer, false), SUCCESS);
  ASSERT_NE(model_buffer.data, nullptr);
  ASSERT_GT(model_buffer.length, 0U);
  ASSERT_GE(model_buffer.length, expected_partition.size());
  const auto *tail = model_buffer.data.get() + (model_buffer.length - expected_partition.size());
  const std::vector<uint8_t> loaded_data(tail, tail + expected_partition.size());
  EXPECT_EQ(loaded_data, expected_partition);
}

TEST_F(UtestModelHelper, SaveCustomOpsPartitionPortableAndNonPortableFails) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  RegisterCustomOpCreatorForModelHelperUt(kNonPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperNonPortableOpForUt>();
  });

  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kNonPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelHelper model_helper;
  EXPECT_NE(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);
}

TEST_F(UtestModelHelper, SaveCustomOpsPartitionSkipsRootGraphRepeatedInSubgraphMap) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  GeModelPtr root_model = std::make_shared<GeModel>();
  root_model->SetGraph(ge_root_model->GetRootGraph());
  ge_root_model->subgraph_instance_name_to_model_[ge_root_model->GetRootGraph()->GetName()] = root_model;

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.SaveCustomOpsPartition(om_file_save_helper, ge_root_model), SUCCESS);

  ModelPartition custom_partition;
  ASSERT_TRUE(FindCustomOpsPartitionForModelHelperUt(om_file_save_helper, custom_partition));
  ASSERT_FALSE(om_file_save_helper->model_contexts_.empty());
  ASSERT_EQ(om_file_save_helper->model_contexts_[0U].owned_partitions_.size(), 1U);
  EXPECT_EQ(custom_partition.data, om_file_save_helper->model_contexts_[0U].owned_partitions_[0U]->data());

  const auto expected_partition_size =
      sizeof(CustomKernelItemHeader) + strlen(kPortableOpTypeForModelHelper) + kPortableKernelBinForModelHelper.size();
  ASSERT_EQ(custom_partition.size, expected_partition_size);
}

TEST_F(UtestModelHelper, CollectCustomOpSoFromCustomOppPathNoTargetSoShouldFail) {
  ScopedHostEnvForModelHelperUt host_env_guard("linux", "x86_64");
  ScopedEnvVarForModelHelperUt custom_opp_guard(kEnvNameCustom);

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  const std::string custom_opp_path = opp_path + "custom_opp_no_match_ut";
  ScopedTmpDirCleanerForModelHelperUt opp_dir_guard(custom_opp_path);
  const std::string target_proto_so_dir = custom_opp_path + "/op_proto/lib/linux/x86_64";
  const std::string target_tiling_so_dir = custom_opp_path + "/op_tiling/lib/linux/x86_64";
  const std::string non_target_so_dir = custom_opp_path + "/op_proto/lib/linux/aarch64";
  const std::string non_target_so_path = non_target_so_dir + "/libcross_only_ut.so";
  system(("mkdir -p " + target_proto_so_dir).c_str());
  system(("mkdir -p " + target_tiling_so_dir).c_str());
  system(("mkdir -p " + non_target_so_dir).c_str());
  system(("touch " + non_target_so_path).c_str());
  mmSetEnv(kEnvNameCustom, custom_opp_path.c_str(), 1);

  GeRootModel ge_root_model;
  EXPECT_NE(ge_root_model.CollectCustomOpSoFromCustomOppPath("linux", "x86_64"), SUCCESS);
  system(("rm -rf " + custom_opp_path).c_str());
}

TEST_F(UtestModelHelper, CheckAndSetNeedSoInOMCrossModePortableOpShouldCollectAllTargetEnvSo) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::string current_env_os;
  std::string current_env_cpu;
  std::string target_env_cpu;
  ASSERT_TRUE(ResolveCrossModeTargetEnvForModelHelperUt(current_env_os, current_env_cpu, target_env_cpu));

  ScopedHostEnvForModelHelperUt host_env_guard(current_env_os, target_env_cpu);
  ScopedEnvVarForModelHelperUt custom_opp_guard(kEnvNameCustom);

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  const std::string custom_opp_path = opp_path + "custom_opp_cross_mode_ut";
  ScopedTmpDirCleanerForModelHelperUt opp_dir_guard(custom_opp_path);
  CrossModeCustomOppPathsForModelHelperUt so_paths;
  ASSERT_TRUE(PrepareCrossModeCustomOppForModelHelperUt(custom_opp_path, current_env_os, current_env_cpu,
                                                        target_env_cpu, so_paths));
  mmSetEnv(kEnvNameCustom, custom_opp_path.c_str(), 1);

  ASSERT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  const auto &custom_op_so_set = ge_root_model->GetCustomOpSoSet();
  ASSERT_EQ(custom_op_so_set.size(), 1U);
  EXPECT_NE(custom_op_so_set.count(so_paths.target_graph_so_path), 0U);
  EXPECT_EQ(custom_op_so_set.count(so_paths.non_target_graph_so_path), 0U);
  for (const auto &so_path : custom_op_so_set) {
    EXPECT_NE(so_path.find(current_env_os + "/" + target_env_cpu), std::string::npos);
  }
}

TEST_F(UtestModelHelper, CheckAndSetNeedSoInOMSameArchShouldKeepExistingBehavior) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::string current_env_os;
  std::string current_env_cpu;
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
  ASSERT_FALSE(current_env_cpu.empty());

  ScopedHostEnvForModelHelperUt host_env_guard(current_env_os, current_env_cpu);
  ScopedEnvVarForModelHelperUt custom_opp_guard(kEnvNameCustom);
  mmSetEnv(kEnvNameCustom, "", 1);

  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_FALSE(ge_root_model->GetCustomOpSoSet().empty());
}

TEST_F(UtestModelHelper, CheckSoArchMatchesTargetNotExistSoShouldFail) {
  GeRootModel ge_root_model;
  std::string test_dir = __FILE__;
  test_dir = test_dir.substr(0, test_dir.rfind("/") + 1);
  EXPECT_NE(ge_root_model.CheckSoArchMatchesTarget(test_dir + "not_exist_custom_op_ut.so", "x86_64"), SUCCESS);
}

TEST_F(UtestModelHelper, CollectCustomOpSoFromCustomOppPathShouldCollectAllSoWithoutNameFilter) {
  ScopedHostEnvForModelHelperUt host_env_guard("linux", "aarch64");
  ScopedEnvVarForModelHelperUt custom_opp_guard(kEnvNameCustom);

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  const std::string custom_opp_path = opp_path + "custom_opp_ambiguous_match_ut";
  ScopedTmpDirCleanerForModelHelperUt opp_dir_guard(custom_opp_path);
  const std::string target_graph_so_dir = custom_opp_path + "/op_graph/lib/linux/aarch64";
  const std::string so_path_0 = target_graph_so_dir + "/libaaa_custom_0.so";
  system(("mkdir -p " + target_graph_so_dir).c_str());
  uint16_t expected_machine = 0U;
  ASSERT_TRUE(GetExpectedElfMachineForCpuModelHelperUt("aarch64", expected_machine));
  ASSERT_TRUE(WriteElfHeaderForModelHelperUt(so_path_0, expected_machine));
  mmSetEnv(kEnvNameCustom, custom_opp_path.c_str(), 1);

  GeRootModel ge_root_model;
  EXPECT_EQ(ge_root_model.CollectCustomOpSoFromCustomOppPath("linux", "aarch64"), SUCCESS);
  EXPECT_EQ(ge_root_model.GetCustomOpSoSet().size(), 1U);
  EXPECT_NE(ge_root_model.GetCustomOpSoSet().count(so_path_0), 0U);

  system(("rm -rf " + custom_opp_path).c_str());
}

TEST_F(UtestModelHelper, CheckAndSetNeedSoInOMPortableOpTargetArchMismatchShouldFail) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::string current_env_os;
  std::string current_env_cpu;
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
  ASSERT_FALSE(current_env_os.empty());
  ASSERT_FALSE(current_env_cpu.empty());
  std::string target_env_cpu = current_env_cpu;
  std::transform(target_env_cpu.begin(), target_env_cpu.end(), target_env_cpu.begin(),
                 [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
  if ((target_env_cpu == "aarch64") || (target_env_cpu == "arm64")) {
    target_env_cpu = "x86_64";
  } else {
    target_env_cpu = "aarch64";
  }

  ScopedHostEnvForModelHelperUt host_env_guard(current_env_os, target_env_cpu);
  ScopedEnvVarForModelHelperUt custom_opp_guard(kEnvNameCustom);

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  const std::string custom_opp_path = opp_path + "custom_opp_arch_mismatch_ut";
  ScopedTmpDirCleanerForModelHelperUt opp_dir_guard(custom_opp_path);
  const std::string mismatch_so_dir = custom_opp_path + "/op_proto/lib/" + current_env_os + "/" + target_env_cpu;
  const std::string mismatch_so_path = mismatch_so_dir + "/lib" + std::string(kPortableOpTypeForModelHelper) + "_ut.so";
  system(("mkdir -p " + mismatch_so_dir).c_str());
  const uint16_t mismatch_e_machine = (target_env_cpu == "x86_64") ? 183U : 62U;  // EM_AARCH64 : EM_X86_64
  ASSERT_TRUE(WriteElfHeaderForModelHelperUt(mismatch_so_path, mismatch_e_machine));
  mmSetEnv(kEnvNameCustom, custom_opp_path.c_str(), 1);

  EXPECT_NE(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);

  system(("rm -rf " + custom_opp_path).c_str());
}

TEST_F(UtestModelHelper, CheckAndSetNeedSoInOMPortableCustomOpSetsCustomSoBit) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::string current_env_os;
  std::string current_env_cpu;
  std::string target_env_cpu;
  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  const std::string custom_opp_path = opp_path + "custom_opp_custom_so_bit_ut";
  ScopedTmpDirCleanerForModelHelperUt opp_dir_guard(custom_opp_path);
  ScopedEnvVarForModelHelperUt custom_opp_guard(kEnvNameCustom);
  CrossModeCustomOppPathsForModelHelperUt so_paths;
  PreparePortableCustomOpCrossModeForModelHelperUt(custom_opp_path, current_env_os, current_env_cpu, target_env_cpu,
                                                   so_paths);
  ScopedHostEnvForModelHelperUt host_env_guard(current_env_os, target_env_cpu);

  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_NE((ge_root_model->GetSoInOmFlag() & 0x1000U), 0U);
  EXPECT_NE(ge_root_model->GetCustomOpSoSet().count(so_paths.target_graph_so_path), 0U);
}

TEST_F(UtestModelHelper, SaveSoStoreModelPartitionInfoCustomOpOnlyGraphShouldSaveSoBinsPartition) {
  RegisterCustomOpCreatorForModelHelperUt(kPortableOpTypeForModelHelper, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<ModelHelperPortableOpForUt>();
  });
  const auto ge_root_model =
      CreateGeRootModelForModelHelperUt(kPortableOpTypeForModelHelper, kPortableOpTypeForModelHelper);
  ASSERT_NE(ge_root_model, nullptr);

  std::string current_env_os;
  std::string current_env_cpu;
  std::string target_env_cpu;
  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  const std::string custom_opp_path = opp_path + "custom_opp_custom_only_save_ut";
  ScopedTmpDirCleanerForModelHelperUt opp_dir_guard(custom_opp_path);
  ScopedEnvVarForModelHelperUt custom_opp_guard(kEnvNameCustom);
  CrossModeCustomOppPathsForModelHelperUt so_paths;
  PreparePortableCustomOpCrossModeForModelHelperUt(custom_opp_path, current_env_os, current_env_cpu, target_env_cpu,
                                                   so_paths);
  ScopedHostEnvForModelHelperUt host_env_guard(current_env_os, target_env_cpu);

  ASSERT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);

  std::shared_ptr<OmFileSaveHelper> om_file_save_helper = std::make_shared<OmFileSaveHelper>();
  GeModelPtr first_ge_model = std::make_shared<GeModel>();
  std::string output_file_name = "custom_only_model.om";
  ModelHelper model_helper;
  ASSERT_EQ(
      model_helper.SaveSoStoreModelPartitionInfo(om_file_save_helper, ge_root_model, output_file_name, first_ge_model),
      SUCCESS);

  ModelPartition so_bins_partition;
  EXPECT_TRUE(FindSoBinsPartitionForModelHelperUt(om_file_save_helper, so_bins_partition));
}

TEST_F(UtestModelHelper, LoadOpSoBinCustomTypeInvalidPayloadShouldFail) {
  OpSoStore op_so_store;
  std::vector<char_t> invalid_payload = {static_cast<char_t>(0xA5), static_cast<char_t>(0x5A),
                                         static_cast<char_t>(0x00), static_cast<char_t>(0xFF)};
  auto invalid_so_data = std::unique_ptr<char_t[]>(new (std::nothrow) char_t[invalid_payload.size()]);
  ASSERT_NE(invalid_so_data, nullptr);
  ASSERT_EQ(memcpy_s(invalid_so_data.get(), invalid_payload.size(), invalid_payload.data(), invalid_payload.size()),
            EOK);

  const auto custom_invalid_so =
      std::make_shared<OpSoBin>("libcustom_invalid.so", "vendor_custom", std::move(invalid_so_data),
                                static_cast<uint32_t>(invalid_payload.size()), static_cast<SoBinType>(3));
  ASSERT_NE(custom_invalid_so, nullptr);
  op_so_store.AddKernel(custom_invalid_so);
  ASSERT_TRUE(op_so_store.Build());

  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;
  OmFileContext cur_ctx;
  ModelPartition so_patition;
  so_patition.type = ModelPartitionType::SO_BINS;
  so_patition.data = const_cast<uint8_t *>(op_so_store.Data());
  so_patition.size = op_so_store.DataSize();
  cur_ctx.partition_datas_.push_back(so_patition);
  load_helper.model_contexts_.push_back(cur_ctx);

  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  ASSERT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  GeModelPtr ge_model = std::make_shared<GeModel>();

  ModelHelper model_helper;
  model_helper.model_ = ge_model;
  std::vector<CustomOpSoHandlePtr> loaded_handles;
  EXPECT_NE(model_helper.LoadOpSoBin(load_helper, ge_root_model, loaded_handles), SUCCESS);
}

TEST_F(UtestModelHelper, CustomOpSoLoaderLoadSuccessAndCleanupShouldReleaseHandleAndFd) {
  std::string source_so_path;
  ASSERT_TRUE(FindNotLoadedSystemSoForModelHelperUt(source_so_path));
  std::vector<char_t> so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(source_so_path, so_data));
  const auto so_bin = BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_fallback_ut.so", "vendor_ut", so_data);
  ASSERT_NE(so_bin, nullptr);

  std::string so_key;
  std::string fd_path;
  {
    CustomOpSoLoader loader;
    std::vector<CustomOpSoHandlePtr> loaded_handles;
    ASSERT_EQ(loader.LoadCustomOpSoBins({so_bin}, loaded_handles), SUCCESS);
    ASSERT_EQ(loaded_handles.size(), 1U);
    ASSERT_EQ(loader.loaded_states_.size(), 1U);
    so_key = loader.loaded_states_.begin()->first;
    EXPECT_EQ(so_key, loaded_handles[0]->GetFingerprintKey());
    EXPECT_EQ(loaded_handles[0]->GetSoName(), "libcustom_op_loader_fallback_ut.so");
    ASSERT_NE(loaded_handles[0]->GetHandle(), nullptr);
    ASSERT_NE(loaded_handles[0]->mem_fd_, -1);
    fd_path = BuildProcFdPathForModelHelperUt(loaded_handles[0]->mem_fd_);
    ASSERT_EQ(mmAccess2(fd_path.c_str(), M_F_OK), EN_OK);

    loader.Cleanup();
    EXPECT_TRUE(loader.loaded_states_.empty());
    EXPECT_EQ(mmAccess2(fd_path.c_str(), M_F_OK), EN_OK);
    loaded_handles.clear();
  }

  EXPECT_FALSE(so_key.empty());
  EXPECT_NE(mmAccess2(fd_path.c_str(), M_F_OK), EN_OK);
}

TEST_F(UtestModelHelper, CustomOpSoLoaderShouldIgnoreLegacyCacheDirEnvAndNeverWriteDiskSo) {
  ScopedEnvVarForModelHelperUt env_guard("GE_CUSTOM_OP_CACHE_DIR");
  const std::string cache_dir =
      "/tmp/custom_op_loader_env_" + std::to_string(mmGetPid()) + "_" + std::to_string(mmGetTid());
  (void)mmRmdir(cache_dir.c_str());
  ASSERT_EQ(CreateDir(cache_dir), 0);
  ASSERT_EQ(mmSetEnv("GE_CUSTOM_OP_CACHE_DIR", cache_dir.c_str(), 1), EN_OK);

  std::string source_so_path;
  ASSERT_TRUE(FindNotLoadedSystemSoForModelHelperUt(source_so_path));
  std::vector<char_t> so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(source_so_path, so_data));
  const auto so_bin = BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_cache_env_ut.so", "vendor_ut", so_data);
  ASSERT_NE(so_bin, nullptr);

  const std::string expected_disk_so_path = cache_dir + "/vendor_ut_libcustom_op_loader_cache_env_ut.so";
  {
    CustomOpSoLoader loader;
    std::vector<CustomOpSoHandlePtr> loaded_handles;
    ASSERT_EQ(loader.LoadCustomOpSoBins({so_bin}, loaded_handles), SUCCESS);
    ASSERT_EQ(loaded_handles.size(), 1U);
    ASSERT_EQ(loader.loaded_states_.size(), 1U);
    EXPECT_NE(mmAccess2(expected_disk_so_path.c_str(), M_F_OK), EN_OK);
    loader.Cleanup();
    EXPECT_TRUE(loader.loaded_states_.empty());
  }

  EXPECT_EQ(mmAccess2(cache_dir.c_str(), M_F_OK), EN_OK);
  EXPECT_NE(mmAccess2(expected_disk_so_path.c_str(), M_F_OK), EN_OK);
  (void)mmRmdir(cache_dir.c_str());
}

TEST_F(UtestModelHelper, CustomOpSoLoaderLoadSameContentReturnsSameLeaseFromWeakCache) {
  std::string source_so_path;
  ASSERT_TRUE(FindNotLoadedSystemSoForModelHelperUt(source_so_path));
  std::vector<char_t> so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(source_so_path, so_data));
  const auto so_bin = BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_same_path_ut.so", "vendor_ut", so_data);
  ASSERT_NE(so_bin, nullptr);

  {
    CustomOpSoLoader loader;
    std::vector<CustomOpSoHandlePtr> first_loaded_handles;
    ASSERT_EQ(loader.LoadCustomOpSoBins({so_bin}, first_loaded_handles), SUCCESS);
    ASSERT_EQ(first_loaded_handles.size(), 1U);
    ASSERT_EQ(loader.loaded_states_.size(), 1U);
    const auto first_it = loader.loaded_states_.begin();
    const auto first_key = first_it->first;
    ASSERT_FALSE(first_key.empty());
    ASSERT_EQ(first_key.size(), 16U);
    ASSERT_NE(first_loaded_handles[0]->GetHandle(), nullptr);
    ASSERT_NE(first_loaded_handles[0]->mem_fd_, -1);
    ASSERT_EQ(mmAccess2(BuildProcFdPathForModelHelperUt(first_loaded_handles[0]->mem_fd_).c_str(), M_F_OK), EN_OK);

    std::vector<CustomOpSoHandlePtr> second_loaded_handles;
    ASSERT_EQ(loader.LoadCustomOpSoBins({so_bin}, second_loaded_handles), SUCCESS);
    ASSERT_EQ(second_loaded_handles.size(), 1U);
    ASSERT_EQ(loader.loaded_states_.size(), 1U);
    const auto second_it = loader.loaded_states_.find(first_key);
    ASSERT_NE(second_it, loader.loaded_states_.end());
    EXPECT_EQ(second_loaded_handles[0], first_loaded_handles[0]);
    EXPECT_EQ(second_it->second.lock(), first_loaded_handles[0]);

    loader.Cleanup();
    EXPECT_TRUE(loader.loaded_states_.empty());
  }
}

TEST_F(UtestModelHelper, CustomOpSoLoaderFingerprintShouldUseHexHashOnly) {
  const std::vector<char_t> so_data = {'a', 'b', 'c'};
  const auto so_bin = BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_hash_ut.so", "vendor_ut", so_data);
  ASSERT_NE(so_bin, nullptr);

  CustomOpSoLoader loader;
  std::string fingerprint_key;
  ASSERT_EQ(loader.CalculateSoBinFingerprint(so_bin, fingerprint_key), SUCCESS);
  EXPECT_EQ(fingerprint_key.size(), 16U);
  for (char c : fingerprint_key) {
    EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
  }
}

TEST_F(UtestModelHelper, CustomOpSoLoaderLoadSameNameDifferentContentShouldReturnIndependentLeases) {
  std::string first_so_path;
  std::string second_so_path;
  ASSERT_TRUE(FindTwoReadableSystemSosForModelHelperUt(first_so_path, second_so_path));
  std::vector<char_t> first_so_data;
  std::vector<char_t> second_so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(first_so_path, first_so_data));
  ASSERT_TRUE(ReadSoDataForModelHelperUt(second_so_path, second_so_data));
  ASSERT_NE(first_so_data, second_so_data);

  const auto first_so_bin =
      BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_same_name_ut.so", "vendor_ut", first_so_data);
  const auto second_so_bin =
      BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_same_name_ut.so", "vendor_ut", second_so_data);
  ASSERT_NE(first_so_bin, nullptr);
  ASSERT_NE(second_so_bin, nullptr);

  {
    CustomOpSoLoader loader;
    std::vector<CustomOpSoHandlePtr> loaded_handles;
    ASSERT_EQ(loader.LoadCustomOpSoBins({first_so_bin, second_so_bin}, loaded_handles), SUCCESS);
    ASSERT_EQ(loaded_handles.size(), 2U);
    ASSERT_EQ(loader.loaded_states_.size(), 2U);
    EXPECT_EQ(loaded_handles[0]->GetSoName(), loaded_handles[1]->GetSoName());
    EXPECT_NE(loaded_handles[0], loaded_handles[1]);
    EXPECT_NE(loaded_handles[0]->GetHandle(), loaded_handles[1]->GetHandle());
    EXPECT_NE(loaded_handles[0]->GetFingerprintKey(), loaded_handles[1]->GetFingerprintKey());
    EXPECT_EQ(loaded_handles[0]->GetFingerprintKey().size(), 16U);
    EXPECT_EQ(loaded_handles[1]->GetFingerprintKey().size(), 16U);

    loader.Cleanup();
    EXPECT_TRUE(loader.loaded_states_.empty());
  }
}

TEST_F(UtestModelHelper, CustomOpSoLoaderWeakCacheShouldExpireAfterReturnedHandlesRelease) {
  std::string source_so_path;
  ASSERT_TRUE(FindNotLoadedSystemSoForModelHelperUt(source_so_path));
  std::vector<char_t> so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(source_so_path, so_data));
  const auto so_bin =
      BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_internal_lease_ut.so", "vendor_ut", so_data);
  ASSERT_NE(so_bin, nullptr);

  CustomOpSoLoader loader;
  CustomOpSoHandlePtr first_handle;
  {
    std::vector<CustomOpSoHandlePtr> loaded_handles;
    ASSERT_EQ(loader.LoadCustomOpSoBins({so_bin}, loaded_handles), SUCCESS);
    ASSERT_EQ(loaded_handles.size(), 1U);
    first_handle = loaded_handles[0];
    ASSERT_EQ(loader.loaded_states_.size(), 1U);
    ASSERT_FALSE(loader.loaded_states_.begin()->second.expired());
  }
  ASSERT_EQ(loader.loaded_states_.size(), 1U);
  ASSERT_FALSE(loader.loaded_states_.begin()->second.expired());
  first_handle.reset();
  ASSERT_TRUE(loader.loaded_states_.begin()->second.expired());

  std::vector<CustomOpSoHandlePtr> reloaded_handles;
  ASSERT_EQ(loader.LoadCustomOpSoBins({so_bin}, reloaded_handles), SUCCESS);
  ASSERT_EQ(reloaded_handles.size(), 1U);
  ASSERT_EQ(loader.loaded_states_.size(), 1U);
  EXPECT_EQ(loader.loaded_states_.begin()->second.lock(), reloaded_handles[0]);

  loader.Cleanup();
  EXPECT_TRUE(loader.loaded_states_.empty());
}

TEST_F(UtestModelHelper, CustomOpSoLoaderBatchFailureShouldReleasePreviouslyDlopenLeaseWithoutOwner) {
  std::string source_so_path;
  ASSERT_TRUE(FindNotLoadedSystemSoForModelHelperUt(source_so_path));
  std::vector<char_t> valid_so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(source_so_path, valid_so_data));
  const auto valid_so_bin =
      BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_batch_valid_ut.so", "vendor_ut", valid_so_data);
  ASSERT_NE(valid_so_bin, nullptr);

  const std::vector<char_t> invalid_so_data = {'n', 'o', 't', '_', 'e', 'l', 'f'};
  const auto invalid_so_bin =
      BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_batch_invalid_ut.so", "vendor_ut", invalid_so_data);
  ASSERT_NE(invalid_so_bin, nullptr);

  CustomOpSoLoader loader;
  std::vector<CustomOpSoHandlePtr> loaded_handles;
  EXPECT_NE(loader.LoadCustomOpSoBins({valid_so_bin, invalid_so_bin}, loaded_handles), SUCCESS);
  EXPECT_TRUE(loaded_handles.empty());
  ASSERT_EQ(loader.loaded_states_.size(), 1U);
  ASSERT_TRUE(loader.loaded_states_.begin()->second.expired());

  loader.Cleanup();
  EXPECT_TRUE(loader.loaded_states_.empty());
}

TEST_F(UtestModelHelper, CustomOpSoLoaderConcurrentSameFingerprintShouldReusePublishedHandle) {
  std::string source_so_path;
  ASSERT_TRUE(FindNotLoadedSystemSoForModelHelperUt(source_so_path));
  std::vector<char_t> so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(source_so_path, so_data));
  const auto so_bin =
      BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_concurrent_reuse_ut.so", "vendor_ut", so_data);
  ASSERT_NE(so_bin, nullptr);

  CustomOpSoLoader loader;
  std::vector<CustomOpSoHandlePtr> first_loaded_handles;
  std::vector<CustomOpSoHandlePtr> second_loaded_handles;
  Status first_status = FAILED;
  Status second_status = FAILED;
  RunConcurrentLoadSameFingerprint(loader, so_bin, first_loaded_handles, second_loaded_handles, first_status,
                                   second_status);
  ASSERT_EQ(first_status, SUCCESS);
  ASSERT_EQ(second_status, SUCCESS);
  ASSERT_EQ(first_loaded_handles.size(), 1U);
  ASSERT_EQ(second_loaded_handles.size(), 1U);
  EXPECT_EQ(first_loaded_handles[0], second_loaded_handles[0]);
  EXPECT_EQ(loader.loaded_states_.size(), 1U);
  loader.Cleanup();
}

TEST_F(UtestModelHelper, CustomOpSoLoaderEmptyBinDataShouldFailWithoutPublishingCache) {
  auto empty_so_data = std::unique_ptr<char_t[]>(new (std::nothrow) char_t[1U]);
  ASSERT_NE(empty_so_data, nullptr);
  const auto empty_so_bin = std::make_shared<OpSoBin>("libcustom_op_loader_empty_ut.so", "vendor_ut",
                                                      std::move(empty_so_data), 0U, SoBinType::kCustomOp);
  ASSERT_NE(empty_so_bin, nullptr);

  CustomOpSoLoader loader;
  std::vector<CustomOpSoHandlePtr> loaded_handles;
  EXPECT_NE(loader.LoadCustomOpSoBins({empty_so_bin}, loaded_handles), SUCCESS);
  EXPECT_TRUE(loaded_handles.empty());
  EXPECT_TRUE(loader.loaded_states_.empty());
}

TEST_F(UtestModelHelper, CustomOpSoLoaderDlopenFailureShouldNotPoisonWeakCache) {
  const std::vector<char_t> invalid_so_data = {'n', 'o', 't', '_', 'e', 'l', 'f'};
  const auto invalid_so_bin =
      BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_invalid_ut.so", "vendor_ut", invalid_so_data);
  ASSERT_NE(invalid_so_bin, nullptr);

  CustomOpSoLoader loader;
  std::vector<CustomOpSoHandlePtr> invalid_loaded_handles;
  EXPECT_NE(loader.LoadCustomOpSoBins({invalid_so_bin}, invalid_loaded_handles), SUCCESS);
  EXPECT_TRUE(invalid_loaded_handles.empty());
  EXPECT_TRUE(loader.loaded_states_.empty());

  std::string source_so_path;
  ASSERT_TRUE(FindNotLoadedSystemSoForModelHelperUt(source_so_path));
  std::vector<char_t> valid_so_data;
  ASSERT_TRUE(ReadSoDataForModelHelperUt(source_so_path, valid_so_data));
  const auto valid_so_bin =
      BuildCustomOpSoBinForModelHelperUt("libcustom_op_loader_invalid_ut.so", "vendor_ut", valid_so_data);
  ASSERT_NE(valid_so_bin, nullptr);

  std::vector<CustomOpSoHandlePtr> valid_loaded_handles;
  EXPECT_EQ(loader.LoadCustomOpSoBins({valid_so_bin}, valid_loaded_handles), SUCCESS);
  EXPECT_EQ(valid_loaded_handles.size(), 1U);
  EXPECT_EQ(loader.loaded_states_.size(), 1U);
  loader.Cleanup();
}

TEST_F(UtestModelHelper, LoadOpSoBinDataFail) {
  OmFileLoadHelper load_helper;
  ModelHelper model_helper;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);

  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;

  load_helper.is_inited_ = true;
  OmFileContext cur_ctx;
  ModelPartition so_patition;
  so_patition.type = ModelPartitionType::SO_BINS;
  so_patition.data = nullptr;
  so_patition.size = 0;
  cur_ctx.partition_datas_.push_back(so_patition);
  load_helper.model_contexts_.push_back(cur_ctx);
  model_helper.model_ = ge_model;
  std::vector<CustomOpSoHandlePtr> loaded_handles;
  EXPECT_EQ(model_helper.LoadOpSoBin(load_helper, ge_root_model, loaded_handles), SUCCESS);
}

TEST_F(UtestModelHelper, LoadOpSoBinDataNonEmptyInvalidPayloadShouldFail) {
  OmFileLoadHelper load_helper;
  load_helper.is_inited_ = true;
  OmFileContext cur_ctx;
  // so_num = 1 but no SoStoreItemHead payload, should make OpSoStore::Load fail.
  std::vector<uint8_t> invalid_so_bins = {0x01U, 0x00U, 0x00U, 0x00U, 0x00U};
  ModelPartition so_patition;
  so_patition.type = ModelPartitionType::SO_BINS;
  so_patition.data = invalid_so_bins.data();
  so_patition.size = invalid_so_bins.size();
  cur_ctx.partition_datas_.push_back(so_patition);
  load_helper.model_contexts_.push_back(cur_ctx);

  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;

  ModelHelper model_helper;
  model_helper.model_ = ge_model;
  std::vector<CustomOpSoHandlePtr> loaded_handles;
  EXPECT_NE(model_helper.LoadOpSoBin(load_helper, ge_root_model, loaded_handles), SUCCESS);
}

TEST_F(UtestModelHelper, GetBinDataSuccess) {
  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  mmSetEnv(kEnvName, opp_path.c_str(), 1);

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_proto_path).c_str());
  inner_proto_path += kOpsProto;
  system(("touch " + inner_proto_path).c_str());
  system(("echo 'ops proto:123 ' > " + inner_proto_path).c_str());

  std::string inner_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_tiling_path).c_str());
  inner_tiling_path += kOpMaster;
  system(("touch " + inner_tiling_path).c_str());
  system(("echo 'op tiling:456 ' > " + inner_tiling_path).c_str());

  ModelHelper model_helper;
  string cpu_info = "x86_64";
  string os_info = "linux";
  auto ret = model_helper.GetSoBinData(cpu_info, os_info);
  EXPECT_EQ(ret, SUCCESS);

  system(("rm -rf " + path_vendors).c_str());
}

TEST_F(UtestModelHelper, SoBinSaveToOmModel_CPU_OS_EMPTY) {
  ModelHelper model_helper;  // 默认为离线模型
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;
  ge_model->SetGraph(graph);

  {
    domi::ModelTaskDef model_task_def;
    std::shared_ptr<domi::ModelTaskDef> model_task_def_ptr = make_shared<domi::ModelTaskDef>(model_task_def);
    domi::TaskDef *task_def = model_task_def_ptr->add_task();
    ge_model->SetModelTaskDef(model_task_def_ptr);

    auto aicore_task = std::unique_ptr<hybrid::AiCoreOpTask>(new (std::nothrow) hybrid::AiCoreOpTask());
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_ALL_KERNEL));
    domi::KernelDefWithHandle *kernel_with_handle = task_def->mutable_kernel_with_handle();
    kernel_with_handle->set_original_kernel_key("");
    kernel_with_handle->set_node_info("");
    kernel_with_handle->set_block_dim(32);
    kernel_with_handle->set_args_size(64);
    string args(64, '1');
    kernel_with_handle->set_args(args.data(), 64);
    domi::KernelContext *context = kernel_with_handle->mutable_context();
    context->set_op_index(1);
    context->set_kernel_type(2);  // ccKernelType::TE
    uint16_t args_offset[9] = {0};
    context->set_args_offset(args_offset, 9 * sizeof(uint16_t));
  }

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  mmSetEnv(kEnvName, opp_path.c_str(), 1);

  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_proto_path).c_str());
  inner_proto_path += kOpsProto;
  system(("touch " + inner_proto_path).c_str());
  system(("echo 'ops proto:123 ' > " + inner_proto_path).c_str());

  std::string inner_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_tiling_path).c_str());
  inner_tiling_path += kOpMaster;
  system(("touch " + inner_tiling_path).c_str());
  system(("echo 'op tiling:456 ' > " + inner_tiling_path).c_str());

  string cpu_info = "x86_64";
  string os_info = "linux";
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0x8000);
  EXPECT_EQ(model_helper.GetSoBinData(cpu_info, os_info), SUCCESS);

  std::map<std::string, std::string> options_map;
  options_map["ge.host_env_os"] = "";
  options_map["ge.host_env_cpu"] = "";
  GetThreadLocalContext().SetGlobalOption(options_map);

  std::string output_file = "static_outputfile.om";
  ModelBufferData model;
  bool is_unknown_shape = false;

  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, is_unknown_shape), SUCCESS);
}

TEST_F(UtestModelHelper, SoBinSaveToOmRootModelErrByFileNameTooLong) {
  ModelHelper model_helper;  // 默认为离线模型
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("graph");
  AttrUtils::SetBool(graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  GeRootModelPtr ge_root_model = std::make_shared<GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), SUCCESS);
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  GeModelPtr ge_model = std::make_shared<GeModel>();
  ge_root_model->subgraph_instance_name_to_model_["graph"] = ge_model;
  ge_model->SetGraph(graph);

  {
    domi::ModelTaskDef model_task_def;
    std::shared_ptr<domi::ModelTaskDef> model_task_def_ptr = make_shared<domi::ModelTaskDef>(model_task_def);
    domi::TaskDef *task_def = model_task_def_ptr->add_task();
    ge_model->SetModelTaskDef(model_task_def_ptr);

    auto aicore_task = std::unique_ptr<hybrid::AiCoreOpTask>(new (std::nothrow) hybrid::AiCoreOpTask());
    task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_ALL_KERNEL));
    domi::KernelDefWithHandle *kernel_with_handle = task_def->mutable_kernel_with_handle();
    kernel_with_handle->set_original_kernel_key("");
    kernel_with_handle->set_node_info("");
    kernel_with_handle->set_block_dim(32);
    kernel_with_handle->set_args_size(64);
    string args(64, '1');
    kernel_with_handle->set_args(args.data(), 64);
    domi::KernelContext *context = kernel_with_handle->mutable_context();
    context->set_op_index(1);
    context->set_kernel_type(2);  // ccKernelType::TE
    uint16_t args_offset[9] = {0};
    context->set_args_offset(args_offset, 9 * sizeof(uint16_t));
  }

  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  mmSetEnv(kEnvName, opp_path.c_str(), 1);

  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_proto_path).c_str());
  inner_proto_path += kOpsProto;
  system(("touch " + inner_proto_path).c_str());
  system(("echo 'ops proto:123 ' > " + inner_proto_path).c_str());

  std::string inner_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_tiling_path).c_str());
  inner_tiling_path += kOpMaster;
  system(("touch " + inner_tiling_path).c_str());
  system(("echo 'op tiling:456 ' > " + inner_tiling_path).c_str());

  string cpu_info = "x86_64";
  string os_info = "linux";
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0x8000);
  EXPECT_EQ(model_helper.GetSoBinData(cpu_info, os_info), SUCCESS);

  std::string output_file = "test12345678910_12345678910_12345678910_12345678910_12345678910_";
  output_file.append("12345678910_12345678910_12345678910_12345678910_12345678910_");
  output_file.append("12345678910_12345678910_12345678910_12345678910_12345678910_");
  output_file.append("12345678910_12345678910__12345678910_12345678910_12345678910.om");

  ModelBufferData model;
  bool is_unknown_shape = true;
  std::map<std::string, std::string> options_map;
  options_map["ge.host_env_os"] = os_info;
  options_map["ge.host_env_cpu"] = cpu_info;
  GetThreadLocalContext().SetGlobalOption(options_map);

  EXPECT_NE(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, is_unknown_shape), SUCCESS);
  system(("rm -rf " + opp_path + kInner).c_str());
}

TEST_F(UtestModelHelper, GetSoBinData_fail) {
  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1);
  opp_path += "/temp/";
  mmSetEnv(kEnvName, opp_path.c_str(), 1);

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=mdc' > " + path_config).c_str());

  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_proto_path).c_str());
  inner_proto_path += kOpsProto;
  system(("touch " + inner_proto_path).c_str());
  system(("echo 'ops proto:123 ' > " + inner_proto_path).c_str());

  std::string vender_tiling_path = opp_path + "vendors/mdc/" + kOpMasterPath;
  system(("mkdir -p " + vender_tiling_path).c_str());
  vender_tiling_path += kOpMaster;
  system(("touch " + vender_tiling_path).c_str());
  system(("echo 'op tiling:456 ' > " + vender_tiling_path).c_str());
  system(("echo 'compiler_version=6.3.T5.0.B121' > " + opp_path + "vendors/mdc/" + "/version.info").c_str());

  std::string customize_tiling_path = opp_path + "customize" + kOpMasterPath;
  system(("mkdir -p " + customize_tiling_path).c_str());
  customize_tiling_path += kOpMaster;
  system(("touch " + customize_tiling_path).c_str());
  system(("echo 'op tiling:456 ' > " + customize_tiling_path).c_str());
  system(("echo 'compiler_version=6.4.T5.0.B121' > " + opp_path + "customize" + "/version.info").c_str());
  mmSetEnv(kEnvNameCustom, (opp_path + "customize").c_str(), 1);

  string cpu_info = "x86_64";
  string os_info = "linux";
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.GetSoBinData(cpu_info, os_info), PARAM_INVALID);

  system(("rm -rf " + inner_proto_path).c_str());
  EXPECT_EQ(model_helper.GetSoBinData(cpu_info, os_info), PARAM_INVALID);
  GeModelPtr ge_model = std::make_shared<GeModel>();
  model_helper.SetModelCompilerVersion(ge_model);

  system(("rm -rf " + opp_path).c_str());
}

TEST_F(UtestModelHelper, GetHardwareInfo_no_device) {
  dlog_setlevel(0, 0, 0);
  g_runtime_stub_mock = "rtGetDevice";

  std::map<std::string, std::string> options;
  options[SOC_VERSION] = "Ascend910";
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.GetHardwareInfo(options), SUCCESS);
  EXPECT_NE(options.size(), 0);
  g_runtime_stub_mock = "";
  dlog_setlevel(0, 3, 0);
}

TEST_F(UtestModelHelper, GetHardwareInfo_no_device_get_count_from_rts_failed) {
  dlog_setlevel(0, 0, 0);
  AclRuntimeStub::SetErrorResultApiName("aclrtGetDeviceInfo");
  std::map<std::string, std::string> options;
  options[SOC_VERSION] = "Ascend910";
  ModelHelper model_helper;
  EXPECT_NE(model_helper.GetHardwareInfo(options), SUCCESS);
  dlog_setlevel(0, 3, 0);
  AclRuntimeStub::SetErrorResultApiName("");
}

TEST_F(UtestModelHelper, GetHardwareInfo_device0) {
  g_runtime_stub_mock = "rtGetDevice2";
  const char *const kVectorcoreNum = "ge.vectorcoreNum";

  std::map<std::string, std::string> options;
  options[AICORE_NUM] = "2";
  options[kVectorcoreNum] = "2";
  options[SOC_VERSION] = "Ascend910";
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.GetHardwareInfo(options), SUCCESS);

  EXPECT_NE(options.size(), 0);
  g_runtime_stub_mock = "";
}

TEST_F(UtestModelHelper, GetSoBinData_upgraded_opp_success) {
  std::vector<std::string> paths;
  gert::CreateBuiltInSplitAndUpgradedSo(paths);
  std::string os_info{"linux"};
  std::string cpu_info{"x86_64"};
  ModelHelper model_helper;
  EXPECT_EQ(model_helper.GetSoBinData(cpu_info, os_info), SUCCESS);
  EXPECT_EQ(model_helper.op_so_store_.kernels_.size(), 4U);
  for (const auto &path : paths) {
    ge::PathUtils::RemoveDirectories(path);
  }
}

TEST_F(UtestModelHelper, SaveOpMasterDevice_BuiltIn_Success) {
  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1) + "/test_tmp/";
  mmSetEnv(kEnvName, opp_path.c_str(), 1);
  std::string inner_op_master = opp_path + "built_in/op_master_device/lib/";
  system(("mkdir -p " + inner_op_master).c_str());
  inner_op_master += "Ascend-V7.6-libopmaster.so";
  system(("touch " + inner_op_master).c_str());
  system(("echo 'Ascend-V7.6-libopmaster' > " + inner_op_master).c_str());

  ModelBufferData model;
  ModelHelper model_helper;
  model_helper.SetSaveMode(true);
  GeRootModelPtr ge_root_model =
      ConstructGeRootModel(false, ModelTaskType::MODEL_TASK_PREPROCESS_KERNEL, ccKernelType::AI_CPU, inner_op_master);
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0x4000);

  std::string output_file = opp_path + "/output.om";
  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, false), SUCCESS);

  ge::ModelParserBase base;
  ge::ModelData model_data;
  EXPECT_EQ(base.LoadFromFile(output_file.c_str(), -1, model_data), SUCCESS);
  EXPECT_EQ(model_helper.LoadRootModel(model_data), SUCCESS);
  if (model_data.model_data != nullptr) {
    delete[] reinterpret_cast<char_t *>(model_data.model_data);
  }

  const auto &root_model = model_helper.GetGeRootModel();
  const auto &so_list = root_model->GetAllSoBin();
  EXPECT_EQ(so_list.size(), 1UL);
  EXPECT_EQ(so_list[0UL]->GetSoBinType(), SoBinType::kOpMasterDevice);
  system(("rm -rf " + opp_path).c_str());
}

TEST_F(UtestModelHelper, SaveOpMasterDevice_WithSpaceRegistry_Success) {
  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1) + "/test_tmp/";
  mmSetEnv(kEnvName, opp_path.c_str(), 1);
  std::string inner_op_master;
  ASSERT_TRUE(PrepareBuiltInOpMasterFilesForModelHelperUt(opp_path, inner_op_master));

  ModelBufferData model;
  ModelHelper model_helper;
  model_helper.SetSaveMode(true);
  GeRootModelPtr ge_root_model =
      ConstructGeRootModel(true, ModelTaskType::MODEL_TASK_PREPROCESS_KERNEL, ccKernelType::AI_CPU, inner_op_master);
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0xc000);

  std::map<std::string, std::string> options_map;
  options_map["ge.host_env_os"] = "linux";
  options_map["ge.host_env_cpu"] = "x86_64";
  GetThreadLocalContext().SetGlobalOption(options_map);
  std::string output_file = opp_path + "/output.om";
  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, false), SUCCESS);

  ge::ModelParserBase base;
  ge::ModelData model_data;
  EXPECT_EQ(base.LoadFromFile((opp_path + "output_linux_x86_64.om").c_str(), -1, model_data), SUCCESS);
  EXPECT_EQ(model_helper.LoadRootModel(model_data), SUCCESS);
  if (model_data.model_data != nullptr) {
    delete[] reinterpret_cast<char_t *>(model_data.model_data);
  }

  const auto &root_model = model_helper.GetGeRootModel();
  const auto &so_list = root_model->GetAllSoBin();
  EXPECT_EQ(so_list.size(), 3UL);
  unordered_map<SoBinType, uint32_t> res;
  res.emplace(SoBinType::kSpaceRegistry, 0U);
  res.emplace(SoBinType::kOpMasterDevice, 0U);
  std::for_each(so_list.begin(), so_list.end(), [&res](const OpSoBinPtr &so_bin) { res[so_bin->GetSoBinType()]++; });
  EXPECT_EQ(res[SoBinType::kSpaceRegistry], 2U);
  EXPECT_EQ(res[SoBinType::kOpMasterDevice], 1U);
  system(("rm -rf " + opp_path).c_str());
}

TEST_F(UtestModelHelper, SaveOpMasterDevice_So_Name_Invalid) {
  GeRootModelPtr ge_root_model =
      ConstructGeRootModel(false, ModelTaskType::MODEL_TASK_PREPROCESS_KERNEL, ccKernelType::AI_CPU);
  EXPECT_NE(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
}

TEST_F(UtestModelHelper, SaveAndLoadOfflineAutofuseSo) {
  std::string opp_path = __FILE__;
  opp_path = opp_path.substr(0, opp_path.rfind("/") + 1) + "/test_tmp/";
  mmSetEnv(kEnvName, opp_path.c_str(), 1);
  std::string inner_op_master = opp_path + "built_in/op_master_device/lib/";
  system(("mkdir -p " + inner_op_master).c_str());
  inner_op_master += "Ascend-V7.6-libopmaster.so";
  system(("touch " + inner_op_master).c_str());
  system(("echo 'Ascend-V7.6-libopmaster' > " + inner_op_master).c_str());

  ModelBufferData model;
  ModelHelper model_helper;
  model_helper.SetSaveMode(true);
  GeRootModelPtr ge_root_model =
      ConstructGeRootModel(false, ModelTaskType::MODEL_TASK_PREPROCESS_KERNEL, ccKernelType::AI_CPU, inner_op_master);

  auto ge_root_graph = ge_root_model->GetRootGraph();
  auto autofuse_stub_so = __FILE__;
  std::cout << "bin path: " << autofuse_stub_so << std::endl;
  ASSERT_TRUE(AddAutofuseNodeAndSetBinFilePathForModelHelperUt(ge_root_graph, autofuse_stub_so));

  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0x6000);

  std::string output_file = opp_path + "/output.om";
  EXPECT_EQ(model_helper.SaveToOmRootModel(ge_root_model, output_file, model, false), SUCCESS);

  ge::ModelParserBase base;
  ge::ModelData model_data;
  EXPECT_EQ(base.LoadFromFile(output_file.c_str(), -1, model_data), SUCCESS);
  EXPECT_EQ(model_helper.LoadRootModel(model_data), SUCCESS);
  if (model_data.model_data != nullptr) {
    delete[] reinterpret_cast<char_t *>(model_data.model_data);
  }

  OmFileLoadHelper load_helper;
  std::vector<uint8_t> so_payload;
  ASSERT_TRUE(BuildOfflineAutofuseSoBinsForModelHelperUt(load_helper, so_payload));
  std::vector<CustomOpSoHandlePtr> loaded_handles;
  EXPECT_EQ(model_helper.LoadOpSoBin(load_helper, ge_root_model, loaded_handles), SUCCESS);

  const auto &root_model = model_helper.GetGeRootModel();
  const auto &so_list = root_model->GetAllSoBin();
  EXPECT_EQ(so_list.size(), 2UL);
  EXPECT_EQ(so_list[0UL]->GetSoBinType(), SoBinType::kOpMasterDevice);
  system(("rm -rf " + opp_path).c_str());
}

TEST_F(UtestModelHelper, SaveToOm_for_SubPkg_Opp) {
  auto options = GetThreadLocalContext().GetAllGlobalOptions();
  std::vector<std::string> paths;
  gert::CreateBuiltInSubPkgSo(paths);
  for (const auto &path : paths) {
    GELOGI("Subpkg so:%s", path.c_str());
  }

  GeModelPtr ge_model = ge::MakeShared<GeModel>();
  ComputeGraphPtr graph = ge::MakeShared<ComputeGraph>("g1");
  ge_model->SetGraph(graph);
  std::string output_file{"subpkg_opp.om"};
  ModelBufferData buffer_data;
  GeRootModelPtr ge_root_model = ge::MakeShared<GeRootModel>();
  ComputeGraphPtr root_graph = ge::MakeShared<ComputeGraph>("subgraph");
  (void)AttrUtils::SetBool(root_graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  ge_root_model->SetRootGraph(root_graph);
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0x8000);
  FillModelTaskDef(ge_model);
  std::map<string, string> env_options;
  env_options["ge.host_env_os"] = "linux";
  env_options["ge.host_env_cpu"] = "x86_64";
  (void)GetThreadLocalContext().SetGlobalOption(env_options);

  ModelHelper model_helper;
  (void)model_helper.SaveToOmModel(ge_model, output_file, buffer_data, ge_root_model);

  const auto so_bins = model_helper.op_so_store_.GetSoBin();
  EXPECT_EQ(so_bins.size(), 4);
  EXPECT_NE(so_bins[0]->GetSoName().find("libopgraph_math.so"), std::string::npos);
  EXPECT_NE(so_bins[1]->GetSoName().find("libophost_math.so"), std::string::npos);
  EXPECT_NE(so_bins[2]->GetSoName().find("libopgraph_math.so"), std::string::npos);
  EXPECT_NE(so_bins[3]->GetSoName().find("libophost_math.so"), std::string::npos);
  (void)GetThreadLocalContext().SetGlobalOption(options);
  for (const auto &path : paths) {
    ge::PathUtils::RemoveDirectories(path);
  }
}

}  // namespace ge
