/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/runtime/dump/data_dump_impl.h"
#include "framework/runtime/dump/dump_config.h"
#include "framework/common/debug/ge_log.h"
#include "framework/common/framework_types_internal.h"
#include "rt_external.h"
#include "acl/acl_rt.h"

namespace ge {
namespace dump {
namespace {
constexpr uint32_t kAicpuLoadFlag = 1U;
constexpr uint32_t kAddrLength = static_cast<uint32_t>(sizeof(void *));
constexpr uint64_t kOpDebugShape = 2048U;
constexpr uint64_t kOpDebugSize = 2048U;
const std::string OP_DEBUG_NAME = "Node_OpDebug";
const std::string OP_DEBUG_TYPE = "Opdebug";
}  // namespace

DataDumpImpl::DataDumpImpl() = default;

DataDumpImpl::~DataDumpImpl() {
  Clear();
}

Status DataDumpImpl::SaveTask(const Om2TaskInfo& task_info, ModelTaskType task_type,
                              rtStream_t stream, bool is_op_debug) {
  const char* op_name = (task_info.op_name != nullptr) ? task_info.op_name : "";
  const char* op_type = (task_info.op_type != nullptr) ? task_info.op_type : "";
  GELOGD("SaveTask: op_name=%s, task_id=%u, stream_id=%u, is_op_debug=%d",
         op_name, task_info.task_id, task_info.stream_id, is_op_debug);

  InnerDumpInfo dump_info = {};
  dump_info.task_id = task_info.task_id;
  dump_info.stream_id = task_info.stream_id;
  dump_info.context_id = task_info.context_id;
  dump_info.thread_id = task_info.thread_id;
  dump_info.task_type = task_type;
  dump_info.stream = stream;
  dump_info.is_op_debug = is_op_debug;
  dump_info.args_base = task_info.args_base;
  dump_info.args_size = task_info.args_size;
  dump_info.op_name = op_name;
  dump_info.op_type = op_type;
  dump_info.is_raw_address = task_info.is_raw_address;

  const auto copy_io_entries = [](const Om2TaskIoEntry *entries, const uint32_t entry_num,
                                  std::vector<InnerTensorInfo> &inner_tensors) -> Status {
    if ((entry_num > 0U) && (entries == nullptr)) {
      GELOGE(PARAM_INVALID, "[Check][Param] OM2 task io entries is null, entry_num=%u.", entry_num);
      return PARAM_INVALID;
    }
    inner_tensors.reserve(entry_num);
    for (uint32_t i = 0U; i < entry_num; ++i) {
      const auto &entry = entries[i];
      if (entry.tensor == nullptr) {
        GELOGE(PARAM_INVALID, "[Check][Param] OM2 task io tensor is null, index=%u.", i);
        return PARAM_INVALID;
      }
      const auto &tensor = *entry.tensor;
      if ((tensor.shape_dims_num > 0U) && (tensor.shape_dims == nullptr)) {
        GELOGE(PARAM_INVALID, "[Check][Param] OM2 task io tensor shape dims is null, index=%u, dims_num=%u.",
               i, tensor.shape_dims_num);
        return PARAM_INVALID;
      }

      InnerTensorInfo inner_tensor{};
      inner_tensor.offset = entry.offset;
      inner_tensor.device_address = tensor.device_address;
      inner_tensor.size = tensor.size;
      inner_tensor.data_type = tensor.data_type;
      inner_tensor.format = tensor.format;
      if (tensor.shape_dims_num > 0U) {
        inner_tensor.shape_dims.assign(tensor.shape_dims, tensor.shape_dims + tensor.shape_dims_num);
      }
      inner_tensors.push_back(inner_tensor);
    }
    return SUCCESS;
  };

  Status ret = copy_io_entries(task_info.inputs, task_info.input_num, dump_info.inputs);
  if (ret != SUCCESS) {
    return ret;
  }
  ret = copy_io_entries(task_info.outputs, task_info.output_num, dump_info.outputs);
  if (ret != SUCCESS) {
    return ret;
  }

  if ((task_info.workspace_num > 0U) &&
      ((task_info.workspace_addrs == nullptr) || (task_info.workspace_sizes == nullptr))) {
    GELOGE(PARAM_INVALID, "[Check][Param] OM2 task workspace info is null, workspace_num=%u.",
           task_info.workspace_num);
    return PARAM_INVALID;
  }
  for (uint32_t i = 0U; i < task_info.workspace_num; ++i) {
    dump_info.workspace_addrs.push_back(task_info.workspace_addrs[i]);
    dump_info.workspace_sizes.push_back(task_info.workspace_sizes[i]);
  }

  task_list_.push_back(dump_info);
  return SUCCESS;
}

Status DataDumpImpl::ExecuteLoadDumpInfo(const toolkit::aicpu::dump::OpMappingInfo& op_mapping_info) {
  std::string proto_str;
  const size_t proto_size = op_mapping_info.ByteSizeLong();
  const bool ret = op_mapping_info.SerializeToString(&proto_str);
  if ((!ret) || (proto_size == 0U)) {
    GELOGE(PARAM_INVALID, "[Call][SerializeToString] failed, proto size %zu.", proto_size);
    return PARAM_INVALID;
  }

  if (dev_mem_load_ != nullptr) {
    GELOGW("dev_mem_load_ has been used.");
    (void)aclrtFree(dev_mem_load_);
    dev_mem_load_ = nullptr;
  }

  aclError rt_ret = aclrtMalloc(&dev_mem_load_, proto_size, ACL_MEM_MALLOC_HUGE_FIRST);
  if (rt_ret != ACL_SUCCESS) {
    GELOGE(RT_FAILED, "[Call][aclrtMalloc] failed, size:%zu, ret:%d", proto_size, rt_ret);
    return RT_FAILED;
  }

  rt_ret = aclrtMemcpy(dev_mem_load_, proto_size, proto_str.c_str(), proto_size, ACL_MEMCPY_HOST_TO_DEVICE);
  if (rt_ret != ACL_SUCCESS) {
    GELOGE(RT_FAILED, "[Call][aclrtMemcpy] failed, size:%zu, ret:%d", proto_size, rt_ret);
    (void)aclrtFree(dev_mem_load_);
    dev_mem_load_ = nullptr;
    return RT_FAILED;
  }

  rt_ret = rtDatadumpInfoLoad(dev_mem_load_, static_cast<uint32_t>(proto_size));
  if (rt_ret != RT_ERROR_NONE) {
    GELOGE(RT_FAILED, "[Call][rtDatadumpInfoLoad] failed, length:%zu, ret:%d", proto_size, rt_ret);
    (void)aclrtFree(dev_mem_load_);
    dev_mem_load_ = nullptr;
    return RT_FAILED;
  }

  load_flag_ = true;
  GELOGI("LoadDumpInfo success, proto size is: %zu.", proto_size);
  return SUCCESS;
}

Status DataDumpImpl::BuildOpMappingBasicInfo(const ModelDumpInfo& model_info,
                                                toolkit::aicpu::dump::OpMappingInfo& op_mapping_info) {
  const char* model_name = (model_info.model_name != nullptr) ? model_info.model_name : "";
  op_mapping_info.set_dump_path(DumpConfig::Instance().GetDumpPath() +
                                std::to_string(model_info.device_id) + "/");
  op_mapping_info.set_model_name(model_name);
  op_mapping_info.set_model_id(model_info.model_id);
  op_mapping_info.set_dump_step(DumpConfig::Instance().GetDumpStep());
  op_mapping_info.set_flag(kAicpuLoadFlag);

  // step_id_addr 分配设备内存并初始化为 0，先释放旧的
  if (step_id_dev_addr_ != nullptr) {
    (void)aclrtFree(step_id_dev_addr_);
    step_id_dev_addr_ = nullptr;
  }

  void* step_id_dev_addr = nullptr;
  const aclError ret = aclrtMalloc(&step_id_dev_addr, sizeof(uint32_t), ACL_MEM_MALLOC_HUGE_FIRST);
  if (ret != ACL_SUCCESS) {
    GELOGE(RT_FAILED, "Malloc step_id_addr failed, ret=%d", ret);
    return RT_FAILED;
  }
  const uint32_t zero_val = 0U;
  const aclError cpy_ret = aclrtMemcpy(step_id_dev_addr, sizeof(uint32_t), &zero_val,
                                        sizeof(uint32_t), ACL_MEMCPY_HOST_TO_DEVICE);
  if (cpy_ret != ACL_SUCCESS) {
    GELOGE(RT_FAILED, "Memcpy step_id_addr failed, ret=%d", cpy_ret);
    (void)aclrtFree(step_id_dev_addr);
    return RT_FAILED;
  }
  step_id_dev_addr_ = step_id_dev_addr;
  op_mapping_info.set_step_id_addr(reinterpret_cast<uint64_t>(step_id_dev_addr));

  // loop_cond_addr 和 iterations_per_loop_addr 保持原逻辑
  if (model_info.loop_cond_addr != 0U) {
    op_mapping_info.set_loop_cond_addr(model_info.loop_cond_addr);
  }
  if (model_info.iterations_per_loop_addr != 0U) {
    op_mapping_info.set_iterations_per_loop_addr(model_info.iterations_per_loop_addr);
  }

  // 设置 dump_data
  const std::string dump_data_str = DumpConfig::Instance().GetDumpData();
  if (dump_data_str == "stats") {
    op_mapping_info.set_dump_data(toolkit::aicpu::dump::DumpData::STATS_DUMP_DATA);
  } else {
    op_mapping_info.set_dump_data(toolkit::aicpu::dump::DumpData::TENSOR_DUMP_DATA);
  }
  return ge::SUCCESS;
}

Status DataDumpImpl::BuildTaskList(toolkit::aicpu::dump::OpMappingInfo& op_mapping_info) const {
  for (const auto& dump_info : task_list_) {
    toolkit::aicpu::dump::Task* task = op_mapping_info.add_task();
    GE_CHECK_NOTNULL(task);
    GELOGD("BuildTaskList: task_id=%u, stream_id=%u, args_base=0x%lx, args_size=%zu, is_raw_address=%u",
           dump_info.task_id, dump_info.stream_id, dump_info.args_base, dump_info.args_size,
           dump_info.is_raw_address);
    task->set_task_id(dump_info.task_id);
    task->set_stream_id(dump_info.stream_id);
    task->set_context_id(dump_info.context_id);
    task->set_thread_id(dump_info.thread_id);

    // 设置 op 信息
    toolkit::aicpu::dump::Op* op = task->mutable_op();
    if (op != nullptr) {
      op->set_op_name(dump_info.op_name);
      op->set_op_type(dump_info.op_type);
    }

    BuildTaskInputs(dump_info, *task);
    BuildTaskOutputs(dump_info, *task);
    BuildTaskWorkspaces(dump_info, *task);
  }
  return SUCCESS;
}

void DataDumpImpl::BuildTaskInputs(const InnerDumpInfo& dump_info, toolkit::aicpu::dump::Task& task) const {
  const std::string& dump_mode = DumpConfig::Instance().GetDumpMode();
  const bool need_dump_input = (dump_mode == GE_DUMP_MODE_INPUT) || (dump_mode == GE_DUMP_MODE_ALL) ||
                               dump_info.is_op_debug;
  if (!need_dump_input) {
    GELOGD("Skip dump input for task_id=%u, dump_mode=%s, is_op_debug=%u",
           dump_info.task_id, dump_mode.c_str(), dump_info.is_op_debug);
    return;
  }

  for (size_t i = 0; i < dump_info.inputs.size(); ++i) {
    toolkit::aicpu::dump::Input* input_tensor = task.add_input();
    const auto& tensor = dump_info.inputs[i];

    // 对齐 v1：直接写 args + offset 地址给 AICPU，不解引用
    uint64_t device_address = tensor.device_address;
    auto addr_type = toolkit::aicpu::dump::AddressType::TRADITIONAL_ADDR;
    if (dump_info.is_raw_address) {
      addr_type = toolkit::aicpu::dump::AddressType::RAW_ADDR;
    } else if (dump_info.args_base != 0U && tensor.offset != UINT64_MAX) {
      device_address = dump_info.args_base + tensor.offset;
    }

    GELOGD("BuildTaskInputs: task_id=%u, input[%zu], args_base=0x%lx, offset=0x%lx, device_address=0x%lx, size=%lu, is_raw=%d",
           dump_info.task_id, i, dump_info.args_base, static_cast<uint64_t>(tensor.offset), device_address, tensor.size,
           dump_info.is_raw_address);

    input_tensor->set_data_type(tensor.data_type);
    input_tensor->set_format(tensor.format);
    input_tensor->set_address(device_address);
    input_tensor->set_size(tensor.size);
    input_tensor->set_addr_type(addr_type);
    for (const auto dim : tensor.shape_dims) {
      input_tensor->mutable_shape()->add_dim(static_cast<uint64_t>(dim));
    }
  }
}

void DataDumpImpl::BuildTaskOutputs(const InnerDumpInfo& dump_info, toolkit::aicpu::dump::Task& task) const {
  const std::string& dump_mode = DumpConfig::Instance().GetDumpMode();
  const bool need_dump_output = (dump_mode == GE_DUMP_MODE_OUTPUT) || (dump_mode == GE_DUMP_MODE_ALL) ||
                                dump_info.is_op_debug;
  if (!need_dump_output) {
    GELOGD("Skip dump output for task_id=%u, dump_mode=%s, is_op_debug=%u",
           dump_info.task_id, dump_mode.c_str(), dump_info.is_op_debug);
    return;
  }

  for (size_t i = 0; i < dump_info.outputs.size(); ++i) {
    toolkit::aicpu::dump::Output* output_tensor = task.add_output();
    const auto& tensor = dump_info.outputs[i];

    // 对齐 v1：直接写 args + offset 地址给 AICPU，不解引用
    uint64_t device_address = tensor.device_address;
    auto addr_type = toolkit::aicpu::dump::AddressType::TRADITIONAL_ADDR;
    if (dump_info.is_raw_address) {
      addr_type = toolkit::aicpu::dump::AddressType::RAW_ADDR;
    } else if (dump_info.args_base != 0U && tensor.offset != UINT64_MAX) {
      device_address = dump_info.args_base + tensor.offset;
    }

    GELOGD("BuildTaskOutputs: task_id=%u, output[%zu], args_base=0x%lx, offset=0x%lx, device_address=0x%lx, size=%lu, is_raw=%d",
           dump_info.task_id, i, dump_info.args_base, static_cast<uint64_t>(tensor.offset), device_address, tensor.size,
           dump_info.is_raw_address);

    output_tensor->set_data_type(tensor.data_type);
    output_tensor->set_format(tensor.format);
    output_tensor->set_address(device_address);
    output_tensor->set_size(tensor.size);
    output_tensor->set_addr_type(addr_type);
    for (const auto dim : tensor.shape_dims) {
      output_tensor->mutable_shape()->add_dim(static_cast<uint64_t>(dim));
    }
  }
}

void DataDumpImpl::BuildTaskWorkspaces(const InnerDumpInfo& dump_info, toolkit::aicpu::dump::Task& task) const {
  // workspace 只在 op_debug 模式下才需要 dump（用于溢出/异常调试）
  if (!dump_info.is_op_debug) {
    GELOGD("Skip dump workspace for task_id=%u, is_op_debug=%u",
           dump_info.task_id, dump_info.is_op_debug);
    return;
  }

  for (size_t i = 0; i < dump_info.workspace_addrs.size(); ++i) {
    toolkit::aicpu::dump::Workspace* workspace = task.add_space();
    GELOGD("BuildTaskWorkspaces: task_id=%u, workspace[%zu], data_addr=0x%lx, size=%lu",
           dump_info.task_id, i, dump_info.workspace_addrs[i], dump_info.workspace_sizes[i]);
    workspace->set_data_addr(dump_info.workspace_addrs[i]);
    workspace->set_size(dump_info.workspace_sizes[i]);
    workspace->set_type(toolkit::aicpu::dump::Workspace::LOG);
  }
}

void DataDumpImpl::SetOpDebugInfo(uint32_t task_id, uint32_t stream_id, void* debug_addr) {
  is_op_debug_ = true;
  op_debug_task_id_ = task_id;
  op_debug_stream_id_ = stream_id;
  op_debug_addr_ = debug_addr;
}

void DataDumpImpl::BuildOpDebugTask(toolkit::aicpu::dump::OpMappingInfo& op_mapping_info) const {
  if (!is_op_debug_) {
    return;
  }

  GELOGI("Add op_debug_info to aicpu, task_id=%u, stream_id=%u",
         op_debug_task_id_, op_debug_stream_id_);

  toolkit::aicpu::dump::Task task;
  task.set_end_graph(false);
  task.set_task_id(op_debug_task_id_);
  task.set_stream_id(op_debug_stream_id_);
  task.mutable_op()->set_op_name(OP_DEBUG_NAME);
  task.mutable_op()->set_op_type(OP_DEBUG_TYPE);

  // set output
  toolkit::aicpu::dump::Output output;
  output.set_original_name(OP_DEBUG_NAME);
  output.set_original_output_index(0);
  output.set_original_output_format(FORMAT_ND);
  output.set_original_output_data_type(DT_UINT8);
  output.set_data_type(DT_UINT8);
  output.set_format(FORMAT_ND);
  output.mutable_shape()->add_dim(kOpDebugShape);
  output.set_address(reinterpret_cast<uintptr_t>(op_debug_addr_));
  output.set_size(kOpDebugSize);
  output.set_addr_type(toolkit::aicpu::dump::AddressType::TRADITIONAL_ADDR);

  task.mutable_output()->Add(std::move(output));
  op_mapping_info.mutable_task()->Add(std::move(task));
}

Status DataDumpImpl::BuildAndLoadOpMappingInfo(const ModelDumpInfo& model_info) {
  GELOGI("BuildAndLoadOpMappingInfo: model_id=%u, task_count=%zu, dump_data=%s, dump_mode=%s, is_op_debug=%d",
         model_info.model_id, task_list_.size(), DumpConfig::Instance().GetDumpData().c_str(),
         DumpConfig::Instance().GetDumpMode().c_str(), is_op_debug_);

  // 如果没有普通算子要 dump，也没有 overflow dump，直接返回
  if (task_list_.empty() && !is_op_debug_) {
    GELOGI("No task to dump, skip build and load op mapping info");
    return SUCCESS;
  }

  toolkit::aicpu::dump::OpMappingInfo op_mapping_info;
  Status ret = BuildOpMappingBasicInfo(model_info, op_mapping_info);
  if (ret != SUCCESS) {
    GELOGE(ret, "Build op mapping basic info failed, ret=%u", ret);
    return ret;
  }

  ret = BuildTaskList(op_mapping_info);
  if (ret != SUCCESS) {
    GELOGE(ret, "[Build][TaskList] failed, ret:%u", ret);
    return ret;
  }

  // 添加 overflow dump 的特殊 Task（包含 p2p_debug_addr）
  BuildOpDebugTask(op_mapping_info);

  // 打印 op_mapping_info 便于调试（和 test1.cpp 格式对齐）
  GELOGD("========== Dump OpMappingInfo Start ==========");
  GELOGD("dump_path: %s", op_mapping_info.dump_path().c_str());
  GELOGD("model_name: %s", op_mapping_info.model_name().c_str());
  GELOGD("model_id: %u", op_mapping_info.model_id());
  GELOGD("step_id_addr: 0x%lx", op_mapping_info.step_id_addr());
  GELOGD("loop_cond_addr: 0x%lx", op_mapping_info.loop_cond_addr());
  GELOGD("iterations_per_loop_addr: 0x%lx", op_mapping_info.iterations_per_loop_addr());
  GELOGD("flag: %u", op_mapping_info.flag());
  GELOGD("dump_step: %s", op_mapping_info.dump_step().c_str());
  GELOGD("dump_data: %d", op_mapping_info.dump_data());
  GELOGD("task count: %d", op_mapping_info.task_size());

  for (int32_t i = 0; i < op_mapping_info.task_size(); ++i) {
    const auto &task = op_mapping_info.task(i);
    GELOGD("---------- Task[%d] ----------", i);
    GELOGD("  task_id: %u", task.task_id());
    GELOGD("  stream_id: %u", task.stream_id());
    GELOGD("  context_id: %u", task.context_id());
    GELOGD("  thread_id: %u", task.thread_id());
    GELOGD("  op_name: %s", task.op().op_name().c_str());
    GELOGD("  op_type: %s", task.op().op_type().c_str());
    GELOGD("  end_graph: %d", task.end_graph());
    GELOGD("  input count: %d", task.input_size());
    for (int32_t j = 0; j < task.input_size(); ++j) {
      const auto &input = task.input(j);
      std::string shape_str;
      for (int32_t k = 0; k < input.shape().dim_size(); ++k) {
        shape_str += (k == 0 ? "" : ", ") + std::to_string(input.shape().dim(k));
      }
      GELOGD("    input[%d]: addr=0x%lx, size=%u, format=%d, data_type=%d, "
             "addr_type=%d, offset=%lu, shape=[%s]",
             j, input.address(), static_cast<uint32_t>(input.size()),
             input.format(), input.data_type(), input.addr_type(), input.offset(),
             shape_str.c_str());
    }
    GELOGD("  output count: %d", task.output_size());
    for (int32_t j = 0; j < task.output_size(); ++j) {
      const auto &output = task.output(j);
      std::string shape_str;
      for (int32_t k = 0; k < output.shape().dim_size(); ++k) {
        shape_str += (k == 0 ? "" : ", ") + std::to_string(output.shape().dim(k));
      }
      GELOGD("    output[%d]: addr=0x%lx, size=%u, format=%d, data_type=%d, "
             "addr_type=%d, offset=%lu, shape=[%s]",
             j, output.address(), static_cast<uint32_t>(output.size()),
             output.format(), output.data_type(), output.addr_type(), output.offset(),
             shape_str.c_str());
    }
    GELOGD("  context count: %d", task.context_size());
    for (int32_t j = 0; j < task.context_size(); ++j) {
      const auto &context = task.context(j);
      GELOGD("    context[%d]: context_id=%u, thread_id=%u",
             j, context.context_id(), context.thread_id());
    }
    GELOGD("  workspace count: %d", task.space_size());
    for (int32_t j = 0; j < task.space_size(); ++j) {
      const auto &ws = task.space(j);
      GELOGD("    workspace[%d]: addr=0x%lx, size=%u, type=%d",
             j, ws.data_addr(), static_cast<uint32_t>(ws.size()), ws.type());
    }
  }
  GELOGD("========== Dump OpMappingInfo End ==========");

  ret = ExecuteLoadDumpInfo(op_mapping_info);
  if (ret != SUCCESS) {
    GELOGE(ret, "[Execute][LoadDumpInfo] failed, ret:%u", ret);
    return ret;
  }

  op_mapping_info_ = std::move(op_mapping_info);
  GELOGI("BuildAndLoadOpMappingInfo success, task_count=%zu", task_list_.size());
  return SUCCESS;
}

void DataDumpImpl::Clear() {
  if (dev_mem_load_ != nullptr) {
    (void)aclrtFree(dev_mem_load_);
    dev_mem_load_ = nullptr;
  }
  if (step_id_dev_addr_ != nullptr) {
    (void)aclrtFree(step_id_dev_addr_);
    step_id_dev_addr_ = nullptr;
  }
  task_list_.clear();
  load_flag_ = false;
}

}  // namespace dump
}  // namespace ge
