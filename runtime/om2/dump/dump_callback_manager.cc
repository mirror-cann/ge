/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/dump_callback_manager.h"
#include "framework/runtime/dump/dump_config.h"
#include "framework/common/debug/ge_log.h"
#include <dump/adump_api.h>

namespace ge {
namespace dump {

constexpr int32_t ADUMP_SUCCESS = 0;
constexpr int32_t ADUMP_FAILED = -1;
constexpr uint32_t DUMP_MODULE_NAME = 48U;

DumpCallbackManager& DumpCallbackManager::GetInstance() {
  static DumpCallbackManager instance;
  return instance;
}

Status DumpCallbackManager::GlobalInit() {
  DumpCallbackManager& manager = GetInstance();
  if (!manager.RegisterDumpCallbacks(DUMP_MODULE_NAME)) {
    GELOGE(INTERNAL_ERROR, "[Register][DumpCallbacks] Failed to register dump callbacks");
    return INTERNAL_ERROR;
  }
  GELOGI("Dump callbacks registered successfully");
  return SUCCESS;
}

bool DumpCallbackManager::RegisterDumpCallbacks(uint32_t module_id) const {
  int32_t result = Adx::AdumpRegisterCallback(
      module_id,
      reinterpret_cast<Adx::AdumpCallback>(EnableDumpCallback),
      reinterpret_cast<Adx::AdumpCallback>(DisableDumpCallback));
  if (result != ADUMP_SUCCESS) {
    GELOGE(INTERNAL_ERROR, "[Register][DumpCallbacks] Register dump callbacks failed, result: %d", result);
    return false;
  }

  GELOGI("Register dump callbacks successfully for module_id: %u", module_id);
  return true;
}

bool DumpCallbackManager::IsEnableExceptionDumpBySwitch(uint64_t dumpSwitch) {
  return ((dumpSwitch & AIC_ERR_NORM_DUMP_BIT) != 0) ||
         ((dumpSwitch & AIC_ERR_BRIEF_DUMP_BIT) != 0);
}

std::string DumpCallbackManager::BuildExceptionDumpJsonBySwitch(uint64_t dumpSwitch) {
  std::string exceptionScene;

  if ((dumpSwitch & AIC_ERR_NORM_DUMP_BIT) != 0) {
    exceptionScene = "aic_err_norm_dump";
  } else if ((dumpSwitch & AIC_ERR_BRIEF_DUMP_BIT) != 0) {
    exceptionScene = "aic_err_brief_dump";
  } else {
    return "";
  }

  nlohmann::json js;
  nlohmann::json jsDump;

  jsDump[GE_DUMP_SCENE] = exceptionScene;
  jsDump[GE_DUMP_PATH] = "";

  js[GE_DUMP] = jsDump;

  return js.dump();
}

bool DumpCallbackManager::ProcessExceptionDumpBySwitch(uint64_t dumpSwitch) {
  std::string configStr = BuildExceptionDumpJsonBySwitch(dumpSwitch);
  if (configStr.empty()) {
    GELOGE(INTERNAL_ERROR, "[Process][ExceptionDump] Failed to build exception dump config by switch");
    return false;
  }

  GELOGI("Processing exception dump by switch, config: %s", configStr.c_str());

  Status ret = HandleEnableDump(configStr.c_str(), static_cast<int32_t>(configStr.size()));
  if (ret == SUCCESS) {
    GELOGI("Enable exception dump by switch processed successfully");
    return true;
  } else {
    GELOGE(ret, "[Process][ExceptionDump] Enable exception dump by switch failed, ret=%u", ret);
    return false;
  }
}

int32_t DumpCallbackManager::EnableDumpCallback(uint64_t dumpSwitch, const char* dumpData, int32_t size) {
  GELOGI("Enable dump callback triggered, dumpSwitch=%lu, size=%d", dumpSwitch, size);

  if ((dumpData == nullptr || size <= 0) && IsEnableExceptionDumpBySwitch(dumpSwitch)) {
    GELOGI("dumpData is null but exception dump bit is set, processing exception dump by switch");
    return ProcessExceptionDumpBySwitch(dumpSwitch) ? ADUMP_SUCCESS : ADUMP_FAILED;
  }

  Status ret = HandleEnableDump(dumpData, size);
  if (ret == SUCCESS) {
    GELOGI("Enable dump callback processed successfully");
    return ADUMP_SUCCESS;
  } else {
    GELOGE(ret, "[Handle][EnableDump] Enable dump callback failed, ret=%u", ret);
    return ADUMP_FAILED;
  }
}

int32_t DumpCallbackManager::DisableDumpCallback(uint64_t dumpSwitch, const char* dumpData, int32_t size) {
  (void)dumpData;
  (void)size;
  GELOGI("Disable dump callback triggered, dumpSwitch=%lu", dumpSwitch);

  Status ret = HandleDisableDump();
  if (ret == SUCCESS) {
    GELOGI("Disable dump callback processed successfully");
    return ADUMP_SUCCESS;
  } else {
    GELOGE(ret, "[Handle][DisableDump] Disable dump callback failed, ret=%u", ret);
    return ADUMP_FAILED;
  }
}

Status DumpCallbackManager::HandleEnableDump(const char* dumpData, int32_t size) {
  if (dumpData == nullptr || size <= 0) {
    GELOGW("[Handle][EnableDump] Invalid dump data in enable callback");
    return SUCCESS;
  }

  DumpConfig& dumpConfig = DumpConfig::Instance();
  Status ret = dumpConfig.ParseAndValidate(dumpData, size);
  if (ret != SUCCESS) {
    GELOGE(ACL_GE_INVALID_DUMP_CONFIG, "[Handle][EnableDump] Parse dump config failed");
    return ret;
  }

  if (!dumpConfig.NeedDump()) {
    GELOGI("No need to dump based on config");
    return SUCCESS;
  }

  // 场景分流处理
  const std::string& dumpScene = dumpConfig.GetDumpScene();
  if (!dumpScene.empty()) {
    return HandleDumpExceptionConfig();
  }

  if (dumpConfig.GetDumpDebug() == GE_DUMP_STATUS_ON) {
    return HandleDumpDebugConfig();
  }

  GELOGI("Enable dump configuration successfully");
  return SUCCESS;
}

Status DumpCallbackManager::HandleDisableDump() {
  DumpConfig& dumpConfig = DumpConfig::Instance();
  dumpConfig.Reset();
  GELOGI("Disable dump configuration successfully");
  return SUCCESS;
}

Status DumpCallbackManager::HandleDumpExceptionConfig() {
  GELOGI("Handling exception dump configuration");
  return SUCCESS;
}

Status DumpCallbackManager::HandleDumpDebugConfig() {
  GELOGI("Handling dump debug configuration for overflow detection");
  return SUCCESS;
}

}  // namespace dump
}  // namespace ge
