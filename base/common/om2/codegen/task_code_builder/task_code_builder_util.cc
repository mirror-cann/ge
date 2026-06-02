/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"

namespace ge {
namespace {
const char *ToOm2L0ArgKind(AddrValueKind kind) {
  switch (kind) {
    case AddrValueKind::kInputInstance:
    case AddrValueKind::kConstTensor:
      return "OM2_L0_ARG_INPUT";
    case AddrValueKind::kOutputInstance:
      return "OM2_L0_ARG_OUTPUT";
    case AddrValueKind::kWorkspace:
      return "OM2_L0_ARG_WORKSPACE";
    case AddrValueKind::kTiling:
      return "OM2_L0_ARG_TILING";
    case AddrValueKind::kShapeInfoBuffer:
      return "OM2_L0_ARG_SHAPE_INFO";
    case AddrValueKind::kLevel1DescPtr:
      return "OM2_L0_ARG_LEVEL1_DESC";
    case AddrValueKind::kPlaceholder:
      return "OM2_L0_ARG_PLACEHOLDER";
    case AddrValueKind::kCustomValue:
      return "OM2_L0_ARG_CUSTOM_VALUE";
    case AddrValueKind::kFftsAddr:
      return "OM2_L0_ARG_FFTS_ADDR";
    case AddrValueKind::kEventAddr:
      return "OM2_L0_ARG_EVENT_ADDR";
    case AddrValueKind::kOverflowAddr:
      return "OM2_L0_ARG_OVERFLOW_ADDR";
    case AddrValueKind::kOptionalEmpty:
    case AddrValueKind::kEmptyAddr:
      return "OM2_L0_ARG_EMPTY_ADDR";
    default:
      return "OM2_L0_ARG_EMPTY_ADDR";
  }
}

uint64_t GetOrderedArgByteSizeForL0(const AddrSemantic &addr) {
  if ((addr.kind == AddrValueKind::kShapeInfoBuffer) && addr.shape_info.has_value()) {
    return addr.shape_info->size() * sizeof(uint64_t);
  }
  return sizeof(uint64_t);
}

uint64_t GetL0SlotSizeOrValue(const AddrSemantic &addr) {
  if ((addr.kind == AddrValueKind::kShapeInfoBuffer) && addr.shape_info.has_value()) {
    return addr.shape_info->size();
  }
  if (addr.kind == AddrValueKind::kTiling) {
    return addr.byte_size;
  }
  if (addr.kind == AddrValueKind::kCustomValue) {
    return addr.custom_value;
  }
  return 0U;
}
}  // namespace

Expr *TaskCodeBuilderUtil::BuildTaskIoEntries(AstBuildContext &ast, const std::vector<AddrSemantic> &addrs) {
  std::vector<Arg> entries;
  for (const auto &addr : addrs) {
    if (!addr.tensor_info.has_value()) {
      continue;
    }
    entries.emplace_back(std::vector<Arg>{
        ast.Var("Om2Tensor", addr.symbol_hint).Addr(),
        std::to_string(addr.tensor_info->args_offset) + "U"});
  }
  return ast.InitList(entries);
}

Expr *TaskCodeBuilderUtil::BuildWorkspaceAddrs(AstBuildContext &ast, const std::vector<AddrSemantic> &addrs) {
  std::vector<Arg> entries;
  entries.reserve(addrs.size());
  for (const auto &addr : addrs) {
    entries.emplace_back(ast.Call("PtrToU64", {ast.Var("auto", addr.symbol_hint)}));
  }
  return ast.InitList(entries);
}

Expr *TaskCodeBuilderUtil::BuildWorkspaceSizes(AstBuildContext &ast, const std::vector<AddrSemantic> &addrs) {
  std::vector<Arg> entries;
  entries.reserve(addrs.size());
  for (const auto &addr : addrs) {
    entries.emplace_back(std::to_string(addr.byte_size) + "U");
  }
  return ast.InitList(entries);
}

Expr *TaskCodeBuilderUtil::BuildL0ArgSlotEntries(AstBuildContext &ast,
                                                 const std::vector<AddrSemantic> &ordered_args) {
  std::vector<Arg> entries;
  entries.reserve(ordered_args.size());
  uint64_t args_offset = 0U;
  uint32_t workspace_index = 0U;
  for (const auto &addr : ordered_args) {
    const uint64_t value = GetL0SlotSizeOrValue(addr);
    const Arg related_index = addr.kind == AddrValueKind::kWorkspace
                                  ? Arg(std::to_string(workspace_index++) + "U")
                                  : (addr.tensor_info.has_value()
                                         ? Arg(std::to_string(addr.tensor_info->args_offset / sizeof(uint64_t)) + "U")
                                         : Arg("0U"));
    entries.emplace_back(std::vector<Arg>{
        ToOm2L0ArgKind(addr.kind),
        "0U",
        std::to_string(args_offset) + "U",
        std::to_string(value) + "UL",
        related_index,
        std::to_string(addr.event_id) + "U",
        std::to_string(addr.level1_target_offset.value_or(0U)) + "U"});
    args_offset += GetOrderedArgByteSizeForL0(addr);
  }
  return ast.InitList(entries);
}

namespace {
void AppendReportTaskCommonArgs(AstBuildContext &ast, const TaskSemanticHeader &header,
                                const ArgsTableEntrySemantic *args_table_entry,
                                const std::vector<AddrSemantic> &input_addrs,
                                const std::vector<AddrSemantic> &output_addrs,
                                const std::vector<AddrSemantic> &workspace_addrs,
                                ModelTaskType task_type, Arg stream,
                                const VarRef &args_table, bool use_args_info_size,
                                std::vector<Arg> &args) {
  args.emplace_back(Arg::StringLiteral(header.op_name));
  args.emplace_back(Arg::StringLiteral(header.op_type));
  args.emplace_back(std::to_string(header.op_desc_id) + "U");
  if (args_table_entry != nullptr) {
    auto args_info = args_table.Attr("GetArgsInfo")(static_cast<int64_t>(args_table_entry->table_index));
    args.emplace_back(ast.ReinterpretCast("uintptr_t", args_info.Arrow("dev_addr")));
    args.emplace_back(use_args_info_size ? Arg(args_info.Arrow("size"))
                                         : Arg(std::to_string(args_table_entry->args_size) + "U"));
  } else {
    args.emplace_back(ast.UInt(0U));
    args.emplace_back("0U");
  }
  args.emplace_back(TaskCodeBuilderUtil::BuildTaskIoEntries(ast, input_addrs));
  args.emplace_back(TaskCodeBuilderUtil::BuildTaskIoEntries(ast, output_addrs));
  args.emplace_back(TaskCodeBuilderUtil::BuildWorkspaceAddrs(ast, workspace_addrs));
  args.emplace_back(TaskCodeBuilderUtil::BuildWorkspaceSizes(ast, workspace_addrs));
  args.emplace_back(ast.UInt(static_cast<uint32_t>(task_type)));
  args.emplace_back(stream);
}
}  // namespace

ExprRef TaskCodeBuilderUtil::BuildReportLaunchedTaskCall(AstBuildContext &ast, const TaskSemanticHeader &header,
                                                         const ArgsTableEntrySemantic *args_table_entry,
                                                         const std::vector<AddrSemantic> &input_addrs,
                                                         const std::vector<AddrSemantic> &output_addrs,
                                                         const std::vector<AddrSemantic> &workspace_addrs,
                                                         ModelTaskType task_type, Arg stream,
                                                         const VarRef &model_id, const VarRef &instance_handle,
                                                         const VarRef &args_table, bool use_args_info_size,
                                                         bool is_raw_address) {
  std::vector<Arg> args;
  args.reserve(13U);
  AppendReportTaskCommonArgs(ast, header, args_table_entry, input_addrs, output_addrs, workspace_addrs,
                             task_type, stream, args_table, use_args_info_size, args);
  args.emplace_back(model_id);
  args.emplace_back(instance_handle);
  if (is_raw_address) {
    args.emplace_back(ast.UInt(1U));
  }
  return ast.Call("ReportLaunchedOm2Task", args);
}

ExprRef TaskCodeBuilderUtil::BuildReportTaskPreprocessCall(
    AstBuildContext &ast, const TaskSemanticHeader &header, const ArgsTableEntrySemantic *args_table_entry,
    const std::vector<AddrSemantic> &input_addrs, const std::vector<AddrSemantic> &output_addrs,
    const std::vector<AddrSemantic> &workspace_addrs, ModelTaskType task_type, Arg stream,
    const VarRef &model_id, const VarRef &instance_handle, const VarRef &args_table, Arg l0_info,
    bool use_args_info_size, bool is_raw_address) {
  std::vector<Arg> args;
  args.reserve(14U);
  AppendReportTaskCommonArgs(ast, header, args_table_entry, input_addrs, output_addrs, workspace_addrs,
                             task_type, stream, args_table, use_args_info_size, args);
  args.emplace_back(l0_info);
  args.emplace_back(model_id);
  args.emplace_back(instance_handle);
  if (is_raw_address) {
    args.emplace_back(ast.UInt(1U));
  }
  return ast.Call("ReportOm2TaskPreprocess", args);
}
}  // namespace ge
