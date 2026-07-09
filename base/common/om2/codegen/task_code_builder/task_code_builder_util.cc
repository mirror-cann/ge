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

#include <iomanip>
#include <sstream>

namespace ge {

Status TaskCodeBuilderUtil::RenderDispatchFunc(AstBuildContext &ast, const std::string &func_name,
                                               const std::vector<BodyItem> &body, std::vector<DeclNode *> &items) {
  std::vector<BodyItem> func_body = body;
  func_body.push_back(ast.Return("ACL_SUCCESS"));
  auto op = ast.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast.Var("const DispatchOpContext &", "ctx");
  items.push_back(ast.DefineFunction(func_name, {op, ctx}, "aclError", ast.Body(func_body)));
  return SUCCESS;
}

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

const char *OpArgTypeName(int32_t type) {
  switch (type) {
    case OP_ARG_INPUT:
      return "OP_ARG_INPUT";
    case OP_ARG_OUTPUT:
      return "OP_ARG_OUTPUT";
    case OP_ARG_WORKSPACE:
      return "OP_ARG_WORKSPACE";
    case OP_ARG_CONST_TENSOR:
      return "OP_ARG_CONST_TENSOR";
    case OP_ARG_LEVEL1_DESC:
      return "OP_ARG_LEVEL1_DESC";
    case OP_ARG_SHAPE_INFO:
      return "OP_ARG_SHAPE_INFO";
    case OP_ARG_CUSTOM_VALUE:
      return "OP_ARG_CUSTOM_VALUE";
    case OP_ARG_PLACEHOLDER:
      return "OP_ARG_PLACEHOLDER";
    case OP_ARG_OPTIONAL_EMPTY:
      return "OP_ARG_OPTIONAL_EMPTY";
    case OP_ARG_FFTS_ADDR:
      return "OP_ARG_FFTS_ADDR";
    case OP_ARG_EVENT_ADDR:
      return "OP_ARG_EVENT_ADDR";
    case OP_ARG_OVERFLOW_ADDR:
      return "OP_ARG_OVERFLOW_ADDR";
    case OP_ARG_TILING:
      return "OP_ARG_TILING";
    case OP_ARG_RAW_ADDR:
      return "OP_ARG_RAW_ADDR";
    default:
      REPORT_INNER_ERR_MSG("E19999", "Unknown OpArgType %d, fallback to OP_ARG_INPUT.", type);
      GELOGE(FAILED, "Unknown OpArgType %d, fallback to OP_ARG_INPUT.", type);
      return "OP_ARG_INPUT";
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
    (void)entries.emplace_back(std::vector<Arg>{ast.Var("Om2Tensor", addr.symbol_hint).Addr(),
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

Expr *TaskCodeBuilderUtil::BuildL0ArgSlotEntries(AstBuildContext &ast, const std::vector<AddrSemantic> &ordered_args) {
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
        ToOm2L0ArgKind(addr.kind), "0U", std::to_string(args_offset) + "U", std::to_string(value) + "UL", related_index,
        std::to_string(addr.event_id) + "U", std::to_string(addr.level1_target_offset.value_or(0U)) + "U"});
    args_offset += GetOrderedArgByteSizeForL0(addr);
  }
  return ast.InitList(entries);
}

Status TaskCodeBuilderUtil::AppendReportLaunchedTaskCall(
    AstBuildContext &ast, std::vector<BodyItem> &items, const std::string &var_prefix, const TaskSemanticHeader &header,
    const ArgsTableEntrySemantic *args_table_entry, const std::vector<AddrSemantic> &input_addrs,
    const std::vector<AddrSemantic> &output_addrs, const std::vector<AddrSemantic> &workspace_addrs,
    ModelTaskType task_type, uint32_t block_dim, Arg stream, const VarRef &model_id, const VarRef &instance_handle,
    const VarRef &args_table, bool use_args_info_size, bool is_raw_address) {
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
Arg TaskCodeBuilderUtil::BuildAddrField(AstBuildContext &ast, const OpArgDesc &a) {
  return ast.DesignatedInit(
      std::vector<std::pair<std::string, Arg>>{
          {"mem_src", a.mem_src},
          {"offset", static_cast<int64_t>(a.offset)},
      },
      true);
}

Arg TaskCodeBuilderUtil::BuildTensorDataField(AstBuildContext &ast, const OpArgDesc &a) {
  if (!a.has_tensor_info) {
    return Arg();
  }
  std::vector<int64_t> padded_shape(kShapeMaxDims, 0);
  for (size_t i = 0; i < a.shape_dims.size() && i < kShapeMaxDims; ++i) {
    padded_shape[i] = a.shape_dims[i];
  }
  return ast.DesignatedInit(
      std::vector<std::pair<std::string, Arg>>{
          {"tensor", ast.DesignatedInit(
                         std::vector<std::pair<std::string, Arg>>{
                             {"size", static_cast<int64_t>(a.size)},
                             {"data_type", a.data_type},
                             {"format", a.format},
                             {"shape", ast.InitList(std::vector<Arg>(padded_shape.begin(), padded_shape.end()))},
                             {"shape_dims", static_cast<int64_t>(a.shape_dims.size())},
                             {"args_offset", ast.UInt(a.args_offset)},
                         },
                         true)},
      },
      true);
}

Arg TaskCodeBuilderUtil::BuildWorkspaceDataField(AstBuildContext &ast, const OpArgDesc &a) {
  return ast.DesignatedInit(
      std::vector<std::pair<std::string, Arg>>{
          {"tensor", ast.DesignatedInit(
                         std::vector<std::pair<std::string, Arg>>{
                             {"size", static_cast<int64_t>(a.size)},
                         },
                         true)},
      },
      true);
}

Arg TaskCodeBuilderUtil::BuildCustomValueDataField(AstBuildContext &ast, const OpArgDesc &a) {
  return ast.DesignatedInit(
      std::vector<std::pair<std::string, Arg>>{
          {"custom_value", static_cast<int64_t>(a.custom_value)},
      },
      true);
}

Arg TaskCodeBuilderUtil::BuildTilingDataField(AstBuildContext &ast, const OpArgDesc &a) {
  Arg raw_data_arg(nullptr);
  uint32_t raw_data_len = 0U;
  if (!a.raw_data.empty()) {
    std::ostringstream oss;
    for (const auto byte : a.raw_data) {
      oss << "\\" << std::oct << std::setw(kWidthPerChar) << std::setfill('0') << static_cast<int>(byte);
    }
    raw_data_arg = ast.ReinterpretCast("const uint8_t *", Arg::StringLiteral(oss.str()));
    raw_data_len = static_cast<uint32_t>(a.raw_data.size());
  }
  return ast.DesignatedInit(
      std::vector<std::pair<std::string, Arg>>{
          {"tiling", ast.DesignatedInit(
                         std::vector<std::pair<std::string, Arg>>{
                             {"raw_data", raw_data_arg},
                             {"raw_data_len", static_cast<int64_t>(raw_data_len)},
                         },
                         true)},
      },
      true);
}

Arg TaskCodeBuilderUtil::RenderOpArgDesc(AstBuildContext &ast, const std::vector<OpArgDesc> &args) {
  if (args.empty()) {
    return Arg(nullptr);
  }

  using DataBuilder = Arg (*)(AstBuildContext &, const OpArgDesc &);
  static const std::unordered_map<int32_t, DataBuilder> kDataBuilders = {
      {OP_ARG_INPUT, &TaskCodeBuilderUtil::BuildTensorDataField},
      {OP_ARG_OUTPUT, &TaskCodeBuilderUtil::BuildTensorDataField},
      {OP_ARG_WORKSPACE, &TaskCodeBuilderUtil::BuildWorkspaceDataField},
      {OP_ARG_CONST_TENSOR, &TaskCodeBuilderUtil::BuildTensorDataField},
      {OP_ARG_LEVEL1_DESC, &TaskCodeBuilderUtil::BuildCustomValueDataField},
      {OP_ARG_SHAPE_INFO, &TaskCodeBuilderUtil::BuildCustomValueDataField},
      {OP_ARG_CUSTOM_VALUE, &TaskCodeBuilderUtil::BuildCustomValueDataField},
      {OP_ARG_EVENT_ADDR, &TaskCodeBuilderUtil::BuildCustomValueDataField},
      {OP_ARG_TILING, &TaskCodeBuilderUtil::BuildTilingDataField},
  };

  std::vector<Arg> arg_entries;
  arg_entries.reserve(args.size());
  for (const auto &a : args) {
    std::vector<std::pair<std::string, Arg>> fields;
    fields.push_back({"type", OpArgTypeName(a.type)});

    const bool needs_addr = (a.type == OP_ARG_INPUT || a.type == OP_ARG_OUTPUT || a.type == OP_ARG_WORKSPACE ||
                             a.type == OP_ARG_CONST_TENSOR);
    if (needs_addr) {
      fields.push_back({"addr", BuildAddrField(ast, a)});
    }

    const auto it = kDataBuilders.find(a.type);
    if (it != kDataBuilders.end()) {
      auto data_arg = it->second(ast, a);
      if (!data_arg.Empty()) {
        fields.push_back({"data", std::move(data_arg)});
      }
    }

    arg_entries.push_back(ast.DesignatedInit(fields, true));
  }
  return ast.CCast("const OpArgInfo[]", ast.InitList(arg_entries, true));
}

OpArgDesc TaskCodeBuilderUtil::ConvertAddrDesc(const AddrSemantic &addr) {
  OpArgDesc arg;
  if (addr.memory_type == (kSessionScopeMemoryMask | RT_MEMORY_HBM)) {
    arg.mem_src = 0xFFFFFFFFU;
  } else if (addr.const_index.has_value()) {
    arg.mem_src = static_cast<uint32_t>(*addr.const_index + 1);
  }
  arg.offset = static_cast<uint64_t>(addr.mem_offset);

  static const std::unordered_map<AddrValueKind, OpArgType> kTypeMap = {
      {AddrValueKind::kInputInstance, OP_ARG_INPUT},
      {AddrValueKind::kOutputInstance, OP_ARG_OUTPUT},
      {AddrValueKind::kWorkspace, OP_ARG_WORKSPACE},
      {AddrValueKind::kConstTensor, OP_ARG_CONST_TENSOR},
      {AddrValueKind::kLevel1DescPtr, OP_ARG_LEVEL1_DESC},
      {AddrValueKind::kCustomValue, OP_ARG_CUSTOM_VALUE},
      {AddrValueKind::kPlaceholder, OP_ARG_PLACEHOLDER},
      {AddrValueKind::kOptionalEmpty, OP_ARG_OPTIONAL_EMPTY},
      {AddrValueKind::kEmptyAddr, OP_ARG_OPTIONAL_EMPTY},
      {AddrValueKind::kFftsAddr, OP_ARG_FFTS_ADDR},
      {AddrValueKind::kEventAddr, OP_ARG_EVENT_ADDR},
      {AddrValueKind::kOverflowAddr, OP_ARG_OVERFLOW_ADDR},
      {AddrValueKind::kTiling, OP_ARG_TILING},
  };
  auto it = kTypeMap.find(addr.kind);
  arg.type = (it != kTypeMap.end()) ? it->second : OP_ARG_RAW_ADDR;

  if (addr.tensor_info.has_value()) {
    arg.has_tensor_info = true;
    arg.size = addr.tensor_info->size;
    arg.data_type = addr.tensor_info->data_type;
    arg.format = addr.tensor_info->format;
    arg.shape_dims = addr.tensor_info->shape_dims;
  } else if (arg.type == OP_ARG_WORKSPACE) {
    arg.size = addr.byte_size;
  }
  // custom_value
  if (addr.kind == AddrValueKind::kLevel1DescPtr) {
    arg.custom_value = addr.level1_target_offset.value_or(0U);
  } else if (addr.kind == AddrValueKind::kCustomValue) {
    arg.custom_value = addr.custom_value;
  } else if (addr.kind == AddrValueKind::kEventAddr) {
    arg.custom_value = addr.event_id;
  }
  return arg;
}

}  // namespace ge
