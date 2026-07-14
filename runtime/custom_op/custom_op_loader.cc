/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "runtime/custom_op/custom_op_loader.h"

#include <mutex>

#include "framework/common/debug/ge_log.h"
#include "register/op_lib_register_impl.h"
#include "runtime/custom_op/python_custom_op_bridge_loader.h"

namespace ge {
namespace custom_op {
namespace {
class CustomOpLoader {
 public:
  static CustomOpLoader &GetInstance() {
    static CustomOpLoader instance;
    return instance;
  }

  Status Load() {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto cpp_load_ret = OpLibRegistry::GetInstance().PreProcessForCustomOp();
    if (cpp_load_ret != GRAPH_SUCCESS) {
      GELOGE(cpp_load_ret, "Load C++ custom ops failed.");
      return cpp_load_ret;
    }
    if ((!python_custom_ops_loaded_) && NeedLoadPythonCustomOps()) {
      const auto ret = LoadPythonCustomOps();
      if (ret != SUCCESS) {
        GELOGE(ret, "Load Python custom ops failed.");
        return ret;
      }
      python_custom_ops_loaded_ = true;
    }
    return SUCCESS;
  }

  Status Unload() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (python_custom_ops_loaded_) {
      UnloadPythonCustomOps();
      python_custom_ops_loaded_ = false;
    }
    return SUCCESS;
  }

  Status ShutdownForProcess() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (python_custom_ops_loaded_) {
      GELOGI("[PythonCustomOp] ShutdownForProcess unloading python custom ops.");
      UnloadPythonCustomOps();
      python_custom_ops_loaded_ = false;
    }

    if (shutdown_done_) {
      GELOGI("Python custom op process-level cleanup already done, skip.");
      return SUCCESS;
    }
    shutdown_done_ = true;

    ShutdownPythonCustomOpsForProcess();
    GELOGI("ShutdownPythonCustomOpsForProcess done.");
    return SUCCESS;
  }

 private:
  std::mutex mutex_;
  bool python_custom_ops_loaded_{false};
  bool shutdown_done_{false};
};
}  // namespace

Status LoadCustomOps() {
  return CustomOpLoader::GetInstance().Load();
}

Status LoadPythonCustomOpsIfNeeded() {
  if (!NeedLoadPythonCustomOps()) {
    return SUCCESS;
  }
  return LoadPythonCustomOps();
}

Status UnloadCustomOps() {
  return CustomOpLoader::GetInstance().Unload();
}

Status ShutdownCustomOpsForProcess() {
  return CustomOpLoader::GetInstance().ShutdownForProcess();
}
}  // namespace custom_op
}  // namespace ge
