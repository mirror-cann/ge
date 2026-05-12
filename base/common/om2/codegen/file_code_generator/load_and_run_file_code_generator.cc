/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/file_code_generator/load_and_run_file_code_generator.h"

#include <typeindex>
#include <unordered_set>

#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "graph/debug/ge_attr_define.h"
#include "common/om2/codegen/om2_codegen_utils.h"

namespace ge {
constexpr uint64_t kDefaultMemBlockSize = 2UL * 1024UL * 1024UL;
constexpr uint64_t kTsMemBlockSize = 4UL * 1024UL;
constexpr uint64_t kMemAlignSize = 512UL;

LoadAndRunFileCodeGenerator::LoadAndRunFileCodeGenerator(AstBuildContext &ast) : Om2ModelClassGeneratorBase(ast) {}

std::vector<DeclNode *> LoadAndRunFileCodeGenerator::BuildAnonymousNamespaceItems(
    const Om2CodegenModel &codegen_model, const std::vector<TaskCodeBuilderPtr> &task_code_builders) const {
  (void)codegen_model;
  std::vector<DeclNode *> items;
  std::unordered_set<std::type_index> helper_types;
  GE_ASSERT_SUCCESS(const_cast<LoadAndRunFileCodeGenerator *>(this)->BuildCommonHelperFunctions(items));
  for (const auto &task_code_builder : task_code_builders) {
    GE_ASSERT_NOTNULL(task_code_builder);
    if (!helper_types.insert(std::type_index(typeid(*task_code_builder))).second) {
      continue;
    }
    GE_ASSERT_SUCCESS(task_code_builder->RenderDistHelper(items));
  }
  return items;
}

MethodDef *LoadAndRunFileCodeGenerator::BuildLoadMethod(const Om2CodegenModel &codegen_model,
                                                      const std::vector<TaskCodeBuilderPtr> &task_code_builders) {
  std::vector<BodyItem> body;
  (void)BuildLoadBody(body, codegen_model, task_code_builders);
  return ast_.DefineMethod("Om2Model", "Load", {}, "aclError", body);
}

Status LoadAndRunFileCodeGenerator::BuildLoadBody(std::vector<BodyItem> &body, const Om2CodegenModel &codegen_model,
                                                const std::vector<TaskCodeBuilderPtr> &task_code_builders) {
  body.push_back(dev_ext_info_mem_ptrs_.Resize(codegen_model.aicpu_task_count));
  for (const auto &entry : codegen_model.const_inputs) {
    body.push_back(ast_.VarDecl("auto", entry.var_name, constants_[entry.const_index]));
  }
  for (const auto &task_code_builder : task_code_builders) {
    GE_ASSERT_NOTNULL(task_code_builder);
    GE_ASSERT_SUCCESS(task_code_builder->RenderDistribution(body));
  }
  body.push_back(ChkStatus(AclmdlRIBuildEnd(model_handle_, nullptr)));
  body.push_back(ast_.Return("ACL_SUCCESS"));
  return SUCCESS;
}

MethodDef *LoadAndRunFileCodeGenerator::BuildRunAsyncMethod(const Om2CodegenModel &codegen_model) {
  std::vector<BodyItem> body;
  (void)BuildRunBodyImpl(body, codegen_model, true);
  auto exe_stream = ast_.Var("aclrtStream &", "exe_stream");
  auto input_count = ast_.Var("size_t", "input_count");
  auto input_data = ast_.Var("void **", "input_data");
  auto output_count = ast_.Var("size_t", "output_count");
  auto output_data = ast_.Var("void **", "output_data");
  return ast_.DefineMethod("Om2Model", "RunAsync", {exe_stream, input_count, input_data, output_count, output_data},
                           "aclError", body);
}

MethodDef *LoadAndRunFileCodeGenerator::BuildRunMethod(const Om2CodegenModel &codegen_model) {
  std::vector<BodyItem> body;
  (void)BuildRunBodyImpl(body, codegen_model, false);
  auto input_count = ast_.Var("size_t", "input_count");
  auto input_data = ast_.Var("void **", "input_data");
  auto output_count = ast_.Var("size_t", "output_count");
  auto output_data = ast_.Var("void **", "output_data");
  auto stream_sync_timeout = ast_.Var("int32_t", "stream_sync_timeout");
  return ast_.DefineMethod("Om2Model", "Run", {input_count, input_data, output_count, output_data, stream_sync_timeout}, "aclError", body);
}

Status LoadAndRunFileCodeGenerator::BuildRunBodyImpl(std::vector<BodyItem> &body,
                                                   const Om2CodegenModel &codegen_model, bool is_async) {
  auto input_count = ast_.Var("size_t", "input_count");
  auto input_data = ast_.Var("void **", "input_data");
  auto output_count = ast_.Var("size_t", "output_count");
  auto output_data = ast_.Var("void **", "output_data");
  auto exe_stream = ast_.Var("aclrtStream &", "exe_stream");
  body.push_back(ast_.If((input_count != "om2::INPUT_NUM") || (output_count != "om2::OUTPUT_NUM"), {
      ast_.Return("ACL_ERROR_FAILURE"),
  }));
  uint32_t input_data_index = 0U;
  uint32_t output_data_index = 0U;
  for (const auto &entry : codegen_model.model_io.entries) {
    std::string tensor_var_name = "";
    std::string addr_var_name = "";
    if (entry.is_input) {
      tensor_var_name = "input_data_" + std::to_string(input_data_index) + "_tensor";
      addr_var_name = "dev_input" + std::to_string(input_data_index) + "_ptr";
      input_data_index++;
    } else {
      tensor_var_name = "output_data_" + std::to_string(output_data_index) + "_tensor";
      addr_var_name = "dev_output" + std::to_string(output_data_index) + "_ptr";
      output_data_index++;
    }
    auto tensor = ast_.Var("auto", tensor_var_name);
    body.push_back(ast_.VarDecl(tensor, ast_.ReinterpretCast("gert::Tensor *",
                                                             (entry.is_input ? input_data : output_data)[entry.index])));
    if (entry.is_addr_refreshable) {
      body.push_back(ChkStatus(args_table_.Attr("UpdateHostArgs")(entry.update_host_args_index,
                                                                  ast_.ReinterpretCast("uintptr_t",
                                                                                       tensor.Arrow("GetAddr")()))));
    } else {
      auto dev_addr = ast_.Var("auto", addr_var_name);
      body.push_back(ast_.VarDecl(dev_addr, GetAddr(ast_.Var("void *", "total_dev_mem_ptr_"), entry.memory_offset)));
      if (is_async) {
        body.push_back(ChkStatus(AclrtMemcpyAsync(dev_addr, tensor.Arrow("GetSize")(), tensor.Arrow("GetAddr")(),
                                                  tensor.Arrow("GetSize")(), "ACL_MEMCPY_DEVICE_TO_DEVICE",
                                                  exe_stream)));
      } else {
        body.push_back(ChkStatus(AclrtMemcpy(dev_addr, tensor.Arrow("GetSize")(), tensor.Arrow("GetAddr")(),
                                             tensor.Arrow("GetSize")(), "ACL_MEMCPY_DEVICE_TO_DEVICE")));
      }
    }
  }
  body.push_back(ast_.BlankLine());
  body.push_back(ChkStatus(args_table_.Attr("CopyArgsToDevice")()));
  if (is_async) {
    body.push_back(ChkStatus(AclmdlRIExecuteAsync(model_handle_, exe_stream)));
  } else {
    body.push_back(ChkStatus(AclmdlRIExecute(model_handle_, ast_.Var("int32_t", "stream_sync_timeout"))));
  }
  body.push_back(ast_.BlankLine());
  body.push_back(ast_.Return("ACL_SUCCESS"));
  return SUCCESS;
}

Status LoadAndRunFileCodeGenerator::BuildCommonHelperFunctions(std::vector<DeclNode *> &items) {
  auto dev_ptr = ast_.Var("void *&", "dev_ptr");
  auto size = ast_.Var("const size_t", "size");
  auto mem_type = ast_.Var("const uint32_t", "mem_type");
  auto mem_ptrs = ast_.Var("std::vector<void *> &", "mem_ptrs");
  auto block_size = ast_.Var("uint64_t", "block_size");
  auto aligned_size = ast_.Var("const auto", "aligned_size");
  auto final_block_size = ast_.Var("const auto", "final_block_size");
  auto rt_ret = ast_.Var("const auto", "rt_ret");

  items.push_back(ast_.Field("constexpr uint16_t", "GE_MODULE_NAME_U16", GE_MODULE_NAME_U16));
  items.push_back(ast_.DefineFunction("MallocDeviceMemory", {dev_ptr, size, mem_type, mem_ptrs}, "aclError", {
      ast_.VarDecl(block_size, kDefaultMemBlockSize),
      ast_.If(mem_type == "RT_MEMORY_TS", {
          ast_.Assign(block_size, kTsMemBlockSize),
      }),
      ast_.VarDecl(aligned_size, ((size + kMemAlignSize) - 1) / kMemAlignSize * kMemAlignSize),
      ast_.VarDecl(final_block_size, ((aligned_size + block_size) - 1) / block_size * block_size),
      ast_.VarDecl(rt_ret, RtMalloc(dev_ptr.Addr(), final_block_size, mem_type, "GE_MODULE_NAME_U16")),
      ast_.If((rt_ret != "RT_ERROR_NONE") || (dev_ptr == nullptr), {
          ast_.Return("ACL_ERROR_FAILURE"),
      }),
      ChkTrue(RtMemset(dev_ptr, block_size, 0, block_size) == "RT_ERROR_NONE"),
      mem_ptrs.PushBack(dev_ptr),
      ast_.Return("ACL_SUCCESS"),
  }));
  return SUCCESS;
}
}  // namespace ge
