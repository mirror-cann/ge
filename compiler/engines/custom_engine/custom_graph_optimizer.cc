/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "custom_graph_optimizer.h"

#include <unordered_map>

#include "custom_op_factory.h"
#include "common/ge_common/ge_types.h"
#include "graph/utils/attr_utils.h"
#include "common/checker.h"
#include "lowering/kernel_run_context_builder.h"
#include "common/compile_profiling/ge_trace_wrapper.h"
#include "common/thread_pool/thread_pool.h"
#include "common/context/local_context.h"
#include "platform/platform_info.h"
#include "mmpa/mmpa_api.h"
#include "runtime/v2/kernel/common_kernel_impl/platform.h"

namespace {
uint32_t GetThreadNum() {
  const char_t *value = nullptr;
  MM_SYS_GET_ENV(MM_ENV_MAX_COMPILE_CORE_NUMBER, value);
  const int64_t thread_num = ((value != nullptr) && (value[0U] != '\0')) ?
    std::strtol(value, nullptr, 10) : 16;
  if (thread_num <= 0) {
    GELOGW("Get invalid MAX_COMPILE_CORE_NUMBER env value %s, use default thread number 16", value);
  }
  return (thread_num > 0) ? static_cast<uint32_t>(thread_num) : 16U;
}

struct CompileTask {
  CompileTask(ge::CompilableOp *op_ptr, gert::KernelContextHolder &&holder, std::string name, std::string type)
      : compilable_op_ptr(op_ptr),
        op_compile_context_holder(std::move(holder)),
        op_name(std::move(name)),
        op_type(std::move(type)) {}

  CompileTask(CompileTask &&) = default;
  CompileTask &operator=(CompileTask &&) = default;
  CompileTask(const CompileTask &) = delete;
  CompileTask &operator=(const CompileTask &) = delete;

  ge::CompilableOp *compilable_op_ptr;
  gert::KernelContextHolder op_compile_context_holder;
  std::string op_name;
  std::string op_type;
};

ge::Status CompileCustomOp(CompileTask *task) {
  GELOGI("[Compile][CustomOp] call compile, op_name:%s, op_type:%s", task->op_name.c_str(), task->op_type.c_str());
  auto *const op_compile_context =
      reinterpret_cast<gert::OpCompileContext *>(task->op_compile_context_holder.GetKernelContext());
  if (op_compile_context == nullptr) {
    GELOGE(ge::FAILED, "[Compile][CustomOp] compile context is null, op_type:%s", task->op_type.c_str());
    return ge::FAILED;
  }
  return task->compilable_op_ptr->Compile(op_compile_context);
}

ge::Status CompileCustomOpSerially(const std::vector<CompileTask *> *tasks) {
  for (auto *task : *tasks) {
    const auto ret = CompileCustomOp(task);
    if (ret != ge::SUCCESS) {
      GELOGE(ret, "[Compile][CustomOp] compile failed, op_name:%s, op_type:%s",
             task->op_name.c_str(), task->op_type.c_str());
      return ret;
    }
  }
  return ge::SUCCESS;
}
}
namespace ge {
CustomGraphOptimizer::~CustomGraphOptimizer() = default;

Status CustomGraphOptimizer::Initialize(const std::map<std::string, std::string> &options,
    ge::OptimizeUtility *const optimize_utility) {
  (void)options;
  (void)optimize_utility;
  return SUCCESS;
}

ge::Status CustomGraphOptimizer::Finalize() {
  return SUCCESS;
}

ge::Status CustomGraphOptimizer::OptimizeOriginalGraph(ge::ComputeGraph &graph) {
  (void)graph;
  return SUCCESS;
}

ge::Status CustomGraphOptimizer::OptimizeFusedGraph(ge::ComputeGraph &graph) {
  (void)graph;
  return SUCCESS;
}

ge::Status CustomGraphOptimizer::OptimizeWholeGraph(ge::ComputeGraph &graph) {
  (void)graph;
  return SUCCESS;
}

ge::Status CustomGraphOptimizer::GetAttributes(ge::GraphOptimizerAttribute &attrs) const {
  attrs.engineName = kEngineNameCustom;
  return SUCCESS;
}

Status CustomGraphOptimizer::OptimizeSubgraphPostProc(ComputeGraph &graph) {
  GELOGI("entering optimize subgraph post proc");
  GE_TIMESTAMP_START(CustomGraphOptimizer);
  std::vector<CompileTask> compile_tasks;
  for (const auto &node : graph.GetAllNodes()) {
    auto op_type = node->GetType();
    AscendString op_type_ascend(op_type.c_str());
    if (CustomOpFactory::IsExistOp(op_type_ascend)) {
      GELOGI("during optimize subgraph post proc, %s is custom op", op_type_ascend.GetString());
      auto *base_custom_op_ptr = CustomOpFactory::CreateOrGetCustomOp(op_type_ascend);
      GE_ASSERT_NOTNULL(base_custom_op_ptr);
      auto *compilable_op_ptr = dynamic_cast<CompilableOp *>(base_custom_op_ptr);
      if (compilable_op_ptr == nullptr) {
        GELOGI("[Compile][CustomOp] custom op did not implement CompilableOp, op_name:%s, op_type:%s",
               node->GetName().c_str(), op_type.c_str());
        continue;
      }
      auto op_compile_context_holder = gert::KernelRunContextBuilder().Build(node->GetOpDesc());
      compile_tasks.emplace_back(compilable_op_ptr, std::move(op_compile_context_holder), node->GetName(), op_type);
    }
  }
  if (compile_tasks.empty()) {
    return SUCCESS;
  }
  const auto actual_thread_num = GetThreadNum();
  GELOGI("Custom op compile thread num is %u", actual_thread_num);
  // Compile may read options through OpCompileContext::GetOption, which ultimately depends on thread-local context.
  ThreadPool executor("custom_opt_compile", actual_thread_num, true);
  std::vector<std::future<Status>> vector_future;
  std::unordered_map<CompilableOp *, std::vector<CompileTask *>> compile_tasks_by_op;
  for (auto &task : compile_tasks) {
    compile_tasks_by_op[task.compilable_op_ptr].push_back(&task);
  }
  
  for (auto &compile_task_group : compile_tasks_by_op) {
    std::future<Status> f = executor.commit(CompileCustomOpSerially, &compile_task_group.second);
    if (!f.valid()) {
      GELOGE(FAILED, "[Call][Commit] failed, Future is invalid");
      return FAILED;
    }
    vector_future.emplace_back(std::move(f));
  }
  
  for (auto &f : vector_future) {
    const Status ret = f.get();
    if (ret != SUCCESS) {
      GELOGE(ret, "[Compile][CustomOp] failed");
      return ret;
    }
  }
  GE_TIMESTAMP_END(CustomGraphOptimizer, "CustomOptimizeSubgraphPostProc");
  return SUCCESS;
}
} // namespace ge
