/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "kernel_task_code_builder.h"
#include <cinttypes>
#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "common/checker.h"
#include "common/ge_common/debug/ge_log.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "graph/args_format_desc.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/tensor_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/def_types.h"

namespace ge {
namespace {
constexpr uint32_t k2BitsMask = 0x00000003U;
const std::string kAllShapeInAicpu = "_AllShape";
constexpr int64_t kDefaultDimInfo = 0x100000001;
constexpr uint64_t kDefaultShapeNum = 0x100000000U;
const std::string kWspUnfoldedMode = "unfolded";
const std::string kWspFoldedMode = "folded";
const std::string kAttrNameAtomicWspMode = "wspMode";
constexpr char_t const *kMaxTilingSize = "op_para_size";
constexpr char_t const *kMaxAtomicCleanTilingSize = "atomic_op_para_size";
constexpr uint32_t kUBAlignedLen = 32U;
constexpr int32_t kSessionInfoOffset = 8;
constexpr int32_t kWidthPerChar = 3;
constexpr uint32_t kAicpuArgsExtInfoAddrOffset = 12U;
constexpr uint32_t kAicpuArgsioAddrOffset = 20U;

std::vector<Arg> ConvertToArgs(const std::vector<int64_t> &values) {
  std::vector<Arg> args;
  args.reserve(values.size());
  for (const auto value : values) {
    args.emplace_back(value);
  }
  return args;
}

void AppendShapeDesc(const ge::GeTensorDesc &tensor_desc, std::vector<int64_t> &shape_infos) {
  const auto &shape = tensor_desc.GetShape();
  if (shape.IsScalar()) {
    shape_infos.push_back(kDefaultDimInfo);
    shape_infos.push_back(0x1);  // shape value [1]
  } else {
    uint64_t dim_info{kDefaultShapeNum};
    dim_info |= (static_cast<uint64_t>(shape.GetDimNum()));
    shape_infos.push_back(static_cast<int64_t>(dim_info));
    for (const int64_t dim : shape.GetDims()) {
      shape_infos.push_back(dim);
    }
  }
}

bool IsWspAddrFolded(const OpDescPtr &op_desc) {
  const string *wsp_mode = ge::AttrUtils::GetStr(op_desc, kAttrNameAtomicWspMode);
  return (wsp_mode != nullptr) && (*wsp_mode == kWspFoldedMode);
}

bool IsMaterializedOutput(const AddrSemantic &semantic) {
  return semantic.kind != AddrValueKind::kOptionalEmpty;
}

bool IsAllKernelTask(const KernelTaskSemantic &semantic) {
  return Om2CodegenUtils::IsAllKernel(semantic.task_type);
}

bool IsAicoreTask(const KernelTaskSemantic &semantic) {
  return IsAllKernelTask(semantic) || Om2CodegenUtils::IsAICoreKernel(semantic.kernel_type);
}

bool IsAicpuTask(const KernelTaskSemantic &semantic) {
  return semantic.kernel_type == ge::ccKernelType::AI_CPU;
}

uint64_t GetOrderedArgByteSize(const AddrSemantic &semantic) {
  if (semantic.kind == AddrValueKind::kLevel1DescPtr) {
    return 0U;
  }
  if ((semantic.kind == AddrValueKind::kShapeInfoBuffer) && semantic.shape_info.has_value()) {
    return semantic.shape_info->size() * kAddressLen;
  }
  return kAddressLen;
}

}  // namespace

void KernelTaskCodeBuilder::AppendOrderedArgValue(const AddrSemantic &semantic) {
  if (args_table_entry_ == nullptr) {
    REPORT_INNER_ERR_MSG("E19999", "Args table entry is required before append ordered arg.");
    GELOGE(FAILED, "[OM2] Args table entry is required before append ordered arg.");
    return;
  }
  if (semantic.memory_app == MemoryAppType::kModelIo) {
    uint64_t current_offset = args_table_entry_->host_offset;
    for (const auto &ordered_arg : semantic_.ordered_arg_values) {
      current_offset += (ordered_arg.kind == AddrValueKind::kShapeInfoBuffer && ordered_arg.shape_info.has_value())
                            ? ordered_arg.shape_info->size() * kAddressLen
                            : kAddressLen;
    }
    io_addr_refresh_records_.push_back(
        IoAddrRefreshRecord{static_cast<uint64_t>(semantic.compile_state_io_addr_offset), current_offset});
  }
  semantic_.ordered_arg_values.push_back(semantic);
}

void KernelTaskCodeBuilder::AppendOrderedArgValueForAicpu(const AddrSemantic &semantic, const uint64_t addr_offset) {
  if (args_table_entry_ == nullptr) {
    REPORT_INNER_ERR_MSG("E19999", "Args table entry is required before append ordered arg.");
    GELOGE(FAILED, "[OM2] Args table entry is required before append ordered arg.");
    return;
  }

  if (semantic.memory_app == MemoryAppType::kModelIo) {
  io_addr_refresh_records_.push_back(
        IoAddrRefreshRecord{static_cast<uint64_t>(semantic.compile_state_io_addr_offset), addr_offset});
    GELOGI("[OM2]append input addr offset map: compile offset[%lu], args info offset[%lu]",
      semantic.compile_state_io_addr_offset, addr_offset);
  }
  semantic_.ordered_arg_values.push_back(semantic);
}

Status KernelTaskCodeBuilder::FillLevel1DescTargetOffsets() {
  size_t level1_desc_count = 0UL;
  while ((level1_desc_count < semantic_.ordered_arg_values.size()) &&
         (semantic_.ordered_arg_values[level1_desc_count].kind == AddrValueKind::kLevel1DescPtr)) {
    ++level1_desc_count;
  }
  uint64_t current_offset = 0U;
  size_t level1_desc_index = 0UL;
  for (size_t i = level1_desc_count; i < semantic_.ordered_arg_values.size(); ++i) {
    if ((semantic_.ordered_arg_values[i].kind == AddrValueKind::kShapeInfoBuffer) &&
        (level1_desc_index < level1_desc_count)) {
      semantic_.ordered_arg_values[level1_desc_index++].level1_target_offset = current_offset;
    }
    current_offset += GetOrderedArgByteSize(semantic_.ordered_arg_values[i]);
  }
  GE_ASSERT_TRUE(level1_desc_index == level1_desc_count,
                 "[OM2] Filled level1 desc count [%zu] mismatch expected [%zu].", level1_desc_index,
                 level1_desc_count);
  return SUCCESS;
}

Status KernelTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  GE_ASSERT_SUCCESS(TaskCodeBuilder::Contribute(context));
  GE_ASSERT_NOTNULL(context.next_args_table_index);
  GE_ASSERT_NOTNULL(context.next_host_args_offset);
  GE_ASSERT_NOTNULL(context.aicpu_task_count);
  semantic_.task_type = context.task_type;
  semantic_.kernel_type = static_cast<ccKernelType>(Om2CodegenUtils::IsAllKernel(context.task_type)
                                                        ? context.task_def.kernel_with_handle().context().kernel_type()
                                                        : context.task_def.kernel().context().kernel_type());
  kernel_name_ = context.task_def.kernel().kernel_name();
  GE_ASSERT_NOTNULL(context.op_desc);
  op_need_print_ = Om2CodegenUtils::OpNeedPrint(context.op_desc);
  is_soft_sync_op_ = IsAllKernelTask(semantic_) && Om2CodegenUtils::IsSoftSyncOp(context.op_desc);
  is_separately_clean_task_ = (!IsAllKernelTask(semantic_)) &&
                              Om2CodegenUtils::IsSeparatelyCleanTask(context.op_desc, kernel_name_);
  is_blocking_aicpu_op_ = IsAicpuTask(semantic_) && Om2CodegenUtils::IsBlockingAicpuOp(context.op_desc);
  GE_ASSERT_SUCCESS(CheckTaskSupport());
  if (IsAicpuTask(semantic_)) {
    semantic_.aicpu_task_index = *context.aicpu_task_count;
    ++(*context.aicpu_task_count);
  }
  if (IsAicpuTask(semantic_)) {
    GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveInputAddrs(context, semantic_.input_addrs));
    GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveOutputAddrs(context, true, semantic_.output_addrs));
  } else {
    GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveWorkspaceAddrs(context, semantic_.workspace_addrs));
    GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveInputAddrs(context, semantic_.input_addrs));
    GE_ASSERT_SUCCESS(Om2ModelUtils::ResolveOutputAddrs(context, true, semantic_.output_addrs));
  }

  GE_ASSERT_SUCCESS(BuildLaunchSemantic(context));

  if (IsAicpuTask(semantic_)) {
    GE_ASSERT_SUCCESS(BuildOrderedArgValuesForAicpu(context));
    GE_ASSERT_SUCCESS(BuildAicpuArgsSemantic(context));
    GE_ASSERT_SUCCESS(BuildAicpuExtInfoSemantic(context));
  } else {
    ArgsFormatInfo args_format_holder;
    GE_ASSERT_SUCCESS(BuildOrderedArgValuesForAicore(context, args_format_holder));
  }
  if (semantic_.args_table_entry.has_value()) {
    ++(*context.next_args_table_index);
    *context.next_host_args_offset += Om2ModelUtils::ArgsSizeAlign8(semantic_.args_table_entry->args_size);
  }
  return SUCCESS;
}

Status KernelTaskCodeBuilder::BuildLaunchSemantic(const TaskSemanticContributeContext &context) {
  GE_ASSERT_NOTNULL(context.func_handle_indices);
  auto &launch_semantic = semantic_.launch;
  launch_semantic.stream_id = context.task_def.stream_id();
  if ((semantic_.task_type == ModelTaskType::MODEL_TASK_VECTOR_ALL_KERNEL) ||
      (semantic_.task_type == ModelTaskType::MODEL_TASK_VECTOR_KERNEL)) {
    launch_semantic.config.engine_type = "ACL_RT_ENGINE_TYPE_AIV";
  }
  bool op_exec_never_timeout = false;
  if (AttrUtils::GetBool(context.op_desc, public_attr::OP_EXEC_NEVER_TIMEOUT, op_exec_never_timeout) &&
      op_exec_never_timeout) {
    launch_semantic.config.time_out = op_exec_never_timeout;
  }
  if (IsAllKernelTask(semantic_)) {
    const auto &kernel_def = context.task_def.kernel_with_handle();
    launch_semantic.config.block_dim_offset = kernel_def.block_dim_offset();
    launch_semantic.config.is_block_task_prefetch = kernel_def.is_block_task_prefetch();
    launch_semantic.block_dim = kernel_def.block_dim() == 0U ? 1U : kernel_def.block_dim();
    launch_semantic.config.schedule_mode = static_cast<uint8_t>(kernel_def.schedule_mode() & k2BitsMask);
  } else {
    const auto &kernel_def = context.task_def.kernel();
    launch_semantic.config.block_dim_offset = kernel_def.block_dim_offset();
    launch_semantic.config.is_block_task_prefetch = kernel_def.is_block_task_prefetch();
    launch_semantic.block_dim = kernel_def.block_dim() == 0U ? 1U : kernel_def.block_dim();
    const auto kernel_type = static_cast<ccKernelType>(kernel_def.context().kernel_type());
    if (Om2CodegenUtils::IsAICoreKernel(kernel_type)) {
      launch_semantic.config.schedule_mode = static_cast<uint8_t>(kernel_def.schedule_mode() & k2BitsMask);
    }
  }
  const std::string kernel_name = context.task_def.kernel().kernel_name();
  const std::string func_handle_key = IsAicpuTask(semantic_) ? context.op_desc->GetType() + kernel_name : kernel_name;
  const auto func_handle_it = context.func_handle_indices->find(func_handle_key);
  GE_ASSERT_TRUE(func_handle_it != context.func_handle_indices->end(),
                 "[OM2] Func handle key %s not found.", func_handle_key.c_str());
  launch_semantic.func_handle_index = func_handle_it->second;
  return SUCCESS;
}

Status KernelTaskCodeBuilder::CheckTaskSupport() const {
  if (op_need_print_) {
    REPORT_INNER_ERR_MSG("E19999", "Unsupport scenario for dfx.");
    GELOGE(FAILED, "Unsupport scenario for dfx.");
    return FAILED;
  }
  if (is_soft_sync_op_) {
    REPORT_INNER_ERR_MSG("E19999", "Unsupport scenario for dfx.");
    GELOGE(FAILED, "Unsupport scenario for static_to_dynamic_softsync_op.");
    return FAILED;
  }
  if (is_separately_clean_task_) {
    REPORT_INNER_ERR_MSG("E19999", "Unsupport scenario for dfx.");
    GELOGE(FAILED, "Unsupport scenario for atomic clean task.");
    return FAILED;
  }
  if (is_blocking_aicpu_op_) {
    REPORT_INNER_ERR_MSG("E19999", "Unsupport scenario for dfx.");
    GELOGE(FAILED, "Unsupport scenario for blocking_op.");
    return FAILED;
  }
  return SUCCESS;
}

Status KernelTaskCodeBuilder::AppendDistributionForAicpu(const std::vector<Arg> &args_vars,
                                                         std::vector<BodyItem> &items) {
  const auto op_index = header_.op_index;
  GE_ASSERT_TRUE(semantic_.args_table_entry.has_value());

  const std::string ioaddr_var_name = "op" + std::to_string(op_index) + "_iow_addr";
  auto ioaddr_var = ast_.Var("std::vector<uint64_t>", ioaddr_var_name);
  items.emplace_back(ast_.VarDecl(ioaddr_var, FlattenHostArgs(args_vars)));
  const std::string args_var_name = "op" + std::to_string(op_index) + "_args";
  auto args_var = ast_.Var("std::vector<uint8_t>", args_var_name);
  GE_ASSERT_SUCCESS(AppendAicpuArgsCode(ioaddr_var, args_var, items));
  auto cfg_holder = AppendLaunchConfigSetup(op_index, items);
  items.emplace_back(ChkStatus(ast_.Call("AicpuKernelTaskDistribute", {
      args_var,
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(semantic_.args_table_entry->table_index)),
      func_handles_[static_cast<int64_t>(semantic_.launch.func_handle_index)],
      static_cast<int64_t>(semantic_.launch.block_dim),
      stream_list_[static_cast<int64_t>(semantic_.launch.stream_id)],
      cfg_holder.Attr("cfg").Addr()})));
  return SUCCESS;
}

Status KernelTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  GELOGD("[OM2] start to generate task distribute code.");
  const auto op_index = header_.op_index;
  items.emplace_back(ast_.Comment("============================= " + header_.op_name + " ==============================="));
  GE_ASSERT_SUCCESS(GenArgsCode());
  std::vector<Arg> args_vars;
  for (const auto &args_addr_node : args_addr_nodes_) {
    (void)items.insert(items.cend(), args_addr_node.nodes.cbegin(), args_addr_node.nodes.cend());
    args_vars.emplace_back(ast_.Var("auto", args_addr_node.var_name));
  }
  GE_ASSERT_TRUE(semantic_.args_table_entry.has_value());
  if (IsAicoreTask(semantic_)) {
    auto cfg_holder = AppendLaunchConfigSetup(op_index, items);
    items.emplace_back(ChkStatus(ast_.Call("KernelTaskDistribute", {
        FlattenHostArgs(args_vars),
        args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(semantic_.args_table_entry->table_index)),
        func_handles_[static_cast<int64_t>(semantic_.launch.func_handle_index)],
        static_cast<int64_t>(semantic_.launch.block_dim),
        stream_list_[static_cast<int64_t>(semantic_.launch.stream_id)],
        cfg_holder.Attr("cfg").Addr()})));
  } else if (IsAicpuTask(semantic_)) {
    GE_ASSERT_SUCCESS(AppendDistributionForAicpu(args_vars, items));
  } else {
    REPORT_INNER_ERR_MSG("E19999", "Unsupported task type %d", static_cast<int32_t>(header_.task_type));
    GELOGE(FAILED, "[OM2] Unsupported task type %d, op_name=%s", static_cast<int32_t>(header_.task_type),
           header_.op_name.c_str());
    return FAILED;
  }

  items.emplace_back(ast_.BlankLine());
  return SUCCESS;
}

Status KernelTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  items.push_back(ast_.Field("constexpr const size_t", "max_launch_cfg_num = 8UL"));
  items.push_back(ast_.Field("constexpr int64_t", "kDImEndFlag = std::numeric_limits<int64_t>::min()"));
  items.push_back(BuildLaunchKernelCfgHolder());
  items.push_back(BuildLaunchKernelConfig());
  items.push_back(BuildAssembleLaunchConfig());
  items.push_back(BuildKernelTaskDistribute());
  items.push_back(ast_.Field("constexpr uint32_t", "kAicpuArgsExtInfoAddrOffset",
                             ast_.UInt(kAicpuArgsExtInfoAddrOffset)));
  items.push_back(ast_.Field("constexpr uint32_t", "kAicpuArgsio_addr_offset",
                             ast_.UInt(kAicpuArgsioAddrOffset)));
  items.push_back(BuildUpdateExtInfoSession());
  items.push_back(BuildAssembleAicpuExtInfo());
  items.push_back(BuildAssembleAicpuArgs());
  items.push_back(BuildAicpuKernelTaskDistribute());
  return SUCCESS;
}

StructDecl *KernelTaskCodeBuilder::BuildLaunchKernelCfgHolder() const {
  return ast_.Struct("LaunchKernelCfgHolder", {
      ast_.Field("aclrtLaunchKernelCfg", "cfg{}"),
      ast_.Field("aclrtLaunchKernelAttr", "attrs[max_launch_cfg_num]"),
  });
}

StructDecl *KernelTaskCodeBuilder::BuildLaunchKernelConfig() const {
  return ast_.Struct("LaunchKernelConfig", {
      ast_.Field("uint8_t", "schedule_mode{0U}"),
      ast_.Field("aclrtEngineType", "engine_type{ACL_RT_ENGINE_TYPE_AIC}"),
      ast_.Field("uint32_t", "block_dim_offset{0U}"),
      ast_.Field("bool", "is_block_task_prefetch{false}"),
      ast_.Field("bool", "is_data_dump{false}"),
      ast_.Field("uint16_t", "time_out{0U}"),
      ast_.Field("uint32_t", "local_memory_size{0U}"),
  });
}

FunctionDef *KernelTaskCodeBuilder::BuildAssembleLaunchConfig() const {
  auto holder = ast_.Var("LaunchKernelCfgHolder &", "holder");
  auto launch_config = ast_.Var("const LaunchKernelConfig &", "launch_config");
  auto actual_cfg_num = ast_.Var("size_t", "actual_cfg_num");
  auto attrs = holder.Attr("attrs");
  std::vector<BodyItem> body = {
      ast_.VarDecl(actual_cfg_num, "0UL"),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("schemMode"), launch_config.Attr("schedule_mode")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("engineType"), launch_config.Attr("engine_type")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("blockDimOffset"), launch_config.Attr("block_dim_offset")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_BLOCK_TASK_PREFETCH"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("isBlockTaskPrefetch"),
                  ast_.StaticCast("uint8_t", launch_config.Attr("is_block_task_prefetch"))),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_DATA_DUMP"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("isDataDump"),
                  ast_.StaticCast("uint8_t", launch_config.Attr("is_data_dump"))),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("timeout"), launch_config.Attr("time_out")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(holder.Attr("cfg").Attr("attrs"), attrs[0].Addr()),
      ast_.Assign(holder.Attr("cfg").Attr("numAttrs"), actual_cfg_num),
  };
  return ast_.DefineFunction("AssembleLaunchConfig", {holder, launch_config}, "void", ast_.Body(body));
}

FunctionDef *KernelTaskCodeBuilder::BuildKernelTaskDistribute() const {
  auto io_addrs = ast_.Var("const std::vector<uint64_t> &", "io_addrs");
  auto args_info = ast_.Var("ArgsInfo *", "args_info");
  auto func_handle = ast_.Var("aclrtFuncHandle", "func_handle");
  auto block_dim = ast_.Var("uint32_t", "block_dim");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto config = ast_.Var("aclrtLaunchKernelCfg *", "config");
  return ast_.DefineFunction("KernelTaskDistribute", {io_addrs, args_info, func_handle, block_dim, stream, config},
                             "aclError", {
                                 ChkNotNull(args_info),
                                 ChkStatus(MemcpyS(args_info.Arrow("host_addr"), args_info.Arrow("size"),
                                                   io_addrs.Data(), io_addrs.Size() * ast_.Sizeof("uint64_t"))),
                                 ChkStatus(AclrtLaunchKernelV2(func_handle, block_dim, args_info.Arrow("dev_addr"),
                                                              args_info.Arrow("size"), config, stream)),
                                 ast_.Return("ACL_SUCCESS"),
                             });
}

FunctionDef *KernelTaskCodeBuilder::BuildUpdateExtInfoSession() const {
  auto ext_info = ast_.Var("uint8_t *", "extInfo");
  auto session_info_offset = ast_.Var("size_t", "session_info_offset");
  auto session_id = ast_.Var("uint64_t *", "session_id");
  auto kernel_id = ast_.Var("uint64_t *", "kernel_id");
  auto session_info = ast_.Var("AicpuSessionInfo *", "session_info");
  return ast_.DefineFunction("UpdateExtInfoSession", {ext_info, session_info_offset, session_id, kernel_id},
                             "aclError", {
                                 ast_.VarDecl(session_info,
                                              ast_.ReinterpretCast("AicpuSessionInfo *",
                                                                   ext_info[session_info_offset].Addr())),
                                 ast_.Assign(session_info.Arrow("sessionId"), ast_.Deref(session_id)),
                                 ast_.Assign(session_info.Arrow("kernelId"), ast_.Deref(kernel_id)),
                                 ast_.Assign(session_info.Arrow("sessFlag"), true),
                                 ast_.PostInc(ast_.Deref(kernel_id)),
                                 ast_.Return("ACL_SUCCESS"),
                             });
}

FunctionDef *KernelTaskCodeBuilder::BuildAssembleAicpuExtInfo() const {
  auto ext_info = ast_.Var("uint8_t *", "ext_info");
  auto ext_info_len = ast_.Var("size_t", "ext_info_len");
  auto session_info_offset = ast_.Var("int32_t", "session_info_offset");
  auto session_id = ast_.Var("uint64_t *", "session_id");
  auto kernel_id = ast_.Var("uint64_t *", "kernel_id");
  auto dev_ext_info_mem_ptrs = ast_.Var("std::vector<void *> &", "dev_ext_info_mem_ptrs");
  auto index = ast_.Var("size_t", "index");
  auto tmp_ext_info = ast_.Var("std::unique_ptr<uint8_t[]>", "tmp_ext_info");
  auto dev_ptr = ast_.Var("void *", "dev_ptr");
  return ast_.DefineFunction("AssembleAicpuExtInfo",
                             {ext_info, ext_info_len, session_info_offset, session_id, kernel_id,
                              dev_ext_info_mem_ptrs, index},
                             "aclError", {
                                 ast_.VarDecl(tmp_ext_info, ast_.MakeUniqueArray(BuiltinType::kUInt8, ext_info_len)),
                                 MemcpyS(tmp_ext_info.GetPtr(), ext_info_len, ext_info, ext_info_len),
                                 ast_.If(session_info_offset != -1, {
                                     ChkStatus(ast_.Call("UpdateExtInfoSession",
                                                         {tmp_ext_info.GetPtr(), session_info_offset,
                                                          session_id, kernel_id})),
                                 }),
                                 ast_.VarDecl(dev_ptr, nullptr),
                                 ChkStatus(AclrtMallocAlign32(dev_ptr.Addr(), ext_info_len,
                                                              "ACL_MEM_MALLOC_HUGE_FIRST")),
                                 ChkStatus(AclrtMemcpy(dev_ptr, ext_info_len, tmp_ext_info.GetPtr(),
                                                       ext_info_len, "ACL_MEMCPY_HOST_TO_DEVICE")),
                                 ast_.Assign(dev_ext_info_mem_ptrs[index], dev_ptr),
                                 ast_.Return("ACL_SUCCESS"),
                             });
}

FunctionDef *KernelTaskCodeBuilder::BuildAssembleAicpuArgs() const {
  auto args = ast_.Var("uint8_t *", "args");
  auto args_len = ast_.Var("size_t", "args_len");
  auto ext_info_addr = ast_.Var("void *", "ext_info_addr");
  auto ext_info_len = ast_.Var("size_t", "ext_info_len");
  auto io_addr = ast_.Var("std::vector<uint64_t> &", "io_addr");
  auto target_args_ptr = ast_.Var("void *", "target_args_ptr");
  auto tmp_args = ast_.Var("std::unique_ptr<uint8_t[]>", "tmp_args");
  auto aicpu_param_head = ast_.Var("const auto", "aicpu_param_head");
  auto ext_info_addr_value = ast_.Var("uint64_t", "ext_info_addr_value");
  auto addrs_size = ast_.Var("size_t", "addrs_size");
  return ast_.DefineFunction("AssembleAicpuArgs", {args, args_len, ext_info_addr, ext_info_len, io_addr,
                                                   target_args_ptr},
                             "aclError", {
                                 ast_.VarDecl(tmp_args, ast_.MakeUniqueArray(BuiltinType::kUInt8, args_len)),
                                 MemcpyS(tmp_args.GetPtr(), args_len, args, args_len),
                                 ast_.VarDecl(aicpu_param_head,
                                              ast_.ReinterpretCast("AicpuParamHead *", tmp_args.GetPtr())),
                                 ast_.Assign(aicpu_param_head.Arrow("extInfoLength"),
                                             ast_.StaticCast("uint32_t", ext_info_len)),
                                 ast_.VarDecl(ext_info_addr_value, ast_.ReinterpretCast("uint64_t", ext_info_addr)),
                                 MemcpyS(tmp_args.GetPtr() + "kAicpuArgsExtInfoAddrOffset",
                                         ast_.Sizeof("uint64_t"), ext_info_addr_value.Addr(),
                                         ast_.Sizeof("uint64_t")),
                                 ast_.VarDecl(addrs_size, ast_.Sizeof("uint64_t") * io_addr.Size()),
                                 MemcpyS(tmp_args.GetPtr() + "kAicpuArgsio_addr_offset", addrs_size,
                                         io_addr.Data(), addrs_size),
                                 MemcpyS(target_args_ptr, args_len, tmp_args.GetPtr(), args_len),
                                 ast_.Return("ACL_SUCCESS"),
                             });
}

FunctionDef *KernelTaskCodeBuilder::BuildAicpuKernelTaskDistribute() const {
  auto args = ast_.Var("const std::vector<uint8_t> &", "args");
  auto args_info = ast_.Var("ArgsInfo *", "args_info");
  auto func_handle = ast_.Var("aclrtFuncHandle", "func_handle");
  auto block_dim = ast_.Var("uint32_t", "block_dim");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto config = ast_.Var("aclrtLaunchKernelCfg *", "config");
  return ast_.DefineFunction("AicpuKernelTaskDistribute", {args, args_info, func_handle, block_dim, stream, config},
                             "aclError", {
                                 ChkNotNull(args_info),
                                 ChkStatus(MemcpyS(args_info.Arrow("host_addr"), args_info.Arrow("size"),
                                                   args.Data(), args.Size())),
                                 ChkStatus(AclrtLaunchKernelV2(func_handle, block_dim, args_info.Arrow("dev_addr"),
                                                              args_info.Arrow("size"), config, stream)),
                                 ast_.Return("ACL_SUCCESS"),
                             });
}

Status KernelTaskCodeBuilder::GenArgsCode() {
  GELOGD("[OM2] start to generate args code.");
  args_addr_nodes_.clear();
  for (const auto &ordered_arg : semantic_.ordered_arg_values) {
    RenderedAddrInfo addr_gen_info;
    GE_ASSERT_SUCCESS(BuildAddrGenInfoFromSemantic(ordered_arg, addr_gen_info));
    args_addr_nodes_.push_back(std::move(addr_gen_info));
  }

  GELOGD("[OM2] generate args code end.");
  return SUCCESS;
}

Status KernelTaskCodeBuilder::BuildAddrGenInfoFromSemantic(const AddrSemantic &semantic,
                                                           RenderedAddrInfo &addr_gen_info) const {
  addr_gen_info.var_name = semantic.symbol_hint;
  addr_gen_info.mem_type =
      (semantic.memory_app == MemoryAppType::kModelIo) ? Om2MemoryAppType::kMemoryTypeModelIo
                                                       : Om2MemoryAppType::kMemoryTypeFix;
  addr_gen_info.compile_state_io_addr_offset = semantic.compile_state_io_addr_offset;
  switch (semantic.kind) {
    case AddrValueKind::kConstTensor:
      GE_ASSERT_TRUE(!semantic.symbol_hint.empty(), "[OM2] Const tensor symbol hint is empty.");
      return SUCCESS;
    case AddrValueKind::kInputInstance:
    case AddrValueKind::kOutputInstance:
    case AddrValueKind::kWorkspace:
      GE_ASSERT_TRUE(!semantic.symbol_hint.empty(), "[OM2] Addr semantic symbol hint is empty.");
      if (semantic.is_reused_from_upstream) {
        return SUCCESS;
      }
      addr_gen_info.nodes.emplace_back(
          ast_.VarDecl("auto", semantic.symbol_hint, GetAddr(total_dev_mem_ptr_, semantic.mem_offset)));
      return SUCCESS;
    case AddrValueKind::kOptionalEmpty:
      GE_ASSERT_TRUE(!semantic.symbol_hint.empty(), "[OM2] Optional addr symbol hint is empty.");
      addr_gen_info.nodes.emplace_back(ast_.VarDecl("auto", semantic.symbol_hint, nullptr));
      return SUCCESS;
    case AddrValueKind::kPlaceholder:
      GE_ASSERT_TRUE(!semantic.symbol_hint.empty(), "[OM2] Placeholder symbol hint is empty.");
      addr_gen_info.nodes.emplace_back(ast_.VarDecl("uint64_t", semantic.symbol_hint, ast_.UInt(0U)));
      return SUCCESS;
    case AddrValueKind::kCustomValue:
      GE_ASSERT_TRUE(!semantic.symbol_hint.empty(), "[OM2] Custom value symbol hint is empty.");
      addr_gen_info.nodes.emplace_back(ast_.VarDecl("auto", semantic.symbol_hint, ast_.UInt(semantic.custom_value)));
      return SUCCESS;
    case AddrValueKind::kLevel1DescPtr: {
      return BuildAddrGenInfoForLevel1DescPtr(semantic, addr_gen_info);
    }
    case AddrValueKind::kShapeInfoBuffer:
      return BuildAddrGenInfoForShapeInfoBuffer(semantic, addr_gen_info);
    default:
      REPORT_INNER_ERR_MSG("E19999", "Unsupported addr semantic kind %d in render stage.",
                           static_cast<int32_t>(semantic.kind));
      GELOGE(FAILED, "[OM2] Unsupported addr semantic kind %d in render stage.",
             static_cast<int32_t>(semantic.kind));
      return FAILED;
  }
}

Status KernelTaskCodeBuilder::BuildAddrGenInfoForLevel1DescPtr(const AddrSemantic &semantic,
                                                           RenderedAddrInfo &addr_gen_info) const {
  GE_ASSERT_TRUE(!semantic.symbol_hint.empty(), "[OM2] Level1 desc symbol hint is empty.");
  GE_ASSERT_TRUE(semantic_.args_table_entry.has_value(),
                  "[OM2] Args table entry is required for level1 desc render.");
  GE_ASSERT_TRUE(semantic.level1_target_offset.has_value(),
                  "[OM2] Level1 desc target offset is required for render.");
  const auto host_args_offset = semantic_.args_table_entry->host_offset + *semantic.level1_target_offset;
  auto symbol = ast_.Var("auto", semantic.symbol_hint);
  addr_gen_info.nodes.emplace_back(
      ast_.VarDecl(symbol, args_table_.Attr("GetDevArgAddr")(static_cast<int64_t>(host_args_offset))));
  addr_gen_info.nodes.emplace_back(ChkNotNull(symbol));
  return SUCCESS;
}

Status KernelTaskCodeBuilder::BuildAddrGenInfoForShapeInfoBuffer(const AddrSemantic &semantic,
                                                           RenderedAddrInfo &addr_gen_info) const {
  GE_ASSERT_TRUE(!semantic.symbol_hint.empty(), "[OM2] Shape info symbol hint is empty.");
  GE_ASSERT_TRUE(semantic.shape_info.has_value(), "[OM2] Shape info semantic data is missing.");
  addr_gen_info.nodes.emplace_back(
      ast_.VarDecl("std::vector<int64_t>", semantic.symbol_hint, ast_.InitList(ConvertToArgs(*semantic.shape_info))));
  return SUCCESS;
}

Status KernelTaskCodeBuilder::AppendAicpuArgsCode(Arg iow_addr, const VarRef &args_var,
                                                  std::vector<BodyItem> &items) {
  GELOGD("[OM2] start to assemble aicpu args code.");
  GE_ASSERT_TRUE(semantic_.aicpu_args.has_value());
  GE_ASSERT_TRUE(semantic_.aicpu_ext_info.has_value());
  const auto &aicpu_args = *semantic_.aicpu_args;
  const auto &aicpu_ext_info = *semantic_.aicpu_ext_info;
  GELOGD("[OM2] args size %u, ext info size %zu.", aicpu_args.args_size, aicpu_ext_info.total_len);

  const std::string args_str = SerializeBytesToOctalString(aicpu_args.args_buffer);
  const std::string ext_info_str = SerializeBytesToOctalString(aicpu_ext_info.serialized_bytes);
  const std::string &args_str_var_name = "op" + std::to_string(header_.op_index) + "_args_str";
  const std::string &ext_info_str_var_name = "op" + std::to_string(header_.op_index) + "_ext_info_str";
  auto args_str_var = ast_.Var("const char *", args_str_var_name);
  auto ext_info_str_var = ast_.Var("const char *", ext_info_str_var_name);
  items.emplace_back(ast_.VarDecl(args_str_var, Arg::StringLiteral(args_str)));
  items.emplace_back(ast_.VarDecl(ext_info_str_var, Arg::StringLiteral(ext_info_str)));
  items.emplace_back(ast_.VarDecl(args_var));
  items.emplace_back(args_var.Resize(static_cast<int64_t>(aicpu_args.args_size)));
  items.emplace_back(ast_.Call("AssembleAicpuExtInfo", {
      ast_.ReinterpretCast("uint8_t *", ast_.ConstCast("char *", ext_info_str_var)),
      static_cast<int64_t>(aicpu_ext_info.total_len),
      static_cast<int64_t>(aicpu_ext_info.session_info_offset),
      session_id_,
      kernel_id_.Addr(),
      dev_ext_info_mem_ptrs_,
      static_cast<int64_t>(semantic_.aicpu_task_index)}));
  items.emplace_back(ast_.Call("AssembleAicpuArgs", {
      ast_.ReinterpretCast("uint8_t *", ast_.ConstCast("char *", args_str_var)),
      static_cast<int64_t>(aicpu_args.args_size),
      dev_ext_info_mem_ptrs_[static_cast<int64_t>(semantic_.aicpu_task_index)],
      static_cast<int64_t>(aicpu_ext_info.total_len),
      iow_addr,
      args_var.Data()}));
  GELOGD("[OM2] AppendAicpuArgsCode end.");
  return SUCCESS;
}

Status KernelTaskCodeBuilder::GetKernelTaskMeta(const domi::TaskDef &task_def, domi::KernelContext &kernel_context,
                                                  uint32_t &args_size, uint32_t &kernel_type) const {
  if (Om2CodegenUtils::IsAllKernel(static_cast<ModelTaskType>(task_def.type()))) {
    const domi::KernelDefWithHandle &kernel_def = task_def.kernel_with_handle();
    args_size = static_cast<uint32_t>(kernel_def.args().size());
    kernel_context = kernel_def.context();
  } else {
    const domi::KernelDef &kernel_def = task_def.kernel();
    args_size = static_cast<uint32_t>(kernel_def.args().size());
    kernel_context = kernel_def.context();
  }
  kernel_type = kernel_context.kernel_type();
  return SUCCESS;
}

std::string KernelTaskCodeBuilder::SerializeBytesToOctalString(const std::vector<uint8_t> &buffer) const {
  std::ostringstream code_stream;
  for (size_t i = 0; i < buffer.size(); ++i) {
    code_stream << "\\";
    code_stream << std::oct << std::setw(kWidthPerChar) << std::setfill('0') << static_cast<int>(buffer[i]);
  }
  return code_stream.str();
}

Expr *KernelTaskCodeBuilder::BuildLaunchConfigExpr(const LaunchConfigSemantic &launch_config) const {
  return ast_.InitList({
      ast_.UInt(launch_config.schedule_mode),
      launch_config.engine_type,
      ast_.UInt(launch_config.block_dim_offset),
      launch_config.is_block_task_prefetch,
      launch_config.is_data_dump,
      ast_.UInt(launch_config.time_out)});
}

VarRef KernelTaskCodeBuilder::AppendLaunchConfigSetup(size_t op_index, std::vector<BodyItem> &items) const {
  const std::string cfg_holder_var_name = "op" + std::to_string(op_index) + "_cfg_holder";
  auto cfg_holder = ast_.Var("LaunchKernelCfgHolder", cfg_holder_var_name);
  items.emplace_back(ast_.VarDecl(cfg_holder));
  items.emplace_back(ast_.Call("AssembleLaunchConfig", {cfg_holder, BuildLaunchConfigExpr(semantic_.launch.config)}));
  return cfg_holder;
}

int64_t KernelTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const auto task_type = static_cast<ModelTaskType>(task_def.type());
  domi::KernelContext context;
  if (!Om2CodegenUtils::IsAllKernel(task_type)) {
    const domi::KernelDef &kernel_def = task_def.kernel();
    context = kernel_def.context();
  } else {
    const domi::KernelDefWithHandle &kernel_def = task_def.kernel_with_handle();
    context = kernel_def.context();
  }
  return static_cast<int64_t>(context.op_index());
}

Status KernelTaskCodeBuilder::UpdateShapeAndType(const std::vector<int64_t> &dims, const DataType data_type,
                                                   AicpuShapeAndType &shape_and_type) const {
  const auto dim_num = dims.size();
  if (dim_num > aicpu::FWKAdapter::kMaxShapeDims) {
    return ACL_ERROR_GE_PARAM_INVALID;
  }

  size_t index = 0U;
  for (; index < dim_num; ++index) {
    shape_and_type.dims[index] = dims[index];
  }
  if (index < aicpu::FWKAdapter::kMaxShapeDims) {
    shape_and_type.dims[index] = kDimEndFlag;
  }

  shape_and_type.type = static_cast<int32_t>(data_type);
  return SUCCESS;
}

Status KernelTaskCodeBuilder::UpdateShapeAndType(const GeShape &shape,
                                                 AicpuShapeAndType *const shape_and_type) const {
  const auto dim_num = shape.GetDimNum();
  if (dim_num > aicpu::FWKAdapter::kMaxShapeDims) {
    GELOGE(ACL_ERROR_GE_PARAM_INVALID,
           "[OM2][Check][DimNum]Update shape and type failed, as dim_num %zu is over max shape dims %u.",
           dim_num, aicpu::FWKAdapter::kMaxShapeDims);
    REPORT_INNER_ERR_MSG("E19999", "Update shape and type failed, as dim_num %zu is over max shape dims %u.",
                       dim_num, aicpu::FWKAdapter::kMaxShapeDims);
    return ACL_ERROR_GE_PARAM_INVALID;
  }
  size_t index = 0U;
  for (; index < dim_num; ++index) {
    shape_and_type->dims[index] = shape.GetDim(index);
  }
  if (index < aicpu::FWKAdapter::kMaxShapeDims) {
    shape_and_type->dims[index] = kDimEndFlag;
  }

  // now only support update shape, type is not support
  return SUCCESS;
}

Status KernelTaskCodeBuilder::ParseExtShape(AicpuExtInfo &aicpu_ext_info, const uint32_t num_tensor,
  const std::string &node_name, const bool all_shape, const OpDescPtr &op_desc) const
{
  std::vector<AicpuShapeAndType *> shape_and_type;
  shape_and_type.clear();
  GE_IF_BOOL_EXEC(aicpu_ext_info.infoLen != (num_tensor * sizeof(AicpuShapeAndType)),
                  REPORT_INNER_ERR_MSG("E19999", "Node[%s] parse ext shape failed as infoLen must be "
                                     "tensor_num[%u]*sizeof(ShapeAndType)[%zu] but %u.",
                                     node_name.c_str(), num_tensor, sizeof(AicpuShapeAndType),
                                     aicpu_ext_info.infoLen);
                  GELOGE(ACL_ERROR_GE_PARAM_INVALID,
                         "[OM2][Check][DataLen]Node[%s] parse ext shape failed as infoLen must be "
                         "tensor_num[%u]*sizeof(ShapeAndType)[%zu] but %u.",
                         node_name.c_str(), num_tensor, sizeof(AicpuShapeAndType), aicpu_ext_info.infoLen);
                  return ACL_ERROR_GE_PARAM_INVALID;);
  const auto tensor_info = PtrToPtr<char, AicpuShapeAndType>(aicpu_ext_info.infoMsg);
  if (all_shape) {
    for (uint32_t i = 0U; i < num_tensor; ++i) {
      shape_and_type.emplace_back(PtrAdd<AicpuShapeAndType>(tensor_info, static_cast<size_t>(num_tensor),
                                                                  static_cast<size_t>(i)));
      const auto tensor_desc = op_desc->MutableInputDesc(i);
      GE_CHECK_NOTNULL(tensor_desc);
      const auto &shape = tensor_desc->GetShape();
      GE_CHK_STATUS_RET(UpdateShapeAndType(shape, shape_and_type[static_cast<size_t>(i)]),
              "[OM2][Update][ShapeAndType] failed, Node[%s] tensor_info[%u] .",
              node_name.c_str(), i);
    }
  }
  GELOGI("[OM2]Node[%s] parse ext shape success infoLen=%u.", node_name.c_str(), aicpu_ext_info.infoLen);
  return SUCCESS;
}

Status KernelTaskCodeBuilder::ParseExtBitmap(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const
{
  GE_IF_BOOL_EXEC(aicpu_ext_info.infoLen != sizeof(uint64_t),
                  REPORT_INNER_ERR_MSG("E19999",
                                    "Node[%s] parse bit_map info failed as infoLen must be %zu but %u.",
                                    node_name.c_str(), sizeof(uint64_t), aicpu_ext_info.infoLen);
                  GELOGE(PARAM_INVALID,
                        "[OM2][Check][DataLen]Node[%s] parse bit_map info failed as infoLen must be %zu but %u.",
                        node_name.c_str(), sizeof(uint64_t), aicpu_ext_info.infoLen);
                  return PARAM_INVALID;);

  uint64_t *bit_map = PtrToPtr<char, uint64_t>(aicpu_ext_info.infoMsg);
  *(bit_map) |= 1UL;
  GELOGI("[OM2] Node[%s] bit_map info success infoLen=%u, value = %" PRIu64 ".",
          node_name.c_str(), aicpu_ext_info.infoLen, *(bit_map));
  return SUCCESS;
}

Status KernelTaskCodeBuilder::ParseExtTopicType(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const
{
  if (aicpu_ext_info.infoLen != sizeof(int32_t)) {
    REPORT_INNER_ERR_MSG("E19999",
                       "Node[%s] parse topic_type info failed as infoLen must be %zu but %u.",
                       node_name.c_str(), sizeof(int32_t), aicpu_ext_info.infoLen);
    GELOGE(ACL_ERROR_GE_PARAM_INVALID,
           "[Check][DataLen]Node[%s] parse topic_type info failed as infoLen must be %zu but %u.",
           node_name.c_str(), sizeof(int32_t), aicpu_ext_info.infoLen);
    return ACL_ERROR_GE_PARAM_INVALID;
  }
  GE_CHECK_NOTNULL(aicpu_ext_info.infoMsg);
  const int32_t type = *(PtrToPtr<char, int32_t>(aicpu_ext_info.infoMsg));
  int32_t deploy_type_flag = Om2CodegenUtils::TopicTypeToRtsFlag(type);
  if (deploy_type_flag == -1) {
    REPORT_INNER_ERR_MSG("E19999", "Node[%s] parse ext topic type failed as need %d %d %d %d but %d.",
                       node_name.c_str(),
                       aicpu::FWKAdapter::FWK_ADPT_TOPIC_DEVICE_ONLY,
                       aicpu::FWKAdapter::FWK_ADPT_TOPIC_DEVICE_FIRST,
                       aicpu::FWKAdapter::FWK_ADPT_TOPIC_HOST_ONLY,
                       aicpu::FWKAdapter::FWK_ADPT_TOPIC_HOST_FIRST,
                       type);
    GELOGE(ACL_ERROR_GE_PARAM_INVALID,
      "[Check][Type]Node[%s] parse ext topic type failed as need %d %d %d %d but %d.",
      node_name.c_str(),
      aicpu::FWKAdapter::FWK_ADPT_TOPIC_DEVICE_ONLY,
      aicpu::FWKAdapter::FWK_ADPT_TOPIC_DEVICE_FIRST,
      aicpu::FWKAdapter::FWK_ADPT_TOPIC_HOST_ONLY,
      aicpu::FWKAdapter::FWK_ADPT_TOPIC_HOST_FIRST,
      type);
    return ACL_ERROR_GE_PARAM_INVALID;
  } else if (deploy_type_flag == static_cast<int32_t>(RT_KERNEL_HOST_ONLY)) {
    REPORT_INNER_ERR_MSG("E19999", "Unsupport scenario. Node[%s], infoType=%d, infoLen=%u.", node_name.c_str(),
                      aicpu_ext_info.infoType, aicpu_ext_info.infoLen);
    GELOGE(FAILED, "[OM2] Unsupport scenario. Node[%s], infoType=%d, infoLen=%u.",
                  node_name.c_str(), aicpu_ext_info.infoType, aicpu_ext_info.infoLen);
    return FAILED;
  }
  return SUCCESS;
}

Status KernelTaskCodeBuilder::ParseExtAsyncWait(AicpuExtInfo &aicpu_ext_info, const std::string &node_name) const
{
  if (aicpu_ext_info.infoLen != sizeof(aicpu::FWKAdapter::AsyncWait)) {
    REPORT_INNER_ERR_MSG("E19999",
                         "Node[%s] parse ext async wait info failed as infoLen must be %zu but %u.",
                         node_name.c_str(), sizeof(aicpu::FWKAdapter::AsyncWait), aicpu_ext_info.infoLen);
    GELOGE(ACL_ERROR_GE_PARAM_INVALID,
           "[Check][DataLen]Node[%s] parse ext async wait info failed as infoLen must be %zu but %u.",
           node_name.c_str(), sizeof(aicpu::FWKAdapter::AsyncWait), aicpu_ext_info.infoLen);
    return ACL_ERROR_GE_PARAM_INVALID;
  }
  return SUCCESS;
}

Status KernelTaskCodeBuilder::ParseExtInfo(uint8_t *ext_info, const size_t ext_info_len, const OpDescPtr &op_desc,
  int32_t &session_info_offset, const uint32_t num_inputs, const uint32_t num_outputs, const std::string &node_name,
  const bool all_shape) const
{
  size_t offset = 0UL;
  while ((offset + sizeof(AicpuExtInfo)) <= ext_info_len) {
    auto tmp_ext_info_data = PtrAdd(ext_info, ext_info_len, offset);
    GE_CHECK_NOTNULL(tmp_ext_info_data);
    auto &aicpu_ext_info = *(PtrToPtr<uint8_t, AicpuExtInfo>(tmp_ext_info_data));
    GELOGD("[OM2] Ext infoType=%d, infoLen=%u.", aicpu_ext_info.infoType, aicpu_ext_info.infoLen);
    switch (aicpu_ext_info.infoType) {
      case aicpu::FWKAdapter::FWK_ADPT_EXT_SHAPE_TYPE:
        GELOGI("[OM2] Reserve infoType[%d] for Node[%s].", aicpu_ext_info.infoType, node_name.c_str());
        break;
      case aicpu::FWKAdapter::FWK_ADPT_EXT_INPUT_SHAPE:
        GE_CHK_STATUS_RET(ParseExtShape(aicpu_ext_info, num_inputs, node_name, all_shape, op_desc),
                                        "[OM2] Parse ext input shape failed, Node[%s].", node_name.c_str());
        break;
      case aicpu::FWKAdapter::FWK_ADPT_EXT_OUTPUT_SHAPE:
        GE_CHK_STATUS_RET(ParseExtShape(aicpu_ext_info, num_outputs, node_name, all_shape, op_desc),
                                        "[OM2] Parse ext output shape failed, Node[%s].", node_name.c_str());
        break;
      case aicpu::FWKAdapter::FWK_ADPT_EXT_SESSION_INFO:
        session_info_offset = offset + kSessionInfoOffset;
        break;
      case aicpu::FWKAdapter::FWK_ADPT_EXT_BITMAP:
        GE_CHK_STATUS_RET(ParseExtBitmap(aicpu_ext_info, node_name.c_str()),
                                         "[OM2] Parse ext bitmap failed, Node[%s].", node_name.c_str());
        break;
      case aicpu::FWKAdapter::FWK_ADPT_EXT_TOPIC_TYPE:
        GE_CHK_STATUS_RET(ParseExtTopicType(aicpu_ext_info, node_name.c_str()),
                                            "[OM2] Parse ext topic type failed, Node[%s].", node_name.c_str());
        break;
      case aicpu::FWKAdapter::FWK_ADPT_EXT_ASYNCWAIT: {
        GE_CHK_STATUS_RET(ParseExtAsyncWait(aicpu_ext_info, node_name.c_str()),
                          "[OM2] Parse ext async wait failed, Node[%s].", node_name.c_str());
        break;
      }
      default:
        GELOGD("[OM2] Node[%s] ignore infoType=%d, infoLen=%u.",
               node_name.c_str(), aicpu_ext_info.infoType, aicpu_ext_info.infoLen);
        break;
    }
    offset += sizeof(AicpuExtInfo);
    offset += aicpu_ext_info.infoLen;
  }

  GE_IF_BOOL_EXEC(offset != ext_info_len,
                  REPORT_INNER_ERR_MSG("E19999", "Node[%s] ext_info format error, parse not reach end,"
                                     "offset=%zu, ext_info_len=%zu.", node_name.c_str(), offset, ext_info_len);
                  GELOGE(ACL_ERROR_GE_PARAM_INVALID, "[OM2][Check][Size]Node[%s] ext_info format error,"
                         "parse not reach end, offset=%zu, ext_info_len=%zu.",
                         node_name.c_str(), offset, ext_info_len);
                  return ACL_ERROR_GE_PARAM_INVALID;);
  return SUCCESS;
}

Status KernelTaskCodeBuilder::InitAicpuTaskExtInfo(uint8_t *ext_info, size_t ext_info_len, const OpDescPtr op_desc,
                                                   int32_t &session_info_offset)
{
  GELOGD("[OM2] start to init aicpu task ext info.");
  std::string node_name = op_desc->GetName();
  const uint32_t num_inputs = static_cast<uint32_t>(op_desc->GetInputsSize());
  const uint32_t num_outputs = static_cast<uint32_t>(op_desc->GetOutputsSize());

  std::vector<AicpuShapeAndType *> output_shape_and_type;
  output_shape_and_type.clear();

  bool all_shape = false;
  (void)AttrUtils::GetBool(op_desc, kAllShapeInAicpu, all_shape);
  GE_ASSERT_SUCCESS(ParseExtInfo(ext_info, ext_info_len, op_desc, session_info_offset, num_inputs, num_outputs,
                                 node_name, all_shape));
  GELOGI("[OM2] Node[%s] parse ext info end.", node_name.c_str());
  return SUCCESS;
}

Status KernelTaskCodeBuilder::ParseArgsFormat(const OpDescPtr &op_desc, ArgsFormatInfo &args_format_holder) const {
  GE_ASSERT_NOTNULL(op_desc);
  (void)OpDescUtils::GetIrInputInstanceDescRange(op_desc, args_format_holder.ir_input_2_range);
  (void)OpDescUtils::GetIrOutputDescRange(op_desc, args_format_holder.ir_output_2_range);
  auto &arg_descs = args_format_holder.arg_descs;
  auto input_descs = op_desc->GetAllInputsDescPtr();
  for (const auto &arg_format : arg_descs) {
    if (arg_format.addr_type == AddrType::INPUT_DESC) {
      GE_ASSERT(arg_format.ir_idx >= 0 &&
                static_cast<size_t>(arg_format.ir_idx) < args_format_holder.ir_input_2_range.size());
      const auto &ir_range = args_format_holder.ir_input_2_range[static_cast<size_t>(arg_format.ir_idx)];
      std::vector<int64_t> shape_info{0};  // placeholder for offset
      for (size_t idx = 0UL; idx < ir_range.second; ++idx) {
        const size_t instance_idx = static_cast<size_t>(ir_range.first + idx);
        GE_ASSERT_TRUE(instance_idx < input_descs.size(), "Instance index [%zu] is out of range, max_size:[%zu].",
                       instance_idx, input_descs.size());
        AppendShapeDesc(*input_descs.at(instance_idx), shape_info);
      }
      shape_info[0UL] = static_cast<int64_t>(shape_info.size() * sizeof(uintptr_t));
      args_format_holder.level1_addr_cnt += ir_range.second + shape_info.size();
      args_format_holder.shape_infos.push_back(shape_info);
    } else if (arg_format.addr_type == AddrType::OUTPUT_DESC) {
      GE_ASSERT(arg_format.ir_idx >= 0 &&
                static_cast<size_t>(arg_format.ir_idx) < args_format_holder.ir_output_2_range.size());
      const auto &ir_range = args_format_holder.ir_output_2_range[static_cast<size_t>(arg_format.ir_idx)];
      std::vector<int64_t> shape_info{0};  // placeholder for offset
      args_format_holder.level1_addr_cnt += ir_range.second;
      for (size_t idx = 0UL; idx < ir_range.second; ++idx) {
        auto output_desc = op_desc->MutableOutputDesc(static_cast<uint32_t>(ir_range.first + idx));
        GE_ASSERT_NOTNULL(output_desc);
        AppendShapeDesc(*output_desc, shape_info);
      }
      shape_info[0UL] = static_cast<int64_t>(shape_info.size() * sizeof(uintptr_t));
      args_format_holder.level1_addr_cnt += ir_range.second + shape_info.size();
      args_format_holder.shape_infos.push_back(shape_info);
    } else if (arg_format.addr_type == AddrType::TILING_CONTEXT &&
               (arg_format.ir_idx == static_cast<int32_t>(TilingContextSubType::TILING_CONTEXT) ||
                arg_format.ir_idx == static_cast<int32_t>(TilingContextSubType::TILING_DATA))) {
      REPORT_INNER_ERR_MSG("E19999", "Unsupport scenario. addr_type[%d], ir_idx[%d].",
                     static_cast<int32_t>(AddrType::TILING_CONTEXT),
                     arg_format.ir_idx);
      GELOGE(FAILED, "[OM2] Unsupport scenario. addr_type[%d], ir_idx[%d].",
                     static_cast<int32_t>(AddrType::TILING_CONTEXT), arg_format.ir_idx);
      return FAILED;
    }
  }
  return SUCCESS;
}

size_t KernelTaskCodeBuilder::GetArgsSizeByFormat(const OpDescPtr op_desc, ArgsFormatInfo &args_format_holder) const {
  const auto &arg_descs = args_format_holder.arg_descs;
  size_t tmp_size = 0U;
  for (const auto &arg_desc : arg_descs) {
    (void)ArgsFormatDesc::GetArgSize(op_desc, arg_desc, tmp_size);
  }
  return tmp_size;
}

size_t KernelTaskCodeBuilder::GetExtraArgsSize(const OpDescPtr &op_desc, const ccKernelType kernel_type,
                                                 const ArgsFormatInfo &args_format_holder) const {
  size_t extra_size = 0UL;
  int32_t max_tiling_len{-1};
  (void)AttrUtils::GetInt(op_desc, kMaxTilingSize, max_tiling_len);
  int32_t max_atomic_tiling_len{-1};
  (void)AttrUtils::GetInt(op_desc, kMaxAtomicCleanTilingSize, max_atomic_tiling_len);
  if ((max_tiling_len > 0) || (max_atomic_tiling_len > 0)) {
    extra_size += kAddressLen;
  }

  if (kernel_type == ccKernelType::TE) {
    const auto is_wsp_addr_folded = IsWspAddrFolded(op_desc);
    if (is_wsp_addr_folded) {
      // kAddressLen: if folded mode, need add a memory for point to wsl addr list
      // kUBAlignedLen:
      // reserved 32B for aligned start with wsl addr list
      // -----------------------------------------------------------
      // | point to wsl addr list | over flow addr | wsl addr list |
      // -----------------------------------------------------------
      extra_size += kAddressLen + kUBAlignedLen;
    }
  }

  // level2 addr
  const size_t shape_info_size = args_format_holder.level1_addr_cnt * sizeof(int64_t);
  extra_size += shape_info_size;

  // reserved tiling sink tensor size
  return extra_size;
}

void KernelTaskCodeBuilder::InitArgsTableEntry(const TaskSemanticContributeContext &context,
                                                 const uint32_t args_size) {
  semantic_.args_table_entry.emplace();
  semantic_.args_table_entry->table_index = *context.next_args_table_index;
  semantic_.args_table_entry->args_size = args_size;
  semantic_.args_table_entry->host_offset = *context.next_host_args_offset;
  args_table_entry_ = &(*semantic_.args_table_entry);
}

std::vector<size_t> KernelTaskCodeBuilder::BuildMaterializedOutputIndices(
    const KernelTaskSemantic &kernel_semantic) const {
  std::vector<size_t> materialized_output_indices;
  for (size_t i = 0U; i < kernel_semantic.output_addrs.size(); ++i) {
    if (IsMaterializedOutput(kernel_semantic.output_addrs[i])) {
      materialized_output_indices.push_back(i);
    }
  }
  return materialized_output_indices;
}

void KernelTaskCodeBuilder::AppendOrderedPlaceholder(const TaskSemanticContributeContext &context) {
  AddrSemantic placeholder;
  placeholder.kind = AddrValueKind::kPlaceholder;
  placeholder.symbol_hint =
      "op" + std::to_string(context.op_index) + "_place_holder" + std::to_string(place_holder_var_index_++);
  AppendOrderedArgValue(placeholder);
}

void KernelTaskCodeBuilder::AppendOrderedCustomValue(const TaskSemanticContributeContext &context,
                                                       const uint64_t custom_value) {
  AddrSemantic custom_value_semantic;
  custom_value_semantic.kind = AddrValueKind::kCustomValue;
  custom_value_semantic.symbol_hint = "op" + std::to_string(context.op_index) + "_custom_value" +
                                      std::to_string(cust_value_var_index_++);
  custom_value_semantic.custom_value = custom_value;
  AppendOrderedArgValue(custom_value_semantic);
}

Status KernelTaskCodeBuilder::AppendOrderedInputOutputByInstanceIndex(
    const ArgDesc &arg_format) {
  if (arg_format.addr_type == AddrType::INPUT_INSTANCE) {
    const size_t inst_idx = static_cast<size_t>(arg_format.ir_idx);
    GE_ASSERT_TRUE(inst_idx < semantic_.input_addrs.size(),
                   "[OM2] Input instance idx [%zu] is invalid, size:[%zu].", inst_idx,
                   semantic_.input_addrs.size());
    AppendOrderedArgValue(semantic_.input_addrs[inst_idx]);
    return SUCCESS;
  }
  const size_t inst_idx = static_cast<size_t>(arg_format.ir_idx);
  GE_ASSERT_TRUE(inst_idx < materialized_output_indices_.size(),
                 "[OM2] Output instance idx [%zu] is invalid, size:[%zu].", inst_idx,
                 materialized_output_indices_.size());
  AppendOrderedArgValue(semantic_.output_addrs[materialized_output_indices_[inst_idx]]);
  return SUCCESS;
}

Status KernelTaskCodeBuilder::AppendOrderedInputOutputRange(
    const ArgDesc &arg_format, const ArgsFormatInfo &args_format_holder,
    const TaskSemanticContributeContext &context) {
  const bool is_input = (arg_format.addr_type == AddrType::INPUT);
  const auto &ir_2_range = is_input ? args_format_holder.ir_input_2_range : args_format_holder.ir_output_2_range;
  const auto iter = ir_2_range.find(static_cast<size_t>(arg_format.ir_idx));
  GE_ASSERT(iter != ir_2_range.end());
  const auto &range_pair = iter->second;
  if (is_input && range_pair.second == 0UL) {
    AppendOrderedPlaceholder(context);
    return SUCCESS;
  }
  size_t begin_idx = range_pair.first;
  for (size_t i = 0UL; i < range_pair.second; ++i, ++begin_idx) {
    if (is_input) {
      GE_ASSERT_TRUE(begin_idx < semantic_.input_addrs.size(),
                     "[OM2] Input range index [%zu] is invalid, size:[%zu].", begin_idx,
                     semantic_.input_addrs.size());
      AppendOrderedArgValue(semantic_.input_addrs[begin_idx]);
    } else {
      GE_ASSERT_TRUE(begin_idx < materialized_output_indices_.size(),
                     "[OM2] Output range index [%zu] is invalid, size:[%zu].", begin_idx,
                     materialized_output_indices_.size());
      AppendOrderedArgValue(semantic_.output_addrs[materialized_output_indices_[begin_idx]]);
    }
  }
  return SUCCESS;
}

Status KernelTaskCodeBuilder::AppendOrderedWorkspace(const ArgDesc &arg_format) {
  if (arg_format.ir_idx < 0) {
    for (const auto &workspace_addr : semantic_.workspace_addrs) {
      AppendOrderedArgValue(workspace_addr);
    }
    return SUCCESS;
  }
  const size_t workspace_idx = static_cast<size_t>(arg_format.ir_idx);
  GE_ASSERT_TRUE(workspace_idx < semantic_.workspace_addrs.size(),
                 "[OM2] Workspace idx [%zu] is invalid, size:[%zu].", workspace_idx,
                 semantic_.workspace_addrs.size());
  AppendOrderedArgValue(semantic_.workspace_addrs[workspace_idx]);
  return SUCCESS;
}

Status KernelTaskCodeBuilder::AppendOrderedArgsByFormat(
    const TaskSemanticContributeContext &context, const ArgsFormatInfo &args_format_holder,
    std::vector<ArgDesc> &dynamic_args_desc) {
  place_holder_var_index_ = 0;
  cust_value_var_index_ = 0;
  for (const auto &arg_format : args_format_holder.arg_descs) {
    switch (arg_format.addr_type) {
      case AddrType::INPUT_INSTANCE:
      case AddrType::OUTPUT_INSTANCE:
        GE_ASSERT_SUCCESS(AppendOrderedInputOutputByInstanceIndex(arg_format));
        break;
      case AddrType::INPUT:
      case AddrType::OUTPUT:
        GE_ASSERT_SUCCESS(AppendOrderedInputOutputRange(arg_format, args_format_holder, context));
        break;
      case AddrType::WORKSPACE:
        GE_ASSERT_SUCCESS(AppendOrderedWorkspace(arg_format));
        break;
      case AddrType::PLACEHOLDER:
        AppendOrderedPlaceholder(context);
        break;
      case AddrType::CUSTOM_VALUE:
        AppendOrderedCustomValue(context, *(PtrToPtr<uint8_t, uint64_t>(arg_format.reserved)));
        break;
      case AddrType::INPUT_DESC:
      case AddrType::OUTPUT_DESC: {
        const size_t dynamic_idx = dynamic_args_desc.size();
        dynamic_args_desc.push_back(arg_format);
        AddrSemantic level1_desc_ptr;
        level1_desc_ptr.kind = AddrValueKind::kLevel1DescPtr;
        level1_desc_ptr.symbol_hint =
            "op" + std::to_string(context.op_index) + "_io_desc" + std::to_string(dynamic_idx);
        AppendOrderedArgValue(level1_desc_ptr);
        break;
      }
      default:
        REPORT_INNER_ERR_MSG("E19999", "Args Format type %d is currently not supported.",
                             static_cast<int32_t>(arg_format.addr_type));
        GELOGE(FAILED, "[OM2] Args Format type %d is currently not supported.",
               static_cast<int32_t>(arg_format.addr_type));
        return FAILED;
    }
  }
  return SUCCESS;
}

Status KernelTaskCodeBuilder::AppendShapeInfoOrderedArgs(
    const TaskSemanticContributeContext &context, const ArgsFormatInfo &args_format_holder,
    const std::vector<ArgDesc> &dynamic_args_desc) {
  GE_ASSERT(dynamic_args_desc.size() == args_format_holder.shape_infos.size());
  for (size_t i = 0UL; i < dynamic_args_desc.size(); ++i) {
    AddrSemantic shape_info_buffer;
    shape_info_buffer.kind = AddrValueKind::kShapeInfoBuffer;
    shape_info_buffer.symbol_hint = "op" + std::to_string(context.op_index) + "_shape_info" + std::to_string(i);
    shape_info_buffer.shape_info = args_format_holder.shape_infos[i];
    AppendOrderedArgValue(shape_info_buffer);

    const auto &dynamic_arg = dynamic_args_desc[i];
    const bool is_input = (dynamic_arg.addr_type == AddrType::INPUT_DESC);
    const auto &ir_2_range = is_input ? args_format_holder.ir_input_2_range : args_format_holder.ir_output_2_range;
    const auto iter = ir_2_range.find(static_cast<size_t>(dynamic_arg.ir_idx));
    GE_ASSERT(iter != ir_2_range.end());
    const auto &range_pair = iter->second;
    size_t begin_idx = range_pair.first;
    for (size_t idx = 0UL; idx < range_pair.second; ++idx, ++begin_idx) {
      if (is_input) {
        GE_ASSERT_TRUE(begin_idx < semantic_.input_addrs.size(),
                       "[OM2] Input desc range index [%zu] is invalid, size:[%zu].", begin_idx,
                       semantic_.input_addrs.size());
        AppendOrderedArgValue(semantic_.input_addrs[begin_idx]);
      } else {
        GE_ASSERT_TRUE(begin_idx < materialized_output_indices_.size(),
                       "[OM2] Output desc range index [%zu] is invalid, size:[%zu].", begin_idx,
                       materialized_output_indices_.size());
        AppendOrderedArgValue(semantic_.output_addrs[materialized_output_indices_[begin_idx]]);
      }
    }
  }
  return SUCCESS;
}

Status KernelTaskCodeBuilder::BuildOrderedArgValuesForAicore(const TaskSemanticContributeContext &context,
                                                               ArgsFormatInfo &args_format_holder) {
  GE_ASSERT_NOTNULL(context.op_desc);
  domi::KernelContext kernel_context;
  uint32_t args_size = 0U;
  uint32_t kernel_type = 0U;
  GE_ASSERT_SUCCESS(GetKernelTaskMeta(context.task_def, kernel_context, args_size, kernel_type));
  GE_ASSERT_TRUE(!kernel_context.args_format().empty());
  GE_ASSERT_SUCCESS(ArgsFormatDesc::Parse(context.op_desc, kernel_context.args_format(), args_format_holder.arg_descs),
                    "[OM2] Formatted args [%s] parsed failed.", kernel_context.args_format().c_str());

  GE_ASSERT_SUCCESS(ParseArgsFormat(context.op_desc, args_format_holder), "[OM2] ParseArgsFormat failed, op:[%s].",
                    context.op_desc->GetNamePtr());

  const size_t format_args_size = GetArgsSizeByFormat(context.op_desc, args_format_holder);
  args_size = std::max(args_size, static_cast<uint32_t>(format_args_size));
  const size_t extra_args_size = GetExtraArgsSize(context.op_desc, static_cast<ccKernelType>(kernel_type),
                                                  args_format_holder);
  GE_ASSERT_TRUE(!AddOverflow(args_size, static_cast<uint32_t>(extra_args_size), args_size));

  InitArgsTableEntry(context, args_size);
  materialized_output_indices_ = BuildMaterializedOutputIndices(semantic_);
  std::vector<ArgDesc> dynamic_args_desc;
  GE_ASSERT_SUCCESS(AppendOrderedArgsByFormat(context, args_format_holder, dynamic_args_desc));
  GE_ASSERT_SUCCESS(AppendShapeInfoOrderedArgs(context, args_format_holder, dynamic_args_desc));
  GE_ASSERT_SUCCESS(FillLevel1DescTargetOffsets());
  return SUCCESS;
}

Status KernelTaskCodeBuilder::BuildOrderedArgValuesForAicpu(const TaskSemanticContributeContext &context) {
  domi::KernelContext kernel_context;
  uint32_t args_size = 0U;
  uint32_t kernel_type = 0U;
  GE_ASSERT_SUCCESS(GetKernelTaskMeta(context.task_def, kernel_context, args_size, kernel_type));
  GE_ASSERT_NOTNULL(context.next_args_table_index);
  GE_ASSERT_NOTNULL(context.next_host_args_offset);
  semantic_.args_table_entry.emplace();
  semantic_.args_table_entry->table_index = *context.next_args_table_index;
  semantic_.args_table_entry->args_size = args_size;
  semantic_.args_table_entry->host_offset = *context.next_host_args_offset;
  args_table_entry_ = &(*semantic_.args_table_entry);
  uint64_t addr_offset = *context.next_host_args_offset + kAicpuArgsioAddrOffset;
  for (const auto &input_addr : semantic_.input_addrs) {
    AppendOrderedArgValueForAicpu(input_addr, addr_offset);
    addr_offset += kAddressLen;
  }
  for (const auto &output_addr : semantic_.output_addrs) {
    if (!IsMaterializedOutput(output_addr)) {
      continue;
    }
    AppendOrderedArgValueForAicpu(output_addr, addr_offset);
    addr_offset += kAddressLen;
  }
  return SUCCESS;
}

Status KernelTaskCodeBuilder::BuildAicpuArgsSemantic(const TaskSemanticContributeContext &context) {
  domi::KernelContext kernel_context;
  uint32_t args_size = 0U;
  uint32_t kernel_type = 0U;
  GE_ASSERT_SUCCESS(GetKernelTaskMeta(context.task_def, kernel_context, args_size, kernel_type));
  const auto &args = context.task_def.kernel().args();
  semantic_.aicpu_args.emplace();
  semantic_.aicpu_args->args_size = args_size;
  semantic_.aicpu_args->args_buffer.assign(args.begin(), args.end());
  return SUCCESS;
}

Status KernelTaskCodeBuilder::BuildAicpuExtInfoSemantic(const TaskSemanticContributeContext &context) {
  const auto &ext_info = context.task_def.kernel().kernel_ext_info();
  std::vector<uint8_t> ext_info_buffer(ext_info.begin(), ext_info.end());
  int32_t session_info_offset = -1;
  GE_ASSERT_SUCCESS(
      InitAicpuTaskExtInfo(ext_info_buffer.data(), ext_info_buffer.size(), context.op_desc, session_info_offset));
  semantic_.aicpu_ext_info.emplace();
  semantic_.aicpu_ext_info->total_len = ext_info_buffer.size();
  semantic_.aicpu_ext_info->session_info_offset = session_info_offset;
  semantic_.aicpu_ext_info->serialized_bytes = std::move(ext_info_buffer);
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_KERNEL, KernelTaskCodeBuilder);
REGISTER_TASK_CODE_BUILDER(MODEL_TASK_ALL_KERNEL, KernelTaskCodeBuilder);
REGISTER_TASK_CODE_BUILDER(MODEL_TASK_VECTOR_KERNEL, KernelTaskCodeBuilder);
REGISTER_TASK_CODE_BUILDER(MODEL_TASK_VECTOR_ALL_KERNEL, KernelTaskCodeBuilder);
REGISTER_TASK_CODE_BUILDER(MODEL_TASK_PREPROCESS_KERNEL, KernelTaskCodeBuilder);
}  // namespace ge
