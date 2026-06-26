/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "python_pass_pybind_bridge.h"

#include <dlfcn.h>

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include "ge/ge_api_types.h"
#include "framework/common/debug/ge_log.h"
#include "graph_metadef/graph/utils/file_utils.h"
#include "pass_registry.h"
#include "python_pass_adapter.h"
#include "python_pass_artifact_selector.h"
#include "python_pass_bridge_loader_helper.h"
#include "python_pass_bridge_c_api.h"
#include "python_pass_fallback_codegen_helper.h"

namespace ge {
namespace fusion {
namespace {
constexpr const char *kPyIsInitializedSymbol = "Py_IsInitialized";
constexpr const char *kPyGetVersionSymbol = "Py_GetVersion";
constexpr const char *kPythonRuntimeProbeScript =
    " -c \"import sys; print('cp%d%d' % sys.version_info[:2]); print(sys.version.split()[0])\" 2>/dev/null";

using python_pass_artifact::BuildPrebuiltBridgeLibraryCandidates;
using python_pass_artifact::ParsePythonTag;
using python_pass_artifact::PythonRuntimeKey;
namespace loader_helper = python_pass_bridge_loader;
namespace fallback_codegen = python_pass_fallback_codegen;

PythonRuntimeKey ResolveLoadedPythonRuntimeKey() {
  using PyIsInitializedFn = int (*)();
  using PyGetVersionFn = const char *(*)();
  auto *py_is_initialized = reinterpret_cast<PyIsInitializedFn>(dlsym(RTLD_DEFAULT, kPyIsInitializedSymbol));
  auto *py_get_version = reinterpret_cast<PyGetVersionFn>(dlsym(RTLD_DEFAULT, kPyGetVersionSymbol));
  PythonRuntimeKey runtime_key;
  runtime_key.has_python_symbols = (py_is_initialized != nullptr) && (py_get_version != nullptr);
  if (!runtime_key.has_python_symbols) {
    runtime_key.source = "unresolved(no loaded Py_* symbols)";
    return runtime_key;
  }
  runtime_key.source = "loaded Py_* symbols";
  runtime_key.is_initialized = (py_is_initialized() != 0);
  const char *version = py_get_version();
  runtime_key.version = (version == nullptr) ? "" : version;
  runtime_key.python_tag = ParsePythonTag(version);
  return runtime_key;
}

bool ReadCommandOutput(const std::string &command, std::string &output) {
  FILE *fp = popen(command.c_str(), "r");
  if (fp == nullptr) {
    return false;
  }
  char buffer[256] = {0};
  while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
    output += buffer;
  }
  const auto ret = pclose(fp);
  return (ret == 0) && (!output.empty());
}

bool ProbePythonRuntimeFromCommand(const char *python_command, PythonRuntimeKey &runtime_key) {
  std::string output;
  if (!ReadCommandOutput(std::string(python_command) + kPythonRuntimeProbeScript, output)) {
    return false;
  }
  const auto python_tag = loader_helper::FirstLine(output);
  if (python_tag.empty()) {
    return false;
  }
  runtime_key = PythonRuntimeKey{};
  runtime_key.python_tag = python_tag;
  runtime_key.version = loader_helper::SecondLine(output);
  runtime_key.python_command = python_command;
  runtime_key.source = std::string("PATH command[") + python_command + "]";
  return true;
}

fallback_codegen::FallbackCodegenDependencies BuildFallbackCodegenDependencies() {
  return fallback_codegen::FallbackCodegenDependencies{
      &ReadCommandOutput,
      &ProbePythonRuntimeFromCommand,
  };
}

PythonRuntimeKey ResolveTargetPythonRuntimeKey() {
  auto runtime_key = ResolveLoadedPythonRuntimeKey();
  if (!runtime_key.python_tag.empty()) {
    return runtime_key;
  }
  PythonRuntimeKey probed_key;
  if (ProbePythonRuntimeFromCommand("python3", probed_key) || ProbePythonRuntimeFromCommand("python", probed_key)) {
    return probed_key;
  }
  return runtime_key;
}

std::string GetLoaderLibraryPath() {
  Dl_info dl_info{};
  if ((dladdr(reinterpret_cast<void *>(&RegisterPythonPassesFromPlugin), &dl_info) == 0) ||
      (dl_info.dli_fname == nullptr) || (dl_info.dli_fname[0] == '\0')) {
    return "";
  }
  const auto real_path = RealPath(dl_info.dli_fname);
  return real_path.empty() ? std::string(dl_info.dli_fname) : real_path;
}

loader_helper::BridgeLoadDependencies BuildBridgeLoadDependencies() {
  return loader_helper::BridgeLoadDependencies{
      &RealPath,
      &dlopen,
      &dlclose,
      &dlsym,
      &ResolveLoadedPythonRuntimeKey,
      kPythonFusionPassBridgeGetApiSymbol,
      kPythonFusionPassBridgeAbiVersion,
      RTLD_NOW | RTLD_GLOBAL,
  };
}

bool RegisterPythonPassFromBridge(const PythonPassDescriptor *pass_desc, const PythonFusionPassCallbacks *callbacks) {
  if ((pass_desc == nullptr) || (callbacks == nullptr)) {
    return false;
  }
  return RegisterPythonPass(*pass_desc, *callbacks);
}

class PythonFusionPassBridgeLoader {
 public:
  static PythonFusionPassBridgeLoader &GetInstance() {
    static PythonFusionPassBridgeLoader loader;
    return loader;
  }

  Status Register() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto ret = EnsureLoaded();
    if (ret != SUCCESS) {
      return ret;
    }
    static constexpr PythonFusionPassRegistrar kRegistrar = {
        &RegisterPythonPassFromBridge,
    };
    GELOGI("Register python passes with bridge library[%s].", loaded_path_.c_str());
    return api_->register_passes(&kRegistrar);
  }

  void Unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    GELOGI("Unload python passes with bridge library[%s], keep bridge loaded for process reuse.", loaded_path_.c_str());
    if ((api_ != nullptr) && (api_->reset_bridge_state != nullptr)) {
      api_->reset_bridge_state();
    }
    // 这里只清理本轮 Python pass 注册态与 C++ runtime 状态，
    // 不接管进程级 Python 解释器生命周期，也不热卸载 bridge so。
    ClearPythonPassRuntimeRegistry();
    PassRegistry::GetInstance().ClearPythonPasses();
  }

  void ShutdownForProcess() {
    std::lock_guard<std::mutex> lock(mutex_);
    GELOGI("Shutdown python pass bridge for process, current library[%s].", loaded_path_.c_str());
    if ((api_ != nullptr) && (api_->shutdown_bridge != nullptr)) {
      api_->shutdown_bridge();
    }
    // 进程级 shutdown 后再显式 dlclose bridge so，
    // 既满足资源释放要求，也避免每轮业务 Unload 都热卸载 bridge。
    api_ = nullptr;
    if (handle_ != nullptr) {
      if (dlclose(handle_) != 0) {
        GELOGW("Close python pass bridge library failed: %s", dlerror());
      }
      handle_ = nullptr;
    }
    loaded_path_.clear();
  }

 private:
  Status EnsureLoaded() {
    if (api_ != nullptr) {
      GELOGI("Reuse already loaded python pass bridge library[%s].", loaded_path_.c_str());
      return SUCCESS;
    }

    const auto runtime_key = ResolveTargetPythonRuntimeKey();
    GELOGI("Python pass runtime key before loading bridge: %s.", runtime_key.ToString().c_str());
    const auto loader_library_path = GetLoaderLibraryPath();
    const auto deps = BuildBridgeLoadDependencies();
    if (TryLoadPrebuiltBridge(runtime_key, loader_library_path, deps) || TryLoadFallbackBridge(runtime_key, deps)) {
      return SUCCESS;
    }
    GELOGE(FAILED, "Load python pass bridge library failed.");
    return FAILED;
  }

  bool TryLoadBridgeCandidates(const PythonRuntimeKey &runtime_key,
                               const std::vector<python_pass_artifact::BridgeLibraryCandidate> &candidates,
                               const loader_helper::BridgeLoadDependencies &deps) {
    for (const auto &candidate : candidates) {
      loader_helper::LoadedBridgeCandidate loaded_bridge;
      const auto status = loader_helper::TryLoadBridgeCandidate(runtime_key, candidate, deps, loaded_bridge);
      if (status != loader_helper::BridgeLoadStatus::kSuccess) {
        const auto error_suffix = loader_helper::BuildBridgeLoadErrorSuffix(status, dlerror());
        GELOGW("Skip python pass bridge candidate[%s], artifact_root[%s], native_module[%s], status[%s]%s.",
               candidate.bridge_path.c_str(), candidate.artifact_root.c_str(), candidate.native_module_path.c_str(),
               loader_helper::BridgeLoadStatusToString(status), error_suffix.c_str());
        continue;
      }
      handle_ = loaded_bridge.handle;
      api_ = loaded_bridge.api;
      loaded_path_ = loaded_bridge.real_path;
      GELOGI("Load python pass bridge from [%s] success.", loaded_path_.c_str());
      return true;
    }
    return false;
  }

  bool TryLoadPrebuiltBridge(const PythonRuntimeKey &runtime_key, const std::string &loader_library_path,
                             const loader_helper::BridgeLoadDependencies &deps) {
    const auto candidates =
        BuildPrebuiltBridgeLibraryCandidates(runtime_key, loader_library_path, kPythonFusionPassBridgeAbiVersion);
    return TryLoadBridgeCandidates(runtime_key, candidates, deps);
  }

  bool TryLoadFallbackBridge(const PythonRuntimeKey &runtime_key, const loader_helper::BridgeLoadDependencies &deps) {
    std::string gen_artifact_root;
    if (!fallback_codegen::RunFallbackCodegen(runtime_key, BuildFallbackCodegenDependencies(), gen_artifact_root)) {
      return false;
    }
    const auto candidate = python_pass_artifact::LoadBridgeCandidateFromArtifactRoot(gen_artifact_root, runtime_key,
                                                                                     kPythonFusionPassBridgeAbiVersion);
    return TryLoadBridgeCandidates(runtime_key, {candidate}, deps);
  }

  std::mutex mutex_;
  void *handle_{nullptr};
  const PythonFusionPassBridgeApi *api_{nullptr};
  std::string loaded_path_;
};
}  // namespace

Status RegisterPythonPassesFromPlugin() {
  return PythonFusionPassBridgeLoader::GetInstance().Register();
}

void UnloadPythonPasses() {
  PythonFusionPassBridgeLoader::GetInstance().Unload();
}

void ShutdownPythonPassesForProcess() {
  PythonFusionPassBridgeLoader::GetInstance().ShutdownForProcess();
}
}  // namespace fusion
}  // namespace ge
