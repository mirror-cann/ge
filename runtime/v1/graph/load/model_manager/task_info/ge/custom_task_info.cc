/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/load/model_manager/task_info/ge/custom_task_info.h"

#include <cinttypes>

#include "acl/acl_rt.h"
#include "common/checker.h"
#include "common/tbe_handle_store/tbe_handle_store.h"
#include "exe_graph/runtime/eager_op_execution_context.h"
#include "exe_graph/runtime/update_args_context.h"
#include "framework/runtime/args_handler.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op.h"
#include "graph/debug/ge_util.h"
#include "graph/load/model_manager/model_manager.h"
#include "graph/load/model_manager/model_utils.h"
#include "graph/load/model_manager/sink_only_allocator.h"
#include "graph/load/model_manager/task_info/ge/sink_op_args_handler.h"
#include "graph/manager/graph_var_manager.h"
#include "graph/utils/node_utils.h"

namespace ge {

namespace {
const ge::char_t *const kDumpOutput = "output";
const ge::char_t *const kDumpInput = "input";

bool IsInputDescValid(const ge::GeTensorDesc &input_desc, size_t &invalid_index_num) {
  if (input_desc.IsValid() != ge::GRAPH_SUCCESS) {
    if (invalid_index_num < std::numeric_limits<size_t>::max()) {
      invalid_index_num++;
    }
    return false;
  }
  return true;
}

void GetStorageShape(const ge::GeTensorDesc &tensor_desc, gert::StorageShape &storage_shape) {
  const auto &storage_dims = tensor_desc.GetShape().GetDims();
  for (const auto &dim : storage_dims) {
    (void)storage_shape.MutableStorageShape().AppendDim(dim);
  }
  const auto &origin_dims = tensor_desc.GetOriginShape().GetDims();
  for (const auto &dim : origin_dims) {
    (void)storage_shape.MutableOriginShape().AppendDim(dim);
  }
}

// inputs layout is input tensors
std::vector<void *> GetHoldersRawPtr(const std::vector<std::unique_ptr<uint8_t[]>> &holders) {
  std::vector<void *> holderRawPtr;
  holderRawPtr.reserve(holders.size());
  for (const auto &holder : holders) {
    (void)holderRawPtr.emplace_back(holder.get());
  }
  return holderRawPtr;
}
}

void CustomTaskInfo::SetCustomDumpInfo(const DumpProperties &dump_properties, DumpOp &dump_op) const {
  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  input_addrs.reserve(input_data_addrs_.size());
  output_addrs.reserve(output_data_addrs_.size());
  for (const auto addr : input_data_addrs_) {
    input_addrs.emplace_back(static_cast<uintptr_t>(addr));
  }
  for (const auto addr : output_data_addrs_) {
    output_addrs.emplace_back(static_cast<uintptr_t>(addr));
  }

  dump_op.SetDumpInfo(dump_properties, op_desc_, input_addrs, output_addrs, stream_);
  if (davinci_model_->IsKnownNode()) {
    dump_op.SetLoopAddr(davinci_model_->GetGlobalStep(), 0U, 0U);
  } else {
    dump_op.SetLoopAddr(davinci_model_->GetGlobalStep(), davinci_model_->GetLoopPerIter(),
                        davinci_model_->GetLoopCond());
  }
  dump_op.SetDynamicModelInfo(davinci_model_->GetDumpModelName(),
                              davinci_model_->GetOmName(),
                              davinci_model_->GetDumpModelId());
  dump_op.SetRootGraphName(davinci_model_->GetRootGraphName());
}

Status CustomTaskInfo::UpdateCustomDumpAddrs(const std::vector<uint64_t> &input_addrs_value,
                                             const std::vector<uint64_t> &output_addrs_value) {
  GE_CHECK_NOTNULL(davinci_model_);
  GE_CHECK_NOTNULL(op_desc_);
  if (!davinci_model_->OpNeedDump(op_desc_->GetName())) {
    return SUCCESS;
  }

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  input_addrs.reserve(input_addrs_value.size());
  output_addrs.reserve(output_addrs_value.size());
  for (const auto addr : input_addrs_value) {
    input_addrs.emplace_back(static_cast<uintptr_t>(addr));
  }
  for (const auto addr : output_addrs_value) {
    output_addrs.emplace_back(static_cast<uintptr_t>(addr));
  }

  GE_CHK_STATUS_RET(input_custom_dump_.UpdateAddrs(input_addrs, {}),
                    "[Update][CustomDumpAddrs] fail! op:%s", op_desc_->GetName().c_str());
  GE_CHK_STATUS_RET(output_custom_dump_.UpdateAddrs({}, output_addrs),
                    "[Update][CustomDumpAddrs] fail! op:%s", op_desc_->GetName().c_str());
  return SUCCESS;
}

Status CustomTaskInfo::InsertDumpOp(const std::string &dump_mode) {
  GE_CHECK_NOTNULL(davinci_model_);
  GE_CHECK_NOTNULL(op_desc_);
  if (!davinci_model_->OpNeedDump(op_desc_->GetName())) {
    return SUCCESS;
  }

  GELOGI("Data Dump is on, dump custom op for node: %s, type: %s.",
         op_desc_->GetName().c_str(), op_desc_->GetType().c_str());
  auto custom_dump_properties = davinci_model_->GetDumpProperties();
  DumpOp *dump_op = nullptr;
  if (dump_mode == kDumpInput) {
    if (custom_dump_properties.GetDumpMode() == kDumpOutput) {
      return SUCCESS;
    }
    GELOGI("Insert input dump op for custom node: %s, type: %s.",
           op_desc_->GetName().c_str(), op_desc_->GetType().c_str());
    custom_dump_properties.ClearOpDebugFlag();
    custom_dump_properties.SetDumpMode(kDumpInput);
    dump_op = &input_custom_dump_;
  } else if (dump_mode == kDumpOutput) {
    if (custom_dump_properties.GetDumpMode() == kDumpInput) {
      return SUCCESS;
    }
    GELOGI("Insert output dump op for custom node: %s, type: %s.",
           op_desc_->GetName().c_str(), op_desc_->GetType().c_str());
    custom_dump_properties.ClearOpDebugFlag();
    custom_dump_properties.SetDumpMode(kDumpOutput);
    dump_op = &output_custom_dump_;
  } else {
    return SUCCESS;
  }

  SetCustomDumpInfo(custom_dump_properties, *dump_op);
  return dump_op->LaunchDumpOp(false, false);
}

Status CustomTaskInfo::ParseTaskRunParam(const domi::TaskDef &task_def, DavinciModel *const davinci_model,
                                              TaskRunParam &task_run_param) {
  GELOGI("CustomTaskInfo  ParseTaskRunParam start");
  const domi::KernelDef &kernel_def = task_def.kernel();
  domi::KernelContext context = kernel_def.context();

  GE_CHECK_NOTNULL(davinci_model);
  op_desc_ = davinci_model->GetOpByIndex(context.op_index());
  GE_CHECK_NOTNULL(op_desc_);

  const RuntimeParam &rts_param = davinci_model->GetRuntimeParam();
  input_data_addrs_ = ModelUtils::GetInputAddrsValue(rts_param, op_desc_, input_mem_types_);
  output_data_addrs_ = ModelUtils::GetOutputAddrsValue(rts_param, op_desc_, output_mem_types_);
  workspace_addrs_ = ModelUtils::GetWorkspaceDataAddrsValue(rts_param, op_desc_, workspace_mem_types_);

  AscendString op_type(op_desc_->GetType().c_str());
  is_args_refreshable_ = CustomOpFactory::IsAddressRefreshable(op_type);

  for (size_t i = 0UL; i < input_data_addrs_.size(); i++) {
    task_run_param.parsed_input_addrs.push_back({input_data_addrs_[i], input_mem_types_[i], is_args_refreshable_, {0}});
  }
  for (size_t i = 0UL; i < output_data_addrs_.size(); i++) {
    task_run_param.parsed_output_addrs.push_back({output_data_addrs_[i], output_mem_types_[i], is_args_refreshable_, {0}});
  }
  for (size_t i = 0UL; i < workspace_addrs_.size(); i++) {
    task_run_param.parsed_workspace_addrs.push_back({workspace_addrs_[i], workspace_mem_types_[i], is_args_refreshable_, {0}});
  }
  const auto mem_size = input_data_addrs_.size() + output_data_addrs_.size() + workspace_addrs_.size();
  task_run_param.args_descs.push_back(
      {static_cast<int64_t>(MemSizeAlign(mem_size, sizeof(uintptr_t))), args_placement_});
  GELOGI(
       "Get args size[%u] of op[%s], is known node[%d], task_type: %d, placement: %d.",
       mem_size, op_desc_->GetName().c_str(),
       static_cast<int32_t>(davinci_model->IsFeatureBaseRefreshable()), static_cast<int32_t>(static_cast<ModelTaskType>(task_def.type())),
       args_placement_);
  return SUCCESS;
}

Status CustomTaskInfo::Init(const domi::TaskDef &task_def, DavinciModel *const davinci_model,
                                 const PisToArgs &args, const PisToPersistentWorkspace &persistent_workspace,
                                 const IowAddrs &iow_addrs) {
  GE_CHECK_NOTNULL(davinci_model);
  GE_CHECK_NOTNULL(op_desc_);
  GELOGI("CustomTaskInfo Init Start, op: %s", op_desc_->GetNamePtr());

  (void)persistent_workspace;
  davinci_model_ = davinci_model;
  GE_CHK_STATUS_RET_NOLOG(SetStream(task_def.stream_id(), davinci_model_->GetStreamList()));

  UpdateIoAndWorkspaceAddrs(iow_addrs);
  GE_ASSERT_TRUE((args[static_cast<size_t>(args_placement_)].dev_addr != 0U),
                 "[Check][Param] Op:%s, dev addr is nullptr.", op_desc_->GetName().c_str());
  auto mem_block_manager_allocator = davinci_model_->GetAllocator();
  sink_only_allocator_ = ComGraphMakeShared<gert::memory::SinkOnlyAllocator>();
  sink_only_allocator_->SetAllocator(mem_block_manager_allocator);

  GELOGI("CustomTaskInfo Init Success, node: %s, logic stream id: %u, stream: %p.",
    op_desc_->GetName().c_str(), task_def.stream_id(), stream_);
  return SUCCESS;
}

void CustomTaskInfo::UpdateIoAndWorkspaceAddrs(const IowAddrs &iow_addrs) {
  for (size_t i = 0UL; i < input_data_addrs_.size(); i++) {
    input_data_addrs_[i] = (iow_addrs.input_logic_addrs.empty())
                           ? input_data_addrs_[i] : iow_addrs.input_logic_addrs[i].logic_addr;
    input_mem_types_[i] = (iow_addrs.input_logic_addrs.empty())
                          ? input_mem_types_[i] : iow_addrs.input_logic_addrs[i].memory_type;
  }

  for (size_t i = 0UL; i < output_data_addrs_.size(); i++) {
    output_data_addrs_[i] = (iow_addrs.output_logic_addrs.empty())
                            ? output_data_addrs_[i] : iow_addrs.output_logic_addrs[i].logic_addr;
    output_mem_types_[i] = (iow_addrs.output_logic_addrs.empty())
                           ? output_mem_types_[i] : iow_addrs.output_logic_addrs[i].memory_type;
  }

  for (size_t i = 0UL; i < workspace_addrs_.size(); i++) {
    workspace_addrs_[i] = (iow_addrs.workspace_logic_addrs.empty())
                          ? workspace_addrs_[i] : iow_addrs.workspace_logic_addrs[i].logic_addr;
    workspace_mem_types_[i] = (iow_addrs.workspace_logic_addrs.empty())
                              ? workspace_mem_types_[i] : iow_addrs.workspace_logic_addrs[i].memory_type;
  }
}
Status CustomTaskInfo::ConstructCustomKernelContextInputsOutputs(
    const ge::OpDescPtr &op_desc, std::vector<std::unique_ptr<uint8_t[]>> &inputs,
    std::vector<std::unique_ptr<uint8_t[]>> &outputs) const {
  size_t invalid_index_num = 0UL;
  for (size_t i = 0UL; i < op_desc->GetAllInputsSize(); i++) {
    if (!IsInputDescValid(op_desc->GetInputDesc(static_cast<uint32_t>(i)), invalid_index_num)) {
      GELOGD("input desc is not valid, skip add input[%zu] into context inputs.", i);
      continue;
    }
    gert::StorageShape storage_shape;
    auto input_desc = op_desc->MutableInputDesc(i);
    GE_ASSERT_NOTNULL(input_desc);
    GetStorageShape(*input_desc, storage_shape);
    // init tensor address, if cannot get const tensor input, set it to nullptr
    const size_t instance_index = i - invalid_index_num;
    GE_ASSERT_TRUE((input_data_addrs_.size() > instance_index),
                   "instance_index %zu is invalid, %zu - %zu, total input size %zu",
                   instance_index, i, invalid_index_num, input_data_addrs_.size() );
    gert::TensorAddress address = ValueToPtr(input_data_addrs_[instance_index]);
    std::unique_ptr<uint8_t[]> tensor_holder = ge::ComGraphMakeUnique<uint8_t[]>(sizeof(gert::Tensor));
    GE_ASSERT_NOTNULL(tensor_holder, "Create context holder inputs failed.");
    new (tensor_holder.get())
        gert::Tensor(storage_shape, {input_desc->GetOriginFormat(), input_desc->GetFormat(), {}},
                     gert::kOnDeviceHbm, input_desc->GetDataType(), address);
    (void)inputs.emplace_back(std::move(tensor_holder));
  }
  for (size_t i = 0UL; i < op_desc->GetOutputsSize(); i++) {
    gert::StorageShape storage_shape;
    auto output_desc = op_desc->MutableOutputDesc(i);
    GE_ASSERT_NOTNULL(output_desc);
    GetStorageShape(*output_desc, storage_shape);
    GE_ASSERT_TRUE((output_data_addrs_.size() > i),
                   "output index %zu is invalid, total output size %zu", i, output_data_addrs_.size() );
    gert::TensorAddress address = ValueToPtr(output_data_addrs_[i]);
    std::unique_ptr<uint8_t[]> tensor_holder = ge::ComGraphMakeUnique<uint8_t[]>(sizeof(gert::Tensor));
    GE_ASSERT_NOTNULL(tensor_holder, "Create context holder outputs failed.");
    new (tensor_holder.get())
        gert::Tensor(storage_shape, {output_desc->GetOriginFormat(), output_desc->GetFormat(), {}},
                     gert::kOnDeviceHbm, output_desc->GetDataType(), address);
    (void)outputs.emplace_back(std::move(tensor_holder));
  }
  return SUCCESS;
}

Status CustomTaskInfo::Distribute() {
  GE_ASSERT_NOTNULL(op_desc_);
  GELOGI("CustomTaskInfo Distribute Start, op: %s", op_desc_->GetName().c_str());
  const TaskProfGuarder prof_guarder(this);

  AscendString op_type(op_desc_->GetType().c_str());
  auto custom_op_ptr = CustomOpFactory::CreateOrGetCustomOp(op_type);
  GE_ASSERT_NOTNULL(custom_op_ptr);

  args_update_op_ = dynamic_cast<ArgsUpdater*>(custom_op_ptr);
  if (args_update_op_ != nullptr) {
    GELOGI("ArgsUpdater operator detected: %s", op_desc_->GetName().c_str());
  }

  GE_ASSERT_SUCCESS(ConstructCustomKernelContextInputsOutputs(op_desc_, inputs_holder_, outputs_holder_));

  args_handler_ = ge::ComGraphMakeUnique<SinkOpArgsHandler>(this);
  GE_ASSERT_NOTNULL(args_handler_);
  std::vector<void*> additional_inputs = {sink_only_allocator_.get(), stream_, args_handler_.get()};

  std::vector<void*> additional_outputs;
  additional_outputs.push_back(&ws_vec_);

  eager_context_holder_ = gert::KernelRunContextBuilder()
      .Inputs(GetHoldersRawPtr(inputs_holder_))
      .Inputs(additional_inputs)
      .Outputs(GetHoldersRawPtr(outputs_holder_))
      .Outputs(additional_outputs)
      .Build(op_desc_);
  auto eager_context = reinterpret_cast<gert::EagerOpExecutionContext *>(eager_context_holder_.context_);
  auto *eager_execute_op_ptr = dynamic_cast<ge::EagerExecuteOp*>(custom_op_ptr);
  if (eager_execute_op_ptr == nullptr) {
    GELOGW("%s is custom op but did not implement EagerExecuteOp", eager_context->GetNodeType());
    return ge::GRAPH_FAILED;
  }
  GE_CHK_STATUS_RET(InsertDumpOp(kDumpInput), "Insert custom input dump op failed, node: %s",
                    op_desc_->GetNamePtr());
  GE_ASSERT_SUCCESS(eager_execute_op_ptr->Execute(eager_context));
  GE_CHK_STATUS_RET(InsertDumpOp(kDumpOutput), "Insert custom output dump op failed, node: %s",
                    op_desc_->GetNamePtr());

  GE_ASSERT_SUCCESS(InitArgsIoAddrsUpdater());

  input_count_ = input_data_addrs_.size();
  output_count_ = output_data_addrs_.size();

  GELOGI(
      "CustomTaskInfo Distribute Success, node: %s, stream_id: %u, stream: %p, task_id: %u",
      op_desc_->GetName().c_str(), stream_id_, stream_, task_id_);
  return SUCCESS;
}

Status CustomTaskInfo::Release() {
  aclrtContext ctx = nullptr;
  GE_CHK_RT(aclrtGetCurrentContext(&ctx));
  args_update_op_ = nullptr;
  sink_only_allocator_.reset();
  return SUCCESS;
}

int64_t CustomTaskInfo::ParseOpIndex(const domi::TaskDef &task_def) const {
  const domi::KernelDef &kernel_def = task_def.kernel();
  domi::KernelContext context = kernel_def.context();
  return static_cast<int64_t>(context.op_index());
}

void CustomTaskInfo::PostProcess(const domi::TaskDef &task_def) {
  const domi::KernelDef &kernel_def = task_def.kernel();
  const domi::KernelContext& context = kernel_def.context();
  davinci_model_->SaveDfxInfo(context.op_index(), task_def, *this);
}

const gert::KernelArgs* CustomTaskInfo::MallocReadOnlyDevArgsImpl(void *host_args, size_t args_size) {
  GE_ASSERT_TRUE(host_args != nullptr && args_size != 0U && davinci_model_ != nullptr);

  if (is_args_refreshable_) {
    // 使用预留段分配，支持地址刷新
    ArgsAllocationResult result;
    GE_ASSERT_SUCCESS(davinci_model_->AllocateArgsBuffer(
        args_size, args_placement_, result));

    GE_ASSERT_EOK(memcpy_s(result.host_addr, args_size, host_args, args_size));

    gert::KernelArgs host_args_entry;
    host_args_entry.args_data = result.host_addr;
    host_args_entry.args_size = args_size;
    host_args_entry.placement = gert::Placement::kPlacementHost;
    kernel_args_host_deque_.push_back(host_args_entry);

    gert::KernelArgs device_args;
    device_args.args_data = reinterpret_cast<void*>(result.device_addr);
    device_args.args_size = args_size;
    device_args.placement = gert::Placement::kPlacementDevice;
    kernel_args_device_deque_.push_back(device_args);

    args_allocation_results_.push_back(result);

    GELOGI("MallocReadOnlyDevArgsImpl: reserved path, task_id=%u, args_size=%zu, "
           "host_addr=%p, device_addr=0x%" PRIx64 ", is_from_reserved=%d, pool_index=%u",
           task_id_, args_size, result.host_addr, result.device_addr,
           result.is_from_reserved, result.extra_pool_index);

    return &kernel_args_device_deque_.back();
  }

  // 直接分配动态内存 + H2D 拷贝，当前 args_placement_ 仅支持 HBM
  void *device_ptr = davinci_model_->MallocDynamicMemory(args_size, RT_MEMORY_HBM);
  GE_ASSERT_NOTNULL(device_ptr);

  GE_ASSERT_RT_OK(aclrtMemcpy(device_ptr, args_size, host_args, args_size, ACL_MEMCPY_HOST_TO_DEVICE));

  gert::KernelArgs device_args;
  device_args.args_data = device_ptr;
  device_args.args_size = args_size;
  device_args.placement = gert::Placement::kPlacementDevice;
  kernel_args_device_deque_.push_back(device_args);

  GELOGI("MallocReadOnlyDevArgsImpl: dynamic path, task_id=%u, args_size=%zu, device_addr=%p",
         task_id_, args_size, device_ptr);

  return &kernel_args_device_deque_.back();
}

const std::deque<gert::KernelArgs>& CustomTaskInfo::GetKernelArgsDeque(gert::Placement placement) const {
  if (placement == gert::Placement::kPlacementHost) {
    return kernel_args_host_deque_;
  } else {
    return kernel_args_device_deque_;
  }
}

Status CustomTaskInfo::UpdateHostArgs(void* base_addr, size_t mem_size) {
  if (args_update_op_ == nullptr) {
    GELOGE(FAILED, "UpdateHostArgs called on non-ArgsUpdater operator, task_id=%u", task_id_);
    return FAILED;
  }

  auto* active_mem_base_addr = reinterpret_cast<uint64_t*>(base_addr);
  if (active_mem_base_addr == nullptr || mem_size == 0) {
    GELOGE(FAILED, "active_mem_base_addr is null or mem_size is zero, task_id=%u", task_id_);
    return FAILED;
  }

  std::vector<MemAllocationAndOffset> mem_allocs;
  args_io_addrs_updater_.GetArgsMemAllocationAndOffset(mem_allocs);

  if (mem_allocs.empty()) {
    GELOGE(FAILED, "mem_allocs is empty, no I/O addresses to update, task_id=%u", task_id_);
    return FAILED;
  }

  size_t io_index = 0;
  for (const auto &mem_alloc : mem_allocs) {
    uint64_t allocation_id = mem_alloc.id;
    uint64_t offset = mem_alloc.offset;

    if (allocation_id >= mem_size) {
      GELOGE(FAILED, "allocation_id %" PRIu64 " out-of-bounds (max %" PRIu64 "), io_index=%zu, task_id=%u",
             allocation_id, mem_size - 1, io_index, task_id_);
      return FAILED;
    }

    uint64_t new_addr = active_mem_base_addr[allocation_id] + offset;

    gert::Tensor* tensor = nullptr;
    if (io_index < input_count_) {
      input_data_addrs_[io_index] = new_addr;
      auto* chain = eager_context_holder_.context_->MutableInput(io_index);
      if (chain != nullptr) {
        tensor = chain->GetPointer<gert::Tensor>();
      }
    } else if (io_index < input_count_ + output_count_) {
      size_t output_index = io_index - input_count_;
      output_data_addrs_[output_index] = new_addr;
      auto* chain = eager_context_holder_.context_->GetOutput(output_index);
      if (chain != nullptr) {
        tensor = chain->GetPointer<gert::Tensor>();
      }
    }

    if (tensor != nullptr) {
      tensor->MutableTensorData().SetAddr(reinterpret_cast<void*>(new_addr), nullptr);
      GELOGI("UpdateHostArgs: io_index=%zu, allocation_id=%" PRIu64 ", offset=%" PRIu64 ", new_addr=0x%" PRIx64,
             io_index, allocation_id, offset, new_addr);
    }

    io_index++;
  }

  auto* ctx = reinterpret_cast<gert::UpdateArgsContext*>(eager_context_holder_.context_);
  graphStatus ret = args_update_op_->UpdateHostArgs(ctx);
  if (ret != GRAPH_SUCCESS) {
    GELOGE(FAILED, "Operator UpdateHostArgs failed, task_id=%u", task_id_);
    return FAILED;
  }

  GE_ASSERT_SUCCESS(UpdateCustomDumpAddrs(input_data_addrs_, output_data_addrs_));

  GELOGD("UpdateHostArgs succeeded, task_id=%u, updated %zu I/O addresses", task_id_, io_index);
  return SUCCESS;
}

Status CustomTaskInfo::InitArgsIoAddrsUpdater() {
  ArgsIoAddrsUpdater::OpInfo op_info{op_desc_->GetName(), op_desc_->GetType()};

  if (args_update_op_ != nullptr) {
    std::vector<uint64_t> logical_addrs;
    logical_addrs.insert(logical_addrs.end(), input_data_addrs_.begin(), input_data_addrs_.end());
    logical_addrs.insert(logical_addrs.end(), output_data_addrs_.begin(), output_data_addrs_.end());

    std::vector<uint64_t> mem_types;
    mem_types.insert(mem_types.end(), input_mem_types_.begin(), input_mem_types_.end());
    mem_types.insert(mem_types.end(), output_mem_types_.begin(), output_mem_types_.end());

    GE_ASSERT_SUCCESS(args_io_addrs_updater_.Init(davinci_model_->GetLogicalMemAllocation(),
                                                   logical_addrs, mem_types, op_info));
    GELOGI("ArgsUpdater operator stored: %s", op_desc_->GetName().c_str());
  }

  return SUCCESS;
}

REGISTER_TASK_INFO(MODEL_TASK_CUSTOM_KERNEL, CustomTaskInfo);
}  // namespace ge
