/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_LOAD_AND_RUN_FILE_CODE_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_LOAD_AND_RUN_FILE_CODE_GENERATOR_H_

#include "common/om2/codegen/om2_model_class_generator_base.h"
#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {
class LoadAndRunFileCodeGenerator : public Om2ModelClassGeneratorBase {
 public:
  explicit LoadAndRunFileCodeGenerator(AstBuildContext &ast);
  ~LoadAndRunFileCodeGenerator() override = default;

  std::vector<DeclNode *> BuildAnonymousNamespaceItems(const Om2CodegenModel &codegen_model,
                                                       const std::vector<TaskCodeBuilderPtr> &task_code_builders) const;
  MethodDef *BuildLoadMethod(const Om2CodegenModel &codegen_model,
                             const std::vector<TaskCodeBuilderPtr> &task_code_builders);
  MethodDef *BuildGetRtModelHandleMethod();
  MethodDef *BuildRunAsyncMethod(const Om2CodegenModel &codegen_model);
  MethodDef *BuildRunMethod(const Om2CodegenModel &codegen_model);

 private:
  Status BuildLoadBody(std::vector<BodyItem> &body, const Om2CodegenModel &codegen_model,
                       const std::vector<TaskCodeBuilderPtr> &task_code_builders);
  Status BuildRunBodyImpl(std::vector<BodyItem> &body, const Om2CodegenModel &codegen_model, bool is_async);
  Status BuildCommonHelperFunctions(std::vector<DeclNode *> &items);
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_LOAD_AND_RUN_FILE_CODE_GENERATOR_H_
