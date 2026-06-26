/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "pass_plugin_loader.h"

#include <cstdlib>
#include <mutex>

#include "framework/common/debug/ge_log.h"
#include "register/custom_pass_helper.h"
#include "python_pass_pybind_bridge.h"

namespace ge {
namespace fusion {
namespace {
constexpr const char *kEnvPythonPassPath = "ASCEND_GE_PY_PASS_PATH";

bool NeedLoadPythonPasses() {
  const char *env_value = std::getenv(kEnvPythonPassPath);
  return (env_value != nullptr) && (env_value[0] != '\0');
}

class PassPluginLoader {
 public:
  static PassPluginLoader &GetInstance() {
    static PassPluginLoader instance;
    return instance;
  }

  Status Load() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!cpp_pass_loaded_) {
      const auto ret = CustomPassHelper::Instance().Load();
      if (ret != SUCCESS) {
        GELOGE(ret, "Load C++ custom pass plugins failed.");
        return ret;
      }
      cpp_pass_loaded_ = true;
    }

    if ((!python_pass_loaded_) && NeedLoadPythonPasses()) {
      const auto ret = RegisterPythonPassesFromPlugin();
      if (ret != SUCCESS) {
        GELOGE(ret, "Load Python fusion pass plugins failed.");
        return ret;
      }
      python_pass_loaded_ = true;
    }
    return SUCCESS;
  }

  Status Unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (python_pass_loaded_) {
      UnloadPythonPasses();
      python_pass_loaded_ = false;
    }

    if (!cpp_pass_loaded_) {
      return SUCCESS;
    }

    cpp_pass_loaded_ = false;
    return CustomPassHelper::Instance().Unload();
  }

  Status ShutdownForProcess() {
    std::lock_guard<std::mutex> lock(mutex_);

    // 业务级状态 reset 必须在 shutdown_done_ 检查之前执行，
    // 否则串行 GEInitialize/GEFinalize 场景下 cpp_pass_loaded_ 不会被 reset，
    // 后续 Load() 会因 cpp_pass_loaded_=true 跳过实际加载。
    if (python_pass_loaded_) {
      GELOGI("[PythonPass] ShutdownForProcess unloading python passes.");
      UnloadPythonPasses();
      python_pass_loaded_ = false;
    }
    if (cpp_pass_loaded_) {
      cpp_pass_loaded_ = false;
      (void)CustomPassHelper::Instance().Unload();
    }

    // 进程级资源（Python 解释器、bridge so）只清理一次
    if (shutdown_done_) {
      GELOGI("[PythonPass] ShutdownForProcess process-level cleanup already done, skip.");
      return SUCCESS;
    }
    shutdown_done_ = true;

    // 进程级 shutdown 额外负责关闭 bridge so。
    ShutdownPythonPassesForProcess();
    GELOGI("[PythonPass] ShutdownPythonPassesForProcess done.");
    return SUCCESS;
  }

 private:
  std::mutex mutex_;
  bool cpp_pass_loaded_{false};
  bool python_pass_loaded_{false};
  bool shutdown_done_{false};
};
}  // namespace

Status LoadPassPlugins() {
  return PassPluginLoader::GetInstance().Load();
}

Status UnloadPassPlugins() {
  return PassPluginLoader::GetInstance().Unload();
}

Status ShutdownPassPluginsForProcess() {
  return PassPluginLoader::GetInstance().ShutdownForProcess();
}
}  // namespace fusion
}  // namespace ge
