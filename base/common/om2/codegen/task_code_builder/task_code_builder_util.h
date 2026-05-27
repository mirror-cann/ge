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
  static ExprRef BuildReportLaunchedTaskCall(AstBuildContext &ast, const TaskSemanticHeader &header,
                                             const ArgsTableEntrySemantic *args_table_entry,
                                             const std::vector<AddrSemantic> &input_addrs,
                                             const std::vector<AddrSemantic> &output_addrs,
                                             const std::vector<AddrSemantic> &workspace_addrs,
                                             ModelTaskType task_type, Arg stream,
                                             const VarRef &model_id, const VarRef &instance_handle,
                                             const VarRef &args_table, bool use_args_info_size);
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_TASK_CODE_BUILDER_UTIL_H_
