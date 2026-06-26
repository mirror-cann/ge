/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_PYTHON_RUNTIME_GE_PYTHON_RUNTIME_MANAGER_H_
#define BASE_COMMON_PYTHON_RUNTIME_GE_PYTHON_RUNTIME_MANAGER_H_

#include <mutex>

#include "ge/ge_api_error_codes.h"

namespace ge {

class GePythonRuntimeManager {
 public:
  static GePythonRuntimeManager &Instance();

  Status EnsureReady();
  Status ShutdownProcess();

 private:
  GePythonRuntimeManager() = default;

  Status EnsureReadyLocked();
  void AttachExistingLocked();
  void ReleaseInitThreadGilLocked();
  void ResetStateLocked();
  Status FinalizeOwnedInterpreterLocked();

  mutable std::mutex mutex_;
  bool ready_{false};
  bool manager_is_owner_{false};
  void *libpython_handle_{nullptr};
  void *py_thread_state_{nullptr};
};

}  // namespace ge

#endif  // BASE_COMMON_PYTHON_RUNTIME_GE_PYTHON_RUNTIME_MANAGER_H_
