/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "om2_codegen.h"
#include <cerrno>
#include <fstream>
#include <sys/stat.h>
#include "common/helper/om2/om2_utils.h"
#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/ast/ast_context.h"
#include "common/om2/codegen/om2_codegen_model_builder.h"
#include "common/om2/codegen/om2_codegen_utils.h"
#include "program_generator.h"
#include "om2_code_printer.h"

namespace ge {
namespace {
constexpr const char *kTmpDir = "/tmp";
constexpr const char *kOm2DumpDir = "/tmp/.tmp_om2_workspace";

bool IsDirExists(const std::string &path) {
  struct stat path_stat = {};
  return (stat(path.c_str(), &path_stat) == 0) && S_ISDIR(path_stat.st_mode);
}

void DumpGeneratedFiles(const Om2CodegenArtifacts &artifacts) {
  if (!IsDirExists(kTmpDir)) {
    GELOGW("[OM2] Skip dumping generated files because /tmp does not exist.");
    return;
  }
  if ((mkdir(kOm2DumpDir, S_IRWXU) != 0) && (errno != EEXIST)) {
    GELOGW("[OM2] Failed to create dump directory %s, errno: %d.", kOm2DumpDir, errno);
    return;
  }

  for (const auto &artifact : artifacts) {
    const std::string dump_path = std::string(kOm2DumpDir) + "/" + artifact.file_name;
    std::ofstream output(dump_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
      GELOGW("[OM2] Failed to open generated file dump path: %s.", dump_path.c_str());
      continue;
    }
    (void)output.write(artifact.data.data(), static_cast<std::streamsize>(artifact.data.size()));
    if (!output.good()) {
      GELOGW("[OM2] Failed to dump generated file: %s.", dump_path.c_str());
    }
  }
}
}  // namespace

Status Om2Codegen::Om2CodegenAndCompile(const ge::GeModelPtr &ge_model, Om2CodegenArtifacts &artifacts,
                                        Om2ConstMetas &const_metas) const {
  artifacts.clear();
  const_metas.clear();
  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  Om2CodegenModel codegen_model;
  std::vector<TaskCodeBuilderPtr> task_code_builders;
  GE_ASSERT_SUCCESS(Om2CodegenModelBuilder::CreateTaskCodeBuilders(ge_model, ast, task_code_builders, codegen_model));
  Om2CodegenModelBuilder builder;
  GE_ASSERT_SUCCESS(builder.Build(ge_model, task_code_builders, codegen_model, const_metas));
  ProgramGenerator generator(ast, task_code_builders, codegen_model);

  Om2CodePrinter code_printer(ge_model->GetName());
  GE_ASSERT_SUCCESS(generator.GenerateProgram(code_printer));
  Om2CodegenArtifacts source_artifacts;
  code_printer.GetOutputFiles(source_artifacts);

  Om2CodegenArtifact so_artifact;
  so_artifact.file_name = "lib" + ge_model->GetName() + "_om2.so";
  const Status compile_ret =
      Om2Utils::CompileGeneratedCppToSo(source_artifacts, ge_model->GetName(), so_artifact, false);
  if (compile_ret != SUCCESS) {
    DumpGeneratedFiles(source_artifacts);
    return compile_ret;
  }
  GELOGI("[OM2] Model %s has finished generating source code files and compiling to the shared library.",
         ge_model->GetName().c_str());
  artifacts = std::move(source_artifacts);
  artifacts.push_back(std::move(so_artifact));
  return SUCCESS;
}
}  // namespace ge
