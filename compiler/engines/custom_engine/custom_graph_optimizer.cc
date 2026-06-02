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

#include <memory>
#include <unordered_map>
#include <vector>

#include "custom_op_factory.h"
#include "common/ge_common/ge_types.h"
#include "common/checker.h"
#include "lowering/kernel_run_context_builder.h"
#include "common/compile_profiling/ge_trace_wrapper.h"
#include "common/thread_pool/thread_pool.h"
#include "common/context/local_context.h"
#include "debug/ge_util.h"
#include "platform/platform_info.h"
#include "mmpa/mmpa_api.h"

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

void GetStorageShape(const ge::GeTensorDesc &tensor_desc, gert::StorageShape &storage_shape) {
  const auto &storage_dims = tensor_desc.GetShape().GetDims();
  for (const auto &dim : storage_dims) {
    (void)storage_shape.MutableStorageShape().AppendDim(dim);
  }
  const auto &origin_dims = tensor_desc.GetOriginShape().GetDims();
  for (const auto &dim : origin_dims) {
    (void)storage_shape.MutableOriginShape().AppendDim(dim);
  }
}

std::vector<void *> GetHoldersRawPtr(const std::vector<std::unique_ptr<uint8_t[]>> &holders) {
  std::vector<void *> holder_raw_ptr;
  holder_raw_ptr.reserve(holders.size());
  for (const auto &holder : holders) {
    (void)holder_raw_ptr.emplace_back(holder.get());
  }
  return holder_raw_ptr;
}

ge::Status ConstructCustomCompileContextInputs(const ge::OpDescPtr &op_desc,
                                               std::vector<std::unique_ptr<uint8_t[]>> &inputs) {
  // Compile context inputs layout is input tensors. Tensor 中同时携带 shape、data type、format
  // 等元信息，供编译期自定义算子直接读取。
  for (size_t i = 0UL; i < op_desc->GetAllInputsSize(); ++i) {
    if (op_desc->GetInputDesc(static_cast<uint32_t>(i)).IsValid() != ge::GRAPH_SUCCESS) {
      GELOGD("input desc is not valid, skip add input[%zu] into compile context inputs.", i);
      continue;
    }
    auto input_desc = op_desc->MutableInputDesc(i);
    GE_ASSERT_NOTNULL(input_desc);
    gert::StorageShape storage_shape;
    GetStorageShape(*input_desc, storage_shape);
    std::unique_ptr<uint8_t[]> tensor_holder = ge::ComGraphMakeUnique<uint8_t[]>(sizeof(gert::Tensor));
    GE_ASSERT_NOTNULL(tensor_holder, "Create compile context input tensor holder failed.");
    new (tensor_holder.get())
        gert::Tensor(storage_shape, {input_desc->GetOriginFormat(), input_desc->GetFormat(), {}},
                     input_desc->GetDataType());
    (void)inputs.emplace_back(std::move(tensor_holder));
  }
  return ge::SUCCESS;
}

ge::Status ConstructCustomCompileContextOutputs(const ge::OpDescPtr &op_desc,
                                                std::vector<std::unique_ptr<uint8_t[]>> &outputs) {
  for (size_t i = 0UL; i < op_desc->GetAllOutputsDescSize(); ++i) {
    auto output_desc = op_desc->MutableOutputDesc(i);
    GE_ASSERT_NOTNULL(output_desc);
    gert::StorageShape storage_shape;
    GetStorageShape(*output_desc, storage_shape);
    auto tensor_holder = ge::ComGraphMakeUnique<uint8_t[]>(sizeof(gert::Tensor));
    GE_ASSERT_NOTNULL(tensor_holder, "Create compile context output tensor holder failed.");
    new (tensor_holder.get())
        gert::Tensor(storage_shape, {output_desc->GetOriginFormat(), output_desc->GetFormat(), {}},
                     output_desc->GetDataType());
    (void)outputs.emplace_back(std::move(tensor_holder));
  }
  return ge::SUCCESS;
}

struct CompileTask {
  CompileTask(ge::CompilableOp *op_ptr, std::vector<std::unique_ptr<uint8_t[]>> &&inputs_holder,
              std::vector<std::unique_ptr<uint8_t[]>> &&outputs_holder, gert::KernelContextHolder &&holder,
              std::string name, std::string type)
      : compilable_op_ptr(op_ptr),
        op_compile_inputs_holder(std::move(inputs_holder)),
        op_compile_outputs_holder(std::move(outputs_holder)),
        op_compile_context_holder(std::move(holder)),
        op_name(std::move(name)),
        op_type(std::move(type)) {}

  CompileTask(CompileTask &&) = default;
  CompileTask &operator=(CompileTask &&) = default;
  CompileTask(const CompileTask &) = delete;
  CompileTask &operator=(const CompileTask &) = delete;

  ge::CompilableOp *compilable_op_ptr;
  std::vector<std::unique_ptr<uint8_t[]>> op_compile_inputs_holder;
  std::vector<std::unique_ptr<uint8_t[]>> op_compile_outputs_holder;
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

ge::Status AppendCompileTaskIfNeeded(const ge::NodePtr &node, std::vector<CompileTask> &compile_tasks) {
  const auto op_type = node->GetType();
  const ge::AscendString op_type_ascend(op_type.c_str());
  if (!ge::CustomOpFactory::IsExistOp(op_type_ascend)) {
    return ge::SUCCESS;
  }
  GELOGI("during optimize whole graph, %s is custom op", op_type_ascend.GetString());
  auto *const base_custom_op_ptr = ge::CustomOpFactory::CreateOrGetCustomOp(op_type_ascend);
  if (base_custom_op_ptr == nullptr) {
    GELOGE(ge::FAILED, "[Compile][CustomOp] create custom op failed, op_name:%s, op_type:%s",
           node->GetName().c_str(), op_type.c_str());
    return ge::FAILED;
  }
  auto *const compilable_op_ptr = dynamic_cast<ge::CompilableOp *>(base_custom_op_ptr);
  if (compilable_op_ptr == nullptr) {
    GELOGI("[Compile][CustomOp] custom op did not implement CompilableOp, op_name:%s, op_type:%s",
           node->GetName().c_str(), op_type.c_str());
    return ge::SUCCESS;
  }
  std::vector<std::unique_ptr<uint8_t[]>> op_compile_inputs_holder;
  std::vector<std::unique_ptr<uint8_t[]>> op_compile_outputs_holder;
  GE_ASSERT_SUCCESS(ConstructCustomCompileContextInputs(node->GetOpDesc(), op_compile_inputs_holder));
  GE_ASSERT_SUCCESS(ConstructCustomCompileContextOutputs(node->GetOpDesc(), op_compile_outputs_holder));
  auto op_compile_context_holder = gert::KernelRunContextBuilder()
      .Inputs(GetHoldersRawPtr(op_compile_inputs_holder))
      .Outputs(GetHoldersRawPtr(op_compile_outputs_holder))
      .Build(node->GetOpDesc());
  compile_tasks.emplace_back(compilable_op_ptr, std::move(op_compile_inputs_holder),
                             std::move(op_compile_outputs_holder),
                             std::move(op_compile_context_holder), node->GetName(), op_type);
  return ge::SUCCESS;
}

ge::Status CollectCompileTasks(ge::ComputeGraph &graph, std::vector<CompileTask> &compile_tasks) {
  for (const auto &node : graph.GetAllNodes()) {
    GE_ASSERT_SUCCESS(AppendCompileTaskIfNeeded(node, compile_tasks));
  }
  return ge::SUCCESS;
}

std::unordered_map<ge::CompilableOp *, std::vector<CompileTask *>> GroupCompileTasksByOp(
    std::vector<CompileTask> &compile_tasks) {
  std::unordered_map<ge::CompilableOp *, std::vector<CompileTask *>> compile_tasks_by_op;
  compile_tasks_by_op.reserve(compile_tasks.size());
  for (auto &task : compile_tasks) {
    compile_tasks_by_op[task.compilable_op_ptr].push_back(&task);
  }
  return compile_tasks_by_op;
}

ge::Status CompileCustomOpsInParallel(std::vector<CompileTask> &compile_tasks) {
  if (compile_tasks.empty()) {
    return ge::SUCCESS;
  }
  const auto actual_thread_num = GetThreadNum();
  GELOGI("Custom op compile thread num is %u", actual_thread_num);
  ge::ThreadPool executor("custom_opt_compile", actual_thread_num, true);
  std::vector<std::future<ge::Status>> vector_future;
  auto compile_tasks_by_op = GroupCompileTasksByOp(compile_tasks);
  vector_future.reserve(compile_tasks_by_op.size());
  // The same CompilableOp instance may be shared by multiple nodes, so keep each instance's tasks serialized while
  // still compiling different CompilableOp instances in parallel.
  for (auto &compile_task_group : compile_tasks_by_op) {
    std::future<ge::Status> f = executor.commit(CompileCustomOpSerially, &compile_task_group.second);
    if (!f.valid()) {
      GELOGE(ge::FAILED, "[Call][Commit] failed, Future is invalid");
      return ge::FAILED;
    }
    vector_future.emplace_back(std::move(f));
  }

  for (auto &f : vector_future) {
    const ge::Status ret = f.get();
    if (ret != ge::SUCCESS) {
      GELOGE(ret, "[Compile][CustomOp] failed");
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
  GELOGI("entering optimize whole graph");
  GE_TIMESTAMP_START(CustomGraphOptimizer);
  std::vector<CompileTask> compile_tasks;
  GE_ASSERT_SUCCESS(CollectCompileTasks(graph, compile_tasks));
  GE_ASSERT_SUCCESS(CompileCustomOpsInParallel(compile_tasks));
  GE_TIMESTAMP_END(CustomGraphOptimizer, "CustomOptimizeWholeGraph");
  return SUCCESS;
}

ge::Status CustomGraphOptimizer::GetAttributes(ge::GraphOptimizerAttribute &attrs) const {
  attrs.engineName = kEngineNameCustom;
  return SUCCESS;
}

} // namespace ge
