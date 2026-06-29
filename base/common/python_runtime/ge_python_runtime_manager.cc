/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/python_runtime/ge_python_runtime_manager.h"
#include "common/python_runtime/ge_python_runtime_manager_helper.h"

#include <dlfcn.h>
#include <string>

#include "framework/common/debug/ge_log.h"
#include "graph/utils/file_utils.h"

namespace ge {

GePythonRuntimeManager &GePythonRuntimeManager::Instance() {
  static GePythonRuntimeManager instance;
  return instance;
}

void GePythonRuntimeManager::AttachExistingLocked() {
  manager_is_owner_ = false;
  const char *version = g_python_api.py_get_version();
  GELOGI("[GePythonRuntime] Attach existing interpreter, version[%s], owner=0.", version);
}

Status GePythonRuntimeManager::EnsureReadyLocked() {
  if (ready_) {
    return SUCCESS;
  }
  if (!ResolvePythonCApi(nullptr, false)) {
    if (!EnsureLibpythonLoaded(&libpython_handle_)) {
      GELOGW("[GePythonRuntime] Load libpython failed.");
      return FAILED;
    }
    if (!ResolvePythonCApi(libpython_handle_, true)) {
      GELOGW("[GePythonRuntime] Resolve Python C API failed after loading libpython.");
      ResetStateLocked();
      return FAILED;
    }
  }

  if (g_python_api.py_is_initialized() != 0) {
    AttachExistingLocked();
    ready_ = true;
    return SUCCESS;
  }

  g_python_api.py_initialize();
  if (g_python_api.py_eval_threads_initialized() == 0) {
    g_python_api.py_eval_init_threads();
  }
  ReleaseInitThreadGilLocked();
  manager_is_owner_ = true;
  ready_ = true;
  const char *version = g_python_api.py_get_version();
  GELOGI("[GePythonRuntime] Initialize owned interpreter, version[%s].", version);
  return SUCCESS;
}

Status GePythonRuntimeManager::EnsureReady() {
  std::lock_guard<std::mutex> lock(mutex_);
  return EnsureReadyLocked();
}

void GePythonRuntimeManager::ReleaseInitThreadGilLocked() {
  if (g_python_api.py_gil_state_check() != 0) {
    py_thread_state_ = g_python_api.py_eval_save_thread();
    GELOGI("[GePythonRuntime] Saved init thread state and released GIL.");
  }
}

void GePythonRuntimeManager::ResetStateLocked() {
  if (libpython_handle_ != nullptr) {
    // GE 初始化/反初始化是进程级的，因此这里不主动 dlclose，直到进程退出时系统统一回收
    libpython_handle_ = nullptr;
  }
  ResetPythonCApi();
  py_thread_state_ = nullptr;
  ready_ = false;
  manager_is_owner_ = false;
}

Status GePythonRuntimeManager::FinalizeOwnedInterpreterLocked() {
  if (!manager_is_owner_) {
    GELOGI("[GePythonRuntime] Skip finalize because manager is not interpreter owner.");
    ResetStateLocked();
    return SUCCESS;
  }
  if (g_python_api.py_is_initialized() == 0) {
    GELOGI("[GePythonRuntime] Skip finalize because interpreter is not initialized.");
    ResetStateLocked();
    return SUCCESS;
  }
  if (py_thread_state_ != nullptr) {
    g_python_api.py_eval_restore_thread(py_thread_state_);
    py_thread_state_ = nullptr;
    GELOGI("[GePythonRuntime] Restored init thread state before finalize.");
  }
  g_python_api.py_finalize();
  ResetStateLocked();
  GELOGI("[GePythonRuntime] Finalize owned interpreter done.");
  return SUCCESS;
}

Status GePythonRuntimeManager::ShutdownProcess() {
  std::lock_guard<std::mutex> lock(mutex_);
  return FinalizeOwnedInterpreterLocked();
}

}  // namespace ge
