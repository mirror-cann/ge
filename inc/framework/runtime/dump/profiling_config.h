/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_CONFIG_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_CONFIG_H_

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "common/ge_common/ge_types.h"

namespace ge {
namespace dump {

struct ProfilingOptions {
  bool model_load_enabled = false;
  bool model_execute_enabled = false;
  bool task_time_enabled = false;
  bool task_time_l1_enabled = false;
  bool op_detail_enabled = false;
  bool device_enabled = false;
  uint64_t module = 0U;
  uint32_t cache_flag = 0U;
  std::vector<uint32_t> device_list;
  std::map<std::string, std::string> config_params;
};

class ProfilingConfig {
 public:
  static ProfilingConfig &Instance();

  Status Enable(const ProfilingOptions &options);
  void Disable();

  bool IsTaskTimeEnabled() const;
  bool IsDeviceEnabled() const;
  bool IsModelLoadEnabled() const;
  bool IsTaskReportEnabled() const;
  bool IsEnabled() const;
  ProfilingOptions GetOptions() const;

 private:
  ProfilingConfig() = default;
  mutable std::mutex mutex_;
  ProfilingOptions options_;
  bool enabled_ = false;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_CONFIG_H_
