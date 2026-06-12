/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/profiling_callback_manager.h"

#include <string>

#include "framework/common/debug/ge_log.h"
#include "framework/runtime/dump/profiling_config.h"
#include "aprof_pub.h"

extern "C" void RegisterOm2ProfilingCommandNotifier(
    void (*notifier)(const void *, uint32_t)) __attribute__((weak));

namespace ge {
namespace dump {
namespace {
constexpr uint32_t kProfCommandStart = 1U;
constexpr uint32_t kProfCommandStop = 2U;
constexpr uint32_t kProfCommandFinalize = 3U;
constexpr uint32_t kMaxDevNum = 64U;
constexpr int32_t kRtError = -1;
const std::string kDeviceNums = "devNums";
const std::string kDeviceIdList = "devIdList";

std::string BuildDeviceIdList(const MsprofCommandHandle &command) {
  std::string device_ids;
  for (uint32_t i = 0U; i < command.devNums; ++i) {
    if (i != 0U) {
      device_ids.append(",");
    }
    device_ids.append(std::to_string(command.devIdList[i]));
  }
  return device_ids;
}

bool BuildProfilingOptions(const MsprofCommandHandle &command, ProfilingOptions &options) {
  if (command.devNums > kMaxDevNum) {
    GELOGE(PARAM_INVALID, "[Check][DeviceNums]Invalid profiling device num: %u", command.devNums);
    return false;
  }

  options.module = command.profSwitch;
  options.cache_flag = command.cacheFlag;
  options.model_load_enabled = ((command.profSwitch & PROF_MODEL_LOAD_MASK) != 0U);
  options.model_execute_enabled = ((command.profSwitch & PROF_MODEL_EXECUTE_MASK) != 0U);
  options.task_time_enabled = ((command.profSwitch & PROF_TASK_TIME_MASK) != 0U);
  options.task_time_l1_enabled = ((command.profSwitch & PROF_TASK_TIME_L1_MASK) != 0U);
  options.op_detail_enabled = ((command.profSwitch & PROF_OP_DETAIL_MASK) != 0U);
  options.device_enabled = options.task_time_l1_enabled;
  for (uint32_t i = 0U; i < command.devNums; ++i) {
    options.device_list.emplace_back(command.devIdList[i]);
  }
  options.config_params[kDeviceNums] = std::to_string(command.devNums);
  options.config_params[kDeviceIdList] = BuildDeviceIdList(command);
  return true;
}
}  // namespace

ProfilingCallbackManager &ProfilingCallbackManager::GetInstance() {
  static ProfilingCallbackManager instance;
  return instance;
}

Status ProfilingCallbackManager::GlobalInit() {
  GELOGD("ProfilingCallbackManager initialized, registering to OM1 profiling notifier.");
  if (::RegisterOm2ProfilingCommandNotifier != nullptr) {
    ::RegisterOm2ProfilingCommandNotifier([](const void *data, uint32_t len) {
      ProfilingCallbackManager::NotifyProfilingCommand(data, len);
    });
    GELOGD("Register OM2 profiling notifier to OM1 succeeded.");
  } else {
    GELOGD("RegisterOm2ProfilingCommandNotifier is not available, "
           "OM2 profiling config will not be forwarded from OM1.");
  }
  return SUCCESS;
}

rtError_t ProfilingCallbackManager::ProfilingCtrlCallback(const uint32_t ctrl_type, void *const ctrl_data,
                                                          const uint32_t data_len) {
  GELOGD("Receive OM2 profiling ctrl callback, ctrl_type=%u, ctrl_data=%p, data_len=%u", ctrl_type, ctrl_data,
         data_len);
  if ((ctrl_data == nullptr) || (data_len == 0U)) {
    GELOGE(PARAM_INVALID, "[Check][ProfilingCallback]Invalid ctrl data.");
    return static_cast<rtError_t>(kRtError);
  }
  if (ctrl_type != RT_PROF_CTRL_SWITCH) {
    GELOGD("Skip unsupported OM2 profiling ctrl type: %u", ctrl_type);
    return RT_ERROR_NONE;
  }
  GetInstance().HandleCtrlSwitch(ctrl_data, data_len);
  return RT_ERROR_NONE;
}

void ProfilingCallbackManager::NotifyProfilingCommand(const void *ctrl_data, uint32_t data_len) {
  if ((ctrl_data == nullptr) || (data_len < sizeof(MsprofCommandHandle))) {
    GELOGW("[Notify][ProfilingCommand]Invalid ctrl data or length: %u", data_len);
    return;
  }
  GetInstance().HandleCtrlSwitch(ctrl_data, data_len);
}

void ProfilingCallbackManager::HandleCtrlSwitch(const void *ctrl_data, uint32_t data_len) const {
  if ((ctrl_data == nullptr) || (data_len < sizeof(MsprofCommandHandle))) {
    GELOGE(PARAM_INVALID, "[Check][ProfilingCallback]Invalid ctrl data or length: %u", data_len);
    return;
  }

  const auto *command = static_cast<const MsprofCommandHandle *>(ctrl_data);
  GELOGD("Handle OM2 profiling command, type=%u, prof_switch=%llu, cache_flag=%u, dev_num=%u", command->type,
         static_cast<unsigned long long>(command->profSwitch), command->cacheFlag, command->devNums);
  switch (command->type) {
    case kProfCommandStart: {
      ProfilingOptions options;
      if (!BuildProfilingOptions(*command, options)) {
        return;
      }
      GELOGD("Start OM2 profiling, model_load=%u, model_execute=%u, task_time=%u, task_time_l1=%u, op_detail=%u, "
             "device=%u, device_ids=%s", options.model_load_enabled, options.model_execute_enabled,
             options.task_time_enabled, options.task_time_l1_enabled, options.op_detail_enabled,
             options.device_enabled, options.config_params[kDeviceIdList].c_str());
      const Status ret = ProfilingConfig::Instance().Enable(options);
      GELOGD("Start OM2 profiling result, ret=%u", ret);
      break;
    }
    case kProfCommandStop:
      GELOGD("Stop OM2 profiling");
      ProfilingConfig::Instance().Disable();
      break;
    case kProfCommandFinalize:
      GELOGD("Finalize OM2 profiling");
      ProfilingConfig::Instance().Disable();
      break;
    default:
      GELOGD("Skip OM2 profiling command type: %u (init/subscribe/cancel/etc. handled by OM1)", command->type);
      break;
  }
}

}  // namespace dump
}  // namespace ge