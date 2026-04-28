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

#include <mutex>
#include <string>
#include <vector>

#include "ge/ge_api_types.h"
#include "framework/common/debug/ge_log.h"
#include "graph_metadef/graph/utils/file_utils.h"
#include "pass_registry.h"
#include "python_pass_adapter.h"
#include "python_pass_bridge_c_api.h"

namespace ge {
namespace fusion {
namespace {
constexpr const char *kPythonFusionPassBridgeLibName = "libge_python_pass_bridge.so";

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
  if ((dladdr(reinterpret_cast<void *>(&RegisterPythonPassesFromPlugin), &dl_info) != 0) &&
      (dl_info.dli_fname != nullptr) && (dl_info.dli_fname[0] != '\0')) {
    candidates.emplace_back(DirName(dl_info.dli_fname) + "/" + kPythonFusionPassBridgeLibName);
  }
  candidates.emplace_back(kPythonFusionPassBridgeLibName);
  return candidates;
}

bool RegisterPythonPassFromBridge(const PythonPassDescriptor *pass_desc,
                                            const PythonFusionPassCallbacks *callbacks) {
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
    GELOGI("Unload python passes with bridge library[%s], keep bridge loaded for process reuse.",
           loaded_path_.c_str());
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

    const auto candidates = BuildBridgeLibraryCandidates();
    for (const auto &candidate : candidates) {
      const auto real_path = RealPath(candidate.c_str());
      if (real_path.empty()) {
        GELOGI("Skip loading python pass bridge candidate[%s] because real path is invalid.", candidate.c_str());
        continue;
      }
      // bridge 会把 libpython 一并拉起并嵌入 CPython。这里使用 RTLD_GLOBAL，
      // 这样 embedded interpreter 后续导入标准库或 native extension 时，
      // 才能从已加载的 libpython 中继续解析 Python C-API 符号。
      void *handle = dlopen(real_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
      if (handle == nullptr) {
        GELOGI("Skip loading python pass bridge candidate[%s]: %s",
               real_path.c_str(),
               dlerror());
        continue;
      }
      auto *get_api = reinterpret_cast<const PythonFusionPassBridgeApi *(*)()>(
          dlsym(handle, kPythonFusionPassBridgeGetApiSymbol));
      if (get_api == nullptr) {
        GELOGW("Get python pass bridge api symbol failed from [%s]: %s",
               real_path.c_str(),
               dlerror());
        (void)dlclose(handle);
        continue;
      }
      const auto *api = get_api();
      if ((api == nullptr) || (api->abi_version != kPythonFusionPassBridgeAbiVersion) ||
          (api->register_passes == nullptr) || (api->reset_bridge_state == nullptr) ||
          (api->shutdown_bridge == nullptr)) {
        GELOGW("Python pass bridge api is invalid from [%s].", real_path.c_str());
        (void)dlclose(handle);
        continue;
      }
      handle_ = handle;
      api_ = api;
      loaded_path_ = real_path;
      GELOGI("Load python pass bridge from [%s] success.", loaded_path_.c_str());
      return SUCCESS;
    }
    GELOGE(FAILED, "Load python pass bridge library failed.");
    return FAILED;
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
