/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/exception_dump_impl.h"
#include "framework/common/debug/ge_log.h"
#include "framework/runtime/dump/dump_config.h"
#include <dump/adump_api.h>
#include "common/dump/adump_opinfo_builder.h"
#include "runtime/kernel.h"
#include <limits>
#include <cstdint>

namespace ge {
namespace dump {
namespace {
constexpr uint64_t kBit8Shift = 56UL;
constexpr uint8_t kCustomLevel2AddrFlag = 0x1U;
constexpr uint8_t kShapeLevel2AddrFlag = 0x2U;
constexpr uint8_t kTilingDataAddrFlag = 0x3U;
constexpr size_t kAtomicIndex = 0UL;
constexpr size_t kSizeNumIndex = 1UL;

// 辅助函数：序列化 vector 成字符串
template<typename T>
std::string ToString(const std::vector<T>& vec) {
  std::stringstream ss;
  ss << "[";
  for (size_t i = 0; i < vec.size(); ++i) {
    if (i > 0) {
      ss << ", ";
    }
    ss << vec[i];
  }
  ss << "]";
  return ss.str();
}

void BuildInputTensorInfos(const Om2TaskInfo& task_info, std::vector<Adx::TensorInfoV2>& tensor_infos) {
  for (uint32_t i = 0; i < task_info.input_num; ++i) {
    if (task_info.inputs != nullptr && task_info.inputs[i].tensor != nullptr) {
      Adx::TensorInfoV2 tensor_info{};
      tensor_info.tensorSize = task_info.inputs[i].tensor->size;
      tensor_info.dataType = static_cast<ge::DataType>(task_info.inputs[i].tensor->data_type);
      tensor_info.format = static_cast<ge::Format>(task_info.inputs[i].tensor->format);
      tensor_info.placement = gert::TensorPlacement::kOnDeviceHbm;
      tensor_info.type = Adx::TensorType::INPUT;
      tensor_info.argsOffSet = task_info.inputs[i].offset;
      tensor_info.tensorAddr = reinterpret_cast<int64_t*>(static_cast<uintptr_t>(task_info.inputs[i].tensor->device_address));
      tensor_infos.push_back(tensor_info);
    }
  }
}

void BuildOutputTensorInfos(const Om2TaskInfo& task_info, std::vector<Adx::TensorInfoV2>& tensor_infos) {
  for (uint32_t i = 0; i < task_info.output_num; ++i) {
    if (task_info.outputs != nullptr && task_info.outputs[i].tensor != nullptr) {
      Adx::TensorInfoV2 tensor_info{};
      tensor_info.tensorSize = task_info.outputs[i].tensor->size;
      tensor_info.dataType = static_cast<ge::DataType>(task_info.outputs[i].tensor->data_type);
      tensor_info.format = static_cast<ge::Format>(task_info.outputs[i].tensor->format);
      tensor_info.placement = gert::TensorPlacement::kOnDeviceHbm;
      tensor_info.type = Adx::TensorType::OUTPUT;
      tensor_info.argsOffSet = task_info.outputs[i].offset;
      tensor_info.tensorAddr = reinterpret_cast<int64_t*>(static_cast<uintptr_t>(task_info.outputs[i].tensor->device_address));
      tensor_infos.push_back(tensor_info);
    }
  }
}

void BuildWorkspaceTensorInfos(const Om2TaskInfo& task_info, std::vector<Adx::TensorInfoV2>& tensor_infos) {
  for (uint32_t i = 0; i < task_info.workspace_num; ++i) {
    if (task_info.workspace_sizes != nullptr) {
      Adx::TensorInfoV2 tensor_info{};
      tensor_info.tensorSize = task_info.workspace_sizes[i];
      tensor_info.dataType = DT_UINT8;
      tensor_info.format = FORMAT_ND;
      tensor_info.placement = gert::TensorPlacement::kOnDeviceHbm;
      tensor_info.type = Adx::TensorType::WORKSPACE;
      if (task_info.workspace_addrs != nullptr) {
        tensor_info.tensorAddr = reinterpret_cast<int64_t*>(static_cast<uintptr_t>(task_info.workspace_addrs[i]));
      }
      tensor_infos.push_back(tensor_info);
    }
  }
}
}  // namespace

ExceptionDumpImpl::ExceptionDumpImpl(uint32_t device_id) : device_id_(device_id) {
}

ExceptionDumpImpl::~ExceptionDumpImpl() = default;

Status ExceptionDumpImpl::SaveOpInfo(const Om2TaskInfo& task_info) {
  const char* op_name = (task_info.op_name != nullptr) ? task_info.op_name : "";
  const char* op_type = (task_info.op_type != nullptr) ? task_info.op_type : "";
  GELOGD("SaveOpInfo: op_name=%s, task_id=%u, stream_id=%u, context_id=%u, device_id=%u, thread_id=%u",
         op_name, task_info.task_id, task_info.stream_id, task_info.context_id, device_id_, task_info.thread_id);

  // 构建 OpDescInfo
  OpDescInfo op_info = {};
  op_info.op_name = op_name;
  op_info.op_type = op_type;
  op_info.id.task_id = task_info.task_id;
  op_info.id.stream_id = task_info.stream_id;
  op_info.id.context_id = task_info.context_id;
  op_info.id.thread_id = task_info.thread_id;
  op_info.args = task_info.args_base;
  op_info.args_size = task_info.args_size;

  // 保存输入输出张量信息
  if ((task_info.input_num > 0U) && (task_info.inputs == nullptr)) {
    GELOGE(PARAM_INVALID, "[Check][Param] OM2 task input entries is null, input_num=%u.", task_info.input_num);
    return PARAM_INVALID;
  }
  for (uint32_t i = 0; i < task_info.input_num; ++i) {
    const auto &entry = task_info.inputs[i];
    if (entry.tensor == nullptr) {
      GELOGE(PARAM_INVALID, "[Check][Param] OM2 task input tensor is null, index=%u.", i);
      return PARAM_INVALID;
    }
    const auto &tensor = *entry.tensor;
    if ((tensor.shape_dims_num > 0U) && (tensor.shape_dims == nullptr)) {
      GELOGE(PARAM_INVALID, "[Check][Param] OM2 task input tensor shape dims is null, index=%u, dims_num=%u.",
             i, tensor.shape_dims_num);
      return PARAM_INVALID;
    }
    op_info.input_addrs.emplace_back(reinterpret_cast<void*>(tensor.device_address));
    op_info.input_size.emplace_back(tensor.size);
    op_info.input_data_type.emplace_back(static_cast<ge::DataType>(tensor.data_type));
    op_info.input_format.emplace_back(static_cast<ge::Format>(tensor.format));
    std::vector<int64_t> shape;
    for (uint32_t j = 0; j < tensor.shape_dims_num; ++j) {
      shape.emplace_back(tensor.shape_dims[j]);
    }
    op_info.input_shape.emplace_back(shape);
  }

  if ((task_info.output_num > 0U) && (task_info.outputs == nullptr)) {
    GELOGE(PARAM_INVALID, "[Check][Param] OM2 task output entries is null, output_num=%u.", task_info.output_num);
    return PARAM_INVALID;
  }
  for (uint32_t i = 0; i < task_info.output_num; ++i) {
    const auto &entry = task_info.outputs[i];
    if (entry.tensor == nullptr) {
      GELOGE(PARAM_INVALID, "[Check][Param] OM2 task output tensor is null, index=%u.", i);
      return PARAM_INVALID;
    }
    const auto &tensor = *entry.tensor;
    if ((tensor.shape_dims_num > 0U) && (tensor.shape_dims == nullptr)) {
      GELOGE(PARAM_INVALID, "[Check][Param] OM2 task output tensor shape dims is null, index=%u, dims_num=%u.",
             i, tensor.shape_dims_num);
      return PARAM_INVALID;
    }
    op_info.output_addrs.emplace_back(reinterpret_cast<void*>(tensor.device_address));
    op_info.output_size.emplace_back(tensor.size);
    op_info.output_data_type.emplace_back(static_cast<ge::DataType>(tensor.data_type));
    op_info.output_format.emplace_back(static_cast<ge::Format>(tensor.format));
    std::vector<int64_t> shape;
    for (uint32_t j = 0; j < tensor.shape_dims_num; ++j) {
      shape.push_back(tensor.shape_dims[j]);
    }
    op_info.output_shape.push_back(shape);
  }

  // 保存 workspace 信息
  if ((task_info.workspace_num > 0U) &&
      ((task_info.workspace_addrs == nullptr) || (task_info.workspace_sizes == nullptr))) {
    GELOGE(PARAM_INVALID, "[Check][Param] OM2 task workspace info is null, workspace_num=%u.",
           task_info.workspace_num);
    return PARAM_INVALID;
  }
  for (uint32_t i = 0; i < task_info.workspace_num; ++i) {
    op_info.space_addrs.push_back(reinterpret_cast<void*>(task_info.workspace_addrs[i]));
    op_info.workspace_bytes.push_back(task_info.workspace_sizes[i]);
  }

  // 保存到 list，与 V1 exception_dumper 保持一致
  op_info_list_.push_back(op_info);

  // L1 Exception Dump：只有 exception dump 开关打开时才上报给 Adump
  if (DumpConfig::Instance().IsExceptionDumpEnabled()) {
    Status ret = ReportL1ExceptionDumpInfo(task_info, op_info);
    if (ret != SUCCESS) {
      GELOGW("Report L1 exception dump info failed, op=%s", op_name);
    }
  }

  return SUCCESS;
}

bool ExceptionDumpImpl::GetOpDescInfo(const OpDescInfoId& op_id, OpDescInfo& op_info) const {
  GELOGI("[Get][OpDescInfo] There are %zu op info saved, target stream_id:%u, task_id:%u, context_id:%u, thread_id:%u, "
         "dev_id: %u.", op_info_list_.size(), op_id.stream_id, op_id.task_id, op_id.context_id, op_id.thread_id,
         op_id.device_id);

  for (const auto& dump_op_info : op_info_list_) {
    // 打印每个条件的匹配结果
    const bool match_task_id = (dump_op_info.id.task_id == op_id.task_id) || (op_id.task_id == UINT32_MAX);
    const bool match_stream_id = (dump_op_info.id.stream_id == op_id.stream_id) || (op_id.stream_id == UINT32_MAX);
    const bool match_context_id = (dump_op_info.id.context_id == op_id.context_id) || (op_id.context_id == UINT32_MAX);
    const bool match_thread_id = (dump_op_info.id.thread_id == op_id.thread_id) || (op_id.thread_id == UINT32_MAX);
    const bool match_device_id = (dump_op_info.id.device_id == op_id.device_id);

    if (match_task_id && match_stream_id && match_context_id && match_thread_id && match_device_id) {
      GELOGI("[Get][OpDescInfo] Find exception op [%s] of task_id: %u, stream_id: %u, context_id: %u, thread_id: %u, "
             "dev_id: %u.",
             dump_op_info.op_name.c_str(), op_id.task_id, op_id.stream_id, op_id.context_id, op_id.thread_id,
             op_id.device_id);
      op_info = dump_op_info;
      // Note: OM2 doesn't need RefreshAddrs because device_address is already a real device
      // address passed in by Runtime through Om2TaskInfo, not a pointer offset that needs
      // to be resolved from args.
      return true;
    }
  }
  return false;
}

Status ExceptionDumpImpl::ReportL1ExceptionDumpInfo(const Om2TaskInfo& task_info, const OpDescInfo& op_info) const {
  const char* op_name = (task_info.op_name != nullptr) ? task_info.op_name : "";
  const char* op_type = (task_info.op_type != nullptr) ? task_info.op_type : "";
  GELOGD("ReportL1ExceptionDumpInfo: op_name=%s, task_id=%u", op_name, task_info.task_id);

  // 1. 构建 TensorInfoV2 数组
  std::vector<Adx::TensorInfoV2> input_tensor_infos;
  std::vector<Adx::TensorInfoV2> output_tensor_infos;
  std::vector<Adx::TensorInfoV2> workspace_tensor_infos;
  BuildInputTensorInfos(task_info, input_tensor_infos);
  BuildOutputTensorInfos(task_info, output_tensor_infos);
  BuildWorkspaceTensorInfos(task_info, workspace_tensor_infos);

  // 2. 构建并上报
  const bool is_dynamic = false;  // Om2 是静态图
  AdumpOpInfoBuilder builder(op_name, op_type, is_dynamic);
  FillAdumpOpInfoBuilder(op_info, input_tensor_infos, output_tensor_infos, workspace_tensor_infos, builder);

  // 3. 上报给 Adump
  return SubmitToAdump(op_name, task_info, op_info, input_tensor_infos, output_tensor_infos, workspace_tensor_infos, builder);
}

void ExceptionDumpImpl::FillAdumpOpInfoBuilder(const OpDescInfo& op_info,
                                               std::vector<Adx::TensorInfoV2>& input_infos,
                                               std::vector<Adx::TensorInfoV2>& output_infos,
                                               std::vector<Adx::TensorInfoV2>& workspace_infos,
                                               AdumpOpInfoBuilder& builder) const {
  // 对齐 V1 行为：context_id 和 thread_id 传 UINT32_MAX（V1 fallback 逻辑实际未生效）
  builder.Task(device_id_, op_info.id.stream_id, op_info.id.task_id, UINT32_MAX)
    .AdditionInfo(Adx::DUMP_ADDITIONAL_BLOCK_DIM, "1")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_TILING_KEY, "0")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_TILING_DATA, "")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_IMPLY_TYPE, "0")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_ALL_ATTRS, "")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_IS_HOST_ARGS, "false")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_NODE_INFO, "")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_DEV_FUNC, "")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_TVM_MAGIC, "")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_OP_FILE_PATH, "./kernel_meta")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_KERNEL_INFO, "kernel_0")
    .AdditionInfo(Adx::DUMP_ADDITIONAL_WORKSPACE_BYTES, ToString(op_info.workspace_bytes).c_str())
    .AdditionInfo(Adx::DUMP_ADDITIONAL_WORKSPACE_ADDRS, ToString(op_info.space_addrs).c_str())
    .TersorInfo(input_infos)
    .TersorInfo(output_infos)
    .TersorInfo(workspace_infos)
    .DeviceInfo(Adx::DEVICE_INFO_NAME_ARGS, reinterpret_cast<void*>(op_info.args), op_info.args_size);
}

Status ExceptionDumpImpl::SubmitToAdump(const char* op_name, const Om2TaskInfo& task_info, const OpDescInfo& op_info,
                                       std::vector<Adx::TensorInfoV2>& input_infos,
                                       std::vector<Adx::TensorInfoV2>& output_infos,
                                       std::vector<Adx::TensorInfoV2>& workspace_infos,
                                       const AdumpOpInfoBuilder& builder) const {
  const Adx::OperatorInfoV2& info = builder.Build();
  // 对齐 V1 行为：日志打印 UINT32_MAX，与传给 Adump 的值保持一致
  GELOGI("[ReportL1ExceptionDumpInfo] op[%s] dev_id: %u, stream_id: %u, task_id: %u, "
         "context_id: %u, thread_id: %u, args: %#lx, args_size: %zu, "
         "input tensor num: %zu, output tensor num: %zu, workspace tensor num: %zu, "
         "total tensor num: %zu.",
         op_name, device_id_, task_info.stream_id, task_info.task_id,
         UINT32_MAX, UINT32_MAX, op_info.args, op_info.args_size,
         input_infos.size(), output_infos.size(), workspace_infos.size(),
         info.tensorInfos.size());

  const int32_t adx_ret = Adx::AdumpAddExceptionOperatorInfoV2(info);
  if (adx_ret != Adx::ADUMP_SUCCESS) {
    GELOGE(FAILED, "AdumpAddExceptionOperatorInfoV2 failed, ret=%d, op[%s], dev_id: %u, "
           "stream_id: %u, task_id: %u", adx_ret, op_name, device_id_,
           task_info.stream_id, task_info.task_id);
    return FAILED;
  }
  return SUCCESS;
}

void ExceptionDumpImpl::Clear() {
  op_info_list_.clear();
}

}  // namespace dump
}  // namespace ge
