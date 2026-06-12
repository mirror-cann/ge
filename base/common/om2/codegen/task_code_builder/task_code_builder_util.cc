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
    (void)entries.emplace_back(std::vector<Arg>{
        ast.Var("Om2Tensor", addr.symbol_hint).Addr(),
        std::to_string(addr.tensor_info->args_offset) + "U"});
  }
  return ast.InitList(entries);
}

Expr *TaskCodeBuilderUtil::BuildWorkspaceAddrs(AstBuildContext &ast, const std::vector<AddrSemantic> &addrs) {
  std::vector<Arg> entries;
  entries.reserve(addrs.size());
  for (const auto &addr : addrs) {
    (void)entries.emplace_back(ast.Call("PtrToU64", {ast.Var("auto", addr.symbol_hint)}));
  }
  return ast.InitList(entries);
}

Expr *TaskCodeBuilderUtil::BuildWorkspaceSizes(AstBuildContext &ast, const std::vector<AddrSemantic> &addrs) {
  std::vector<Arg> entries;
  entries.reserve(addrs.size());
  for (const auto &addr : addrs) {
    (void)entries.emplace_back(std::to_string(addr.byte_size) + "U");
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
    (void)entries.emplace_back(std::vector<Arg>{
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

Status TaskCodeBuilderUtil::AppendReportLaunchedTaskCall(AstBuildContext &ast, std::vector<BodyItem> &items,
                                                         const std::string &var_prefix,
                                                         const TaskSemanticHeader &header,
                                                         const ArgsTableEntrySemantic *args_table_entry,
                                                         const std::vector<AddrSemantic> &input_addrs,
                                                         const std::vector<AddrSemantic> &output_addrs,
                                                         const std::vector<AddrSemantic> &workspace_addrs,
                                                         ModelTaskType task_type, uint32_t block_dim, Arg stream,
                                                         const VarRef &model_id, const VarRef &instance_handle,
                                                         const VarRef &args_table, bool use_args_info_size,
                                                         bool is_raw_address) {
  std::vector<Arg> args;
  args.reserve(17U);
  (void)args.emplace_back(Arg::StringLiteral(header.op_name));
  (void)args.emplace_back(Arg::StringLiteral(header.op_type));
  (void)args.emplace_back(std::to_string(header.op_desc_id) + "U");
  if (args_table_entry != nullptr) {
    auto args_info = args_table.Attr("GetArgsInfo")(static_cast<int64_t>(args_table_entry->table_index));
    (void)args.emplace_back(ast.ReinterpretCast("uintptr_t", args_info.Arrow("dev_addr")));
    (void)args.emplace_back(use_args_info_size ? Arg(args_info.Arrow("size"))
                                         : Arg(std::to_string(args_table_entry->args_size) + "U"));
  } else {
    (void)args.emplace_back(ast.UInt(0U));
    (void)args.emplace_back("0U");
  }
  const std::string inputs_var = var_prefix + "_report_inputs";
  const std::string outputs_var = var_prefix + "_report_outputs";
  const std::string workspace_addrs_var = var_prefix + "_report_workspace_addrs";
  const std::string workspace_sizes_var = var_prefix + "_report_workspace_sizes";
  auto input_entries = TaskCodeBuilderUtil::BuildTaskIoEntries(ast, input_addrs);
  auto output_entries = TaskCodeBuilderUtil::BuildTaskIoEntries(ast, output_addrs);
  auto workspace_addr_entries = TaskCodeBuilderUtil::BuildWorkspaceAddrs(ast, workspace_addrs);
  auto workspace_size_entries = TaskCodeBuilderUtil::BuildWorkspaceSizes(ast, workspace_addrs);
  auto count_report_tensors = [](const std::vector<AddrSemantic> &addrs) {
    size_t count = 0U;
    for (const auto &addr : addrs) {
      if (addr.tensor_info.has_value()) {
        ++count;
      }
    }
    return count;
  };
  const auto input_num = count_report_tensors(input_addrs);
  const auto output_num = count_report_tensors(output_addrs);
  const auto workspace_num = workspace_addrs.size();
  if (input_num > 0U) {
    (void)items.emplace_back(ast.VarDecl("const Om2TaskIoEntry", inputs_var + "[]", input_entries));
    (void)args.emplace_back(inputs_var);
  } else {
    (void)args.emplace_back("nullptr");
  }
  (void)args.emplace_back(ast.ULong(input_num));
  if (output_num > 0U) {
    (void)items.emplace_back(ast.VarDecl("const Om2TaskIoEntry", outputs_var + "[]", output_entries));
    (void)args.emplace_back(outputs_var);
  } else {
    (void)args.emplace_back("nullptr");
  }
  (void)args.emplace_back(ast.UInt(static_cast<uint32_t>(output_num)));
  if (workspace_num > 0U) {
    (void)items.emplace_back(ast.VarDecl("const uint64_t", workspace_addrs_var + "[]", workspace_addr_entries));
    (void)items.emplace_back(ast.VarDecl("const uint64_t", workspace_sizes_var + "[]", workspace_size_entries));
    (void)args.emplace_back(workspace_addrs_var);
    (void)args.emplace_back(workspace_sizes_var);
  } else {
    (void)args.emplace_back("nullptr");
    (void)args.emplace_back("nullptr");
  }
  (void)args.emplace_back(ast.UInt(static_cast<uint32_t>(workspace_num)));
  (void)args.emplace_back(ast.UInt(static_cast<uint32_t>(task_type)));
  (void)args.emplace_back(ast.UInt(block_dim));
  (void)args.emplace_back(stream);
  (void)args.emplace_back(model_id);
  (void)args.emplace_back(instance_handle);
  if (is_raw_address) {
    (void)args.emplace_back(ast.UInt(1U));
  }
  (void)items.emplace_back(ast.Call("OM2_CHK_STATUS", {ast.Call("ReportLaunchedOm2Task", args)}));
  return SUCCESS;
}

ExprRef TaskCodeBuilderUtil::BuildReportTaskPreprocessCall(
    AstBuildContext &ast, const TaskSemanticHeader &header, const ArgsTableEntrySemantic *args_table_entry,
    const std::vector<AddrSemantic> &input_addrs, const std::vector<AddrSemantic> &output_addrs,
    const std::vector<AddrSemantic> &workspace_addrs, ModelTaskType task_type, uint32_t block_dim, Arg stream,
    const VarRef &model_id, const VarRef &instance_handle, const VarRef &args_table, Arg l0_info,
    bool use_args_info_size, bool is_raw_address) {
  std::vector<Arg> args;
  args.reserve(14U);
  (void)args.emplace_back(Arg::StringLiteral(header.op_name));
  (void)args.emplace_back(Arg::StringLiteral(header.op_type));
  (void)args.emplace_back(std::to_string(header.op_desc_id) + "U");
  if (args_table_entry != nullptr) {
    auto args_info = args_table.Attr("GetArgsInfo")(static_cast<int64_t>(args_table_entry->table_index));
    (void)args.emplace_back(ast.ReinterpretCast("uintptr_t", args_info.Arrow("dev_addr")));
    (void)args.emplace_back(use_args_info_size ? Arg(args_info.Arrow("size"))
                                         : Arg(std::to_string(args_table_entry->args_size) + "U"));
  } else {
    (void)args.emplace_back(ast.UInt(0U));
    (void)args.emplace_back("0U");
  }
  (void)args.emplace_back(TaskCodeBuilderUtil::BuildTaskIoEntries(ast, input_addrs));
  (void)args.emplace_back(TaskCodeBuilderUtil::BuildTaskIoEntries(ast, output_addrs));
  (void)args.emplace_back(TaskCodeBuilderUtil::BuildWorkspaceAddrs(ast, workspace_addrs));
  (void)args.emplace_back(TaskCodeBuilderUtil::BuildWorkspaceSizes(ast, workspace_addrs));
  (void)args.emplace_back(ast.UInt(static_cast<uint32_t>(task_type)));
  (void)args.emplace_back(ast.UInt(block_dim));
  (void)args.emplace_back(stream);
  (void)args.emplace_back(l0_info);
  (void)args.emplace_back(model_id);
  (void)args.emplace_back(instance_handle);
  if (is_raw_address) {
    (void)args.emplace_back(ast.UInt(1U));
  }
  return ast.Call("ReportOm2TaskPreprocess", args);
}
}  // namespace ge
