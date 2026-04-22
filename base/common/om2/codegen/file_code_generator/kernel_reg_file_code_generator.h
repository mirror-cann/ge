/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_KERNEL_REG_FILE_CODE_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_KERNEL_REG_FILE_CODE_GENERATOR_H_

#include "common/om2/codegen/om2_model_class_generator_base.h"

namespace ge {
class KernelRegFileCodeGenerator : public Om2ModelClassGeneratorBase {
 public:
  explicit KernelRegFileCodeGenerator(AstBuildContext &ast);
  ~KernelRegFileCodeGenerator() override = default;

  StructDecl *BuildBinaryBufferStruct() const;
  StructDecl *BuildAicoreRegisterInfoStruct() const;
  StructDecl *BuildAicpuRegisterInfoStruct() const;
  StructDecl *BuildCustAicpuRegisterInfoStruct() const;
  FunctionDef *BuildAssembleAicpuLoadOptions() const;
  FunctionDef *BuildRegisterAicoreKernel() const;
  FunctionDef *BuildRegisterAicpuKernel() const;
  FunctionDef *BuildRegisterCustAicpuKernel() const;
  MethodDef *BuildRegisterKernels(const Om2CodegenModel &codegen_model);

 private:
  ExprRef GenerateJsonFile(Arg register_info, Arg json_path) const;
  ExprRef ReadBinaryFileToBuffer(Arg file_path) const;
  ExprRef AssembleAicpuLoadOptionsCall(Arg load_options, Arg cpu_kernel_mode) const;
  ExprRef CallRegisterAicoreKernel(Arg bin_handle, Arg func_handle, Arg register_info, Arg bin_info_map) const;
  ExprRef CallRegisterAicpuKernel(Arg bin_handle, Arg func_handle, Arg register_info) const;
  ExprRef CallRegisterCustAicpuKernel(Arg bin_handle, Arg func_handle, Arg register_info) const;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_FILE_CODE_GENERATOR_KERNEL_REG_FILE_CODE_GENERATOR_H_
