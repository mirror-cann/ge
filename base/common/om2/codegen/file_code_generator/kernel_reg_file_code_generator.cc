/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/file_code_generator/kernel_reg_file_code_generator.h"

namespace ge {

KernelRegFileCodeGenerator::KernelRegFileCodeGenerator(AstBuildContext &ast) : Om2ModelClassGeneratorBase(ast) {}

StructDecl *KernelRegFileCodeGenerator::BuildBinaryBufferStruct() {
  return ast_.Struct("BinaryBuffer", {
      ast_.Field("std::unique_ptr<uint8_t[]>", "data"),
      ast_.Field("size_t", "size", 0),
  });
}

StructDecl *KernelRegFileCodeGenerator::BuildAicoreRegisterInfoStruct() {
  return ast_.Struct("AicoreRegisterInfo", {
      ast_.Field("uint32_t", "magic"),
      ast_.Field("const char *", "kernel_name"),
      ast_.Field("std::string", "file"),
  });
}

StructDecl *KernelRegFileCodeGenerator::BuildAicpuRegisterInfoStruct() {
  return ast_.Struct("AicpuRegisterInfo", {
      ast_.Field("const char *", "op_type"),
      ast_.Field("const char *", "so_name"),
      ast_.Field("const char *", "kernel_name"),
      ast_.Field("const char *", "op_kernel_lib"),
  });
}

StructDecl *KernelRegFileCodeGenerator::BuildCustAicpuRegisterInfoStruct() {
  return ast_.Struct("CustAicpuRegisterInfo", {
      ast_.Field("const char *", "kernel_name"),
      ast_.Field("const char *", "func_name"),
      ast_.Field("const char *", "kernel_file"),
  });
}

FunctionDef *KernelRegFileCodeGenerator::BuildAssembleAicpuLoadOptions() {
  auto load_options = ast_.Var("aclrtBinaryLoadOptions &", "load_options");
  auto cpu_kernel_mode = ast_.Var("int32_t", "cpu_kernel_mode");
  auto option = ast_.Var("aclrtBinaryLoadOption", "option");
  return ast_.DefineFunction("AssembleAicpuLoadOptions", {load_options, cpu_kernel_mode}, "void", {
      ast_.VarDecl(option),
      ast_.Assign(load_options.Attr("numOpt"), 1),
      ast_.Assign(load_options.Attr("options"), option.Addr()),
      ast_.Assign(option.Attr("type"), "ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE"),
      ast_.Assign(option.Attr("value").Attr("cpuKernelMode"), cpu_kernel_mode),
  });
}

FunctionDef *KernelRegFileCodeGenerator::BuildRegisterAicoreKernel() {
  auto bin_handle = ast_.Var("aclrtBinHandle &", "bin_handle");
  auto func_handle = ast_.Var("aclrtFuncHandle &", "func_handle");
  auto register_info = ast_.Var("const AicoreRegisterInfo &", "register_info");
  auto bin_info_map = ast_.Var("std::unordered_map<std::string, BinDataInfo> &", "bin_info_map");
  auto bin_info = ast_.Var("auto &", "bin_info");
  auto load_options = ast_.Var("aclrtBinaryLoadOptions", "load_options");
  auto option = ast_.Var("aclrtBinaryLoadOption", "option");
  return ast_.DefineFunction("RegisterAicoreKernel", {bin_handle, func_handle, register_info, bin_info_map}, "aclError", {
      ast_.VarDecl(bin_info, bin_info_map[register_info.Attr("file")]),
      ast_.VarDecl(load_options),
      ast_.VarDecl(option),
      ast_.Assign(load_options.Attr("numOpt"), 1),
      ast_.Assign(load_options.Attr("options"), option.Addr()),
      ast_.Assign(option.Attr("type"), "ACL_RT_BINARY_LOAD_OPT_MAGIC"),
      ast_.Assign(option.Attr("value").Attr("magic"), register_info.Attr("magic")),
      ChkStatus(AclrtBinaryLoadFromData(bin_info.Attr("data"), bin_info.Attr("size"), load_options.Addr(),
                                       bin_handle.Addr())),
      ChkStatus(AclrtBinaryGetFunction(bin_handle, register_info.Attr("kernel_name"), func_handle.Addr())),
      ast_.Return("ACL_SUCCESS"),
  });
}

FunctionDef *KernelRegFileCodeGenerator::BuildRegisterAicpuKernel() {
  auto bin_handle = ast_.Var("aclrtBinHandle &", "bin_handle");
  auto func_handle = ast_.Var("aclrtFuncHandle &", "func_handle");
  auto register_info = ast_.Var("const AicpuRegisterInfo &", "register_info");
  auto load_options = ast_.Var("aclrtBinaryLoadOptions", "load_options");
  auto option = ast_.Var("aclrtBinaryLoadOption", "option");
  auto json_path = ast_.Var("std::string", "json_path");
  auto cleanup_guard = ast_.Lambda({ast_.CaptureRef(json_path)},
                                   {ast_.IgnoreOutput(ast_.RemoveFile(json_path.CStr()))});
  return ast_.DefineFunction("RegisterAicpuKernel", {bin_handle, func_handle, register_info}, "aclError", {
      ast_.VarDecl(json_path),
      ChkStatus(GenerateJsonFile(register_info, json_path)),
      MakeGuard("json_guard", cleanup_guard),
      ChkTrue(!json_path.Empty()),
      ast_.VarDecl(load_options),
      ast_.VarDecl(option),
      ast_.Assign(load_options.Attr("numOpt"), 1),
      ast_.Assign(load_options.Attr("options"), option.Addr()),
      ast_.Assign(option.Attr("type"), "ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE"),
      ast_.Assign(option.Attr("value").Attr("cpuKernelMode"), 0),
      ChkStatus(AclrtBinaryLoadFromFile(json_path.CStr(), load_options.Addr(), bin_handle.Addr())),
      ChkStatus(AclrtBinaryGetFunction(bin_handle, register_info.Attr("op_type"), func_handle.Addr())),
      ast_.Return("ACL_SUCCESS"),
  });
}

FunctionDef *KernelRegFileCodeGenerator::BuildRegisterCustAicpuKernel() {
  auto bin_handle = ast_.Var("aclrtBinHandle &", "bin_handle");
  auto func_handle = ast_.Var("aclrtFuncHandle &", "func_handle");
  auto register_info = ast_.Var("const CustAicpuRegisterInfo &", "register_info");
  auto load_options = ast_.Var("aclrtBinaryLoadOptions", "load_options");
  auto kernel_buf = ast_.Var("const auto &", "kernel_buf");
  return ast_.DefineFunction("RegisterCustAicpuKernel", {bin_handle, func_handle, register_info}, "aclError", {
      ast_.VarDecl(kernel_buf, ReadBinaryFileToBuffer(register_info.Attr("kernel_file"))),
      ChkTrue((kernel_buf.Attr("size") > 0) && (kernel_buf.Attr("data") != nullptr)),
      ast_.VarDecl(load_options),
      AssembleAicpuLoadOptionsCall(load_options, 2),
      ChkStatus(AclrtBinaryLoadFromData(kernel_buf.Attr("data").GetPtr(), kernel_buf.Attr("size"),
                                       load_options.Addr(), bin_handle.Addr())),
      ChkStatus(AclrtRegisterCpuFunc(bin_handle, register_info.Attr("func_name"),
                                     register_info.Attr("kernel_name"), func_handle.Addr())),
      ast_.Return("ACL_SUCCESS"),
  });
}

MethodDef *KernelRegFileCodeGenerator::BuildRegisterKernels(const Om2CodegenModel &codegen_model) {
  std::vector<BodyItem> items;
  for (const auto &binary : codegen_model.kernel_registry.binaries) {
    if (binary.kind == KernelBinaryKind::kAicore) {
      items.emplace_back(ChkStatus(CallRegisterAicoreKernel(
          bin_handles_[binary.func_handle_index],
          func_handles_[binary.func_handle_index],
          {binary.magic, ast_.Str(binary.kernel_name), ast_.Str(binary.file_name)}, bin_info_map_)));
      continue;
    }
    if (binary.kind == KernelBinaryKind::kAicpu) {
      items.emplace_back(ChkStatus(CallRegisterAicpuKernel(
          bin_handles_[binary.func_handle_index],
          func_handles_[binary.func_handle_index],
          {ast_.Str(binary.op_type), ast_.Str(binary.so_name), ast_.Str(binary.kernel_name),
           ast_.Str(binary.op_kernel_lib)})));
      continue;
    }
    items.emplace_back(ChkStatus(CallRegisterCustAicpuKernel(
        bin_handles_[binary.func_handle_index],
        func_handles_[binary.func_handle_index],
        {ast_.Str(binary.kernel_name), ast_.Str(binary.kernel_name), ast_.Str(binary.file_name)})));
  }
  items.emplace_back(ast_.Return("ACL_SUCCESS"));
  return ast_.DefineMethod("Om2Model", "RegisterKernels", std::vector<VarRef>{}, "aclError", items);
}

ExprRef KernelRegFileCodeGenerator::GenerateJsonFile(Arg register_info, Arg json_path) {
  return ast_.Call("GenerateJsonFile", {register_info, json_path});
}

ExprRef KernelRegFileCodeGenerator::ReadBinaryFileToBuffer(Arg file_path) {
  return ast_.Call("ReadBinaryFileToBuffer", {file_path});
}

ExprRef KernelRegFileCodeGenerator::AssembleAicpuLoadOptionsCall(Arg load_options, Arg cpu_kernel_mode) {
  return ast_.Call("AssembleAicpuLoadOptions", {load_options, cpu_kernel_mode});
}

ExprRef KernelRegFileCodeGenerator::CallRegisterAicoreKernel(Arg bin_handle, Arg func_handle, Arg register_info,
                                                       Arg bin_info_map) {
  return ast_.Call("RegisterAicoreKernel", {bin_handle, func_handle, register_info, bin_info_map});
}

ExprRef KernelRegFileCodeGenerator::CallRegisterAicpuKernel(Arg bin_handle, Arg func_handle, Arg register_info) {
  return ast_.Call("RegisterAicpuKernel", {bin_handle, func_handle, register_info});
}

ExprRef KernelRegFileCodeGenerator::CallRegisterCustAicpuKernel(Arg bin_handle, Arg func_handle, Arg register_info) {
  return ast_.Call("RegisterCustAicpuKernel", {bin_handle, func_handle, register_info});
}
}  // namespace ge
