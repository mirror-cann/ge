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

Status KernelExTaskCodeBuilder::InitLaunchInfo(const TaskSemanticContributeContext &context) {
  GE_ASSERT_NOTNULL(context.func_handle_indices);
  build_data_.semantic.launch.stream_id = context.task_def.stream_id();
  build_data_.semantic.launch.block_dim = 1U;
  const auto func_handle_it = context.func_handle_indices->find(context.op_desc->GetType());
  GE_ASSERT_TRUE(func_handle_it != context.func_handle_indices->end(), "[OM2] func handle not found, key: %s",
                 context.op_desc->GetType().c_str());
  build_data_.semantic.launch.func_handle_index = func_handle_it->second;
  const auto session_func_handle_it = context.func_handle_indices->find(kTfSessionTask);
  GE_ASSERT_TRUE(session_func_handle_it != context.func_handle_indices->end(),
                 "[OM2] tf session func handle not found");
  build_data_.semantic.launch.tf_session_func_handle_index = session_func_handle_it->second;
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::InitTaskExInfo(const TaskSemanticContributeContext &context) {
  const auto &args = context.task_def.kernel_ex().args();
  const auto &task_info = context.task_def.kernel_ex().task_info();
  build_data_.args_blob.assign(args.begin(), args.end());
  build_data_.args_blob_len = static_cast<uint32_t>(args.size());
  build_data_.task_info_blob.assign(task_info.begin(), task_info.end());
  build_data_.task_info_blob_len = static_cast<uint32_t>(task_info.size());
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::InitIowAddrRefreshInfo(uint64_t current_offset) {
  for (const auto &input_addr : build_data_.semantic.input_addrs) {
    if (input_addr.memory_app == MemoryAppType::kModelIo) {
      io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(input_addr.compile_state_io_addr_offset), current_offset});
      current_offset += static_cast<uint32_t>(sizeof(uint64_t));
    }
    build_data_.semantic.ordered_arg_values.push_back(input_addr);
  }
  for (const auto &out_addr : build_data_.semantic.output_addrs) {
    if (out_addr.memory_app == MemoryAppType::kModelIo) {
      io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(out_addr.compile_state_io_addr_offset), current_offset});
      current_offset += static_cast<uint32_t>(sizeof(uint64_t));
    }
    build_data_.semantic.ordered_arg_values.push_back(out_addr);
  }
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::InitArgsTableInfo(const TaskSemanticContributeContext &context) {
  const size_t inputs_size = context.op_desc->GetInputsSize();
  const size_t outputs_size = context.op_desc->GetOutputsSize();
  REQUIRE_COMPAT_UINT32(sizeof(uint64_t) * (inputs_size + outputs_size));
  uint32_t mem_size = static_cast<uint32_t>(sizeof(uint64_t) * (inputs_size + outputs_size));
  const uint32_t mem_size_t = static_cast<uint32_t>(
      sizeof(uint64_t) * (build_data_.semantic.input_addrs.size() + build_data_.semantic.output_addrs.size()));
  GELOGD("[OM2] mem_size %u, inputs_size %zu, outputs_size %zu, input_data_addrs size %zu, output_data_addrs size %zu.",
         mem_size, inputs_size, outputs_size, build_data_.semantic.input_addrs.size(),
         build_data_.semantic.output_addrs.size());
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
  (void)build_data_.semantic.args_table_entry.emplace();
  build_data_.semantic.args_table_entry->table_index = *context.next_args_table_index;
  build_data_.semantic.args_table_entry->host_offset = *context.next_host_args_offset;
  build_data_.semantic.args_table_entry->args_size = static_cast<int64_t>(mem_size) + kOffset;
  args_table_entry_ = &(*build_data_.semantic.args_table_entry);
  if (build_data_.semantic.args_table_entry.has_value()) {
    ++(*context.next_args_table_index);
    *context.next_host_args_offset +=
        Om2ModelUtils::ArgsSizeAlign8(static_cast<uint32_t>(build_data_.semantic.args_table_entry->args_size));
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
  build_data_.deploy_type = static_cast<uint32_t>(ext_handle.GetDeployTypeFlag());
  build_data_.mem_type = static_cast<uint32_t>(ext_handle.GetMemType());
  build_data_.memcpy_kind = static_cast<uint32_t>(ext_handle.GetMemcpyKind());
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
  const size_t ext_info_len = ext_handle.GetExtInfoLen();
  const uint8_t *ext_info_ptr = ext_handle.GetExtInfo();
  build_data_.ext_info_blob.assign(ext_info_ptr, ext_info_ptr + ext_info_len);
  build_data_.ext_info_blob_len = static_cast<uint32_t>(ext_info_len);
  return SUCCESS;
}

FunctionDef *KernelExTaskCodeBuilder::RenderTfAicpuKernelTaskDistribute() const {
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

FunctionDef *KernelExTaskCodeBuilder::RenderAssembleTfAicpuArgs() const {
  auto kernel_ex_args = ast_.Var("const uint8_t *", "kernel_ex_args");
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

FunctionDef *KernelExTaskCodeBuilder::RenderAssembleTfAicpuExSessionIdInfo() const {
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

FunctionDef *KernelExTaskCodeBuilder::RenderAssembleTfAicpuExKernelIdInfo() const {
  auto kernel_id = ast_.Var("uint64_t *", "kernel_id");
  auto tf_ai_cpu_ex_info = ast_.Var("TfAiCpuExInfo *", "tf_ai_cpu_ex_info");
  return ast_.DefineFunction("AssembleTfAicpuExKernelIdInfo", {kernel_id, tf_ai_cpu_ex_info}, "aclError",
                             {ast_.Assign(tf_ai_cpu_ex_info.Arrow("kernelID"), ast_.Deref(kernel_id)),
                              ast_.PostInc(ast_.Deref(kernel_id)), ast_.Return("ACL_SUCCESS")});
}

FunctionDef *KernelExTaskCodeBuilder::RenderAssembleTfAicpuExWorkSpaceAddrInfo() const {
  auto kernel_ex_task_info = ast_.Var("const uint8_t *", "kernel_ex_task_info");
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

FunctionDef *KernelExTaskCodeBuilder::RenderAssembleTfAicpuExInputOutputAddrInfo() const {
  auto args_info = ast_.Var("ArgsInfo *", "args_info");
  auto tf_ai_cpu_ex_info = ast_.Var("TfAiCpuExInfo *", "tf_ai_cpu_ex_info");
  return ast_.DefineFunction(
      "AssembleTfAicpuExInputOutputAddrInfo", {args_info, tf_ai_cpu_ex_info}, "aclError",
      {ChkNotNull(args_info),
       ast_.Assign(tf_ai_cpu_ex_info.Arrow("inputOutputAddr"),
                   ast_.StaticCast("uint64_t", ast_.ReinterpretCast("uintptr_t", args_info.Arrow("dev_addr")))),
       ast_.Return("ACL_SUCCESS")});
}

FunctionDef *KernelExTaskCodeBuilder::RenderAssembleTfAicpuExExtInfo() const {
  auto kernel_ex_ext_info = ast_.Var("const uint8_t *", "kernel_ex_ext_info");
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
  build_data_.semantic.task_type = context.task_type;
  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveInputAddrs(context, build_data_.semantic.input_addrs));
  GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveOutputAddrs(context, false, build_data_.semantic.output_addrs));
  GE_ASSERT_SUCCESS(InitArgsTableInfo(context));
  GE_ASSERT_SUCCESS(InitTaskExInfo(context));
  GE_ASSERT_SUCCESS(InitTaskExtInfo(context));
  GE_ASSERT_SUCCESS(InitLaunchInfo(context));
  for (const auto &addr : build_data_.semantic.ordered_arg_values) {
    OpArgDesc arg = TaskCodeBuilderUtil::ConvertAddrDesc(addr);
    arg.args_offset = build_data_.ordered_args.size() * 8U;
    build_data_.ordered_args.push_back(std::move(arg));
  }
  build_data_.args_info_num = static_cast<uint32_t>(build_data_.ordered_args.size());
  build_data_.engine_type_val = 0U;
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  (void)items.push_back(RenderTfAicpuKernelTaskDistribute());
  (void)items.push_back(RenderAssembleTfAicpuExSessionIdInfo());
  (void)items.push_back(RenderAssembleTfAicpuExKernelIdInfo());
  (void)items.push_back(RenderAssembleTfAicpuExWorkSpaceAddrInfo());
  (void)items.push_back(RenderAssembleTfAicpuExInputOutputAddrInfo());
  (void)items.push_back(RenderAssembleTfAicpuExExtInfo());
  (void)items.push_back(RenderAssembleTfAicpuArgs());
  GE_ASSERT_SUCCESS(RenderDispatchFunc(items));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDispatchFunc(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  GE_ASSERT_SUCCESS(RenderDispatchFuncSetup(body, op, ctx));
  GE_ASSERT_SUCCESS(RenderDispatchFuncLaunch(body, op, ctx));
  GE_ASSERT_SUCCESS(RenderDispatchFuncReport(body, op, ctx));
  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDispatchFuncSetup(std::vector<BodyItem> &body, const VarRef &op,
                                                        const VarRef &ctx) {
  auto num_io = ast_.Var("uint32_t", "num_io");
  (void)body.emplace_back(ast_.VarDecl(num_io, op.Arrow("dispatch_info").Attr("kernel_ex").Attr("args_info_num")));

  auto kernel_ex_args = ast_.Var("const uint8_t *", "kernel_ex_args");
  auto kernel_ex_args_len = ast_.Var("uint32_t", "kernel_ex_args_len");
  auto kernel_ex_task_info = ast_.Var("const uint8_t *", "kernel_ex_task_info");
  auto kernel_ex_task_info_len = ast_.Var("uint32_t", "kernel_ex_task_info_len");
  auto kernel_ex_ext_info = ast_.Var("const uint8_t *", "kernel_ex_ext_info");
  auto kernel_ex_ext_info_len = ast_.Var("uint32_t", "kernel_ex_ext_info_len");

  (void)body.emplace_back(ast_.VarDecl(kernel_ex_args, op.Arrow("dispatch_info").Attr("kernel_ex").Attr("args_blob")));
  (void)body.emplace_back(
      ast_.VarDecl(kernel_ex_args_len, op.Arrow("dispatch_info").Attr("kernel_ex").Attr("args_blob_len")));
  (void)body.emplace_back(
      ast_.VarDecl(kernel_ex_task_info, op.Arrow("dispatch_info").Attr("kernel_ex").Attr("task_info_blob")));
  (void)body.emplace_back(
      ast_.VarDecl(kernel_ex_task_info_len, op.Arrow("dispatch_info").Attr("kernel_ex").Attr("task_info_blob_len")));
  (void)body.emplace_back(
      ast_.VarDecl(kernel_ex_ext_info, op.Arrow("dispatch_info").Attr("kernel_ex").Attr("ext_info_blob")));
  (void)body.emplace_back(
      ast_.VarDecl(kernel_ex_ext_info_len, op.Arrow("dispatch_info").Attr("kernel_ex").Attr("ext_info_blob_len")));

  auto iow_addr = ast_.Var("std::vector<uint64_t>", "iow_addr");
  (void)body.emplace_back(ast_.VarDecl(iow_addr));
  (void)body.emplace_back(
      ast_.For(ast_.VarDecl("uint32_t", "i", ast_.UInt(0)), ast_.Var("", "i") < num_io, ast_.PostInc(ast_.Var("", "i")),
               {
                   ast_.VarDecl(ast_.Var("void *", "addr"),
                                ast_.Call("ResolveOpAddr", {op.Arrow("dispatch_info")
                                                                .Attr("kernel_ex")
                                                                .Attr("args_info")[ast_.Var("", "i")]
                                                                .Attr("addr")
                                                                .Attr("mem_src"),
                                                            op.Arrow("dispatch_info")
                                                                .Attr("kernel_ex")
                                                                .Attr("args_info")[ast_.Var("", "i")]
                                                                .Attr("addr")
                                                                .Attr("offset"),
                                                            ctx.Attr("total_dev_mem_ptr"),
                                                            ctx.Attr("session_scope_mem_ptr"), ctx.Attr("constants")})),
                   iow_addr.PushBack(ast_.ReinterpretCast("uint64_t", ast_.Var("", "addr"))),
               }));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDispatchFuncLaunchConfig(std::vector<BodyItem> &body, const VarRef &op,
                                                               const VarRef &ctx) {
  auto cfg_holder = ast_.Var("LaunchKernelCfgHolder", "cfg_holder");
  (void)body.emplace_back(ast_.VarDecl(cfg_holder));
  auto is_data_dump =
      ast_.Call("GetIsDataDump", {op.Arrow("op_name"), ctx.Attr("model_id"), ctx.Attr("instance_handle")});
  auto launch_config = ast_.Var("LaunchKernelConfig", "launch_config");
  (void)body.emplace_back(ast_.VarDecl(
      launch_config,
      ast_.InitList({op.Arrow("dispatch_info").Attr("kernel_ex").Attr("launch").Attr("schedule_mode"),
                     ast_.StaticCast("aclrtEngineType",
                                     op.Arrow("dispatch_info").Attr("kernel_ex").Attr("launch").Attr("engine_type")),
                     op.Arrow("dispatch_info").Attr("kernel_ex").Attr("launch").Attr("block_dim_offset"),
                     op.Arrow("dispatch_info").Attr("kernel_ex").Attr("launch").Attr("is_block_task_prefetch"),
                     is_data_dump, op.Arrow("dispatch_info").Attr("kernel_ex").Attr("launch").Attr("time_out"),
                     ast_.UInt(0)})));
  (void)body.emplace_back(ChkStatus(ast_.Call("AssembleLaunchConfig", {cfg_holder, launch_config})));

  auto mem_ptrs = ast_.Var("std::vector<void *>", "mem_ptrs");
  (void)body.emplace_back(ast_.VarDecl(mem_ptrs));
  auto args_var = ast_.Var("std::vector<uint8_t>", "args");
  (void)body.emplace_back(ast_.VarDecl(args_var));
  (void)body.emplace_back(args_var.Resize(static_cast<int64_t>(sizeof(STR_FWK_OP_KERNEL))));
  auto kernel_buf_var = ast_.Var("void *", "kernel_buf");
  (void)body.emplace_back(ast_.VarDecl(kernel_buf_var, nullptr));
  auto tf_ai_cpu_ex_info_var = ast_.Var("TfAiCpuExInfo", "tf_ai_cpu_ex_info");
  (void)body.emplace_back(ast_.VarDecl(tf_ai_cpu_ex_info_var));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDispatchFuncAssembleExInfo(std::vector<BodyItem> &body, const VarRef &op,
                                                                 const VarRef &ctx) {
  auto local_session_id = ast_.Var("uint64_t", "local_session_id");
  (void)body.emplace_back(ast_.VarDecl(local_session_id, ast_.Deref(ctx.Attr("session_id"))));
  (void)body.emplace_back(ChkStatus(
      ast_.Call("AssembleTfAicpuExSessionIdInfo",
                {local_session_id.Addr(), static_cast<int64_t>(sizeof(STR_FWK_OP_KERNEL)),
                 ctx.Attr("func_handles")[op.Arrow("dispatch_info").Attr("kernel_ex").Attr("tf_session_func_idx")],
                 op.Arrow("dispatch_info").Attr("kernel_ex").Attr("block_dim"),
                 ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("kernel_ex").Attr("stream_id")],
                 ast_.Var("", "cfg_holder").Attr("cfg").Addr(), ast_.Var("", "tf_ai_cpu_ex_info").Addr(),
                 ast_.Var("", "mem_ptrs")})));

  (void)body.emplace_back(ChkStatus(
      ast_.Call("AssembleTfAicpuExKernelIdInfo", {ctx.Attr("kernel_id"), ast_.Var("", "tf_ai_cpu_ex_info").Addr()})));

  (void)body.emplace_back(
      ChkStatus(ast_.Call("AssembleTfAicpuExWorkSpaceAddrInfo",
                          {ast_.Var("", "kernel_ex_task_info"), ast_.Var("", "kernel_ex_task_info_len"),
                           ast_.Var("", "tf_ai_cpu_ex_info").Addr(), ast_.Var("", "mem_ptrs")})));

  (void)body.emplace_back(ChkStatus(ast_.Call(
      "AssembleTfAicpuExInputOutputAddrInfo",
      {ctx.Attr("args_table").Attr("GetArgsInfo")(op.Arrow("dispatch_info").Attr("kernel_ex").Attr("args_table_idx")),
       ast_.Var("", "tf_ai_cpu_ex_info").Addr()})));

  (void)body.emplace_back(ChkStatus(
      ast_.Call("AssembleTfAicpuExExtInfo", {ast_.Var("", "kernel_ex_ext_info"), ast_.Var("", "kernel_ex_ext_info_len"),
                                             ast_.Var("", "tf_ai_cpu_ex_info").Addr(), ast_.Var("", "mem_ptrs")})));

  (void)body.emplace_back(ChkStatus(
      ast_.Call("AssembleTfAicpuArgs",
                {ast_.Var("", "kernel_ex_args"), ast_.Var("", "kernel_ex_args_len"), ast_.Var("", "args").Data(),
                 static_cast<int64_t>(sizeof(STR_FWK_OP_KERNEL)), ast_.Var("", "tf_ai_cpu_ex_info").Addr()})));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDispatchFuncLaunchTask(std::vector<BodyItem> &body, const VarRef &op,
                                                             const VarRef &ctx) {
  auto kernel_buf_var = ast_.Var("void *", "kernel_buf");
  (void)body.emplace_back(ChkStatus(AclrtMallocAlign32(
      kernel_buf_var.Addr(), static_cast<int64_t>(sizeof(STR_FWK_OP_KERNEL)), "ACL_MEM_MALLOC_HUGE_FIRST")));
  (void)body.emplace_back(ast_.Var("", "mem_ptrs").PushBack(kernel_buf_var));
  (void)body.emplace_back(ChkStatus(
      AclrtMemcpy(kernel_buf_var, static_cast<int64_t>(sizeof(STR_FWK_OP_KERNEL)), ast_.Var("", "args").Data(),
                  static_cast<int64_t>(sizeof(STR_FWK_OP_KERNEL)), "ACL_MEMCPY_HOST_TO_DEVICE")));

  (void)body.emplace_back(ChkStatus(ast_.Call(
      "TfAicpuKernelTaskDistribute",
      {ast_.Var("", "iow_addr"),
       ctx.Attr("args_table").Attr("GetArgsInfo")(op.Arrow("dispatch_info").Attr("kernel_ex").Attr("args_table_idx")),
       kernel_buf_var, static_cast<uint32_t>(sizeof(STR_FWK_OP_KERNEL)),
       ctx.Attr("func_handles")[op.Arrow("dispatch_info").Attr("kernel_ex").Attr("func_idx")],
       op.Arrow("dispatch_info").Attr("kernel_ex").Attr("block_dim"),
       ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("kernel_ex").Attr("stream_id")],
       ast_.Var("", "cfg_holder").Attr("cfg").Addr()})));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDispatchFuncLaunch(std::vector<BodyItem> &body, const VarRef &op,
                                                         const VarRef &ctx) {
  GE_ASSERT_SUCCESS(RenderDispatchFuncLaunchConfig(body, op, ctx));
  GE_ASSERT_SUCCESS(RenderDispatchFuncAssembleExInfo(body, op, ctx));
  GE_ASSERT_SUCCESS(RenderDispatchFuncLaunchTask(body, op, ctx));
  return SUCCESS;
}

Status KernelExTaskCodeBuilder::RenderDispatchFuncReport(std::vector<BodyItem> &body, const VarRef &op,
                                                         const VarRef &ctx) {
  auto io_tensors = ast_.Var("std::vector<Om2Tensor>", "io_tensors");
  (void)body.emplace_back(ast_.VarDecl(io_tensors));
  (void)body.emplace_back(io_tensors.Attr("reserve")(ast_.Var("", "num_io")));
  auto report_inputs = ast_.Var("std::vector<Om2TaskIoEntry>", "report_inputs");
  (void)body.emplace_back(ast_.VarDecl(report_inputs));
  auto report_outputs = ast_.Var("std::vector<Om2TaskIoEntry>", "report_outputs");
  (void)body.emplace_back(ast_.VarDecl(report_outputs));

  auto kex = op.Arrow("dispatch_info").Attr("kernel_ex");
  auto args_info = kex.Attr("args_info");
  auto idx = ast_.Var("", "_i");
  auto item = args_info[idx];
  auto tensor = item.Attr("data").Attr("tensor");

  (void)body.emplace_back(ast_.For(
      ast_.VarDecl("uint32_t", "_i", ast_.UInt(0)), idx < ast_.Var("", "num_io"), ast_.PostInc(idx),
      {io_tensors.PushBack(ast_.Call(
           "BuildOm2Tensor",
           {ast_.ReinterpretCast("void *", ast_.Var("", "iow_addr")[idx]), tensor.Attr("size"),
            tensor.Attr("data_type"), tensor.Attr("format"), tensor.Attr("shape"), tensor.Attr("shape_dims")})),
       ast_.VarDecl(ast_.Var("Om2TaskIoEntry", "_entry"),
                    ast_.InitList({ast_.Var("", "io_tensors").Attr("back")().Addr(), tensor.Attr("args_offset")})),
       ast_.If(item.Attr("type") != ast_.Var("", "OP_ARG_OUTPUT"), {report_inputs.PushBack(ast_.Var("", "_entry"))},
               {report_outputs.PushBack(ast_.Var("", "_entry"))})}));

  auto args_table_info = ctx.Attr("args_table").Attr("GetArgsInfo")(kex.Attr("args_table_idx"));
  (void)body.emplace_back(ChkStatus(
      ast_.Call("ReportLaunchedOm2Task",
                {op.Arrow("op_name"), kex.Attr("op_type"), ast_.UInt(0),
                 ast_.ReinterpretCast("uintptr_t", args_table_info.Arrow("dev_addr")), args_table_info.Arrow("size"),
                 report_inputs.Data(), ast_.StaticCast("uint64_t", report_inputs.Size()), report_outputs.Data(),
                 ast_.StaticCast("uint32_t", report_outputs.Size()), Arg(nullptr), Arg(nullptr), ast_.UInt(0U),
                 kex.Attr("task_type"), kex.Attr("block_dim"), ctx.Attr("stream_list")[kex.Attr("stream_id")],
                 ctx.Attr("model_id"), ctx.Attr("instance_handle")})));
  return SUCCESS;
}

int64_t KernelExTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const auto &kernel_ex_def = task_def.kernel_ex();
  return static_cast<int64_t>(kernel_ex_def.op_index());
}

Status KernelExTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});
  auto launch_values = std::vector<Arg>{
      static_cast<int64_t>(build_data_.semantic.launch.config.schedule_mode),
      static_cast<int64_t>(build_data_.semantic.launch.config.block_dim_offset),
      static_cast<int64_t>(build_data_.semantic.launch.config.is_block_task_prefetch),
      static_cast<int64_t>(build_data_.semantic.launch.config.time_out),
      static_cast<int64_t>(build_data_.engine_type_val),
  };
  auto kernel_ex_fields = std::vector<std::pair<std::string, Arg>>{
      {"args_info", TaskCodeBuilderUtil::RenderOpArgDesc(ast_, build_data_.ordered_args)},
      {"args_info_num", static_cast<int64_t>(build_data_.args_info_num)},
      {"op_type", Arg::StringLiteral(header_.op_type)},
      {"block_dim", build_data_.semantic.launch.block_dim},
      {"stream_id", build_data_.semantic.launch.stream_id},
      {"func_idx", static_cast<int64_t>(build_data_.semantic.launch.func_handle_index)},
      {"tf_session_func_idx", static_cast<int64_t>(build_data_.semantic.launch.tf_session_func_handle_index)},
      {"args_table_idx", static_cast<int64_t>(build_data_.semantic.args_table_entry->table_index)},
      {"args_blob", build_data_.args_blob.empty()
                        ? Arg(nullptr)
                        : ast_.ReinterpretCast("const uint8_t *",
                                               Arg::StringLiteral(SerializeBytesToOctalString(build_data_.args_blob)))},
      {"args_blob_len", static_cast<int64_t>(build_data_.args_blob_len)},
      {"task_info_blob", build_data_.task_info_blob.empty()
                             ? Arg(nullptr)
                             : ast_.ReinterpretCast("const uint8_t *", Arg::StringLiteral(SerializeBytesToOctalString(
                                                                           build_data_.task_info_blob)))},
      {"task_info_blob_len", static_cast<int64_t>(build_data_.task_info_blob_len)},
      {"ext_info_blob", build_data_.ext_info_blob.empty()
                            ? Arg(nullptr)
                            : ast_.ReinterpretCast("const uint8_t *", Arg::StringLiteral(SerializeBytesToOctalString(
                                                                          build_data_.ext_info_blob)))},
      {"ext_info_blob_len", static_cast<int64_t>(build_data_.ext_info_blob_len)},
      {"launch", ast_.InitList(launch_values)},
      {"task_type", static_cast<int64_t>(build_data_.semantic.task_type)},
  };
  fields.push_back(
      {"dispatch_info", ast_.DesignatedInit({{"kernel_ex", ast_.DesignatedInit(kernel_ex_fields, true)}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_KERNEL_EX, KernelExTaskCodeBuilder);
}  // namespace ge
