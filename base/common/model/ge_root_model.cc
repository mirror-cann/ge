/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/model/ge_root_model.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <dlfcn.h>
#include <elf.h>
#include <fstream>
#include <unistd.h>
#include <set>
#include "common/op_so_store/op_so_store_utils.h"
#include "common/plugin/plugin_manager.h"
#include "common/ge_common/string_util.h"
#include "graph/debug/ge_attr_define.h"
#include "common/op_tiling/op_tiling_rt2.h"
#include "common/checker.h"
#include "graph/ge_context.h"
#include "common/host_resource_center/host_resource_serializer.h"
#include "graph/manager/graph_var_manager.h"
#include "graph_metadef/graph/utils/file_utils.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "external/ge_common/ge_api_types.h"
#include "external/graph/custom_op.h"
#include "graph/custom_op_factory.h"

namespace ge {
namespace {
constexpr uint8_t kElfMachineOffset = 18U;

std::string ToLowerCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](const unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

bool GetExpectedElfMachine(const std::string &target_cpu, uint16_t &expected_machine) {
  const auto normalized_target_cpu = ToLowerCopy(target_cpu);
  if ((normalized_target_cpu == "x86_64") || (normalized_target_cpu == "amd64")) {
    expected_machine = EM_X86_64;
    return true;
  }
  if ((normalized_target_cpu == "aarch64") || (normalized_target_cpu == "arm64")) {
    expected_machine = EM_AARCH64;
    return true;
  }
  if (normalized_target_cpu == "arm") {
    expected_machine = EM_ARM;
    return true;
  }
  return false;
}

// 从 so 文件头读取 ELF 的 e_machine（CPU 架构标识），供后续架构匹配校验使用。
// 返回语义：true+is_elf=true 表示已解析到 machine；true+is_elf=false 表示可读但非 ELF；false 表示读取或头校验失败。
bool ReadElfMachine(const std::string &so_path, uint16_t &machine, bool &is_elf) {
  std::ifstream file(so_path, std::ios::binary);
  if (!file.good()) {
    return false;
  }

  std::array<char, EI_NIDENT> ident{};
  file.read(ident.data(), static_cast<std::streamsize>(ident.size()));
  if (file.gcount() != static_cast<std::streamsize>(ident.size())) {
    return false;
  }
  const auto to_u8 = [](const char ch) { return static_cast<uint8_t>(static_cast<unsigned char>(ch)); };
  if ((to_u8(ident[EI_MAG0]) != ELFMAG0) || (to_u8(ident[EI_MAG1]) != ELFMAG1) ||
      (to_u8(ident[EI_MAG2]) != ELFMAG2) || (to_u8(ident[EI_MAG3]) != ELFMAG3)) {
    is_elf = false;
    return true;
  }
  is_elf = true;
  if ((to_u8(ident[EI_DATA]) != ELFDATA2LSB) && (to_u8(ident[EI_DATA]) != ELFDATA2MSB)) {
    return false;
  }

  std::array<char, 2> machine_bytes{};
  file.seekg(kElfMachineOffset, std::ios::beg);
  if (!file.good()) {
    return false;
  }
  file.read(machine_bytes.data(), static_cast<std::streamsize>(machine_bytes.size()));
  if (file.gcount() != static_cast<std::streamsize>(machine_bytes.size())) {
    return false;
  }
  const auto machine_byte0 = to_u8(machine_bytes[0]);
  const auto machine_byte1 = to_u8(machine_bytes[1]);
  if (to_u8(ident[EI_DATA]) == ELFDATA2MSB) {
    machine = static_cast<uint16_t>((machine_byte0 << 8U) | machine_byte1);
  } else {
    machine = static_cast<uint16_t>((machine_byte1 << 8U) | machine_byte0);
  }
  return true;
}

void FallbackHostEnvByCompileTime(std::string &host_env_os, std::string &host_env_cpu) {
  if (host_env_os.empty()) {
#if defined(__linux__)
    host_env_os = "linux";
#endif
  }
  if (host_env_cpu.empty()) {
#if defined(__aarch64__) || defined(__arm64__)
    host_env_cpu = "aarch64";
#elif defined(__x86_64__) || defined(__amd64__)
    host_env_cpu = "x86_64";
#elif defined(__arm__)
    host_env_cpu = "arm";
#else
    GELOGW("[CustomOp] Unknown CPU architecture, unable to determine host env CPU, will use empty string.");
#endif
  }
}

/*
 * P2P类型的feature map内存，目前只给hccl算子使用，而大部分hccl算子都是fixed地址的，所以这里简化处理，将全部p2p内存作为fixed内存，
 * 并将子图中最大的长度作为整图fix内存大小
 */
Status GetP2pFixedFeatureMemorySize(const GeRootModel *ge_root_model, size_t &p2p_mem_size) {
  size_t p2p_max_size = 0U;
  for (const auto &name_to_model : ge_root_model->GetSubgraphInstanceNameToModel()) {
    const auto &ge_model_temp = name_to_model.second;
    size_t p2p_size = 0U;
    (void)AttrUtils::GetInt(ge_model_temp, ATTR_MODEL_P2P_MEMORY_SIZE, p2p_size);
    if (p2p_size > p2p_max_size) {
      p2p_max_size = p2p_size;
    }
    GELOGI("[IMAS]model_name:%s p2p fixed_feature_memory size:%zu, p2p max size:%zu.",
           ge_model_temp->GetName().c_str(), p2p_size, p2p_max_size);
  }
  p2p_mem_size = p2p_max_size;
  return SUCCESS;
}

void CollectCustomOpTypesFromGraph(const ComputeGraphPtr &graph, std::set<std::string> &used_custom_op_types) {
  if (graph == nullptr) {
    return;
  }
  for (const auto &node : graph->GetAllNodes()) {
    const auto &op_type = node->GetType();
    if (CustomOpFactory::IsExistOp(AscendString(op_type.c_str()))) {
      (void)used_custom_op_types.insert(op_type);
    }
  }
}
}
Status GeRootModel::Initialize(const ComputeGraphPtr &root_graph) {
  GE_ASSERT_NOTNULL(root_graph);
  SetRootGraph(root_graph);
  if (model_name_.empty()) {
    model_name_ = root_graph_->GetName();
  }
  GE_ASSERT_NOTNULL(host_resource_center_);
  GE_ASSERT_SUCCESS(host_resource_center_->TakeOverHostResources(root_graph));
  return SUCCESS;
}

Status GeRootModel::ModifyOwnerGraphForSubModels() {
  GE_ASSERT_NOTNULL(root_graph_);
  for (auto &iter : subgraph_instance_name_to_model_) {
    auto sub_graph = root_graph_->GetSubgraph(iter.first);
    if (sub_graph != nullptr) {
      iter.second->SetGraph(sub_graph);
    }
  }
  return SUCCESS;
}

void GeRootModel::SetSubgraphInstanceNameToModel(const std::string &instance_name, const GeModelPtr &ge_model) {
  (void)subgraph_instance_name_to_model_.insert(std::pair<std::string, GeModelPtr>(instance_name, ge_model));
}

void GeRootModel::RemoveInstanceSubgraphModel(const std::string &instance_name) {
  (void)subgraph_instance_name_to_model_.erase(instance_name);
}

Status GeRootModel::CheckIsUnknownShape(bool &is_dynamic_shape) const {
  if (root_graph_ == nullptr) {
    return FAILED;
  }
  is_dynamic_shape = false;
  (void)AttrUtils::GetBool(root_graph_, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, is_dynamic_shape);
  is_dynamic_shape = (is_dynamic_shape || (root_graph_->GetGraphUnknownFlag()));
  return SUCCESS;
}

const uint8_t *GeRootModel::GetOpSoStoreData() const { return op_so_store_.Data(); }

size_t GeRootModel::GetOpStoreDataSize() const { return op_so_store_.DataSize(); }

uint16_t GeRootModel::GetSoInOmFlag() const { return so_in_om_; }

SoInOmInfo GeRootModel::GetSoInOmInfo() const { return so_info_; }

bool GeRootModel::LoadSoBinData(const uint8_t *const data, const size_t len) {
  return op_so_store_.Load(data, len);
}

std::vector<OpSoBinPtr> GeRootModel::GetAllSoBin() const {
  return op_so_store_.GetSoBin();
}

void GeRootModel::SetSoInOmInfo(const SoInOmInfo &so_info) {
  so_info_ = so_info;
  return;
}

Status GeRootModel::CheckAndSetNeedSoInOM() {
  GE_ASSERT_SUCCESS(CheckAndSetSpaceRegistry(), "Check space registry failed.");
  GE_ASSERT_SUCCESS(CheckAndSetOpMasterDevice(), "Check op master device failed.");
  GE_ASSERT_SUCCESS(CheckAndSetAutofuseSo(), "Check autofuse so failed.");
  GE_ASSERT_SUCCESS(CheckAndSetCustomOpSo(), "Check custom op so failed.");
  GELOGI("so in om flag:0x%x", so_in_om_);
  return SUCCESS;
}

Status GeRootModel::CollectCustomOpSoFromCustomOppPath(const std::string &target_os, const std::string &target_cpu) {
  GE_ASSERT_TRUE(!target_os.empty() && !target_cpu.empty(), "target_os or target_cpu is empty.");

  std::string custom_opp_path;
  const char *custom_opp_env = std::getenv("ASCEND_CUSTOM_OPP_PATH");
  if (custom_opp_env != nullptr) {
    custom_opp_path = custom_opp_env;
  }
  if (custom_opp_path.empty()) {
    PluginManager::GetPluginPathFromCustomOppPath("", custom_opp_path);
  }
  GE_ASSERT_TRUE(!custom_opp_path.empty(),
                 "[CustomOp] ASCEND_CUSTOM_OPP_PATH is empty in cross-compile mode.");

  const auto custom_opp_roots = StringUtils::Split(custom_opp_path, ':');
  const std::string so_sub_dir = "/op_graph/lib/" + target_os + "/" + target_cpu;

  for (const auto &custom_opp_root : custom_opp_roots) {
    if (custom_opp_root.empty()) {
      continue;
    }
    std::vector<std::string> so_files;
    PluginManager::GetFileListWithSuffix(custom_opp_root + so_sub_dir, ".so", so_files);
    for (const auto &so_path : so_files) {
      GE_ASSERT_TRUE(access(so_path.c_str(), R_OK) == 0, "custom op so[%s] is not readable.", so_path.c_str());
      GE_ASSERT_SUCCESS(CheckSoArchMatchesTarget(so_path, target_cpu), "Custom op so arch check failed.");
      const auto insert_ret = custom_op_so_set_.insert(so_path);
      if (insert_ret.second) {
        GELOGI("[CustomOp] Collect custom op so[%s] from custom opp path in cross-compile mode.", so_path.c_str());
      }
    }
  }

  GE_ASSERT_TRUE(!custom_op_so_set_.empty(),
                 "[CustomOp] Cannot find target env custom op so from ASCEND_CUSTOM_OPP_PATH, target env[%s/%s].",
                 target_os.c_str(), target_cpu.c_str());
  return SUCCESS;
}

Status GeRootModel::GetTargetHostEnv(std::string &host_env_os, std::string &host_env_cpu) const {
  (void)GetThreadLocalContext().GetOption(OPTION_HOST_ENV_OS, host_env_os);
  (void)GetThreadLocalContext().GetOption(OPTION_HOST_ENV_CPU, host_env_cpu);
  if (host_env_os.empty() || host_env_cpu.empty()) {
    PluginManager::GetCurEnvPackageOsAndCpuType(host_env_os, host_env_cpu);
  }
  if (host_env_os.empty() || host_env_cpu.empty()) {
    GELOGW("[CustomOp] Target host env is empty, fallback to compile-time os/cpu.");
    FallbackHostEnvByCompileTime(host_env_os, host_env_cpu);
  }
  GE_ASSERT_TRUE(!host_env_os.empty() && !host_env_cpu.empty(),
                 "host_env_os or host_env_cpu is empty when resolving custom op so.");
  return SUCCESS;
}

bool GeRootModel::IsCrossCompileTarget(const std::string &target_os, const std::string &target_cpu) const {
  std::string current_env_os;
  std::string current_env_cpu;
  PluginManager::GetCurEnvPackageOsAndCpuType(current_env_os, current_env_cpu);
  if (current_env_os.empty() || current_env_cpu.empty()) {
    GELOGW("[CustomOp] Current env os/cpu is empty, fallback to compile-time arch for cross-compile decision.");
    FallbackHostEnvByCompileTime(current_env_os, current_env_cpu);
  }
  if (current_env_os.empty() || current_env_cpu.empty()) {
    GELOGW("[CustomOp] Current env os/cpu is still empty after fallback, treat as same-arch path.");
    return false;
  }
  return (ToLowerCopy(target_os) != ToLowerCopy(current_env_os)) ||
         (ToLowerCopy(target_cpu) != ToLowerCopy(current_env_cpu));
}

Status GeRootModel::CheckSoArchMatchesTarget(const std::string &so_path, const std::string &target_cpu) const {
  uint16_t expected_machine = 0U;
  if (!GetExpectedElfMachine(target_cpu, expected_machine)) {
    GELOGW("[CustomOp] Skip so arch check for unsupported target cpu[%s].", target_cpu.c_str());
    return SUCCESS;
  }

  uint16_t actual_machine = 0U;
  bool is_elf = false;
  if (!ReadElfMachine(so_path, actual_machine, is_elf)) {
    GELOGE(FAILED, "[CustomOp] Failed to parse elf header for so[%s].", so_path.c_str());
    return FAILED;
  }
  if (!is_elf) {
    GELOGE(FAILED, "[CustomOp] so[%s] is not an ELF file.", so_path.c_str());
    return FAILED;
  }
  if (actual_machine != expected_machine) {
    GELOGE(FAILED, "[CustomOp] so[%s] arch mismatch, target cpu[%s] expects e_machine[%u], actual[%u].",
           so_path.c_str(), target_cpu.c_str(), expected_machine, actual_machine);
    return FAILED;
  }
  return SUCCESS;
}

Status GeRootModel::ResolvePortableOpSoPath(const std::string &op_type, PortableOp *portable_op, std::string &so_path) {
  GE_ASSERT_NOTNULL(portable_op);
  std::string target_os;
  std::string target_cpu;
  GE_ASSERT_SUCCESS(GetTargetHostEnv(target_os, target_cpu), "Get target host env failed.");
  const bool is_cross_compile = IsCrossCompileTarget(target_os, target_cpu);
  GE_ASSERT_TRUE(!is_cross_compile,
                 "Cross-compile mode should not resolve custom op so by op_type[%s].",
                 op_type.c_str());
  Dl_info dl_info;
  auto **vtable = reinterpret_cast<void ***>(portable_op);
  if ((vtable != nullptr) && (*vtable != nullptr) && ((*vtable)[0] != nullptr) &&
      (dladdr((*vtable)[0], &dl_info) != 0) && (dl_info.dli_fname != nullptr)) {
    so_path = dl_info.dli_fname;
  }
  GE_ASSERT_TRUE(!so_path.empty(),
                 "[CustomOp] Resolve custom op so path by dladdr failed, op_type[%s].",
                 op_type.c_str());
  GE_ASSERT_SUCCESS(CheckSoArchMatchesTarget(so_path, target_cpu), "Custom op so arch check failed.");
  return SUCCESS;
}

Status GeRootModel::CheckAndSetCustomOpSo() {
  GE_ASSERT_NOTNULL(root_graph_);
  std::string target_os;
  std::string target_cpu;
  GE_ASSERT_SUCCESS(GetTargetHostEnv(target_os, target_cpu), "Get target host env failed.");
  const bool is_cross_compile = IsCrossCompileTarget(target_os, target_cpu);
  std::set<std::string> used_custom_op_types;
  CollectCustomOpTypesFromGraph(root_graph_, used_custom_op_types);
  for (const auto &item : subgraph_instance_name_to_model_) {
    const auto &ge_model = item.second;
    if (ge_model == nullptr || ge_model->GetGraph() == nullptr || ge_model->GetGraph() == root_graph_) {
      continue;
    }
    CollectCustomOpTypesFromGraph(ge_model->GetGraph(), used_custom_op_types);
  }

  bool has_portable_custom_op = false;
  for (const auto &op_type : used_custom_op_types) {
    auto op = CustomOpFactory::CreateOrGetCustomOp(AscendString(op_type.c_str()));
    GE_ASSERT_NOTNULL(op);
    auto *portable_op = dynamic_cast<PortableOp *>(op);
    if (portable_op == nullptr) {
      GELOGI("[CustomOp] op[%s] is not PortableOp, skip so collect.", op_type.c_str());
      continue;
    }
    has_portable_custom_op = true;
    if (is_cross_compile) {
      continue;
    }

    std::string so_path;
    GE_ASSERT_SUCCESS(ResolvePortableOpSoPath(op_type, portable_op, so_path),
                      "Resolve custom op so path failed for op[%s].", op_type.c_str());

    GE_ASSERT_TRUE(access(so_path.c_str(), R_OK) == 0, "custom op so[%s] is not readable.", so_path.c_str());
    (void)custom_op_so_set_.insert(so_path);
    GELOGI("[CustomOp] Collect custom op so[%s] for op[%s].", so_path.c_str(), op_type.c_str());
  }

  if (is_cross_compile && has_portable_custom_op) {
    GE_ASSERT_SUCCESS(CollectCustomOpSoFromCustomOppPath(target_os, target_cpu),
                      "Collect custom op so from ASCEND_CUSTOM_OPP_PATH failed.");
  }

  if (!custom_op_so_set_.empty()) {
    OpSoStoreUtils::SetSoBinType(SoBinType::kCustomOp, so_in_om_);
  }
  GELOGI("[CustomOp]The num of so is %zu.", custom_op_so_set_.size());
  return SUCCESS;
}

Status GeRootModel::CheckAndSetOpMasterDevice() {
  for (const auto &item : subgraph_instance_name_to_model_) {
    const auto &ge_model = item.second;
    GE_ASSERT_NOTNULL(ge_model);
    if (ge_model->GetModelTaskDefPtr() == nullptr) {
      GELOGI("Root model [%s][%d] has no task", ge_model->GetName().c_str(), ge_model->GetModelId());
      continue;
    }
    const auto &tasks = ge_model->GetModelTaskDefPtr()->task();
    for (int32_t i = 0; i < tasks.size(); ++i) {
      const domi::TaskDef &task_def = tasks[i];
      GELOGI("Task id = %d, task type = %u", i, task_def.type());
      if (static_cast<ModelTaskType>(task_def.type()) != ModelTaskType::MODEL_TASK_PREPROCESS_KERNEL) {
        continue;
      }
      const auto &so_name = task_def.kernel().so_name();
      GE_ASSERT_TRUE(!so_name.empty(), "task [%u] has not set so_name", task_def.type());
      auto result = op_master_device_so_set_.insert(so_name);
      if (result.second) {
        GELOGI("[OpMasterDevice]Get so [%s] from model[%u] task[%u] kernel_type[%u].", so_name.c_str(),
               ge_model->GetModelId(), task_def.type(), task_def.kernel().context().kernel_type());
      }
    }
  }
  if (!op_master_device_so_set_.empty()) {
    OpSoStoreUtils::SetSoBinType(SoBinType::kOpMasterDevice, so_in_om_);
  }
  GELOGI("[OpMasterDevice]The num of so is %zu.", op_master_device_so_set_.size());
  return SUCCESS;
}

Status GeRootModel::CheckAndSetSpaceRegistry() {
  const ComputeGraphPtr &comp_graph = root_graph_;
  GE_ASSERT_NOTNULL(comp_graph);
  bool is_dynamic_shape = false;
  (void)AttrUtils::GetBool(comp_graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, is_dynamic_shape);
  is_dynamic_shape = (is_dynamic_shape || (comp_graph->GetGraphUnknownFlag()));
  if (is_dynamic_shape) {
    OpSoStoreUtils::SetSoBinType(SoBinType::kSpaceRegistry, so_in_om_);
    GELOGI("[SpaceRegistry]Has space registry as dynamic_shape.");
    return SUCCESS;
  }

  bool stc_to_dyn_soft_sync = false;
  for (const auto &node : comp_graph->GetAllNodes()) {
    (void)ge::AttrUtils::GetBool(node->GetOpDesc(), "_static_to_dynamic_softsync_op", stc_to_dyn_soft_sync);
    if (stc_to_dyn_soft_sync) {
      OpSoStoreUtils::SetSoBinType(SoBinType::kSpaceRegistry, so_in_om_);
      GELOGI("[SpaceRegistry]Has space registry as static_to_dynamic_softsync_op.");
      return SUCCESS;
    }
  }
  GELOGI("[SpaceRegistry]Has no space registry.");
  return SUCCESS;
}

Status GeRootModel::CheckAndSetAutofuseSo() {
  auto nodes = root_graph_->GetAllNodesPtr();
  for (auto node : nodes) {
    const std::string* so_path_ptr = ge::AttrUtils::GetStr(node->GetOpDesc(), "bin_file_path");
    if (so_path_ptr != nullptr) {
      auto result = autofuse_so_set_.insert(*so_path_ptr);
      if (result.second) {
          GELOGD("Added autofuse so %s.", (*so_path_ptr).c_str());
      }
    }
  }
  if (!autofuse_so_set_.empty()) {
    OpSoStoreUtils::SetSoBinType(SoBinType::kAutofuse, so_in_om_);
  }
  GELOGI("[AutofuseSo]The num of so is %zu.", autofuse_so_set_.size());
  return SUCCESS;
}

bool GeRootModel::IsNeedMallocFixedFeatureMem() const {
  bool is_unknown_shape = false;
  (void) CheckIsUnknownShape(is_unknown_shape);
  // 动态shape静态子图, 在init图中申请
  if (is_unknown_shape) {
    return false;
  }

  // 用户如果通过这个option设置了fixed内存，也不需要GE申请了
  std::string is_addr_fixed_opt;
  (void)ge::GetContext().GetOption("ge.exec.static_model_addr_fixed", is_addr_fixed_opt);
  if (!is_addr_fixed_opt.empty()) {
    GELOGI("user set ge.exec.static_model_addr_fixed option, return false");
    return false;
  }
  if (VarManager::IsGeUseExtendSizeMemory(false)) {
    GELOGI("enable extend memory, need to malloc fixed_feature_memory use session allocator, model_name: %s,"
        " model_id: %u", GetModelName().c_str(), GetCurModelId());
    return true;
  }
  std::string is_refreshable;
  (void)GetContext().GetOption(OPTION_FEATURE_BASE_REFRESHABLE, is_refreshable);
  static const std::string kEnabled = "1";
  // 如果用户没有设置feature base可刷新，ge也不需要申请fix内存
  if (is_refreshable != kEnabled) {
    GELOGI("feature base is not refreshable, return false, model_name: %s,"
           " model_id: %u", GetModelName().c_str(), GetCurModelId());
    return false;
  }
  return true;
}

/*
 * 如果用户设置了feature base可刷新，并且配置了正常的fix地址，Ge不再申请fix地址，need refresh为false。
 * 如果用户设置了feature base可刷新，没有配置fix地址，Ge需要兜底申请fix内存，need refresh为false。
 * 如果用户设置了feature base可刷新，并且配置了fix地址为nullptr，并且size也设置为0，Ge也不申请fix地址，并设置need refresh为true。
 * 如果用户没有设置feature base可刷新，配置了staticMemoryPolicy=4/2，图间共用fix优先内存, ge申请fix内存，need refresha为false.
 */
bool GeRootModel::IsNeedMallocFixedFeatureMemByType(const rtMemType_t rt_mem_type) const {
  const auto fixed_mem_iter = fixed_feature_mems_.find(rt_mem_type);
  // 没有配置fix地址, Ge需要兜底申请fix内存
  if (fixed_mem_iter == fixed_feature_mems_.end()) {
    GELOGI("fixed_feature_memory base is not set by user, return true, memory type: %s, model_name: %s, model_id: %u",
           MemTypeUtils::ToString(rt_mem_type).c_str(), GetModelName().c_str(), GetCurModelId());
    return true;
  }
  // 配置了正常的fix地址，Ge不再申请fix地址
  if (fixed_mem_iter->second.addr != nullptr) {
    GELOGI("fixed_feature_memory base[%p], return false, memory type: %s, model_name: %s, model_id: %u",
           fixed_mem_iter->second.addr, MemTypeUtils::ToString(rt_mem_type).c_str(), GetModelName().c_str(),
           GetCurModelId());
    return false;
  }
  // 配置了fix地址为nullptr，Ge也不申请fix地址
  if (fixed_mem_iter->second.user_alloc && (fixed_mem_iter->second.addr == nullptr)) {
    GELOGI("user set fixed_feature_memory base nullptr, return false, memory type: %s, model_name: %s, model_id: %u",
           MemTypeUtils::ToString(rt_mem_type).c_str(), GetModelName().c_str(), GetCurModelId());
    return false;
  }
  if (VarManager::IsGeUseExtendSizeMemory(false)) {
    GELOGI("enable extend memory, need to malloc fixed_feature_memory use session allocator, model_name: %s,"
           " model_id: %u", GetModelName().c_str(), GetCurModelId());
    return true;
  }
  GELOGI("fixed_feature_memory info: addr:%p, size:%zu, ge_alloc:%d, user_alloc:%d, memory type: %s, return false,"
      "model_name: %s, model_id: %u", fixed_mem_iter->second.addr, fixed_mem_iter->second.size,
      fixed_mem_iter->second.ge_alloc, fixed_mem_iter->second.user_alloc, MemTypeUtils::ToString(rt_mem_type).c_str(),
      GetModelName().c_str(), GetCurModelId());
  return false;
}

Status GeRootModel::GetSummaryFeatureMemory(std::vector<FeatureMemoryPtr> &all_feature_memory,
                                            size_t &hbm_fixed_feature_mem) {
  if (all_feature_memory_init_flag_) {
    all_feature_memory = all_feature_memory_;
    return SUCCESS;
  }
  hbm_fixed_feature_mem = 0U;
  for (const auto &name_to_model : GetSubgraphInstanceNameToModel()) {
    const auto &ge_model_temp = name_to_model.second;
    std::vector<std::vector<int64_t>> sub_mem_infos;
    (void)AttrUtils::GetListListInt(ge_model_temp, ATTR_MODEL_SUB_MEMORY_INFO, sub_mem_infos);
    size_t fixed_mem_size = 0UL;
    for (size_t index = 0UL; index < sub_mem_infos.size(); ++index) {
      const auto &sub_memory_info = sub_mem_infos[index];
      // 0U: memory_type, 1U:logic_memory_base, 2U:memory_size, 3U:is_fixed_addr_prior
      const bool is_fixed_addr_prior = (sub_memory_info.size() > 3U) && (sub_memory_info[3U] != 0L);
      // 2U:memory_size, is_fixed_addr_prior is true set memory size to fixed size
      fixed_mem_size += (is_fixed_addr_prior ? static_cast<size_t>(sub_memory_info[2U]) : 0UL);
    }
    if (fixed_mem_size > hbm_fixed_feature_mem) {
      hbm_fixed_feature_mem = fixed_mem_size;
    }
    GELOGI("[IMAS]model_name:%s hbm fixed_feature_memory size:%zu, hbm max size:%zu.",
           ge_model_temp->GetName().c_str(), fixed_mem_size, hbm_fixed_feature_mem);
    sub_mem_infos.clear();
  }
  if (hbm_fixed_feature_mem > 0U) {
    auto feature_memory = FeatureMemory::Builder::Build(MemoryType::MEMORY_TYPE_DEFAULT,
                                                        hbm_fixed_feature_mem, {true});
    GE_ASSERT_NOTNULL(feature_memory);
    (void)all_feature_memory_.emplace_back(std::move(feature_memory));
  }

  size_t p2p_fixed_size = 0U;
  GE_ASSERT_SUCCESS(GetP2pFixedFeatureMemorySize(this, p2p_fixed_size));
  if (p2p_fixed_size > 0U) {
    auto p2p_feature_memory = FeatureMemory::Builder::Build(MemoryType::MEMORY_TYPE_P2P,
                                                            p2p_fixed_size, {true});
    GE_ASSERT_NOTNULL(p2p_feature_memory);
    (void)all_feature_memory_.emplace_back(std::move(p2p_feature_memory));
  }
  all_feature_memory = all_feature_memory_;
  all_feature_memory_init_flag_ = true;
  return SUCCESS;
}

HostResourceCenterPtr GeRootModel::GetHostResourceCenterPtr() const {
  return host_resource_center_;
}
std::shared_ptr<GeRootModel> GeRootModel::Fork() {
  std::shared_ptr<GeRootModel> ge_root_model = MakeShared<ge::GeRootModel>();
  GE_ASSERT_NOTNULL(ge_root_model);
  ge_root_model->root_graph_ = root_graph_;
  ge_root_model->model_name_ = model_name_;
  ge_root_model->flatten_graph_ = this->flatten_graph_;
  ge_root_model->subgraph_instance_name_to_model_ = this->subgraph_instance_name_to_model_;
  ge_root_model->is_specific_stream_ = this->is_specific_stream_;
  ge_root_model->total_weight_size_ = this->total_weight_size_;
  ge_root_model->nodes_to_task_defs_ = this->nodes_to_task_defs_;
  ge_root_model->graph_to_static_models_ = this->graph_to_static_models_;
  ge_root_model->op_so_store_ = this->op_so_store_;
  ge_root_model->so_in_om_ = this->so_in_om_;
  ge_root_model->so_info_ = this->so_info_;
  ge_root_model->file_constant_weight_dir_ = this->file_constant_weight_dir_;
  ge_root_model->host_resource_center_ = this->host_resource_center_;
  ge_root_model->op_master_device_so_set_ = this->op_master_device_so_set_;
  ge_root_model->autofuse_so_set_ = this->autofuse_so_set_;
  ge_root_model->custom_op_so_set_ = this->custom_op_so_set_;
  return ge_root_model;
}
}  // namespace ge
