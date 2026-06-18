/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_CALLBACK_MANAGER_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_CALLBACK_MANAGER_H_

#include <cstdint>

#include "common/ge_common/ge_types.h"
#include "rt_external_base.h"

namespace ge {
namespace dump {

class ProfilingCallbackManager {
 public:
  static ProfilingCallbackManager &GetInstance();
  static Status GlobalInit();
  static rtError_t ProfilingCtrlCallback(const uint32_t ctrl_type, void *const ctrl_data, const uint32_t data_len);
  static void NotifyProfilingCommand(const void *ctrl_data, uint32_t data_len);

 private:
  ProfilingCallbackManager() = default;
  ~ProfilingCallbackManager() = default;
  void HandleCtrlSwitch(const void *ctrl_data, uint32_t data_len) const;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_CALLBACK_MANAGER_H_
