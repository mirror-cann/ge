/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_DUMP_CALLBACK_MANAGER_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_DUMP_CALLBACK_MANAGER_H_

#include <cstdint>
#include <string>
#include "common/ge_common/ge_types.h"
#include "ge/ge_api_error_codes.h"

namespace ge {
namespace dump {

class DumpCallbackManager {
 public:
  static DumpCallbackManager& GetInstance();
  static Status GlobalInit();
  bool RegisterDumpCallbacks(uint32_t module_id) const;

 private:
  DumpCallbackManager() = default;
  ~DumpCallbackManager() = default;

  // 回调处理函数
  static int32_t EnableDumpCallback(uint64_t dumpSwitch, const char* dumpData, int32_t size);
  static int32_t DisableDumpCallback(uint64_t dumpSwitch, const char* dumpData, int32_t size);

  // 异常 Dump 位开关处理
  static bool IsEnableExceptionDumpBySwitch(uint64_t dumpSwitch);
  static std::string BuildExceptionDumpJsonBySwitch(uint64_t dumpSwitch);
  static bool ProcessExceptionDumpBySwitch(uint64_t dumpSwitch);

  // 内部处理逻辑
  static Status HandleEnableDump(const char* dumpData, int32_t size);
  static Status HandleDisableDump();
  static Status HandleDumpExceptionConfig();
  static Status HandleDumpDebugConfig();
};

}  // namespace dump
}  // namespace ge

#endif  // GE_FRAMEWORK_RUNTIME_DUMP_DUMP_CALLBACK_MANAGER_H_
