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

#include <iomanip>
#include <set>
#include <sstream>
#include <map>
#include <typeindex>
#include <unordered_set>

#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "graph/debug/ge_attr_define.h"
#include "common/om2/codegen/om2_codegen_utils.h"

namespace ge {
namespace {
constexpr uint64_t kDefaultMemBlockSize = 2UL * 1024UL * 1024UL;
constexpr uint64_t kTsMemBlockSize = 4UL * 1024UL;
constexpr uint64_t kMemAlignSize = 512UL;
}  // namespace

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
  GE_ASSERT_SUCCESS(BuildDispatchOp(items, task_code_builders));
  return items;
}

MethodDef *LoadAndRunFileCodeGenerator::BuildLoadMethod(const Om2CodegenModel &codegen_model,
                                                        const std::vector<TaskCodeBuilderPtr> &task_code_builders) {
  std::vector<BodyItem> body;
  (void)BuildLoadBody(body, codegen_model, task_code_builders);
  return ast_.DefineMethod("Om2Model", "Load", {}, "aclError", body);
}

MethodDef *LoadAndRunFileCodeGenerator::BuildGetRtModelHandleMethod() const {
  return ast_.DefineMethod("Om2Model", "GetRtModelHandle", {}, "aclmdlRI", {ast_.Return("model_handle_")});
}

Status LoadAndRunFileCodeGenerator::BuildLoadBody(std::vector<BodyItem> &body, const Om2CodegenModel &codegen_model,
                                                  const std::vector<TaskCodeBuilderPtr> &task_code_builders) {
  body.push_back(ast_.Call("OM2_LOGI", {ast_.Str("Load begin")}));
  body.push_back(dev_ext_info_mem_ptrs_.Resize(codegen_model.aicpu_task_count));
  uint32_t op_idx = 0;
  for (const auto &task_code_builder : task_code_builders) {
    GE_ASSERT_NOTNULL(task_code_builder);
    if (task_code_builder->SupportsTableDriven()) {
      // 表驱动模式：调用 DispatchOp
      const auto stream_id = static_cast<int64_t>(task_code_builder->GetHeader().stream_id);
      body.push_back(ChkStatus(
          ast_.Call("DispatchOp",
                    {ast_.Var("OpDef", "kOpDefs")[static_cast<int64_t>(op_idx)].Addr(),
                     ast_.InitList({total_dev_mem_ptr_, session_scope_mem_ptr_, constants_, args_table_,
                                    func_handles_.Attr("data")(), stream_list_[stream_id], model_id_, instance_handle_,
                                    mem_event_id_mem_map_, dev_dynamic_mem_ptrs_, overflow_addr_})})));
      op_idx++;
    } else {
      // RenderDistribution 模式：直接生成代码
      std::vector<BodyItem> task_body;
      GE_ASSERT_SUCCESS(task_code_builder->RenderDistribution(task_body));
      body.push_back(ast_.Block(task_body));
    }
  }
  body.push_back(ChkStatus(AclmdlRIBuildEnd(model_handle_, nullptr)));
  body.push_back(ast_.Call("OM2_LOGI", {ast_.Str("Load done")}));
  body.push_back(ast_.Return("ACL_SUCCESS"));
  return SUCCESS;
}

DeclNode *LoadAndRunFileCodeGenerator::BuildOpDefTable(
    const Om2CodegenModel &codegen_model, const std::vector<TaskCodeBuilderPtr> &task_code_builders) const {
  (void)codegen_model;
  std::vector<Arg> entries;
  for (const auto &task_code_builder : task_code_builders) {
    GE_ASSERT_NOTNULL(task_code_builder);
    if (!task_code_builder->SupportsTableDriven()) {
      continue;  // 跳过不支持表驱动的 task
    }
    const auto dispatch_type = static_cast<uint32_t>(task_code_builder->GetTaskType());

    std::vector<std::pair<std::string, Arg>> fields;
    GE_ASSERT_SUCCESS(task_code_builder->RenderOpDefTableFields(fields, dispatch_type));

    entries.push_back(ast_.InitListWithDesignators(fields));
  }
  return ast_.Field("const OpDef", "kOpDefs[]", ast_.InitList(entries));
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
  return ast_.DefineMethod("Om2Model", "Run", {input_count, input_data, output_count, output_data, stream_sync_timeout},
                           "aclError", body);
}

Status LoadAndRunFileCodeGenerator::BuildRunBodyImpl(std::vector<BodyItem> &body, const Om2CodegenModel &codegen_model,
                                                     bool is_async) {
  auto input_count = ast_.Var("size_t", "input_count");
  auto input_data = ast_.Var("void **", "input_data");
  auto output_count = ast_.Var("size_t", "output_count");
  auto output_data = ast_.Var("void **", "output_data");
  auto exe_stream = ast_.Var("aclrtStream &", "exe_stream");
  auto body_item = is_async ? ast_.Str("RunAsync begin") : ast_.Str("Run begin");
  body.push_back(ast_.Call("OM2_LOGI", {body_item}));
  body.push_back(ast_.If((input_count != "om2::INPUT_NUM") || (output_count != "om2::OUTPUT_NUM"),
                         {
                             ast_.Return("ACL_ERROR_FAILURE"),
                         }));
  // 声明 input_data_N_tensor / output_data_N_tensor 变量（按 index 去重）。
  BuildRunBodyDeclareTensorIoVars(body, codegen_model.model_io.entries, input_data, output_data);

  // 可刷新的 entry（无论输入输出）调 UpdateHostArgs 刷新 producer 地址；
  // 不可刷新的输入 entry 从用户 buffer Memcpy 到 device 地址；
  // 不可刷新的输出 entry 此处跳过，由 BuildRunBodyCopyOutputs 执行后拷回。
  BuildRunBodyProcessInputsAndAddrRefresh(body, codegen_model.model_io.entries, exe_stream, is_async);

  body.push_back(ast_.BlankLine());
  body.push_back(ChkStatus(args_table_.Attr("CopyArgsToDevice")()));
  if (is_async) {
    body.push_back(ChkStatus(AclmdlRIExecuteAsync(model_handle_, exe_stream)));
  } else {
    body.push_back(ChkStatus(AclmdlRIExecute(model_handle_, ast_.Var("int32_t", "stream_sync_timeout"))));
  }

  // 执行后：非输入、非地址刷新的输出从 device 拷回用户 buffer。
  BuildRunBodyCopyOutputs(body, codegen_model.model_io.entries, exe_stream, is_async);

  body.push_back(ast_.BlankLine());
  auto done_item = is_async ? ast_.Str("RunAsync done") : ast_.Str("Run done");
  body.push_back(ast_.Call("OM2_LOGI", {done_item}));
  body.push_back(ast_.Return("ACL_SUCCESS"));
  return SUCCESS;
}

void LoadAndRunFileCodeGenerator::BuildRunBodyDeclareTensorIoVars(std::vector<BodyItem> &body,
                                                                  const std::vector<ModelIoEntry> &entries,
                                                                  const VarRef &input_data, const VarRef &output_data) {
  std::set<uint32_t> declared_input_indices;
  std::set<uint32_t> declared_output_indices;
  for (const auto &entry : entries) {
    std::string tensor_var_name;
    bool should_declare_tensor = false;
    if (entry.is_input) {
      tensor_var_name = "input_data_" + std::to_string(entry.index) + "_tensor";
      should_declare_tensor = declared_input_indices.insert(entry.index).second;
    } else {
      tensor_var_name = "output_data_" + std::to_string(entry.index) + "_tensor";
      should_declare_tensor = declared_output_indices.insert(entry.index).second;
    }
    if (should_declare_tensor) {
      auto tensor = ast_.Var("auto", tensor_var_name);
      body.push_back(ast_.VarDecl(
          tensor, ast_.ReinterpretCast("gert::Tensor *", (entry.is_input ? input_data : output_data)[entry.index])));
    }
  }
}

void LoadAndRunFileCodeGenerator::BuildRunBodyProcessInputsAndAddrRefresh(std::vector<BodyItem> &body,
                                                                          const std::vector<ModelIoEntry> &entries,
                                                                          const VarRef &exe_stream, bool is_async) {
  for (const auto &entry : entries) {
    const std::string tensor_var_name = entry.is_input ? ("input_data_" + std::to_string(entry.index) + "_tensor")
                                                       : ("output_data_" + std::to_string(entry.index) + "_tensor");
    auto tensor = ast_.Var("auto", tensor_var_name);
    if (entry.is_addr_refreshable) {
      // OM2 临时处理：NoTask 连续输入复用输出展开为多段地址刷新。
      ExprRef addr_expr = ast_.ReinterpretCast("uintptr_t", tensor.Arrow("GetAddr")());
      if (entry.addr_offset != 0) {
        addr_expr = addr_expr + entry.addr_offset;
      }
      body.push_back(ChkStatus(args_table_.Attr("UpdateHostArgs")(entry.update_host_args_index, addr_expr)));
    } else if (entry.is_input) {
      const std::string addr_var_name = "dev_input" + std::to_string(entry.index) + "_ptr";
      auto dev_addr = ast_.Var("auto", addr_var_name);
      body.push_back(ast_.VarDecl(dev_addr, GetAddr(ast_.Var("void *", "total_dev_mem_ptr_"), entry.memory_offset)));
      if (is_async) {
        body.push_back(
            ChkStatus(AclrtMemcpyAsync(dev_addr, tensor.Arrow("GetSize")(), tensor.Arrow("GetAddr")(),
                                       tensor.Arrow("GetSize")(), "ACL_MEMCPY_DEVICE_TO_DEVICE", exe_stream)));
      } else {
        body.push_back(ChkStatus(AclrtMemcpy(dev_addr, tensor.Arrow("GetSize")(), tensor.Arrow("GetAddr")(),
                                             tensor.Arrow("GetSize")(), "ACL_MEMCPY_DEVICE_TO_DEVICE")));
      }
    }
  }
}

void LoadAndRunFileCodeGenerator::BuildRunBodyCopyOutputs(std::vector<BodyItem> &body,
                                                          const std::vector<ModelIoEntry> &entries,
                                                          const VarRef &exe_stream, bool is_async) {
  for (const auto &entry : entries) {
    if (entry.is_input || entry.is_addr_refreshable) {
      continue;
    }
    const std::string tensor_var_name = "output_data_" + std::to_string(entry.index) + "_tensor";
    const std::string addr_var_name = "dev_output" + std::to_string(entry.index) + "_ptr";
    auto tensor = ast_.Var("auto", tensor_var_name);
    auto dev_addr = ast_.Var("auto", addr_var_name);
    body.push_back(ast_.VarDecl(dev_addr, GetAddr(ast_.Var("void *", "total_dev_mem_ptr_"), entry.memory_offset)));
    if (is_async) {
      body.push_back(ChkStatus(AclrtMemcpyAsync(tensor.Arrow("GetAddr")(), tensor.Arrow("GetSize")(), dev_addr,
                                                tensor.Arrow("GetSize")(), "ACL_MEMCPY_DEVICE_TO_DEVICE", exe_stream)));
    } else {
      body.push_back(ChkStatus(AclrtMemcpy(tensor.Arrow("GetAddr")(), tensor.Arrow("GetSize")(), dev_addr,
                                           tensor.Arrow("GetSize")(), "ACL_MEMCPY_DEVICE_TO_DEVICE")));
    }
  }
}

Status LoadAndRunFileCodeGenerator::BuildAclrtMallocFunction(std::vector<DeclNode *> &items) const {
  auto ptr = ast_.Var("void **", "ptr");
  auto size = ast_.Var("size_t", "size");
  auto mem_type = ast_.Var("uint32_t", "mem_type");
  auto module_id = ast_.Var("uint16_t", "module_id");
  auto attr = ast_.Var("aclrtMallocAttribute", "attr");
  auto cfg = ast_.Var("aclrtMallocConfig", "cfg");

  std::vector<BodyItem> body;
  body.push_back(ast_.Assign(ast_.Deref(ptr), Arg(nullptr)));
  body.push_back(ast_.If(size == ast_.UInt(0), {ast_.Return("ACL_SUCCESS")}));
  body.push_back(ast_.VarDecl(attr));
  body.push_back(ast_.Assign(attr.Attr("attr"), "ACL_RT_MEM_ATTR_MODULE_ID"));
  body.push_back(ast_.Assign(attr.Attr("value").Attr("moduleId"), module_id));
  body.push_back(ast_.VarDecl(cfg));
  body.push_back(ast_.Assign(cfg.Attr("attrs"), attr.Addr()));
  body.push_back(ast_.Assign(cfg.Attr("numAttrs"), ast_.UInt(1)));

  std::vector<BodyItem> switch_body;
  switch_body.push_back(ast_.Case("RT_MEMORY_TS"));
  switch_body.push_back(
      ast_.Return(ast_.Call("aclrtMallocForTaskScheduler", {ptr, size, "ACL_MEM_MALLOC_HUGE_FIRST", cfg.Addr()})));
  switch_body.push_back(ast_.Case("RT_MEMORY_HOST"));
  switch_body.push_back(ast_.Return(ast_.Call("aclrtMallocHostWithCfg", {ptr, size, cfg.Addr()})));
  switch_body.push_back(ast_.Case("RT_MEMORY_P2P_HBM"));
  switch_body.push_back(ast_.Case("RT_MEMORY_P2P_DDR"));
  switch_body.push_back(
      ast_.Return(ast_.Call("aclrtMallocWithCfg", {ptr, size, "ACL_MEM_MALLOC_HUGE_FIRST_P2P", cfg.Addr()})));
  switch_body.push_back(ast_.Case("RT_MEMORY_DDR"));
  switch_body.push_back(ast_.Case("RT_MEMORY_DDR_NC"));
  switch_body.push_back(
      ast_.Return(ast_.Call("aclrtMallocWithCfg", {ptr, size, "ACL_MEM_TYPE_LOW_BAND_WIDTH", cfg.Addr()})));
  switch_body.push_back(ast_.Case(Arg(nullptr)));
  switch_body.push_back(
      ast_.Return(ast_.Call("aclrtMallocWithCfg", {ptr, size, "ACL_MEM_TYPE_HIGH_BAND_WIDTH", cfg.Addr()})));

  body.push_back(ast_.Switch(mem_type, switch_body));

  items.push_back(ast_.DefineFunction("AclrtMalloc", {ptr, size, mem_type, module_id}, "aclError", ast_.Body(body)));
  return SUCCESS;
}

Status LoadAndRunFileCodeGenerator::BuildCommonHelperFunctions(std::vector<DeclNode *> &items) const {
  GE_ASSERT_SUCCESS(BuildAclrtMallocFunction(items));

  auto dev_ptr = ast_.Var("void *&", "dev_ptr");
  auto size = ast_.Var("const size_t", "size");
  auto mem_type = ast_.Var("const uint32_t", "mem_type");
  auto mem_ptrs = ast_.Var("std::vector<void *> &", "mem_ptrs");
  auto block_size = ast_.Var("uint64_t", "block_size");
  auto aligned_size = ast_.Var("const auto", "aligned_size");
  auto final_block_size = ast_.Var("const auto", "final_block_size");
  auto rt_ret = ast_.Var("const auto", "rt_ret");

  items.push_back(ast_.Field("constexpr uint16_t", "GE_MODULE_NAME_U16", GE_MODULE_NAME_U16));
  items.push_back(ast_.DefineFunction(
      "MallocDeviceMemory", {dev_ptr, size, mem_type, mem_ptrs}, "aclError",
      {
          ast_.VarDecl(block_size, kDefaultMemBlockSize),
          ast_.If(mem_type == "RT_MEMORY_TS",
                  {
                      ast_.Assign(block_size, kTsMemBlockSize),
                  }),
          ast_.VarDecl(aligned_size, ((size + kMemAlignSize) - 1) / kMemAlignSize * kMemAlignSize),
          ast_.VarDecl(final_block_size, ((aligned_size + block_size) - 1) / block_size * block_size),
          ast_.VarDecl(rt_ret, AclrtMallocHelper(dev_ptr.Addr(), final_block_size, mem_type, "GE_MODULE_NAME_U16")),
          ast_.If((rt_ret != "ACL_SUCCESS") || (dev_ptr == nullptr),
                  {
                      ast_.Return("ACL_ERROR_FAILURE"),
                  }),
          ChkStatus(AclrtMemset(dev_ptr, block_size, 0, block_size)),
          mem_ptrs.PushBack(dev_ptr),
          ast_.Return("ACL_SUCCESS"),
      }));
  items.push_back(ast_.Field("constexpr const size_t", "max_launch_cfg_num = 8UL"));
  items.push_back(BuildLaunchKernelCfgHolder());
  items.push_back(BuildLaunchKernelConfig());
  items.push_back(BuildAssembleLaunchConfig());
  return SUCCESS;
}

StructDecl *LoadAndRunFileCodeGenerator::BuildLaunchKernelCfgHolder() const {
  return ast_.Struct("LaunchKernelCfgHolder", {
                                                  ast_.Field("aclrtLaunchKernelCfg", "cfg{}"),
                                                  ast_.Field("aclrtLaunchKernelAttr", "attrs[max_launch_cfg_num]"),
                                              });
}

StructDecl *LoadAndRunFileCodeGenerator::BuildLaunchKernelConfig() const {
  return ast_.Struct("LaunchKernelConfig", {
                                               ast_.Field("uint8_t", "schedule_mode{0U}"),
                                               ast_.Field("aclrtEngineType", "engine_type{ACL_RT_ENGINE_TYPE_AIC}"),
                                               ast_.Field("uint32_t", "block_dim_offset{0U}"),
                                               ast_.Field("bool", "is_block_task_prefetch{false}"),
                                               ast_.Field("uint8_t", "is_data_dump{0U}"),
                                               ast_.Field("uint16_t", "time_out{0U}"),
                                               ast_.Field("uint32_t", "local_memory_size{0U}"),
                                           });
}

FunctionDef *LoadAndRunFileCodeGenerator::BuildAssembleLaunchConfig() const {
  auto holder = ast_.Var("LaunchKernelCfgHolder &", "holder");
  auto launch_config = ast_.Var("const LaunchKernelConfig &", "launch_config");
  auto actual_cfg_num = ast_.Var("size_t", "actual_cfg_num");
  auto attrs = holder.Attr("attrs");
  std::vector<BodyItem> body = {
      ast_.VarDecl(actual_cfg_num, "0UL"),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("schemMode"), launch_config.Attr("schedule_mode")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("engineType"), launch_config.Attr("engine_type")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("blockDimOffset"), launch_config.Attr("block_dim_offset")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_BLOCK_TASK_PREFETCH"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("isBlockTaskPrefetch"),
                  ast_.StaticCast("uint8_t", launch_config.Attr("is_block_task_prefetch"))),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_DATA_DUMP"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("isDataDump"), launch_config.Attr("is_data_dump")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("dynUBufSize"), launch_config.Attr("local_memory_size")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(attrs[actual_cfg_num].Attr("id"), "ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT"),
      ast_.Assign(attrs[actual_cfg_num].Attr("value").Attr("timeout"), launch_config.Attr("time_out")),
      ast_.PostInc(actual_cfg_num),
      ast_.Assign(holder.Attr("cfg").Attr("attrs"), attrs[0].Addr()),
      ast_.Assign(holder.Attr("cfg").Attr("numAttrs"), actual_cfg_num),
  };
  body.push_back(ast_.Return("ACL_SUCCESS"));
  return ast_.DefineFunction("AssembleLaunchConfig", {holder, launch_config}, "aclError", ast_.Body(body));
}

Status LoadAndRunFileCodeGenerator::BuildDispatchOp(std::vector<DeclNode *> &items,
                                                    const std::vector<TaskCodeBuilderPtr> &task_code_builders) const {
  // Collect unique dispatch type → first builder mapping
  // Use the first builder for each dispatch type (all builders mapping to same type are equivalent)
  // Only include table-driven task types
  std::map<uint32_t, TaskCodeBuilderPtr> type_to_builder;
  for (const auto &builder : task_code_builders) {
    GE_ASSERT_NOTNULL(builder);
    if (!builder->SupportsTableDriven()) {
      continue;  // 跳过不支持表驱动的 task
    }
    const uint32_t dtype = static_cast<uint32_t>(builder->GetTaskType());
    type_to_builder[dtype] = builder;
  }

  // Build switch body items: case labels + case body + break + default
  std::vector<BodyItem> switch_body;

  for (const auto &[dtype, builder] : type_to_builder) {
    // case label: raw ModelTaskType value
    switch_body.push_back(ast_.Case(ast_.UInt(dtype)));
    // case body from builder (wrap in Block to avoid cross-init of case-scoped variables)
    std::vector<BodyItem> case_body;
    GE_ASSERT_SUCCESS(builder->RenderDispatchOpCaseBody(case_body));
    switch_body.push_back(ast_.Block(case_body));
    // break
    switch_body.push_back(ast_.Break());
  }

  // default case
  switch_body.push_back(ast_.Case(Arg(nullptr)));  // default:
  switch_body.push_back(ast_.Return("ACL_ERROR_FAILURE"));

  // Build the DispatchOp function
  auto op = ast_.Var("const OpDef *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  auto dispatch_op = ast_.DefineFunction(
      "DispatchOp", {op, ctx}, "aclError",
      {
          ast_.Switch(ast_.StaticCast("OpDispatchType", ast_.Var("", "op").Arrow("dispatch_type")), switch_body),
          ast_.Return("ACL_SUCCESS"),
      });
  items.push_back(dispatch_op);
  return SUCCESS;
}
}  // namespace ge
