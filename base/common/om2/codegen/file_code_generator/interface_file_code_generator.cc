/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/file_code_generator/interface_file_code_generator.h"

namespace ge {
TypeAliasDecl *InterfaceFileCodeGenerator::BuildOm2ModelHandleAlias() {
  return ast_.TypeAlias("void *", "Om2ModelHandle");
}

StructDecl *InterfaceFileCodeGenerator::BuildBinDataInfoStruct() {
  return ast_.Struct("BinDataInfo", {
                                        ast_.Field("const void *", "data"),
                                        ast_.Field("size_t", "size"),
                                    });
}

StructDecl *InterfaceFileCodeGenerator::BuildAicpuParamHeadStruct() {
  return ast_.Struct("AicpuParamHead", {
                                           ast_.Field("uint32_t", "length"),
                                           ast_.Field("uint32_t", "ioAddrNum"),
                                           ast_.Field("uint32_t", "extInfoLength"),
                                           ast_.Field("uint64_t", "extInfoAddr"),
                                       });
}

StructDecl *InterfaceFileCodeGenerator::BuildAicpuSessionInfoStruct() {
  return ast_.Struct("AicpuSessionInfo", {
                                             ast_.Field("uint64_t", "sessionId"),
                                             ast_.Field("uint64_t", "kernelId"),
                                             ast_.Field("bool", "sessFlag"),
                                         });
}

StructDecl *InterfaceFileCodeGenerator::BuildTfAiCpuExInfoStruct() {
  return ast_.Struct("TfAiCpuExInfo", {
                                          ast_.Field("uint32_t", "fwkKernelType"),
                                          ast_.Field("uint32_t", "fwkOperateType"),
                                          ast_.Field("uint64_t", "sessionID"),
                                          ast_.Field("uint64_t", "stepIDAddr"),
                                          ast_.Field("uint64_t", "kernelID"),
                                          ast_.Field("uint64_t", "nodeDefLen"),
                                          ast_.Field("uint64_t", "nodeDefBuf"),
                                          ast_.Field("uint64_t", "funDefLibLen"),
                                          ast_.Field("uint64_t", "funDefLibBuf"),
                                          ast_.Field("uint64_t", "inputOutputLen"),
                                          ast_.Field("uint64_t", "inputOutputBuf"),
                                          ast_.Field("uint64_t", "workspaceBaseAddr"),
                                          ast_.Field("uint64_t", "inputOutputAddr"),
                                          ast_.Field("uint64_t", "extInfoLen"),
                                          ast_.Field("uint64_t", "extInfoAddr"),
                                      });
}

StructDecl *InterfaceFileCodeGenerator::BuildArgsInfoStruct() {
  return ast_.Struct("ArgsInfo", {
                                     ast_.Field("void *", "host_addr"),
                                     ast_.Field("void *", "dev_addr"),
                                     ast_.Field("size_t", "size"),
                                 });
}

ClassDecl *InterfaceFileCodeGenerator::BuildOm2ArgsTableClass() {
  std::vector<DeclNode *> items = {
      ast_.Public(),
      ast_.DeclareMethod("Om2ArgsTable", {}, "", " = default"),
      ast_.DeclareMethod("~Om2ArgsTable", {}, ""),
      ast_.DeclareMethod("Init", {}, "aclError"),
      ast_.DeclareMethod("GetArgsInfo", {ast_.Var("size_t", "index")}, "ArgsInfo *"),
      ast_.DeclareMethod("GetDevArgAddr", {ast_.Var("size_t", "offset")}, "void *"),
      ast_.DeclareMethod("GetHostArgAddr", {ast_.Var("size_t", "offset")}, "void *"),
      ast_.DeclareMethod("UpdateHostArgs", {ast_.Var("size_t", "index"), ast_.Var("const uintptr_t", "addr")},
                         "aclError"),
      ast_.DeclareMethod("CopyArgsToDevice", {}, "aclError"),
      ast_.Private(),
      ast_.Field("int64_t", "args_size_"),
      ast_.Field("std::vector<ArgsInfo>", "args_info_"),
      ast_.Field("std::vector<uint8_t>", "host_args_"),
      ast_.Field("void *", "dev_args_"),
      ast_.Field("std::vector<std::vector<void *>>", "iow_args_addrs_"),
  };
  return ast_.Class("Om2ArgsTable", items);
}

ClassDecl *InterfaceFileCodeGenerator::BuildOm2ModelClass(const Om2CodegenModel &codegen_model) {
  const auto &runtime = codegen_model.runtime;
  std::vector<DeclNode *> items = {
      ast_.Public(),
      ast_.DeclareMethod(
          "Om2Model",
          {ast_.Var("const char **", "bin_files"), ast_.Var("const void **", "bin_data"),
           ast_.Var("size_t *", "bin_size"), ast_.Var("size_t", "bin_num"), ast_.Var("void **", "constants"),
           ast_.Var("void *", "work_ptr"), ast_.Var("uint64_t *", "session_id"), ast_.Var("uint32_t", "model_id"),
           ast_.Var("void *", "instance_handle")},
          ""),
      ast_.DeclareMethod("~Om2Model", {}, ""),
      ast_.DeclareMethod("InitResources", {}, "aclError"),
      ast_.DeclareMethod("RegisterKernels", {}, "aclError"),
      ast_.DeclareMethod("Load", {}, "aclError"),
      ast_.DeclareMethod("GetRtModelHandle", {}, "aclmdlRI"),
      ast_.DeclareMethod(
          "Run",
          {ast_.Var("size_t", "input_count"), ast_.Var("void **", "input_data"), ast_.Var("size_t", "output_count"),
           ast_.Var("void **", "output_data"), ast_.Var("int32_t", "stream_sync_timeout")},
          "aclError"),
      ast_.DeclareMethod(
          "RunAsync",
          {ast_.Var("aclrtStream &", "exe_stream"), ast_.Var("size_t", "input_count"),
           ast_.Var("void **", "input_data"), ast_.Var("size_t", "output_count"), ast_.Var("void **", "output_data")},
          "aclError"),
      ast_.DeclareMethod("ReleaseResources", {}, "aclError"),
      ast_.Private(),
      ast_.Field("void **", "constants_"),
      ast_.Field("aclmdlRI", "model_handle_"),
  };
  DealParamForOm2ModelClass(items, runtime);
  items.push_back(ast_.Field("void *", "total_dev_mem_ptr_"));
  items.push_back(ast_.Field("bool", "is_stream_list_bind_"));
  items.push_back(ast_.Field("std::unordered_map<std::string, BinDataInfo>", "bin_info_map_"));
  items.push_back(ast_.Field("Om2ArgsTable", "args_table_"));
  items.push_back(ast_.Field("uint64_t *", "session_id_"));
  items.push_back(ast_.Field("uint32_t", "model_id_"));
  items.push_back(ast_.Field("void *", "instance_handle_"));
  items.push_back(ast_.Field("uint64_t", "kernel_id_"));
  items.push_back(ast_.Field("std::vector<void *>", "dev_ext_info_mem_ptrs_"));
  items.push_back(ast_.Field("std::map<uint32_t, void *>", "mem_event_id_mem_map_"));
  items.push_back(ast_.Field("void *", "overflow_addr_"));
  items.push_back(ast_.Field("std::vector<void *>", "dev_dynamic_mem_ptrs_"));
  items.push_back(ast_.Field("void *", "session_scope_mem_ptr_"));
  return ast_.Class("Om2Model", items);
}

void InterfaceFileCodeGenerator::DealParamForOm2ModelClass(std::vector<DeclNode *> &items,
                                                           const RuntimeResourceSemantic &runtime) {
  if (runtime.kernel_bin_num > 0U) {
    items.push_back(ast_.Field("std::vector<aclrtBinHandle>", "bin_handles_"));
  }
  items.push_back(ast_.Field("std::vector<aclrtFuncHandle>", "func_handles_"));
  items.push_back(ast_.Field("std::vector<aclrtStream>", "stream_list_"));
  items.push_back(ast_.Field("std::vector<aclrtNotify>", "notify_list_"));
  items.push_back(ast_.Field("std::vector<aclrtEvent>", "event_list_"));
  items.push_back(ast_.Field("std::vector<aclrtLabel>", "label_list_"));
  if (runtime.label_num > 0U) {
    items.push_back(ast_.Field("aclrtLabelList", "aclrt_label_list_"));
  }
  items.push_back(ast_.Field("std::vector<aclrtLabel>", "label_used_"));
  items.push_back(ast_.Field("std::map<uint32_t, aclrtLabelList>", "label_switch_label_list_"));
  items.push_back(ast_.Field("std::map<uint32_t, std::pair<void *, uint32_t>>", "label_goto_args_"));
  items.push_back(ast_.Field("std::map<uint32_t, aclrtLabelList>", "label_goto_ex_label_list_"));
  if (runtime.has_label_switch) {
    items.push_back(ast_.DeclareMethod(
        "CreateLabelListForLabelSwitch",
        {ast_.Var("uint32_t", "op_index"), ast_.Var("std::vector<uint32_t>", "label_list_indexs")}, "aclError"));
  }
  if (runtime.has_label_goto) {
    items.push_back(ast_.DeclareMethod("CreateLabelListForLabelGotoEx",
                                       {ast_.Var("uint32_t", "op_index"), ast_.Var("uint32_t", "label_index")},
                                       "aclError"));
    items.push_back(ast_.Field("std::vector<void *>", "label_goto_ex_index_values_"));
  }
}

std::vector<DeclNode *> InterfaceFileCodeGenerator::BuildExternalApiDecls() {
  return {
      ast_.DeclareFunction(
          "Om2ModelCreate",
          {ast_.Var("om2::Om2ModelHandle *", "model_handle"), ast_.Var("aclmdlRI *", "rt_model_handle"),
           ast_.Var("const char **", "bin_files"), ast_.Var("const void **", "bin_data"),
           ast_.Var("size_t *", "bin_size"), ast_.Var("int", "bin_num"), ast_.Var("void **", "constants"),
           ast_.Var("void *", "work_ptr"), ast_.Var("uint64_t *", "session_id"), ast_.Var("uint32_t", "model_id"),
           ast_.Var("void *", "instance_handle")},
          "aclError"),
      ast_.DeclareFunction("Om2ModelLoad", {ast_.Var("om2::Om2ModelHandle *", "model_handle")}, "aclError"),
      ast_.DeclareFunction("Om2ModelRunAsync",
                           {ast_.Var("om2::Om2ModelHandle *", "model_handle"), ast_.Var("aclrtStream", "stream"),
                            ast_.Var("int", "input_count"), ast_.Var("void **", "input_data"),
                            ast_.Var("int", "output_count"), ast_.Var("void **", "output_data")},
                           "aclError"),
      ast_.DeclareFunction("Om2ModelRun",
                           {ast_.Var("om2::Om2ModelHandle *", "model_handle"), ast_.Var("int", "input_count"),
                            ast_.Var("void **", "input_data"), ast_.Var("int", "output_count"),
                            ast_.Var("void **", "output_data"), ast_.Var("int32_t", "stream_sync_timeout")},
                           "aclError"),
      ast_.DeclareFunction("Om2ModelDestroy", {ast_.Var("om2::Om2ModelHandle *", "model_handle")}, "aclError"),
  };
}
}  // namespace ge
