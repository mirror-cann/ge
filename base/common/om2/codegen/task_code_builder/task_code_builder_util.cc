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
  args.emplace_back(BuildTaskIoEntries(ast, input_addrs));
  args.emplace_back(BuildTaskIoEntries(ast, output_addrs));
  args.emplace_back(BuildWorkspaceAddrs(ast, workspace_addrs));
  args.emplace_back(BuildWorkspaceSizes(ast, workspace_addrs));
  args.emplace_back(ast.UInt(static_cast<uint32_t>(task_type)));
  args.emplace_back(stream);
  args.emplace_back(model_id);
  args.emplace_back(instance_handle);
  if (is_raw_address) {
    args.emplace_back(ast.UInt(1U));
  }
  return ast.Call("ReportLaunchedOm2Task", args);
}
}  // namespace ge
