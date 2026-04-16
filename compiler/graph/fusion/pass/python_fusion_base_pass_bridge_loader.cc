/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "python_fusion_base_pass_pybind_bridge.h"

#include <dlfcn.h>
#include <unistd.h>

#include <mutex>
#include <string>
#include <vector>

#include "framework/common/debug/ge_log.h"
#include "pass_registry.h"
#include "python_fusion_base_pass_adapter.h"
#include "python_fusion_base_pass_bridge_c_api.h"

namespace ge {
namespace fusion {
namespace {
constexpr const char *kPythonFusionBasePassBridgeLibName = "libge_python_pass_bridge.so";

std::string DirName(const std::string &path) {
  const auto pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return ".";
  }
  if (pos == 0U) {
    return "/";
  }
  return path.substr(0U, pos);
}

std::vector<std::string> BuildBridgeLibraryCandidates() {
  std::vector<std::string> candidates;
  Dl_info dl_info{};
  if ((dladdr(reinterpret_cast<void *>(&RegisterPythonFusionBasePassesFromPlugin), &dl_info) != 0) &&
      (dl_info.dli_fname != nullptr) && (dl_info.dli_fname[0] != '\0')) {
    candidates.emplace_back(DirName(dl_info.dli_fname) + "/" + kPythonFusionBasePassBridgeLibName);
  }
  candidates.emplace_back(kPythonFusionBasePassBridgeLibName);
  return candidates;
}

bool RegisterPythonFusionBasePassFromBridge(const PythonPassDescriptor *pass_desc,
                                            const PythonFusionBasePassCallbacks *callbacks) {
  if ((pass_desc == nullptr) || (callbacks == nullptr)) {
    return false;
  }
  return RegisterPythonFusionBasePass(*pass_desc, *callbacks);
}

class PythonFusionBasePassBridgeLoader {
 public:
  static PythonFusionBasePassBridgeLoader &GetInstance() {
    static PythonFusionBasePassBridgeLoader loader;
    return loader;
  }

  Status Register() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto ret = EnsureLoaded();
    if (ret != SUCCESS) {
      return ret;
    }
    static const PythonFusionBasePassRegistrar kRegistrar = {
        &RegisterPythonFusionBasePassFromBridge,
    };
    return api_->register_fusion_base_passes(&kRegistrar);
  }

  void Unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((api_ != nullptr) && (api_->reset_bridge_state != nullptr)) {
      api_->reset_bridge_state();
    }
    // 这里只清理本轮 Python pass 注册态与 C++ runtime 状态，
    // 不接管进程级 Python 解释器生命周期，也不热卸载 bridge so。
    ClearPythonFusionBasePassRuntimeRegistry();
    PassRegistry::GetInstance().ClearPythonPasses();
  }

  void ShutdownForProcess() {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((api_ != nullptr) && (api_->shutdown_bridge != nullptr)) {
      api_->shutdown_bridge();
    }
    // 进程级 shutdown 后再显式 dlclose bridge so，
    // 既满足资源释放要求，也避免每轮业务 Unload 都热卸载 bridge。
    api_ = nullptr;
    if (handle_ != nullptr) {
      if (dlclose(handle_) != 0) {
        GELOGW("Close python fusion base bridge library failed: %s", dlerror());
      }
      handle_ = nullptr;
    }
    loaded_path_.clear();
  }

 private:
  Status EnsureLoaded() {
    if (api_ != nullptr) {
      return SUCCESS;
    }

    const auto candidates = BuildBridgeLibraryCandidates();
    for (const auto &candidate : candidates) {
      // bridge 会把 libpython 一并拉起并嵌入 CPython。这里使用 RTLD_GLOBAL，
      // 这样 embedded interpreter 后续导入标准库或 native extension 时，
      // 才能从已加载的 libpython 中继续解析 Python C-API 符号。
      void *handle = dlopen(candidate.c_str(), RTLD_NOW | RTLD_GLOBAL);
      if (handle == nullptr) {
        GELOGI("Skip loading python fusion base bridge candidate[%s]: %s",
               candidate.c_str(),
               dlerror());
        continue;
      }
      auto *get_api = reinterpret_cast<const PythonFusionBasePassBridgeApi *(*)()>(
          dlsym(handle, kPythonFusionBasePassBridgeGetApiSymbol));
      if (get_api == nullptr) {
        GELOGW("Get python fusion base bridge api symbol failed from [%s]: %s",
               candidate.c_str(),
               dlerror());
        (void)dlclose(handle);
        continue;
      }
      const auto *api = get_api();
      if ((api == nullptr) || (api->abi_version != kPythonFusionBasePassBridgeAbiVersion) ||
          (api->register_fusion_base_passes == nullptr) || (api->reset_bridge_state == nullptr) ||
          (api->shutdown_bridge == nullptr)) {
        GELOGW("Python fusion base bridge api is invalid from [%s].", candidate.c_str());
        (void)dlclose(handle);
        continue;
      }
      handle_ = handle;
      api_ = api;
      loaded_path_ = candidate;
      GELOGI("Load python fusion base bridge from [%s] success.", loaded_path_.c_str());
      return SUCCESS;
    }
    GELOGE(FAILED, "Load python fusion base bridge library failed.");
    return FAILED;
  }

  std::mutex mutex_;
  void *handle_{nullptr};
  const PythonFusionBasePassBridgeApi *api_{nullptr};
  std::string loaded_path_;
};
}  // namespace

Status RegisterPythonFusionBasePassesFromPlugin() {
  return PythonFusionBasePassBridgeLoader::GetInstance().Register();
}

void UnloadPythonFusionBasePasses() {
  PythonFusionBasePassBridgeLoader::GetInstance().Unload();
}

void ShutdownPythonFusionBasePassesForProcess() {
  PythonFusionBasePassBridgeLoader::GetInstance().ShutdownForProcess();
}
}  // namespace fusion
}  // namespace ge
