/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/common/helper/om2_package_helper.h"
#include "common/helper/om2/zip_archive_writer.h"
#include "common/helper/om2/json_file.h"
#include "framework/runtime/om2_model_executor.h"
#include "ge/ge_ir_build.h"
#include "file_utils.h"
#include "runtime/om2/zip_archive_reader.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include "common/env_path.h"
#include "mmpa/mmpa_api.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/tensor_utils.h"
#include "graph/utils/file_utils.h"
#include "graph_metadef/depends/checker/tensor_check_utils.h"

#include "ge_runtime_stub/include/common/share_graph.h"
#include "ge_runtime_stub/include/faker/ge_model_builder.h"
#include "ge_runtime_stub/include/faker/aicore_taskdef_faker.h"
#include "ge_runtime_stub/include/faker/aicpu_taskdef_faker.h"

#include <cinttypes>
#include <securec.h>
#include "faker/task_def_faker.h"
#include "aicpu_engine_struct.h"
#include "aicpu_task_struct.h"
#include "engine/aicpu/kernel/aicpu_ext_info_handle.h"

namespace ge {
namespace {
using AicpuShapeAndType = aicpu::FWKAdapter::ShapeAndType;
using AicpuExtInfo = aicpu::FWKAdapter::ExtInfo;
using AsyncWaitInfo = aicpu::FWKAdapter::AsyncWait;
using WorkSpaceInfo = aicpu::FWKAdapter::WorkSpaceInfo;
using AicpuSessionInfo = SessionInfo;

static void SyncKernelNameFromOpDesc(const GeModelPtr &ge_model) {
  auto model_task_def = ge_model->GetModelTaskDefPtr();
  if (model_task_def == nullptr) { return; }
  const auto &graph = ge_model->GetGraph();
  if (graph == nullptr) { return; }
  for (int i = 0; i < model_task_def->task_size(); ++i) {
    auto *task_def = model_task_def->mutable_task(i);
    for (const auto &node : graph->GetDirectNode()) {
      auto op_desc = node->GetOpDesc();
      if (op_desc == nullptr) { continue; }
      std::string kernel_name;
      if (ge::AttrUtils::GetStr(op_desc, "_kernelname", kernel_name)) {
        task_def->mutable_kernel()->set_kernel_name(kernel_name);
      }
    }
  }
}

static void SyncKernelNameForAllModels(const GeRootModelPtr &ge_root_model) {
  if (ge_root_model == nullptr) { return; }
  for (const auto &kv : ge_root_model->GetSubgraphInstanceNameToModel()) {
    SyncKernelNameFromOpDesc(kv.second);
  }
}

struct AicpuTaskArgs {
  aicpu::AicpuParamHead head;
  uint64_t io_addrp[6];
} __attribute__((packed));
void AppendShape(aicpu::FWKAdapter::FWKTaskExtInfoType type, size_t shape_num, std::string &out) {
  size_t len = sizeof(AicpuShapeAndType) * shape_num + sizeof(AicpuExtInfo);
  vector<char> vec(len, 0);
  AicpuExtInfo *aicpu_ext_info = reinterpret_cast<AicpuExtInfo *>(vec.data());
  aicpu_ext_info->infoType = type;
  aicpu_ext_info->infoLen = sizeof(AicpuShapeAndType) * shape_num;
  AicpuShapeAndType input_shape_and_types[shape_num] = {};
  for (auto m = 0U; m < shape_num; m++) {
    input_shape_and_types[m].dims[0] = 5;
  }
  memcpy_s(aicpu_ext_info->infoMsg, sizeof(AicpuShapeAndType) * shape_num,
           reinterpret_cast<void *>(input_shape_and_types), sizeof(AicpuShapeAndType) * shape_num);

  std::string s(vec.data(), len);
  out.append(s);
}

void AppendSessionInfo(std::string &out) {
  size_t len = sizeof(AicpuSessionInfo) + sizeof(AicpuExtInfo);
  vector<char> vec(len, 0);
  AicpuExtInfo *aicpu_ext_info = reinterpret_cast<AicpuExtInfo *>(vec.data());
  aicpu_ext_info->infoType = aicpu::FWKAdapter::FWK_ADPT_EXT_SESSION_INFO;
  aicpu_ext_info->infoLen = sizeof(AicpuSessionInfo);
  AicpuSessionInfo session = {};
  *(ge::PtrToPtr<char, AicpuSessionInfo>(aicpu_ext_info->infoMsg)) = session;
  std::string s(vec.data(), len);
  out.append(s);
}

void AppendBitMap(std::string &out) {
  size_t len = sizeof(uint64_t) + sizeof(AicpuExtInfo);
  vector<char> vec(len, 0);
  AicpuExtInfo *aicpu_ext_info = reinterpret_cast<AicpuExtInfo *>(vec.data());
  aicpu_ext_info->infoType = aicpu::FWKAdapter::FWK_ADPT_EXT_BITMAP;
  aicpu_ext_info->infoLen = sizeof(uint64_t);
  *(ge::PtrToPtr<char, uint64_t>(aicpu_ext_info->infoMsg)) = 1;
  std::string s(vec.data(), len);
  out.append(s);
}

void AppendUpdateAddr(std::string &out) {
  size_t len = sizeof(uint32_t) + sizeof(AicpuExtInfo);
  vector<char> vec(len, 0);
  AicpuExtInfo *aicpu_ext_info = reinterpret_cast<AicpuExtInfo *>(vec.data());
  aicpu_ext_info->infoType = aicpu::FWKAdapter::FWK_ADPT_EXT_UPDATE_ADDR;
  aicpu_ext_info->infoLen = sizeof(uint32_t);
  *(ge::PtrToPtr<char, uint32_t>(aicpu_ext_info->infoMsg)) = 1;
  std::string s(vec.data(), len);
  out.append(s);
}

void AppendTopicType(std::string &out) {
  size_t len = sizeof(int32_t) + sizeof(AicpuExtInfo);
  vector<char> vec(len, 0);
  AicpuExtInfo *aicpu_ext_info = reinterpret_cast<AicpuExtInfo *>(vec.data());
  aicpu_ext_info->infoType = aicpu::FWKAdapter::FWK_ADPT_EXT_TOPIC_TYPE;
  aicpu_ext_info->infoLen = sizeof(int32_t);
  *(ge::PtrToPtr<char, int32_t>(aicpu_ext_info->infoMsg)) = 1;
  std::string s(vec.data(), len);
  out.append(s);
}

std::string GetFakeExtInfo() {
  std::string ext_info;
  AppendShape(aicpu::FWKAdapter::FWK_ADPT_EXT_INPUT_SHAPE, 2, ext_info);
  AppendShape(aicpu::FWKAdapter::FWK_ADPT_EXT_OUTPUT_SHAPE, 1, ext_info);
  AppendSessionInfo(ext_info);
  AppendBitMap(ext_info);
  AppendUpdateAddr(ext_info);
  AppendTopicType(ext_info);
  return ext_info;
}

GeRootModelPtr CreateGeRootModelWithAicoreOp() {
  auto graph = gert::ShareGraph::AicoreStaticGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder
          .AddTaskDef("Add",
                      gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i_instance0*}{i_instance1*}{o_instance0*}"))
          .FakeTbeBin({"Add"})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if ((op_desc->GetType() == DATA)) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, 0);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);

  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithInternalConstOp() {
  auto ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }

  auto &compute_graph = ge_root_model->GetRootGraph();
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetType() != "Add") {
      continue;
    }
    op_desc->SetIsInputConst({true, true});
    auto input_desc0 = op_desc->MutableInputDesc(0);
    auto input_desc1 = op_desc->MutableInputDesc(1);
    if ((input_desc0 == nullptr) || (input_desc1 == nullptr)) {
      return nullptr;
    }
    TensorUtils::SetDataOffset(*input_desc0, 0);
    TensorUtils::SetDataOffset(*input_desc1, 200704);
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint8_t> weights_value(401408, 1U);
  ge_model->SetWeight(Buffer::CopyFrom(weights_value.data(), weights_value.size()));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, static_cast<int64_t>(weights_value.size()));
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithFileConstOp() {
  auto ge_root_model = CreateGeRootModelWithInternalConstOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }

  auto &compute_graph = ge_root_model->GetRootGraph();
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if (op_desc->GetName() == "data2") {
      op_desc->SetType(FILECONSTANT);
      (void)AttrUtils::SetStr(op_desc, ATTR_NAME_LOCATION, "weight_combined.bin");
      (void)AttrUtils::SetInt(op_desc, ATTR_NAME_OFFSET, 64);
      (void)AttrUtils::SetInt(op_desc, ATTR_NAME_LENGTH, 200704);
    } else if (op_desc->GetType() == "Add") {
      op_desc->SetIsInputConst({true, false});
    }
  }
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
      builder
          .AddTaskDef("Add",
                      gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i0*}{i1*}{o0*}{ws0*}"))
          .FakeTbeBin({"Add"})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if ((op_desc->GetType() == DATA)) {
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
    if ((op_desc->GetType() == DATA)) {
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
        auto ext_info = GetFakeExtInfo();
        auto kernel_def = task_def->mutable_kernel();
        AicpuTaskArgs args = {};
        args.head.length = sizeof(args);
        args.head.ioAddrNum = 3;
        kernel_def->set_args(reinterpret_cast<const char *>(&args), args.head.length);
        kernel_def->set_args_size(args.head.length);
        kernel_def->set_kernel_ext_info(ext_info.c_str(), ext_info.size());
        kernel_def->set_kernel_ext_info_size(ext_info.size());
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

GeRootModelPtr CreateGeRootModelWithCustAicpuOp() {
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
    if ((op_desc->GetType() == DATA)) {
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
        // kernel_type=7 is CUST_AI_CPU, do NOT override to 6 (AI_CPU)
        auto ext_info = GetFakeExtInfo();
        auto kernel_def = task_def->mutable_kernel();
        AicpuTaskArgs args = {};
        args.head.length = sizeof(args);
        args.head.ioAddrNum = 3;
        kernel_def->set_args(reinterpret_cast<const char *>(&args), args.head.length);
        kernel_def->set_args_size(args.head.length);
        kernel_def->set_kernel_ext_info(ext_info.c_str(), ext_info.size());
        kernel_def->set_kernel_ext_info_size(ext_info.size());
      }
    }
  }

  // Set CUST_AICPU kernel binary on op_desc extended attributes
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() != DATA) && (op_desc->GetType() != NETOUTPUT)) {
      std::vector<char> kernel_bin(64, '\0');
      auto cust_aicpu_kernel = std::make_shared<ge::OpKernelBin>("libcust_aicpu_kernel.so", std::move(kernel_bin));
      op_desc->SetExtAttr(ge::OP_EXTATTR_CUSTAICPU_KERNEL, cust_aicpu_kernel);
      (void)ge::AttrUtils::SetStr(op_desc, "kernelSo", "libcust_aicpu_kernel.so");
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
  for (const auto &node : graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() == "Add")) {
      op_desc->AppendIrInput("x1", kIrInputRequired);
      op_desc->AppendIrInput("x2", kIrInputRequired);
      op_desc->AppendIrOutput("y", kIrOutputRequired);
    }
  }
  gert::GeModelBuilder builder(graph);
  gert::AiCpuTfTaskDefFaker tf_aicpu_task_def_faker;
  gert::AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto ge_root_model = builder.AddTaskDef("add1", tf_aicpu_task_def_faker.SetNeedMemcpy(false))
                              .AddTaskDef("add2", aicpu_task_def_faker.SetNeedMemcpy(false))
                              .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if ((op_desc->GetType() == DATA)) {
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
      if ((task_def != nullptr) && task_def->has_kernel_ex()) {
        auto ext_info = GetFakeExtInfo();
        auto kernel_ex_def = task_def->mutable_kernel_ex();
        AicpuTaskArgs args = {};
        args.head.length = sizeof(args);
        args.head.ioAddrNum = 3;
        kernel_ex_def->set_args(reinterpret_cast<const char *>(&args), args.head.length);
        kernel_ex_def->set_args_size(args.head.length);
        kernel_ex_def->set_kernel_ext_info(ext_info.c_str(), ext_info.size());
        kernel_ex_def->set_kernel_ext_info_size(ext_info.size());
        kernel_ex_def->set_task_info("kernel_ex_test_task");
        kernel_ex_def->set_task_info_size(kernel_ex_def->task_info().size());
      }
      if ((task_def != nullptr) && task_def->has_kernel()) {
        task_def->mutable_kernel()->mutable_context()->set_kernel_type(6U);
        auto ext_info = GetFakeExtInfo();
        auto kernel_def = task_def->mutable_kernel();
        AicpuTaskArgs args = {};
        args.head.length = sizeof(args);
        args.head.ioAddrNum = 3;
        kernel_def->set_args(reinterpret_cast<const char *>(&args), args.head.length);
        kernel_def->set_args_size(args.head.length);
        kernel_def->set_kernel_ext_info(ext_info.c_str(), ext_info.size());
        kernel_def->set_kernel_ext_info_size(ext_info.size());
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

GeRootModelPtr CreateGeRootModelWithAicoreOpOfDynamicIo() {
  auto graph = std::make_shared<ComputeGraph>("g1");
  GeTensorDesc tensor_desc(GeShape({1, 1, 224, 224}), FORMAT_NCHW, DT_FLOAT);
  auto data_x_desc = std::make_shared<OpDesc>("data_x", DATA);
  (void)data_x_desc->AddInputDesc(tensor_desc);
  (void)data_x_desc->AddOutputDesc(tensor_desc);
  AttrUtils::SetInt(data_x_desc, ATTR_NAME_INDEX, 0);
  auto data_x = graph->AddNode(data_x_desc);
  auto data_dx_desc = std::make_shared<OpDesc>("data_dx", DATA);
  (void)data_dx_desc->AddInputDesc(tensor_desc);
  (void)data_dx_desc->AddOutputDesc(tensor_desc);
  AttrUtils::SetInt(data_dx_desc, ATTR_NAME_INDEX, 1);
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
  netoutput_desc->SetSrcName({"add1"});
  netoutput_desc->SetSrcIndex({0});
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder
          .AddTaskDef("Add",
                      gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i_desc0}{i_desc1}{o_desc0}"))
          .FakeTbeBin({"Add"})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if ((op_desc->GetType() == DATA)) {
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

std::string GetParentDir(const std::string &path) {
  const auto pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return {};
  }
  return path.substr(0, pos);
}

void WriteTextFile(const std::string &file_path, const std::string &content) {
  const auto parent_dir = GetParentDir(file_path);
  ASSERT_FALSE(parent_dir.empty());
  ASSERT_EQ(CreateDir(parent_dir), 0);
  std::ofstream ofs(file_path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(ofs.is_open());
  ofs << content;
  ASSERT_TRUE(ofs.good());
}

void WriteBinaryFile(const std::string &file_path, const std::vector<uint8_t> &content) {
  const auto parent_dir = GetParentDir(file_path);
  ASSERT_FALSE(parent_dir.empty());
  ASSERT_EQ(CreateDir(parent_dir), 0);
  std::ofstream ofs(file_path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(ofs.is_open());
  ofs.write(reinterpret_cast<const char *>(content.data()), static_cast<std::streamsize>(content.size()));
  ASSERT_TRUE(ofs.good());
}

void RunCommandOrAssert(const std::string &command) {
  const std::string wrapped_command = "env ASAN_OPTIONS=detect_leaks=0:halt_on_error=0 LSAN_OPTIONS=exitcode=0 " +
                                      command;
  ASSERT_EQ(system(wrapped_command.c_str()), 0) << wrapped_command;
}

std::string MakeFakeOm2ManifestJson() {
  return R"({
    "atc_command": "",
    "model_num": 1,
    "om2_version": "0"
})";
}

std::string MakeFakeOm2ModelMetaJson() {
  return R"({
    "dynamic_batch_info": [],
    "dynamic_output_shape": [],
    "dynamic_type": 0,
    "inputs": [
      {"data_type": "DT_FLOAT", "format": "NCHW", "index": 0, "name": "data1",
       "shape": [1, 1, 224, 224], "shape_range": [], "shape_v2": [1, 1, 224, 224], "size": 0},
      {"data_type": "DT_FLOAT", "format": "NCHW", "index": 1, "name": "data2",
       "shape": [1, 1, 224, 224], "shape_range": [], "shape_v2": [1, 1, 224, 224], "size": 0}
    ],
    "name": "g1",
    "outputs": [
      {"data_type": "DT_FLOAT", "format": "NCHW", "index": 0, "name": "output",
       "shape": [1, 1, 224, 224], "shape_range": [], "size": 0}
    ],
    "work_size": 2048,
    "user_designate_shape_order": []
})";
}

std::string MakeFakeOm2ConstantsConfigJson() {
  return R"({
    "internal_weight_size": 16,
    "consts": {
      "const_0": {
        "index": 0,
        "type": "INTERNAL",
        "file_name": "",
        "offset": 0,
        "size": 16,
        "op_name": "const_0"
      }
    }
})";
}

std::string MakeFakeOm2InterfaceHeader() {
  return R"(#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
int Om2ModelCreate(void **model_handle, void **rt_model_handle, const char **bin_files, const void **bin_data,
                   size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id,
                   uint32_t model_id, void *instance_handle);
int Om2ModelLoad(void **model_handle);
int Om2ModelRunAsync(void **model_handle, void *stream, int input_count, void **input_data, int output_count,
                     void **output_data);
int Om2ModelRun(void **model_handle, int input_count, void **input_data, int output_count, void **output_data, int32_t stream_sync_timeout);
int Om2ModelDestroy(void **model_handle);
}
)";
}

std::string MakeFakeOm2LoadAndRunCpp() {
  return R"(#include "g1_interface.h"

#include <new>

namespace {
struct FakeModel {
  uint64_t session_id;
};
}

extern "C" int Om2ModelCreate(void **model_handle, void **rt_model_handle, const char **, const void **, size_t *, int,
                              void **constants, void *work_ptr, uint64_t *session_id, uint32_t, void *) {
  if ((model_handle == nullptr) || (rt_model_handle == nullptr) || (work_ptr == nullptr) || (constants == nullptr) ||
      (constants[0] == nullptr)) {
    return 1;
  }
  auto *model = new (std::nothrow) FakeModel();
  if (model == nullptr) {
    return 1;
  }
  model->session_id = (session_id == nullptr) ? 0UL : *session_id;
  *model_handle = model;
  *rt_model_handle = reinterpret_cast<void *>(0x12345678U);
  return 0;
}

extern "C" int Om2ModelLoad(void **model_handle) {
  return ((model_handle == nullptr) || (*model_handle == nullptr)) ? 1 : 0;
}

extern "C" int Om2ModelRunAsync(void **model_handle, void *, int input_count, void **input_data, int output_count,
                                void **output_data) {
  if ((model_handle == nullptr) || (*model_handle == nullptr) || (input_data == nullptr) || (output_data == nullptr)) {
    return 1;
  }
  return (input_count == 2 && output_count == 1) ? 0 : 1;
}

extern "C" int Om2ModelRun(void **model_handle, int input_count, void **input_data, int output_count,
                           void **output_data, int32_t stream_sync_timeout) {
  if ((model_handle == nullptr) || (*model_handle == nullptr) || (input_data == nullptr) || (output_data == nullptr)) {
    return 1;
  }
  (void)stream_sync_timeout;  // Mock 实现不使用该参数
  return (input_count == 2 && output_count == 1) ? 0 : 1;
}

extern "C" int Om2ModelDestroy(void **model_handle) {
  if ((model_handle == nullptr) || (*model_handle == nullptr)) {
    return 0;
  }
  delete static_cast<FakeModel *>(*model_handle);
  *model_handle = nullptr;
  return 0;
}
)";
}

std::string MakeFakeOm2EmptyCpp() {
  return R"(#include "g1_interface.h"
)";
}

std::string MakeFakeOm2CMakeLists() {
  return R"(cmake_minimum_required(VERSION 3.10)
project(g1_om2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(g1_om2 SHARED
  g1_resources.cpp
  g1_kernel_reg.cpp
  g1_load_and_run.cpp
  g1_args_manager.cpp
)

set_target_properties(g1_om2 PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
)";
}

std::string MakeFakeOm2OpAttrJson() {
  return R"({
  "test_op": {
    "_datadump_original_op_names": {
      "type": "LIST_STRING",
      "value": ["original_op1", "original_op2"]
    }
  }
})";
}

void CreateFakeOm2File(const std::string &work_dir, const std::string &output_file) {
  const std::string runtime_dir = PathUtils::Join({work_dir, "fake_om2_runtime"});
  const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
  const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});
  const std::string constant_path = PathUtils::Join({work_dir, "constant_0"});
  const std::string constants_config_path = PathUtils::Join({work_dir, "model_0_constants_config.json"});

  (void)PathUtils::RemoveDirectories(runtime_dir);
  ASSERT_EQ(CreateDir(runtime_dir), 0);
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeFakeOm2InterfaceHeader());
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeFakeOm2EmptyCpp());
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeFakeOm2EmptyCpp());
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeFakeOm2EmptyCpp());
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeFakeOm2LoadAndRunCpp());
  WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeFakeOm2CMakeLists());

  RunCommandOrAssert("cmake -S " + runtime_dir + " -B " + build_dir);
  RunCommandOrAssert("cmake --build " + build_dir + " -j1");
  ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

  WriteBinaryFile(constant_path, {1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U});
  WriteTextFile(constants_config_path, MakeFakeOm2ConstantsConfigJson());

  ZipArchiveWriter zip_writer(output_file);
  ASSERT_TRUE(zip_writer.IsMemFileOpened());
  const auto manifest = MakeFakeOm2ManifestJson();
  const auto model_meta = MakeFakeOm2ModelMetaJson();
  const auto op_attr = MakeFakeOm2OpAttrJson();
  ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/debug/op_attr.json", op_attr.data(), op_attr.size(), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
  ASSERT_TRUE(zip_writer.WriteFile("data/constants/constant_0", constant_path, false));
  ASSERT_TRUE(zip_writer.WriteFile("data/constants/model_0_constants_config.json", constants_config_path, false));
  ASSERT_TRUE(zip_writer.SaveModelDataToFile());
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);
}

void ConstructOm2IoTensors(std::vector<gert::Tensor> &input_tensors, std::vector<gert::Tensor> &output_tensors,
                           std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) {
  input_tensors.resize(2U);
  output_tensors.resize(1U);
  TensorCheckUtils::ConstructGertTensor(input_tensors[0], {1, 1, 224, 224}, DT_FLOAT, FORMAT_NCHW);
  TensorCheckUtils::ConstructGertTensor(input_tensors[1], {1, 1, 224, 224}, DT_FLOAT, FORMAT_NCHW);
  TensorCheckUtils::ConstructGertTensor(output_tensors[0], {1, 1, 224, 224}, DT_FLOAT, FORMAT_NCHW);
  inputs = {&input_tensors[0], &input_tensors[1]};
  outputs = {&output_tensors[0]};
}

void ExpectOm2ArchiveFiles(const RAIIZipArchive &archive, const std::set<std::string> &expect_files) {
  const auto file_names = archive.ListFiles();
  EXPECT_EQ(file_names.size(), expect_files.size());
  for (const auto &file_name : file_names) {
    EXPECT_EQ(expect_files.count(file_name), 1);
  }
}

JsonFile ExtractConstantsConfig(const RAIIZipArchive &archive, const std::string &zip_base_name) {
  size_t constants_config_size = 0U;
  const auto constants_config_buf =
      archive.ExtractToMem(zip_base_name + "/data/constants/model_0_constants_config.json", constants_config_size);
  EXPECT_NE(constants_config_buf, nullptr);
  if (constants_config_buf == nullptr) {
    return JsonFile(nullptr, 0U);
  }
  return JsonFile(reinterpret_cast<const uint8_t *>(constants_config_buf.get()), constants_config_size);
}

// -----------------------------------------------------------------------
// Helper: 在现有 AiCore 模型上添加 CMO task_def
// -----------------------------------------------------------------------
GeRootModelPtr CreateGeRootModelWithCmoTask(uint32_t cmo_type = 1U,
                                             uint32_t op_code = 3U) {
  auto ge_root_model = CreateGeRootModelWithAicoreOp();
  GE_ASSERT_NOTNULL(ge_root_model);
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  GE_ASSERT_NOTNULL(ge_model);
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  GE_ASSERT_NOTNULL(model_task_def);
  auto *task_def = model_task_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CMO));
  task_def->set_stream_id(0U);
  auto *cmo = task_def->mutable_cmo_task();
  cmo->set_cmo_type(cmo_type);
  cmo->set_logic_id(0);
  cmo->set_qos(0);
  cmo->set_part_id(0);
  cmo->set_pmg(0);
  cmo->set_op_code(op_code);
  cmo->set_num_inner(0);
  cmo->set_num_outer(0);
  cmo->set_length_inner(0);
  cmo->set_source_addr(0U);
  cmo->set_strider_outer(0);
  cmo->set_strider_inner(0);
  return ge_root_model;
}

// -----------------------------------------------------------------------
// Helper: 在现有 AiCore 模型上添加 Barrier task_def
// -----------------------------------------------------------------------
GeRootModelPtr CreateGeRootModelWithBarrierTask(int32_t barrier_count = 1) {
  auto ge_root_model = CreateGeRootModelWithAicoreOp();
  GE_ASSERT_NOTNULL(ge_root_model);
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  GE_ASSERT_NOTNULL(ge_model);
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  GE_ASSERT_NOTNULL(model_task_def);
  auto *task_def = model_task_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_BARRIER));
  task_def->set_stream_id(0U);
  auto *barrier = task_def->mutable_cmo_barrier_task();
  barrier->set_logic_id_num(static_cast<uint32_t>(barrier_count));
  for (int32_t i = 0; i < barrier_count; ++i) {
    auto *info = barrier->add_barrier_info();
    info->set_cmo_type(static_cast<uint32_t>(i));
    info->set_logic_id(static_cast<uint32_t>(i));
  }
  return ge_root_model;
}

// -----------------------------------------------------------------------
// Helper: 在现有 AiCore 模型上添加 CMO_ADDR task_def
// 空 args_format 会触发 BuildAutoArgsFormat 自动生成
// -----------------------------------------------------------------------
GeRootModelPtr CreateGeRootModelWithCmoAddrTask(bool with_explicit_format = false) {
  auto ge_root_model = CreateGeRootModelWithAicoreOp();
  GE_ASSERT_NOTNULL(ge_root_model);
  auto &compute_graph = ge_root_model->GetRootGraph();
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  GE_ASSERT_NOTNULL(ge_model);
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  GE_ASSERT_NOTNULL(model_task_def);

  // 为 Add 节点设置 tensor_desc 和 offset/max_size 属性（BuildAutoArgsFormat 需要）
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() != DATA) && (op_desc->GetType() != NETOUTPUT)) {
      GeTensorDesc tensor(GeShape({4, 4, 4, 4}), FORMAT_ND, DT_INT64);
      TensorUtils::SetSize(tensor, 2048);
      op_desc->UpdateInputDesc(0, tensor);
      (void)AttrUtils::SetInt(op_desc, "offset", 512);
    }
  }

  auto add_node = compute_graph->FindNode("add1");
  const int64_t op_index = (add_node != nullptr) ? add_node->GetOpDescBarePtr()->GetId() : 2;

  auto *task_def = model_task_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CMO_ADDR));
  task_def->set_stream_id(0U);
  auto *cmo_addr = task_def->mutable_cmo_addr_task();
  cmo_addr->set_op_index(static_cast<uint32_t>(op_index));
  cmo_addr->set_cmo_op_code(6);  // PREFETCH
  cmo_addr->set_src(0U);
  cmo_addr->set_num_inner(0);
  cmo_addr->set_num_outer(0);
  cmo_addr->set_length_inner(1024);
  cmo_addr->set_stride_outer(0);
  cmo_addr->set_stride_inner(0);
  if (with_explicit_format) {
    cmo_addr->set_args_format("{}{.32b}{#.32b64}{i_instance0*}{}");
  }
  return ge_root_model;
}

}  // namespace

class Om2St : public testing::Test {
 public:
  void SetUp() override {
    const ::testing::TestInfo *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    test_case_name = test_info->test_case_name();  // Om2ST
    test_work_dir = EnvPath().GetOrCreateCaseTmpPath(test_case_name);
    setenv("ASCEND_WORK_PATH", test_work_dir.c_str(), 1);
    const auto ascend_install_path = EnvPath().GetAscendInstallPath();
    setenv("ASCEND_HOME_PATH", ascend_install_path.c_str(), 1);
  }
  void TearDown() override {
    EnvPath().RemoveRfCaseTmpPath(test_case_name);
    unsetenv("ASCEND_WORK_PATH");
    unsetenv("ASCEND_HOME_PATH");
  }

 public:
  std::string test_case_name;
  std::string test_work_dir;
  const std::string kZipFileBaseName = "fake_test";
};

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithAicoreNode) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithInternalConst) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithInternalConstOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/constant_0",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
    "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);

  const JsonFile constants_json = ExtractConstantsConfig(archive, kZipFileBaseName);
  ASSERT_TRUE(constants_json.IsValid());
  EXPECT_EQ(constants_json.Raw().at("internal_weight_size"), JsonFile::json(401408));
  ASSERT_TRUE(constants_json.Raw().contains("consts"));
  const auto &consts = constants_json.Raw().at("consts");
  ASSERT_TRUE(consts.contains("constant_0"));
  ASSERT_TRUE(consts.contains("constant_1"));
  EXPECT_EQ(consts.at("constant_0").at("type"), JsonFile::json("INTERNAL"));
  EXPECT_EQ(consts.at("constant_0").at("offset"), JsonFile::json(0));
  EXPECT_EQ(consts.at("constant_0").at("size"), JsonFile::json(200704));
  EXPECT_EQ(consts.at("constant_1").at("type"), JsonFile::json("INTERNAL"));
  EXPECT_EQ(consts.at("constant_1").at("offset"), JsonFile::json(200704));
  EXPECT_EQ(consts.at("constant_1").at("size"), JsonFile::json(200704));
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithFileConstMeta) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithFileConstOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/constant_0",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);

  const JsonFile constants_json = ExtractConstantsConfig(archive, kZipFileBaseName);
  ASSERT_TRUE(constants_json.IsValid());
  EXPECT_EQ(constants_json.Raw().at("internal_weight_size"), JsonFile::json(401408));
  ASSERT_TRUE(constants_json.Raw().contains("consts"));
  const auto &consts = constants_json.Raw().at("consts");
  ASSERT_TRUE(consts.contains("constant_1"));
  ASSERT_TRUE(consts.contains("data2"));
  EXPECT_EQ(consts.at("constant_1").at("type"), JsonFile::json("INTERNAL"));
  EXPECT_EQ(consts.at("constant_1").at("offset"), JsonFile::json(0));
  EXPECT_EQ(consts.at("constant_1").at("size"), JsonFile::json(200704));
  EXPECT_EQ(consts.at("data2").at("type"), JsonFile::json("COMBINED"));
  EXPECT_EQ(consts.at("data2").at("file_name"), JsonFile::json("weight_combined.bin"));
  EXPECT_EQ(consts.at("data2").at("offset"), JsonFile::json(64));
  EXPECT_EQ(consts.at("data2").at("size"), JsonFile::json(200704));
}

TEST_F(Om2St, SaveOm2Model_Ok_SaveOnlineBufferWithAclgrphSaveModel) {
  Om2PackageHelper om2_packager;
  om2_packager.SetSaveMode(false);
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);

  ModelBufferData model_data;
  const std::string package_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_buffer.om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, package_file, model_data, false), SUCCESS);
  ASSERT_NE(model_data.data, nullptr);
  ASSERT_GT(model_data.length, 0U);

  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_saved"});
  EXPECT_EQ(aclgrphSaveModel(output_file, model_data), SUCCESS);
  EXPECT_EQ(mmAccess2((output_file + ".om2").c_str(), M_F_OK), EOK);
  EXPECT_NE(mmAccess2((output_file + ".om").c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file + ".om2", model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  EXPECT_NE(std::find(file_names.begin(), file_names.end(), "g1/manifest.json"), file_names.end());
}

TEST_F(Om2St, SaveOm2Model_Ok_RelocateExternalWeightsWithAclgrphSaveModel) {
  const std::string tmp_weight_dir = PathUtils::Join({test_work_dir, "tmp_weight"});
  const std::string weight_file_name = "weight_combined.bin";
  const std::string old_weight_path = PathUtils::Join({tmp_weight_dir, weight_file_name});
  const std::string output_file = PathUtils::Join({test_work_dir, "saved_model"});
  const std::string new_weight_path = PathUtils::Join({test_work_dir, "weight", weight_file_name});

  WriteBinaryFile(old_weight_path, {1U, 2U, 3U, 4U, 5U});

  ModelBufferData model;
  {
    ZipArchiveWriter zip_writer(PathUtils::Join({test_work_dir, "build_model.om2"}));
    ASSERT_TRUE(zip_writer.IsMemFileOpened());
    auto consts = JsonFile::json::object();
    consts["not_object"] = "skip";
    JsonFile internal_const;
    internal_const.Set("type", "INTERNAL").Set("file_path", old_weight_path);
    consts["internal_const"] = internal_const.Raw();
    JsonFile no_path_const;
    no_path_const.Set("type", "COMBINED").Set("file_name", "no_path.bin");
    consts["no_path_const"] = no_path_const.Raw();
    JsonFile empty_path_const;
    empty_path_const.Set("type", "COMBINED").Set("file_path", "");
    consts["empty_path_const"] = empty_path_const.Raw();
    JsonFile file_const;
    file_const.Set("index", 0U)
        .Set("type", "COMBINED")
        .Set("file_name", "")
        .Set("file_path", old_weight_path)
        .Set("offset", 0)
        .Set("size", 5)
        .Set("op_name", "file_const");
    consts["file_const"] = file_const.Raw();
    JsonFile constants_config;
    constants_config.Set("internal_weight_size", 0U).Set("consts", consts);
    const std::string constants_config_str = constants_config.Dump();
    ASSERT_TRUE(zip_writer.WriteBytes("data/constants/model_0_constants_config.json", constants_config_str.data(),
                                      constants_config_str.size(), false));
    const std::string no_consts_config = R"({"internal_weight_size":0})";
    ASSERT_TRUE(zip_writer.WriteBytes("data/constants/model_1_constants_config.json", no_consts_config.data(),
                                      no_consts_config.size(), false));
    const std::string skipped_consts_config = R"({"consts":{"internal":{"type":"INTERNAL"}}})";
    ASSERT_TRUE(zip_writer.WriteBytes("data/constants/model_2_constants_config.json", skipped_consts_config.data(),
                                      skipped_consts_config.size(), false));
    const std::string runtime_entry = "runtime";
    ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/runtime/libfake.so", runtime_entry.data(), runtime_entry.size(),
                                      false));
    const std::string manifest = R"({"archive_version":"1.0","model_num":3})";
    ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
    ASSERT_TRUE(zip_writer.SaveModelData(model, false));
  }

  EXPECT_EQ(aclgrphSaveModel(output_file, model), SUCCESS);
  EXPECT_EQ(mmAccess2((output_file + ".om2").c_str(), M_F_OK), EOK);
  EXPECT_EQ(mmAccess2(new_weight_path.c_str(), M_F_OK), EOK);
  EXPECT_NE(mmAccess2(old_weight_path.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file + ".om2", model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  EXPECT_NE(std::find(file_names.begin(), file_names.end(), "saved_model/data/model_0/runtime/libfake.so"),
            file_names.end());
  EXPECT_NE(std::find(file_names.begin(), file_names.end(),
                      "saved_model/data/constants/model_1_constants_config.json"),
            file_names.end());
  EXPECT_NE(std::find(file_names.begin(), file_names.end(),
                      "saved_model/data/constants/model_2_constants_config.json"),
            file_names.end());
  const JsonFile constants_json = ExtractConstantsConfig(archive, "saved_model");
  ASSERT_TRUE(constants_json.IsValid());
  const auto &rewritten_consts = constants_json.Raw().at("consts");
  EXPECT_EQ(rewritten_consts.at("not_object"), JsonFile::json("skip"));
  EXPECT_TRUE(rewritten_consts.at("internal_const").contains("file_path"));
  EXPECT_FALSE(rewritten_consts.at("no_path_const").contains("file_path"));
  EXPECT_EQ(rewritten_consts.at("empty_path_const").at("file_path"), JsonFile::json(""));
  const auto &file_const_json = constants_json.Raw().at("consts").at("file_const");
  EXPECT_EQ(file_const_json.at("file_name"), JsonFile::json(weight_file_name));
  EXPECT_FALSE(file_const_json.contains("file_path"));
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithAicoreOp2) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp2();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  EXPECT_EQ(file_names.size(), expect_files.size());
  for (const auto &file_name : file_names) {
    EXPECT_EQ(expect_files.count(file_name), 1);
  }
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithAicoreOpOfDynamicIo) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOpOfDynamicIo();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  EXPECT_EQ(file_names.size(), expect_files.size());
  for (const auto &file_name : file_names) {
    EXPECT_EQ(expect_files.count(file_name), 1);
  }
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithAicpuOp) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/model_0/model_meta.json",
    "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  EXPECT_EQ(file_names.size(), expect_files.size());
  for (const auto &file_name : file_names) {
    EXPECT_EQ(expect_files.count(file_name), 1);
  }
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithCustAicpuOp) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithCustAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  EXPECT_EQ(file_names.size(), expect_files.size());
  for (const auto &file_name : file_names) {
    EXPECT_EQ(expect_files.count(file_name), 1);
  }
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithTfAicpuOp) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithTfAicpuOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  EXPECT_EQ(file_names.size(), expect_files.size());
  for (const auto &file_name : file_names) {
    EXPECT_EQ(expect_files.count(file_name), 1);
  }
}

TEST_F(Om2St, LoadGeneratedOm2_Ok_ExecutorMainFlow) {
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_load.om2"});
  CreateFakeOm2File(test_work_dir, output_file);

  bool is_support = false;
  ASSERT_EQ(gert::IsOm2Model(output_file.c_str(), is_support), SUCCESS);
  ASSERT_TRUE(is_support);

  ge::ModelData model_data;
  ASSERT_EQ(gert::LoadOm2DataFromFile(output_file, model_data), SUCCESS);
  std::shared_ptr<void> model_data_guard(model_data.model_data, [](const void *const p) {
    if (p != nullptr) {
      delete[] static_cast<const uint8_t *>(p);
    }
  });

  is_support = false;
  ASSERT_EQ(gert::IsOm2Model(model_data.model_data, model_data.model_len, is_support), SUCCESS);
  ASSERT_TRUE(is_support);

  size_t work_size = 0U;
  size_t weight_size = 0U;
  ASSERT_EQ(gert::GetOm2MemAndWeightSize(output_file, work_size, weight_size), SUCCESS);
  EXPECT_EQ(work_size, 2048U);
  EXPECT_EQ(weight_size, 16U);
  size_t mem_work_size = 0U;
  size_t mem_weight_size = 0U;
  ASSERT_EQ(gert::GetOm2MemAndWeightSize(model_data.model_data, model_data.model_len, mem_work_size, mem_weight_size),
            SUCCESS);
  EXPECT_EQ(mem_work_size, work_size);
  EXPECT_EQ(mem_weight_size, weight_size);

  gert::Om2ModelLoadArg load_arg;
  load_arg.device_id = 0;
  ge::Status error_code = SUCCESS;
  auto executor = gert::LoadOm2ExecutorFromData(model_data, load_arg, error_code);
  ASSERT_EQ(error_code, SUCCESS);
  ASSERT_NE(executor, nullptr);

  const std::vector<ge::Om2TensorDesc> *input_desc = nullptr;
  const std::vector<ge::Om2TensorDesc> *output_desc = nullptr;
  EXPECT_EQ(executor->GetModelDescInfo(input_desc, output_desc), SUCCESS);
  ASSERT_NE(input_desc, nullptr);
  ASSERT_NE(output_desc, nullptr);
  EXPECT_EQ(input_desc->size(), 2U);
  EXPECT_EQ(output_desc->size(), 1U);

  std::vector<std::vector<int64_t>> dynamic_batch_info;
  int32_t dynamic_type = -1;
  EXPECT_EQ(executor->GetDynamicBatchInfo(dynamic_batch_info, dynamic_type), SUCCESS);
  EXPECT_TRUE(dynamic_batch_info.empty());
  EXPECT_EQ(dynamic_type, 0);

  std::vector<std::string> dynamic_output_shape;
  EXPECT_EQ(executor->GetModelAttrs(dynamic_output_shape), SUCCESS);
  EXPECT_TRUE(dynamic_output_shape.empty());

  std::vector<std::string> user_designate_shape_order;
  EXPECT_EQ(executor->GetUserDesignateShapeOrder(user_designate_shape_order), SUCCESS);
  EXPECT_TRUE(user_designate_shape_order.empty());

  std::vector<gert::Tensor> input_tensors;
  std::vector<gert::Tensor> output_tensors;
  std::vector<gert::Tensor *> inputs;
  std::vector<gert::Tensor *> outputs;
  ConstructOm2IoTensors(input_tensors, output_tensors, inputs, outputs);
  EXPECT_EQ(executor->Run(inputs, outputs), SUCCESS);
  EXPECT_EQ(executor->RunAsync(nullptr, inputs, outputs), SUCCESS);
}

TEST_F(Om2St, LoadGeneratedOm2WithExternalResources_Ok) {
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_load_external.om2"});
  CreateFakeOm2File(test_work_dir, output_file);

  ge::ModelData model_data;
  ASSERT_EQ(gert::LoadOm2DataFromFile(output_file, model_data), SUCCESS);
  std::shared_ptr<void> model_data_guard(model_data.model_data, [](const void *const p) {
    if (p != nullptr) {
      delete[] static_cast<const uint8_t *>(p);
    }
  });

  size_t work_size = 0U;
  size_t weight_size = 0U;
  ASSERT_EQ(gert::GetOm2MemAndWeightSize(model_data.model_data, model_data.model_len, work_size, weight_size), SUCCESS);
  ASSERT_GT(work_size, 0U);
  ASSERT_GT(weight_size, 0U);

  std::vector<uint8_t> external_work(work_size, 0U);
  std::vector<uint8_t> external_weight(weight_size, 0U);
  gert::Om2ModelLoadArg load_arg;
  load_arg.device_id = 0;
  load_arg.work_ptr = external_work.data();
  load_arg.work_size = external_work.size();
  load_arg.weight_ptr = external_weight.data();
  load_arg.weight_size = external_weight.size();

  ge::Status error_code = SUCCESS;
  auto executor = gert::LoadOm2ExecutorFromData(model_data, load_arg, error_code);
  ASSERT_EQ(error_code, SUCCESS);
  ASSERT_NE(executor, nullptr);

  const std::vector<ge::Om2TensorDesc> *input_desc = nullptr;
  const std::vector<ge::Om2TensorDesc> *output_desc = nullptr;
  EXPECT_EQ(executor->GetModelDescInfo(input_desc, output_desc), SUCCESS);
  ASSERT_NE(input_desc, nullptr);
  ASSERT_NE(output_desc, nullptr);
  EXPECT_EQ(input_desc->size(), 2U);
  EXPECT_EQ(output_desc->size(), 1U);
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithCmoTask) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithCmoTask(1U, 3U);
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
  GELOGI("Om2St: CMO task packaging succeeded.");
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithBarrierTask) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithBarrierTask(3);
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
  GELOGI("Om2St: Barrier task packaging succeeded.");
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithCmoAddrTask) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithCmoAddrTask(false);
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
  GELOGI("Om2St: CMO_ADDR task packaging succeeded (auto format).");
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithCmoAddrTaskExplicitFormat) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithCmoAddrTask(true);
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/te_Add_12345_AicoreKernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
  GELOGI("Om2St: CMO_ADDR task packaging succeeded (explicit format).");
}
}  // namespace ge
