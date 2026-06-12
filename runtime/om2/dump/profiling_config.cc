/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/profiling_config.h"

#include "framework/common/debug/ge_log.h"

namespace ge {
namespace dump {

ProfilingConfig &ProfilingConfig::Instance() {
  static ProfilingConfig instance;
  return instance;
}

Status ProfilingConfig::Enable(const ProfilingOptions &options) {
  const std::lock_guard<std::mutex> lock(mutex_);
  options_ = options;
  enabled_ = options_.model_load_enabled || options_.model_execute_enabled || options_.task_time_enabled ||
             options_.task_time_l1_enabled || options_.op_detail_enabled || options_.device_enabled;
  GELOGD("Enable OM2 profiling config, enabled=%u, model_load=%u, model_execute=%u, task_time=%u, "
         "task_time_l1=%u, op_detail=%u, device=%u, module=%llu, cache_flag=%u, device_num=%zu", enabled_,
         options_.model_load_enabled, options_.model_execute_enabled, options_.task_time_enabled,
         options_.task_time_l1_enabled, options_.op_detail_enabled, options_.device_enabled,
         static_cast<unsigned long long>(options_.module), options_.cache_flag, options_.device_list.size());
  return SUCCESS;
}

void ProfilingConfig::Disable() {
  const std::lock_guard<std::mutex> lock(mutex_);
  GELOGD("Disable OM2 profiling config, previous_enabled=%u", enabled_);
  options_ = ProfilingOptions{};
  enabled_ = false;
}

bool ProfilingConfig::IsTaskTimeEnabled() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return enabled_ && options_.task_time_enabled;
}

bool ProfilingConfig::IsDeviceEnabled() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return enabled_ && options_.device_enabled;
}

bool ProfilingConfig::IsModelLoadEnabled() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return enabled_ && options_.model_load_enabled;
}

bool ProfilingConfig::IsTaskReportEnabled() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return enabled_ && (options_.task_time_enabled || options_.task_time_l1_enabled || options_.op_detail_enabled ||
                      options_.device_enabled);
}

bool ProfilingConfig::IsEnabled() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return enabled_;
}

ProfilingOptions ProfilingConfig::GetOptions() const {
  const std::lock_guard<std::mutex> lock(mutex_);
  return options_;
}

}  // namespace dump
}  // namespace ge
