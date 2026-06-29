/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_PROGRAM_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_PROGRAM_GENERATOR_H_

#include <set>
#include <string>
#include <unordered_set>
#include <vector>
#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_codegen_types.h"
#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/om2_code_printer.h"
#include "framework/common/ge_inner_error_codes.h"

namespace ge {
#define EMIT_CODE(ss, code) ((ss) << (code) << '\n')

class ProgramGenerator {
 public:
  ProgramGenerator(AstBuildContext &ast, const std::vector<TaskCodeBuilderPtr> &task_code_builders,
                   const Om2CodegenModel &codegen_model)
      : ast_(ast), task_code_builder_list_(task_code_builders), codegen_model_(codegen_model) {}
  Status GenerateProgram(Om2CodePrinter &code_printer);

 private:
  Status GenerateInterfaceHeader(Om2CodePrinter &code_printer);
  std::vector<DeclNode *> BuildInterfaceHeaderIncludes() const;
  Status GenerateResourcesSource(Om2CodePrinter &code_printer);
  Status GenerateKernelRegSource(Om2CodePrinter &code_printer);
  Status GenerateArgsManagerSource(Om2CodePrinter &code_printer);
  Status GenerateLoadAndRunSource(Om2CodePrinter &code_printer);
  Status GenerateMakeFile(Om2CodePrinter &code_printer);

  AstBuildContext &ast_;
  std::vector<TaskCodeBuilderPtr> task_code_builder_list_;
  uint64_t args_table_index_ = 0U;
  Om2CodegenModel codegen_model_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_PROGRAM_GENERATOR_H_
