/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "custom_op_kernel.h"
#include "register/kernel_registry.h"
#include "common/checker.h"
#include "common/plugin/ge_make_unique_util.h"
#include "graph/custom_op.h"
#include "graph/custom_op/cast.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op_registry.h"
#include "kernel/memory/multi_stream_mem_block.h"
#include "graph/def_types.h"
#include "graph/utils/type_utils.h"
#include "exe_graph/runtime/eager_op_execution_context.h"
#include "rt_external_kernel.h"
#include "runtime/v2/engine/custom/kernel/eager_args_handler.h"

namespace gert {
namespace kernel {
namespace {
// 自定义算子特有的输入，从 AdditionalInputIndex::kNum 开始
enum class CustomOpInput { kFunc = static_cast<uint32_t>(EagerOpExecutionContext::AdditionalInputIndex::kNum), kEnd };

std::string PrintNodeType(const KernelContext *context) {
  std::stringstream ss;
  auto extend_context = reinterpret_cast<const ExtendedKernelContext *>(context);
  auto compute_node_info = extend_context->GetComputeNodeInfo();
  if (compute_node_info == nullptr) {
    return "compute_node_info is nullptr";
  }
  ss << "node_type[" << compute_node_info->GetNodeType() << "]";
  return ss.str();
}

void ShapeToStringStream(std::stringstream &ss, const Shape &shape) {
  ss << "[";
  for (size_t j = 0U; j < shape.GetDimNum(); ++j) {
    ss << shape.GetDim(j);
    if (j + 1U < shape.GetDimNum()) {
      ss << ", ";
    }
  }
  ss << "]";
}

void PrintTensor(std::stringstream &ss, const gert::Tensor *tensor) {
  ss << "format: ";
  ss << ge::TypeUtils::FormatToSerialString(tensor->GetStorageFormat()) << ", ";
  ss << "datatype: ";
  ss << ge::TypeUtils::DataTypeToSerialString(tensor->GetDataType()) << ", ";
  ss << "shape: ";
  ShapeToStringStream(ss, tensor->GetStorageShape());
  ss << ", addr: " << ge::PtrToValue(tensor->GetAddr());
}
}  // namespace

// call after kernel launch
std::string PrintStreamIdAndTaskId() {
  std::stringstream ss;
  uint32_t stream_id = 0U;
  uint32_t flip_task_id = 0U;
  if (rtGetTaskIdAndStreamID(&flip_task_id, &stream_id) == RT_ERROR_NONE) {
    const uint32_t task_id = flip_task_id & 0xFFFFU;  // lower 16bits
    const uint32_t flip_num = flip_task_id >> 16U;    // high 16bits
    ss << "stream_id=" << stream_id << ", task_id=" << task_id << ", flip_num=" << flip_num
       << ", flip_task_id=" << flip_task_id;
  }
  return ss.str();
}

ge::graphStatus FindCustomOpFunc(KernelContext *context) {
  const char *node_type = context->GetInputValue<char *>(0);
  GE_ASSERT_NOTNULL(node_type, "Failed to find custom op func, node type is nullptr");
  auto custom_op_registry = context->GetInputValue<ge::CustomOpRegistry *>(1);
  GE_ASSERT_NOTNULL(custom_op_registry, "Failed to find custom op func, custom op registry is nullptr.");
  ge::BaseCustomOp *custom_op_ptr = custom_op_registry->CreateOrGetCustomOp(node_type);
  GE_ASSERT_NOTNULL(custom_op_ptr, "Failed to find custom op func for op type %s in custom op registry.", node_type);
  auto chain = context->GetOutput(0);
  GE_ASSERT_NOTNULL(chain);
  chain->Set(custom_op_ptr, nullptr);
  return ge::GRAPH_SUCCESS;
}

static ge::graphStatus CreateOutputTensors(const ExtendedKernelContext *extended_kernel_context,
                                           KernelContext *context) {
  const size_t node_output_num = extended_kernel_context->GetComputeNodeOutputNum();
  for (size_t index = 0; index < node_output_num; index++) {
    auto chain = context->GetOutput(index);
    GE_ASSERT_NOTNULL(chain);
    auto output_desc = extended_kernel_context->GetOutputDesc(index);
    GE_ASSERT_NOTNULL(output_desc);
    chain->SetWithDefaultDeleter(new (std::nothrow)
                                     Tensor(StorageShape(), output_desc->GetFormat(), output_desc->GetDataType()));
  }
  return ge::GRAPH_SUCCESS;
}

static ge::graphStatus CreateWorkspaceHolder(KernelContext *context, size_t node_output_num) {
  auto workspace_memory_av = context->GetOutput(node_output_num);
  GE_ASSERT_NOTNULL(workspace_memory_av);
  auto workspace_memory_holder = new (std::nothrow) std::vector<memory::MultiStreamMemBlock *>();
  GE_ASSERT_NOTNULL(workspace_memory_holder);
  // 节省MallocWorkspace时vector添加元素时动态扩容的开销
  workspace_memory_holder->reserve(1U);
  workspace_memory_av->SetWithDefaultDeleter(workspace_memory_holder);
  return ge::GRAPH_SUCCESS;
}

static ge::graphStatus CreateCustomOpOutputs(const ge::FastNode *node, KernelContext *context) {
  (void)node;
  auto *extended_kernel_context = reinterpret_cast<ExtendedKernelContext *>(context);
  GE_ASSERT_NOTNULL(extended_kernel_context);
  const size_t node_output_num = extended_kernel_context->GetComputeNodeOutputNum();
  GE_ASSERT_SUCCESS(CreateOutputTensors(extended_kernel_context, context));
  GE_ASSERT_SUCCESS(CreateWorkspaceHolder(context, node_output_num));

  // allocator 在 Create 阶段尚未就绪（Init 图未执行），创建空 EagerArgsHandler，
  // 在 RunFunc 阶段初始化
  auto args_handler = ge::MakeUnique<EagerArgsHandler>();
  GE_ASSERT_NOTNULL(args_handler);

  auto *args_output = context->GetOutput(
      node_output_num + static_cast<size_t>(EagerOpExecutionContext::AdditionalOutputIndex::kArgsHandler));
  GE_ASSERT_NOTNULL(args_output);
  args_output->SetWithDefaultDeleter(static_cast<ArgsHandler *>(args_handler.release()));

  return ge::GRAPH_SUCCESS;
}

static ge::graphStatus CopyShapeFromTemplateTensors(KernelContext *context, size_t node_input_num,
                                                    size_t node_output_num) {
  const size_t template_tensor_start = node_input_num + static_cast<size_t>(CustomOpInput::kEnd);
  for (size_t index = 0; index < node_output_num; ++index) {
    auto template_tensor = context->GetInputPointer<Tensor>(template_tensor_start + index);
    auto output_tensor = context->GetOutputPointer<Tensor>(index);
    GE_ASSERT_NOTNULL(template_tensor);
    GE_ASSERT_NOTNULL(output_tensor);
    output_tensor->MutableStorageShape() = template_tensor->GetStorageShape();
    output_tensor->MutableOriginShape() = template_tensor->GetOriginShape();
  }
  return ge::GRAPH_SUCCESS;
}

static ge::graphStatus ExecuteCustomOpImpl(KernelContext *context) {
  auto *eager_context = reinterpret_cast<EagerOpExecutionContext *>(context);
  GE_ASSERT_NOTNULL(eager_context);
  const size_t node_input_num = eager_context->GetComputeNodeInputNum();
  const size_t node_output_num = eager_context->GetComputeNodeOutputNum();
  auto *chain = context->GetOutput(node_output_num +
                                   static_cast<size_t>(EagerOpExecutionContext::AdditionalOutputIndex::kArgsHandler));
  GE_ASSERT_NOTNULL(chain);
  auto *args_handler = static_cast<EagerArgsHandler *>(chain->GetValue<ArgsHandler *>());
  GE_ASSERT_NOTNULL(args_handler);
  if (!args_handler->IsInitialized()) {
    auto *allocator = context->GetInputValue<GertAllocator *>(
        node_input_num + static_cast<size_t>(EagerOpExecutionContext::AdditionalInputIndex::kDeviceAllocator));
    GE_ASSERT_NOTNULL(allocator);
    auto stream_id = allocator->GetStreamId();
    args_handler->Initialize(allocator, stream_id);
    GELOGD("EagerArgsHandler initialized in RunFunc with allocator %p", allocator);
  }

  auto custom_op_ptr =
      context->GetInputValue<ge::BaseCustomOp *>(node_input_num + static_cast<size_t>(CustomOpInput::kFunc));
  GE_ASSERT_NOTNULL(custom_op_ptr);
  auto *eager_execute_op_ptr = ge::CustomOpCast<ge::EagerExecuteOp>(custom_op_ptr);
  if (eager_execute_op_ptr == nullptr) {
    GELOGE(ge::FAILED, "%s is custom op but did not implement EagerExecuteOp", eager_context->GetNodeType());
    return ge::GRAPH_FAILED;
  }
  GE_ASSERT_SUCCESS(eager_execute_op_ptr->Execute(reinterpret_cast<EagerOpExecutionContext *>(context)));
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus ExecuteCustomOpFunc(KernelContext *context) {
  return ExecuteCustomOpImpl(context);
}

ge::graphStatus ExecuteCustomOpWithInferShapeFunc(KernelContext *context) {
  auto *eager_context = reinterpret_cast<EagerOpExecutionContext *>(context);
  GE_ASSERT_NOTNULL(eager_context);
  GE_ASSERT_SUCCESS(CopyShapeFromTemplateTensors(context, eager_context->GetComputeNodeInputNum(),
                                                 eager_context->GetComputeNodeOutputNum()));
  return ExecuteCustomOpImpl(context);
}

ge::graphStatus FreeCustomOpWorkspacesFunc(KernelContext *context) {
  auto memory_vec = context->MutableInputPointer<std::vector<GertMemBlock *>>(0);
  GE_ASSERT_NOTNULL(memory_vec);
  auto gert_allocator = context->GetInputValue<GertAllocator *>(1);
  GE_ASSERT_NOTNULL(gert_allocator);
  auto stream_id = gert_allocator->GetStreamId();
  for (size_t i = 0UL; i < memory_vec->size(); i++) {
    if (memory_vec->at(i) != nullptr) {
      memory_vec->at(i)->Free(stream_id);
    }
  }
  memory_vec->clear();
  return ge::GRAPH_SUCCESS;
}

ge::graphStatus FreeArgsGuarderFunc(KernelContext *context) {
  auto *handler_base = context->GetInputValue<ArgsHandler *>(0);
  if (handler_base != nullptr) {
    static_cast<EagerArgsHandler *>(handler_base)->Release();
  }
  return ge::GRAPH_SUCCESS;
}

static std::vector<std::string> CustomOpExecuteKernelTrace(const KernelContext *context) {
  auto extend_context = reinterpret_cast<const ExtendedKernelContext *>(context);
  auto compute_node_info = extend_context->GetComputeNodeInfo();
  if (compute_node_info == nullptr) {
    return {PrintNodeType(context), "compute_node_info is nullptr"};
  }
  auto *eager_op_context = reinterpret_cast<const EagerOpExecutionContext *>(context);
  std::stringstream input_tensor_ss;
  input_tensor_ss << "input tensor: ";
  for (size_t i = 0U; i < compute_node_info->GetInputsNum(); ++i) {
    auto tensor = eager_op_context->GetInputTensor(i);
    if (tensor == nullptr) {
      return {PrintNodeType(context), "The " + std::to_string(i) + "th's input tensor is nullptr"};
    }
    input_tensor_ss << "[" << i << "]: [";
    PrintTensor(input_tensor_ss, tensor);
    input_tensor_ss << "]";
    if ((i + 1U) < compute_node_info->GetInputsNum()) {
      input_tensor_ss << ", ";
    }
  }
  std::stringstream output_tensor_ss;
  output_tensor_ss << "output tensor: ";
  for (size_t i = 0U; i < compute_node_info->GetOutputsNum(); ++i) {
    auto tensor = eager_op_context->GetOutputTensor(i);
    if (tensor == nullptr) {
      return {PrintNodeType(context), "The " + std::to_string(i) + "th's output tensor is nullptr"};
    }
    output_tensor_ss << "[" << i << "]: [";
    PrintTensor(output_tensor_ss, tensor);
    output_tensor_ss << "]";
    if ((i + 1U) < compute_node_info->GetOutputsNum()) {
      output_tensor_ss << ", ";
    }
  }
  return {PrintNodeType(context), input_tensor_ss.str(), output_tensor_ss.str(), PrintStreamIdAndTaskId()};
}

ge::graphStatus CustomOpProfilingDataFill(const KernelContext *context, ProfilingInfoWrapper &prof_info) {
  prof_info.SetBlockDim(std::numeric_limits<uint32_t>::max());
  auto extend_context = reinterpret_cast<const ExtendedKernelContext *>(context);
  auto compute_node_info = extend_context->GetComputeNodeInfo();
  GE_ASSERT_NOTNULL(compute_node_info);
  auto node_input_num = compute_node_info->GetInputsNum();
  const auto eager_context = reinterpret_cast<const EagerOpExecutionContext *>(context);
  GE_ASSERT_NOTNULL(eager_context);
  std::vector<std::vector<int64_t>> input_shapes;
  for (size_t i = 0UL; i < node_input_num; i++) {
    auto tensor = eager_context->GetInputTensor(i);
    GE_ASSERT_NOTNULL(tensor);
    auto shape = tensor->GetStorageShape();
    std::vector<int64_t> dims;
    for (size_t j = 0U; j < shape.GetDimNum(); ++j) {
      dims.emplace_back(shape.GetDim(j));
    }
    input_shapes.emplace_back(dims);
  }
  auto node_output_num = compute_node_info->GetOutputsNum();
  std::vector<std::vector<int64_t>> output_shapes;
  for (size_t i = 0UL; i < node_output_num; i++) {
    auto tensor = eager_context->GetOutputTensor(i);
    GE_ASSERT_NOTNULL(tensor);
    auto shape = tensor->GetStorageShape();
    std::vector<int64_t> dims;
    for (size_t j = 0U; j < shape.GetDimNum(); ++j) {
      dims.emplace_back(shape.GetDim(j));
    }
    output_shapes.emplace_back(dims);
  }
  GE_ASSERT_SUCCESS(prof_info.FillShapeInfo(input_shapes, output_shapes));
  return ge::GRAPH_SUCCESS;
}

REGISTER_KERNEL(FindCustomOp).RunFunc(FindCustomOpFunc);
REGISTER_KERNEL(ExecuteCustomOp)
    .OutputsCreator(CreateCustomOpOutputs)
    .RunFunc(ExecuteCustomOpFunc)
    .TracePrinter(CustomOpExecuteKernelTrace)
    .ProfilingInfoFiller(CustomOpProfilingDataFill);
REGISTER_KERNEL(ExecuteCustomOpWithInferShape)
    .OutputsCreator(CreateCustomOpOutputs)
    .RunFunc(ExecuteCustomOpWithInferShapeFunc)
    .TracePrinter(CustomOpExecuteKernelTrace)
    .ProfilingInfoFiller(CustomOpProfilingDataFill);
REGISTER_KERNEL(FreeCustomOpWorkspaces).RunFunc(FreeCustomOpWorkspacesFunc);
REGISTER_KERNEL(FreeArgsGuarder).RunFunc(FreeArgsGuarderFunc);
}  // namespace kernel
}  // namespace gert
