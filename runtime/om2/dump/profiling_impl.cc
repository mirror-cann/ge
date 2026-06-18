/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/profiling_impl.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "common/checker.h"
#include "framework/common/debug/ge_log.h"
#include "framework/runtime/dump/profiling_config.h"
#include "graph_metadef/common/opskernel/ops_kernel_info_types.h"
#include "mmpa/mmpa_api.h"
#include "aprof_pub.h"
#include "profiling/prof_common.h"

namespace ge {
namespace dump {
namespace {
constexpr uint32_t kAgingFlag = 1U;
constexpr uint32_t kNonAgingFlag = 0U;
constexpr uint32_t kModelGraphIdMapDataLen = 16U;
constexpr uint32_t kOm1ModelLoadType = MSPROF_REPORT_MODEL_GRAPH_ID_MAP_TYPE + 2U;
constexpr uint32_t kTensorInfoBytes = 44U;
constexpr uint32_t kTensorInfoBytesWithCap = 56U;
constexpr uint32_t kInvalidContextId = 0xFFFFFFFFU;

const std::map<ModelTaskType, MsprofGeTaskType> kModelTaskTypeToProfTaskType = {
    {ModelTaskType::MODEL_TASK_KERNEL, MSPROF_GE_TASK_TYPE_AI_CORE},
    {ModelTaskType::MODEL_TASK_VECTOR_KERNEL, MSPROF_GE_TASK_TYPE_AIV},
    {ModelTaskType::MODEL_TASK_VECTOR_ALL_KERNEL, MSPROF_GE_TASK_TYPE_AIV},
    {ModelTaskType::MODEL_TASK_KERNEL_EX, MSPROF_GE_TASK_TYPE_AI_CPU},
    {ModelTaskType::MODEL_TASK_DSA, MSPROF_GE_TASK_TYPE_DSA},
    {ModelTaskType::MODEL_TASK_HCCL, MSPROF_GE_TASK_TYPE_HCCL},
    {ModelTaskType::MODEL_TASK_ALL_KERNEL, MSPROF_GE_TASK_TYPE_AI_CORE},
    {ModelTaskType::MODEL_TASK_SUPER_KERNEL, MSPROF_GE_TASK_TYPE_AI_CORE},
    {ModelTaskType::MODEL_TASK_FUSION_KERNEL, MSPROF_GE_TASK_TYPE_AI_CORE},
    {ModelTaskType::MODEL_TASK_KERNEL_LAUNCH_V2, MSPROF_GE_TASK_TYPE_AI_CORE},
    {ModelTaskType::MODEL_TASK_CUSTOM_KERNEL, MSPROF_GE_TASK_TYPE_AI_CORE},
};

Status CheckMsprofRet(int32_t ret, const char *action, const char *name) {
  if ((ret == MSPROF_ERROR_NONE) || (ret == MSPROF_ERROR_UNINITIALIZE)) {
    return SUCCESS;
  }
  GELOGW("%s failed, name=%s, ret=%d", action, name, ret);
  return FAILED;
}

void FillProfTensorDesc(const TaskDescInfo &task_desc_info, size_t tensor_index, size_t offset_idx,
                        MsprofTensorInfo *tensor_info) {
  const bool is_input = tensor_index < task_desc_info.input_shape.size();
  const auto &formats = is_input ? task_desc_info.input_format : task_desc_info.output_format;
  const auto &data_types = is_input ? task_desc_info.input_data_type : task_desc_info.output_data_type;
  const auto &shapes = is_input ? task_desc_info.input_shape : task_desc_info.output_shape;
  const size_t desc_index = is_input ? tensor_index : tensor_index - task_desc_info.input_shape.size();

  auto &tensor_data = tensor_info->tensorData[offset_idx];
  tensor_data.tensorType = static_cast<uint32_t>(is_input ? MSPROF_GE_TENSOR_TYPE_INPUT : MSPROF_GE_TENSOR_TYPE_OUTPUT);
  tensor_data.format = static_cast<uint32_t>(formats[desc_index]);
  const DataType data_type = data_types[desc_index];
  tensor_data.dataType = (static_cast<uint32_t>(data_type) < static_cast<uint32_t>(DT_MAX))
                             ? static_cast<uint32_t>(data_type)
                             : static_cast<uint32_t>(DT_UNDEFINED);
  const size_t shape_size = shapes[desc_index].size();
  const size_t copy_size = std::min(static_cast<size_t>(MSPROF_GE_TENSOR_DATA_SHAPE_LEN), shape_size);
  for (size_t i = 0U; i < copy_size; ++i) {
    tensor_data.shape[i] = static_cast<uint32_t>(shapes[desc_index][i]);
  }
  if (shape_size < static_cast<size_t>(MSPROF_GE_TENSOR_DATA_SHAPE_LEN)) {
    tensor_data.shape[shape_size] = 0U;
  }
}

void BuildSingleTensorInfo(const TaskDescInfo &task_desc_info, uint32_t tid, size_t index, uint32_t tensor_num,
                           MsprofAdditionalInfo &tensor_info) {
  tensor_info.type = MSPROF_REPORT_NODE_TENSOR_INFO_TYPE;
  tensor_info.level = static_cast<uint16_t>(MSPROF_REPORT_NODE_LEVEL);
  tensor_info.timeStamp = task_desc_info.prof_time;
  tensor_info.threadId = tid;
  tensor_info.dataLen = kTensorInfoBytesWithCap + (tensor_num - 1U) * kTensorInfoBytes;
  auto *tensor_data = reinterpret_cast<MsprofTensorInfo *>(tensor_info.data);
  tensor_data->opName = MsprofGetHashId(task_desc_info.op_name.c_str(), task_desc_info.op_name.length());
  tensor_data->tensorNum = tensor_num;
  for (size_t i = 0U; i < static_cast<size_t>(tensor_num); ++i) {
    FillProfTensorDesc(task_desc_info, index * static_cast<size_t>(MSPROF_GE_TENSOR_DATA_NUM) + i, i, tensor_data);
  }
}

void AppendTensorInfo(const Om2TaskIoEntry *entries, uint64_t entry_num, const char *op_name,
                      std::vector<Format> &formats, std::vector<DataType> &data_types,
                      std::vector<std::vector<int64_t>> &shapes) {
  if (entries == nullptr) {
    if (entry_num != 0U) {
      GELOGW("Task io entries is null, op_name=%s, entry_num=%llu", op_name,
             static_cast<unsigned long long>(entry_num));
    }
    return;
  }

  for (uint64_t i = 0U; i < entry_num; ++i) {
    const Om2Tensor *tensor = entries[i].tensor;
    if (tensor == nullptr) {
      GELOGW("Task io tensor is null, op_name=%s, index=%llu", op_name, static_cast<unsigned long long>(i));
      continue;
    }
    if ((tensor->shape_dims == nullptr) && (tensor->shape_dims_num != 0U)) {
      GELOGW("Task io tensor shape is null, op_name=%s, index=%llu, shape_dims_num=%llu", op_name,
             static_cast<unsigned long long>(i), static_cast<unsigned long long>(tensor->shape_dims_num));
      continue;
    }

    formats.emplace_back(static_cast<Format>(tensor->format));
    data_types.emplace_back(static_cast<DataType>(tensor->data_type));
    std::vector<int64_t> shape;
    if (tensor->shape_dims_num != 0U) {
      shape.assign(tensor->shape_dims, tensor->shape_dims + tensor->shape_dims_num);
    }
    shapes.emplace_back(std::move(shape));
  }
}
}  // namespace

Status ProfilingImpl::ReportModelLoadBegin(const ModelDumpInfo &model_info) const {
  if (!ProfilingConfig::Instance().IsModelLoadEnabled()) {
    GELOGD("Skip reporting OM2 profiling model load begin, model_load profiling disabled, model_id=%u",
           model_info.model_id);
    return SUCCESS;
  }

  const char *model_name = (model_info.model_name != nullptr) ? model_info.model_name : "";
  GELOGD("Report OM2 profiling model load begin, model_id=%u, model_name=%s", model_info.model_id, model_name);
  MsprofEvent model_load_event{};
  model_load_event.type = kOm1ModelLoadType;
  model_load_event.itemId = model_info.model_id;
  model_load_event.level = MSPROF_REPORT_MODEL_LEVEL;
  model_load_event.timeStamp = MsprofSysCycleTime();
  model_load_event.threadId = 0U;
  model_load_event.requestId = 0U;
  return CheckMsprofRet(MsprofReportEvent(kNonAgingFlag, &model_load_event), "Report model load begin", model_name);
}

Status ProfilingImpl::ReportModelLoadEnd(const ModelDumpInfo &model_info) const {
  if (!ProfilingConfig::Instance().IsModelLoadEnabled()) {
    GELOGD("Skip reporting OM2 profiling model load end, model_load profiling disabled, model_id=%u",
           model_info.model_id);
    return SUCCESS;
  }

  const char *model_name = (model_info.model_name != nullptr) ? model_info.model_name : "";
  GELOGD("Report OM2 profiling model load end, model_id=%u, model_name=%s", model_info.model_id, model_name);
  const uint64_t prof_time = MsprofSysCycleTime();
  const uint32_t tid = 0U;

  MsprofAdditionalInfo graph_id_info{};
  graph_id_info.level = MSPROF_REPORT_MODEL_LEVEL;
  graph_id_info.type = MSPROF_REPORT_MODEL_GRAPH_ID_MAP_TYPE;
  graph_id_info.timeStamp = prof_time;
  graph_id_info.threadId = tid;
  graph_id_info.dataLen = kModelGraphIdMapDataLen;
  auto *graph_id_data = reinterpret_cast<MsprofGraphIdInfo *>(graph_id_info.data);
  graph_id_data->graphId = UINT32_MAX;
  graph_id_data->modelName = MsprofGetHashId(model_name, strlen(model_name));
  graph_id_data->modelId = model_info.model_id;
  GE_CHK_STATUS_RET(CheckMsprofRet(MsprofReportAdditionalInfo(kNonAgingFlag, &graph_id_info,
                                                              static_cast<uint32_t>(sizeof(MsprofAdditionalInfo))),
                                   "Report model graph id map", model_name));

  MsprofEvent model_load_event{};
  model_load_event.type = kOm1ModelLoadType;
  model_load_event.itemId = model_info.model_id;
  model_load_event.level = MSPROF_REPORT_MODEL_LEVEL;
  model_load_event.timeStamp = prof_time;
  model_load_event.threadId = tid;
  model_load_event.requestId = 0U;
  return CheckMsprofRet(MsprofReportEvent(kNonAgingFlag, &model_load_event), "Report model load end", model_name);
}

Status ProfilingImpl::SaveTaskInfo(const Om2TaskInfo &task_info, const ModelDumpInfo &model_info) const {
  const char *op_name = (task_info.op_name != nullptr) ? task_info.op_name : "";
  if (!ProfilingConfig::Instance().IsTaskReportEnabled()) {
    GELOGD("Skip saving OM2 profiling task info, task profiling disabled, op_name=%s, task_type=%u", op_name,
           task_info.task_type);
    return SUCCESS;
  }

  GELOGD("Save OM2 profiling task info, op_name=%s, op_type=%s, task_type=%u, task_id=%u, stream_id=%u, "
         "block_dim=%u, input_num=%llu, output_num=%u, workspace_num=%u, context_id=%u, thread_id=%u", op_name,
         task_info.op_type != nullptr ? task_info.op_type : "", task_info.task_type, task_info.task_id,
         task_info.stream_id, task_info.block_dim, static_cast<unsigned long long>(task_info.input_num),
         task_info.output_num, task_info.workspace_num, task_info.context_id, task_info.thread_id);
  TaskDescInfo task_desc_info{};
  uint32_t prof_task_type = static_cast<uint32_t>(MSPROF_GE_TASK_TYPE_INVALID);
  GE_CHK_STATUS_RET(BuildTaskDescInfo(task_info, model_info, task_desc_info, prof_task_type));
  if (prof_task_type == static_cast<uint32_t>(MSPROF_GE_TASK_TYPE_INVALID)) {
    GELOGD("Skip reporting OM2 profiling task info, unsupported task type, op_name=%s, task_type=%u", op_name,
           task_info.task_type);
    return SUCCESS;
  }
  return ReportTaskDescInfo(task_desc_info, prof_task_type, task_info.thread_id);
}

Status ProfilingImpl::BuildTaskDescInfo(const Om2TaskInfo &task_info, const ModelDumpInfo &model_info,
                                                TaskDescInfo &task_desc_info, uint32_t &prof_task_type) const {
  const auto model_task_type = static_cast<ModelTaskType>(task_info.task_type);
  const auto iter = kModelTaskTypeToProfTaskType.find(model_task_type);
  if (iter == kModelTaskTypeToProfTaskType.end()) {
    GELOGD("Skip unsupported profiling task type: %u", task_info.task_type);
    prof_task_type = static_cast<uint32_t>(MSPROF_GE_TASK_TYPE_INVALID);
    return SUCCESS;
  }

  const char *op_name = (task_info.op_name != nullptr) ? task_info.op_name : "";
  task_desc_info.prof_time = MsprofSysCycleTime();
  task_desc_info.model_name = (model_info.model_name != nullptr) ? model_info.model_name : "";
  task_desc_info.op_name = op_name;
  task_desc_info.op_type = (task_info.op_type != nullptr) ? task_info.op_type : "";
  task_desc_info.block_dim = task_info.block_dim;
  task_desc_info.task_id = task_info.task_id;
  task_desc_info.stream_id = task_info.stream_id;
  task_desc_info.cur_iter_num = 0;
  task_desc_info.task_type = std::to_string(static_cast<uint32_t>(iter->second));
  task_desc_info.context_id = task_info.context_id;
  prof_task_type = static_cast<uint32_t>(iter->second);

  AppendTensorInfo(task_info.inputs, task_info.input_num, op_name, task_desc_info.input_format,
                   task_desc_info.input_data_type, task_desc_info.input_shape);
  AppendTensorInfo(task_info.outputs, task_info.output_num, op_name, task_desc_info.output_format,
                   task_desc_info.output_data_type, task_desc_info.output_shape);
  GELOGD("Build OM2 profiling task desc, op_name=%s, prof_task_type=%u, input_desc_num=%zu, output_desc_num=%zu",
         op_name, prof_task_type, task_desc_info.input_shape.size(), task_desc_info.output_shape.size());
  return SUCCESS;
}

Status ProfilingImpl::ReportTaskDescInfo(const TaskDescInfo &task_desc_info, uint32_t prof_task_type,
                                          uint32_t tid) const {
  MsprofCompactInfo node_basic_info{};
  node_basic_info.level = static_cast<uint16_t>(MSPROF_REPORT_NODE_LEVEL);
  node_basic_info.type = MSPROF_REPORT_NODE_BASIC_INFO_TYPE;
  node_basic_info.timeStamp = task_desc_info.prof_time;
  node_basic_info.threadId = tid;
  auto &prof_node_basic_info = node_basic_info.data.nodeBasicInfo;
  prof_node_basic_info.opName = MsprofGetHashId(task_desc_info.op_name.c_str(), task_desc_info.op_name.length());
  prof_node_basic_info.opType = MsprofGetHashId(task_desc_info.op_type.c_str(), task_desc_info.op_type.length());
  prof_node_basic_info.taskType = prof_task_type;
  prof_node_basic_info.blockDim = task_desc_info.block_dim;
  GELOGD("Report OM2 profiling compact info, op_name=%s, prof_task_type=%u, block_dim=%u, task_id=%u, "
         "stream_id=%u, tid=%u", task_desc_info.op_name.c_str(), prof_task_type, task_desc_info.block_dim,
         task_desc_info.task_id, task_desc_info.stream_id, tid);
  const int32_t ret = MsprofReportCompactInfo(kAgingFlag, &node_basic_info,
                                              static_cast<uint32_t>(sizeof(MsprofCompactInfo)));
  if ((ret != MSPROF_ERROR_NONE) && (ret != MSPROF_ERROR_UNINITIALIZE)) {
    GELOGW("Report profiling compact info failed, op_name=%s, ret=%d", task_desc_info.op_name.c_str(), ret);
    return FAILED;
  }
  GE_CHK_STATUS_RET(ReportTensorInfo(task_desc_info, node_basic_info.threadId));
  GE_CHK_STATUS_RET(ReportContextIdInfo(task_desc_info, node_basic_info.threadId));
  return SUCCESS;
}

Status ProfilingImpl::ReportTensorInfo(const TaskDescInfo &task_desc_info, uint32_t tid) const {
  const size_t total_num = task_desc_info.input_shape.size() + task_desc_info.output_shape.size();
  GELOGD("Report OM2 profiling tensor info, op_name=%s, input_num=%zu, output_num=%zu, total_num=%zu, tid=%u",
         task_desc_info.op_name.c_str(), task_desc_info.input_shape.size(), task_desc_info.output_shape.size(),
         total_num, tid);
  const size_t batch_num = total_num / static_cast<size_t>(MSPROF_GE_TENSOR_DATA_NUM);
  for (size_t i = 0U; i < batch_num; ++i) {
    MsprofAdditionalInfo tensor_info{};
    BuildSingleTensorInfo(task_desc_info, tid, i, static_cast<uint32_t>(MSPROF_GE_TENSOR_DATA_NUM), tensor_info);
    GE_CHK_STATUS_RET(CheckMsprofRet(MsprofReportAdditionalInfo(kAgingFlag, &tensor_info,
                                                                static_cast<uint32_t>(sizeof(MsprofAdditionalInfo))),
                                     "Report profiling tensor info", task_desc_info.op_name.c_str()));
  }

  const size_t remain_num = total_num % static_cast<size_t>(MSPROF_GE_TENSOR_DATA_NUM);
  if (remain_num == 0U) {
    return SUCCESS;
  }
  MsprofAdditionalInfo tensor_info{};
  BuildSingleTensorInfo(task_desc_info, tid, batch_num, static_cast<uint32_t>(remain_num), tensor_info);
  return CheckMsprofRet(MsprofReportAdditionalInfo(kAgingFlag, &tensor_info,
                                                   static_cast<uint32_t>(sizeof(MsprofAdditionalInfo))),
                        "Report profiling tensor info", task_desc_info.op_name.c_str());
}

Status ProfilingImpl::ReportContextIdInfo(const TaskDescInfo &task_desc_info, uint32_t tid) const {
  if (task_desc_info.context_id == kInvalidContextId) {
    GELOGD("Skip reporting OM2 profiling context id, op_name=%s, context_id=%u", task_desc_info.op_name.c_str(),
           task_desc_info.context_id);
    return SUCCESS;
  }

  GELOGD("Report OM2 profiling context id, op_name=%s, context_id=%u, tid=%u", task_desc_info.op_name.c_str(),
         task_desc_info.context_id, tid);
  MsprofAdditionalInfo context_info{};
  context_info.level = static_cast<uint16_t>(MSPROF_REPORT_NODE_LEVEL);
  context_info.type = MSPROF_REPORT_NODE_CONTEXT_ID_INFO_TYPE;
  context_info.timeStamp = task_desc_info.prof_time;
  context_info.threadId = tid;
  context_info.dataLen = static_cast<uint32_t>(sizeof(MsprofContextIdInfo));
  auto *context_data = reinterpret_cast<MsprofContextIdInfo *>(context_info.data);
  context_data->opName = MsprofGetHashId(task_desc_info.op_name.c_str(), task_desc_info.op_name.length());
  context_data->ctxIdNum = 1U;
  context_data->ctxIds[0] = task_desc_info.context_id;
  return CheckMsprofRet(MsprofReportAdditionalInfo(kAgingFlag, &context_info,
                                                   static_cast<uint32_t>(sizeof(MsprofAdditionalInfo))),
                        "Report profiling context id info", task_desc_info.op_name.c_str());
}

}  // namespace dump
}  // namespace ge
