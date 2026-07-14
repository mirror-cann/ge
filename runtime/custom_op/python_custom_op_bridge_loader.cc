/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "runtime/custom_op/python_custom_op_bridge_loader.h"

#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <vector>

#include "common/ge_common/string_util.h"
#include "common/python_runtime/python_artifact_utils.h"
#include "common/python_runtime/python_bridge_loader_utils.h"
#include "framework/common/debug/ge_log.h"
#include "graph/ascend_string.h"
#include "graph/custom_op_factory.h"
#include "graph_metadef/graph/utils/file_utils.h"
#include "mmpa/mmpa_api.h"
#include "runtime/custom_op/python_custom_op_adapter.h"
#include "runtime/custom_op/python_custom_op_bridge_c_api.h"

namespace ge {
namespace custom_op {
namespace {

constexpr const char *kCustomOpArtifactsRelativePath = "custom_op/python_custom_op_artifacts";
constexpr const char *kPythonFileSuffix = ".py";
constexpr const char *kPythonPackageInitFile = "__init__.py";

namespace artifact = ::ge::python_artifact;
namespace bridge_loader = ::ge::python_bridge_loader;

bool IsPythonFile(const std::string &path) {
  return (path.size() > strlen(kPythonFileSuffix)) &&
         (path.compare(path.size() - strlen(kPythonFileSuffix), strlen(kPythonFileSuffix), kPythonFileSuffix) == 0);
}

bool IsSkippedModuleEntry(const char *name) {
  if (name == nullptr) {
    return true;
  }
  return (name[0] == '_') || (strcmp(name, ".") == 0) || (strcmp(name, "..") == 0);
}

bool HasPackageInitFile(const std::string &dir) {
  struct stat path_stat{};
  return stat((dir + "/" + kPythonPackageInitFile).c_str(), &path_stat) == 0;
}

bool HasPythonCustomOpEntry(const std::string &path) {
  if (path.empty()) {
    return false;
  }

  struct stat path_stat{};
  if (stat(path.c_str(), &path_stat) != 0) {
    GELOGI("Skip scanning python custom op path[%s] because stat failed.", path.c_str());
    return false;
  }
  if (S_ISREG(path_stat.st_mode)) {
    return IsPythonFile(path);
  }
  if (!S_ISDIR(path_stat.st_mode)) {
    return false;
  }

  DIR *dir = opendir(path.c_str());
  if (dir == nullptr) {
    GELOGI("Skip scanning python custom op directory[%s] because opendir failed.", path.c_str());
    return false;
  }
  struct dirent *entry = nullptr;
  while ((entry = readdir(dir)) != nullptr) {
    if (IsSkippedModuleEntry(entry->d_name)) {
      continue;
    }
    const std::string entry_path = path + "/" + entry->d_name;
    struct stat child_stat{};
    if (stat(entry_path.c_str(), &child_stat) != 0) {
      GELOGI("Skip scanning python custom op path[%s] because stat failed.", entry_path.c_str());
      continue;
    }
    if (S_ISREG(child_stat.st_mode) && IsPythonFile(entry_path)) {
      (void)closedir(dir);
      return true;
    }
    if (S_ISDIR(child_stat.st_mode) && HasPackageInitFile(entry_path)) {
      (void)closedir(dir);
      return true;
    }
  }
  (void)closedir(dir);
  return false;
}

bool HasPythonModuleInCustomOppPath(const char *env_value) {
  const std::string custom_opp_path = env_value;
  const std::vector<std::string> custom_opp_paths = StringUtils::Split(custom_opp_path, ':');
  if (custom_opp_paths.empty()) {
    return false;
  }
  for (const auto &path : custom_opp_paths) {
    if (!path.empty() && HasPythonCustomOpEntry(path)) {
      return true;
    }
  }
  return false;
}

bool NeedLoadPythonCustomOpsImpl() {
  const char_t *custom_opp_path_env = nullptr;
  MM_SYS_GET_ENV(MM_ENV_ASCEND_CUSTOM_OPP_PATH, custom_opp_path_env);
  if ((custom_opp_path_env == nullptr) || (custom_opp_path_env[0] == '\0')) {
    GELOGI("Skip loading python custom ops because ASCEND_CUSTOM_OPP_PATH is empty.");
    return false;
  }
  if (!HasPythonModuleInCustomOppPath(custom_opp_path_env)) {
    GELOGI(
        "Skip loading python custom ops because no loadable python custom op entry is found in "
        "ASCEND_CUSTOM_OPP_PATH.");
    return false;
  }
  return true;
}

std::string GetLoaderLibraryPath() {
  Dl_info dl_info{};
  if ((dladdr(reinterpret_cast<void *>(&LoadPythonCustomOps), &dl_info) == 0) || (dl_info.dli_fname == nullptr) ||
      (dl_info.dli_fname[0] == '\0')) {
    return "";
  }
  const auto real_path = RealPath(dl_info.dli_fname);
  return real_path.empty() ? std::string(dl_info.dli_fname) : real_path;
}

bool IsBridgeApiValid(const PythonCustomOpBridgeApi *api, const uint32_t expected_abi) {
  return (api != nullptr) && (api->abi_version == expected_abi) && (api->set_artifact_config != nullptr) &&
         (api->register_custom_ops != nullptr) && (api->reset_bridge_state != nullptr) &&
         (api->shutdown_bridge != nullptr);
}

bridge_loader::BridgeLoadDependencies BuildBridgeLoadDependencies() {
  return bridge_loader::BridgeLoadDependencies{
      &RealPath,
      &dlopen,
      &dlclose,
      &dlsym,
      &artifact::ResolveLoadedPythonRuntimeKey,
      kPythonCustomOpBridgeGetApiSymbol,
      kPythonCustomOpBridgeAbiVersion,
      RTLD_NOW | RTLD_GLOBAL,
  };
}

class PythonCustomOpBridgeLoader {
 public:
  static PythonCustomOpBridgeLoader &GetInstance() {
    static PythonCustomOpBridgeLoader loader;
    return loader;
  }

  Status Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto ret = EnsureLoaded();
    if (ret != SUCCESS) {
      return ret;
    }
    return RegisterCustomOpsFromBridge();
  }

  void Unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    GELOGI("Unload python custom ops with bridge library[%s].", loaded_path_.c_str());
    CustomOpFactory::RemoveCustomOps(GetRegisteredOpTypes());
    if ((api_ != nullptr) && (api_->reset_bridge_state != nullptr)) {
      api_->reset_bridge_state();
    }
    ClearPythonCustomOpRuntimeRegistry();
    ClearRegisteredState();
  }

  void ShutdownForProcess() {
    std::lock_guard<std::mutex> lock(mutex_);
    GELOGI("Shutdown python custom op bridge for process, current library[%s].", loaded_path_.c_str());
    if ((api_ != nullptr) && (api_->shutdown_bridge != nullptr)) {
      api_->shutdown_bridge();
    }
    api_ = nullptr;
    if (handle_ != nullptr) {
      if (dlclose(handle_) != 0) {
        GELOGW("Close python custom op bridge library failed: %s", dlerror());
      }
      handle_ = nullptr;
    }
    loaded_path_.clear();
    ClearRegisteredState();
  }

 private:
  Status RegisterCustomOpsFromBridge() {
    static constexpr PythonCustomOpRegistrar kRegistrar = {
        &RegisterCustomOpFromBridge,
    };
    GELOGI("Register python custom ops with bridge library[%s].", loaded_path_.c_str());
    const auto ret = api_->register_custom_ops(&kRegistrar);
    if (ret != SUCCESS) {
      GELOGE(ret, "[Register][PythonCustomOps] failed with bridge library[%s].", loaded_path_.c_str());
      return ret;
    }
    return SUCCESS;
  }

  static bool RegisterCustomOpFromBridge(const PythonCustomOpDescriptor *desc,
                                         const PythonCustomOpCallbacks *callbacks) {
    if ((desc == nullptr) || (callbacks == nullptr)) {
      return false;
    }
    return GetInstance().RegisterCustomOp(*desc, *callbacks);
  }

  bool RegisterCustomOp(const PythonCustomOpDescriptor &desc, const PythonCustomOpCallbacks &callbacks) {
    const auto registered_op_type_iter = registered_op_type_to_descriptor_key_.find(desc.op_type);
    if (registered_op_type_iter != registered_op_type_to_descriptor_key_.end()) {
      if (registered_op_type_iter->second == desc.descriptor_key) {
        return true;
      }
      GELOGE(FAILED,
             "[Check][PythonCustomOp]Op type[%s] has been registered with descriptor key[%s] by python custom op, "
             "current descriptor key is[%s].",
             desc.op_type.c_str(), registered_op_type_iter->second.c_str(), desc.descriptor_key.c_str());
      return false;
    }
    if (!PythonCustomOpRuntimeRegistry::Register(desc, callbacks)) {
      GELOGE(FAILED, "[Register][PythonCustomOpRuntimeRegistry] failed, descriptor key[%s], op type[%s].",
             desc.descriptor_key.c_str(), desc.op_type.c_str());
      return false;
    }

    const auto ret = CustomOpFactory::RegisterCustomOpCreator(
        AscendString(desc.op_type.c_str()), [registered_desc = desc]() -> std::unique_ptr<BaseCustomOp> {
          auto *adapter = new (std::nothrow) PythonCustomOpAdapter(registered_desc);
          if ((adapter == nullptr) || (!adapter->IsValid())) {
            delete adapter;
            return nullptr;
          }
          return std::unique_ptr<BaseCustomOp>(adapter);
        });
    if (ret != GRAPH_SUCCESS) {
      (void)PythonCustomOpRuntimeRegistry::Unregister(desc.descriptor_key);
      GELOGE(FAILED, "[Register][PythonCustomOpCreator] failed, descriptor key[%s], op type[%s].",
             desc.descriptor_key.c_str(), desc.op_type.c_str());
      return false;
    }
    registered_op_type_to_descriptor_key_[desc.op_type] = desc.descriptor_key;
    GELOGI("Python custom op[%s] is registered from bridge, descriptor key[%s].", desc.op_type.c_str(),
           desc.descriptor_key.c_str());
    return true;
  }

  void ClearRegisteredState() {
    registered_op_type_to_descriptor_key_.clear();
  }

  std::vector<AscendString> GetRegisteredOpTypes() const {
    std::vector<AscendString> op_types;
    op_types.reserve(registered_op_type_to_descriptor_key_.size());
    for (const auto &item : registered_op_type_to_descriptor_key_) {
      op_types.emplace_back(AscendString(item.first.c_str()));
    }
    return op_types;
  }

  Status EnsureLoaded() {
    if (api_ != nullptr) {
      GELOGI("Reuse already loaded python custom op bridge library[%s].", loaded_path_.c_str());
      return SUCCESS;
    }
    const auto runtime_key = artifact::ResolveLoadedPythonRuntimeKey();
    if (!runtime_key.has_python_symbols) {
      GELOGE(FAILED, "[Check][PythonRuntime]Python symbols are not loaded before custom op preprocessing, runtime[%s].",
             runtime_key.ToString().c_str());
      return FAILED;
    }
    if (!runtime_key.is_initialized) {
      GELOGE(FAILED,
             "[Check][PythonRuntime]Python interpreter is not initialized before custom op preprocessing, "
             "runtime[%s].",
             runtime_key.ToString().c_str());
      return FAILED;
    }
    GELOGI("Python custom op runtime key before loading bridge: %s.", runtime_key.ToString().c_str());
    if (TryLoadPrebuiltBridge(runtime_key)) {
      return SUCCESS;
    }
    GELOGE(FAILED, "Load python custom op bridge library failed.");
    return FAILED;
  }

  bool TryLoadPrebuiltBridge(const artifact::PythonRuntimeKey &runtime_key) {
    const auto candidates = artifact::BuildPrebuiltBridgeLibraryCandidates(
        runtime_key, GetLoaderLibraryPath(), kCustomOpArtifactsRelativePath, kPythonCustomOpBridgeAbiVersion);
    return TryLoadBridgeCandidates(runtime_key, candidates);
  }

  bool TryLoadBridgeCandidates(const artifact::PythonRuntimeKey &runtime_key,
                               const std::vector<artifact::BridgeLibraryCandidate> &candidates) {
    for (const auto &candidate : candidates) {
      bridge_loader::LoadedBridgeCandidate<PythonCustomOpBridgeApi> loaded_bridge;
      const auto deps = BuildBridgeLoadDependencies();
      const auto status =
          bridge_loader::TryLoadBridgeCandidate<PythonCustomOpBridgeApi, PythonCustomOpBridgeArtifactConfig>(
              runtime_key, candidate, deps, &IsBridgeApiValid, loaded_bridge);
      if (status != bridge_loader::BridgeLoadStatus::kSuccess) {
        const auto error_suffix = bridge_loader::BuildBridgeLoadErrorSuffix(status, dlerror());
        GELOGW("Skip python custom op bridge candidate[%s], artifact_root[%s], native_module[%s], status[%s]%s.",
               candidate.bridge_path.c_str(), candidate.artifact_root.c_str(), candidate.native_module_path.c_str(),
               bridge_loader::BridgeLoadStatusToString(status), error_suffix.c_str());
        continue;
      }
      handle_ = loaded_bridge.handle;
      api_ = loaded_bridge.api;
      loaded_path_ = loaded_bridge.real_path;
      GELOGI("Load python custom op bridge from [%s] success.", loaded_path_.c_str());
      return true;
    }
    return false;
  }

  std::mutex mutex_;
  void *handle_{nullptr};
  const PythonCustomOpBridgeApi *api_{nullptr};
  std::string loaded_path_;
  std::map<std::string, std::string> registered_op_type_to_descriptor_key_;
};
}  // namespace

bool NeedLoadPythonCustomOps() {
  return NeedLoadPythonCustomOpsImpl();
}

Status LoadPythonCustomOps() {
  return PythonCustomOpBridgeLoader::GetInstance().Load();
}

void UnloadPythonCustomOps() {
  PythonCustomOpBridgeLoader::GetInstance().Unload();
}

void ShutdownPythonCustomOpsForProcess() {
  PythonCustomOpBridgeLoader::GetInstance().ShutdownForProcess();
}
}  // namespace custom_op
}  // namespace ge
