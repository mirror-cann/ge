/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_TASK_CODE_BUILDER_UTIL_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_TASK_CODE_BUILDER_UTIL_H_

#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/om2_codegen_types.h"

namespace ge {
class TaskCodeBuilderUtil {
 public:
  static Expr *BuildTaskIoEntries(AstBuildContext &ast, const std::vector<AddrSemantic> &addrs);
  static Expr *BuildWorkspaceAddrs(AstBuildContext &ast, const std::vector<AddrSemantic> &addrs);
  static Expr *BuildWorkspaceSizes(AstBuildContext &ast, const std::vector<AddrSemantic> &addrs);
  static Expr *BuildL0ArgSlotEntries(AstBuildContext &ast, const std::vector<AddrSemantic> &ordered_args);
  static Status AppendReportLaunchedTaskCall(AstBuildContext &ast, std::vector<BodyItem> &items,
                                             const std::string &var_prefix, const TaskSemanticHeader &header,
                                             const ArgsTableEntrySemantic *args_table_entry,
                                             const std::vector<AddrSemantic> &input_addrs,
                                             const std::vector<AddrSemantic> &output_addrs,
                                             const std::vector<AddrSemantic> &workspace_addrs, ModelTaskType task_type,
                                             uint32_t block_dim, Arg stream, const VarRef &model_id,
                                             const VarRef &instance_handle, const VarRef &args_table,
                                             bool use_args_info_size, bool is_raw_address = false);
  // 将 case body 包装为独立的 dispatch 函数并添加到 items
  static Status RenderDispatchFunc(AstBuildContext &ast, const std::string &func_name,
                                   const std::vector<BodyItem> &body, std::vector<DeclNode *> &items);
  static ExprRef BuildReportTaskPreprocessCall(
      AstBuildContext &ast, const TaskSemanticHeader &header, const ArgsTableEntrySemantic *args_table_entry,
      const std::vector<AddrSemantic> &input_addrs, const std::vector<AddrSemantic> &output_addrs,
      const std::vector<AddrSemantic> &workspace_addrs, ModelTaskType task_type, uint32_t block_dim, Arg stream,
      const VarRef &model_id, const VarRef &instance_handle, const VarRef &args_table, Arg l0_info,
      bool use_args_info_size, bool is_raw_address = false);
  // 将 OpArgDesc 列表转换为 OpArgInfo 数组的 AST 表达式（表驱动优化）
  static Arg RenderOpArgDesc(AstBuildContext &ast, const std::vector<OpArgDesc> &args);
  static Arg BuildAddrField(AstBuildContext &ast, const OpArgDesc &a);
  static Arg BuildTensorDataField(AstBuildContext &ast, const OpArgDesc &a);
  static Arg BuildWorkspaceDataField(AstBuildContext &ast, const OpArgDesc &a);
  static Arg BuildCustomValueDataField(AstBuildContext &ast, const OpArgDesc &a);
  static Arg BuildTilingDataField(AstBuildContext &ast, const OpArgDesc &a);
  // 将 AddrSemantic 转换为 OpArgDesc（RAW_ADDR 类型）
  static OpArgDesc ConvertAddrDesc(const AddrSemantic &addr);
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_TASK_CODE_BUILDER_UTIL_H_
