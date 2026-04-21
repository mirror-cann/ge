/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_RESOURCES_FILE_CODE_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_RESOURCES_FILE_CODE_GENERATOR_H_

#include "common/om2/codegen/om2_model_class_generator_base.h"
#include "common/om2/codegen/task_code_builder_factory.h"

namespace ge {
class ResourcesFileCodeGenerator : public Om2ModelClassGeneratorBase {
 public:
  explicit ResourcesFileCodeGenerator(AstBuildContext &ast);
  ~ResourcesFileCodeGenerator() override = default;

  MethodDef *BuildOm2ModelConstructor(const Om2CodegenModel &codegen_model);
  MethodDef *BuildOm2ModelDestructor() const;
  MethodDef *BuildInitResourcesMethod(const Om2CodegenModel &codegen_model,
                                      const std::vector<TaskCodeBuilderPtr> &task_code_builders);
  MethodDef *BuildReleaseResourcesMethod(const Om2CodegenModel &codegen_model);

private:
  void BuildReleaseResourcesMethodForControlTask(std::vector<BodyItem> &body, const RuntimeResourceSemantic &runtime);
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_RESOURCES_FILE_CODE_GENERATOR_H_
