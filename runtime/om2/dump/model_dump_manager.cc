/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/model_dump_manager.h"
#include "framework/runtime/dump/dump_config.h"
#include "framework/runtime/dump/dump_callback_manager.h"
#include "framework/runtime/dump/data_dump_impl.h"
#include "framework/runtime/dump/exception_dump_impl.h"
#include "framework/runtime/dump/overflow_dump_impl.h"
#include "framework/common/debug/ge_log.h"

namespace ge {
namespace dump {
Status ModelDumpManager::GlobalInit() {
  GELOGD("ModelDumpManager::GlobalInit start");
  DumpConfig::Instance().Reset();
  return DumpCallbackManager::GlobalInit();
}

ModelDumpManager::ModelDumpManager(uint32_t model_id) : model_id_(model_id) {
  GELOGD("ModelDumpManager constructed, model_id=%u", model_id);

  data_dump_impl_ = std::make_unique<DataDumpImpl>();
  exception_impl_ = std::make_unique<ExceptionDumpImpl>();
  overflow_impl_ = std::make_unique<OverflowDumpImpl>();
}

ModelDumpManager::~ModelDumpManager() {
  Clear();
  GELOGD("ModelDumpManager destructed, model_id=%u", model_id_);
}

Status ModelDumpManager::SetModelDumpInfo(const ModelDumpInfo& model_info) {
  const char* model_name = (model_info.model_name != nullptr) ? model_info.model_name : "";
  GELOGD("SetModelDumpInfo: model_id=%u, model_name=%s",
         model_info.model_id, model_name);
  model_info_ = model_info;
  exception_impl_->SetDeviceId(model_info.device_id);

  // 如果 Overflow 开关开启，自动注册
  if (DumpConfig::Instance().IsOverflowDumpEnabled()) {
    Status ret = overflow_impl_->RegisterForModel(model_info.rt_model_handle);
    if (ret != SUCCESS) {
      GELOGE(ret, "Overflow register failed for model_id=%u", model_info.model_id);
      return ret;
    }
    // 将 overflow debug 信息传递给 data_dump_impl，用于构建 Opdebug 算子
    data_dump_impl_->SetOpDebugInfo(overflow_impl_->GetOpDebugTaskId(),
                                    overflow_impl_->GetOpDebugStreamId(),
                                    overflow_impl_->GetOpDebugAddr());
    GELOGD("Set overflow debug info: task_id=%u, stream_id=%u, addr=%p",
           overflow_impl_->GetOpDebugTaskId(), overflow_impl_->GetOpDebugStreamId(),
           overflow_impl_->GetOpDebugAddr());
  }

  return SUCCESS;
}

Status ModelDumpManager::IsDataDumpEnabled(const char* op_name, uint8_t* is_data_dump) const {
  if (is_data_dump == nullptr) {
    GELOGW("is_data_dump is null, skip");
    return PARAM_INVALID;
  }

  const char* safe_op_name = (op_name != nullptr) ? op_name : "";

  // 对齐 v1 KernelTaskInfo.is_data_dump_ 的判断逻辑：
  // is_data_dump_ 是纯 data dump 相关的，只判断：
  // 1. data dump 开关已启用
  // 2. 算子在 dump layer/list 配置中
  const bool need_data_dump = DumpConfig::Instance().IsDataDumpEnabled() &&
                              DumpConfig::Instance().IsOpNeedDump(safe_op_name);

  GELOGD("IsDataDumpEnabled: op_name=%s, need_data_dump=%u",
         safe_op_name, static_cast<uint32_t>(need_data_dump));
  *is_data_dump = need_data_dump ? 1U : 0U;
  return SUCCESS;
}

Status ModelDumpManager::PreprocessOm2TaskInfo(const Om2TaskInfo& task_info) {
  const char* op_name = (task_info.op_name != nullptr) ? task_info.op_name : "";
  GELOGD("PreprocessOm2TaskInfo: op_name=%s, stream_id=%u", op_name, task_info.stream_id);

  if (task_info.l0_exception_dump_info == nullptr) {
    return SUCCESS;
  }

  Status ret = exception_impl_->ReportL0ExceptionDumpInfo(task_info);
  if (ret != SUCCESS) {
    GELOGE(ret, "Report L0 exception dump info failed, op_name=%s", op_name);
    return ret;
  }
  return SUCCESS;
}

Status ModelDumpManager::AddOm2TaskInfo(const Om2TaskInfo& task_info) {
  const char* op_name = (task_info.op_name != nullptr) ? task_info.op_name : "";
  GELOGD("AddOm2TaskInfo: op_name=%s, task_id=%u, stream_id=%u",
         op_name, task_info.task_id, task_info.stream_id);

  // 判断该算子是否需要保存到 data dump：
  // 1. 配置了 data dump 且算子在列表中
  // 2. 开启了 overflow dump（所有算子都需要保存用于定位）
  const bool need_data_dump = DumpConfig::Instance().IsDataDumpEnabled() &&
                              DumpConfig::Instance().IsOpNeedDump(op_name);
  const bool need_overflow_dump = DumpConfig::Instance().IsOverflowDumpEnabled();
  const bool need_save_to_data_dump = need_data_dump || need_overflow_dump;

  GELOGD("AddOm2TaskInfo: op_name=%s, need_data_dump=%u, need_overflow_dump=%u, "
         "need_save_to_data_dump=%u", op_name, need_data_dump, need_overflow_dump,
         need_save_to_data_dump);

  // task_type 类型转换
  ModelTaskType type = static_cast<ModelTaskType>(task_info.task_type);

  // Data Dump / Overflow Dump：保存 Task 信息到 AICPU
  // Overflow dump 需要保存算子的输入输出信息用于问题定位
  if (need_save_to_data_dump) {
    Status ret = data_dump_impl_->SaveTask(task_info, type, task_info.stream,
                                           overflow_impl_->IsOpDebugEnabled());
    if (ret != SUCCESS) {
      GELOGE(ret, "Save task dump info failed, op_name=%s", op_name);
      return ret;
    }
  }

  Status ret = exception_impl_->SaveOpInfo(task_info);
  if (ret != SUCCESS) {
    GELOGE(ret, "Save task exception info failed, op_name=%s", op_name);
    return ret;
  }

  return SUCCESS;
}

Status ModelDumpManager::DispatchDumpInfo() {
  GELOGD("DispatchDumpInfo: model_id=%u", model_id_);

  // Exception dump 优先级最高，不需要下发 data dump 信息
  if (DumpConfig::Instance().IsExceptionDumpEnabled()) {
    GELOGD("Exception dump enabled, skip data dump dispatch");
    return SUCCESS;
  }

  // Data dump 或 Overflow dump：构建 OpMapping Proto 并下发到 AICPU
  // Overflow dump 虽然走 Adump 路径写入数据，但仍需下发 Opdebug 算子的信息给 AICPU
  if (DumpConfig::Instance().IsDataDumpEnabled() || DumpConfig::Instance().IsOverflowDumpEnabled()) {
    return data_dump_impl_->BuildAndLoadOpMappingInfo(model_info_);
  }

  return SUCCESS;
}

bool ModelDumpManager::GetOpDescInfo(const OpDescInfoId& op_id, OpDescInfo& op_info) const {
  return exception_impl_->GetOpDescInfo(op_id, op_info);
}

void ModelDumpManager::Clear() {
  GELOGD("Clear: model_id=%u", model_id_);

  if (data_dump_impl_ != nullptr) {
    data_dump_impl_->Clear();
  }
  if (exception_impl_ != nullptr) {
    exception_impl_->Clear();
  }
  if (overflow_impl_ != nullptr && model_info_.rt_model_handle != nullptr) {
    overflow_impl_->UnregisterForModel(model_info_.rt_model_handle);
  }
}

}  // namespace dump
}  // namespace ge
