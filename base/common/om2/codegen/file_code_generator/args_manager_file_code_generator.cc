/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/file_code_generator/args_manager_file_code_generator.h"

namespace ge {
ArgsManagerFileCodeGenerator::ArgsManagerFileCodeGenerator(AstBuildContext &ast)
    : CodeGeneratorBase(ast),
      args_size_(ast_.Var("int64_t", "args_size_")),
      args_info_(ast_.Var("std::vector<ArgsInfo>", "args_info_")),
      host_args_(ast_.Var("std::vector<uint8_t>", "host_args_")),
      dev_args_(ast_.Var("void *", "dev_args_")),
      iow_args_addrs_(ast_.Var("std::vector<std::vector<void *>>", "iow_args_addrs_")) {}

MethodDef *ArgsManagerFileCodeGenerator::BuildInitMethod(const Om2CodegenModel &codegen_model) {
  std::vector<Arg> args_info_items;
  args_info_items.reserve(codegen_model.args_table.entries.size());
  for (const auto &entry : codegen_model.args_table.entries) {
    args_info_items.push_back({GetHostArgAddr(entry.host_offset), GetDevArgAddr(entry.host_offset), entry.args_size});
  }

  std::vector<Arg> refresh_group_items;
  refresh_group_items.reserve(codegen_model.args_table.host_args_offsets.size());
  for (const auto &group : codegen_model.args_table.host_args_offsets) {
    std::vector<Arg> host_offsets;
    host_offsets.reserve(group.size());
    for (const uint64_t host_offset : group) {
      (void)host_offsets.push_back(GetHostArgAddr(host_offset));
    }
    (void)refresh_group_items.push_back(host_offsets);
  }

  return ast_.DefineMethod("Om2ArgsTable", "Init", {}, "aclError",
                           {
                               ast_.Assign(args_size_, codegen_model.args_table.total_host_args_len),
                               host_args_.Clear(),
                               host_args_.Resize(args_size_),
                               ChkStatus(AclrtMalloc(dev_args_.Addr(), args_size_, "ACL_MEM_MALLOC_HUGE_FIRST")),
                               ast_.Assign(args_info_, args_info_items),
                               ast_.Assign(iow_args_addrs_, refresh_group_items),
                               ast_.Return("ACL_SUCCESS"),
                           });
}

MethodDef *ArgsManagerFileCodeGenerator::BuildDestructor() {
  return ast_.DefineMethod("Om2ArgsTable", "~Om2ArgsTable", {}, "", {});
}

MethodDef *ArgsManagerFileCodeGenerator::BuildGetArgsInfoMethod() {
  auto index = ast_.Var("size_t", "index");
  return ast_.DefineMethod("Om2ArgsTable", "GetArgsInfo", {index}, "ArgsInfo *",
                           {
                               ast_.If(index >= args_info_.Size(), {ast_.Return(nullptr)}),
                               ast_.Return(args_info_[index].Addr()),
                           });
}

MethodDef *ArgsManagerFileCodeGenerator::BuildGetDevArgAddrMethod() {
  auto offset = ast_.Var("size_t", "offset");
  return ast_.DefineMethod("Om2ArgsTable", "GetDevArgAddr", {offset}, "void *",
                           {
                               ast_.If(offset >= args_size_, {ast_.Return(nullptr)}),
                               ast_.Return(GetAddr(dev_args_, offset)),
                           });
}

MethodDef *ArgsManagerFileCodeGenerator::BuildGetHostArgAddrMethod() {
  auto offset = ast_.Var("size_t", "offset");
  return ast_.DefineMethod("Om2ArgsTable", "GetHostArgAddr", {offset}, "void *",
                           {
                               ast_.If(offset >= args_size_, {ast_.Return(nullptr)}),
                               ast_.Return(GetAddr(host_args_.Data(), offset)),
                           });
}

MethodDef *ArgsManagerFileCodeGenerator::BuildUpdateHostArgsMethod() {
  auto index = ast_.Var("size_t", "index");
  auto addr = ast_.Var("const uintptr_t", "addr");
  auto host_addr = ast_.Var("void *", "host_addr");
  return ast_.DefineMethod("Om2ArgsTable", "UpdateHostArgs", {index, addr}, "aclError",
                           {
                               ast_.If(index >= iow_args_addrs_.Size(), {ast_.Return("ACL_ERROR_FAILURE")}),
                               ast_.RangeFor(host_addr, iow_args_addrs_.At(index),
                                             {
                                                 BodyItem(ast_.Memcpy(host_addr, addr.Addr(), ast_.Sizeof(addr))),
                                             }),
                               ast_.Return("ACL_SUCCESS"),
                           });
}

MethodDef *ArgsManagerFileCodeGenerator::BuildCopyArgsToDeviceMethod() {
  return ast_.DefineMethod(
      "Om2ArgsTable", "CopyArgsToDevice", {}, "aclError",
      {
          ChkStatus(AclrtMemcpy(dev_args_, args_size_, host_args_.Data(), args_size_, "ACL_MEMCPY_HOST_TO_DEVICE")),
          ast_.Return("ACL_SUCCESS"),
      });
}

ExprRef ArgsManagerFileCodeGenerator::GetHostArgAddr(Arg offset) {
  return ast_.Call("GetHostArgAddr", {offset});
}

ExprRef ArgsManagerFileCodeGenerator::GetDevArgAddr(Arg offset) {
  return ast_.Call("GetDevArgAddr", {offset});
}
}  // namespace ge
