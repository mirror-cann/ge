/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "kernel_ex_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"
#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "aicpu_engine_struct.h"
#include "common/ge_common/debug/ge_log.h"
#include "common/om2/codegen/om2_aicpu_ext_info_handler.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/math_util.h"

namespace ge {
namespace {
const std::string kAttrAicpuAllshape = "_AllShape";
const std::string kTfSessionTask = "TfSessionTask";
constexpr int64_t kOffset = 8;
}  // namespace

std::string KernelExTaskCodeBuilder::SerializeBytesToOctalString(const std::vector<uint8_t> &buffer) {
  std::ostringstream code_stream;
  for (size_t i = 0; i < buffer.size(); ++i) {
    code_stream << "\\";
    code_stream << std::oct << std::setw(kWidthPerChar) << std::setfill('0') << static_cast<int32_t>(buffer[i]);
  }
  return code_stream.str();
}

Expr *KernelExTaskCodeBuilder::BuildLaunchConfigExpr(const LaunchConfigSemantic &launch_config,
                                                     Arg is_data_dump) const {
  return ast_.InitList(
      {ast_.UInt(static_cast<uint64_t>(launch_config.schedule_mode)), launch_config.engine_type,
       ast_.UInt(static_cast<uint64_t>(launch_config.block_dim_offset)), launch_config.is_block_task_prefetch,
       is_data_dump.Empty()
           ? Arg(ast_.Call("GetIsDataDump", {Arg::StringLiteral(header_.op_name), model_id_, instance_handle_}))
           : is_data_dump,
       ast_.UInt(static_cast<uint64_t>(launch_config.time_out))});
}

VarRef KernelExTaskCodeBuilder::AppendLaunchConfigSetup(size_t op_index, std::vector<BodyItem> &items,
                                                        Arg is_data_dump) const {
  const std::string cfg_holder_var_name = "op" + std::to_string(op_index) + "_cfg_holder";
  auto cfg_holder = ast_.Var("LaunchKernelCfgHolder", cfg_holder_var_name);
  (void)items.emplace_back(ast_.VarDecl(cfg_holder));
  (void)items.emplace_back(
      ast_.Call("AssembleLaunchConfig", {cfg_holder, BuildLaunchConfigExpr(semantic_.launch.config, is_data_dump)}));
  return cfg_holder;
}

Status KernelExTaskCodeBuilder::InitLaunchInfo(const TaskSemanticContributeContext &context) {
  GE_ASSERT_NOTNULL(context.func_handle_indices);
  semantic_.launch.stream_id = context.task_def.stream_id();
  semantic_.launch.block_dim = 1U;
  const auto func_handle_it = context.func_handle_indices->find(context.op_desc->GetType());
  GE_ASSERT_TRUE(func_handle_it != context.func_handle_indices->end(), "[OM2] func handle not found, key: %s",
                 context.op_desc->GetType().c_str());
  semantic_.launch.func_handle_index = func_handle_it->second;
  const auto session_func_handle_it = context.func_handle_indices->find(kTfSessionTask);
  GE_ASSERT_TRUE(session_func_handle_it != context.func_handle_indices->end(),
                 "[OM2] tf session func handle not found");
  semantic_.launch.tf_session_func_handle_index = session_func_handle_it->second;
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::InitTaskExInfo(const TaskSemanticContributeContext &context) {
  task_def_kernel_ex_args_ = context.task_def.kernel_ex().args();
  task_def_kernel_ex_args_size_ = context.task_def.kernel_ex().args().size();
  task_def_kernel_ex_task_info_ = context.task_def.kernel_ex().task_info();
  task_def_kernel_ex_task_info_size_ = context.task_def.kernel_ex().task_info().size();
  task_def_kernel_ex_ext_info_ = context.task_def.kernel_ex().kernel_ext_info();
  task_def_kernel_ex_ext_info_size_ = context.task_def.kernel_ex().kernel_ext_info().size();
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::InitIowAddrRefreshInfo(uint64_t current_offset) {
  for (const auto &input_addr : semantic_.input_addrs) {
    if (input_addr.memory_app == MemoryAppType::kModelIo) {
      io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(input_addr.compile_state_io_addr_offset), current_offset});
      current_offset += static_cast<uint32_t>(sizeof(uint64_t));
    }
    semantic_.ordered_arg_values.push_back(input_addr);
  }
  for (const auto &out_addr : semantic_.output_addrs) {
    if (out_addr.memory_app == MemoryAppType::kModelIo) {
      io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(out_addr.compile_state_io_addr_offset), current_offset});
      current_offset += static_cast<uint32_t>(sizeof(uint64_t));
    }
    semantic_.ordered_arg_values.push_back(out_addr);
  }
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::InitArgsTableInfo(const TaskSemanticContributeContext &context) {
  const size_t inputs_size = context.op_desc->GetInputsSize();
  const size_t outputs_size = context.op_desc->GetOutputsSize();
  REQUIRE_COMPAT_UINT32(sizeof(uint64_t) * (inputs_size + outputs_size));
  uint32_t mem_size = static_cast<uint32_t>(sizeof(uint64_t) * (inputs_size + outputs_size));
  const uint32_t mem_size_t =
      static_cast<uint32_t>(sizeof(uint64_t) * (semantic_.input_addrs.size() + semantic_.output_addrs.size()));
  GELOGD("[OM2] mem_size %u, inputs_size %zu, outputs_size %zu, input_data_addrs size %zu, output_data_addrs size %zu.",
         mem_size, inputs_size, outputs_size, semantic_.input_addrs.size(), semantic_.output_addrs.size());
  mem_size = (mem_size > mem_size_t) ? mem_size : mem_size_t;
  int32_t deploy_type_flag = static_cast<int32_t>(RT_KERNEL_DEVICE_FIRST);
  const auto &ext_info = context.task_def.kernel_ex().kernel_ext_info();
  if (!ext_info.empty()) {
    int32_t unknown_shape_type_val = 0;
    (void)AttrUtils::GetInt(context.op_desc, ATTR_NAME_UNKNOWN_SHAPE_TYPE, unknown_shape_type_val);
    const auto unknown_type = static_cast<UnknowShapeOpType>(unknown_shape_type_val);
    const uint32_t num_inputs = static_cast<uint32_t>(context.op_desc->GetInputsSize());
    const uint32_t num_outputs = static_cast<uint32_t>(context.op_desc->GetOutputsSize());
    om2::Om2AicpuExtInfoHandler ext_handle(context.op_desc->GetName(), num_inputs, num_outputs, unknown_type);
    GE_CHK_STATUS_RET(ext_handle.Parse(ext_info), "[OM2]Parse kernel ext info failed, ext_info_size: %zu",
                      ext_info.size());
    deploy_type_flag = ext_handle.GetDeployTypeFlag();
  }
  GELOGI("kernel task name %s, args_size %u, args_size_t %u, deploy_type_flag %d", context.op_desc->GetName().c_str(),
         mem_size, mem_size_t, deploy_type_flag);
  (void)semantic_.args_table_entry.emplace();
  semantic_.args_table_entry->table_index = *context.next_args_table_index;
  semantic_.args_table_entry->host_offset = *context.next_host_args_offset;
  semantic_.args_table_entry->args_size = static_cast<int64_t>(mem_size) + kOffset;
  args_table_entry_ = &(*semantic_.args_table_entry);
  if (semantic_.args_table_entry.has_value()) {
    ++(*context.next_args_table_index);
    *context.next_host_args_offset +=
        Om2ModelUtils::ArgsSizeAlign8(static_cast<uint32_t>(semantic_.args_table_entry->args_size));
  }
  const uint64_t current_offset = args_table_entry_->host_offset;
  GE_ASSERT_SUCCESS(InitIowAddrRefreshInfo(current_offset));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::InitTaskExtInfo(const TaskSemanticContributeContext &context) {
  const auto &ext_info = context.task_def.kernel_ex().kernel_ext_info();
  if (ext_info.empty()) {
    return SUCCESS;
  }
  int32_t unknown_shape_type_val = 0;
  (void)AttrUtils::GetInt(context.op_desc, ATTR_NAME_UNKNOWN_SHAPE_TYPE, unknown_shape_type_val);
  const auto unknown_type = static_cast<UnknowShapeOpType>(unknown_shape_type_val);
  const uint32_t num_inputs = static_cast<uint32_t>(context.op_desc->GetInputsSize());
  const uint32_t num_outputs = static_cast<uint32_t>(context.op_desc->GetOutputsSize());
  om2::Om2AicpuExtInfoHandler ext_handle(context.op_desc->GetName(), num_inputs, num_outputs, unknown_type);
  GE_CHK_STATUS_RET(ext_handle.Parse(ext_info), "[OM2][Parse][KernelExtInfo] failed, ext_info_size: %zu",
                    ext_info.size());
  GE_CHK_STATUS_RET(ext_handle.UpdateExecuteMode(false), "[OM2][Update][ExecuteMode] failed.");
  deploy_type_flag_ = ext_handle.GetDeployTypeFlag();
  mem_type_ = ext_handle.GetMemType();
  memcpy_kind_ = ext_handle.GetMemcpyKind();
  bool all_shape = false;
  (void)AttrUtils::GetBool(context.op_desc, kAttrAicpuAllshape, all_shape);
  if (all_shape) {
    GELOGD("[OM2]Aicpu all_shape kernel need to update io shape.");
    for (uint32_t i = 0U; i < num_inputs; i++) {
      const auto input_desc = context.op_desc->MutableInputDesc(i);
      GE_CHECK_NOTNULL(input_desc);
      GE_CHK_STATUS_RET(ext_handle.UpdateInputShapeAndType(i, *input_desc),
                        "[OM2][Call][UpdateInputShapeAndType] Input[%u] update input shape failed, op:%s.", i,
                        context.op_desc->GetName().c_str());
    }
    if (unknown_type != DEPEND_COMPUTE) {
      for (uint32_t i = 0U; i < num_outputs; ++i) {
        const auto output_desc = context.op_desc->MutableOutputDesc(i);
        GE_CHECK_NOTNULL(output_desc);
        GE_CHK_STATUS_RET(ext_handle.UpdateOutputShapeAndType(i, *output_desc),
                          "[OM2][Call][UpdateOutputShapeAndType] Output[%u] update output shape failed, op:%s.", i,
                          context.op_desc->GetName().c_str());
      }
    }
  }
  ext_info_len_ = ext_handle.GetExtInfoLen();
  ext_info_ = ext_handle.GetExtInfo();
  ext_info_buffer_.resize(ext_info_len_);
  if (memcpy_s(ext_info_buffer_.data(), ext_info_buffer_.size(), ext_info_, ext_info_len_) != EOK) {
    GELOGE(FAILED, "[OM2]Failed to copy ext info", context.op_desc->GetName().c_str());
    return FAILED;
  }
  return SUCCESS;
}

FunctionDef *KernelExTaskCodeBuilder::BuildTfAicpuKernelTaskDistribute() const {
  auto io_addrs = ast_.Var("const std::vector<uint64_t> &", "io_addrs");
  auto args_info = ast_.Var("ArgsInfo *", "args_info");
  auto kernel_buf = ast_.Var("void *", "kernel_buf");
  auto kernel_buf_size = ast_.Var("uint32_t", "kernel_buf_size");
  auto func_handle = ast_.Var("aclrtFuncHandle", "func_handle");
  auto block_dim = ast_.Var("uint32_t", "block_dim");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto config = ast_.Var("aclrtLaunchKernelCfg *", "config");
  return ast_.DefineFunction(
      "TfAicpuKernelTaskDistribute",
      {io_addrs, args_info, kernel_buf, kernel_buf_size, func_handle, block_dim, stream, config}, "aclError",
      {
          ast_.If(args_info != nullptr,
                  {
                      ChkStatus(MemcpyS(args_info.Arrow("host_addr"), args_info.Arrow("size"), io_addrs.Data(),
                                        io_addrs.Size() * ast_.Sizeof("uint64_t"))),
                  }),
          ChkStatus(AclrtLaunchKernelV2(func_handle, block_dim, kernel_buf, kernel_buf_size, config, stream)),
          ast_.Return("ACL_SUCCESS"),
      });
}

FunctionDef *KernelExTaskCodeBuilder::BuildAssembleTfAicpuArgs() const {
  auto kernel_ex_args = ast_.Var("uint8_t *", "kernel_ex_args");
  auto kernel_ex_args_len = ast_.Var("size_t", "kernel_ex_args_len");
  auto target_args_ptr = ast_.Var("void *", "target_args_ptr");
  auto target_args_len = ast_.Var("size_t", "target_args_len");
  auto tf_ai_cpu_ex_info = ast_.Var("TfAiCpuExInfo *", "tf_ai_cpu_ex_info");
  auto tmp_args = ast_.Var("std::vector<uint8_t>", "tmp_args");
  // 更新STR_FWK_OP_KERNEL结构体相关字段, 当前采用计算偏移的方式更新
  return ast_.DefineFunction(
      "AssembleTfAicpuArgs", {kernel_ex_args, kernel_ex_args_len, target_args_ptr, target_args_len, tf_ai_cpu_ex_info},
      "aclError",
      {ast_.VarDecl(tmp_args), tmp_args.Resize(target_args_len),
       ChkStatus(MemcpyS(tmp_args.Data(), target_args_len, kernel_ex_args, kernel_ex_args_len)),
       ChkStatus(MemcpyS(tmp_args.Data() + ast_.Sizeof("uint32_t") * 2, ast_.Sizeof("uint64_t"),
                         tf_ai_cpu_ex_info.Arrow("sessionID").Addr(), ast_.Sizeof("uint64_t"))),
       ChkStatus(MemcpyS(tmp_args.Data() + ast_.Sizeof("uint32_t") * 2 + ast_.Sizeof("uint64_t") * 2,
                         ast_.Sizeof("uint64_t"), tf_ai_cpu_ex_info.Arrow("kernelID").Addr(), ast_.Sizeof("uint64_t"))),
       ChkStatus(MemcpyS(tmp_args.Data() + ast_.Sizeof("uint32_t") * 2 + ast_.Sizeof("uint64_t") * 9,
                         ast_.Sizeof("uint64_t"), tf_ai_cpu_ex_info.Arrow("workspaceBaseAddr").Addr(),
                         ast_.Sizeof("uint64_t"))),
       ChkStatus(MemcpyS(tmp_args.Data() + ast_.Sizeof("uint32_t") * 2 + ast_.Sizeof("uint64_t") * 10,
                         ast_.Sizeof("uint64_t"), tf_ai_cpu_ex_info.Arrow("inputOutputAddr").Addr(),
                         ast_.Sizeof("uint64_t"))),
       ChkStatus(MemcpyS(tmp_args.Data() + ast_.Sizeof("uint32_t") * 2 + ast_.Sizeof("uint64_t") * 11,
                         ast_.Sizeof("uint64_t"), tf_ai_cpu_ex_info.Arrow("extInfoLen").Addr(),
                         ast_.Sizeof("uint64_t"))),
       ChkStatus(MemcpyS(tmp_args.Data() + ast_.Sizeof("uint32_t") * 2 + ast_.Sizeof("uint64_t") * 12,
                         ast_.Sizeof("uint64_t"), tf_ai_cpu_ex_info.Arrow("extInfoAddr").Addr(),
                         ast_.Sizeof("uint64_t"))),
       ChkStatus(MemcpyS(target_args_ptr, target_args_len, tmp_args.Data(), target_args_len)),
       ast_.Return("ACL_SUCCESS")});
}

FunctionDef *KernelExTaskCodeBuilder::BuildAssembleTfAicpuExSessionIdInfo() const {
  auto session_id = ast_.Var("uint64_t *", "session_id");
  auto op_kernel_size = ast_.Var("size_t", "op_kernel_size");
  auto func_handle = ast_.Var("aclrtFuncHandle", "func_handle");
  auto block_dim = ast_.Var("uint32_t", "block_dim");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto config = ast_.Var("aclrtLaunchKernelCfg *", "config");
  auto tf_ai_cpu_ex_info = ast_.Var("TfAiCpuExInfo *", "tf_ai_cpu_ex_info");
  auto mem_ptrs = ast_.Var("std::vector<void *> &", "mem_ptrs");

  auto device_id = ast_.Var("int32_t", "device_id");
  auto device_base = ast_.Var("void *", "device_base");
  auto tmp_args = ast_.Var("std::vector<int8_t>", "tmp_args");
  auto kKernelType = ast_.Var("uint32_t *", "kKernelType");
  auto fwkOperateType = ast_.Var("uint32_t *", "fwkOperateType");
  auto iow_addrs = ast_.Var("std::vector<uint64_t>", "iow_addrs");
  return ast_.DefineFunction(
      "AssembleTfAicpuExSessionIdInfo",
      {session_id, op_kernel_size, func_handle, block_dim, stream, config, tf_ai_cpu_ex_info, mem_ptrs}, "aclError",
      {ast_.Assign(tf_ai_cpu_ex_info.Arrow("sessionID"), ast_.Deref(session_id)), ast_.VarDecl(tmp_args),
       tmp_args.Resize(op_kernel_size), ast_.VarDecl(iow_addrs),
       ChkStatus(MemcpyS(tmp_args.Data() + ast_.Sizeof("uint32_t") * 1, ast_.Sizeof("uint64_t"), session_id,
                         ast_.Sizeof("uint64_t"))),
       ast_.VarDecl(device_id, 0), ChkStatus(ast_.Call("aclrtGetDevice", {device_id.Addr()})),
       ast_.VarDecl(device_base, nullptr),
       ChkStatus(AclrtMallocAlign32(device_base.Addr(), op_kernel_size, "ACL_MEM_TYPE_HIGH_BAND_WIDTH")),
       mem_ptrs.PushBack(device_base),
       ChkStatus(
           AclrtMemcpy(device_base, op_kernel_size, tmp_args.Data(), op_kernel_size, "ACL_MEMCPY_HOST_TO_DEVICE")),
       ChkStatus(ast_.Call("TfAicpuKernelTaskDistribute",
                           {iow_addrs, nullptr, device_base, op_kernel_size, func_handle, block_dim, stream, config})),
       ChkStatus(ast_.Call("aclrtSynchronizeStream", {stream})), ast_.Return("ACL_SUCCESS")});
}

FunctionDef *KernelExTaskCodeBuilder::BuildAssembleTfAicpuExKernelIdInfo() const {
  auto kernel_id = ast_.Var("uint64_t *", "kernel_id");
  auto tf_ai_cpu_ex_info = ast_.Var("TfAiCpuExInfo *", "tf_ai_cpu_ex_info");
  return ast_.DefineFunction("AssembleTfAicpuExKernelIdInfo", {kernel_id, tf_ai_cpu_ex_info}, "aclError",
                             {ast_.Assign(tf_ai_cpu_ex_info.Arrow("kernelID"), ast_.Deref(kernel_id)),
                              ast_.PostInc(ast_.Deref(kernel_id)), ast_.Return("ACL_SUCCESS")});
}

FunctionDef *KernelExTaskCodeBuilder::BuildAssembleTfAicpuExWorkSpaceAddrInfo() const {
  auto kernel_ex_task_info = ast_.Var("uint8_t *", "kernel_ex_task_info");
  auto kernel_ex_task_info_len = ast_.Var("size_t", "kernel_ex_task_info_len");
  auto tf_ai_cpu_ex_info = ast_.Var("TfAiCpuExInfo *", "tf_ai_cpu_ex_info");
  auto workspace_dev_ptr = ast_.Var("void *", "workspace_dev_ptr");
  auto mem_ptrs = ast_.Var("std::vector<void *> &", "mem_ptrs");
  return ast_.DefineFunction(
      "AssembleTfAicpuExWorkSpaceAddrInfo", {kernel_ex_task_info, kernel_ex_task_info_len, tf_ai_cpu_ex_info, mem_ptrs},
      "aclError",
      {ast_.VarDecl(workspace_dev_ptr, nullptr),
       ChkStatus(AclrtMallocAlign32(workspace_dev_ptr.Addr(), kernel_ex_task_info_len, "ACL_MEM_MALLOC_HUGE_FIRST")),
       mem_ptrs.PushBack(workspace_dev_ptr),
       ChkStatus(AclrtMemcpy(workspace_dev_ptr, kernel_ex_task_info_len, kernel_ex_task_info, kernel_ex_task_info_len,
                             "ACL_MEMCPY_HOST_TO_DEVICE")),
       ast_.Assign(tf_ai_cpu_ex_info.Arrow("workspaceBaseAddr"),
                   ast_.StaticCast("uint64_t", ast_.ReinterpretCast("uintptr_t", workspace_dev_ptr))),
       ast_.Return("ACL_SUCCESS")});
}

FunctionDef *KernelExTaskCodeBuilder::BuildAssembleTfAicpuExInputOutputAddrInfo() const {
  auto args_info = ast_.Var("ArgsInfo *", "args_info");
  auto tf_ai_cpu_ex_info = ast_.Var("TfAiCpuExInfo *", "tf_ai_cpu_ex_info");
  return ast_.DefineFunction(
      "AssembleTfAicpuExInputOutputAddrInfo", {args_info, tf_ai_cpu_ex_info}, "aclError",
      {ChkNotNull(args_info),
       ast_.Assign(tf_ai_cpu_ex_info.Arrow("inputOutputAddr"),
                   ast_.StaticCast("uint64_t", ast_.ReinterpretCast("uintptr_t", args_info.Arrow("dev_addr")))),
       ast_.Return("ACL_SUCCESS")});
}

FunctionDef *KernelExTaskCodeBuilder::BuildAssembleTfAicpuExExtInfo() const {
  auto kernel_ex_ext_info = ast_.Var("uint8_t *", "kernel_ex_ext_info");
  auto kernel_ex_ext_info_len = ast_.Var("size_t", "kernel_ex_ext_info_len");
  auto tf_ai_cpu_ex_info = ast_.Var("TfAiCpuExInfo *", "tf_ai_cpu_ex_info");
  auto ext_info_dev_ptr = ast_.Var("void *", "ext_info_dev_ptr");
  auto mem_ptrs = ast_.Var("std::vector<void *> &", "mem_ptrs");
  return ast_.DefineFunction(
      "AssembleTfAicpuExExtInfo", {kernel_ex_ext_info, kernel_ex_ext_info_len, tf_ai_cpu_ex_info, mem_ptrs}, "aclError",
      {ast_.VarDecl(ext_info_dev_ptr, nullptr),
       ChkStatus(AclrtMallocAlign32(ext_info_dev_ptr.Addr(), kernel_ex_ext_info_len, "ACL_MEM_MALLOC_HUGE_FIRST")),
       mem_ptrs.PushBack(ext_info_dev_ptr),
       ChkStatus(AclrtMemcpy(ext_info_dev_ptr, kernel_ex_ext_info_len, kernel_ex_ext_info, kernel_ex_ext_info_len,
                             "ACL_MEMCPY_HOST_TO_DEVICE")),
       ast_.Assign(tf_ai_cpu_ex_info.Arrow("extInfoAddr"),
                   ast_.StaticCast("uint64_t", ast_.ReinterpretCast("uintptr_t", ext_info_dev_ptr))),
       ast_.Assign(tf_ai_cpu_ex_info.Arrow("extInfoLen"), ast_.StaticCast("uint64_t", kernel_ex_ext_info_len)),
       ast_.Return("ACL_SUCCESS")});
}

Status KernelExTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  GE_ASSERT_SUCCESS(TaskCodeBuilder::Contribute(context));
  GE_ASSERT_NOTNULL(context.next_args_table_index);
  GE_ASSERT_NOTNULL(context.next_host_args_offset);
  GE_ASSERT_NOTNULL(context.op_desc);
  semantic_.task_type = context.task_type;
  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveInputAddrs(context, semantic_.input_addrs));
  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveOutputAddrs(context, false, semantic_.output_addrs));
  GE_ASSERT_SUCCESS(InitArgsTableInfo(context));
  GE_ASSERT_SUCCESS(InitTaskExInfo(context));
  GE_ASSERT_SUCCESS(InitTaskExtInfo(context));
  GE_ASSERT_SUCCESS(InitLaunchInfo(context));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderGetAddrInfo(std::vector<BodyItem> &items,
                                                  std::vector<Arg> &flatten_args_vars) const {
  for (const auto &input_addr : semantic_.input_addrs) {
    GE_ASSERT_SUCCESS(AppendOm2TensorAddrInfo(input_addr, "input", items, flatten_args_vars));
  }
  for (const auto &output_addr : semantic_.output_addrs) {
    GE_ASSERT_SUCCESS(AppendOm2TensorAddrInfo(output_addr, "output", items, flatten_args_vars));
  }
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::AppendOm2TensorAddrInfo(const AddrSemantic &addr, const char *addr_type,
                                                        std::vector<BodyItem> &items,
                                                        std::vector<Arg> &flatten_args_vars) const {
  GE_ASSERT_TRUE(!addr.symbol_hint.empty(), "[OM2] KernelEx %s addr symbol hint is empty.", addr_type);
  GE_ASSERT_TRUE(addr.tensor_info.has_value(), "[OM2] KernelEx %s tensor info is required for %s.", addr_type,
                 addr.symbol_hint.c_str());
  const auto &tensor_info = *addr.tensor_info;
  const std::string shape_var_name = addr.symbol_hint + "_shape";
  items.push_back(
      ast_.VarDecl("std::vector<int64_t>", shape_var_name, ast_.InitList(ConvertToArgs(tensor_info.shape_dims))));
  const auto device_addr = (addr.kind == AddrValueKind::kConstTensor && addr.const_index.has_value())
                               ? Arg(constants_[static_cast<int64_t>(*addr.const_index)])
                               : Arg(GetAddr(total_dev_mem_ptr_, addr.mem_offset));
  items.push_back(ast_.VarDecl(
      "Om2Tensor", addr.symbol_hint,
      ast_.Call("BuildOm2Tensor", {device_addr, ast_.ULong(tensor_info.size), tensor_info.data_type, tensor_info.format,
                                   ast_.Var("std::vector<int64_t>", shape_var_name).Data(),
                                   ast_.Var("std::vector<int64_t>", shape_var_name).Size()})));
  (void)flatten_args_vars.emplace_back(ast_.Var("auto", addr.symbol_hint));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  GELOGD("[OM2] start to generate kernel ex task distribute code.");
  const int64_t op_index = header_.op_index;
  (void)items.emplace_back(
      ast_.Comment("============================= " + header_.op_name + " ==============================="));
  std::vector<Arg> flatten_args_vars;
  GE_ASSERT_SUCCESS(RenderGetAddrInfo(items, flatten_args_vars));

  auto args_var = ast_.Var("std::vector<uint8_t>", "op" + std::to_string(op_index) + "_args");
  (void)items.emplace_back(ast_.VarDecl(args_var));
  (void)items.emplace_back(args_var.Resize(sizeof(STR_FWK_OP_KERNEL)));
  auto kernel_buf_var = ast_.Var("void *", "op" + std::to_string(op_index) + "_kernel_buf");
  (void)items.emplace_back(ast_.VarDecl(kernel_buf_var, nullptr));
  auto tf_ai_cpu_ex_info_var = ast_.Var("TfAiCpuExInfo", "op" + std::to_string(op_index) + "_tf_ai_cpu_ex_info");
  (void)items.emplace_back(ast_.VarDecl(tf_ai_cpu_ex_info_var));

  const std::string kernel_ex_args_str_var_name = "op" + std::to_string(op_index) + "_kernel_ex_args_str";
  auto kernel_ex_args_str_var = ast_.Var("const char *", kernel_ex_args_str_var_name);
  std::vector<uint8_t> kernel_ex_args_buffer(task_def_kernel_ex_args_.begin(), task_def_kernel_ex_args_.end());
  (void)items.emplace_back(
      ast_.VarDecl(kernel_ex_args_str_var, Arg::StringLiteral(SerializeBytesToOctalString(kernel_ex_args_buffer))));

  const std::string kernel_ex_task_info_str_var_name = "op" + std::to_string(op_index) + "_kernel_ex_task_info_str";
  auto kernel_ex_task_info_str_var = ast_.Var("const char *", kernel_ex_task_info_str_var_name);
  std::vector<uint8_t> kernel_ex_task_info_buffer(task_def_kernel_ex_task_info_.begin(),
                                                  task_def_kernel_ex_task_info_.end());
  (void)items.emplace_back(ast_.VarDecl(kernel_ex_task_info_str_var,
                                        Arg::StringLiteral(SerializeBytesToOctalString(kernel_ex_task_info_buffer))));

  const std::string kernel_ex_ext_info_str_var_name = "op" + std::to_string(op_index) + "_kernel_ex_ext_info_str";
  auto kernel_ex_ext_info_str_var = ast_.Var("const char *", kernel_ex_ext_info_str_var_name);
  (void)items.emplace_back(
      ast_.VarDecl(kernel_ex_ext_info_str_var, Arg::StringLiteral(SerializeBytesToOctalString(ext_info_buffer_))));

  auto cfg_holder = AppendLaunchConfigSetup(op_index, items);
  auto mem_ptrs = ast_.Var("std::vector<void *>", "dev_dynamic_mem_ptrs_");
  (void)items.emplace_back(ChkStatus(ast_.Call(
      "AssembleTfAicpuExSessionIdInfo",
      {session_id_, static_cast<uint32_t>(sizeof(STR_FWK_OP_KERNEL)),
       func_handles_[static_cast<int64_t>(semantic_.launch.tf_session_func_handle_index)],
       static_cast<int64_t>(semantic_.launch.block_dim), stream_list_[static_cast<int64_t>(semantic_.launch.stream_id)],
       cfg_holder.Attr("cfg").Addr(), tf_ai_cpu_ex_info_var.Addr(), mem_ptrs})));

  (void)items.emplace_back(
      ChkStatus(ast_.Call("AssembleTfAicpuExKernelIdInfo", {kernel_id_.Addr(), tf_ai_cpu_ex_info_var.Addr()})));

  (void)items.emplace_back(ChkStatus(
      ast_.Call("AssembleTfAicpuExWorkSpaceAddrInfo",
                {ast_.ReinterpretCast("uint8_t *", ast_.ConstCast("char *", kernel_ex_task_info_str_var)),
                 static_cast<int64_t>(task_def_kernel_ex_task_info_size_), tf_ai_cpu_ex_info_var.Addr(), mem_ptrs})));

  (void)items.emplace_back(ChkStatus(
      ast_.Call("AssembleTfAicpuExInputOutputAddrInfo",
                {args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(semantic_.args_table_entry->table_index)),
                 tf_ai_cpu_ex_info_var.Addr()})));

  (void)items.emplace_back(
      ChkStatus(ast_.Call("AssembleTfAicpuExExtInfo",
                          {ast_.ReinterpretCast("uint8_t *", ast_.ConstCast("char *", kernel_ex_ext_info_str_var)),
                           static_cast<int64_t>(ext_info_len_), tf_ai_cpu_ex_info_var.Addr(), mem_ptrs})));

  (void)items.emplace_back(ChkStatus(ast_.Call(
      "AssembleTfAicpuArgs", {ast_.ReinterpretCast("uint8_t *", ast_.ConstCast("char *", kernel_ex_args_str_var)),
                              static_cast<int64_t>(task_def_kernel_ex_args_size_), args_var.Data(),
                              sizeof(STR_FWK_OP_KERNEL), tf_ai_cpu_ex_info_var.Addr()})));

  (void)items.emplace_back(
      ChkStatus(AclrtMallocAlign32(kernel_buf_var.Addr(), sizeof(STR_FWK_OP_KERNEL), "ACL_MEM_MALLOC_HUGE_FIRST")));
  (void)items.emplace_back(mem_ptrs.PushBack(kernel_buf_var));
  (void)items.emplace_back(ChkStatus(AclrtMemcpy(kernel_buf_var, sizeof(STR_FWK_OP_KERNEL), args_var.Data(),
                                                 sizeof(STR_FWK_OP_KERNEL), "ACL_MEMCPY_HOST_TO_DEVICE")));
  (void)items.emplace_back(ChkStatus(
      ast_.Call("TfAicpuKernelTaskDistribute",
                {FlattenHostArgs(flatten_args_vars),
                 args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(semantic_.args_table_entry->table_index)),
                 kernel_buf_var, static_cast<uint32_t>(sizeof(STR_FWK_OP_KERNEL)),
                 func_handles_[static_cast<int64_t>(semantic_.launch.func_handle_index)],
                 static_cast<int64_t>(semantic_.launch.block_dim),
                 stream_list_[static_cast<int64_t>(semantic_.launch.stream_id)], cfg_holder.Attr("cfg").Addr()})));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::AppendReportLaunchedTaskCall(
      ast_, items, "op" + std::to_string(header_.op_index) + "_kernel_ex", header_,
      semantic_.args_table_entry.has_value() ? &(*semantic_.args_table_entry) : nullptr, semantic_.input_addrs,
      semantic_.output_addrs, semantic_.workspace_addrs, semantic_.task_type, semantic_.launch.block_dim,
      stream_list_[static_cast<int64_t>(semantic_.launch.stream_id)], model_id_, instance_handle_, args_table_, true));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  items.push_back(BuildTfAicpuKernelTaskDistribute());
  items.push_back(BuildAssembleTfAicpuExSessionIdInfo());
  items.push_back(BuildAssembleTfAicpuExKernelIdInfo());
  items.push_back(BuildAssembleTfAicpuExWorkSpaceAddrInfo());
  items.push_back(BuildAssembleTfAicpuExInputOutputAddrInfo());
  items.push_back(BuildAssembleTfAicpuExExtInfo());
  items.push_back(BuildAssembleTfAicpuArgs());
  return SUCCESS;
}

int64_t KernelExTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const auto &kernel_ex_def = task_def.kernel_ex();
  return static_cast<int64_t>(kernel_ex_def.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_KERNEL_EX, KernelExTaskCodeBuilder);
}  // namespace ge
