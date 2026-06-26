/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

#include "common/om2/codegen/om2_codegen_utils.h"
#include "common/om2/codegen/om2_codegen_model_builder.h"
#include "common/om2/codegen/emitter/cpp_emitter.h"
#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/ast/ast_context.h"
#include "common/om2/codegen/task_code_builder/fe/fusion_end_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/fe/fusion_start_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/fe/kernel_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/rts/end_graph_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/rts/memcpy_async_task_code_builder.h"
#include "common/om2/codegen/task_code_builder/rts/stream_switch_task_code_builder.h"
#include "framework/common/taskdown_common.h"
#include "ge_runtime_stub/include/common/share_graph.h"
#include "ge_runtime_stub/include/faker/aicore_taskdef_faker.h"
#include "ge_runtime_stub/include/faker/aicpu_taskdef_faker.h"
#include "ge_runtime_stub/include/faker/ge_model_builder.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/tensor_utils.h"

namespace ge {
namespace {
AstBuildContext &GetOm2CodegenModelBuilderUtAst() {
  static AstContext ast_ctx;
  static AstBuildContext ast(ast_ctx);
  return ast;
}

template <typename T, typename = void>
struct HasIsAllKernelMember : std::false_type {};

template <typename T>
struct HasIsAllKernelMember<T, std::void_t<decltype(std::declval<T &>().is_all_kernel)>> : std::true_type {};

template <typename T, typename = void>
struct HasIsAicpuMember : std::false_type {};

template <typename T>
struct HasIsAicpuMember<T, std::void_t<decltype(std::declval<T &>().is_aicpu)>> : std::true_type {};

template <typename T, typename = void>
struct HasLaunchKernelConfigAssembler : std::false_type {};

template <typename T>
struct HasLaunchKernelConfigAssembler<T, std::void_t<decltype(&T::AssembleLaunchKernelConfig)>> : std::true_type {};

template <typename NodeT>
void EmitCodeFromNodes(const std::vector<NodeT *> &nodes, std::stringstream &output) {
  CppEmitter emitter;
  for (const auto *node : nodes) {
    if (node != nullptr) {
      std::string code_content;
      ASSERT_EQ(node->Accept(emitter, code_content), SUCCESS);
      output << code_content << '\n';
    }
  }
}

std::string EmitCodeFromBodyItems(const std::vector<BodyItem> &items) {
  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  const auto nodes = ast.Body(items);
  std::stringstream output;
  EmitCodeFromNodes(nodes, output);
  return output.str();
}

template <typename T>
T *FindTaskBuilder(const std::vector<TaskCodeBuilderPtr> &task_builders) {
  for (const auto &task_builder : task_builders) {
    auto *builder = dynamic_cast<T *>(task_builder.get());
    if (builder != nullptr) {
      return builder;
    }
  }
  return nullptr;
}

static void SyncKernelNameFromOpDesc(const GeModelPtr &ge_model) {
  auto model_task_def = ge_model->GetModelTaskDefPtr();
  if (model_task_def == nullptr) {
    return;
  }
  const auto &graph = ge_model->GetGraph();
  if (graph == nullptr) {
    return;
  }
  for (int i = 0; i < model_task_def->task_size(); ++i) {
    auto *task_def = model_task_def->mutable_task(i);
    for (const auto &node : graph->GetDirectNode()) {
      auto op_desc = node->GetOpDesc();
      if (op_desc == nullptr) {
        continue;
      }
      std::string kernel_name;
      if (ge::AttrUtils::GetStr(op_desc, "_kernelname", kernel_name)) {
        task_def->mutable_kernel()->set_kernel_name(kernel_name);
      }
    }
  }
}

static void SyncKernelNameForAllModels(const GeRootModelPtr &ge_root_model) {
  if (ge_root_model == nullptr) {
    return;
  }
  for (const auto &kv : ge_root_model->GetSubgraphInstanceNameToModel()) {
    SyncKernelNameFromOpDesc(kv.second);
  }
}

GeRootModelPtr CreateGeRootModelWithAicoreOp() {
  auto graph = gert::ShareGraph::AicoreStaticGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder
          .AddTaskDef(
              "Add",
              gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i_instance0*}{i_instance1*}{o_instance0*}{ws0*}"))
          .FakeTbeBin({"Add"})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->AppendIrInput("x1", kIrInputRequired);
      op_desc->AppendIrInput("x2", kIrInputRequired);
      op_desc->AppendIrOutput("y", kIrOutputRequired);
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      op_desc->SetWorkspaceBytes(std::vector<int64_t>(1, 64));
      op_desc->SetWorkspace(std::vector<int64_t>(1, 0));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithAicoreOp2() {
  auto graph = gert::ShareGraph::AicoreStaticGraph();
  graph->TopologicalSorting();
  for (const auto &node : graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() == "Add")) {
      op_desc->AppendIrInput("x1", kIrInputRequired);
      op_desc->AppendIrInput("x2", kIrInputRequired);
      op_desc->AppendIrOutput("y", kIrOutputRequired);
    }
  }
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder.AddTaskDef("Add", gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i0*}{i1*}{o0*}{ws0*}"))
          .FakeTbeBin({"Add"})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      op_desc->SetWorkspaceBytes(std::vector<int64_t>(1, 64));
      op_desc->SetWorkspace(std::vector<int64_t>(1, 0));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithAtomicAicoreOp() {
  const std::string args_format = "{i_instance0*}{i_instance1*}{o_instance0*}{ws0*}";
  auto graph = gert::ShareGraph::AicoreStaticGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder
          .AddTaskDef("Add",
                      gert::AiCoreTaskDefFaker("add_stub").AtomicStubNum("atomic_add_stub").ArgsFormat(args_format))
          .FakeTbeBin({gert::GeModelBuilder::TbeConfig("Add", true)})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  OpDescPtr add_desc = nullptr;
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      if (op_desc->GetType() == "Add") {
        op_desc->AppendIrInput("x1", kIrInputRequired);
        op_desc->AppendIrInput("x2", kIrInputRequired);
        op_desc->AppendIrOutput("y", kIrOutputRequired);
        (void)AttrUtils::SetBool(op_desc, ATTR_NAME_NEED_GENTASK_ATOMIC, true);
        (void)AttrUtils::SetStr(op_desc, kAtomicPrefix + TVM_ATTR_NAME_MAGIC, "RT_DEV_BINARY_MAGIC_ELF_AIVEC");
        add_desc = op_desc;
      }
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      op_desc->SetWorkspaceBytes(std::vector<int64_t>(1, 64));
      op_desc->SetWorkspace(std::vector<int64_t>(1, 0));
    }
  }

  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if ((add_desc == nullptr) || name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  const auto kernel_name_ptr = AttrUtils::GetStr(add_desc, "_kernelname");
  const auto atomic_kernel_name_ptr = AttrUtils::GetStr(add_desc, ATOMIC_ATTR_TBE_KERNEL_NAME);
  if ((model_task_def == nullptr) || (model_task_def->task_size() < 2) || (kernel_name_ptr == nullptr) ||
      (atomic_kernel_name_ptr == nullptr)) {
    return nullptr;
  }
  model_task_def->mutable_task(0)->mutable_kernel()->set_kernel_name(*atomic_kernel_name_ptr);
  model_task_def->mutable_task(0)->mutable_kernel()->mutable_context()->set_args_format(args_format);
  model_task_def->mutable_task(1)->mutable_kernel()->set_kernel_name(*kernel_name_ptr);
  model_task_def->mutable_task(1)->mutable_kernel()->mutable_context()->set_args_format(args_format);

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithConstInputOp() {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  const auto compute_graph = ge_model->GetGraph();
  if (compute_graph == nullptr) {
    return nullptr;
  }
  std::vector<uint8_t> weights_value(256 * 1024, 0);
  const size_t weight_size = weights_value.size();
  ge_model->SetWeight(Buffer::CopyFrom(weights_value.data(), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc == nullptr) || (op_desc->GetType() != "Add")) {
      continue;
    }
    op_desc->SetIsInputConst({true, false});
    auto input_desc = op_desc->MutableInputDesc(0);
    if (input_desc == nullptr) {
      return nullptr;
    }
    (void)TensorUtils::SetDataOffset(*input_desc, 128);
  }
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithAicoreOpOfDynamicIo(
    const std::string &args_format = "{i_desc0}{i_desc1}{o_desc0}") {
  auto graph = std::make_shared<ComputeGraph>("g1");
  GeTensorDesc tensor_desc(GeShape({1, 1, 224, 224}), FORMAT_NCHW, DT_FLOAT);
  auto data_x_desc = std::make_shared<OpDesc>("data_x", DATA);
  (void)data_x_desc->AddOutputDesc(tensor_desc);
  auto data_x = graph->AddNode(data_x_desc);
  auto data_dx_desc = std::make_shared<OpDesc>("data_dx", DATA);
  (void)data_dx_desc->AddOutputDesc(tensor_desc);
  auto data_dx = graph->AddNode(data_dx_desc);
  auto op_desc = std::make_shared<OpDesc>("add1", "Add");
  (void)op_desc->AddInputDesc("x", tensor_desc);
  (void)op_desc->AddDynamicInputDesc("dx", 1);
  (void)op_desc->AddDynamicOutputDesc("dy", 1);
  op_desc->AppendIrInput("x", kIrInputRequired);
  op_desc->AppendIrInput("dx", kIrInputDynamic);
  op_desc->AppendIrOutput("dy", kIrOutputDynamic);
  if (op_desc->GetInputsSize() > 1U) {
    (void)op_desc->UpdateInputDesc(1U, tensor_desc);
  }
  if (op_desc->GetOutputsSize() > 0U) {
    (void)op_desc->UpdateOutputDesc(0U, tensor_desc);
  }
  auto dynamic_node = graph->AddNode(op_desc);
  auto netoutput_desc = std::make_shared<OpDesc>("netoutput", NETOUTPUT);
  (void)netoutput_desc->AddInputDesc(tensor_desc);
  auto netoutput = graph->AddNode(netoutput_desc);
  if ((data_x == nullptr) || (data_dx == nullptr) || (dynamic_node == nullptr) || (netoutput == nullptr)) {
    return nullptr;
  }
  GraphUtils::AddEdge(data_x->GetOutDataAnchor(0), dynamic_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(data_dx->GetOutDataAnchor(0), dynamic_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(dynamic_node->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0));
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model = builder.AddTaskDef("Add", gert::AiCoreTaskDefFaker("add_stub").ArgsFormat(args_format))
                           .FakeTbeBin({"Add"})
                           .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithConstInputsInTaskOrder() {
  auto graph = gert::ShareGraph::Aicpu4thGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  gert::AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto ge_root_model = builder.AddTaskDef("add2", aicpu_task_def_faker.SetNeedMemcpy(false))
                           .AddTaskDef("add1", aicpu_task_def_faker.SetNeedMemcpy(false))
                           .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      if (node->GetName() == "add1") {
        op_desc->SetIsInputConst({true, false});
        auto input_desc = op_desc->MutableInputDesc(0);
        if (input_desc == nullptr) {
          return nullptr;
        }
        (void)TensorUtils::SetDataOffset(*input_desc, 128);
      } else if (node->GetName() == "add2") {
        op_desc->SetIsInputConst({true, false});
        auto input_desc = op_desc->MutableInputDesc(0);
        if (input_desc == nullptr) {
          return nullptr;
        }
        (void)TensorUtils::SetDataOffset(*input_desc, 256);
      }
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if (model_task_def != nullptr) {
    for (int32_t i = 0; i < model_task_def->task_size(); ++i) {
      auto *task_def = model_task_def->mutable_task(i);
      if ((task_def != nullptr) && task_def->has_kernel()) {
        task_def->mutable_kernel()->mutable_context()->set_kernel_type(6U);
      }
    }
  }
  std::vector<uint64_t> weights_value(64, 1024);
  size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  std::vector<uint8_t> expanded_weights(256 * 1024, 0);
  weight_size = expanded_weights.size();
  ge_model->SetWeight(Buffer::CopyFrom(expanded_weights.data(), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithAicpuOp() {
  auto graph = gert::ShareGraph::Aicpu4thGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  gert::AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto ge_root_model = builder.AddTaskDef("add1", aicpu_task_def_faker.SetNeedMemcpy(false))
                           .AddTaskDef("add2", aicpu_task_def_faker.SetNeedMemcpy(false))
                           .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if (model_task_def != nullptr) {
    for (int32_t i = 0; i < model_task_def->task_size(); ++i) {
      auto *task_def = model_task_def->mutable_task(i);
      if ((task_def != nullptr) && task_def->has_kernel()) {
        task_def->mutable_kernel()->mutable_context()->set_kernel_type(6U);
      }
    }
  }
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithTfAicpuOp() {
  auto graph = gert::ShareGraph::Aicpu4thGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  gert::AiCpuTfTaskDefFaker tf_aicpu_task_def_faker;
  gert::AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto ge_root_model = builder.AddTaskDef("add1", tf_aicpu_task_def_faker)
                           .AddTaskDef("add2", aicpu_task_def_faker.SetNeedMemcpy(false))
                           .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if (model_task_def != nullptr) {
    for (int32_t i = 0; i < model_task_def->task_size(); ++i) {
      auto *task_def = model_task_def->mutable_task(i);
      if ((task_def != nullptr) && task_def->has_kernel()) {
        task_def->mutable_kernel()->mutable_context()->set_kernel_type(6U);
      }
    }
  }
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithAicoreChainOp() {
  auto graph = gert::ShareGraph::Aicpu4thGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder
          .AddTaskDef(
              "add1",
              gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i_instance0*}{i_instance1*}{o_instance0*}{ws0*}"))
          .AddTaskDef(
              "add2",
              gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i_instance0*}{i_instance1*}{o_instance0*}{ws0*}"))
          .FakeTbeBin({"Add"})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->AppendIrInput("x1", kIrInputRequired);
      op_desc->AppendIrInput("x2", kIrInputRequired);
      op_desc->AppendIrOutput("y", kIrOutputRequired);
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      op_desc->SetWorkspaceBytes(std::vector<int64_t>(1, 64));
      op_desc->SetWorkspace(std::vector<int64_t>(1, 0));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeModelPtr CreateGeModelWithMemcpyAsyncTask() {
  auto graph = gert::ShareGraph::AicoreStaticGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder
          .AddTaskDef(
              "Add",
              gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i_instance0*}{i_instance1*}{o_instance0*}{ws0*}"))
          .FakeTbeBin({"Add"})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();
  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->AppendIrInput("x1", kIrInputRequired);
      op_desc->AppendIrInput("x2", kIrInputRequired);
      op_desc->AppendIrOutput("y", kIrOutputRequired);
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      op_desc->SetWorkspaceBytes(std::vector<int64_t>(1, 64));
      op_desc->SetWorkspace(std::vector<int64_t>(1, 0));
    }
  }

  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  auto *memcpy_task = model_task_def->add_task();
  if (memcpy_task == nullptr) {
    return nullptr;
  }
  memcpy_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC));
  memcpy_task->set_stream_id(0U);
  auto *memcpy_async_def = memcpy_task->mutable_memcpy_async();
  const uint64_t logic_mem_base = 0U;
  memcpy_async_def->set_op_index(2);
  memcpy_async_def->set_src(logic_mem_base + 1024);
  memcpy_async_def->set_dst(logic_mem_base + 2048);
  memcpy_async_def->set_dst_max(1024U);
  memcpy_async_def->set_count(512U);
  memcpy_async_def->set_kind(1U);

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_model;
}

GeModelPtr CreateGeModelWithStreamSwitchTask() {
  auto graph = std::make_shared<ComputeGraph>("g1");
  GeTensorDesc tensor_desc(GeShape({1, 4, 4, 8}), FORMAT_NCHW, DT_FLOAT);
  TensorUtils::SetSize(tensor_desc, 512U);

  auto data0_desc = std::make_shared<OpDesc>("data0", DATA);
  (void)data0_desc->AddOutputDesc(tensor_desc);
  auto data0 = graph->AddNode(data0_desc);

  auto data1_desc = std::make_shared<OpDesc>("data1", DATA);
  (void)data1_desc->AddOutputDesc(tensor_desc);
  auto data1 = graph->AddNode(data1_desc);

  auto stream_switch_desc = std::make_shared<OpDesc>("stream_switch", "StreamSwitch");
  (void)stream_switch_desc->AddInputDesc("pred", tensor_desc);
  (void)stream_switch_desc->AddInputDesc("value", tensor_desc);
  stream_switch_desc->SetInputOffset({1024, 2048});
  (void)AttrUtils::SetInt(stream_switch_desc, ATTR_NAME_STREAM_SWITCH_COND, 1);
  (void)AttrUtils::SetListInt(stream_switch_desc, ATTR_NAME_ACTIVE_STREAM_LIST, {1});
  (void)AttrUtils::SetInt(stream_switch_desc, ATTR_NAME_SWITCH_DATA_TYPE, 0);
  auto stream_switch = graph->AddNode(stream_switch_desc);

  if ((data0 == nullptr) || (data1 == nullptr) || (stream_switch == nullptr)) {
    return nullptr;
  }
  GraphUtils::AddEdge(data0->GetOutDataAnchor(0), stream_switch->GetInDataAnchor(0));
  GraphUtils::AddEdge(data1->GetOutDataAnchor(0), stream_switch->GetInDataAnchor(1));
  graph->TopologicalSorting();
  graph->SetGraphUnknownFlag(false);

  data0_desc->SetOutputOffset({1024});
  data1_desc->SetOutputOffset({2048});

  auto ge_model = MakeShared<GeModel>();
  if (ge_model == nullptr) {
    return nullptr;
  }
  ge_model->SetGraph(graph);

  const auto model_def = MakeShared<domi::ModelTaskDef>();
  if (model_def == nullptr) {
    return nullptr;
  }
  ge_model->SetModelTaskDef(model_def);

  auto *task_def = model_def->add_task();
  if (task_def == nullptr) {
    return nullptr;
  }
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_STREAM_SWITCH));
  task_def->set_stream_id(0U);
  task_def->mutable_stream_switch()->set_op_index(static_cast<uint32_t>(stream_switch_desc->GetId()));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, 0);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 2);
  return ge_model;
}

GeRootModelPtr CreateGeRootModelWithSimpleTasksAndStub() {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if ((model_task_def == nullptr) || (model_task_def->task_size() == 0)) {
    return nullptr;
  }

  auto *fusion_start_task = model_task_def->add_task();
  auto *fusion_end_task = model_task_def->add_task();
  auto *end_graph_task = model_task_def->add_task();
  if ((fusion_start_task == nullptr) || (fusion_end_task == nullptr) || (end_graph_task == nullptr)) {
    return nullptr;
  }

  fusion_start_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_FUSION_START));
  fusion_start_task->set_stream_id(1U);

  fusion_end_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_FUSION_END));
  fusion_end_task->set_stream_id(2U);

  end_graph_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_END_GRAPH));
  end_graph_task->set_stream_id(3U);

  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithRuntimeAttrs() {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_NOTIFY_NUM, 2);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_EVENT_NUM, 3);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_LABEL_NUM, 4);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithoutDataAttrIndex() {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  const auto compute_graph = ge_model->GetGraph();
  if (compute_graph == nullptr) {
    return nullptr;
  }
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc == nullptr) || (op_desc->GetType() != DATA)) {
      continue;
    }
    (void)op_desc->DelAttr(ATTR_NAME_INDEX);
    break;
  }
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithReorderedDataAttrIndex() {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  const auto compute_graph = ge_model->GetGraph();
  if (compute_graph == nullptr) {
    return nullptr;
  }
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc == nullptr) || (op_desc->GetType() != DATA)) {
      continue;
    }
    if (node->GetName() == "data1") {
      (void)AttrUtils::SetInt(op_desc, ATTR_NAME_INDEX, 1);
      op_desc->SetOutputOffset({2048});
    } else if (node->GetName() == "data2") {
      (void)AttrUtils::SetInt(op_desc, ATTR_NAME_INDEX, 0);
      op_desc->SetOutputOffset({1024});
    }
  }
  return ge_root_model;
}

class CustomFusionStartTaskCodeBuilder : public FusionStartTaskCodeBuilder {
 public:
  using FusionStartTaskCodeBuilder::FusionStartTaskCodeBuilder;

  Status Contribute(TaskSemanticContributeContext &context) override {
    contribute_called_ = true;
    return FusionStartTaskCodeBuilder::Contribute(context);
  }

  bool WasContributeCalled() const {
    return contribute_called_;
  }

  void SetSemanticStreamIdForTest(const uint32_t stream_id) {
    header_.stream_id = stream_id;
  }

 private:
  bool contribute_called_{false};
};

class CustomFusionEndTaskCodeBuilder : public FusionEndTaskCodeBuilder {
 public:
  using FusionEndTaskCodeBuilder::FusionEndTaskCodeBuilder;

  void SetSemanticStreamIdForTest(const uint32_t stream_id) {
    header_.stream_id = stream_id;
  }
};

class CustomEndGraphTaskCodeBuilder : public EndGraphTaskCodeBuilder {
 public:
  using EndGraphTaskCodeBuilder::EndGraphTaskCodeBuilder;

  void SetSemanticStreamIdForTest(const uint32_t stream_id) {
    header_.stream_id = stream_id;
  }
};

// 构造 NoTask ConcatD 输出复用图：
// data0 -> add0 -> ConcatD(NoTask) -> NetOutput
// data1 -> add1 -^
// add0/add1 输出连续，对应 NetOutput 基址 1024。
GeRootModelPtr CreateGeRootModelWithNoTaskConcatOutput() {
  auto graph = std::make_shared<ComputeGraph>("g1");
  GeTensorDesc tensor_desc(GeShape({1, 32}), FORMAT_ND, DT_FLOAT);

  auto data0_desc = std::make_shared<OpDesc>("data0", DATA);
  (void)data0_desc->AddOutputDesc(tensor_desc);
  auto data0 = graph->AddNode(data0_desc);

  auto data1_desc = std::make_shared<OpDesc>("data1", DATA);
  (void)data1_desc->AddOutputDesc(tensor_desc);
  auto data1 = graph->AddNode(data1_desc);

  auto add0_desc = std::make_shared<OpDesc>("add0", "Add");
  (void)add0_desc->AddInputDesc("x", tensor_desc);
  (void)add0_desc->AddOutputDesc("y", tensor_desc);
  add0_desc->AppendIrInput("x", kIrInputRequired);
  add0_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add0 = graph->AddNode(add0_desc);

  auto add1_desc = std::make_shared<OpDesc>("add1", "Add");
  (void)add1_desc->AddInputDesc("x", tensor_desc);
  (void)add1_desc->AddOutputDesc("y", tensor_desc);
  add1_desc->AppendIrInput("x", kIrInputRequired);
  add1_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add1 = graph->AddNode(add1_desc);

  auto concat_desc = std::make_shared<OpDesc>("concat_notask", "ConcatD");
  (void)concat_desc->AddInputDesc("x0", tensor_desc);
  (void)concat_desc->AddInputDesc("x1", tensor_desc);
  GeTensorDesc out_tensor_desc(GeShape({1, 64}), FORMAT_ND, DT_FLOAT);
  (void)concat_desc->AddOutputDesc("y", out_tensor_desc);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_NOTASK, true);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_OUTPUT_REUSE_INPUT, true);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
  (void)AttrUtils::SetInt(concat_desc, ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 0);
  auto concat = graph->AddNode(concat_desc);

  auto netoutput_desc = std::make_shared<OpDesc>("netoutput", NETOUTPUT);
  (void)netoutput_desc->AddInputDesc(tensor_desc);
  auto netoutput = graph->AddNode(netoutput_desc);

  if ((data0 == nullptr) || (data1 == nullptr) || (add0 == nullptr) ||
      (add1 == nullptr) || (concat == nullptr) || (netoutput == nullptr)) {
    return nullptr;
  }
  GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add0->GetInDataAnchor(0));
  GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add1->GetInDataAnchor(0));
  GraphUtils::AddEdge(add0->GetOutDataAnchor(0), concat->GetInDataAnchor(0));
  GraphUtils::AddEdge(add1->GetOutDataAnchor(0), concat->GetInDataAnchor(1));
  GraphUtils::AddEdge(concat->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0));
  graph->TopologicalSorting();

  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder.AddTaskDef("add0", gert::AiCoreTaskDefFaker("add0_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .FakeTbeBin({"Add"})
             .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();
  compute_graph->SetGraphUnknownFlag(false);

  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      if (op_desc->GetName() == "data0") {
        op_desc->SetOutputOffset({512});
      } else {
        op_desc->SetOutputOffset({768});
      }
    } else if (op_desc->GetType() == "Add") {
      if (op_desc->GetName() == "add0") {
        op_desc->SetInputOffset({512});
        op_desc->SetOutputOffset({1024});
      } else {
        op_desc->SetInputOffset({768});
        op_desc->SetOutputOffset({1536});
      }
    } else if (op_desc->GetType() == "ConcatD") {
      op_desc->SetInputOffset({1024, 1536});
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({1024});
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

// 构造第二段输入偏移小于基址的 NoTask ConcatD 图。
GeRootModelPtr CreateGeRootModelWithNoTaskConcatNegativeOffset() {
  auto ge_root_model = CreateGeRootModelWithNoTaskConcatOutput();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  const auto compute_graph = ge_model->GetGraph();
  if (compute_graph == nullptr) {
    return nullptr;
  }
  // add1 输出偏移小于 NetOutput 基址，展开后产生负偏移。
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetName() == "add1")) {
      op_desc->SetOutputOffset({800});
      break;
    }
  }
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithNoTaskConcatOutputReuseDimOne() {
  auto ge_root_model = CreateGeRootModelWithNoTaskConcatOutput();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  const auto compute_graph = ge_model->GetGraph();
  if (compute_graph == nullptr) {
    return nullptr;
  }
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetName() == "concat_notask")) {
      (void)AttrUtils::SetInt(op_desc, ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 1);
      break;
    }
  }
  return ge_root_model;
}

// 构造 2 个模型输出：output[0] 经过 NoTask ConcatD，output[1] 为普通输出。
// data0 -> add0 -> ConcatD(NoTask) -> NetOutput.output[0]
//         add1 -^
// data1 -> add2 -----------------------> NetOutput.output[1]
// 展开后 output_count 仍为 2。
GeRootModelPtr CreateGeRootModelWithMixedOutputs() {
  auto graph = std::make_shared<ComputeGraph>("g1");
  GeTensorDesc tensor_desc(GeShape({1, 32}), FORMAT_ND, DT_FLOAT);

  auto data0_desc = std::make_shared<OpDesc>("data0", DATA);
  (void)data0_desc->AddOutputDesc(tensor_desc);
  auto data0 = graph->AddNode(data0_desc);

  auto data1_desc = std::make_shared<OpDesc>("data1", DATA);
  (void)data1_desc->AddOutputDesc(tensor_desc);
  auto data1 = graph->AddNode(data1_desc);

  auto add0_desc = std::make_shared<OpDesc>("add0", "Add");
  (void)add0_desc->AddInputDesc("x", tensor_desc);
  (void)add0_desc->AddOutputDesc("y", tensor_desc);
  add0_desc->AppendIrInput("x", kIrInputRequired);
  add0_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add0 = graph->AddNode(add0_desc);

  auto add1_desc = std::make_shared<OpDesc>("add1", "Add");
  (void)add1_desc->AddInputDesc("x", tensor_desc);
  (void)add1_desc->AddOutputDesc("y", tensor_desc);
  add1_desc->AppendIrInput("x", kIrInputRequired);
  add1_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add1 = graph->AddNode(add1_desc);

  auto add2_desc = std::make_shared<OpDesc>("add2", "Add");
  (void)add2_desc->AddInputDesc("x", tensor_desc);
  (void)add2_desc->AddOutputDesc("y", tensor_desc);
  add2_desc->AppendIrInput("x", kIrInputRequired);
  add2_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add2 = graph->AddNode(add2_desc);

  auto concat_desc = std::make_shared<OpDesc>("concat_notask", "ConcatD");
  (void)concat_desc->AddInputDesc("x0", tensor_desc);
  (void)concat_desc->AddInputDesc("x1", tensor_desc);
  GeTensorDesc out_tensor_desc(GeShape({1, 64}), FORMAT_ND, DT_FLOAT);
  (void)concat_desc->AddOutputDesc("y", out_tensor_desc);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_NOTASK, true);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_OUTPUT_REUSE_INPUT, true);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
  (void)AttrUtils::SetInt(concat_desc, ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 0);
  auto concat = graph->AddNode(concat_desc);

  auto netoutput_desc = std::make_shared<OpDesc>("netoutput", NETOUTPUT);
  (void)netoutput_desc->AddInputDesc(out_tensor_desc);
  (void)netoutput_desc->AddInputDesc(tensor_desc);
  auto netoutput = graph->AddNode(netoutput_desc);

  if ((data0 == nullptr) || (data1 == nullptr) || (add0 == nullptr) ||
      (add1 == nullptr) || (add2 == nullptr) || (concat == nullptr) || (netoutput == nullptr)) {
    return nullptr;
  }
  GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add0->GetInDataAnchor(0));
  GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add1->GetInDataAnchor(0));
  GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add2->GetInDataAnchor(0));
  GraphUtils::AddEdge(add0->GetOutDataAnchor(0), concat->GetInDataAnchor(0));
  GraphUtils::AddEdge(add1->GetOutDataAnchor(0), concat->GetInDataAnchor(1));
  GraphUtils::AddEdge(concat->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0));
  GraphUtils::AddEdge(add2->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1));
  graph->TopologicalSorting();

  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder.AddTaskDef("add0", gert::AiCoreTaskDefFaker("add0_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .AddTaskDef("add2", gert::AiCoreTaskDefFaker("add2_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .FakeTbeBin({"Add"})
             .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();
  compute_graph->SetGraphUnknownFlag(false);

  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      if (op_desc->GetName() == "data0") {
        op_desc->SetOutputOffset({512});
      } else {
        op_desc->SetOutputOffset({768});
      }
    } else if (op_desc->GetType() == "Add") {
      if (op_desc->GetName() == "add0") {
        op_desc->SetInputOffset({512});
        op_desc->SetOutputOffset({1024});
      } else if (op_desc->GetName() == "add1") {
        op_desc->SetInputOffset({512});
        op_desc->SetOutputOffset({1536});
      } else {
        op_desc->SetInputOffset({768});
        op_desc->SetOutputOffset({2048});
      }
    } else if (op_desc->GetType() == "ConcatD") {
      op_desc->SetInputOffset({1024, 1536});
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({1024, 2048});
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

// 构造 PhonyConcat NoTask 输出复用图：
// data0 -> add0 -> PhonyConcat -> NetOutput
// data1 -> add1 -^
GeRootModelPtr CreateGeRootModelWithPhonyConcatOutput() {
  auto graph = std::make_shared<ComputeGraph>("g1");
  GeTensorDesc tensor_desc(GeShape({1, 32}), FORMAT_ND, DT_FLOAT);

  auto data0_desc = std::make_shared<OpDesc>("data0", DATA);
  (void)data0_desc->AddOutputDesc(tensor_desc);
  auto data0 = graph->AddNode(data0_desc);

  auto data1_desc = std::make_shared<OpDesc>("data1", DATA);
  (void)data1_desc->AddOutputDesc(tensor_desc);
  auto data1 = graph->AddNode(data1_desc);

  auto add0_desc = std::make_shared<OpDesc>("add0", "Add");
  (void)add0_desc->AddInputDesc("x", tensor_desc);
  (void)add0_desc->AddOutputDesc("y", tensor_desc);
  add0_desc->AppendIrInput("x", kIrInputRequired);
  add0_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add0 = graph->AddNode(add0_desc);

  auto add1_desc = std::make_shared<OpDesc>("add1", "Add");
  (void)add1_desc->AddInputDesc("x", tensor_desc);
  (void)add1_desc->AddOutputDesc("y", tensor_desc);
  add1_desc->AppendIrInput("x", kIrInputRequired);
  add1_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add1 = graph->AddNode(add1_desc);

  // PhonyConcat 设置 NoTask 相关属性，不生成 task。
  auto phony_desc = std::make_shared<OpDesc>("phony_concat", PHONYCONCAT);
  (void)phony_desc->AddInputDesc("x0", tensor_desc);
  (void)phony_desc->AddInputDesc("x1", tensor_desc);
  GeTensorDesc out_tensor_desc(GeShape({1, 64}), FORMAT_ND, DT_FLOAT);
  (void)phony_desc->AddOutputDesc("y", out_tensor_desc);
  (void)AttrUtils::SetBool(phony_desc, ATTR_NAME_NOTASK, true);
  (void)AttrUtils::SetBool(phony_desc, ATTR_NAME_OUTPUT_REUSE_INPUT, true);
  (void)AttrUtils::SetBool(phony_desc, ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
  (void)AttrUtils::SetInt(phony_desc, ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 0);
  auto phony = graph->AddNode(phony_desc);

  auto netoutput_desc = std::make_shared<OpDesc>("netoutput", NETOUTPUT);
  (void)netoutput_desc->AddInputDesc(out_tensor_desc);
  auto netoutput = graph->AddNode(netoutput_desc);

  if ((data0 == nullptr) || (data1 == nullptr) || (add0 == nullptr) ||
      (add1 == nullptr) || (phony == nullptr) || (netoutput == nullptr)) {
    return nullptr;
  }
  GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add0->GetInDataAnchor(0));
  GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add1->GetInDataAnchor(0));
  GraphUtils::AddEdge(add0->GetOutDataAnchor(0), phony->GetInDataAnchor(0));
  GraphUtils::AddEdge(add1->GetOutDataAnchor(0), phony->GetInDataAnchor(1));
  GraphUtils::AddEdge(phony->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0));
  graph->TopologicalSorting();

  // 只有 add0/add1 是真实 task。
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder.AddTaskDef("add0", gert::AiCoreTaskDefFaker("add0_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .FakeTbeBin({"Add"})
             .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();
  compute_graph->SetGraphUnknownFlag(false);

  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      if (op_desc->GetName() == "data0") {
        op_desc->SetOutputOffset({512});
      } else {
        op_desc->SetOutputOffset({768});
      }
    } else if (op_desc->GetType() == "Add") {
      if (op_desc->GetName() == "add0") {
        op_desc->SetInputOffset({512});
        op_desc->SetOutputOffset({1024});
      } else if (op_desc->GetName() == "add1") {
        op_desc->SetInputOffset({768});
        op_desc->SetOutputOffset({1536});
      }
    } else if (op_desc->GetType() == PHONYCONCAT) {
      op_desc->SetInputOffset({1024, 1536});
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({1024});
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

// 构造两个根 NetOutput，验证输出索引连续。
// data0 -> add0 -> NetOutput0.output[0]
// data1 -> add1 -> NetOutput1.output[0]
GeRootModelPtr CreateGeRootModelWithTwoNetOutputs() {
  auto graph = std::make_shared<ComputeGraph>("g1");
  GeTensorDesc tensor_desc(GeShape({1, 32}), FORMAT_ND, DT_FLOAT);

  auto data0_desc = std::make_shared<OpDesc>("data0", DATA);
  (void)data0_desc->AddOutputDesc(tensor_desc);
  auto data0 = graph->AddNode(data0_desc);

  auto data1_desc = std::make_shared<OpDesc>("data1", DATA);
  (void)data1_desc->AddOutputDesc(tensor_desc);
  auto data1 = graph->AddNode(data1_desc);

  auto add0_desc = std::make_shared<OpDesc>("add0", "Add");
  (void)add0_desc->AddInputDesc("x", tensor_desc);
  (void)add0_desc->AddOutputDesc("y", tensor_desc);
  add0_desc->AppendIrInput("x", kIrInputRequired);
  add0_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add0 = graph->AddNode(add0_desc);

  auto add1_desc = std::make_shared<OpDesc>("add1", "Add");
  (void)add1_desc->AddInputDesc("x", tensor_desc);
  (void)add1_desc->AddOutputDesc("y", tensor_desc);
  add1_desc->AppendIrInput("x", kIrInputRequired);
  add1_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add1 = graph->AddNode(add1_desc);

  auto netoutput0_desc = std::make_shared<OpDesc>("netoutput0", NETOUTPUT);
  (void)netoutput0_desc->AddInputDesc(tensor_desc);
  auto netoutput0 = graph->AddNode(netoutput0_desc);

  auto netoutput1_desc = std::make_shared<OpDesc>("netoutput1", NETOUTPUT);
  (void)netoutput1_desc->AddInputDesc(tensor_desc);
  auto netoutput1 = graph->AddNode(netoutput1_desc);

  if ((data0 == nullptr) || (data1 == nullptr) || (add0 == nullptr) ||
      (add1 == nullptr) || (netoutput0 == nullptr) || (netoutput1 == nullptr)) {
    return nullptr;
  }
  GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add0->GetInDataAnchor(0));
  GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add1->GetInDataAnchor(0));
  GraphUtils::AddEdge(add0->GetOutDataAnchor(0), netoutput0->GetInDataAnchor(0));
  GraphUtils::AddEdge(add1->GetOutDataAnchor(0), netoutput1->GetInDataAnchor(0));
  graph->TopologicalSorting();

  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder.AddTaskDef("add0", gert::AiCoreTaskDefFaker("add0_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add1_stub").ArgsFormat("{i_instance0*}{o_instance0*}"))
             .FakeTbeBin({"Add"})
             .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();
  compute_graph->SetGraphUnknownFlag(false);

  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() == DATA) {
      if (op_desc->GetName() == "data0") {
        op_desc->SetOutputOffset({512});
      } else {
        op_desc->SetOutputOffset({768});
      }
    } else if (op_desc->GetType() == "Add") {
      if (op_desc->GetName() == "add0") {
        op_desc->SetInputOffset({512});
        op_desc->SetOutputOffset({1024});
      } else {
        op_desc->SetInputOffset({768});
        op_desc->SetOutputOffset({2048});
      }
    } else if (op_desc->GetType() == NETOUTPUT) {
      if (op_desc->GetName() == "netoutput0") {
        op_desc->SetInputOffset({1024});
      } else {
        op_desc->SetInputOffset({2048});
      }
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

Status BuildCodegenModel(const GeRootModelPtr &ge_root_model, Om2CodegenModel &doc,
                         std::vector<TaskCodeBuilderPtr> *task_generators_out = nullptr,
                         Om2ConstMetas *const_metas_out = nullptr) {
  GE_ASSERT_NOTNULL(ge_root_model);
  SyncKernelNameForAllModels(ge_root_model);
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  GE_ASSERT_TRUE(!name_to_ge_model.empty());
  const auto &ge_model = name_to_ge_model.begin()->second;
  GE_ASSERT_NOTNULL(ge_model);

  std::vector<TaskCodeBuilderPtr> task_builders;
  GE_ASSERT_SUCCESS(
      Om2CodegenModelBuilder::CreateTaskCodeBuilders(ge_model, GetOm2CodegenModelBuilderUtAst(), task_builders, doc));

  Om2CodegenModelBuilder builder;
  Om2ConstMetas const_metas;
  GE_CHK_STATUS_RET(builder.Build(ge_model, task_builders, doc, const_metas));
  if (task_generators_out != nullptr) {
    *task_generators_out = task_builders;
  }
  if (const_metas_out != nullptr) {
    *const_metas_out = const_metas;
  }
  return SUCCESS;
}

Status BuildCodegenModel(const GeModelPtr &ge_model, Om2CodegenModel &doc,
                         std::vector<TaskCodeBuilderPtr> *task_generators_out = nullptr,
                         Om2ConstMetas *const_metas_out = nullptr) {
  GE_ASSERT_NOTNULL(ge_model);
  SyncKernelNameFromOpDesc(ge_model);
  std::vector<TaskCodeBuilderPtr> task_builders;
  GE_ASSERT_SUCCESS(
      Om2CodegenModelBuilder::CreateTaskCodeBuilders(ge_model, GetOm2CodegenModelBuilderUtAst(), task_builders, doc));

  Om2CodegenModelBuilder builder;
  Om2ConstMetas const_metas;
  GE_CHK_STATUS_RET(builder.Build(ge_model, task_builders, doc, const_metas));
  if (task_generators_out != nullptr) {
    *task_generators_out = task_builders;
  }
  if (const_metas_out != nullptr) {
    *const_metas_out = const_metas;
  }
  return SUCCESS;
}

Status BuildCodegenModelWithTaskGenerators(const GeRootModelPtr &ge_root_model,
                                           const std::vector<TaskCodeBuilderPtr> &task_builders, Om2CodegenModel &doc) {
  GE_ASSERT_NOTNULL(ge_root_model);
  SyncKernelNameForAllModels(ge_root_model);
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  GE_ASSERT_TRUE(!name_to_ge_model.empty());
  const auto &ge_model = name_to_ge_model.begin()->second;
  GE_ASSERT_NOTNULL(ge_model);
  Om2CodegenModelBuilder builder;
  Om2ConstMetas const_metas;
  return builder.Build(ge_model, task_builders, doc, const_metas);
}

}  // namespace

class Om2CodegenModelBuilderUt : public testing::Test {};

TEST_F(Om2CodegenModelBuilderUt, BuildRuntimeSemantic_Aicore_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  EXPECT_EQ(doc.model_name, "g1");
  EXPECT_EQ(doc.runtime.total_mem_size, 2048U);
  EXPECT_EQ(doc.runtime.total_weight_size, 512U);
  EXPECT_EQ(doc.runtime.stream_num, 1U);
  EXPECT_EQ(doc.runtime.notify_num, 0U);
  EXPECT_EQ(doc.runtime.event_num, 0U);
  EXPECT_EQ(doc.runtime.label_num, 0U);
  ASSERT_EQ(doc.runtime.stream_flag_values.size(), 1U);
  EXPECT_EQ(doc.runtime.stream_flag_values[0], "RT_STREAM_PERSISTENT");
}

TEST_F(Om2CodegenModelBuilderUt, BuildRuntimeSemantic_WithNotifyEventLabel_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithRuntimeAttrs();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  EXPECT_EQ(doc.runtime.total_mem_size, 2048U);
  EXPECT_EQ(doc.runtime.total_weight_size, 512U);
  EXPECT_EQ(doc.runtime.stream_num, 1U);
  EXPECT_EQ(doc.runtime.notify_num, 2U);
  EXPECT_EQ(doc.runtime.event_num, 3U);
  EXPECT_EQ(doc.runtime.label_num, 4U);
  ASSERT_EQ(doc.runtime.stream_flag_values.size(), 1U);
  EXPECT_EQ(doc.runtime.stream_flag_values[0], "RT_STREAM_PERSISTENT");
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIoSemantic_Aicore_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  ASSERT_EQ(doc.model_io.input_count, 2U);
  ASSERT_EQ(doc.model_io.output_count, 1U);
  ASSERT_EQ(doc.model_io.entries.size(), 3U);
  EXPECT_EQ(doc.model_io.io_offsets, std::set<int64_t>({1024, 3072}));

  EXPECT_EQ(doc.model_io.entries[0].index, 0U);
  EXPECT_EQ(doc.model_io.entries[0].memory_offset, 1024);
  EXPECT_EQ(doc.model_io.entries[0].update_host_args_index, 0U);
  EXPECT_TRUE(doc.model_io.entries[0].is_input);
  EXPECT_TRUE(doc.model_io.entries[0].is_addr_refreshable);

  EXPECT_EQ(doc.model_io.entries[1].index, 1U);
  EXPECT_EQ(doc.model_io.entries[1].memory_offset, 1024);
  EXPECT_EQ(doc.model_io.entries[1].update_host_args_index, 1U);
  EXPECT_TRUE(doc.model_io.entries[1].is_input);
  EXPECT_TRUE(doc.model_io.entries[1].is_addr_refreshable);

  EXPECT_EQ(doc.model_io.entries[2].index, 0U);
  EXPECT_EQ(doc.model_io.entries[2].memory_offset, 3072);
  EXPECT_EQ(doc.model_io.entries[2].update_host_args_index, 2U);
  EXPECT_FALSE(doc.model_io.entries[2].is_input);
  EXPECT_TRUE(doc.model_io.entries[2].is_addr_refreshable);
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIoSemantic_WithoutDataAttrIndex_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithoutDataAttrIndex();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);
  ASSERT_EQ(doc.model_io.entries.size(), 3U);
  EXPECT_EQ(doc.model_io.entries[0].index, 0U);
  EXPECT_EQ(doc.model_io.entries[0].update_host_args_index, 0U);
  EXPECT_EQ(doc.model_io.entries[1].index, 1U);
  EXPECT_EQ(doc.model_io.entries[1].update_host_args_index, 1U);
  EXPECT_EQ(doc.model_io.entries[2].index, 0U);
  EXPECT_FALSE(doc.model_io.entries[2].is_input);
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIoSemantic_ReorderedDataAttrIndex_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithReorderedDataAttrIndex();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);
  ASSERT_EQ(doc.model_io.entries.size(), 3U);
  EXPECT_EQ(doc.model_io.entries[0].index, 0U);
  EXPECT_EQ(doc.model_io.entries[0].memory_offset, 1024);
  EXPECT_EQ(doc.model_io.entries[0].update_host_args_index, 0U);
  EXPECT_EQ(doc.model_io.entries[1].index, 1U);
  EXPECT_EQ(doc.model_io.entries[1].memory_offset, 2048);
  EXPECT_EQ(doc.model_io.entries[1].update_host_args_index, 1U);
  EXPECT_EQ(doc.model_io.entries[2].index, 0U);
  EXPECT_FALSE(doc.model_io.entries[2].is_input);
}

GeRootModelPtr CreateGeRootModelWithIoMemoryOffsetOverlap() {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    return nullptr;
  }
  const auto ge_model = name_to_ge_model.begin()->second;
  const auto compute_graph = ge_model->GetGraph();
  if (compute_graph == nullptr) {
    return nullptr;
  }
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc == nullptr) || (op_desc->GetType() != NETOUTPUT)) {
      continue;
    }
    op_desc->SetInputOffset({1024});
    break;
  }
  return ge_root_model;
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIoSemantic_IoMemoryOffsetOverlap_Fail) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithIoMemoryOffsetOverlap();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), PARAM_INVALID);
}

TEST_F(Om2CodegenModelBuilderUt, BuildConstInputs_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithConstInputOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  ASSERT_EQ(doc.const_inputs.size(), 1U);
  EXPECT_EQ(doc.const_inputs[0].const_index, 0U);
  EXPECT_EQ(doc.const_inputs[0].var_name, "const_0");
}

TEST_F(Om2CodegenModelBuilderUt, BuildConstInputs_FollowsTaskOrder_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithConstInputsInTaskOrder();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  Om2ConstMetas const_metas;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, nullptr, &const_metas), SUCCESS);

  ASSERT_EQ(doc.const_inputs.size(), 2U);
  ASSERT_EQ(const_metas.size(), 2U);
  for (size_t i = 0U; i < doc.const_inputs.size(); ++i) {
    EXPECT_EQ(doc.const_inputs[i].const_index, i);
    EXPECT_EQ(doc.const_inputs[i].var_name, "const_" + std::to_string(i));
    EXPECT_EQ(const_metas[i].index, i);
    EXPECT_EQ(const_metas[i].type, "INTERNAL");
  }
  EXPECT_EQ(const_metas[0].offset, 128);
  EXPECT_EQ(const_metas[1].offset, 256);
}

TEST_F(Om2CodegenModelBuilderUt, BuildKernelRegistry_Aicore_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  ASSERT_EQ(doc.kernel_registry.binaries.size(), 1U);
  EXPECT_EQ(doc.runtime.kernel_bin_num, 1U);
  ASSERT_EQ(doc.kernel_registry.func_handle_indices.size(), 1U);
  EXPECT_EQ(doc.kernel_registry.func_handle_indices.at("add1_faked_kernel"), 0U);

  const auto &binary = doc.kernel_registry.binaries[0];
  EXPECT_EQ(binary.kind, KernelBinaryKind::kAicore);
  EXPECT_EQ(binary.kernel_name, "add1_faked_kernel");
  EXPECT_EQ(binary.file_name, "add1_faked_kernel.o");
  EXPECT_EQ(binary.magic, "ACL_RT_BINARY_MAGIC_ELF_VECTOR_CORE");
  EXPECT_EQ(binary.func_handle_index, 0U);
}

TEST_F(Om2CodegenModelBuilderUt, BuildKernelRegistryAndLaunch_AicoreAtomic_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAtomicAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  ASSERT_NE(ge_model, nullptr);

  ASSERT_EQ(
      Om2CodegenModelBuilder::CreateTaskCodeBuilders(ge_model, GetOm2CodegenModelBuilderUtAst(), task_builders, doc),
      SUCCESS);
  Om2CodegenModelBuilder builder;
  Om2ConstMetas const_metas;
  ASSERT_EQ(builder.Build(ge_model, task_builders, doc, const_metas), SUCCESS);

  const std::string kernel_name = "add1_faked_kernel";
  const std::string atomic_kernel_name = "add1_faked_atomic_kernel";
  const std::string atomic_func_handle_key = atomic_kernel_name + "_atomic";
  ASSERT_EQ(doc.kernel_registry.binaries.size(), 2U);
  EXPECT_EQ(doc.runtime.kernel_bin_num, 2U);
  ASSERT_EQ(doc.kernel_registry.func_handle_indices.size(), 2U);
  ASSERT_EQ(doc.kernel_registry.func_handle_indices.count(kernel_name), 1U);
  ASSERT_EQ(doc.kernel_registry.func_handle_indices.count(atomic_func_handle_key), 1U);
  const uint32_t kernel_func_idx = doc.kernel_registry.func_handle_indices.at(kernel_name);
  const uint32_t atomic_func_idx = doc.kernel_registry.func_handle_indices.at(atomic_func_handle_key);
  EXPECT_NE(kernel_func_idx, atomic_func_idx);

  const auto atomic_binary_it = std::find_if(
      doc.kernel_registry.binaries.begin(), doc.kernel_registry.binaries.end(),
      [&atomic_kernel_name](const KernelBinaryRecord &binary) { return binary.kernel_name == atomic_kernel_name; });
  ASSERT_NE(atomic_binary_it, doc.kernel_registry.binaries.end());
  EXPECT_EQ(atomic_binary_it->kind, KernelBinaryKind::kAicore);
  EXPECT_EQ(atomic_binary_it->file_name, "add1_faked_atomic_kernel.o");
  EXPECT_EQ(atomic_binary_it->magic, "ACL_RT_BINARY_MAGIC_ELF_VECTOR_CORE");
  EXPECT_EQ(atomic_binary_it->func_handle_index, atomic_func_idx);

  ASSERT_EQ(task_builders.size(), 2U);
  const auto *atomic_task_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  const auto *kernel_task_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[1].get());
  ASSERT_NE(atomic_task_builder, nullptr);
  ASSERT_NE(kernel_task_builder, nullptr);
  EXPECT_EQ(atomic_task_builder->GetTaskSemantic().launch.func_handle_index, atomic_func_idx);
  EXPECT_EQ(kernel_task_builder->GetTaskSemantic().launch.func_handle_index, kernel_func_idx);
}

TEST_F(Om2CodegenModelBuilderUt, BuildKernelRegistry_Aicpu_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  ASSERT_EQ(doc.kernel_registry.binaries.size(), 1U);
  EXPECT_EQ(doc.runtime.kernel_bin_num, 1U);
  ASSERT_EQ(doc.kernel_registry.func_handle_indices.size(), 1U);
  EXPECT_EQ(doc.kernel_registry.func_handle_indices.at("Addname"), 0U);

  const auto &binary0 = doc.kernel_registry.binaries[0];
  EXPECT_EQ(binary0.kind, KernelBinaryKind::kAicpu);
  EXPECT_EQ(binary0.kernel_name, "name");
  EXPECT_EQ(binary0.op_type, "Add");
  EXPECT_EQ(binary0.so_name, "");
  EXPECT_EQ(binary0.op_kernel_lib, "AICPUKernel");
  EXPECT_EQ(binary0.func_handle_index, 0U);
}

TEST_F(Om2CodegenModelBuilderUt, BuildTaskSemantics_SimpleTasksAndStub_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithSimpleTasksAndStub();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  ASSERT_EQ(task_builders.size(), 4U);

  const auto *kernel_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  ASSERT_NE(kernel_builder, nullptr);
  const auto &kernel_task = kernel_builder->GetTaskSemantic();
  ASSERT_TRUE(kernel_task.args_table_entry.has_value());
  EXPECT_EQ(kernel_task.launch.stream_id, 0U);
  EXPECT_EQ(kernel_task.launch.func_handle_index, 0U);
  EXPECT_EQ(kernel_task.task_type, ModelTaskType::MODEL_TASK_KERNEL);
  EXPECT_TRUE(Om2CodegenUtils::IsAICoreKernel(kernel_task.kernel_type));

  const auto *fusion_start_builder = dynamic_cast<FusionStartTaskCodeBuilder *>(task_builders[1].get());
  ASSERT_NE(fusion_start_builder, nullptr);

  const auto *fusion_end_builder = dynamic_cast<FusionEndTaskCodeBuilder *>(task_builders[2].get());
  ASSERT_NE(fusion_end_builder, nullptr);

  const auto *end_graph_builder = dynamic_cast<EndGraphTaskCodeBuilder *>(task_builders[3].get());
  ASSERT_NE(end_graph_builder, nullptr);
}

TEST_F(Om2CodegenModelBuilderUt, BuildTaskSemantics_AicpuTaskCount_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  EXPECT_EQ(doc.aicpu_task_count, 2U);
  ASSERT_EQ(task_builders.size(), 2U);
  const auto *task0_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  const auto *task1_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[1].get());
  ASSERT_NE(task0_builder, nullptr);
  ASSERT_NE(task1_builder, nullptr);
  const auto &task0 = task0_builder->GetTaskSemantic();
  const auto &task1 = task1_builder->GetTaskSemantic();
  EXPECT_EQ(task0.task_type, ModelTaskType::MODEL_TASK_KERNEL);
  EXPECT_EQ(task1.task_type, ModelTaskType::MODEL_TASK_KERNEL);
  EXPECT_EQ(task0.kernel_type, ccKernelType::AI_CPU);
  EXPECT_EQ(task1.kernel_type, ccKernelType::AI_CPU);
  EXPECT_EQ(task0.aicpu_task_index, 0U);
  EXPECT_EQ(task1.aicpu_task_index, 1U);
}

TEST_F(Om2CodegenModelBuilderUt, BuildTaskSemantics_KernelTask_UsesTaskTypeAndKernelType_Ok) {
  EXPECT_FALSE(HasIsAllKernelMember<KernelTaskSemantic>::value);
  EXPECT_FALSE(HasIsAicpuMember<KernelTaskSemantic>::value);
  EXPECT_FALSE(HasLaunchKernelConfigAssembler<KernelTaskCodeBuilder>::value);
}

TEST_F(Om2CodegenModelBuilderUt, BuildTaskSemantics_UsesGeneratorContribute_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithSimpleTasksAndStub();
  ASSERT_NE(ge_root_model, nullptr);
  std::vector<TaskCodeBuilderPtr> task_builders(4U);
  task_builders[0] = MakeShared<KernelTaskCodeBuilder>(GetOm2CodegenModelBuilderUtAst());
  task_builders[1] = MakeShared<CustomFusionStartTaskCodeBuilder>(GetOm2CodegenModelBuilderUtAst());
  task_builders[2] = MakeShared<FusionEndTaskCodeBuilder>(GetOm2CodegenModelBuilderUtAst());
  task_builders[3] = MakeShared<EndGraphTaskCodeBuilder>(GetOm2CodegenModelBuilderUtAst());
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModelWithTaskGenerators(ge_root_model, task_builders, doc), SUCCESS);

  const auto *fusion_start_builder = dynamic_cast<CustomFusionStartTaskCodeBuilder *>(task_builders[1].get());
  ASSERT_NE(fusion_start_builder, nullptr);
  EXPECT_TRUE(fusion_start_builder->WasContributeCalled());
}

TEST_F(Om2CodegenModelBuilderUt, BuildTaskSemantics_KernelLaunchAndArgs_AicoreFormat_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp2();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  ASSERT_EQ(doc.args_table.entries.size(), 1U);
  const auto *kernel_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  ASSERT_NE(kernel_builder, nullptr);
  const auto &kernel_task = kernel_builder->GetTaskSemantic();
  ASSERT_TRUE(kernel_task.args_table_entry.has_value());

  EXPECT_EQ(kernel_task.launch.block_dim, 8U);
  EXPECT_EQ(kernel_task.launch.stream_id, 0U);
  EXPECT_EQ(kernel_task.launch.config.schedule_mode, 0U);
  EXPECT_EQ(kernel_task.launch.config.engine_type, "ACL_RT_ENGINE_TYPE_AIC");
  EXPECT_EQ(kernel_task.launch.func_handle_index, 0U);
  EXPECT_EQ(kernel_task.args_table_entry->table_index, doc.args_table.entries[0].table_index);
  EXPECT_EQ(kernel_task.args_table_entry->args_size, doc.args_table.entries[0].args_size);
  EXPECT_EQ(kernel_task.args_table_entry->host_offset, doc.args_table.entries[0].host_offset);
  EXPECT_EQ(kernel_task.args_table_entry->table_index, 0U);

  std::vector<std::string> ordered_arg_symbols;
  for (const auto &addr : kernel_task.ordered_arg_values) {
    ordered_arg_symbols.push_back(addr.symbol_hint);
  }
  EXPECT_EQ(ordered_arg_symbols, std::vector<std::string>({"op2_input0", "op2_input1", "op2_output0", "op2_ws0"}));
  std::vector<uint64_t> io_addr_refresh_host_offsets;
  std::vector<int64_t> io_addr_refresh_compile_offsets;
  for (const auto &record : kernel_builder->GetIoAddrRefreshRecords()) {
    io_addr_refresh_host_offsets.push_back(record.host_offset);
    io_addr_refresh_compile_offsets.push_back(static_cast<int64_t>(record.compile_state_io_addr_offset));
  }
  std::sort(io_addr_refresh_host_offsets.begin(), io_addr_refresh_host_offsets.end());
  EXPECT_EQ(io_addr_refresh_host_offsets, std::vector<uint64_t>({0U, 8U, 16U}));
  EXPECT_EQ(io_addr_refresh_compile_offsets, std::vector<int64_t>({1024, 1024, 1024}));
}

TEST_F(Om2CodegenModelBuilderUt, BuildTaskSemantics_KernelLaunchAndArgs_DynamicIo_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOpOfDynamicIo();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  ASSERT_EQ(doc.args_table.entries.size(), 1U);
  const auto *kernel_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  ASSERT_NE(kernel_builder, nullptr);
  const auto &kernel_task = kernel_builder->GetTaskSemantic();
  ASSERT_TRUE(kernel_task.args_table_entry.has_value());

  EXPECT_EQ(kernel_task.args_table_entry->table_index, doc.args_table.entries[0].table_index);
  EXPECT_EQ(kernel_task.args_table_entry->args_size, doc.args_table.entries[0].args_size);
  EXPECT_EQ(kernel_task.args_table_entry->host_offset, doc.args_table.entries[0].host_offset);
  EXPECT_EQ(kernel_task.args_table_entry->table_index, 0U);
  EXPECT_EQ(kernel_task.args_table_entry->host_offset, 0U);

  ASSERT_GE(kernel_task.ordered_arg_values.size(), 6U);
  EXPECT_EQ(kernel_task.ordered_arg_values[0].kind, AddrValueKind::kLevel1DescPtr);
  EXPECT_EQ(kernel_task.ordered_arg_values[1].kind, AddrValueKind::kLevel1DescPtr);
  EXPECT_EQ(kernel_task.ordered_arg_values[2].kind, AddrValueKind::kLevel1DescPtr);
  EXPECT_EQ(kernel_task.ordered_arg_values[3].kind, AddrValueKind::kShapeInfoBuffer);
  EXPECT_EQ(kernel_task.ordered_arg_values[4].kind, AddrValueKind::kInputInstance);
  EXPECT_EQ(kernel_task.ordered_arg_values[5].kind, AddrValueKind::kShapeInfoBuffer);
  ASSERT_TRUE(kernel_task.ordered_arg_values[0].level1_target_offset.has_value());
  ASSERT_TRUE(kernel_task.ordered_arg_values[1].level1_target_offset.has_value());
  ASSERT_TRUE(kernel_task.ordered_arg_values[2].level1_target_offset.has_value());
  EXPECT_EQ(*kernel_task.ordered_arg_values[0].level1_target_offset, 24U);
  EXPECT_LT(*kernel_task.ordered_arg_values[0].level1_target_offset,
            *kernel_task.ordered_arg_values[1].level1_target_offset);
  EXPECT_LT(*kernel_task.ordered_arg_values[1].level1_target_offset,
            *kernel_task.ordered_arg_values[2].level1_target_offset);
}

TEST_F(Om2CodegenModelBuilderUt, BuildTaskSemantics_KernelLaunchAndArgs_NonPrefixDesc_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOpOfDynamicIo("{i0}{i_desc1}{o_desc0}");
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  const auto *kernel_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  ASSERT_NE(kernel_builder, nullptr);
  const auto &ordered_args = kernel_builder->GetTaskSemantic().ordered_arg_values;

  ASSERT_GE(ordered_args.size(), 7U);
  EXPECT_EQ(ordered_args[0].kind, AddrValueKind::kInputInstance);
  EXPECT_EQ(ordered_args[1].kind, AddrValueKind::kLevel1DescPtr);
  EXPECT_EQ(ordered_args[2].kind, AddrValueKind::kLevel1DescPtr);
  EXPECT_EQ(ordered_args[3].kind, AddrValueKind::kShapeInfoBuffer);
  EXPECT_EQ(ordered_args[5].kind, AddrValueKind::kShapeInfoBuffer);
  ASSERT_TRUE(ordered_args[1].level1_target_offset.has_value());
  ASSERT_TRUE(ordered_args[2].level1_target_offset.has_value());
  EXPECT_EQ(*ordered_args[1].level1_target_offset, 24U);
  EXPECT_EQ(*ordered_args[2].level1_target_offset, 80U);
}

TEST_F(Om2CodegenModelBuilderUt, BuildTaskSemantics_KernelLaunchAndArgs_Aicpu_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  ASSERT_EQ(doc.args_table.entries.size(), 2U);
  const auto *task0_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  const auto *task1_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[1].get());
  ASSERT_NE(task0_builder, nullptr);
  ASSERT_NE(task1_builder, nullptr);
  const auto &task0 = task0_builder->GetTaskSemantic();
  const auto &task1 = task1_builder->GetTaskSemantic();
  ASSERT_TRUE(task0.args_table_entry.has_value());
  ASSERT_TRUE(task1.args_table_entry.has_value());

  const std::vector<const KernelTaskSemantic *> tasks{&task0, &task1};
  for (size_t i = 0U; i < tasks.size(); ++i) {
    EXPECT_EQ(tasks[i]->launch.func_handle_index, 0U);
    EXPECT_EQ(tasks[i]->kernel_type, ccKernelType::AI_CPU);
    EXPECT_EQ(tasks[i]->args_table_entry->table_index, doc.args_table.entries[i].table_index);
    EXPECT_EQ(tasks[i]->args_table_entry->args_size, doc.args_table.entries[i].args_size);
    EXPECT_EQ(tasks[i]->args_table_entry->host_offset, doc.args_table.entries[i].host_offset);
    EXPECT_EQ(tasks[i]->aicpu_task_index, i);
    ASSERT_FALSE(tasks[i]->ordered_arg_values.empty());
    ASSERT_TRUE(tasks[i]->aicpu_args.has_value());
    ASSERT_TRUE(tasks[i]->aicpu_ext_info.has_value());
    EXPECT_EQ(tasks[i]->aicpu_args->args_size, tasks[i]->aicpu_args->args_buffer.size());
    EXPECT_GT(tasks[i]->aicpu_args->args_buffer.size(), 0U);
    EXPECT_EQ(tasks[i]->aicpu_ext_info->total_len, tasks[i]->aicpu_ext_info->serialized_bytes.size());
    EXPECT_GT(tasks[i]->aicpu_ext_info->serialized_bytes.size(), 0U);
    EXPECT_GE(tasks[i]->aicpu_ext_info->session_info_offset, -1);
  }
}

TEST_F(Om2CodegenModelBuilderUt, RenderDistribution_UsesSemanticLaunchAndArgs_Aicore_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp2();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  ASSERT_EQ(task_builders.size(), 1U);
  auto *kernel_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  ASSERT_NE(kernel_builder, nullptr);
  const auto &kernel_task = kernel_builder->GetTaskSemantic();
  ASSERT_TRUE(kernel_task.args_table_entry.has_value());

  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  std::vector<BodyItem> items;
  const uint32_t stale_func_handle_index = kernel_task.launch.func_handle_index + 7U;
  const uint64_t stale_args_table_index = kernel_task.args_table_entry->table_index + 5U;
  ASSERT_EQ(kernel_builder->RenderDistribution(items), SUCCESS);
  const auto nodes = ast.Body(items);
  std::stringstream output;
  EmitCodeFromNodes(nodes, output);
  const auto code = output.str();
  EXPECT_NE(code.find("static constexpr int64_t op2_input0_shape[] = {1, 1, 224, 224};"), std::string::npos);
  EXPECT_NE(code.find("Om2Tensor op2_input0 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024), "
                      "200704UL, 1, 0, op2_input0_shape, 4UL);"),
            std::string::npos);
  EXPECT_NE(code.find("FlattenHostArgs(op2_input0, op2_input1, op2_output0, op2_ws0)"), std::string::npos);
  EXPECT_NE(code.find("GetIsDataDump(\"add1\", model_id_, instance_handle_)"), std::string::npos);
  EXPECT_NE(code.find("OM2_CHK_STATUS(ReportLaunchedOm2Task(\"add1\", \"Add\", 2U, "
                      "reinterpret_cast<uintptr_t>(args_table_.GetArgsInfo(0)->dev_addr), "
                      "args_table_.GetArgsInfo(0)->size, op2_report_inputs, 2UL, op2_report_outputs, 1U, "
                      "op2_report_workspace_addrs, op2_report_workspace_sizes, 1U, 0U, "
                      "8U, stream_list_[0], model_id_, instance_handle_))"),
            std::string::npos);
  EXPECT_NE(code.find("args_table_.GetArgsInfo(" + std::to_string(kernel_task.args_table_entry->table_index) + ")"),
            std::string::npos);
  EXPECT_EQ(code.find("args_table_.GetArgsInfo(" + std::to_string(stale_args_table_index) + ")"), std::string::npos);
  EXPECT_NE(code.find("func_handles_[" + std::to_string(kernel_task.launch.func_handle_index) + "]"),
            std::string::npos);
  EXPECT_EQ(code.find("func_handles_[" + std::to_string(stale_func_handle_index) + "]"), std::string::npos);
}

TEST_F(Om2CodegenModelBuilderUt, RenderDistribution_UsesConstInputTensor_Aicore_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithConstInputOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  ASSERT_EQ(task_builders.size(), 1U);
  auto *kernel_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  ASSERT_NE(kernel_builder, nullptr);
  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  std::vector<BodyItem> items;
  ASSERT_EQ(kernel_builder->RenderDistribution(items), SUCCESS);
  const auto nodes = ast.Body(items);
  std::stringstream output;
  EmitCodeFromNodes(nodes, output);
  const auto code = output.str();
  EXPECT_NE(code.find("FlattenHostArgs(op2_input0, op2_input1, op2_output0, op2_ws0)"), std::string::npos);
  EXPECT_NE(code.find("OM2_CHK_STATUS(ReportLaunchedOm2Task(\"add1\", \"Add\", 2U, "
                      "reinterpret_cast<uintptr_t>(args_table_.GetArgsInfo(0)->dev_addr), "
                      "args_table_.GetArgsInfo(0)->size, op2_report_inputs, 2UL, op2_report_outputs, 1U, "
                      "op2_report_workspace_addrs, op2_report_workspace_sizes, 1U, 0U, "
                      "8U, stream_list_[0], model_id_, instance_handle_))"),
            std::string::npos);
}

TEST_F(Om2CodegenModelBuilderUt, RenderDistribution_UsesSemanticLaunchAndArgs_Aicpu_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  ASSERT_EQ(task_builders.size(), 2U);
  auto *kernel_builder = dynamic_cast<KernelTaskCodeBuilder *>(task_builders[0].get());
  ASSERT_NE(kernel_builder, nullptr);
  const auto &kernel_task = kernel_builder->GetTaskSemantic();
  ASSERT_EQ(kernel_task.task_type, ModelTaskType::MODEL_TASK_KERNEL);
  ASSERT_EQ(kernel_task.kernel_type, ccKernelType::AI_CPU);
  ASSERT_TRUE(kernel_task.args_table_entry.has_value());

  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  std::vector<BodyItem> items;
  const uint32_t stale_func_handle_index = kernel_task.launch.func_handle_index + 9U;
  const uint64_t stale_args_table_index = kernel_task.args_table_entry->table_index + 6U;
  const uint64_t stale_aicpu_task_index = kernel_task.aicpu_task_index + 8U;
  ASSERT_EQ(kernel_builder->RenderDistribution(items), SUCCESS);
  const auto nodes = ast.Body(items);
  std::stringstream output;
  EmitCodeFromNodes(nodes, output);
  const auto code = output.str();
  EXPECT_NE(code.find("args_table_.GetArgsInfo(" + std::to_string(kernel_task.args_table_entry->table_index) + ")"),
            std::string::npos);
  EXPECT_EQ(code.find("args_table_.GetArgsInfo(" + std::to_string(stale_args_table_index) + ")"), std::string::npos);
  EXPECT_NE(code.find("func_handles_[" + std::to_string(kernel_task.launch.func_handle_index) + "]"),
            std::string::npos);
  EXPECT_EQ(code.find("func_handles_[" + std::to_string(stale_func_handle_index) + "]"), std::string::npos);
  EXPECT_NE(code.find("dev_ext_info_mem_ptrs_[" + std::to_string(kernel_task.aicpu_task_index) + "]"),
            std::string::npos);
  EXPECT_EQ(code.find("dev_ext_info_mem_ptrs_[" + std::to_string(stale_aicpu_task_index) + "]"), std::string::npos);
}

TEST_F(Om2CodegenModelBuilderUt, RenderDistribution_UsesTensorAndDeviceAddress_MemcpyAsync_Ok) {
  auto ge_model = CreateGeModelWithMemcpyAsyncTask();
  ASSERT_NE(ge_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_model, doc, &task_builders), SUCCESS);

  auto *memcpy_async_builder = FindTaskBuilder<MemcpyAsyncTaskCodeBuilder>(task_builders);
  ASSERT_NE(memcpy_async_builder, nullptr);

  std::vector<BodyItem> items;
  ASSERT_EQ(memcpy_async_builder->RenderDistribution(items), SUCCESS);
  const auto code = EmitCodeFromBodyItems(items);
  EXPECT_NE(code.find("auto op2_output0 = GET_ADDR(total_dev_mem_ptr_, 2048)"), std::string::npos);
  EXPECT_NE(code.find("FlattenHostArgs(op2_input0, op2_output0)"), std::string::npos);
  EXPECT_NE(code.find("memcpy_s(args_table_.GetArgsInfo(1)->host_addr"), std::string::npos);
  EXPECT_NE(code.find("KernelMemcpyAsyncDistribute"), std::string::npos);
}

TEST_F(Om2CodegenModelBuilderUt, RenderDistribution_UsesTensorAndDeviceAddress_StreamSwitch_Ok) {
  auto ge_model = CreateGeModelWithStreamSwitchTask();
  ASSERT_NE(ge_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_model, doc, &task_builders), SUCCESS);

  auto *stream_switch_builder = FindTaskBuilder<StreamSwitchTaskCodeBuilder>(task_builders);
  ASSERT_NE(stream_switch_builder, nullptr);

  std::vector<BodyItem> items;
  ASSERT_EQ(stream_switch_builder->RenderDistribution(items), SUCCESS);
  const auto code = EmitCodeFromBodyItems(items);
  EXPECT_NE(code.find("Om2Tensor op2_input0 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024),"), std::string::npos);
  EXPECT_NE(code.find("Om2Tensor op2_input1 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 2048),"), std::string::npos);
  EXPECT_NE(code.find("ValueToPtr(op2_input0.device_address)"), std::string::npos);
  EXPECT_NE(code.find("ValueToPtr(op2_input1.device_address)"), std::string::npos);
  EXPECT_NE(code.find("KernelStreamSwitchDistribute"), std::string::npos);
}

TEST_F(Om2CodegenModelBuilderUt, AggregateArgsTable_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc, &task_builders), SUCCESS);

  ASSERT_EQ(doc.args_table.entries.size(), task_builders.size());
  uint64_t expected_total_host_args_len = 0UL;
  std::multimap<uint64_t, uint64_t> io_addr_offset_map;
  for (size_t i = 0U; i < task_builders.size(); ++i) {
    const auto *args_entry = task_builders[i]->GetArgsTableEntry();
    ASSERT_NE(args_entry, nullptr);
    EXPECT_EQ(doc.args_table.entries[i].table_index, i);
    EXPECT_EQ(doc.args_table.entries[i].args_size, args_entry->args_size);
    EXPECT_EQ(doc.args_table.entries[i].host_offset, args_entry->host_offset);
    expected_total_host_args_len =
        std::max(expected_total_host_args_len, args_entry->host_offset + args_entry->args_size);
    for (const auto &record : task_builders[i]->GetIoAddrRefreshRecords()) {
      io_addr_offset_map.emplace(record.compile_state_io_addr_offset, record.host_offset);
    }
  }
  EXPECT_EQ(doc.args_table.total_host_args_len, expected_total_host_args_len);

  ASSERT_EQ(doc.args_table.host_args_offsets.size(), doc.model_io.entries.size());
  for (size_t i = 0U; i < doc.model_io.entries.size(); ++i) {
    std::vector<uint64_t> expected_host_offsets;
    const auto range = io_addr_offset_map.equal_range(static_cast<uint64_t>(doc.model_io.entries[i].memory_offset));
    for (auto it = range.first; it != range.second; ++it) {
      expected_host_offsets.push_back(it->second);
    }
    std::sort(expected_host_offsets.begin(), expected_host_offsets.end());
    EXPECT_EQ(doc.args_table.host_args_offsets[i], expected_host_offsets);
  }
}

TEST_F(Om2CodegenModelBuilderUt, RenderDistribution_UsesSemanticSimpleTaskContribs_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithSimpleTasksAndStub();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  std::vector<TaskCodeBuilderPtr> task_builders(4U);
  task_builders[0] = MakeShared<KernelTaskCodeBuilder>(GetOm2CodegenModelBuilderUtAst());
  task_builders[1] = MakeShared<CustomFusionStartTaskCodeBuilder>(GetOm2CodegenModelBuilderUtAst());
  task_builders[2] = MakeShared<CustomFusionEndTaskCodeBuilder>(GetOm2CodegenModelBuilderUtAst());
  task_builders[3] = MakeShared<CustomEndGraphTaskCodeBuilder>(GetOm2CodegenModelBuilderUtAst());
  ASSERT_EQ(BuildCodegenModelWithTaskGenerators(ge_root_model, task_builders, doc), SUCCESS);
  ASSERT_EQ(task_builders.size(), 4U);

  auto *fusion_start_builder = dynamic_cast<CustomFusionStartTaskCodeBuilder *>(task_builders[1].get());
  auto *fusion_end_builder = dynamic_cast<CustomFusionEndTaskCodeBuilder *>(task_builders[2].get());
  auto *end_graph_builder = dynamic_cast<CustomEndGraphTaskCodeBuilder *>(task_builders[3].get());
  ASSERT_NE(fusion_start_builder, nullptr);
  ASSERT_NE(fusion_end_builder, nullptr);
  ASSERT_NE(end_graph_builder, nullptr);

  fusion_start_builder->SetSemanticStreamIdForTest(11U);
  fusion_end_builder->SetSemanticStreamIdForTest(12U);
  end_graph_builder->SetSemanticStreamIdForTest(13U);

  auto &task_defs = *ge_root_model->GetSubgraphInstanceNameToModel().begin()->second->GetModelTaskDefPtr();
  task_defs.mutable_task(1)->set_stream_id(101U);
  task_defs.mutable_task(2)->set_stream_id(102U);
  task_defs.mutable_task(3)->set_stream_id(103U);

  AstContext ast_ctx;
  AstBuildContext ast(ast_ctx);
  std::vector<BodyItem> items;
  ASSERT_EQ(fusion_start_builder->RenderDistribution(items), SUCCESS);
  ASSERT_EQ(fusion_end_builder->RenderDistribution(items), SUCCESS);
  ASSERT_EQ(end_graph_builder->RenderDistribution(items), SUCCESS);
  const auto nodes = ast.Body(items);

  std::stringstream output;
  EmitCodeFromNodes(nodes, output);
  const std::string code = output.str();
  EXPECT_NE(code.find("KernelFusionStartDistribute(stream_list_[11])"), std::string::npos);
  EXPECT_NE(code.find("KernelFusionEndDistribute(stream_list_[12])"), std::string::npos);
  EXPECT_NE(code.find("EndGraphTaskDistribute(model_handle_, stream_list_[13])"), std::string::npos);
  EXPECT_EQ(code.find("stream_list_[101]"), std::string::npos);
  EXPECT_EQ(code.find("stream_list_[102]"), std::string::npos);
  EXPECT_EQ(code.find("stream_list_[103]"), std::string::npos);
}

TEST_F(Om2CodegenModelBuilderUt, BuildKernelRegistry_TFAicpu_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithTfAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  // TF AiCPU (add1) + CC AiCPU (add2) + TF session task
  ASSERT_EQ(doc.kernel_registry.binaries.size(), 3U);
  EXPECT_EQ(doc.runtime.kernel_bin_num, 3U);

  // add1 is a TF AiCPU task (MODEL_TASK_KERNEL_EX)
  const auto &tf_binary = doc.kernel_registry.binaries[0];
  EXPECT_EQ(tf_binary.kind, KernelBinaryKind::kAicpu);
  EXPECT_EQ(tf_binary.kernel_name, "TFOperateAPI");
  EXPECT_EQ(tf_binary.op_type, "Add");
  EXPECT_EQ(tf_binary.so_name, "libtf_kernels.so");
  EXPECT_EQ(tf_binary.op_kernel_lib, "TFKernel");
  EXPECT_EQ(tf_binary.func_handle_index, 0U);

  // add2 is a CC AiCPU task (MODEL_TASK_KERNEL)
  const auto &aicpu_binary = doc.kernel_registry.binaries[1];
  EXPECT_EQ(aicpu_binary.kind, KernelBinaryKind::kAicpu);
  EXPECT_EQ(aicpu_binary.op_type, "Add");
  EXPECT_EQ(aicpu_binary.op_kernel_lib, "AICPUKernel");
  EXPECT_EQ(aicpu_binary.func_handle_index, 1U);

  // TF session task registered automatically
  const auto &session_binary = doc.kernel_registry.binaries[2];
  EXPECT_EQ(session_binary.kind, KernelBinaryKind::kAicpu);
  EXPECT_EQ(session_binary.kernel_name, "TFOperateAPI");
  EXPECT_EQ(session_binary.func_handle_index, 2U);

  ASSERT_EQ(doc.kernel_registry.func_handle_indices.size(), 3U);
  EXPECT_EQ(doc.kernel_registry.func_handle_indices.at("Add"), 0U);
  EXPECT_NE(doc.kernel_registry.func_handle_indices.find("TfSessionTask"),
            doc.kernel_registry.func_handle_indices.end());
}

TEST_F(Om2CodegenModelBuilderUt, BuildKernelRegistry_TFAicpu_DuplicateOpType_Ok) {
  auto graph = gert::ShareGraph::Aicpu4thGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  gert::AiCpuTfTaskDefFaker tf_aicpu_task_def_faker;
  auto ge_root_model = builder.AddTaskDef("add1", tf_aicpu_task_def_faker)
                           .AddTaskDef("add2", tf_aicpu_task_def_faker)
                           .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();
  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
    }
  }
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);

  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  // Two TF AiCPU tasks with same op_type "Add" should only register once + TF session
  ASSERT_EQ(doc.kernel_registry.binaries.size(), 2U);
  EXPECT_EQ(doc.kernel_registry.func_handle_indices.count("Add"), 1U);
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIo_NoTaskConcat_ExpandOk) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithNoTaskConcatOutput();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  // 2 个输入 + 2 个展开输出项。
  ASSERT_EQ(doc.model_io.entries.size(), 4U);
  EXPECT_EQ(doc.model_io.input_count, 2U);
  EXPECT_EQ(doc.model_io.output_count, 1U);  // 真实模型输出数不随展开增加

  // 输入项：data0 offset=512，data1 offset=768。
  EXPECT_EQ(doc.model_io.entries[0].is_input, true);
  EXPECT_EQ(doc.model_io.entries[0].memory_offset, 512);
  EXPECT_EQ(doc.model_io.entries[0].addr_offset, 0);

  EXPECT_EQ(doc.model_io.entries[1].is_input, true);
  EXPECT_EQ(doc.model_io.entries[1].memory_offset, 768);
  EXPECT_EQ(doc.model_io.entries[1].addr_offset, 0);

  // 输出项 0：add0 输出，addr_offset=0。
  EXPECT_EQ(doc.model_io.entries[2].is_input, false);
  EXPECT_EQ(doc.model_io.entries[2].index, 0U);
  EXPECT_EQ(doc.model_io.entries[2].memory_offset, 1024);
  EXPECT_EQ(doc.model_io.entries[2].addr_offset, 0);

  // 输出项 1：add1 输出，addr_offset=512。
  EXPECT_EQ(doc.model_io.entries[3].is_input, false);
  EXPECT_EQ(doc.model_io.entries[3].index, 0U);
  EXPECT_EQ(doc.model_io.entries[3].memory_offset, 1536);
  EXPECT_EQ(doc.model_io.entries[3].addr_offset, 512);

  // host_args_offsets 数量与 entries 对齐。
  ASSERT_EQ(doc.args_table.host_args_offsets.size(), doc.model_io.entries.size());
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIo_NoTaskConcat_NegativeOffset_Fail) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithNoTaskConcatNegativeOffset();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), PARAM_INVALID);
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIo_NoTaskConcatReuseDimOne_CopyOnly) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithNoTaskConcatOutputReuseDimOne();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  // 2 个输入 + 1 个拷贝输出；dim=1 不展开地址刷新。
  ASSERT_EQ(doc.model_io.entries.size(), 3U);
  EXPECT_EQ(doc.model_io.input_count, 2U);
  EXPECT_EQ(doc.model_io.output_count, 1U);

  EXPECT_EQ(doc.model_io.entries[2].is_input, false);
  EXPECT_EQ(doc.model_io.entries[2].index, 0U);
  EXPECT_EQ(doc.model_io.entries[2].memory_offset, 1024);
  EXPECT_EQ(doc.model_io.entries[2].addr_offset, 0);
  EXPECT_FALSE(doc.model_io.entries[2].is_addr_refreshable);

  ASSERT_EQ(doc.args_table.host_args_offsets.size(), doc.model_io.entries.size());
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIo_MixedOutputs_OutputCountCorrect) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithMixedOutputs();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  // 2 个输入 + 2 个展开输出项 + 1 个普通输出项。
  ASSERT_EQ(doc.model_io.entries.size(), 5U);
  EXPECT_EQ(doc.model_io.input_count, 2U);
  EXPECT_EQ(doc.model_io.output_count, 2U);  // 真实模型输出数为 2

  // output[0] 展开项：index=0。
  EXPECT_EQ(doc.model_io.entries[2].index, 0U);
  EXPECT_EQ(doc.model_io.entries[2].memory_offset, 1024);
  EXPECT_EQ(doc.model_io.entries[2].addr_offset, 0);

  EXPECT_EQ(doc.model_io.entries[3].index, 0U);
  EXPECT_EQ(doc.model_io.entries[3].memory_offset, 1536);
  EXPECT_EQ(doc.model_io.entries[3].addr_offset, 512);

  // output[1] 普通项：index=1。
  EXPECT_EQ(doc.model_io.entries[4].index, 1U);
  EXPECT_EQ(doc.model_io.entries[4].memory_offset, 2048);
  EXPECT_EQ(doc.model_io.entries[4].addr_offset, 0);
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIo_PhonyConcat_ExpandOk) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithPhonyConcatOutput();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  // 2 个输入 + 2 个 PhonyConcat 展开输出项。
  ASSERT_EQ(doc.model_io.entries.size(), 4U);
  EXPECT_EQ(doc.model_io.input_count, 2U);
  EXPECT_EQ(doc.model_io.output_count, 1U);  // 真实模型输出数不随展开增加

  // 输出项 0：PhonyConcat 的 add0 输入。
  EXPECT_EQ(doc.model_io.entries[2].is_input, false);
  EXPECT_EQ(doc.model_io.entries[2].index, 0U);
  EXPECT_EQ(doc.model_io.entries[2].memory_offset, 1024);
  EXPECT_EQ(doc.model_io.entries[2].addr_offset, 0);

  // 输出项 1：PhonyConcat 的 add1 输入。
  EXPECT_EQ(doc.model_io.entries[3].is_input, false);
  EXPECT_EQ(doc.model_io.entries[3].index, 0U);
  EXPECT_EQ(doc.model_io.entries[3].memory_offset, 1536);
  EXPECT_EQ(doc.model_io.entries[3].addr_offset, 512);

  ASSERT_EQ(doc.args_table.host_args_offsets.size(), doc.model_io.entries.size());
}

TEST_F(Om2CodegenModelBuilderUt, BuildModelIo_TwoNetOutputs_DenseIndexing) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithTwoNetOutputs();
  ASSERT_NE(ge_root_model, nullptr);
  Om2CodegenModel doc;
  ASSERT_EQ(BuildCodegenModel(ge_root_model, doc), SUCCESS);

  // 2 个输入 + 2 个输出。
  ASSERT_EQ(doc.model_io.entries.size(), 4U);
  EXPECT_EQ(doc.model_io.input_count, 2U);
  EXPECT_EQ(doc.model_io.output_count, 2U);  // 两个 NetOutput 各有一个真实输出

  // netoutput0 输出：index=0。
  EXPECT_EQ(doc.model_io.entries[2].is_input, false);
  EXPECT_EQ(doc.model_io.entries[2].index, 0U);
  EXPECT_EQ(doc.model_io.entries[2].memory_offset, 1024);

  // netoutput1 输出：index=1，索引不重置。
  EXPECT_EQ(doc.model_io.entries[3].is_input, false);
  EXPECT_EQ(doc.model_io.entries[3].index, 1U);
  EXPECT_EQ(doc.model_io.entries[3].memory_offset, 2048);
}
}  // namespace ge
