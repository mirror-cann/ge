/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_INTERFACE_FILE_CODE_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_INTERFACE_FILE_CODE_GENERATOR_H_

#include "common/om2/codegen/code_generator_base.h"

namespace ge {
class InterfaceFileCodeGenerator : public CodeGeneratorBase {
 public:
  using CodeGeneratorBase::CodeGeneratorBase;
  ~InterfaceFileCodeGenerator() override = default;

  TypeAliasDecl *BuildOm2ModelHandleAlias();
  StructDecl *BuildBinDataInfoStruct();
  StructDecl *BuildAicpuParamHeadStruct();
  StructDecl *BuildAicpuSessionInfoStruct();
  StructDecl *BuildTfAiCpuExInfoStruct();
  StructDecl *BuildArgsInfoStruct();
  ClassDecl *BuildOm2ArgsTableClass();
  ClassDecl *BuildOm2ModelClass(const Om2CodegenModel &codegen_model);
  std::vector<DeclNode *> BuildExternalApiDecls();
  std::vector<DeclNode *> BuildRtForwardDecls();

 private:
  void DealParamForOm2ModelClass(std::vector<DeclNode *> &items, const RuntimeResourceSemantic &runtime);
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_INTERFACE_FILE_CODE_GENERATOR_H_
