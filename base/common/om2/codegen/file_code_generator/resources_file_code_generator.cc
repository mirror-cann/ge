/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/file_code_generator/resources_file_code_generator.h"

#include "common/om2/codegen/task_code_builder/task_code_builder.h"

namespace ge {

ResourcesFileCodeGenerator::ResourcesFileCodeGenerator(AstBuildContext &ast) : Om2ModelClassGeneratorBase(ast) {}

MethodDef *ResourcesFileCodeGenerator::BuildOm2ModelConstructor(const Om2CodegenModel &codegen_model) {
  auto bin_files = ast_.Var("const char **", "bin_files");
  auto bin_data = ast_.Var("const void **", "bin_data");
  auto bin_size = ast_.Var("size_t *", "bin_size");
  auto bin_num = ast_.Var("size_t", "bin_num");
  auto constants = ast_.Var("void **", "constants");
  auto work_ptr = ast_.Var("void *", "work_ptr");
  auto session_id = ast_.Var("uint64_t *", "session_id");
  auto model_id = ast_.Var("uint32_t", "model_id");
  auto instance_handle = ast_.Var("void *", "instance_handle");
  auto i = ast_.Var("size_t", "i");
  std::vector<BodyItem> body = {
      ast_.For(ast_.VarDecl(i, 0), i < bin_num, ast_.PreInc(i), {
          ast_.Assign(bin_info_map_[ast_.ToStr(bin_files[i])], {bin_data[i], bin_size[i]}),
      }),
  };
  const auto &runtime = codegen_model.runtime;
  if (runtime.kernel_bin_num > 0U) {
    body.emplace_back(bin_handles_.Resize(runtime.kernel_bin_num));
    body.emplace_back(func_handles_.Resize(runtime.kernel_bin_num));
  }
  if (runtime.stream_num > 0U) {
    body.emplace_back(stream_list_.Resize(runtime.stream_num));
  }
  if (runtime.notify_num > 0U) {
    body.emplace_back(notify_list_.Resize(runtime.notify_num));
  }
  if (runtime.event_num > 0U) {
    body.emplace_back(event_list_.Resize(runtime.event_num));
  }
  if (runtime.label_num > 0U) {
    body.emplace_back(label_list_.Resize(runtime.label_num));
  }
  return ast_.DefineMethod(
      "Om2Model", "Om2Model",
      {bin_files, bin_data, bin_size, bin_num, constants, work_ptr, session_id, model_id, instance_handle}, "",
      {ast_.MemberInit("constants_", constants), ast_.MemberInit("total_dev_mem_ptr_", work_ptr),
       ast_.MemberInit("session_id_", session_id), ast_.MemberInit("model_id_", model_id),
       ast_.MemberInit("instance_handle_", instance_handle), ast_.MemberInit("kernel_id_", 0),
       ast_.MemberInit("session_scope_mem_ptr_", nullptr)},
      body);
}

MethodDef *ResourcesFileCodeGenerator::BuildOm2ModelDestructor() const {
  return ast_.DefineMethod("Om2Model", "~Om2Model", {}, "", {
      ast_.IgnoreOutput(ast_.Call("ReleaseResources", {})),
  });
}

MethodDef *ResourcesFileCodeGenerator::BuildInitResourcesMethod(
    const Om2CodegenModel &codegen_model, const std::vector<TaskCodeBuilderPtr> &task_code_builders) {
  std::vector<BodyItem> body = {
      ast_.Comment("1. 创建 model"),
      ChkStatus(AclmdlRIBuildBegin(model_handle_.Addr(), 0)),
      ast_.BlankLine(),
      ast_.Comment("2. 获取overflow地址"),
      ChkStatus(AclrtCtxGetFloatOverflowAddr(overflow_addr_.Addr())),
      ast_.BlankLine(),
      ast_.Comment("3. 创建其他资源"),
  };
  const auto &runtime = codegen_model.runtime;
  BuildInitStreamResources(body, runtime);
  BuildInitNotifyResources(body, runtime);
  BuildInitEventResources(body, runtime);
  BuildInitLabelResources(body, runtime);
  for (const auto &task_code_builder : task_code_builders) {
    GE_ASSERT_NOTNULL(task_code_builder);
    GE_ASSERT_SUCCESS(task_code_builder->RenderInitResource(body));
  }
  BuildInitSessionScopeMemory(body, runtime);
  body.emplace_back(args_table_.Attr("Init")());
  body.emplace_back(ast_.Return("ACL_SUCCESS"));
  return ast_.DefineMethod("Om2Model", "InitResources", {}, "aclError", body);
}

void ResourcesFileCodeGenerator::BuildInitStreamResources(std::vector<BodyItem> &body,
                                                          const RuntimeResourceSemantic &runtime) {
  if (runtime.stream_num == 0U) {
    return;
  }
  body.emplace_back(ast_.Comment("创建下沉Stream并绑定模型"));
  for (uint32_t i = 0U; i < runtime.stream_num; ++i) {
    const auto stream_flag = ast_.Var("uint32_t", "stream" + std::to_string(i) + "_flag");
    body.emplace_back(ast_.VarDecl(stream_flag, runtime.stream_flag_values[i]));
    body.emplace_back(ChkRt(RtStreamCreateWithFlags(stream_list_[i].Addr(), 0, stream_flag)));
    const auto bind_flag = ast_.Var("auto", "bind" + std::to_string(i) + "_flag");
    body.emplace_back(ast_.VarDecl(bind_flag, runtime.bind_flag_values[i]));
    body.emplace_back(ChkRt(RtModelBindStream(model_handle_, stream_list_[i], bind_flag)));
  }
  body.emplace_back(ast_.Assign(is_stream_list_bind_, true));
}

void ResourcesFileCodeGenerator::BuildInitNotifyResources(std::vector<BodyItem> &body,
                                                          const RuntimeResourceSemantic &runtime) {
  if (runtime.notify_num == 0U) {
    return;
  }
  auto i = ast_.Var("size_t", "i");
  body.emplace_back(ast_.Comment("创建Notify"));
  body.emplace_back(ast_.For(ast_.VarDecl(i, 0), i < runtime.notify_num, ast_.PreInc(i), {
      ChkStatus(AclrtCreateNotify(notify_list_[i].Addr(), "ACL_NOTIFY_DEVICE_USE_ONLY")),
  }));
}

void ResourcesFileCodeGenerator::BuildInitEventResources(std::vector<BodyItem> &body,
                                                         const RuntimeResourceSemantic &runtime) {
  if (runtime.event_num == 0U) {
    return;
  }
  auto i = ast_.Var("size_t", "i");
  body.emplace_back(ast_.Comment("创建Event"));
  body.emplace_back(ast_.For(ast_.VarDecl(i, 0), i < runtime.event_num, ast_.PreInc(i), {
      ChkStatus(AclrtCreateEventWithFlag(event_list_[i].Addr(),
                                         "ACL_EVENT_SYNC | ACL_EVENT_CAPTURE_STREAM_PROGRESS | ACL_EVENT_TIME_LINE")),
  }));
}

void ResourcesFileCodeGenerator::BuildInitLabelResources(std::vector<BodyItem> &body,
                                                         const RuntimeResourceSemantic &runtime) {
  if (runtime.label_num == 0U) {
    return;
  }
  auto i = ast_.Var("size_t", "i");
  body.emplace_back(ast_.Comment("创建Label"));
  body.emplace_back(ast_.For(ast_.VarDecl(i, 0), i < runtime.label_num, ast_.PreInc(i), {
      ChkStatus(AclrtCreateLabel(label_list_[i].Addr())),
  }));
}

void ResourcesFileCodeGenerator::BuildInitSessionScopeMemory(std::vector<BodyItem> &body,
                                                             const RuntimeResourceSemantic &runtime) {
  const uint64_t ss_key = kSessionScopeMemoryMask | RT_MEMORY_HBM;
  const auto ss_it = runtime.memory_infos.find(ss_key);
  if (ss_it == runtime.memory_infos.end() || ss_it->second.memory_size <= 0) {
    return;
  }
  body.emplace_back(ast_.Comment("Allocate session scope memory"));
  body.emplace_back(ChkStatus(AclrtMalloc(session_scope_mem_ptr_.Addr(),
                                           static_cast<int64_t>(ss_it->second.memory_size),
                                           "ACL_MEM_MALLOC_HUGE_FIRST")));
}

MethodDef *ResourcesFileCodeGenerator::BuildReleaseResourcesMethod(const Om2CodegenModel &codegen_model) {
  std::vector<BodyItem> body;
  const auto &runtime = codegen_model.runtime;
  if (runtime.label_num > 0U) {
    auto label = ast_.Var("auto", "label");
    body.emplace_back(ast_.RangeFor(label, label_list_, {
        ast_.If(label != nullptr, {ChkStatus(AclrtDestroyLabel(label))}),
    }));
  }
  if (runtime.event_num > 0U) {
    auto event = ast_.Var("auto", "event");
    body.emplace_back(ast_.RangeFor(event, event_list_, {
        ChkStatus(AclrtDestroyEvent(event)),
    }));
  }
  if (runtime.notify_num > 0U) {
    auto notify = ast_.Var("auto", "notify");
    body.emplace_back(ast_.RangeFor(notify, notify_list_, {
        ChkStatus(AclrtDestroyNotify(notify)),
    }));
  }
  if (runtime.stream_num > 0U) {
    auto stream = ast_.Var("auto", "stream");
    body.emplace_back(ast_.If(is_stream_list_bind_, {ast_.RangeFor(stream, stream_list_, {
                                                    ChkStatus(AclmdlRIUnbindStream(model_handle_, stream)),
                                                })}));
    body.emplace_back(ast_.RangeFor(stream, stream_list_, {
        ChkStatus(AclrtDestroyStream(stream)),
    }));
  }
  if (runtime.kernel_bin_num > 0U) {
    auto bin_handle = ast_.Var("auto", "bin_handle");
    body.emplace_back(ast_.RangeFor(bin_handle, bin_handles_, {
        ChkStatus(AclrtBinaryUnLoad(bin_handle)),
    }));
  }
  BuildReleaseResourcesMethodForControlTask(body, runtime);
  auto i = ast_.Var("int", "i");
  body.emplace_back(ChkStatus(AclmdlRIDestroy(model_handle_)));
  body.emplace_back(ast_.If(session_scope_mem_ptr_ != nullptr, {
      ChkStatus(AclrtFree(session_scope_mem_ptr_)),
  }));
  body.emplace_back(ast_.For(ast_.VarDecl(i, 0), i < dev_ext_info_mem_ptrs_.Size(), ast_.PostInc(i), {
      ast_.If(dev_ext_info_mem_ptrs_[i] != nullptr, {ChkStatus(AclrtFree(dev_ext_info_mem_ptrs_[i]))}),
  }));
  body.emplace_back(ast_.For(ast_.VarDecl(i, 0), i < dev_dynamic_mem_ptrs_.Size(), ast_.PostInc(i), {
      ast_.If(dev_dynamic_mem_ptrs_[i] != nullptr, {ChkStatus(AclrtFree(dev_dynamic_mem_ptrs_[i]))}),
  }));
  body.emplace_back(ast_.Return("ACL_SUCCESS"));
  return ast_.DefineMethod("Om2Model", "ReleaseResources", {}, "aclError", body);
}

void ResourcesFileCodeGenerator::BuildReleaseResourcesMethodForControlTask(std::vector<BodyItem> &body,
                                                                           const RuntimeResourceSemantic &runtime) {
  if (runtime.has_label_switch) {
    auto label = ast_.Var("auto &", "label");
    body.emplace_back(ast_.RangeFor(label, label_switch_label_list_, {
        ast_.If(label.Attr("second") != nullptr, {ChkStatus(AclrtDestroyLabelList(label.Attr("second")))}),
    }));
  }
  if (runtime.has_label_goto) {
    auto label_goto_ex_index_value = ast_.Var("auto &", "label_goto_ex_index_value");
    body.emplace_back(
      ast_.RangeFor(label_goto_ex_index_value, label_goto_ex_index_values_,
                    {ChkStatus(AclrtFree(label_goto_ex_index_value)),})
    );
    auto label_goto_arg = ast_.Var("auto &", "label_goto_arg");
    auto arg_addr = ast_.Var("void *", "arg_addr");
    body.emplace_back(
      ast_.RangeFor(label_goto_arg, label_goto_args_,
                    {
                      ast_.VarDecl(arg_addr, label_goto_arg.Attr("second").Attr("first")),
                      ast_.If(arg_addr != nullptr, {ChkStatus(AclrtDestroyLabelList(arg_addr))})
                    }));
    body.emplace_back(label_goto_args_.Clear());
  }
}
}  // namespace ge
