/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/model_dump_c_api.h"
#include "framework/runtime/dump/model_dump_manager.h"
#include "framework/common/debug/ge_log.h"

namespace {
constexpr int32_t PARAM_INVALID = 0x07FFFFFF;
constexpr int32_t SUCCESS = 0;
}  // namespace

// 对外暴露的 C API 函数，需要 extern "C" 确保 C 链接
extern "C" {
int32_t OM2_C_API_EXPORT ReportDfxTaskPreprocess(uint32_t model_id,
                                                 void* instance_handle,
                                                 const struct Om2TaskInfo* task_info,
                                                 const void* extended_attrs,
                                                 size_t extended_attrs_size) {
  (void)model_id;

  if ((extended_attrs != nullptr) || (extended_attrs_size != 0U)) {
    GELOGW("Extended attrs is reserved and must be null, skip preprocess");
    return PARAM_INVALID;
  }

  if ((instance_handle == nullptr) || (task_info == nullptr)) {
    GELOGW("ModelDumpManager handle or task_info is null, skip preprocess");
    return PARAM_INVALID;
  }

  auto* manager = static_cast<ge::dump::ModelDumpManager*>(instance_handle);
  return static_cast<int32_t>(manager->PreprocessOm2TaskInfo(*task_info));
}

int32_t OM2_C_API_EXPORT ReportDfxTaskPostprocess(uint32_t model_id,
                                                  void* instance_handle,
                                                  const struct Om2TaskInfo* task_info,
                                                  const void* extended_attrs,
                                                  size_t extended_attrs_size) {
  (void)model_id;

  if ((extended_attrs != nullptr) || (extended_attrs_size != 0U)) {
    GELOGW("Extended attrs is reserved and must be null, skip postprocess");
    return PARAM_INVALID;
  }

  if ((instance_handle == nullptr) || (task_info == nullptr)) {
    GELOGW("ModelDumpManager handle or task_info is null, skip postprocess");
    return PARAM_INVALID;
  }

  auto* manager = static_cast<ge::dump::ModelDumpManager*>(instance_handle);
  return static_cast<int32_t>(manager->AddOm2TaskInfo(*task_info));
}


int32_t OM2_C_API_EXPORT IsDataDumpEnabled(uint32_t model_id,
                                          void* instance_handle,
                                          const char* op_name,
                                          uint8_t* is_data_dump) {
  (void)model_id;

  if ((instance_handle == nullptr) || (is_data_dump == nullptr)) {
    GELOGW("ModelDumpManager handle or is_data_dump is null, skip");
    return PARAM_INVALID;
  }

  auto* manager = static_cast<ge::dump::ModelDumpManager*>(instance_handle);
  return static_cast<int32_t>(manager->IsDataDumpEnabled(op_name, is_data_dump));
}

}  // extern "C"
