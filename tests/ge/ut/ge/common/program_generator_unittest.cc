/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "common/om2/codegen/ast/ast_context.h"
#include "common/om2/codegen/program_generator.h"
#include "common/om2/codegen/om2_codegen_model_builder.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "register/op_tiling_info.h"
#include "framework/common/taskdown_common.h"
#include "ge_runtime_stub/include/common/share_graph.h"
#include "ge_runtime_stub/include/faker/ge_model_builder.h"
#include "ge_runtime_stub/include/faker/aicore_taskdef_faker.h"
#include "ge_runtime_stub/include/faker/aicpu_taskdef_faker.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/tensor_utils.h"

#include <cinttypes>
#include <fstream>
#include <map>
#include <sstream>
#include <securec.h>
#include "common/env_path.h"
#include "faker/task_def_faker.h"
#include "aicpu_engine_struct.h"
#include "aicpu_task_struct.h"
#include "engine/aicpu/kernel/aicpu_ext_info_handle.h"
#include "common/om2/codegen/om2_aicpu_ext_info_handler.h"

namespace ge {
namespace {
using AicpuShapeAndType = aicpu::FWKAdapter::ShapeAndType;
using AicpuExtInfo = aicpu::FWKAdapter::ExtInfo;
using AsyncWaitInfo = aicpu::FWKAdapter::AsyncWait;
using WorkSpaceInfo = aicpu::FWKAdapter::WorkSpaceInfo;
using AicpuSessionInfo = SessionInfo;

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

std::string GetInterfaceDumpApisExpected() {
  return R"(
struct Om2TaskIoEntry {
  const struct Om2Tensor *tensor;
  uint64_t offset;
};

enum Om2L0ArgKind {
    OM2_L0_ARG_INPUT = 0,
    OM2_L0_ARG_OUTPUT = 1,
    OM2_L0_ARG_WORKSPACE = 2,
    OM2_L0_ARG_TILING = 3,
    OM2_L0_ARG_SHAPE_INFO = 4,
    OM2_L0_ARG_LEVEL1_DESC = 5,
    OM2_L0_ARG_PLACEHOLDER = 6,
    OM2_L0_ARG_CUSTOM_VALUE = 7,
    OM2_L0_ARG_FFTS_ADDR = 8,
    OM2_L0_ARG_EVENT_ADDR = 9,
    OM2_L0_ARG_OVERFLOW_ADDR = 10,
    OM2_L0_ARG_EMPTY_ADDR = 11
};

struct Om2L0ArgSlotInfo {
    uint32_t kind;
    uint32_t flags;
    uint64_t args_offset;
    uint64_t value;
    uint32_t related_index;
    uint32_t event_id;
    uint64_t level1_target_offset;
};

struct Om2L0TaskRawInfo {
    uint32_t version;
    uint32_t need_assert_or_printf;
    uint64_t arg_num;
    const struct Om2L0ArgSlotInfo* args;
};

struct Om2TaskInfo {
  const char* op_name;
  const char* op_type;
  uint32_t task_id;
  uint32_t stream_id;
  uint32_t context_id;
  uint32_t thread_id;
  uint32_t block_dim;
  uint64_t op_desc_id;
  uintptr_t args_base;
  uint64_t args_size;
  uint64_t input_num;
  const struct Om2TaskIoEntry* inputs;
  uint32_t output_num;
  const struct Om2TaskIoEntry* outputs;
  uint32_t workspace_num;
  const uint64_t* workspace_addrs;
  const uint64_t* workspace_sizes;
  uint32_t task_type;
  void* stream;
  uint32_t is_raw_address;
  const struct Om2L0TaskRawInfo* l0_exception_dump_info;
};

extern "C" {
__attribute__((weak)) int32_t ReportDfxTaskPreprocess(uint32_t model_id,
                                                       void* instance_handle,
                                                       const struct Om2TaskInfo* task_info,
                                                       const void* extended_attrs,
                                                       size_t extended_attrs_size);

__attribute__((weak)) int32_t ReportDfxTaskPostprocess(uint32_t model_id,
                                                        void* instance_handle,
                                                        const struct Om2TaskInfo* task_info,
                                                        const void* extended_attrs,
                                                        size_t extended_attrs_size);

__attribute__((weak)) int32_t IsDataDumpEnabled(uint32_t model_id,
                                                      void* instance_handle,
                                                      const char* op_name,
                                                      uint8_t* is_data_dump);
}
)";
}

std::string GetLoadAndRunDumpHelpersExpected() {
  return R"(Om2Tensor BuildOm2Tensor(void *device_address, uint64_t size, int32_t data_type,
                         int32_t format, const int64_t *shape_dims, uint64_t shape_dims_num) {
  Om2Tensor tensor{};
  tensor.device_address = PtrToU64(device_address);
  tensor.size = size;
  tensor.data_type = data_type;
  tensor.format = format;
  tensor.shape_dims = shape_dims;
  tensor.shape_dims_num = shape_dims_num;
  return tensor;
}

aclError AssembleOm2TaskInfo(Om2TaskInfo *task_info, const char *op_name, const char *op_type,
                             uint32_t task_id, uint32_t stream_id, uint32_t block_dim,
                             uint64_t op_desc_id, uintptr_t args_base, uint64_t args_size,
                             const Om2TaskIoEntry *inputs, uint64_t input_num,
                             const Om2TaskIoEntry *outputs, uint32_t output_num,
                             const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                             uint32_t workspace_num, uint32_t task_type, void *stream,
                             uint32_t is_raw_address = 0U) {
  task_info->op_name = op_name;
  task_info->op_type = op_type;
  task_info->task_id = task_id;
  task_info->stream_id = stream_id;
  task_info->context_id = 0U;
  task_info->thread_id = 0U;
  task_info->block_dim = block_dim;
  task_info->op_desc_id = op_desc_id;
  task_info->args_base = args_base;
  task_info->args_size = args_size;
  task_info->input_num = input_num;
  task_info->inputs = inputs;
  task_info->output_num = output_num;
  task_info->outputs = outputs;
  task_info->workspace_num = workspace_num;
  task_info->workspace_addrs = workspace_addrs;
  task_info->workspace_sizes = workspace_sizes;
  task_info->task_type = task_type;
  task_info->stream = stream;
  task_info->is_raw_address = is_raw_address;
  task_info->l0_exception_dump_info = nullptr;
  return ACL_SUCCESS;
}

aclError ReportOm2TaskPreprocess(const char *op_name, const char *op_type, uint64_t op_desc_id,
                                uintptr_t args_base, uint64_t args_size,
                                const std::vector<Om2TaskIoEntry> &inputs,
                                const std::vector<Om2TaskIoEntry> &outputs,
                                const std::vector<uint64_t> &workspace_addrs,
                                const std::vector<uint64_t> &workspace_sizes,
                                uint32_t task_type, uint32_t block_dim, void *stream,
                                const Om2L0TaskRawInfo *l0_info,
                                uint32_t model_id, void *instance_handle,
                                uint32_t is_raw_address = 0U) {
  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  OM2_CHK_TRUE(workspace_addrs.size() == workspace_sizes.size());

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, 0U, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs.data(), static_cast<uint64_t>(inputs.size()),
                                      outputs.data(), static_cast<uint32_t>(outputs.size()),
                                      workspace_addrs.empty() ? nullptr : workspace_addrs.data(),
                                      workspace_sizes.empty() ? nullptr : workspace_sizes.data(),
                                      static_cast<uint32_t>(workspace_addrs.size()),
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = l0_info;
  if (ReportDfxTaskPreprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPreprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

aclError ReportLaunchedOm2Task(const char *op_name, const char *op_type, uint64_t op_desc_id,
                               uintptr_t args_base, uint64_t args_size,
                               const Om2TaskIoEntry *inputs, uint64_t input_num,
                               const Om2TaskIoEntry *outputs, uint32_t output_num,
                               const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                               uint32_t workspace_num,
                               uint32_t task_type, uint32_t block_dim, void *stream,
                               uint32_t model_id, void *instance_handle,
                               uint32_t is_raw_address = 0U) {
  uint32_t task_id = 0U;
  OM2_CHK_RT(aclrtGetThreadLastTaskId(&task_id));

  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, task_id, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs, input_num,
                                      outputs, output_num,
                                      workspace_addrs, workspace_sizes, workspace_num,
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = nullptr;
  if (ReportDfxTaskPostprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPostprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

uint8_t GetIsDataDump(const char *op_name, uint32_t model_id, void *instance_handle) {
  if (IsDataDumpEnabled != nullptr && instance_handle != nullptr) {
    uint8_t is_data_dump = 0U;
    auto ret = IsDataDumpEnabled(model_id, instance_handle, op_name, &is_data_dump);
    return ret == 0 ? is_data_dump : 0U;
  }
  return 0U;
}
)";
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
    if ((op_desc->GetType() == DATA)) {
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

GeRootModelPtr CreateGeRootModelWithConstInputOp() {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp2();
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

GeRootModelPtr CreateGeRootModelWithAicpuEmptyShape() {
  auto graph = std::make_shared<ComputeGraph>("g1");

  GeTensorDesc tensor_desc(GeShape(), FORMAT_ND, DT_FLOAT);
  (void)TensorUtils::SetSize(tensor_desc, 4);

  auto data1_desc = std::make_shared<OpDesc>("data1", DATA);
  (void)data1_desc->AddOutputDesc(tensor_desc);
  auto data1 = graph->AddNode(data1_desc);

  auto data2_desc = std::make_shared<OpDesc>("data2", DATA);
  (void)data2_desc->AddOutputDesc(tensor_desc);
  auto data2 = graph->AddNode(data2_desc);

  auto op_desc = std::make_shared<OpDesc>("add1", "Add");
  (void)op_desc->AddInputDesc("x1", tensor_desc);
  (void)op_desc->AddInputDesc("x2", tensor_desc);
  (void)op_desc->AddOutputDesc("y", tensor_desc);
  op_desc->AppendIrInput("x1", kIrInputRequired);
  op_desc->AppendIrInput("x2", kIrInputRequired);
  op_desc->AppendIrOutput("y", kIrOutputRequired);
  auto add_node = graph->AddNode(op_desc);

  auto netoutput_desc = std::make_shared<OpDesc>("netoutput", NETOUTPUT);
  (void)netoutput_desc->AddInputDesc(tensor_desc);
  auto netoutput = graph->AddNode(netoutput_desc);

  GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add_node->GetInDataAnchor(0));
  GraphUtils::AddEdge(data2->GetOutDataAnchor(0), add_node->GetInDataAnchor(1));
  GraphUtils::AddEdge(add_node->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0));

  graph->TopologicalSorting();

  gert::GeModelBuilder builder(graph);
  gert::AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto ge_root_model = builder.AddTaskDef("add1", aicpu_task_def_faker.SetNeedMemcpy(false)).BuildGeRootModel();
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
        auto ext_info = GetFakeExtInfo();
        auto *kernel_def = task_def->mutable_kernel();
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

GeRootModelPtr CreateGeRootModelWithTfAicpuOp() {
  auto graph = gert::ShareGraph::Aicpu4thGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  gert::AiCpuTfTaskDefFaker tf_aicpu_task_def_faker;
  auto ge_root_model = builder.AddTaskDef("add1", tf_aicpu_task_def_faker).BuildGeRootModel();
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
  auto ge_root_model =
      builder.AddTaskDef("Add", gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i_desc0}{i_desc1}{o_desc0}"))
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

GeRootModelPtr CreateGeRootModelWithArgs() {
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
          .AddTaskDef("Add", gert::AiCoreTaskDefFaker("add_stub")
                                 .ArgsFormat("{i_desc0}{i_desc1}{o_desc0}{event_addr}{ffts_addr}{overflow_addr}{t}"))
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
      (void)AttrUtils::SetBool(op_desc, GLOBALWORKSPACE_TYPE, true);
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

GeRootModelPtr CreateGeRootModelWithMemcpyAddrAsync() {
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
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  auto *memcpy_addr_task = model_task_def->add_task();
  memcpy_addr_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_MEMCPY_ADDR_ASYNC));
  memcpy_addr_task->set_stream_id(0U);
  auto *memcpy_async_def = memcpy_addr_task->mutable_memcpy_async();
  const uint64_t logic_mem_base = 0U;
  memcpy_async_def->set_op_index(2);
  memcpy_async_def->set_src(logic_mem_base + 1024);
  memcpy_async_def->set_dst(logic_mem_base + 2048);
  memcpy_async_def->set_dst_max(1024U);
  memcpy_async_def->set_count(512U);
  memcpy_async_def->set_kind(1U);
  memcpy_async_def->set_args_format("{}{}{i_instance0*}{o_instance0*}");

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithMemcpyAddrAsyncAndCustomValue() {
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
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  auto *memcpy_addr_task = model_task_def->add_task();
  memcpy_addr_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_MEMCPY_ADDR_ASYNC));
  memcpy_addr_task->set_stream_id(0U);
  auto *memcpy_async_def = memcpy_addr_task->mutable_memcpy_async();
  const uint64_t logic_mem_base = 0U;
  memcpy_async_def->set_op_index(2);
  memcpy_async_def->set_src(logic_mem_base + 1024);
  memcpy_async_def->set_dst(logic_mem_base + 2048);
  memcpy_async_def->set_dst_max(1024U);
  memcpy_async_def->set_count(512U);
  memcpy_async_def->set_kind(1U);
  memcpy_async_def->set_args_format("{#42}{#.32b114}{#514}{i_instance0*}{o_instance0*}");

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithMemcpyAddrAsyncConstTensor() {
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
      // Mark input 0 as const with data_offset 0 in weight buffer.
      // Must set a valid tensor size that fits within weight_size.
      op_desc->SetIsInputConst({true, false});
      GeTensorDesc tensor(GeShape({1}), FORMAT_ND, DT_INT64);
      TensorUtils::SetSize(tensor, 8);
      TensorUtils::SetDataOffset(tensor, 0);
      op_desc->UpdateInputDesc(0, tensor);
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;

  // Place weight address range right after FM range so GetRtAddress enters Weight path
  constexpr uint64_t kMemSize = 4096U;
  constexpr uint64_t kWeightBase = kMemSize;  // just past FM range [0, kMemSize)
  (void)AttrUtils::SetInt(ge_model, MODEL_ATTR_TASK_GEN_WEIGHT_ADDR, kWeightBase);

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, kMemSize);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);

  // memcpy_addr_async task: src => const tensor (weight), dst => FM (output)
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  auto *memcpy_addr_task = model_task_def->add_task();
  memcpy_addr_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_MEMCPY_ADDR_ASYNC));
  memcpy_addr_task->set_stream_id(0U);
  auto *memcpy_async_def = memcpy_addr_task->mutable_memcpy_async();
  memcpy_async_def->set_op_index(2);
  memcpy_async_def->set_src(kWeightBase + 0U);  // weight_base + data_offset(0) => kConstTensor
  memcpy_async_def->set_dst(2048U);             // FM address => kOutputInstance
  memcpy_async_def->set_dst_max(1024U);
  memcpy_async_def->set_count(512U);
  memcpy_async_def->set_kind(1U);
  memcpy_async_def->set_args_format("{}{}{i_instance0*}{o_instance0*}");

  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithMemcpyAsync() {
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
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  auto *memcpy_task = model_task_def->add_task();
  memcpy_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC));
  memcpy_task->set_stream_id(0U);
  auto *memcpy_async_def = memcpy_task->mutable_memcpy_async();
  const uint64_t logic_mem_base = 0U;
  memcpy_async_def->set_op_index(2);
  // src/dst 不使用 model io entry 的偏移地址 (1024, 3072)，不走 io_refresh_ 路径
  memcpy_async_def->set_src(logic_mem_base + 2048);
  memcpy_async_def->set_dst(logic_mem_base + 512);
  memcpy_async_def->set_dst_max(1024U);
  memcpy_async_def->set_count(512U);
  memcpy_async_def->set_kind(1U);

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

// memcpy_async 的 io_refresh_ 场景：src 或 dst 的内存偏移与 model IO entry 匹配
GeRootModelPtr CreateGeRootModelWithMemcpyAsyncIoRefresh() {
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
      op_desc->SetOutputOffset({2048});  // model IO entry at offset 2048
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({4096});
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
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  auto *memcpy_task = model_task_def->add_task();
  memcpy_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC));
  memcpy_task->set_stream_id(0U);
  auto *memcpy_async_def = memcpy_task->mutable_memcpy_async();
  memcpy_async_def->set_op_index(2);
  // src = 2048 matches DATA output_offset => triggers io_refresh_ path
  memcpy_async_def->set_src(2048U);
  memcpy_async_def->set_dst(1024U);
  memcpy_async_def->set_dst_max(1024U);
  memcpy_async_def->set_count(512U);
  memcpy_async_def->set_kind(1U);

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 8192);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  return ge_root_model;
}

// -----------------------------------------------------------------------
// Helper: 在现有 AiCore 模型上添加 BARRIER task_def
// -----------------------------------------------------------------------
GeRootModelPtr CreateGeRootModelWithCmoBarrierTask(int32_t barrier_count = 1) {
  auto ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  if (ge_model == nullptr) {
    return nullptr;
  }
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if (model_task_def == nullptr) {
    return nullptr;
  }
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
// Helper: 在现有 AiCore 模型上添加 CMO task_def
// -----------------------------------------------------------------------
GeRootModelPtr CreateGeRootModelWithCmoTask(uint32_t cmo_type = 1U, uint32_t op_code = 3U) {
  auto ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  if (ge_model == nullptr) {
    return nullptr;
  }
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if (model_task_def == nullptr) {
    return nullptr;
  }
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
// Helper: 在现有 AiCore 模型上添加 CMO_ADDR task_def
// 空 args_format 会触发 BuildAutoArgsFormat 自动生成
// -----------------------------------------------------------------------
GeRootModelPtr CreateGeRootModelWithCmoAddrTask(bool with_explicit_format = false) {
  auto ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  auto &compute_graph = ge_root_model->GetRootGraph();
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  if (ge_model == nullptr) {
    return nullptr;
  }
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if (model_task_def == nullptr) {
    return nullptr;
  }

  // 为 Add 节点设置 tensor_desc 和 offset 属性（BuildAutoArgsFormat 需要）
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

GeRootModelPtr CreateGeRootModelWithCmoAddrTaskConstTensor() {
  auto ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return nullptr;
  }
  auto &compute_graph = ge_root_model->GetRootGraph();
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  if (ge_model == nullptr) {
    return nullptr;
  }
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if (model_task_def == nullptr) {
    return nullptr;
  }

  // Set up weight data for const tensor
  std::vector<uint8_t> weights_value(256 * 1024, 0);
  const size_t weight_size = weights_value.size();
  ge_model->SetWeight(Buffer::CopyFrom(weights_value.data(), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  // Place weight address range beyond FM range so GetRtAddress enters Weight range
  (void)AttrUtils::SetInt(ge_model, MODEL_ATTR_TASK_GEN_WEIGHT_ADDR, 2048);

  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() != DATA) && (op_desc->GetType() != NETOUTPUT)) {
      GeTensorDesc tensor(GeShape({4, 4, 4, 4}), FORMAT_ND, DT_INT64);
      TensorUtils::SetSize(tensor, 2048);
      op_desc->UpdateInputDesc(0, tensor);
      (void)AttrUtils::SetInt(op_desc, "offset", 512);
      // Mark input 0 as const and set data offset into weight area
      op_desc->SetIsInputConst({true});
      auto input_desc = op_desc->MutableInputDesc(0);
      if (input_desc != nullptr) {
        (void)TensorUtils::SetDataOffset(*input_desc, 128);
      }
    }
  }

  auto add_node = compute_graph->FindNode("add1");
  const int64_t op_index = (add_node != nullptr) ? add_node->GetOpDescBarePtr()->GetId() : 2;

  auto *task_def = model_task_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CMO_ADDR));
  task_def->set_stream_id(0U);
  auto *cmo_addr = task_def->mutable_cmo_addr_task();
  cmo_addr->set_op_index(static_cast<uint32_t>(op_index));
  cmo_addr->set_cmo_op_code(6);
  cmo_addr->set_src(2048U + 128U);  // weight_base + data_offset for kConstTensor
  cmo_addr->set_num_inner(0);
  cmo_addr->set_num_outer(0);
  cmo_addr->set_length_inner(1024);
  cmo_addr->set_stride_outer(0);
  cmo_addr->set_stride_inner(0);
  return ge_root_model;
}

// 构造 NoTask ConcatD 输出复用图：
// data0 -> add0 -> ConcatD(NoTask) -> NetOutput
// data1 -> add1 -^
GeRootModelPtr CreateGeRootModelWithNoTaskConcatOutput() {
  auto graph = std::make_shared<ComputeGraph>("g1");
  GeTensorDesc tensor_desc(GeShape({1, 32}), FORMAT_ND, DT_FLOAT);
  TensorUtils::SetSize(tensor_desc, 128U);

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

  GeTensorDesc out_tensor_desc(GeShape({1, 64}), FORMAT_ND, DT_FLOAT);
  TensorUtils::SetSize(out_tensor_desc, 256U);
  auto concat_desc = std::make_shared<OpDesc>("concat_notask", "ConcatD");
  (void)concat_desc->AddInputDesc("x0", tensor_desc);
  (void)concat_desc->AddInputDesc("x1", tensor_desc);
  (void)concat_desc->AddOutputDesc("y", out_tensor_desc);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_NOTASK, true);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_OUTPUT_REUSE_INPUT, true);
  (void)AttrUtils::SetBool(concat_desc, ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, true);
  (void)AttrUtils::SetInt(concat_desc, ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, 0);
  auto concat = graph->AddNode(concat_desc);

  auto netoutput_desc = std::make_shared<OpDesc>("netoutput", NETOUTPUT);
  (void)netoutput_desc->AddInputDesc(out_tensor_desc);
  auto netoutput = graph->AddNode(netoutput_desc);

  if ((data0 == nullptr) || (data1 == nullptr) || (add0 == nullptr) || (add1 == nullptr) || (concat == nullptr) ||
      (netoutput == nullptr)) {
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

ProgramGenerator CreateProgramGenerator(GeRootModelPtr &ge_root_model) {
  static AstContext ast_ctx;
  static AstBuildContext ast(ast_ctx);
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  if (name_to_ge_model.empty()) {
    ADD_FAILURE() << "[OM2] No subgraphs found in ge_root_model";
    return ProgramGenerator(ast, {}, Om2CodegenModel());
  }
  const auto &ge_model = name_to_ge_model.begin()->second;
  SyncKernelNameFromOpDesc(ge_model);
  std::vector<TaskCodeBuilderPtr> task_code_builders;
  Om2CodegenModel codegen_model;
  if (Om2CodegenModelBuilder::CreateTaskCodeBuilders(ge_model, ast, task_code_builders, codegen_model) != SUCCESS) {
    ADD_FAILURE() << "[OM2] Failed to create task code handlers";
    return ProgramGenerator(ast, {}, Om2CodegenModel());
  }
  Om2CodegenModelBuilder builder;
  Om2ConstMetas const_metas;
  if (builder.Build(ge_model, task_code_builders, codegen_model, const_metas) != SUCCESS) {
    ADD_FAILURE() << "[OM2] Failed to build om2 codegen model";
    return ProgramGenerator(ast, {}, Om2CodegenModel());
  }
  return ProgramGenerator(ast, task_code_builders, codegen_model);
}

const std::map<GeneratedFileIndex, std::string> kGeneratedFileNames = {
    {GeneratedFileIndex::kKernelRegistryFile, "g1_kernel_reg.cpp"},
    {GeneratedFileIndex::kResourcesFile, "g1_resources.cpp"},
    {GeneratedFileIndex::kArgsManagerFile, "g1_args_manager.cpp"},
    {GeneratedFileIndex::kLoadingAndRunningFile, "g1_load_and_run.cpp"},
    {GeneratedFileIndex::kInterfaceHeaderFile, "g1_interface.h"},
    {GeneratedFileIndex::kCMakeListsFile, "Makefile"},
};

Status ReadGeneratedArtifact(const Om2CodegenArtifacts &artifacts, const GeneratedFileIndex file_index,
                             std::string &output) {
  const auto iter = kGeneratedFileNames.find(file_index);
  GE_ASSERT_TRUE(iter != kGeneratedFileNames.end(), "[OM2] unknown generated file index: %zu",
                 static_cast<size_t>(file_index));
  for (const auto &artifact : artifacts) {
    if (artifact.file_name == iter->second) {
      output = artifact.data;
      return SUCCESS;
    }
  }
  GELOGE(FAILED, "[OM2] failed to find generated artifact: %s", iter->second.c_str());
  return FAILED;
}

Status GenerateProgramFiles(ProgramGenerator &generator, std::map<GeneratedFileIndex, std::string> &outputs) {
  Om2CodePrinter code_printer("g1");
  GE_ASSERT_SUCCESS(generator.GenerateProgram(code_printer));
  Om2CodegenArtifacts artifacts;
  code_printer.GetOutputFiles(artifacts);
  outputs.clear();
  for (const auto file_index : {GeneratedFileIndex::kKernelRegistryFile, GeneratedFileIndex::kResourcesFile,
                                GeneratedFileIndex::kArgsManagerFile, GeneratedFileIndex::kLoadingAndRunningFile,
                                GeneratedFileIndex::kInterfaceHeaderFile, GeneratedFileIndex::kCMakeListsFile}) {
    std::string output;
    GE_ASSERT_SUCCESS(ReadGeneratedArtifact(artifacts, file_index, output));
    outputs.emplace(file_index, std::move(output));
  }
  return SUCCESS;
}

std::string GetExpectedArgsManagerSource() {
  return R"(#line 1 "g1_args_manager.cpp"
#include "g1_interface.h"

namespace om2 {
aclError Om2ArgsTable::Init() {
  args_size_ = 168;
  host_args_.clear();
  host_args_.resize(args_size_);
  OM2_CHK_STATUS(aclrtMalloc(&dev_args_, args_size_, ACL_MEM_MALLOC_HUGE_FIRST));
  args_info_ = {{GetHostArgAddr(0), GetDevArgAddr(0), 168}};
  iow_args_addrs_ = {{GetHostArgAddr(0), GetHostArgAddr(8), GetHostArgAddr(16)}, {GetHostArgAddr(0), GetHostArgAddr(8), GetHostArgAddr(16)}, {}};
  return ACL_SUCCESS;
}

Om2ArgsTable::~Om2ArgsTable() {
}

ArgsInfo * Om2ArgsTable::GetArgsInfo(size_t index) {
  if ((index >= args_info_.size())) {
    return nullptr;
  }
  return &args_info_[index];
}

void * Om2ArgsTable::GetDevArgAddr(size_t offset) {
  if ((offset >= args_size_)) {
    return nullptr;
  }
  return GET_ADDR(dev_args_, offset);
}

void * Om2ArgsTable::GetHostArgAddr(size_t offset) {
  if ((offset >= args_size_)) {
    return nullptr;
  }
  return GET_ADDR(host_args_.data(), offset);
}

aclError Om2ArgsTable::UpdateHostArgs(size_t index, const uintptr_t addr) {
  if ((index >= iow_args_addrs_.size())) {
    return ACL_ERROR_FAILURE;
  }
  for (void *host_addr : iow_args_addrs_.at(index)) {
    std::memcpy(host_addr, &addr, sizeof(addr));
  }
  return ACL_SUCCESS;
}

aclError Om2ArgsTable::CopyArgsToDevice() {
  OM2_CHK_STATUS(aclrtMemcpy(dev_args_, args_size_, host_args_.data(), args_size_, ACL_MEMCPY_HOST_TO_DEVICE));
  return ACL_SUCCESS;
}
} // namespace om2
)";
}
}  // namespace

class ProgramGeneratorUt : public testing::Test {
 public:
  void SetUp() override {}
  void TearDown() override {
    EnvPath().RemoveRfCaseTmpPath("ProgramGeneratorUt");
  }
};

TEST_F(ProgramGeneratorUt, GenerateMakefile_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const std::string expected = R"(CANN_ROOT ?= $(ASCEND_HOME_PATH)
USE_STUB_LIB ?= 1

ifeq ($(origin CXX),default)
CXX := c++
endif
CXX ?= c++

TARGET := libg1_om2.so
SRC_FILES := g1_resources.cpp g1_kernel_reg.cpp g1_load_and_run.cpp g1_args_manager.cpp

ifndef CPPFLAGS
CPPFLAGS := \
  -I$(CANN_ROOT)/include \
  -I$(CANN_ROOT)/pkg_inc \
  -I$(CANN_ROOT)/pkg_inc/base \
  -I$(CANN_ROOT)/pkg_inc/runtime \
  -I$(CANN_ROOT)/pkg_inc/profiling \
  -I$(CURDIR)/include
endif

ifndef CXXFLAGS
CXXFLAGS := -std=c++17 -O2 -fPIC
endif

ifeq ($(USE_STUB_LIB),1)
LIB_PATH ?= $(CANN_ROOT)/devlib
else
LIB_PATH ?= $(CANN_ROOT)/lib64
endif

ifndef LDFLAGS
LDFLAGS := -shared -L$(LIB_PATH) -Wl,--no-as-needed
endif
ifndef LDLIBS
LDLIBS := -lacl_rt -Wl,--as-needed
endif

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC_FILES)
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

  clean:
	@rm -f $(TARGET)
)";
  ASSERT_EQ(outputs[GeneratedFileIndex::kCMakeListsFile], expected + "\n");
}

TEST_F(ProgramGeneratorUt, GenerateResourcesSource_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);
  const std::string expected = R"(#line 1 "g1_resources.cpp"
#include "g1_interface.h"

namespace om2 {
Om2Model::Om2Model(const char **bin_files, const void **bin_data, size_t *bin_size, size_t bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle)
  : constants_(constants), total_dev_mem_ptr_(work_ptr), session_id_(session_id), model_id_(model_id), instance_handle_(instance_handle), kernel_id_(0), session_scope_mem_ptr_(nullptr) {
  for (size_t i = 0; (i < bin_num); ++i) {
    bin_info_map_[std::string(bin_files[i])] = {bin_data[i], bin_size[i]};
  }
  bin_handles_.resize(1);
  func_handles_.resize(1);
  stream_list_.resize(1);
  OM2_LOGD("Om2Model created");
}

Om2Model::~Om2Model() {
  OM2_LOGD("~Om2Model");
  (void)ReleaseResources();
}

aclError Om2Model::InitResources() {
  OM2_LOGI("InitResources begin");
  // 1. 创建 model
  OM2_CHK_STATUS(aclmdlRIBuildBegin(&model_handle_, 0));

  // 2. 获取overflow地址
  OM2_CHK_STATUS(aclrtCtxGetFloatOverflowAddr(&overflow_addr_));

  // 3. 创建其他资源
  // 创建下沉Stream并绑定模型
  uint32_t stream0_flag = RT_STREAM_PERSISTENT;
  OM2_CHK_RT(rtStreamCreateWithFlags(&stream_list_[0], 0, stream0_flag));
  auto bind0_flag = RT_HEAD_STREAM;
  OM2_CHK_STATUS(aclmdlRIBindStream(model_handle_, stream_list_[0], bind0_flag));
  is_stream_list_bind_ = true;
  args_table_.Init();
  OM2_LOGI("InitResources done");
  return ACL_SUCCESS;
}

aclError Om2Model::ReleaseResources() {
  OM2_LOGI("ReleaseResources begin");
  if (is_stream_list_bind_) {
    for (auto stream : stream_list_) {
      OM2_CHK_STATUS(aclmdlRIUnbindStream(model_handle_, stream));
    }
  }
  for (auto stream : stream_list_) {
    OM2_CHK_STATUS(aclrtDestroyStream(stream));
  }
  for (auto bin_handle : bin_handles_) {
    OM2_CHK_STATUS(aclrtBinaryUnLoad(bin_handle));
  }
  OM2_CHK_STATUS(aclmdlRIDestroy(model_handle_));
  if ((session_scope_mem_ptr_ != nullptr)) {
    OM2_CHK_STATUS(aclrtFree(session_scope_mem_ptr_));
  }
  for (int i = 0; (i < dev_ext_info_mem_ptrs_.size()); i++) {
    if ((dev_ext_info_mem_ptrs_[i] != nullptr)) {
      OM2_CHK_STATUS(aclrtFree(dev_ext_info_mem_ptrs_[i]));
    }
  }
  for (int i = 0; (i < dev_dynamic_mem_ptrs_.size()); i++) {
    if ((dev_dynamic_mem_ptrs_[i] != nullptr)) {
      OM2_CHK_STATUS(aclrtFree(dev_dynamic_mem_ptrs_[i]));
    }
  }
  OM2_LOGI("ReleaseResources done");
  return ACL_SUCCESS;
}
} // namespace om2)";
  ASSERT_EQ(outputs[GeneratedFileIndex::kResourcesFile], expected + "\n");
}

TEST_F(ProgramGeneratorUt, GenerateArgsManagerSource_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  ASSERT_EQ(outputs[GeneratedFileIndex::kArgsManagerFile], GetExpectedArgsManagerSource());
}

TEST_F(ProgramGeneratorUt, GenerateInterfaceHeader_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);
  const std::string expected = R"(#include <iostream>
#include <cstddef>
#include <ctime>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <type_traits>
#include <array>
#include "securec.h"
#include "acl/acl.h"
#include "acl/acl_base.h"
#include "exe_graph/runtime/runtime_tensor.h"
#include "rt_external.h"
#include "dlog_pub.h"
#include <sys/syscall.h>
#include <unistd.h>
#include <cinttypes>

// OM2 Logging Macros
#define OM2_MODULE_NAME static_cast<int32_t>(GE)
#define OM2_LOG_HEADER "OM2"

static inline uint64_t Om2GetTid() {
#ifdef __GNUC__
  return static_cast<uint64_t>(syscall(__NR_gettid));
#else
  return static_cast<uint64_t>(GetCurrentThreadId());
#endif
}

static inline bool Om2IsLogEnable(int32_t level) {
  return CheckLogLevel(OM2_MODULE_NAME, level) == 1;
}

#define OM2_LOGD(fmt, ...) \
  do { \
    if (Om2IsLogEnable(DLOG_DEBUG)) { \
      DlogRecord(OM2_MODULE_NAME, DLOG_DEBUG, "[%s:%d] %" PRIu64 " [%s] %s: " fmt, \
                 DLOG_FILE_NAME, __LINE__, Om2GetTid(), OM2_LOG_HEADER, __FUNCTION__, ##__VA_ARGS__); \
    } \
  } while (false)

#define OM2_LOGI(fmt, ...) \
  do { \
    if (Om2IsLogEnable(DLOG_INFO)) { \
      DlogRecord(OM2_MODULE_NAME, DLOG_INFO, "[%s:%d] %" PRIu64 " [%s] %s: " fmt, \
                 DLOG_FILE_NAME, __LINE__, Om2GetTid(), OM2_LOG_HEADER, __FUNCTION__, ##__VA_ARGS__); \
    } \
  } while (false)

#define OM2_LOGW(fmt, ...) \
  do { \
    if (Om2IsLogEnable(DLOG_WARN)) { \
      DlogRecord(OM2_MODULE_NAME, DLOG_WARN, "[%s:%d] %" PRIu64 " [%s] %s: " fmt, \
                 DLOG_FILE_NAME, __LINE__, Om2GetTid(), OM2_LOG_HEADER, __FUNCTION__, ##__VA_ARGS__); \
    } \
  } while (false)

#define OM2_LOGE(fmt, ...) \
  do { \
    if (Om2IsLogEnable(DLOG_ERROR)) { \
      DlogRecord(OM2_MODULE_NAME, DLOG_ERROR, "[%s:%d] %" PRIu64 " [%s] %s: " fmt, \
                 DLOG_FILE_NAME, __LINE__, Om2GetTid(), OM2_LOG_HEADER, __FUNCTION__, ##__VA_ARGS__); \
    } \
  } while (false)

#define OM2_CHK_STATUS(expr, ...)            \
do {                                       \
  const aclError _chk_status = (expr);     \
  if (_chk_status != ACL_SUCCESS) {        \
    OM2_LOGE(__VA_ARGS__);                 \
    return _chk_status;                    \
  }                                        \
} while (false)

#define OM2_CHK_NOTNULL(ptr, ...)            \
do {                                       \
  if ((ptr) == nullptr) {                  \
    OM2_LOGE(__VA_ARGS__);                 \
    return ACL_ERROR_FAILURE;              \
  }                                        \
} while (false)

#define OM2_CHK_TRUE(expr, ...)              \
do {                                       \
  if (!(expr)) {                           \
    OM2_LOGE(__VA_ARGS__);                 \
    return ACL_ERROR_FAILURE;              \
  }                                        \
} while (false)

#define OM2_CHK_RT(expr, ...)               \
do {                                       \
  const rtError_t _rt_err = (expr);        \
  if (_rt_err != RT_ERROR_NONE) {          \
    OM2_LOGE(__VA_ARGS__);                 \
    return ACL_ERROR_FAILURE;              \
  }                                        \
} while (false)

#define GET_ADDR(mem_ptr, offset)   \
(reinterpret_cast<void *>(                 \
  reinterpret_cast<uintptr_t>(mem_ptr) +   \
  static_cast<uintptr_t>(offset)))

#define OM2_MAKE_GUARD(var, callback) const ::om2::ScopeGuard const_guard_##var(callback)

template<typename T>
inline T *PtrAdd(T *const ptr, const size_t max_buf_len, const size_t idx) {
  if ((ptr != nullptr) && (idx < max_buf_len)) {
    return reinterpret_cast<T *>(ptr + idx);
  }
  return nullptr;
}
template<typename TI, typename TO>
inline TO *PtrToPtr(TI *const ptr) {
  return reinterpret_cast<TO *>(ptr);
}
inline uint64_t PtrToValue(const void *const ptr) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
}
inline void *ValueToPtr(const uint64_t value) {
  return reinterpret_cast<void *>(static_cast<uintptr_t>(value));
}
inline uint64_t PtrToU64(const void *ptr) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ptr));
}

inline void *ResolveOpAddr(uint32_t mem_src, uint64_t offset,
                          void *total_dev_mem_ptr, void *session_scope_mem_ptr,
                          void **constants) {
  void *base_ptr;
  if (mem_src == 0xFFFFFFFFU) {
    base_ptr = session_scope_mem_ptr;
  } else if (mem_src == 0U) {
    base_ptr = total_dev_mem_ptr;
  } else {
    base_ptr = constants[mem_src - 1U];
  }
  return GET_ADDR(base_ptr, offset);
}

extern "C" {
struct Om2Tensor {
  uint64_t device_address;
  uint64_t size;
  int32_t data_type;
  int32_t format;
  const int64_t* shape_dims;
  uint64_t shape_dims_num;
};
}

template<typename... Args>
inline std::vector<uint64_t> FlattenHostArgs(Args&&... args) {
  std::vector<uint64_t> buf;
  auto append_arg = [&](auto&& arg) {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_pointer_v<T>) {
      buf.push_back(reinterpret_cast<uint64_t>(arg));
    } else if constexpr (std::is_same_v<T, Om2Tensor>) {
      buf.push_back(arg.device_address);
    } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
      for (auto d : arg) buf.push_back(static_cast<uint64_t>(d));
    } else if constexpr (std::is_integral_v<T>) {
      buf.push_back(static_cast<uint64_t>(arg));
    } else {
      static_assert(sizeof(T) == 0, "Unsupported type in FlattenHostArgs");
    }
  };
  (append_arg(std::forward<Args>(args)), ...);
  return buf;
}

struct Om2TaskIoEntry {
  const struct Om2Tensor *tensor;
  uint64_t offset;
};

enum Om2L0ArgKind {
    OM2_L0_ARG_INPUT = 0,
    OM2_L0_ARG_OUTPUT = 1,
    OM2_L0_ARG_WORKSPACE = 2,
    OM2_L0_ARG_TILING = 3,
    OM2_L0_ARG_SHAPE_INFO = 4,
    OM2_L0_ARG_LEVEL1_DESC = 5,
    OM2_L0_ARG_PLACEHOLDER = 6,
    OM2_L0_ARG_CUSTOM_VALUE = 7,
    OM2_L0_ARG_FFTS_ADDR = 8,
    OM2_L0_ARG_EVENT_ADDR = 9,
    OM2_L0_ARG_OVERFLOW_ADDR = 10,
    OM2_L0_ARG_EMPTY_ADDR = 11
};

struct Om2L0ArgSlotInfo {
    uint32_t kind;
    uint32_t flags;
    uint64_t args_offset;
    uint64_t value;
    uint32_t related_index;
    uint32_t event_id;
    uint64_t level1_target_offset;
};

struct Om2L0TaskRawInfo {
    uint32_t version;
    uint32_t need_assert_or_printf;
    uint64_t arg_num;
    const struct Om2L0ArgSlotInfo* args;
};

struct Om2TaskInfo {
  const char* op_name;
  const char* op_type;
  uint32_t task_id;
  uint32_t stream_id;
  uint32_t context_id;
  uint32_t thread_id;
  uint32_t block_dim;
  uint64_t op_desc_id;
  uintptr_t args_base;
  uint64_t args_size;
  uint64_t input_num;
  const struct Om2TaskIoEntry* inputs;
  uint32_t output_num;
  const struct Om2TaskIoEntry* outputs;
  uint32_t workspace_num;
  const uint64_t* workspace_addrs;
  const uint64_t* workspace_sizes;
  uint32_t task_type;
  void* stream;
  uint32_t is_raw_address;
  const struct Om2L0TaskRawInfo* l0_exception_dump_info;
};

extern "C" {
__attribute__((weak)) int32_t ReportDfxTaskPreprocess(uint32_t model_id,
                                                       void* instance_handle,
                                                       const struct Om2TaskInfo* task_info,
                                                       const void* extended_attrs,
                                                       size_t extended_attrs_size);

__attribute__((weak)) int32_t ReportDfxTaskPostprocess(uint32_t model_id,
                                                        void* instance_handle,
                                                        const struct Om2TaskInfo* task_info,
                                                        const void* extended_attrs,
                                                        size_t extended_attrs_size);

__attribute__((weak)) int32_t IsDataDumpEnabled(uint32_t model_id,
                                                      void* instance_handle,
                                                      const char* op_name,
                                                      uint8_t* is_data_dump);
}

struct rtLabelDevInfo {
  uint16_t modelId;
  uint16_t streamId;
  uint16_t labelId;
  uint16_t reserved[7];
};

#ifdef __cplusplus
extern "C" {
#endif

rtError_t rtCmoAddrTaskLaunch(void *cmoAddrInfo, uint64_t destMax, rtCmoOpCode_t cmoOpCode, rtStream_t stm, uint32_t flag);

#ifdef __cplusplus
}
#endif

namespace om2 {
constexpr int32_t INPUT_NUM = 2;
constexpr int32_t OUTPUT_NUM = 1;
typedef void *Om2ModelHandle;

struct BinDataInfo {
  const void *data;
  size_t size;
};

struct AicpuParamHead {
  uint32_t length;
  uint32_t ioAddrNum;
  uint32_t extInfoLength;
  uint64_t extInfoAddr;
};

struct AicpuSessionInfo {
  uint64_t sessionId;
  uint64_t kernelId;
  bool sessFlag;
};

struct ArgsInfo {
  void *host_addr;
  void *dev_addr;
  size_t size;
};

struct TfAiCpuExInfo {
  uint32_t fwkKernelType;
  uint32_t fwkOperateType;
  uint64_t sessionID;
  uint64_t stepIDAddr;
  uint64_t kernelID;
  uint64_t nodeDefLen;
  uint64_t nodeDefBuf;
  uint64_t funDefLibLen;
  uint64_t funDefLibBuf;
  uint64_t inputOutputLen;
  uint64_t inputOutputBuf;
  uint64_t workspaceBaseAddr;
  uint64_t inputOutputAddr;
  uint64_t extInfoLen;
  uint64_t extInfoAddr;
};

class ScopeGuard {
  public:
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;

    explicit ScopeGuard(const std::function<void()> &on_exit_scope)
        : on_exit_scope_(on_exit_scope) {}

    ~ScopeGuard() {
      if (on_exit_scope_) {
        try {
          on_exit_scope_();
        } catch (std::bad_function_call &) {
        } catch (...) {
        }
      }
    }

  private:
    std::function<void()> on_exit_scope_;
};

class Om2ArgsTable {
  public:
    Om2ArgsTable() = default;
    ~Om2ArgsTable();
    aclError Init();
    ArgsInfo * GetArgsInfo(size_t index);
    void * GetDevArgAddr(size_t offset);
    void * GetHostArgAddr(size_t offset);
    aclError UpdateHostArgs(size_t index, const uintptr_t addr);
    aclError CopyArgsToDevice();
  private:
    int64_t args_size_;
    std::vector<ArgsInfo> args_info_;
    std::vector<uint8_t> host_args_;
    void *dev_args_;
    std::vector<std::vector<void *>> iow_args_addrs_;
};

// 算子分发类型枚举
enum OpDispatchType : uint32_t {
    DISPATCH_AICORE = 0,        // AI Core / AICPU 算子执行
};

// 算子参数类型枚举
enum OpArgType : int32_t {
    OP_ARG_INPUT = 0,          // 输入张量
    OP_ARG_OUTPUT = 1,         // 输出张量
    OP_ARG_WORKSPACE = 2,      // 工作空间
    OP_ARG_CONST_TENSOR = 3,   // 常量张量
    OP_ARG_LEVEL1_DESC = 4,    // Level1 描述符
    OP_ARG_SHAPE_INFO = 5,     // Shape 信息
    OP_ARG_CUSTOM_VALUE = 6,   // 自定义值
    OP_ARG_PLACEHOLDER = 7,    // 占位符
    OP_ARG_OPTIONAL_EMPTY = 8, // 可选空参数
    OP_ARG_FFTS_ADDR = 9,      // FFTS 地址
    OP_ARG_EVENT_ADDR = 10,    // 事件地址
    OP_ARG_OVERFLOW_ADDR = 11, // 溢出地址
    OP_ARG_TILING = 12,        // Tiling 数据
    OP_ARG_RAW_ADDR = 13,      // 原始地址
};

// 算子参数信息结构体
struct OpArgInfo {
  int32_t type;                      // 参数类型（OpArgType，switch 条件）
  struct {                           // 地址解析（INPUT/OUTPUT/WORKSPACE/CONST_TENSOR）
    uint32_t mem_src;                // 内存来源（0=设备内存，0xFFFFFFFF=session，≥1=常量数组索引）
    uint64_t offset;                 // 内存偏移量
  } addr;
  union {
    struct {                         // Tensor 元数据（INPUT/OUTPUT）
      uint64_t size;                 // 数据大小（字节），WORKSPACE 也用此字段
      int32_t data_type;             // 数据类型
      int32_t format;                // 数据格式
      int64_t shape[8];              // Shape 维度数组（最多8维）
      uint32_t num_shape_dims;       // 实际 shape 维度数
      uint64_t args_offset;          // args table 内字节偏移
    } tensor;
    uint64_t custom_value;           // 自定义值（LEVEL1_DESC/SHAPE_INFO/CUSTOM_VALUE/EVENT_ADDR）
    struct {                         // Tiling 数据（TILING）
      const uint8_t *raw_data;       // 原始数据指针
      uint32_t raw_data_len;         // 原始数据长度
    } tiling;
  } data;
};

// 算子定义结构体
struct OpDef {
  OpDispatchType dispatch_type;   // 分发类型，决定走哪个 case 分支
  const char *op_name;            // 算子名称，用于日志和 Report 上报
  const OpArgInfo *argsInfo;           // 参数信息数组，地址解析时遍历
  // 各 task 专属数据下沉到 union（后续适配其他 task 时再增加对应成员）
  union {
    struct {                      // AICORE 专属 (kernel_type=0)
      const char *op_type;        // 算子类型名，用于 Report 上报
      uint32_t num_io_addrs;      // IO 地址数量，控制地址解析循环次数
      uint32_t args_idx;          // 参数表索引，用于 GetArgsInfo 查找
      struct {                    // Launch 配置，构建 LaunchKernelConfig → AssembleLaunchConfig
        uint8_t schedule_mode;    // 调度模式
        uint32_t engine_type;     // 引擎类型
        uint32_t block_dim_offset;// BlockDim 偏移量
        bool is_block_task_prefetch; // 是否预取 Block Task
        uint16_t time_out;        // 超时时间
        uint32_t local_memory_size;  // 本地内存大小
      } launch;
      struct {                    // Kernel 执行参数，传给 KernelTaskDistribute
        uint32_t block_dim;       // Block 维度
        uint32_t func_idx;        // 函数句柄索引，用于查找 func_handles
      } kernel;
      struct {                    // L0 信息，构建 Om2L0TaskRawInfo
        uint32_t need_assert_or_printf; // 是否需要 assert/printf
        uint32_t num_l0_slots;    // L0 slot 数量
        const Om2L0ArgSlotInfo *l0_slots; // L0 slot 信息数组
      } l0;
    } aicore;
    struct {  // AICPU 专属（共享 DISPATCH_AICORE, kernel_type=1）
      uint32_t kernel_type;
      uint32_t args_idx;
      uint32_t num_io_addrs;
      uint32_t func_idx;
      uint32_t block_dim;
      uint8_t schedule_mode;
      uint32_t engine_type;
      uint32_t block_dim_offset;
      bool is_block_task_prefetch;
      uint16_t time_out;
      uint32_t local_memory_size;
      uint32_t ext_info_len;
      int32_t session_info_offset;
      uint32_t aicpu_task_index;
      uint32_t task_type;
    } aicpu;
  } task_data;
};

// 算子分发上下文结构体
struct DispatchOpContext {
  void *total_dev_mem_ptr;   // 设备总内存指针
  void *session_scope_mem_ptr; // Session 作用域内存指针
  void **constants;          // 常量数组
  Om2ArgsTable &args_table;  // 参数表
  aclrtFuncHandle *func_handles; // 函数句柄数组
  aclrtStream stream;        // 执行流
  uint32_t model_id;         // 模型 ID
  void *instance_handle;     // 实例句柄
  std::map<uint32_t, void *> &event_id_mem_map; // 事件 ID 到内存的映射
  std::vector<void *> &dev_dynamic_mem_ptrs; // 设备动态内存指针列表
  void *overflow_addr;       // 溢出地址
};


class Om2Model {
  public:
    Om2Model(const char **bin_files, const void **bin_data, size_t *bin_size, size_t bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle);
    ~Om2Model();
    aclError InitResources();
    aclError RegisterKernels();
    aclError Load();
    aclmdlRI GetRtModelHandle();
    aclError Run(size_t input_count, void **input_data, size_t output_count, void **output_data, int32_t stream_sync_timeout);
    aclError RunAsync(aclrtStream &exe_stream, size_t input_count, void **input_data, size_t output_count, void **output_data);
    aclError ReleaseResources();
  private:
    void **constants_;
    aclmdlRI model_handle_;
    std::vector<aclrtBinHandle> bin_handles_;
    std::vector<aclrtFuncHandle> func_handles_;
    std::vector<aclrtStream> stream_list_;
    std::vector<aclrtNotify> notify_list_;
    std::vector<aclrtEvent> event_list_;
    std::vector<aclrtLabel> label_list_;
    std::vector<aclrtLabel> label_used_;
    std::map<uint32_t, aclrtLabelList> label_switch_label_list_;
    std::map<uint32_t, std::pair<void *, uint32_t>> label_goto_args_;
    std::map<uint32_t, aclrtLabelList> label_goto_ex_label_list_;
    void *total_dev_mem_ptr_;
    bool is_stream_list_bind_;
    std::unordered_map<std::string, BinDataInfo> bin_info_map_;
    Om2ArgsTable args_table_;
    uint64_t *session_id_;
    uint32_t model_id_;
    void *instance_handle_;
    uint64_t kernel_id_;
    std::vector<void *> dev_ext_info_mem_ptrs_;
    std::map<uint32_t, void *> mem_event_id_mem_map_;
    void *overflow_addr_;
    std::vector<void *> dev_dynamic_mem_ptrs_;
    void *session_scope_mem_ptr_;
};
} // namespace om2
#ifdef __cplusplus
extern "C" {
#endif

aclError Om2ModelCreate(om2::Om2ModelHandle *model_handle, aclmdlRI *rt_model_handle, const char **bin_files, const void **bin_data, size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle);

aclError Om2ModelLoad(om2::Om2ModelHandle *model_handle);

aclError Om2ModelRunAsync(om2::Om2ModelHandle *model_handle, aclrtStream stream, int input_count, void **input_data, int output_count, void **output_data);

aclError Om2ModelRun(om2::Om2ModelHandle *model_handle, int input_count, void **input_data, int output_count, void **output_data, int32_t stream_sync_timeout);

aclError Om2ModelDestroy(om2::Om2ModelHandle *model_handle);

#ifdef __cplusplus
}
#endif
)";
  ASSERT_EQ(outputs[GeneratedFileIndex::kInterfaceHeaderFile], expected);
}

TEST_F(ProgramGeneratorUt, GenerateKernelRegSource_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const std::string kernel_reg_expected = R"OM2(#line 1 "g1_kernel_reg.cpp"
#include "g1_interface.h"
namespace om2 {
namespace {
constexpr uint32_t kMaxJsonFileLen = 512U;
struct BinaryBuffer {
  std::unique_ptr<uint8_t[]> data;
  size_t size = 0;
};

struct AicoreRegisterInfo {
  uint32_t magic;
  bool use_tiling_key = false;
  uint64_t tiling_key = 0;
  const char *kernel_name;
  std::string file;
};

struct AicpuRegisterInfo {
  const char *op_type;
  const char *so_name;
  const char *kernel_name;
  const char *op_kernel_lib;
};

struct CustAicpuRegisterInfo {
  std::string file;
  const char *op_type;
  const char *func_name;
};

BinaryBuffer ReadBinaryFileToBuffer(const std::string &file_path) {
  BinaryBuffer result;
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return result;
  }
  std::streamsize file_size = file.tellg();
  if (file_size <= 0) {
    return result;
  }
  result.size = static_cast<size_t>(file_size);
  result.data = std::make_unique<uint8_t[]>(result.size);
  file.seekg(0, std::ios::beg);
  file.read(reinterpret_cast<char *>(result.data.get()), result.size);
  if (!file.good()) {
    file.close();
    result.data.reset();
    result.size = 0;
  }
  return result;
}

aclError GenerateJsonFile(const AicpuRegisterInfo &register_info, std::string &json_path) {
  using namespace std::chrono;
  int64_t cur_timestamp = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
  json_path = "/tmp/temp_ops_info_" + std::to_string(cur_timestamp) + ".json";
  std::string json_data_format = R"(
{
    "%s":{
        "opInfo":{
            "opKernelLib":"%s",
            "kernelSo":"%s",
            "functionName":"%s"
        }
    }
}
)";
  char json_data[kMaxJsonFileLen];
  auto ret = snprintf_s(json_data, kMaxJsonFileLen, kMaxJsonFileLen - 1U, json_data_format.c_str(),
                        register_info.op_type, register_info.op_kernel_lib, register_info.so_name,
                        register_info.kernel_name);
  OM2_CHK_TRUE(ret >= 0);
  std::ofstream ofs(json_path.c_str(), std::ios::trunc);
  OM2_CHK_TRUE(ofs);
  ofs << json_data;
  return ACL_SUCCESS;
}

void AssembleAicpuLoadOptions(aclrtBinaryLoadOptions &load_options, int32_t cpu_kernel_mode) {
  aclrtBinaryLoadOption option;
  load_options.numOpt = 1;
  load_options.options = &option;
  option.type = ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE;
  option.value.cpuKernelMode = cpu_kernel_mode;
}

aclError RegisterAicoreKernel(aclrtBinHandle &bin_handle, aclrtFuncHandle &func_handle, const AicoreRegisterInfo &register_info, std::unordered_map<std::string, BinDataInfo> &bin_info_map) {
  auto &bin_info = bin_info_map[register_info.file];
  aclrtBinaryLoadOptions load_options;
  aclrtBinaryLoadOption option;
  load_options.numOpt = 1;
  load_options.options = &option;
  option.type = ACL_RT_BINARY_LOAD_OPT_MAGIC;
  option.value.magic = register_info.magic;
  OM2_CHK_STATUS(aclrtBinaryLoadFromData(bin_info.data, bin_info.size, &load_options, &bin_handle));
  if (register_info.use_tiling_key) {
    OM2_CHK_STATUS(aclrtBinaryGetFunctionByEntry(bin_handle, register_info.tiling_key, &func_handle));
  } else {
    OM2_CHK_STATUS(aclrtBinaryGetFunction(bin_handle, register_info.kernel_name, &func_handle));
  }
  return ACL_SUCCESS;
}

aclError RegisterAicpuKernel(aclrtBinHandle &bin_handle, aclrtFuncHandle &func_handle, const AicpuRegisterInfo &register_info) {
  std::string json_path;
  OM2_CHK_STATUS(GenerateJsonFile(register_info, json_path));
  OM2_MAKE_GUARD(json_guard, [&json_path]() {
    (void)std::remove(json_path.c_str());
  });
  OM2_CHK_TRUE(!json_path.empty());
  aclrtBinaryLoadOptions load_options;
  aclrtBinaryLoadOption option;
  load_options.numOpt = 1;
  load_options.options = &option;
  option.type = ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE;
  option.value.cpuKernelMode = 0;
  OM2_CHK_STATUS(aclrtBinaryLoadFromFile(json_path.c_str(), &load_options, &bin_handle));
  OM2_CHK_STATUS(aclrtBinaryGetFunction(bin_handle, register_info.op_type, &func_handle));
  return ACL_SUCCESS;
}

aclError RegisterCustAicpuKernel(aclrtBinHandle &bin_handle, aclrtFuncHandle &func_handle, const CustAicpuRegisterInfo &register_info, std::unordered_map<std::string, BinDataInfo> &bin_info_map) {
  auto &bin_info = bin_info_map[register_info.file];
  aclrtBinaryLoadOptions load_options;
  aclrtBinaryLoadOption option;
  load_options.numOpt = 1;
  load_options.options = &option;
  option.type = ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE;
  option.value.cpuKernelMode = 2;
  OM2_CHK_STATUS(aclrtBinaryLoadFromData(bin_info.data, bin_info.size, &load_options, &bin_handle));
  OM2_CHK_STATUS(aclrtRegisterCpuFunc(bin_handle, register_info.func_name, register_info.op_type, &func_handle));
  return ACL_SUCCESS;
}
} // namespace
aclError Om2Model::RegisterKernels() {
  OM2_LOGI("RegisterKernels begin");
  OM2_CHK_STATUS(RegisterAicoreKernel(bin_handles_[0], func_handles_[0], {ACL_RT_BINARY_MAGIC_ELF_VECTOR_CORE, false, 0, "add1_faked_kernel", "add1_faked_kernel.o"}, bin_info_map_));
  OM2_LOGI("RegisterKernels done");
  return ACL_SUCCESS;
}
} // namespace om2)OM2";
  ASSERT_EQ(outputs[GeneratedFileIndex::kKernelRegistryFile], kernel_reg_expected + "\n");
}

TEST_F(ProgramGeneratorUt, GenerateProgram_FileStorageShape_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  for (const auto file_index : {GeneratedFileIndex::kKernelRegistryFile, GeneratedFileIndex::kResourcesFile,
                                GeneratedFileIndex::kArgsManagerFile, GeneratedFileIndex::kLoadingAndRunningFile,
                                GeneratedFileIndex::kInterfaceHeaderFile, GeneratedFileIndex::kCMakeListsFile}) {
    ASSERT_NE(outputs.find(file_index), outputs.end());
    ASSERT_FALSE(outputs[file_index].empty());
  }
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSource_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const std::string expected = R"(#line 1 "g1_load_and_run.cpp"
#include "g1_interface.h"

namespace om2 {
namespace {
Om2Tensor BuildOm2Tensor(void *device_address, uint64_t size, int32_t data_type,
                         int32_t format, const int64_t *shape_dims, uint64_t shape_dims_num) {
  Om2Tensor tensor{};
  tensor.device_address = PtrToU64(device_address);
  tensor.size = size;
  tensor.data_type = data_type;
  tensor.format = format;
  tensor.shape_dims = shape_dims;
  tensor.shape_dims_num = shape_dims_num;
  return tensor;
}

aclError AssembleOm2TaskInfo(Om2TaskInfo *task_info, const char *op_name, const char *op_type,
                             uint32_t task_id, uint32_t stream_id, uint32_t block_dim,
                             uint64_t op_desc_id, uintptr_t args_base, uint64_t args_size,
                             const Om2TaskIoEntry *inputs, uint64_t input_num,
                             const Om2TaskIoEntry *outputs, uint32_t output_num,
                             const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                             uint32_t workspace_num, uint32_t task_type, void *stream,
                             uint32_t is_raw_address = 0U) {
  task_info->op_name = op_name;
  task_info->op_type = op_type;
  task_info->task_id = task_id;
  task_info->stream_id = stream_id;
  task_info->context_id = 0U;
  task_info->thread_id = 0U;
  task_info->block_dim = block_dim;
  task_info->op_desc_id = op_desc_id;
  task_info->args_base = args_base;
  task_info->args_size = args_size;
  task_info->input_num = input_num;
  task_info->inputs = inputs;
  task_info->output_num = output_num;
  task_info->outputs = outputs;
  task_info->workspace_num = workspace_num;
  task_info->workspace_addrs = workspace_addrs;
  task_info->workspace_sizes = workspace_sizes;
  task_info->task_type = task_type;
  task_info->stream = stream;
  task_info->is_raw_address = is_raw_address;
  task_info->l0_exception_dump_info = nullptr;
  return ACL_SUCCESS;
}

aclError ReportOm2TaskPreprocess(const char *op_name, const char *op_type, uint64_t op_desc_id,
                                uintptr_t args_base, uint64_t args_size,
                                const std::vector<Om2TaskIoEntry> &inputs,
                                const std::vector<Om2TaskIoEntry> &outputs,
                                const std::vector<uint64_t> &workspace_addrs,
                                const std::vector<uint64_t> &workspace_sizes,
                                uint32_t task_type, uint32_t block_dim, void *stream,
                                const Om2L0TaskRawInfo *l0_info,
                                uint32_t model_id, void *instance_handle,
                                uint32_t is_raw_address = 0U) {
  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  OM2_CHK_TRUE(workspace_addrs.size() == workspace_sizes.size());

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, 0U, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs.data(), static_cast<uint64_t>(inputs.size()),
                                      outputs.data(), static_cast<uint32_t>(outputs.size()),
                                      workspace_addrs.empty() ? nullptr : workspace_addrs.data(),
                                      workspace_sizes.empty() ? nullptr : workspace_sizes.data(),
                                      static_cast<uint32_t>(workspace_addrs.size()),
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = l0_info;
  if (ReportDfxTaskPreprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPreprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

aclError ReportLaunchedOm2Task(const char *op_name, const char *op_type, uint64_t op_desc_id,
                               uintptr_t args_base, uint64_t args_size,
                               const Om2TaskIoEntry *inputs, uint64_t input_num,
                               const Om2TaskIoEntry *outputs, uint32_t output_num,
                               const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                               uint32_t workspace_num,
                               uint32_t task_type, uint32_t block_dim, void *stream,
                               uint32_t model_id, void *instance_handle,
                               uint32_t is_raw_address = 0U) {
  uint32_t task_id = 0U;
  OM2_CHK_RT(aclrtGetThreadLastTaskId(&task_id));

  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, task_id, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs, input_num,
                                      outputs, output_num,
                                      workspace_addrs, workspace_sizes, workspace_num,
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = nullptr;
  if (ReportDfxTaskPostprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPostprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

uint8_t GetIsDataDump(const char *op_name, uint32_t model_id, void *instance_handle) {
  if (IsDataDumpEnabled != nullptr && instance_handle != nullptr) {
    uint8_t is_data_dump = 0U;
    auto ret = IsDataDumpEnabled(model_id, instance_handle, op_name, &is_data_dump);
    return ret == 0 ? is_data_dump : 0U;
  }
  return 0U;
}

aclError AclrtMalloc(void **ptr, size_t size, uint32_t mem_type, uint16_t module_id) {
  *ptr = nullptr;
  if ((size == 0U)) {
    return ACL_SUCCESS;
  }
  aclrtMallocAttribute attr;
  attr.attr = ACL_RT_MEM_ATTR_MODULE_ID;
  attr.value.moduleId = module_id;
  aclrtMallocConfig cfg;
  cfg.attrs = &attr;
  cfg.numAttrs = 1U;
  switch (mem_type) {
    case RT_MEMORY_TS:
    return aclrtMallocForTaskScheduler(ptr, size, ACL_MEM_MALLOC_HUGE_FIRST, &cfg);
    case RT_MEMORY_HOST:
    return aclrtMallocHostWithCfg(ptr, size, &cfg);
    case RT_MEMORY_P2P_HBM:
    case RT_MEMORY_P2P_DDR:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_MALLOC_HUGE_FIRST_P2P, &cfg);
    case RT_MEMORY_DDR:
    case RT_MEMORY_DDR_NC:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_TYPE_LOW_BAND_WIDTH, &cfg);
    default:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_TYPE_HIGH_BAND_WIDTH, &cfg);
  }
}
constexpr uint16_t GE_MODULE_NAME_U16 = 45;
aclError MallocDeviceMemory(void *&dev_ptr, const size_t size, const uint32_t mem_type, std::vector<void *> &mem_ptrs) {
  uint64_t block_size = 2097152;
  if ((mem_type == RT_MEMORY_TS)) {
    block_size = 4096;
  }
  const auto aligned_size = ((((size + 512) - 1) / 512) * 512);
  const auto final_block_size = ((((aligned_size + block_size) - 1) / block_size) * block_size);
  const auto rt_ret = AclrtMalloc(&dev_ptr, final_block_size, mem_type, GE_MODULE_NAME_U16);
  if (((rt_ret != ACL_SUCCESS) || (dev_ptr == nullptr))) {
    return ACL_ERROR_FAILURE;
  }
  OM2_CHK_STATUS(aclrtMemset(dev_ptr, block_size, 0, block_size));
  mem_ptrs.push_back(dev_ptr);
  return ACL_SUCCESS;
}
constexpr const size_t max_launch_cfg_num = 8UL;
struct LaunchKernelCfgHolder {
  aclrtLaunchKernelCfg cfg{};
  aclrtLaunchKernelAttr attrs[max_launch_cfg_num];
};

struct LaunchKernelConfig {
  uint8_t schedule_mode{0U};
  aclrtEngineType engine_type{ACL_RT_ENGINE_TYPE_AIC};
  uint32_t block_dim_offset{0U};
  bool is_block_task_prefetch{false};
  uint8_t is_data_dump{0U};
  uint16_t time_out{0U};
  uint32_t local_memory_size{0U};
};

aclError AssembleLaunchConfig(LaunchKernelCfgHolder &holder, const LaunchKernelConfig &launch_config) {
  size_t actual_cfg_num = 0UL;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
  holder.attrs[actual_cfg_num].value.schemMode = launch_config.schedule_mode;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
  holder.attrs[actual_cfg_num].value.engineType = launch_config.engine_type;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
  holder.attrs[actual_cfg_num].value.blockDimOffset = launch_config.block_dim_offset;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCK_TASK_PREFETCH;
  holder.attrs[actual_cfg_num].value.isBlockTaskPrefetch = static_cast<uint8_t>(launch_config.is_block_task_prefetch);
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DATA_DUMP;
  holder.attrs[actual_cfg_num].value.isDataDump = launch_config.is_data_dump;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
  holder.attrs[actual_cfg_num].value.dynUBufSize = launch_config.local_memory_size;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
  holder.attrs[actual_cfg_num].value.timeout = launch_config.time_out;
  actual_cfg_num++;
  holder.cfg.attrs = &holder.attrs[0];
  holder.cfg.numAttrs = actual_cfg_num;
  return ACL_SUCCESS;
}
constexpr int64_t kDImEndFlag = std::numeric_limits<int64_t>::min();
aclError KernelTaskDistribute(const std::vector<uint64_t> &io_addrs, ArgsInfo *args_info, aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config) {
  OM2_CHK_NOTNULL(args_info);
  OM2_CHK_STATUS(memcpy_s(args_info->host_addr, args_info->size, io_addrs.data(), (io_addrs.size() * sizeof(uint64_t))));
  OM2_CHK_STATUS(aclrtLaunchKernelV2(func_handle, block_dim, args_info->dev_addr, args_info->size, config, stream));
  return ACL_SUCCESS;
}
constexpr uint32_t kAicpuArgsExtInfoAddrOffset = 12U;
constexpr uint32_t kAicpuArgsio_addr_offset = 20U;
aclError UpdateExtInfoSession(uint8_t *extInfo, size_t session_info_offset, uint64_t *session_id, uint64_t *kernel_id) {
  AicpuSessionInfo *session_info = reinterpret_cast<AicpuSessionInfo *>(&extInfo[session_info_offset]);
  session_info->sessionId = *session_id;
  session_info->kernelId = *kernel_id;
  session_info->sessFlag = true;
  *kernel_id++;
  return ACL_SUCCESS;
}

aclError AssembleAicpuExtInfo(uint8_t *ext_info, size_t ext_info_len, int32_t session_info_offset, uint64_t *session_id, uint64_t *kernel_id, std::vector<void *> &dev_ext_info_mem_ptrs, size_t index) {
  std::unique_ptr<uint8_t[]> tmp_ext_info = std::make_unique<uint8_t[]>(ext_info_len);
  memcpy_s(tmp_ext_info.get(), ext_info_len, ext_info, ext_info_len);
  if ((session_info_offset != -1)) {
    OM2_CHK_STATUS(UpdateExtInfoSession(tmp_ext_info.get(), session_info_offset, session_id, kernel_id));
  }
  void *dev_ptr = nullptr;
  OM2_CHK_STATUS(aclrtMallocAlign32(&dev_ptr, ext_info_len, ACL_MEM_MALLOC_HUGE_FIRST));
  OM2_CHK_STATUS(aclrtMemcpy(dev_ptr, ext_info_len, tmp_ext_info.get(), ext_info_len, ACL_MEMCPY_HOST_TO_DEVICE));
  dev_ext_info_mem_ptrs[index] = dev_ptr;
  return ACL_SUCCESS;
}

aclError AssembleAicpuArgs(uint8_t *args, size_t args_len, void *ext_info_addr, size_t ext_info_len, std::vector<uint64_t> &io_addr, void *target_args_ptr) {
  std::unique_ptr<uint8_t[]> tmp_args = std::make_unique<uint8_t[]>(args_len);
  memcpy_s(tmp_args.get(), args_len, args, args_len);
  const auto aicpu_param_head = reinterpret_cast<AicpuParamHead *>(tmp_args.get());
  aicpu_param_head->extInfoLength = static_cast<uint32_t>(ext_info_len);
  uint64_t ext_info_addr_value = reinterpret_cast<uint64_t>(ext_info_addr);
  memcpy_s((tmp_args.get() + kAicpuArgsExtInfoAddrOffset), sizeof(uint64_t), &ext_info_addr_value, sizeof(uint64_t));
  size_t addrs_size = (sizeof(uint64_t) * io_addr.size());
  memcpy_s((tmp_args.get() + kAicpuArgsio_addr_offset), addrs_size, io_addr.data(), addrs_size);
  memcpy_s(target_args_ptr, args_len, tmp_args.get(), args_len);
  return ACL_SUCCESS;
}

aclError AicpuKernelTaskDistribute(const std::vector<uint8_t> &args, ArgsInfo *args_info, aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config) {
  OM2_CHK_NOTNULL(args_info);
  OM2_CHK_STATUS(memcpy_s(args_info->host_addr, args_info->size, args.data(), args.size()));
  OM2_CHK_STATUS(aclrtLaunchKernelV2(func_handle, block_dim, args_info->dev_addr, args_info->size, config, stream));
  return ACL_SUCCESS;
}

aclError GetEventIdAddr(void *&event_addr, std::map<uint32_t, void *> &event_id_mem_map, uint32_t event_id, std::vector<void *> &mem_ptrs) {
  auto it = event_id_mem_map.find(event_id);
  if ((event_id_mem_map.end() != it)) {
    event_addr = it->second;
    return ACL_SUCCESS;
  }
  OM2_CHK_STATUS(MallocDeviceMemory(event_addr, 8, 2, mem_ptrs));
  OM2_CHK_STATUS(aclrtMemset(event_addr, 8, 0, 8));
  event_id_mem_map[event_id] = event_addr;
  return ACL_SUCCESS;
}

aclError DispatchOp(const OpDef *op, const DispatchOpContext &ctx) {
  switch (static_cast<OpDispatchType>(op->dispatch_type)) {
    case 0U:
    {
      LaunchKernelCfgHolder cfg_holder;
      LaunchKernelConfig launch_config = {
        .schedule_mode = op->task_data.aicore.launch.schedule_mode,
        .engine_type = static_cast<aclrtEngineType>(op->task_data.aicore.launch.engine_type),
        .block_dim_offset = op->task_data.aicore.launch.block_dim_offset,
        .is_block_task_prefetch = op->task_data.aicore.launch.is_block_task_prefetch,
        .is_data_dump = GetIsDataDump(op->op_name, ctx.model_id, ctx.instance_handle),
        .time_out = op->task_data.aicore.launch.time_out,
        .local_memory_size = op->task_data.aicore.launch.local_memory_size,
      };
      OM2_CHK_STATUS(AssembleLaunchConfig(cfg_holder, launch_config));
      ArgsInfo *args_info = ctx.args_table.GetArgsInfo(op->task_data.aicore.args_idx);
      OM2_CHK_NOTNULL(args_info);
      std::vector<uint64_t> ordered_io_addrs;
      std::vector<Om2Tensor> io_tensors;
      (io_tensors.reserve(op->task_data.aicore.num_io_addrs));
      std::vector<Om2TaskIoEntry> report_inputs;
      std::vector<Om2TaskIoEntry> report_outputs;
      std::vector<uint64_t> report_workspace_addrs;
      std::vector<uint64_t> report_workspace_sizes;
      for (uint32_t j = 0U; (j < op->task_data.aicore.num_io_addrs); j++) {
        const auto &a = op->argsInfo[j];
        uint64_t _addr = 0U;
        switch (a.type) {
          case OP_ARG_INPUT:
          case OP_ARG_OUTPUT:
          case OP_ARG_CONST_TENSOR:
          {
            _addr = reinterpret_cast<uint64_t>(ResolveOpAddr(a.addr.mem_src, a.addr.offset, ctx.total_dev_mem_ptr, ctx.session_scope_mem_ptr, ctx.constants));
            io_tensors.push_back(BuildOm2Tensor(reinterpret_cast<void *>(_addr), a.data.tensor.size, a.data.tensor.data_type, a.data.tensor.format, a.data.tensor.shape, a.data.tensor.num_shape_dims));
            Om2TaskIoEntry _entry = {&io_tensors.back(), a.data.tensor.args_offset};
            if (((a.type == OP_ARG_INPUT) || (a.type == OP_ARG_CONST_TENSOR))) {
              report_inputs.push_back(_entry);
            } else {
              report_outputs.push_back(_entry);
            }
            break;
          }
          case OP_ARG_WORKSPACE:
          {
            _addr = reinterpret_cast<uint64_t>(ResolveOpAddr(a.addr.mem_src, a.addr.offset, ctx.total_dev_mem_ptr, ctx.session_scope_mem_ptr, ctx.constants));
            report_workspace_addrs.push_back(_addr);
            report_workspace_sizes.push_back(a.data.tensor.size);
            break;
          }
          case OP_ARG_LEVEL1_DESC:
          {
            void *_desc = ctx.args_table.GetDevArgAddr(a.data.custom_value);
            OM2_CHK_NOTNULL(_desc);
            _addr = reinterpret_cast<uint64_t>(_desc);
            break;
          }
          case OP_ARG_SHAPE_INFO:
          case OP_ARG_CUSTOM_VALUE:
          {
            _addr = a.data.custom_value;
            break;
          }
          case OP_ARG_PLACEHOLDER:
          case OP_ARG_OPTIONAL_EMPTY:
          {
            _addr = 0U;
            break;
          }
          case OP_ARG_FFTS_ADDR:
          {
            void *_ffts = nullptr;
            OM2_CHK_STATUS(aclrtGetHardwareSyncAddr(&_ffts));
            _addr = reinterpret_cast<uint64_t>(_ffts);
            break;
          }
          case OP_ARG_EVENT_ADDR:
          {
            void *_event = nullptr;
            OM2_CHK_STATUS(GetEventIdAddr(_event, ctx.event_id_mem_map, static_cast<uint32_t>(a.data.custom_value), ctx.dev_dynamic_mem_ptrs));
            _addr = reinterpret_cast<uint64_t>(_event);
            break;
          }
          case OP_ARG_OVERFLOW_ADDR:
          {
            _addr = reinterpret_cast<uint64_t>(ctx.overflow_addr);
            break;
          }
          case OP_ARG_TILING:
          {
            void *_tiling = nullptr;
            OM2_CHK_STATUS(MallocDeviceMemory(_tiling, a.data.tiling.raw_data_len, 2U, ctx.dev_dynamic_mem_ptrs));
            OM2_CHK_STATUS(aclrtMemcpy(_tiling, a.data.tiling.raw_data_len, a.data.tiling.raw_data, a.data.tiling.raw_data_len, ACL_MEMCPY_HOST_TO_DEVICE));
            _addr = reinterpret_cast<uint64_t>(_tiling);
            break;
          }
          default:
          {
            _addr = 0U;
            break;
          }
        }
        ordered_io_addrs.push_back(_addr);
      }
      Om2L0TaskRawInfo l0_info = {1U, op->task_data.aicore.l0.need_assert_or_printf, static_cast<uint64_t>(op->task_data.aicore.l0.num_l0_slots), op->task_data.aicore.l0.l0_slots};
      OM2_CHK_STATUS(ReportOm2TaskPreprocess(op->op_name, op->task_data.aicore.op_type, 0U, reinterpret_cast<uintptr_t>(args_info->dev_addr), args_info->size, report_inputs, report_outputs, report_workspace_addrs, report_workspace_sizes, static_cast<uint32_t>(op->dispatch_type), op->task_data.aicore.kernel.block_dim, ctx.stream, &l0_info, ctx.model_id, ctx.instance_handle));
      OM2_CHK_STATUS(KernelTaskDistribute(ordered_io_addrs, args_info, ctx.func_handles[op->task_data.aicore.kernel.func_idx], op->task_data.aicore.kernel.block_dim, ctx.stream, &cfg_holder.cfg));
      OM2_CHK_STATUS(ReportLaunchedOm2Task(op->op_name, op->task_data.aicore.op_type, 0U, reinterpret_cast<uintptr_t>(args_info->dev_addr), args_info->size, report_inputs.data(), static_cast<uint64_t>(report_inputs.size()), report_outputs.data(), static_cast<uint32_t>(report_outputs.size()), report_workspace_addrs.data(), report_workspace_sizes.data(), static_cast<uint32_t>(report_workspace_sizes.size()), static_cast<uint32_t>(op->dispatch_type), op->task_data.aicore.kernel.block_dim, ctx.stream, ctx.model_id, ctx.instance_handle));
    }
    break;
    default:
    return ACL_ERROR_FAILURE;
  }
  return ACL_SUCCESS;
}
const OpDef kOpDefs[] = {{
  .dispatch_type = static_cast<OpDispatchType>(0),
  .op_name = "add1",
  .argsInfo = (const OpArgInfo[]){
    {.type = 0, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 0}}},
    {.type = 0, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 8}}},
    {.type = 1, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 16}}},
    {.type = 2, .addr = {.mem_src = 0, .offset = 0}, .data = {.tensor = {.size = 64}}},
  },
  .task_data = {
    .aicore = {
      .op_type = "Add",
      .num_io_addrs = 4,
      .args_idx = 0,
      .launch = {.schedule_mode = 0, .engine_type = 0, .block_dim_offset = 0, .is_block_task_prefetch = false, .time_out = 0, .local_memory_size = 0},
      .kernel = {.block_dim = 8, .func_idx = 0},
      .l0 = {.need_assert_or_printf = 0, .num_l0_slots = 4, .l0_slots = (const Om2L0ArgSlotInfo[]){{OM2_L0_ARG_INPUT, 0U, 0U, 0UL, 0U, 0U, 0U}, {OM2_L0_ARG_INPUT, 0U, 8U, 0UL, 1U, 0U, 0U}, {OM2_L0_ARG_OUTPUT, 0U, 16U, 0UL, 2U, 0U, 0U}, {OM2_L0_ARG_WORKSPACE, 0U, 24U, 0UL, 0U, 0U, 0U}}},
    },
  },
}};
} // namespace
aclmdlRI Om2Model::GetRtModelHandle() {
  return model_handle_;
}

aclError Om2Model::Load() {
  OM2_LOGI("Load begin");
  dev_ext_info_mem_ptrs_.resize(0);
  OM2_CHK_STATUS(DispatchOp(&kOpDefs[0], {total_dev_mem_ptr_, session_scope_mem_ptr_, constants_, args_table_, func_handles_.data(), stream_list_[0], model_id_, instance_handle_, mem_event_id_mem_map_, dev_dynamic_mem_ptrs_, overflow_addr_}));
  OM2_CHK_STATUS(aclmdlRIBuildEnd(model_handle_, nullptr));
  OM2_LOGI("Load done");
  return ACL_SUCCESS;
}

aclError Om2Model::RunAsync(aclrtStream &exe_stream, size_t input_count, void **input_data, size_t output_count, void **output_data) {
  OM2_LOGI("RunAsync begin");
  if (((input_count != om2::INPUT_NUM) || (output_count != om2::OUTPUT_NUM))) {
    return ACL_ERROR_FAILURE;
  }
  auto input_data_0_tensor = reinterpret_cast<gert::Tensor *>(input_data[0]);
  auto input_data_1_tensor = reinterpret_cast<gert::Tensor *>(input_data[1]);
  auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0]);
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(0, reinterpret_cast<uintptr_t>(input_data_0_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(1, reinterpret_cast<uintptr_t>(input_data_1_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr())));

  OM2_CHK_STATUS(args_table_.CopyArgsToDevice());
  OM2_CHK_STATUS(aclmdlRIExecuteAsync(model_handle_, exe_stream));

  OM2_LOGI("RunAsync done");
  return ACL_SUCCESS;
}

aclError Om2Model::Run(size_t input_count, void **input_data, size_t output_count, void **output_data, int32_t stream_sync_timeout) {
  OM2_LOGI("Run begin");
  if (((input_count != om2::INPUT_NUM) || (output_count != om2::OUTPUT_NUM))) {
    return ACL_ERROR_FAILURE;
  }
  auto input_data_0_tensor = reinterpret_cast<gert::Tensor *>(input_data[0]);
  auto input_data_1_tensor = reinterpret_cast<gert::Tensor *>(input_data[1]);
  auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0]);
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(0, reinterpret_cast<uintptr_t>(input_data_0_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(1, reinterpret_cast<uintptr_t>(input_data_1_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr())));

  OM2_CHK_STATUS(args_table_.CopyArgsToDevice());
  OM2_CHK_STATUS(aclmdlRIExecute(model_handle_, stream_sync_timeout));

  OM2_LOGI("Run done");
  return ACL_SUCCESS;
}
} // namespace om2
aclError Om2ModelCreate(om2::Om2ModelHandle *model_handle, aclmdlRI *rt_model_handle, const char **bin_files, const void **bin_data, size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle) {
  OM2_LOGI("Om2ModelCreate");
  if ((model_handle == nullptr) || (rt_model_handle == nullptr) || (*model_handle != nullptr)) {
    OM2_LOGE("Om2ModelCreate: invalid handle");
    return ACL_ERROR_FAILURE;
  }
  auto *obj = new om2::Om2Model(bin_files, bin_data, bin_size, bin_num, constants, work_ptr, session_id,
                                model_id, instance_handle);
  if (obj == nullptr) {
    OM2_LOGE("Om2ModelCreate: new Om2Model failed");
    return ACL_ERROR_FAILURE;
  }
  auto ret = obj->InitResources();
  if (ret != ACL_SUCCESS) {
    OM2_LOGE("Om2ModelCreate: InitResources failed, ret: %d", ret);
    delete obj;
    return ret;
  }
  ret = obj->RegisterKernels();
  if (ret != ACL_SUCCESS) {
    OM2_LOGE("Om2ModelCreate: RegisterKernels failed, ret: %d", ret);
    delete obj;
    return ret;
  }
  *model_handle = reinterpret_cast<om2::Om2ModelHandle>(obj);
  *rt_model_handle = obj->GetRtModelHandle();
  OM2_LOGI("Om2ModelCreate done");
  return ACL_SUCCESS;
}

aclError Om2ModelLoad(om2::Om2ModelHandle *model_handle) {
  OM2_LOGI("Om2ModelLoad");
  if ((model_handle == nullptr) || (*model_handle == nullptr)) {
    OM2_LOGE("Om2ModelLoad: invalid handle");
    return ACL_ERROR_FAILURE;
  }
  return static_cast<om2::Om2Model*>(*model_handle)->Load();
}

aclError Om2ModelRunAsync(om2::Om2ModelHandle *model_handle, aclrtStream stream, int input_count, void **input_data, int output_count, void **output_data) {
  OM2_LOGI("Om2ModelRunAsync");
  return static_cast<om2::Om2Model*>(*model_handle)->RunAsync(stream, input_count, input_data, output_count, output_data);
}

aclError Om2ModelRun(om2::Om2ModelHandle *model_handle, int input_count, void **input_data, int output_count, void **output_data, int32_t stream_sync_timeout) {
  OM2_LOGI("Om2ModelRun");
  return static_cast<om2::Om2Model*>(*model_handle)->Run(input_count, input_data, output_count, output_data, stream_sync_timeout);
}

aclError Om2ModelDestroy(om2::Om2ModelHandle *model_handle) {
  OM2_LOGI("Om2ModelDestroy");
  delete static_cast<om2::Om2Model*>(*model_handle);
  return ACL_SUCCESS;
})";
  ASSERT_EQ(outputs[GeneratedFileIndex::kLoadingAndRunningFile], expected + "\n");
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSource2_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp2();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const std::string expected = R"(#line 1 "g1_load_and_run.cpp"
#include "g1_interface.h"

namespace om2 {
namespace {
Om2Tensor BuildOm2Tensor(void *device_address, uint64_t size, int32_t data_type,
                         int32_t format, const int64_t *shape_dims, uint64_t shape_dims_num) {
  Om2Tensor tensor{};
  tensor.device_address = PtrToU64(device_address);
  tensor.size = size;
  tensor.data_type = data_type;
  tensor.format = format;
  tensor.shape_dims = shape_dims;
  tensor.shape_dims_num = shape_dims_num;
  return tensor;
}

aclError AssembleOm2TaskInfo(Om2TaskInfo *task_info, const char *op_name, const char *op_type,
                             uint32_t task_id, uint32_t stream_id, uint32_t block_dim,
                             uint64_t op_desc_id, uintptr_t args_base, uint64_t args_size,
                             const Om2TaskIoEntry *inputs, uint64_t input_num,
                             const Om2TaskIoEntry *outputs, uint32_t output_num,
                             const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                             uint32_t workspace_num, uint32_t task_type, void *stream,
                             uint32_t is_raw_address = 0U) {
  task_info->op_name = op_name;
  task_info->op_type = op_type;
  task_info->task_id = task_id;
  task_info->stream_id = stream_id;
  task_info->context_id = 0U;
  task_info->thread_id = 0U;
  task_info->block_dim = block_dim;
  task_info->op_desc_id = op_desc_id;
  task_info->args_base = args_base;
  task_info->args_size = args_size;
  task_info->input_num = input_num;
  task_info->inputs = inputs;
  task_info->output_num = output_num;
  task_info->outputs = outputs;
  task_info->workspace_num = workspace_num;
  task_info->workspace_addrs = workspace_addrs;
  task_info->workspace_sizes = workspace_sizes;
  task_info->task_type = task_type;
  task_info->stream = stream;
  task_info->is_raw_address = is_raw_address;
  task_info->l0_exception_dump_info = nullptr;
  return ACL_SUCCESS;
}

aclError ReportOm2TaskPreprocess(const char *op_name, const char *op_type, uint64_t op_desc_id,
                                uintptr_t args_base, uint64_t args_size,
                                const std::vector<Om2TaskIoEntry> &inputs,
                                const std::vector<Om2TaskIoEntry> &outputs,
                                const std::vector<uint64_t> &workspace_addrs,
                                const std::vector<uint64_t> &workspace_sizes,
                                uint32_t task_type, uint32_t block_dim, void *stream,
                                const Om2L0TaskRawInfo *l0_info,
                                uint32_t model_id, void *instance_handle,
                                uint32_t is_raw_address = 0U) {
  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  OM2_CHK_TRUE(workspace_addrs.size() == workspace_sizes.size());

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, 0U, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs.data(), static_cast<uint64_t>(inputs.size()),
                                      outputs.data(), static_cast<uint32_t>(outputs.size()),
                                      workspace_addrs.empty() ? nullptr : workspace_addrs.data(),
                                      workspace_sizes.empty() ? nullptr : workspace_sizes.data(),
                                      static_cast<uint32_t>(workspace_addrs.size()),
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = l0_info;
  if (ReportDfxTaskPreprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPreprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

aclError ReportLaunchedOm2Task(const char *op_name, const char *op_type, uint64_t op_desc_id,
                               uintptr_t args_base, uint64_t args_size,
                               const Om2TaskIoEntry *inputs, uint64_t input_num,
                               const Om2TaskIoEntry *outputs, uint32_t output_num,
                               const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                               uint32_t workspace_num,
                               uint32_t task_type, uint32_t block_dim, void *stream,
                               uint32_t model_id, void *instance_handle,
                               uint32_t is_raw_address = 0U) {
  uint32_t task_id = 0U;
  OM2_CHK_RT(aclrtGetThreadLastTaskId(&task_id));

  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, task_id, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs, input_num,
                                      outputs, output_num,
                                      workspace_addrs, workspace_sizes, workspace_num,
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = nullptr;
  if (ReportDfxTaskPostprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPostprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

uint8_t GetIsDataDump(const char *op_name, uint32_t model_id, void *instance_handle) {
  if (IsDataDumpEnabled != nullptr && instance_handle != nullptr) {
    uint8_t is_data_dump = 0U;
    auto ret = IsDataDumpEnabled(model_id, instance_handle, op_name, &is_data_dump);
    return ret == 0 ? is_data_dump : 0U;
  }
  return 0U;
}

aclError AclrtMalloc(void **ptr, size_t size, uint32_t mem_type, uint16_t module_id) {
  *ptr = nullptr;
  if ((size == 0U)) {
    return ACL_SUCCESS;
  }
  aclrtMallocAttribute attr;
  attr.attr = ACL_RT_MEM_ATTR_MODULE_ID;
  attr.value.moduleId = module_id;
  aclrtMallocConfig cfg;
  cfg.attrs = &attr;
  cfg.numAttrs = 1U;
  switch (mem_type) {
    case RT_MEMORY_TS:
    return aclrtMallocForTaskScheduler(ptr, size, ACL_MEM_MALLOC_HUGE_FIRST, &cfg);
    case RT_MEMORY_HOST:
    return aclrtMallocHostWithCfg(ptr, size, &cfg);
    case RT_MEMORY_P2P_HBM:
    case RT_MEMORY_P2P_DDR:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_MALLOC_HUGE_FIRST_P2P, &cfg);
    case RT_MEMORY_DDR:
    case RT_MEMORY_DDR_NC:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_TYPE_LOW_BAND_WIDTH, &cfg);
    default:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_TYPE_HIGH_BAND_WIDTH, &cfg);
  }
}
constexpr uint16_t GE_MODULE_NAME_U16 = 45;
aclError MallocDeviceMemory(void *&dev_ptr, const size_t size, const uint32_t mem_type, std::vector<void *> &mem_ptrs) {
  uint64_t block_size = 2097152;
  if ((mem_type == RT_MEMORY_TS)) {
    block_size = 4096;
  }
  const auto aligned_size = ((((size + 512) - 1) / 512) * 512);
  const auto final_block_size = ((((aligned_size + block_size) - 1) / block_size) * block_size);
  const auto rt_ret = AclrtMalloc(&dev_ptr, final_block_size, mem_type, GE_MODULE_NAME_U16);
  if (((rt_ret != ACL_SUCCESS) || (dev_ptr == nullptr))) {
    return ACL_ERROR_FAILURE;
  }
  OM2_CHK_STATUS(aclrtMemset(dev_ptr, block_size, 0, block_size));
  mem_ptrs.push_back(dev_ptr);
  return ACL_SUCCESS;
}
constexpr const size_t max_launch_cfg_num = 8UL;
struct LaunchKernelCfgHolder {
  aclrtLaunchKernelCfg cfg{};
  aclrtLaunchKernelAttr attrs[max_launch_cfg_num];
};

struct LaunchKernelConfig {
  uint8_t schedule_mode{0U};
  aclrtEngineType engine_type{ACL_RT_ENGINE_TYPE_AIC};
  uint32_t block_dim_offset{0U};
  bool is_block_task_prefetch{false};
  uint8_t is_data_dump{0U};
  uint16_t time_out{0U};
  uint32_t local_memory_size{0U};
};

aclError AssembleLaunchConfig(LaunchKernelCfgHolder &holder, const LaunchKernelConfig &launch_config) {
  size_t actual_cfg_num = 0UL;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
  holder.attrs[actual_cfg_num].value.schemMode = launch_config.schedule_mode;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
  holder.attrs[actual_cfg_num].value.engineType = launch_config.engine_type;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
  holder.attrs[actual_cfg_num].value.blockDimOffset = launch_config.block_dim_offset;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCK_TASK_PREFETCH;
  holder.attrs[actual_cfg_num].value.isBlockTaskPrefetch = static_cast<uint8_t>(launch_config.is_block_task_prefetch);
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DATA_DUMP;
  holder.attrs[actual_cfg_num].value.isDataDump = launch_config.is_data_dump;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
  holder.attrs[actual_cfg_num].value.dynUBufSize = launch_config.local_memory_size;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
  holder.attrs[actual_cfg_num].value.timeout = launch_config.time_out;
  actual_cfg_num++;
  holder.cfg.attrs = &holder.attrs[0];
  holder.cfg.numAttrs = actual_cfg_num;
  return ACL_SUCCESS;
}
constexpr int64_t kDImEndFlag = std::numeric_limits<int64_t>::min();
aclError KernelTaskDistribute(const std::vector<uint64_t> &io_addrs, ArgsInfo *args_info, aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config) {
  OM2_CHK_NOTNULL(args_info);
  OM2_CHK_STATUS(memcpy_s(args_info->host_addr, args_info->size, io_addrs.data(), (io_addrs.size() * sizeof(uint64_t))));
  OM2_CHK_STATUS(aclrtLaunchKernelV2(func_handle, block_dim, args_info->dev_addr, args_info->size, config, stream));
  return ACL_SUCCESS;
}
constexpr uint32_t kAicpuArgsExtInfoAddrOffset = 12U;
constexpr uint32_t kAicpuArgsio_addr_offset = 20U;
aclError UpdateExtInfoSession(uint8_t *extInfo, size_t session_info_offset, uint64_t *session_id, uint64_t *kernel_id) {
  AicpuSessionInfo *session_info = reinterpret_cast<AicpuSessionInfo *>(&extInfo[session_info_offset]);
  session_info->sessionId = *session_id;
  session_info->kernelId = *kernel_id;
  session_info->sessFlag = true;
  *kernel_id++;
  return ACL_SUCCESS;
}

aclError AssembleAicpuExtInfo(uint8_t *ext_info, size_t ext_info_len, int32_t session_info_offset, uint64_t *session_id, uint64_t *kernel_id, std::vector<void *> &dev_ext_info_mem_ptrs, size_t index) {
  std::unique_ptr<uint8_t[]> tmp_ext_info = std::make_unique<uint8_t[]>(ext_info_len);
  memcpy_s(tmp_ext_info.get(), ext_info_len, ext_info, ext_info_len);
  if ((session_info_offset != -1)) {
    OM2_CHK_STATUS(UpdateExtInfoSession(tmp_ext_info.get(), session_info_offset, session_id, kernel_id));
  }
  void *dev_ptr = nullptr;
  OM2_CHK_STATUS(aclrtMallocAlign32(&dev_ptr, ext_info_len, ACL_MEM_MALLOC_HUGE_FIRST));
  OM2_CHK_STATUS(aclrtMemcpy(dev_ptr, ext_info_len, tmp_ext_info.get(), ext_info_len, ACL_MEMCPY_HOST_TO_DEVICE));
  dev_ext_info_mem_ptrs[index] = dev_ptr;
  return ACL_SUCCESS;
}

aclError AssembleAicpuArgs(uint8_t *args, size_t args_len, void *ext_info_addr, size_t ext_info_len, std::vector<uint64_t> &io_addr, void *target_args_ptr) {
  std::unique_ptr<uint8_t[]> tmp_args = std::make_unique<uint8_t[]>(args_len);
  memcpy_s(tmp_args.get(), args_len, args, args_len);
  const auto aicpu_param_head = reinterpret_cast<AicpuParamHead *>(tmp_args.get());
  aicpu_param_head->extInfoLength = static_cast<uint32_t>(ext_info_len);
  uint64_t ext_info_addr_value = reinterpret_cast<uint64_t>(ext_info_addr);
  memcpy_s((tmp_args.get() + kAicpuArgsExtInfoAddrOffset), sizeof(uint64_t), &ext_info_addr_value, sizeof(uint64_t));
  size_t addrs_size = (sizeof(uint64_t) * io_addr.size());
  memcpy_s((tmp_args.get() + kAicpuArgsio_addr_offset), addrs_size, io_addr.data(), addrs_size);
  memcpy_s(target_args_ptr, args_len, tmp_args.get(), args_len);
  return ACL_SUCCESS;
}

aclError AicpuKernelTaskDistribute(const std::vector<uint8_t> &args, ArgsInfo *args_info, aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config) {
  OM2_CHK_NOTNULL(args_info);
  OM2_CHK_STATUS(memcpy_s(args_info->host_addr, args_info->size, args.data(), args.size()));
  OM2_CHK_STATUS(aclrtLaunchKernelV2(func_handle, block_dim, args_info->dev_addr, args_info->size, config, stream));
  return ACL_SUCCESS;
}

aclError GetEventIdAddr(void *&event_addr, std::map<uint32_t, void *> &event_id_mem_map, uint32_t event_id, std::vector<void *> &mem_ptrs) {
  auto it = event_id_mem_map.find(event_id);
  if ((event_id_mem_map.end() != it)) {
    event_addr = it->second;
    return ACL_SUCCESS;
  }
  OM2_CHK_STATUS(MallocDeviceMemory(event_addr, 8, 2, mem_ptrs));
  OM2_CHK_STATUS(aclrtMemset(event_addr, 8, 0, 8));
  event_id_mem_map[event_id] = event_addr;
  return ACL_SUCCESS;
}

aclError DispatchOp(const OpDef *op, const DispatchOpContext &ctx) {
  switch (static_cast<OpDispatchType>(op->dispatch_type)) {
    case 0U:
    {
      LaunchKernelCfgHolder cfg_holder;
      LaunchKernelConfig launch_config = {
        .schedule_mode = op->task_data.aicore.launch.schedule_mode,
        .engine_type = static_cast<aclrtEngineType>(op->task_data.aicore.launch.engine_type),
        .block_dim_offset = op->task_data.aicore.launch.block_dim_offset,
        .is_block_task_prefetch = op->task_data.aicore.launch.is_block_task_prefetch,
        .is_data_dump = GetIsDataDump(op->op_name, ctx.model_id, ctx.instance_handle),
        .time_out = op->task_data.aicore.launch.time_out,
        .local_memory_size = op->task_data.aicore.launch.local_memory_size,
      };
      OM2_CHK_STATUS(AssembleLaunchConfig(cfg_holder, launch_config));
      ArgsInfo *args_info = ctx.args_table.GetArgsInfo(op->task_data.aicore.args_idx);
      OM2_CHK_NOTNULL(args_info);
      std::vector<uint64_t> ordered_io_addrs;
      std::vector<Om2Tensor> io_tensors;
      (io_tensors.reserve(op->task_data.aicore.num_io_addrs));
      std::vector<Om2TaskIoEntry> report_inputs;
      std::vector<Om2TaskIoEntry> report_outputs;
      std::vector<uint64_t> report_workspace_addrs;
      std::vector<uint64_t> report_workspace_sizes;
      for (uint32_t j = 0U; (j < op->task_data.aicore.num_io_addrs); j++) {
        const auto &a = op->argsInfo[j];
        uint64_t _addr = 0U;
        switch (a.type) {
          case OP_ARG_INPUT:
          case OP_ARG_OUTPUT:
          case OP_ARG_CONST_TENSOR:
          {
            _addr = reinterpret_cast<uint64_t>(ResolveOpAddr(a.addr.mem_src, a.addr.offset, ctx.total_dev_mem_ptr, ctx.session_scope_mem_ptr, ctx.constants));
            io_tensors.push_back(BuildOm2Tensor(reinterpret_cast<void *>(_addr), a.data.tensor.size, a.data.tensor.data_type, a.data.tensor.format, a.data.tensor.shape, a.data.tensor.num_shape_dims));
            Om2TaskIoEntry _entry = {&io_tensors.back(), a.data.tensor.args_offset};
            if (((a.type == OP_ARG_INPUT) || (a.type == OP_ARG_CONST_TENSOR))) {
              report_inputs.push_back(_entry);
            } else {
              report_outputs.push_back(_entry);
            }
            break;
          }
          case OP_ARG_WORKSPACE:
          {
            _addr = reinterpret_cast<uint64_t>(ResolveOpAddr(a.addr.mem_src, a.addr.offset, ctx.total_dev_mem_ptr, ctx.session_scope_mem_ptr, ctx.constants));
            report_workspace_addrs.push_back(_addr);
            report_workspace_sizes.push_back(a.data.tensor.size);
            break;
          }
          case OP_ARG_LEVEL1_DESC:
          {
            void *_desc = ctx.args_table.GetDevArgAddr(a.data.custom_value);
            OM2_CHK_NOTNULL(_desc);
            _addr = reinterpret_cast<uint64_t>(_desc);
            break;
          }
          case OP_ARG_SHAPE_INFO:
          case OP_ARG_CUSTOM_VALUE:
          {
            _addr = a.data.custom_value;
            break;
          }
          case OP_ARG_PLACEHOLDER:
          case OP_ARG_OPTIONAL_EMPTY:
          {
            _addr = 0U;
            break;
          }
          case OP_ARG_FFTS_ADDR:
          {
            void *_ffts = nullptr;
            OM2_CHK_STATUS(aclrtGetHardwareSyncAddr(&_ffts));
            _addr = reinterpret_cast<uint64_t>(_ffts);
            break;
          }
          case OP_ARG_EVENT_ADDR:
          {
            void *_event = nullptr;
            OM2_CHK_STATUS(GetEventIdAddr(_event, ctx.event_id_mem_map, static_cast<uint32_t>(a.data.custom_value), ctx.dev_dynamic_mem_ptrs));
            _addr = reinterpret_cast<uint64_t>(_event);
            break;
          }
          case OP_ARG_OVERFLOW_ADDR:
          {
            _addr = reinterpret_cast<uint64_t>(ctx.overflow_addr);
            break;
          }
          case OP_ARG_TILING:
          {
            void *_tiling = nullptr;
            OM2_CHK_STATUS(MallocDeviceMemory(_tiling, a.data.tiling.raw_data_len, 2U, ctx.dev_dynamic_mem_ptrs));
            OM2_CHK_STATUS(aclrtMemcpy(_tiling, a.data.tiling.raw_data_len, a.data.tiling.raw_data, a.data.tiling.raw_data_len, ACL_MEMCPY_HOST_TO_DEVICE));
            _addr = reinterpret_cast<uint64_t>(_tiling);
            break;
          }
          default:
          {
            _addr = 0U;
            break;
          }
        }
        ordered_io_addrs.push_back(_addr);
      }
      Om2L0TaskRawInfo l0_info = {1U, op->task_data.aicore.l0.need_assert_or_printf, static_cast<uint64_t>(op->task_data.aicore.l0.num_l0_slots), op->task_data.aicore.l0.l0_slots};
      OM2_CHK_STATUS(ReportOm2TaskPreprocess(op->op_name, op->task_data.aicore.op_type, 0U, reinterpret_cast<uintptr_t>(args_info->dev_addr), args_info->size, report_inputs, report_outputs, report_workspace_addrs, report_workspace_sizes, static_cast<uint32_t>(op->dispatch_type), op->task_data.aicore.kernel.block_dim, ctx.stream, &l0_info, ctx.model_id, ctx.instance_handle));
      OM2_CHK_STATUS(KernelTaskDistribute(ordered_io_addrs, args_info, ctx.func_handles[op->task_data.aicore.kernel.func_idx], op->task_data.aicore.kernel.block_dim, ctx.stream, &cfg_holder.cfg));
      OM2_CHK_STATUS(ReportLaunchedOm2Task(op->op_name, op->task_data.aicore.op_type, 0U, reinterpret_cast<uintptr_t>(args_info->dev_addr), args_info->size, report_inputs.data(), static_cast<uint64_t>(report_inputs.size()), report_outputs.data(), static_cast<uint32_t>(report_outputs.size()), report_workspace_addrs.data(), report_workspace_sizes.data(), static_cast<uint32_t>(report_workspace_sizes.size()), static_cast<uint32_t>(op->dispatch_type), op->task_data.aicore.kernel.block_dim, ctx.stream, ctx.model_id, ctx.instance_handle));
    }
    break;
    default:
    return ACL_ERROR_FAILURE;
  }
  return ACL_SUCCESS;
}
const OpDef kOpDefs[] = {{
  .dispatch_type = static_cast<OpDispatchType>(0),
  .op_name = "add1",
  .argsInfo = (const OpArgInfo[]){
    {.type = 0, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 0}}},
    {.type = 0, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 8}}},
    {.type = 1, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 16}}},
    {.type = 2, .addr = {.mem_src = 0, .offset = 0}, .data = {.tensor = {.size = 64}}},
  },
  .task_data = {
    .aicore = {
      .op_type = "Add",
      .num_io_addrs = 4,
      .args_idx = 0,
      .launch = {.schedule_mode = 0, .engine_type = 0, .block_dim_offset = 0, .is_block_task_prefetch = false, .time_out = 0, .local_memory_size = 0},
      .kernel = {.block_dim = 8, .func_idx = 0},
      .l0 = {.need_assert_or_printf = 0, .num_l0_slots = 4, .l0_slots = (const Om2L0ArgSlotInfo[]){{OM2_L0_ARG_INPUT, 0U, 0U, 0UL, 0U, 0U, 0U}, {OM2_L0_ARG_INPUT, 0U, 8U, 0UL, 1U, 0U, 0U}, {OM2_L0_ARG_OUTPUT, 0U, 16U, 0UL, 2U, 0U, 0U}, {OM2_L0_ARG_WORKSPACE, 0U, 24U, 0UL, 0U, 0U, 0U}}},
    },
  },
}};
} // namespace
aclmdlRI Om2Model::GetRtModelHandle() {
  return model_handle_;
}

aclError Om2Model::Load() {
  OM2_LOGI("Load begin");
  dev_ext_info_mem_ptrs_.resize(0);
  OM2_CHK_STATUS(DispatchOp(&kOpDefs[0], {total_dev_mem_ptr_, session_scope_mem_ptr_, constants_, args_table_, func_handles_.data(), stream_list_[0], model_id_, instance_handle_, mem_event_id_mem_map_, dev_dynamic_mem_ptrs_, overflow_addr_}));
  OM2_CHK_STATUS(aclmdlRIBuildEnd(model_handle_, nullptr));
  OM2_LOGI("Load done");
  return ACL_SUCCESS;
}

aclError Om2Model::RunAsync(aclrtStream &exe_stream, size_t input_count, void **input_data, size_t output_count, void **output_data) {
  OM2_LOGI("RunAsync begin");
  if (((input_count != om2::INPUT_NUM) || (output_count != om2::OUTPUT_NUM))) {
    return ACL_ERROR_FAILURE;
  }
  auto input_data_0_tensor = reinterpret_cast<gert::Tensor *>(input_data[0]);
  auto input_data_1_tensor = reinterpret_cast<gert::Tensor *>(input_data[1]);
  auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0]);
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(0, reinterpret_cast<uintptr_t>(input_data_0_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(1, reinterpret_cast<uintptr_t>(input_data_1_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr())));

  OM2_CHK_STATUS(args_table_.CopyArgsToDevice());
  OM2_CHK_STATUS(aclmdlRIExecuteAsync(model_handle_, exe_stream));

  OM2_LOGI("RunAsync done");
  return ACL_SUCCESS;
}

aclError Om2Model::Run(size_t input_count, void **input_data, size_t output_count, void **output_data, int32_t stream_sync_timeout) {
  OM2_LOGI("Run begin");
  if (((input_count != om2::INPUT_NUM) || (output_count != om2::OUTPUT_NUM))) {
    return ACL_ERROR_FAILURE;
  }
  auto input_data_0_tensor = reinterpret_cast<gert::Tensor *>(input_data[0]);
  auto input_data_1_tensor = reinterpret_cast<gert::Tensor *>(input_data[1]);
  auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0]);
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(0, reinterpret_cast<uintptr_t>(input_data_0_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(1, reinterpret_cast<uintptr_t>(input_data_1_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr())));

  OM2_CHK_STATUS(args_table_.CopyArgsToDevice());
  OM2_CHK_STATUS(aclmdlRIExecute(model_handle_, stream_sync_timeout));

  OM2_LOGI("Run done");
  return ACL_SUCCESS;
}
} // namespace om2
aclError Om2ModelCreate(om2::Om2ModelHandle *model_handle, aclmdlRI *rt_model_handle, const char **bin_files, const void **bin_data, size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle) {
  OM2_LOGI("Om2ModelCreate");
  if ((model_handle == nullptr) || (rt_model_handle == nullptr) || (*model_handle != nullptr)) {
    OM2_LOGE("Om2ModelCreate: invalid handle");
    return ACL_ERROR_FAILURE;
  }
  auto *obj = new om2::Om2Model(bin_files, bin_data, bin_size, bin_num, constants, work_ptr, session_id,
                                model_id, instance_handle);
  if (obj == nullptr) {
    OM2_LOGE("Om2ModelCreate: new Om2Model failed");
    return ACL_ERROR_FAILURE;
  }
  auto ret = obj->InitResources();
  if (ret != ACL_SUCCESS) {
    OM2_LOGE("Om2ModelCreate: InitResources failed, ret: %d", ret);
    delete obj;
    return ret;
  }
  ret = obj->RegisterKernels();
  if (ret != ACL_SUCCESS) {
    OM2_LOGE("Om2ModelCreate: RegisterKernels failed, ret: %d", ret);
    delete obj;
    return ret;
  }
  *model_handle = reinterpret_cast<om2::Om2ModelHandle>(obj);
  *rt_model_handle = obj->GetRtModelHandle();
  OM2_LOGI("Om2ModelCreate done");
  return ACL_SUCCESS;
}

aclError Om2ModelLoad(om2::Om2ModelHandle *model_handle) {
  OM2_LOGI("Om2ModelLoad");
  if ((model_handle == nullptr) || (*model_handle == nullptr)) {
    OM2_LOGE("Om2ModelLoad: invalid handle");
    return ACL_ERROR_FAILURE;
  }
  return static_cast<om2::Om2Model*>(*model_handle)->Load();
}

aclError Om2ModelRunAsync(om2::Om2ModelHandle *model_handle, aclrtStream stream, int input_count, void **input_data, int output_count, void **output_data) {
  OM2_LOGI("Om2ModelRunAsync");
  return static_cast<om2::Om2Model*>(*model_handle)->RunAsync(stream, input_count, input_data, output_count, output_data);
}

aclError Om2ModelRun(om2::Om2ModelHandle *model_handle, int input_count, void **input_data, int output_count, void **output_data, int32_t stream_sync_timeout) {
  OM2_LOGI("Om2ModelRun");
  return static_cast<om2::Om2Model*>(*model_handle)->Run(input_count, input_data, output_count, output_data, stream_sync_timeout);
}

aclError Om2ModelDestroy(om2::Om2ModelHandle *model_handle) {
  OM2_LOGI("Om2ModelDestroy");
  delete static_cast<om2::Om2Model*>(*model_handle);
  return ACL_SUCCESS;
})";
  ASSERT_EQ(outputs[GeneratedFileIndex::kLoadingAndRunningFile], expected + "\n");
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSource_ConstInputTensor_Ok) {
  auto const_ge_root_model = CreateGeRootModelWithConstInputOp();
  auto generator = CreateProgramGenerator(const_ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &source = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  EXPECT_NE(source.find("kOpDefs"), std::string::npos);
  EXPECT_NE(source.find("DispatchOp"), std::string::npos);
  EXPECT_NE(source.find("BuildOm2Tensor"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSource_AicpuEmptyShape_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicpuEmptyShape();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  // Verify all 6 files are generated
  for (const auto file_index : {GeneratedFileIndex::kKernelRegistryFile, GeneratedFileIndex::kResourcesFile,
                                GeneratedFileIndex::kArgsManagerFile, GeneratedFileIndex::kLoadingAndRunningFile,
                                GeneratedFileIndex::kInterfaceHeaderFile, GeneratedFileIndex::kCMakeListsFile}) {
    EXPECT_NE(outputs.find(file_index), outputs.end());
    EXPECT_FALSE(outputs[file_index].empty());
  }
  // RenderDistribution 路径应在 Load() 中生成 BuildOm2Tensor 和 FlattenHostArgs/AicpuKernelTaskDistribute
  const auto &source = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  EXPECT_NE(source.find("BuildOm2Tensor"), std::string::npos);
  EXPECT_NE(source.find("FlattenHostArgs"), std::string::npos);
  EXPECT_NE(source.find("AicpuKernelTaskDistribute"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForAicpu_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicpuOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const std::string expected = R"(#line 1 "g1_load_and_run.cpp"
#include "g1_interface.h"

namespace om2 {
namespace {
Om2Tensor BuildOm2Tensor(void *device_address, uint64_t size, int32_t data_type,
                         int32_t format, const int64_t *shape_dims, uint64_t shape_dims_num) {
  Om2Tensor tensor{};
  tensor.device_address = PtrToU64(device_address);
  tensor.size = size;
  tensor.data_type = data_type;
  tensor.format = format;
  tensor.shape_dims = shape_dims;
  tensor.shape_dims_num = shape_dims_num;
  return tensor;
}

aclError AssembleOm2TaskInfo(Om2TaskInfo *task_info, const char *op_name, const char *op_type,
                             uint32_t task_id, uint32_t stream_id, uint32_t block_dim,
                             uint64_t op_desc_id, uintptr_t args_base, uint64_t args_size,
                             const Om2TaskIoEntry *inputs, uint64_t input_num,
                             const Om2TaskIoEntry *outputs, uint32_t output_num,
                             const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                             uint32_t workspace_num, uint32_t task_type, void *stream,
                             uint32_t is_raw_address = 0U) {
  task_info->op_name = op_name;
  task_info->op_type = op_type;
  task_info->task_id = task_id;
  task_info->stream_id = stream_id;
  task_info->context_id = 0U;
  task_info->thread_id = 0U;
  task_info->block_dim = block_dim;
  task_info->op_desc_id = op_desc_id;
  task_info->args_base = args_base;
  task_info->args_size = args_size;
  task_info->input_num = input_num;
  task_info->inputs = inputs;
  task_info->output_num = output_num;
  task_info->outputs = outputs;
  task_info->workspace_num = workspace_num;
  task_info->workspace_addrs = workspace_addrs;
  task_info->workspace_sizes = workspace_sizes;
  task_info->task_type = task_type;
  task_info->stream = stream;
  task_info->is_raw_address = is_raw_address;
  task_info->l0_exception_dump_info = nullptr;
  return ACL_SUCCESS;
}

aclError ReportOm2TaskPreprocess(const char *op_name, const char *op_type, uint64_t op_desc_id,
                                uintptr_t args_base, uint64_t args_size,
                                const std::vector<Om2TaskIoEntry> &inputs,
                                const std::vector<Om2TaskIoEntry> &outputs,
                                const std::vector<uint64_t> &workspace_addrs,
                                const std::vector<uint64_t> &workspace_sizes,
                                uint32_t task_type, uint32_t block_dim, void *stream,
                                const Om2L0TaskRawInfo *l0_info,
                                uint32_t model_id, void *instance_handle,
                                uint32_t is_raw_address = 0U) {
  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  OM2_CHK_TRUE(workspace_addrs.size() == workspace_sizes.size());

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, 0U, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs.data(), static_cast<uint64_t>(inputs.size()),
                                      outputs.data(), static_cast<uint32_t>(outputs.size()),
                                      workspace_addrs.empty() ? nullptr : workspace_addrs.data(),
                                      workspace_sizes.empty() ? nullptr : workspace_sizes.data(),
                                      static_cast<uint32_t>(workspace_addrs.size()),
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = l0_info;
  if (ReportDfxTaskPreprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPreprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

aclError ReportLaunchedOm2Task(const char *op_name, const char *op_type, uint64_t op_desc_id,
                               uintptr_t args_base, uint64_t args_size,
                               const Om2TaskIoEntry *inputs, uint64_t input_num,
                               const Om2TaskIoEntry *outputs, uint32_t output_num,
                               const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                               uint32_t workspace_num,
                               uint32_t task_type, uint32_t block_dim, void *stream,
                               uint32_t model_id, void *instance_handle,
                               uint32_t is_raw_address = 0U) {
  uint32_t task_id = 0U;
  OM2_CHK_RT(aclrtGetThreadLastTaskId(&task_id));

  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, task_id, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs, input_num,
                                      outputs, output_num,
                                      workspace_addrs, workspace_sizes, workspace_num,
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = nullptr;
  if (ReportDfxTaskPostprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPostprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

uint8_t GetIsDataDump(const char *op_name, uint32_t model_id, void *instance_handle) {
  if (IsDataDumpEnabled != nullptr && instance_handle != nullptr) {
    uint8_t is_data_dump = 0U;
    auto ret = IsDataDumpEnabled(model_id, instance_handle, op_name, &is_data_dump);
    return ret == 0 ? is_data_dump : 0U;
  }
  return 0U;
}

aclError AclrtMalloc(void **ptr, size_t size, uint32_t mem_type, uint16_t module_id) {
  *ptr = nullptr;
  if ((size == 0U)) {
    return ACL_SUCCESS;
  }
  aclrtMallocAttribute attr;
  attr.attr = ACL_RT_MEM_ATTR_MODULE_ID;
  attr.value.moduleId = module_id;
  aclrtMallocConfig cfg;
  cfg.attrs = &attr;
  cfg.numAttrs = 1U;
  switch (mem_type) {
    case RT_MEMORY_TS:
    return aclrtMallocForTaskScheduler(ptr, size, ACL_MEM_MALLOC_HUGE_FIRST, &cfg);
    case RT_MEMORY_HOST:
    return aclrtMallocHostWithCfg(ptr, size, &cfg);
    case RT_MEMORY_P2P_HBM:
    case RT_MEMORY_P2P_DDR:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_MALLOC_HUGE_FIRST_P2P, &cfg);
    case RT_MEMORY_DDR:
    case RT_MEMORY_DDR_NC:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_TYPE_LOW_BAND_WIDTH, &cfg);
    default:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_TYPE_HIGH_BAND_WIDTH, &cfg);
  }
}
constexpr uint16_t GE_MODULE_NAME_U16 = 45;
aclError MallocDeviceMemory(void *&dev_ptr, const size_t size, const uint32_t mem_type, std::vector<void *> &mem_ptrs) {
  uint64_t block_size = 2097152;
  if ((mem_type == RT_MEMORY_TS)) {
    block_size = 4096;
  }
  const auto aligned_size = ((((size + 512) - 1) / 512) * 512);
  const auto final_block_size = ((((aligned_size + block_size) - 1) / block_size) * block_size);
  const auto rt_ret = AclrtMalloc(&dev_ptr, final_block_size, mem_type, GE_MODULE_NAME_U16);
  if (((rt_ret != ACL_SUCCESS) || (dev_ptr == nullptr))) {
    return ACL_ERROR_FAILURE;
  }
  OM2_CHK_STATUS(aclrtMemset(dev_ptr, block_size, 0, block_size));
  mem_ptrs.push_back(dev_ptr);
  return ACL_SUCCESS;
}
constexpr const size_t max_launch_cfg_num = 8UL;
struct LaunchKernelCfgHolder {
  aclrtLaunchKernelCfg cfg{};
  aclrtLaunchKernelAttr attrs[max_launch_cfg_num];
};

struct LaunchKernelConfig {
  uint8_t schedule_mode{0U};
  aclrtEngineType engine_type{ACL_RT_ENGINE_TYPE_AIC};
  uint32_t block_dim_offset{0U};
  bool is_block_task_prefetch{false};
  uint8_t is_data_dump{0U};
  uint16_t time_out{0U};
  uint32_t local_memory_size{0U};
};

aclError AssembleLaunchConfig(LaunchKernelCfgHolder &holder, const LaunchKernelConfig &launch_config) {
  size_t actual_cfg_num = 0UL;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
  holder.attrs[actual_cfg_num].value.schemMode = launch_config.schedule_mode;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
  holder.attrs[actual_cfg_num].value.engineType = launch_config.engine_type;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
  holder.attrs[actual_cfg_num].value.blockDimOffset = launch_config.block_dim_offset;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCK_TASK_PREFETCH;
  holder.attrs[actual_cfg_num].value.isBlockTaskPrefetch = static_cast<uint8_t>(launch_config.is_block_task_prefetch);
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DATA_DUMP;
  holder.attrs[actual_cfg_num].value.isDataDump = launch_config.is_data_dump;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
  holder.attrs[actual_cfg_num].value.dynUBufSize = launch_config.local_memory_size;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
  holder.attrs[actual_cfg_num].value.timeout = launch_config.time_out;
  actual_cfg_num++;
  holder.cfg.attrs = &holder.attrs[0];
  holder.cfg.numAttrs = actual_cfg_num;
  return ACL_SUCCESS;
}
constexpr int64_t kDImEndFlag = std::numeric_limits<int64_t>::min();
aclError KernelTaskDistribute(const std::vector<uint64_t> &io_addrs, ArgsInfo *args_info, aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config) {
  OM2_CHK_NOTNULL(args_info);
  OM2_CHK_STATUS(memcpy_s(args_info->host_addr, args_info->size, io_addrs.data(), (io_addrs.size() * sizeof(uint64_t))));
  OM2_CHK_STATUS(aclrtLaunchKernelV2(func_handle, block_dim, args_info->dev_addr, args_info->size, config, stream));
  return ACL_SUCCESS;
}
constexpr uint32_t kAicpuArgsExtInfoAddrOffset = 12U;
constexpr uint32_t kAicpuArgsio_addr_offset = 20U;
aclError UpdateExtInfoSession(uint8_t *extInfo, size_t session_info_offset, uint64_t *session_id, uint64_t *kernel_id) {
  AicpuSessionInfo *session_info = reinterpret_cast<AicpuSessionInfo *>(&extInfo[session_info_offset]);
  session_info->sessionId = *session_id;
  session_info->kernelId = *kernel_id;
  session_info->sessFlag = true;
  *kernel_id++;
  return ACL_SUCCESS;
}

aclError AssembleAicpuExtInfo(uint8_t *ext_info, size_t ext_info_len, int32_t session_info_offset, uint64_t *session_id, uint64_t *kernel_id, std::vector<void *> &dev_ext_info_mem_ptrs, size_t index) {
  std::unique_ptr<uint8_t[]> tmp_ext_info = std::make_unique<uint8_t[]>(ext_info_len);
  memcpy_s(tmp_ext_info.get(), ext_info_len, ext_info, ext_info_len);
  if ((session_info_offset != -1)) {
    OM2_CHK_STATUS(UpdateExtInfoSession(tmp_ext_info.get(), session_info_offset, session_id, kernel_id));
  }
  void *dev_ptr = nullptr;
  OM2_CHK_STATUS(aclrtMallocAlign32(&dev_ptr, ext_info_len, ACL_MEM_MALLOC_HUGE_FIRST));
  OM2_CHK_STATUS(aclrtMemcpy(dev_ptr, ext_info_len, tmp_ext_info.get(), ext_info_len, ACL_MEMCPY_HOST_TO_DEVICE));
  dev_ext_info_mem_ptrs[index] = dev_ptr;
  return ACL_SUCCESS;
}

aclError AssembleAicpuArgs(uint8_t *args, size_t args_len, void *ext_info_addr, size_t ext_info_len, std::vector<uint64_t> &io_addr, void *target_args_ptr) {
  std::unique_ptr<uint8_t[]> tmp_args = std::make_unique<uint8_t[]>(args_len);
  memcpy_s(tmp_args.get(), args_len, args, args_len);
  const auto aicpu_param_head = reinterpret_cast<AicpuParamHead *>(tmp_args.get());
  aicpu_param_head->extInfoLength = static_cast<uint32_t>(ext_info_len);
  uint64_t ext_info_addr_value = reinterpret_cast<uint64_t>(ext_info_addr);
  memcpy_s((tmp_args.get() + kAicpuArgsExtInfoAddrOffset), sizeof(uint64_t), &ext_info_addr_value, sizeof(uint64_t));
  size_t addrs_size = (sizeof(uint64_t) * io_addr.size());
  memcpy_s((tmp_args.get() + kAicpuArgsio_addr_offset), addrs_size, io_addr.data(), addrs_size);
  memcpy_s(target_args_ptr, args_len, tmp_args.get(), args_len);
  return ACL_SUCCESS;
}

aclError AicpuKernelTaskDistribute(const std::vector<uint8_t> &args, ArgsInfo *args_info, aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config) {
  OM2_CHK_NOTNULL(args_info);
  OM2_CHK_STATUS(memcpy_s(args_info->host_addr, args_info->size, args.data(), args.size()));
  OM2_CHK_STATUS(aclrtLaunchKernelV2(func_handle, block_dim, args_info->dev_addr, args_info->size, config, stream));
  return ACL_SUCCESS;
}

aclError GetEventIdAddr(void *&event_addr, std::map<uint32_t, void *> &event_id_mem_map, uint32_t event_id, std::vector<void *> &mem_ptrs) {
  auto it = event_id_mem_map.find(event_id);
  if ((event_id_mem_map.end() != it)) {
    event_addr = it->second;
    return ACL_SUCCESS;
  }
  OM2_CHK_STATUS(MallocDeviceMemory(event_addr, 8, 2, mem_ptrs));
  OM2_CHK_STATUS(aclrtMemset(event_addr, 8, 0, 8));
  event_id_mem_map[event_id] = event_addr;
  return ACL_SUCCESS;
}

aclError DispatchOp(const OpDef *op, const DispatchOpContext &ctx) {
  switch (static_cast<OpDispatchType>(op->dispatch_type)) {
    default:
    return ACL_ERROR_FAILURE;
  }
  return ACL_SUCCESS;
}
const OpDef kOpDefs[] = {};
} // namespace
aclmdlRI Om2Model::GetRtModelHandle() {
  return model_handle_;
}

aclError Om2Model::Load() {
  OM2_LOGI("Load begin");
  dev_ext_info_mem_ptrs_.resize(2);
  {
    // ============================= add1 ===============================
    static constexpr int64_t op2_input0_shape[] = {-1, -1, -1, -1};
    Om2Tensor op2_input0 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024), 0UL, 1, 2, op2_input0_shape, 4UL);
    static constexpr int64_t op2_input1_shape[] = {-1, -1, -1, -1};
    Om2Tensor op2_input1 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024), 0UL, 1, 2, op2_input1_shape, 4UL);
    static constexpr int64_t op2_output0_shape[] = {-1, -1, -1, -1};
    Om2Tensor op2_output0 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024), 0UL, 1, 2, op2_output0_shape, 4UL);
    std::vector<uint64_t> op2_iow_addr = FlattenHostArgs(op2_input0, op2_input1, op2_output0);
    const char *op2_args_str = "\104\000\000\000\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000";
    const char *op2_ext_info_str = "\001\000\000\000\210\000\000\000\000\000\000\000\005\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\005\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000\000\000\104\000\000\000\000\000\000\000\005\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\005\000\000\000\021\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\006\000\000\000\010\000\000\000\001\000\000\000\000\000\000\000\003\000\000\000\004\000\000\000\001\000\000\000\007\000\000\000\004\000\000\000\001\000\000\000";
    std::vector<uint8_t> op2_args;
    op2_args.resize(68);
    AssembleAicpuExtInfo(reinterpret_cast<uint8_t *>(const_cast<char *>(op2_ext_info_str)), 285, 228, session_id_, &kernel_id_, dev_ext_info_mem_ptrs_, 0);
    AssembleAicpuArgs(reinterpret_cast<uint8_t *>(const_cast<char *>(op2_args_str)), 68, dev_ext_info_mem_ptrs_[0], 285, op2_iow_addr, op2_args.data());
    LaunchKernelCfgHolder op2_cfg_holder;
    AssembleLaunchConfig(op2_cfg_holder, {0U, ACL_RT_ENGINE_TYPE_AIC, 0U, false, GetIsDataDump("add1", model_id_, instance_handle_), 0U, 0U});
    OM2_CHK_STATUS(AicpuKernelTaskDistribute(op2_args, args_table_.GetArgsInfo(0), func_handles_[0], 8, stream_list_[0], &op2_cfg_holder.cfg));
    const Om2TaskIoEntry op2_report_inputs[] = {{&op2_input0, 0U}, {&op2_input1, 0U}};
    const Om2TaskIoEntry op2_report_outputs[] = {{&op2_output0, 0U}};
    OM2_CHK_STATUS(ReportLaunchedOm2Task("add1", "Add", 2U, reinterpret_cast<uintptr_t>(args_table_.GetArgsInfo(0)->dev_addr), args_table_.GetArgsInfo(0)->size, op2_report_inputs, 2UL, op2_report_outputs, 1U, nullptr, nullptr, 0U, 0U, 8U, stream_list_[0], model_id_, instance_handle_));

  }
  {
    // ============================= add2 ===============================
    static constexpr int64_t op3_input0_shape[] = {-1, -1, -1, -1};
    Om2Tensor op3_input0 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024), 0UL, 1, 2, op3_input0_shape, 4UL);
    static constexpr int64_t op3_input1_shape[] = {-1, -1, -1, -1};
    Om2Tensor op3_input1 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024), 0UL, 1, 2, op3_input1_shape, 4UL);
    static constexpr int64_t op3_output0_shape[] = {-1, -1, -1, -1};
    Om2Tensor op3_output0 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024), 0UL, 1, 2, op3_output0_shape, 4UL);
    std::vector<uint64_t> op3_iow_addr = FlattenHostArgs(op3_input0, op3_input1, op3_output0);
    const char *op3_args_str = "\104\000\000\000\003\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000";
    const char *op3_ext_info_str = "\001\000\000\000\210\000\000\000\000\000\000\000\005\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\005\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\002\000\000\000\104\000\000\000\000\000\000\000\005\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\005\000\000\000\021\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\006\000\000\000\010\000\000\000\001\000\000\000\000\000\000\000\003\000\000\000\004\000\000\000\001\000\000\000\007\000\000\000\004\000\000\000\001\000\000\000";
    std::vector<uint8_t> op3_args;
    op3_args.resize(68);
    AssembleAicpuExtInfo(reinterpret_cast<uint8_t *>(const_cast<char *>(op3_ext_info_str)), 285, 228, session_id_, &kernel_id_, dev_ext_info_mem_ptrs_, 1);
    AssembleAicpuArgs(reinterpret_cast<uint8_t *>(const_cast<char *>(op3_args_str)), 68, dev_ext_info_mem_ptrs_[1], 285, op3_iow_addr, op3_args.data());
    LaunchKernelCfgHolder op3_cfg_holder;
    AssembleLaunchConfig(op3_cfg_holder, {0U, ACL_RT_ENGINE_TYPE_AIC, 0U, false, GetIsDataDump("add2", model_id_, instance_handle_), 0U, 0U});
    OM2_CHK_STATUS(AicpuKernelTaskDistribute(op3_args, args_table_.GetArgsInfo(1), func_handles_[0], 8, stream_list_[0], &op3_cfg_holder.cfg));
    const Om2TaskIoEntry op3_report_inputs[] = {{&op3_input0, 0U}, {&op3_input1, 0U}};
    const Om2TaskIoEntry op3_report_outputs[] = {{&op3_output0, 0U}};
    OM2_CHK_STATUS(ReportLaunchedOm2Task("add2", "Add", 3U, reinterpret_cast<uintptr_t>(args_table_.GetArgsInfo(1)->dev_addr), args_table_.GetArgsInfo(1)->size, op3_report_inputs, 2UL, op3_report_outputs, 1U, nullptr, nullptr, 0U, 0U, 8U, stream_list_[0], model_id_, instance_handle_));

  }
  OM2_CHK_STATUS(aclmdlRIBuildEnd(model_handle_, nullptr));
  OM2_LOGI("Load done");
  return ACL_SUCCESS;
}

aclError Om2Model::RunAsync(aclrtStream &exe_stream, size_t input_count, void **input_data, size_t output_count, void **output_data) {
  OM2_LOGI("RunAsync begin");
  if (((input_count != om2::INPUT_NUM) || (output_count != om2::OUTPUT_NUM))) {
    return ACL_ERROR_FAILURE;
  }
  auto input_data_0_tensor = reinterpret_cast<gert::Tensor *>(input_data[0]);
  auto input_data_1_tensor = reinterpret_cast<gert::Tensor *>(input_data[1]);
  auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0]);
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(0, reinterpret_cast<uintptr_t>(input_data_0_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(1, reinterpret_cast<uintptr_t>(input_data_1_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr())));

  OM2_CHK_STATUS(args_table_.CopyArgsToDevice());
  OM2_CHK_STATUS(aclmdlRIExecuteAsync(model_handle_, exe_stream));

  OM2_LOGI("RunAsync done");
  return ACL_SUCCESS;
}

aclError Om2Model::Run(size_t input_count, void **input_data, size_t output_count, void **output_data, int32_t stream_sync_timeout) {
  OM2_LOGI("Run begin");
  if (((input_count != om2::INPUT_NUM) || (output_count != om2::OUTPUT_NUM))) {
    return ACL_ERROR_FAILURE;
  }
  auto input_data_0_tensor = reinterpret_cast<gert::Tensor *>(input_data[0]);
  auto input_data_1_tensor = reinterpret_cast<gert::Tensor *>(input_data[1]);
  auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0]);
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(0, reinterpret_cast<uintptr_t>(input_data_0_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(1, reinterpret_cast<uintptr_t>(input_data_1_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr())));

  OM2_CHK_STATUS(args_table_.CopyArgsToDevice());
  OM2_CHK_STATUS(aclmdlRIExecute(model_handle_, stream_sync_timeout));

  OM2_LOGI("Run done");
  return ACL_SUCCESS;
}
} // namespace om2
aclError Om2ModelCreate(om2::Om2ModelHandle *model_handle, aclmdlRI *rt_model_handle, const char **bin_files, const void **bin_data, size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle) {
  OM2_LOGI("Om2ModelCreate");
  if ((model_handle == nullptr) || (rt_model_handle == nullptr) || (*model_handle != nullptr)) {
    OM2_LOGE("Om2ModelCreate: invalid handle");
    return ACL_ERROR_FAILURE;
  }
  auto *obj = new om2::Om2Model(bin_files, bin_data, bin_size, bin_num, constants, work_ptr, session_id,
                                model_id, instance_handle);
  if (obj == nullptr) {
    OM2_LOGE("Om2ModelCreate: new Om2Model failed");
    return ACL_ERROR_FAILURE;
  }
  auto ret = obj->InitResources();
  if (ret != ACL_SUCCESS) {
    OM2_LOGE("Om2ModelCreate: InitResources failed, ret: %d", ret);
    delete obj;
    return ret;
  }
  ret = obj->RegisterKernels();
  if (ret != ACL_SUCCESS) {
    OM2_LOGE("Om2ModelCreate: RegisterKernels failed, ret: %d", ret);
    delete obj;
    return ret;
  }
  *model_handle = reinterpret_cast<om2::Om2ModelHandle>(obj);
  *rt_model_handle = obj->GetRtModelHandle();
  OM2_LOGI("Om2ModelCreate done");
  return ACL_SUCCESS;
}

aclError Om2ModelLoad(om2::Om2ModelHandle *model_handle) {
  OM2_LOGI("Om2ModelLoad");
  if ((model_handle == nullptr) || (*model_handle == nullptr)) {
    OM2_LOGE("Om2ModelLoad: invalid handle");
    return ACL_ERROR_FAILURE;
  }
  return static_cast<om2::Om2Model*>(*model_handle)->Load();
}

aclError Om2ModelRunAsync(om2::Om2ModelHandle *model_handle, aclrtStream stream, int input_count, void **input_data, int output_count, void **output_data) {
  OM2_LOGI("Om2ModelRunAsync");
  return static_cast<om2::Om2Model*>(*model_handle)->RunAsync(stream, input_count, input_data, output_count, output_data);
}

aclError Om2ModelRun(om2::Om2ModelHandle *model_handle, int input_count, void **input_data, int output_count, void **output_data, int32_t stream_sync_timeout) {
  OM2_LOGI("Om2ModelRun");
  return static_cast<om2::Om2Model*>(*model_handle)->Run(input_count, input_data, output_count, output_data, stream_sync_timeout);
}

aclError Om2ModelDestroy(om2::Om2ModelHandle *model_handle) {
  OM2_LOGI("Om2ModelDestroy");
  delete static_cast<om2::Om2Model*>(*model_handle);
  return ACL_SUCCESS;
})";
  ASSERT_EQ(outputs[GeneratedFileIndex::kLoadingAndRunningFile], expected + "\n");
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForDynamicIo_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOpOfDynamicIo();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const std::string expected = R"(#line 1 "g1_load_and_run.cpp"
#include "g1_interface.h"

namespace om2 {
namespace {
Om2Tensor BuildOm2Tensor(void *device_address, uint64_t size, int32_t data_type,
                         int32_t format, const int64_t *shape_dims, uint64_t shape_dims_num) {
  Om2Tensor tensor{};
  tensor.device_address = PtrToU64(device_address);
  tensor.size = size;
  tensor.data_type = data_type;
  tensor.format = format;
  tensor.shape_dims = shape_dims;
  tensor.shape_dims_num = shape_dims_num;
  return tensor;
}

aclError AssembleOm2TaskInfo(Om2TaskInfo *task_info, const char *op_name, const char *op_type,
                             uint32_t task_id, uint32_t stream_id, uint32_t block_dim,
                             uint64_t op_desc_id, uintptr_t args_base, uint64_t args_size,
                             const Om2TaskIoEntry *inputs, uint64_t input_num,
                             const Om2TaskIoEntry *outputs, uint32_t output_num,
                             const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                             uint32_t workspace_num, uint32_t task_type, void *stream,
                             uint32_t is_raw_address = 0U) {
  task_info->op_name = op_name;
  task_info->op_type = op_type;
  task_info->task_id = task_id;
  task_info->stream_id = stream_id;
  task_info->context_id = 0U;
  task_info->thread_id = 0U;
  task_info->block_dim = block_dim;
  task_info->op_desc_id = op_desc_id;
  task_info->args_base = args_base;
  task_info->args_size = args_size;
  task_info->input_num = input_num;
  task_info->inputs = inputs;
  task_info->output_num = output_num;
  task_info->outputs = outputs;
  task_info->workspace_num = workspace_num;
  task_info->workspace_addrs = workspace_addrs;
  task_info->workspace_sizes = workspace_sizes;
  task_info->task_type = task_type;
  task_info->stream = stream;
  task_info->is_raw_address = is_raw_address;
  task_info->l0_exception_dump_info = nullptr;
  return ACL_SUCCESS;
}

aclError ReportOm2TaskPreprocess(const char *op_name, const char *op_type, uint64_t op_desc_id,
                                uintptr_t args_base, uint64_t args_size,
                                const std::vector<Om2TaskIoEntry> &inputs,
                                const std::vector<Om2TaskIoEntry> &outputs,
                                const std::vector<uint64_t> &workspace_addrs,
                                const std::vector<uint64_t> &workspace_sizes,
                                uint32_t task_type, uint32_t block_dim, void *stream,
                                const Om2L0TaskRawInfo *l0_info,
                                uint32_t model_id, void *instance_handle,
                                uint32_t is_raw_address = 0U) {
  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  OM2_CHK_TRUE(workspace_addrs.size() == workspace_sizes.size());

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, 0U, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs.data(), static_cast<uint64_t>(inputs.size()),
                                      outputs.data(), static_cast<uint32_t>(outputs.size()),
                                      workspace_addrs.empty() ? nullptr : workspace_addrs.data(),
                                      workspace_sizes.empty() ? nullptr : workspace_sizes.data(),
                                      static_cast<uint32_t>(workspace_addrs.size()),
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = l0_info;
  if (ReportDfxTaskPreprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPreprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

aclError ReportLaunchedOm2Task(const char *op_name, const char *op_type, uint64_t op_desc_id,
                               uintptr_t args_base, uint64_t args_size,
                               const Om2TaskIoEntry *inputs, uint64_t input_num,
                               const Om2TaskIoEntry *outputs, uint32_t output_num,
                               const uint64_t *workspace_addrs, const uint64_t *workspace_sizes,
                               uint32_t workspace_num,
                               uint32_t task_type, uint32_t block_dim, void *stream,
                               uint32_t model_id, void *instance_handle,
                               uint32_t is_raw_address = 0U) {
  uint32_t task_id = 0U;
  OM2_CHK_RT(aclrtGetThreadLastTaskId(&task_id));

  uint32_t stream_id = 0U;
  OM2_CHK_STATUS(aclrtStreamGetId(stream, reinterpret_cast<int32_t *>(&stream_id)));

  Om2TaskInfo task_info{};
  OM2_CHK_STATUS(AssembleOm2TaskInfo(&task_info, op_name, op_type, task_id, stream_id, block_dim,
                                      op_desc_id, args_base, args_size,
                                      inputs, input_num,
                                      outputs, output_num,
                                      workspace_addrs, workspace_sizes, workspace_num,
                                      task_type, stream, is_raw_address));
  task_info.l0_exception_dump_info = nullptr;
  if (ReportDfxTaskPostprocess != nullptr && instance_handle != nullptr) {
    OM2_CHK_STATUS(ReportDfxTaskPostprocess(model_id, instance_handle, &task_info, nullptr, 0U));
  }
  return ACL_SUCCESS;
}

uint8_t GetIsDataDump(const char *op_name, uint32_t model_id, void *instance_handle) {
  if (IsDataDumpEnabled != nullptr && instance_handle != nullptr) {
    uint8_t is_data_dump = 0U;
    auto ret = IsDataDumpEnabled(model_id, instance_handle, op_name, &is_data_dump);
    return ret == 0 ? is_data_dump : 0U;
  }
  return 0U;
}

aclError AclrtMalloc(void **ptr, size_t size, uint32_t mem_type, uint16_t module_id) {
  *ptr = nullptr;
  if ((size == 0U)) {
    return ACL_SUCCESS;
  }
  aclrtMallocAttribute attr;
  attr.attr = ACL_RT_MEM_ATTR_MODULE_ID;
  attr.value.moduleId = module_id;
  aclrtMallocConfig cfg;
  cfg.attrs = &attr;
  cfg.numAttrs = 1U;
  switch (mem_type) {
    case RT_MEMORY_TS:
    return aclrtMallocForTaskScheduler(ptr, size, ACL_MEM_MALLOC_HUGE_FIRST, &cfg);
    case RT_MEMORY_HOST:
    return aclrtMallocHostWithCfg(ptr, size, &cfg);
    case RT_MEMORY_P2P_HBM:
    case RT_MEMORY_P2P_DDR:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_MALLOC_HUGE_FIRST_P2P, &cfg);
    case RT_MEMORY_DDR:
    case RT_MEMORY_DDR_NC:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_TYPE_LOW_BAND_WIDTH, &cfg);
    default:
    return aclrtMallocWithCfg(ptr, size, ACL_MEM_TYPE_HIGH_BAND_WIDTH, &cfg);
  }
}
constexpr uint16_t GE_MODULE_NAME_U16 = 45;
aclError MallocDeviceMemory(void *&dev_ptr, const size_t size, const uint32_t mem_type, std::vector<void *> &mem_ptrs) {
  uint64_t block_size = 2097152;
  if ((mem_type == RT_MEMORY_TS)) {
    block_size = 4096;
  }
  const auto aligned_size = ((((size + 512) - 1) / 512) * 512);
  const auto final_block_size = ((((aligned_size + block_size) - 1) / block_size) * block_size);
  const auto rt_ret = AclrtMalloc(&dev_ptr, final_block_size, mem_type, GE_MODULE_NAME_U16);
  if (((rt_ret != ACL_SUCCESS) || (dev_ptr == nullptr))) {
    return ACL_ERROR_FAILURE;
  }
  OM2_CHK_STATUS(aclrtMemset(dev_ptr, block_size, 0, block_size));
  mem_ptrs.push_back(dev_ptr);
  return ACL_SUCCESS;
}
constexpr const size_t max_launch_cfg_num = 8UL;
struct LaunchKernelCfgHolder {
  aclrtLaunchKernelCfg cfg{};
  aclrtLaunchKernelAttr attrs[max_launch_cfg_num];
};

struct LaunchKernelConfig {
  uint8_t schedule_mode{0U};
  aclrtEngineType engine_type{ACL_RT_ENGINE_TYPE_AIC};
  uint32_t block_dim_offset{0U};
  bool is_block_task_prefetch{false};
  uint8_t is_data_dump{0U};
  uint16_t time_out{0U};
  uint32_t local_memory_size{0U};
};

aclError AssembleLaunchConfig(LaunchKernelCfgHolder &holder, const LaunchKernelConfig &launch_config) {
  size_t actual_cfg_num = 0UL;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_SCHEM_MODE;
  holder.attrs[actual_cfg_num].value.schemMode = launch_config.schedule_mode;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_ENGINE_TYPE;
  holder.attrs[actual_cfg_num].value.engineType = launch_config.engine_type;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCKDIM_OFFSET;
  holder.attrs[actual_cfg_num].value.blockDimOffset = launch_config.block_dim_offset;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_BLOCK_TASK_PREFETCH;
  holder.attrs[actual_cfg_num].value.isBlockTaskPrefetch = static_cast<uint8_t>(launch_config.is_block_task_prefetch);
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DATA_DUMP;
  holder.attrs[actual_cfg_num].value.isDataDump = launch_config.is_data_dump;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_DYN_UBUF_SIZE;
  holder.attrs[actual_cfg_num].value.dynUBufSize = launch_config.local_memory_size;
  actual_cfg_num++;
  holder.attrs[actual_cfg_num].id = ACL_RT_LAUNCH_KERNEL_ATTR_TIMEOUT;
  holder.attrs[actual_cfg_num].value.timeout = launch_config.time_out;
  actual_cfg_num++;
  holder.cfg.attrs = &holder.attrs[0];
  holder.cfg.numAttrs = actual_cfg_num;
  return ACL_SUCCESS;
}
constexpr int64_t kDImEndFlag = std::numeric_limits<int64_t>::min();
aclError KernelTaskDistribute(const std::vector<uint64_t> &io_addrs, ArgsInfo *args_info, aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config) {
  OM2_CHK_NOTNULL(args_info);
  OM2_CHK_STATUS(memcpy_s(args_info->host_addr, args_info->size, io_addrs.data(), (io_addrs.size() * sizeof(uint64_t))));
  OM2_CHK_STATUS(aclrtLaunchKernelV2(func_handle, block_dim, args_info->dev_addr, args_info->size, config, stream));
  return ACL_SUCCESS;
}
constexpr uint32_t kAicpuArgsExtInfoAddrOffset = 12U;
constexpr uint32_t kAicpuArgsio_addr_offset = 20U;
aclError UpdateExtInfoSession(uint8_t *extInfo, size_t session_info_offset, uint64_t *session_id, uint64_t *kernel_id) {
  AicpuSessionInfo *session_info = reinterpret_cast<AicpuSessionInfo *>(&extInfo[session_info_offset]);
  session_info->sessionId = *session_id;
  session_info->kernelId = *kernel_id;
  session_info->sessFlag = true;
  *kernel_id++;
  return ACL_SUCCESS;
}

aclError AssembleAicpuExtInfo(uint8_t *ext_info, size_t ext_info_len, int32_t session_info_offset, uint64_t *session_id, uint64_t *kernel_id, std::vector<void *> &dev_ext_info_mem_ptrs, size_t index) {
  std::unique_ptr<uint8_t[]> tmp_ext_info = std::make_unique<uint8_t[]>(ext_info_len);
  memcpy_s(tmp_ext_info.get(), ext_info_len, ext_info, ext_info_len);
  if ((session_info_offset != -1)) {
    OM2_CHK_STATUS(UpdateExtInfoSession(tmp_ext_info.get(), session_info_offset, session_id, kernel_id));
  }
  void *dev_ptr = nullptr;
  OM2_CHK_STATUS(aclrtMallocAlign32(&dev_ptr, ext_info_len, ACL_MEM_MALLOC_HUGE_FIRST));
  OM2_CHK_STATUS(aclrtMemcpy(dev_ptr, ext_info_len, tmp_ext_info.get(), ext_info_len, ACL_MEMCPY_HOST_TO_DEVICE));
  dev_ext_info_mem_ptrs[index] = dev_ptr;
  return ACL_SUCCESS;
}

aclError AssembleAicpuArgs(uint8_t *args, size_t args_len, void *ext_info_addr, size_t ext_info_len, std::vector<uint64_t> &io_addr, void *target_args_ptr) {
  std::unique_ptr<uint8_t[]> tmp_args = std::make_unique<uint8_t[]>(args_len);
  memcpy_s(tmp_args.get(), args_len, args, args_len);
  const auto aicpu_param_head = reinterpret_cast<AicpuParamHead *>(tmp_args.get());
  aicpu_param_head->extInfoLength = static_cast<uint32_t>(ext_info_len);
  uint64_t ext_info_addr_value = reinterpret_cast<uint64_t>(ext_info_addr);
  memcpy_s((tmp_args.get() + kAicpuArgsExtInfoAddrOffset), sizeof(uint64_t), &ext_info_addr_value, sizeof(uint64_t));
  size_t addrs_size = (sizeof(uint64_t) * io_addr.size());
  memcpy_s((tmp_args.get() + kAicpuArgsio_addr_offset), addrs_size, io_addr.data(), addrs_size);
  memcpy_s(target_args_ptr, args_len, tmp_args.get(), args_len);
  return ACL_SUCCESS;
}

aclError AicpuKernelTaskDistribute(const std::vector<uint8_t> &args, ArgsInfo *args_info, aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config) {
  OM2_CHK_NOTNULL(args_info);
  OM2_CHK_STATUS(memcpy_s(args_info->host_addr, args_info->size, args.data(), args.size()));
  OM2_CHK_STATUS(aclrtLaunchKernelV2(func_handle, block_dim, args_info->dev_addr, args_info->size, config, stream));
  return ACL_SUCCESS;
}

aclError GetEventIdAddr(void *&event_addr, std::map<uint32_t, void *> &event_id_mem_map, uint32_t event_id, std::vector<void *> &mem_ptrs) {
  auto it = event_id_mem_map.find(event_id);
  if ((event_id_mem_map.end() != it)) {
    event_addr = it->second;
    return ACL_SUCCESS;
  }
  OM2_CHK_STATUS(MallocDeviceMemory(event_addr, 8, 2, mem_ptrs));
  OM2_CHK_STATUS(aclrtMemset(event_addr, 8, 0, 8));
  event_id_mem_map[event_id] = event_addr;
  return ACL_SUCCESS;
}

aclError DispatchOp(const OpDef *op, const DispatchOpContext &ctx) {
  switch (static_cast<OpDispatchType>(op->dispatch_type)) {
    case 0U:
    {
      LaunchKernelCfgHolder cfg_holder;
      LaunchKernelConfig launch_config = {
        .schedule_mode = op->task_data.aicore.launch.schedule_mode,
        .engine_type = static_cast<aclrtEngineType>(op->task_data.aicore.launch.engine_type),
        .block_dim_offset = op->task_data.aicore.launch.block_dim_offset,
        .is_block_task_prefetch = op->task_data.aicore.launch.is_block_task_prefetch,
        .is_data_dump = GetIsDataDump(op->op_name, ctx.model_id, ctx.instance_handle),
        .time_out = op->task_data.aicore.launch.time_out,
        .local_memory_size = op->task_data.aicore.launch.local_memory_size,
      };
      OM2_CHK_STATUS(AssembleLaunchConfig(cfg_holder, launch_config));
      ArgsInfo *args_info = ctx.args_table.GetArgsInfo(op->task_data.aicore.args_idx);
      OM2_CHK_NOTNULL(args_info);
      std::vector<uint64_t> ordered_io_addrs;
      std::vector<Om2Tensor> io_tensors;
      (io_tensors.reserve(op->task_data.aicore.num_io_addrs));
      std::vector<Om2TaskIoEntry> report_inputs;
      std::vector<Om2TaskIoEntry> report_outputs;
      std::vector<uint64_t> report_workspace_addrs;
      std::vector<uint64_t> report_workspace_sizes;
      for (uint32_t j = 0U; (j < op->task_data.aicore.num_io_addrs); j++) {
        const auto &a = op->argsInfo[j];
        uint64_t _addr = 0U;
        switch (a.type) {
          case OP_ARG_INPUT:
          case OP_ARG_OUTPUT:
          case OP_ARG_CONST_TENSOR:
          {
            _addr = reinterpret_cast<uint64_t>(ResolveOpAddr(a.addr.mem_src, a.addr.offset, ctx.total_dev_mem_ptr, ctx.session_scope_mem_ptr, ctx.constants));
            io_tensors.push_back(BuildOm2Tensor(reinterpret_cast<void *>(_addr), a.data.tensor.size, a.data.tensor.data_type, a.data.tensor.format, a.data.tensor.shape, a.data.tensor.num_shape_dims));
            Om2TaskIoEntry _entry = {&io_tensors.back(), a.data.tensor.args_offset};
            if (((a.type == OP_ARG_INPUT) || (a.type == OP_ARG_CONST_TENSOR))) {
              report_inputs.push_back(_entry);
            } else {
              report_outputs.push_back(_entry);
            }
            break;
          }
          case OP_ARG_WORKSPACE:
          {
            _addr = reinterpret_cast<uint64_t>(ResolveOpAddr(a.addr.mem_src, a.addr.offset, ctx.total_dev_mem_ptr, ctx.session_scope_mem_ptr, ctx.constants));
            report_workspace_addrs.push_back(_addr);
            report_workspace_sizes.push_back(a.data.tensor.size);
            break;
          }
          case OP_ARG_LEVEL1_DESC:
          {
            void *_desc = ctx.args_table.GetDevArgAddr(a.data.custom_value);
            OM2_CHK_NOTNULL(_desc);
            _addr = reinterpret_cast<uint64_t>(_desc);
            break;
          }
          case OP_ARG_SHAPE_INFO:
          case OP_ARG_CUSTOM_VALUE:
          {
            _addr = a.data.custom_value;
            break;
          }
          case OP_ARG_PLACEHOLDER:
          case OP_ARG_OPTIONAL_EMPTY:
          {
            _addr = 0U;
            break;
          }
          case OP_ARG_FFTS_ADDR:
          {
            void *_ffts = nullptr;
            OM2_CHK_STATUS(aclrtGetHardwareSyncAddr(&_ffts));
            _addr = reinterpret_cast<uint64_t>(_ffts);
            break;
          }
          case OP_ARG_EVENT_ADDR:
          {
            void *_event = nullptr;
            OM2_CHK_STATUS(GetEventIdAddr(_event, ctx.event_id_mem_map, static_cast<uint32_t>(a.data.custom_value), ctx.dev_dynamic_mem_ptrs));
            _addr = reinterpret_cast<uint64_t>(_event);
            break;
          }
          case OP_ARG_OVERFLOW_ADDR:
          {
            _addr = reinterpret_cast<uint64_t>(ctx.overflow_addr);
            break;
          }
          case OP_ARG_TILING:
          {
            void *_tiling = nullptr;
            OM2_CHK_STATUS(MallocDeviceMemory(_tiling, a.data.tiling.raw_data_len, 2U, ctx.dev_dynamic_mem_ptrs));
            OM2_CHK_STATUS(aclrtMemcpy(_tiling, a.data.tiling.raw_data_len, a.data.tiling.raw_data, a.data.tiling.raw_data_len, ACL_MEMCPY_HOST_TO_DEVICE));
            _addr = reinterpret_cast<uint64_t>(_tiling);
            break;
          }
          default:
          {
            _addr = 0U;
            break;
          }
        }
        ordered_io_addrs.push_back(_addr);
      }
      Om2L0TaskRawInfo l0_info = {1U, op->task_data.aicore.l0.need_assert_or_printf, static_cast<uint64_t>(op->task_data.aicore.l0.num_l0_slots), op->task_data.aicore.l0.l0_slots};
      OM2_CHK_STATUS(ReportOm2TaskPreprocess(op->op_name, op->task_data.aicore.op_type, 0U, reinterpret_cast<uintptr_t>(args_info->dev_addr), args_info->size, report_inputs, report_outputs, report_workspace_addrs, report_workspace_sizes, static_cast<uint32_t>(op->dispatch_type), op->task_data.aicore.kernel.block_dim, ctx.stream, &l0_info, ctx.model_id, ctx.instance_handle));
      OM2_CHK_STATUS(KernelTaskDistribute(ordered_io_addrs, args_info, ctx.func_handles[op->task_data.aicore.kernel.func_idx], op->task_data.aicore.kernel.block_dim, ctx.stream, &cfg_holder.cfg));
      OM2_CHK_STATUS(ReportLaunchedOm2Task(op->op_name, op->task_data.aicore.op_type, 0U, reinterpret_cast<uintptr_t>(args_info->dev_addr), args_info->size, report_inputs.data(), static_cast<uint64_t>(report_inputs.size()), report_outputs.data(), static_cast<uint32_t>(report_outputs.size()), report_workspace_addrs.data(), report_workspace_sizes.data(), static_cast<uint32_t>(report_workspace_sizes.size()), static_cast<uint32_t>(op->dispatch_type), op->task_data.aicore.kernel.block_dim, ctx.stream, ctx.model_id, ctx.instance_handle));
    }
    break;
    default:
    return ACL_ERROR_FAILURE;
  }
  return ACL_SUCCESS;
}
const OpDef kOpDefs[] = {{
  .dispatch_type = static_cast<OpDispatchType>(0),
  .op_name = "add1",
  .argsInfo = (const OpArgInfo[]){
    {.type = 4, .data = {.custom_value = 24}},
    {.type = 4, .data = {.custom_value = 80}},
    {.type = 4, .data = {.custom_value = 136}},
    {.type = 5, .data = {.custom_value = 48}},
    {.type = 5, .data = {.custom_value = 4294967300}},
    {.type = 5, .data = {.custom_value = 1}},
    {.type = 5, .data = {.custom_value = 1}},
    {.type = 5, .data = {.custom_value = 224}},
    {.type = 5, .data = {.custom_value = 224}},
    {.type = 0, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 72}}},
    {.type = 5, .data = {.custom_value = 48}},
    {.type = 5, .data = {.custom_value = 4294967300}},
    {.type = 5, .data = {.custom_value = 1}},
    {.type = 5, .data = {.custom_value = 1}},
    {.type = 5, .data = {.custom_value = 224}},
    {.type = 5, .data = {.custom_value = 224}},
    {.type = 0, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 128}}},
    {.type = 5, .data = {.custom_value = 48}},
    {.type = 5, .data = {.custom_value = 4294967300}},
    {.type = 5, .data = {.custom_value = 1}},
    {.type = 5, .data = {.custom_value = 1}},
    {.type = 5, .data = {.custom_value = 224}},
    {.type = 5, .data = {.custom_value = 224}},
    {.type = 1, .addr = {.mem_src = 0, .offset = 1024}, .data = {.tensor = {.size = 200704, .data_type = 1, .format = 0, .shape = {1, 1, 224, 224, 0, 0, 0, 0}, .num_shape_dims = 4, .args_offset = 184}}},
  },
  .task_data = {
    .aicore = {
      .op_type = "Add",
      .num_io_addrs = 24,
      .args_idx = 0,
      .launch = {.schedule_mode = 0, .engine_type = 0, .block_dim_offset = 0, .is_block_task_prefetch = false, .time_out = 0, .local_memory_size = 0},
      .kernel = {.block_dim = 8, .func_idx = 0},
      .l0 = {.need_assert_or_printf = 0, .num_l0_slots = 9, .l0_slots = (const Om2L0ArgSlotInfo[]){{OM2_L0_ARG_LEVEL1_DESC, 0U, 0U, 0UL, 0U, 0U, 24U}, {OM2_L0_ARG_LEVEL1_DESC, 0U, 8U, 0UL, 0U, 0U, 80U}, {OM2_L0_ARG_LEVEL1_DESC, 0U, 16U, 0UL, 0U, 0U, 136U}, {OM2_L0_ARG_SHAPE_INFO, 0U, 24U, 6UL, 0U, 0U, 0U}, {OM2_L0_ARG_INPUT, 0U, 72U, 0UL, 9U, 0U, 0U}, {OM2_L0_ARG_SHAPE_INFO, 0U, 80U, 6UL, 0U, 0U, 0U}, {OM2_L0_ARG_INPUT, 0U, 128U, 0UL, 16U, 0U, 0U}, {OM2_L0_ARG_SHAPE_INFO, 0U, 136U, 6UL, 0U, 0U, 0U}, {OM2_L0_ARG_OUTPUT, 0U, 184U, 0UL, 23U, 0U, 0U}}},
    },
  },
}};
} // namespace
aclmdlRI Om2Model::GetRtModelHandle() {
  return model_handle_;
}

aclError Om2Model::Load() {
  OM2_LOGI("Load begin");
  dev_ext_info_mem_ptrs_.resize(0);
  OM2_CHK_STATUS(DispatchOp(&kOpDefs[0], {total_dev_mem_ptr_, session_scope_mem_ptr_, constants_, args_table_, func_handles_.data(), stream_list_[0], model_id_, instance_handle_, mem_event_id_mem_map_, dev_dynamic_mem_ptrs_, overflow_addr_}));
  OM2_CHK_STATUS(aclmdlRIBuildEnd(model_handle_, nullptr));
  OM2_LOGI("Load done");
  return ACL_SUCCESS;
}

aclError Om2Model::RunAsync(aclrtStream &exe_stream, size_t input_count, void **input_data, size_t output_count, void **output_data) {
  OM2_LOGI("RunAsync begin");
  if (((input_count != om2::INPUT_NUM) || (output_count != om2::OUTPUT_NUM))) {
    return ACL_ERROR_FAILURE;
  }
  auto input_data_0_tensor = reinterpret_cast<gert::Tensor *>(input_data[0]);
  auto input_data_1_tensor = reinterpret_cast<gert::Tensor *>(input_data[1]);
  auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0]);
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(0, reinterpret_cast<uintptr_t>(input_data_0_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(1, reinterpret_cast<uintptr_t>(input_data_1_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr())));

  OM2_CHK_STATUS(args_table_.CopyArgsToDevice());
  OM2_CHK_STATUS(aclmdlRIExecuteAsync(model_handle_, exe_stream));

  OM2_LOGI("RunAsync done");
  return ACL_SUCCESS;
}

aclError Om2Model::Run(size_t input_count, void **input_data, size_t output_count, void **output_data, int32_t stream_sync_timeout) {
  OM2_LOGI("Run begin");
  if (((input_count != om2::INPUT_NUM) || (output_count != om2::OUTPUT_NUM))) {
    return ACL_ERROR_FAILURE;
  }
  auto input_data_0_tensor = reinterpret_cast<gert::Tensor *>(input_data[0]);
  auto input_data_1_tensor = reinterpret_cast<gert::Tensor *>(input_data[1]);
  auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0]);
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(0, reinterpret_cast<uintptr_t>(input_data_0_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(1, reinterpret_cast<uintptr_t>(input_data_1_tensor->GetAddr())));
  OM2_CHK_STATUS(args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr())));

  OM2_CHK_STATUS(args_table_.CopyArgsToDevice());
  OM2_CHK_STATUS(aclmdlRIExecute(model_handle_, stream_sync_timeout));

  OM2_LOGI("Run done");
  return ACL_SUCCESS;
}
} // namespace om2
aclError Om2ModelCreate(om2::Om2ModelHandle *model_handle, aclmdlRI *rt_model_handle, const char **bin_files, const void **bin_data, size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle) {
  OM2_LOGI("Om2ModelCreate");
  if ((model_handle == nullptr) || (rt_model_handle == nullptr) || (*model_handle != nullptr)) {
    OM2_LOGE("Om2ModelCreate: invalid handle");
    return ACL_ERROR_FAILURE;
  }
  auto *obj = new om2::Om2Model(bin_files, bin_data, bin_size, bin_num, constants, work_ptr, session_id,
                                model_id, instance_handle);
  if (obj == nullptr) {
    OM2_LOGE("Om2ModelCreate: new Om2Model failed");
    return ACL_ERROR_FAILURE;
  }
  auto ret = obj->InitResources();
  if (ret != ACL_SUCCESS) {
    OM2_LOGE("Om2ModelCreate: InitResources failed, ret: %d", ret);
    delete obj;
    return ret;
  }
  ret = obj->RegisterKernels();
  if (ret != ACL_SUCCESS) {
    OM2_LOGE("Om2ModelCreate: RegisterKernels failed, ret: %d", ret);
    delete obj;
    return ret;
  }
  *model_handle = reinterpret_cast<om2::Om2ModelHandle>(obj);
  *rt_model_handle = obj->GetRtModelHandle();
  OM2_LOGI("Om2ModelCreate done");
  return ACL_SUCCESS;
}

aclError Om2ModelLoad(om2::Om2ModelHandle *model_handle) {
  OM2_LOGI("Om2ModelLoad");
  if ((model_handle == nullptr) || (*model_handle == nullptr)) {
    OM2_LOGE("Om2ModelLoad: invalid handle");
    return ACL_ERROR_FAILURE;
  }
  return static_cast<om2::Om2Model*>(*model_handle)->Load();
}

aclError Om2ModelRunAsync(om2::Om2ModelHandle *model_handle, aclrtStream stream, int input_count, void **input_data, int output_count, void **output_data) {
  OM2_LOGI("Om2ModelRunAsync");
  return static_cast<om2::Om2Model*>(*model_handle)->RunAsync(stream, input_count, input_data, output_count, output_data);
}

aclError Om2ModelRun(om2::Om2ModelHandle *model_handle, int input_count, void **input_data, int output_count, void **output_data, int32_t stream_sync_timeout) {
  OM2_LOGI("Om2ModelRun");
  return static_cast<om2::Om2Model*>(*model_handle)->Run(input_count, input_data, output_count, output_data, stream_sync_timeout);
}

aclError Om2ModelDestroy(om2::Om2ModelHandle *model_handle) {
  OM2_LOGI("Om2ModelDestroy");
  delete static_cast<om2::Om2Model*>(*model_handle);
  return ACL_SUCCESS;
})";
  ASSERT_EQ(outputs[GeneratedFileIndex::kLoadingAndRunningFile], expected + "\n");
}

TEST_F(ProgramGeneratorUt, DoesNotDependOnGeModel_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForArgs_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithArgs();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);
  // New table-driven patterns in DispatchOp
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("OP_ARG_TILING"), std::string::npos);
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("KernelTaskDistribute(ordered_io_addrs"),
            std::string::npos);
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("OP_ARG_EVENT_ADDR"), std::string::npos);
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("OP_ARG_OVERFLOW_ADDR"), std::string::npos);
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("OP_ARG_FFTS_ADDR"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForMemcpyAddrAsync_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithMemcpyAddrAsync();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  // KernelMemcpyAddrAsyncDistribute helper function should be generated
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("KernelMemcpyAddrAsyncDistribute"),
            std::string::npos);
  // rtMemcpyAsyncPtr should be called in the helper
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("rtMemcpyAsyncPtr"), std::string::npos);
  // memcpy_addr_async always generates iow_addr variable
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("_iow_addr"), std::string::npos);
  // memcpy_addr_async uses args_table_.GetArgsInfo
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("args_table_.GetArgsInfo"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSource_MemcpyAddrAsync_CustomValue_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithMemcpyAddrAsyncAndCustomValue();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  // KernelMemcpyAddrAsyncDistribute helper function should be generated
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("KernelMemcpyAddrAsyncDistribute"),
            std::string::npos);
  // custom value writeback should be generated for 64-bit value
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("ValueToPtr"), std::string::npos);
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("PtrToValue"), std::string::npos);
  // should generate assignment for custom value
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("*reinterpret_cast<uint64_t *>"),
            std::string::npos);
  // should generate assignment for 32-bit custom value
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("*reinterpret_cast<uint32_t *>"),
            std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForMemcpyAddrAsync_ConstTensor_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithMemcpyAddrAsyncConstTensor();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  // KernelMemcpyAddrAsyncDistribute helper function should be generated
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("KernelMemcpyAddrAsyncDistribute"),
            std::string::npos);
  // ConstTensor address references constants_[]
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("constants_["), std::string::npos);
  // iow_addr variable should be generated
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("_iow_addr"), std::string::npos);
  // args_table_.GetArgsInfo should be used
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("args_table_.GetArgsInfo"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForMemcpyAsync_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithMemcpyAsync();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // KernelMemcpyAsyncDistribute helper function should be generated
  EXPECT_NE(load_run.find("KernelMemcpyAsyncDistribute"), std::string::npos);
  // rtGeneralCtrl should be called in the helper
  EXPECT_NE(load_run.find("rtGeneralCtrl"), std::string::npos);
  // KernelMemcpyAsyncDistribute call in Load() — non-io_refresh_ path passes variable directly (not via args_table_)
  EXPECT_NE(load_run.find("OM2_CHK_STATUS(KernelMemcpyAsyncDistribute("), std::string::npos);
  // non-io_refresh_ path: address variables before the call
  EXPECT_NE(load_run.find("auto op2_output0 = GET_ADDR(total_dev_mem_ptr_, 512)"), std::string::npos);
  // non-io_refresh_ path passes variables directly to KernelMemcpyAsyncDistribute, no ValueToPtr/args_table_
  EXPECT_NE(load_run.find("op2_input0"), std::string::npos);
  EXPECT_NE(load_run.find("op2_output0"), std::string::npos);
  EXPECT_NE(load_run.find("static_cast<rtMemcpyKind_t>(1)"), std::string::npos);
  // non-io_refresh_ path should NOT have io_refresh_ specific patterns
  EXPECT_EQ(load_run.find("FlattenHostArgs"), std::string::npos);
  EXPECT_EQ(load_run.find("ioaddr_var"), std::string::npos);
  EXPECT_EQ(load_run.find("ValueToPtr"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForMemcpyAsync_IoRefresh_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithMemcpyAsyncIoRefresh();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  // io_refresh_ path generates iow_addr variable via FlattenHostArgs
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("_iow_addr"), std::string::npos);
  // io_refresh_ path uses args_table_ for addr refresh
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("args_table_.GetArgsInfo"), std::string::npos);
  // io_refresh_ path: src points to dev_addr (not inline address)
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("ValueToPtr"), std::string::npos);
  // io_refresh_ path forces RT_MEMCPY_ADDR_DEVICE_TO_DEVICE (kind 4)
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("rtMemcpyKind_t"), std::string::npos);
  // KernelMemcpyAsyncDistribute helper should still be generated
  EXPECT_NE(outputs[GeneratedFileIndex::kLoadingAndRunningFile].find("KernelMemcpyAsyncDistribute"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForCmoBarrierTask_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithCmoBarrierTask(3);
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // Barrier comment header
  EXPECT_NE(load_run.find("(Barrier)"), std::string::npos);
  // Barrier info variable declaration
  EXPECT_NE(load_run.find("rtBarrierTaskInfo_t barrier_info"), std::string::npos);
  // KernelBarrierTaskDistribute call in Load()
  EXPECT_NE(load_run.find("KernelBarrierTaskDistribute("), std::string::npos);
  // KernelBarrierTaskDistribute helper function
  EXPECT_NE(load_run.find("RT_GNL_CTRL_TYPE_BARRIER_TSK"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForCmoTask_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithCmoTask(1U, 3U);
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // CMO comment header
  EXPECT_NE(load_run.find("(CMO)"), std::string::npos);
  // cmo_info variable declaration
  EXPECT_NE(load_run.find("rtCmoTaskInfo_t cmo_info"), std::string::npos);
  // KernelCmoTaskDistribute call in Load()
  EXPECT_NE(load_run.find("KernelCmoTaskDistribute("), std::string::npos);
  // KernelCmoTaskDistribute helper function
  EXPECT_NE(load_run.find("RT_GNL_CTRL_TYPE_CMO_TSK"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForCmoAddrTask_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithCmoAddrTask(false);
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // CMO_ADDR comment header
  EXPECT_NE(load_run.find("(CMO_ADDR)"), std::string::npos);
  // iow_addr variable
  EXPECT_NE(load_run.find("_iow_addr0"), std::string::npos);
  // KernelCmoAddrTaskDistribute call
  EXPECT_NE(load_run.find("KernelCmoAddrTaskDistribute("), std::string::npos);
  // KernelCmoAddrTaskDistribute helper: rtCmoAddrTaskLaunch
  EXPECT_NE(load_run.find("rtCmoAddrTaskLaunch"), std::string::npos);
  // args_table_.GetArgsInfo
  EXPECT_NE(load_run.find("args_table_.GetArgsInfo("), std::string::npos);
  // aclrtMemcpy with DEVICE_TO_HOST
  EXPECT_NE(load_run.find("ACL_MEMCPY_DEVICE_TO_HOST"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForCmoAddrTaskExplicitFormat_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithCmoAddrTask(true);
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // CMO_ADDR comment header
  EXPECT_NE(load_run.find("(CMO_ADDR)"), std::string::npos);
  // iow_addr variable
  EXPECT_NE(load_run.find("_iow_addr0"), std::string::npos);
  // KernelCmoAddrTaskDistribute call
  EXPECT_NE(load_run.find("KernelCmoAddrTaskDistribute("), std::string::npos);
  // KernelCmoAddrTaskDistribute helper: rtCmoAddrTaskLaunch
  EXPECT_NE(load_run.find("rtCmoAddrTaskLaunch"), std::string::npos);
  // args_table_.GetArgsInfo
  EXPECT_NE(load_run.find("args_table_.GetArgsInfo("), std::string::npos);
  // aclrtMemcpy with DEVICE_TO_HOST
  EXPECT_NE(load_run.find("ACL_MEMCPY_DEVICE_TO_HOST"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForCmoAddrTask_ConstTensor) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithCmoAddrTaskConstTensor();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // CMO_ADDR comment header
  EXPECT_NE(load_run.find("(CMO_ADDR)"), std::string::npos);
  // KernelCmoAddrTaskDistribute call
  EXPECT_NE(load_run.find("KernelCmoAddrTaskDistribute("), std::string::npos);
  // const tensor with const_index path: uses constants_[N]
  EXPECT_NE(load_run.find("BuildOm2Tensor(reinterpret_cast<void *>(_addr)"), std::string::npos);
}

GeRootModelPtr CreateGeRootModelWithDsaOp() {
  auto graph = gert::ShareGraph::BuildDsaRandomNormalKnownGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  builder.AddDefaultWeights();
  auto ge_root_model = builder.BuildGeRootModel();
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
      op_desc->SetInputOffset({2048});
    } else {
      // DSA op: 4 inputs, 1 output, 2 workspaces (session scope)
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 2048));
      op_desc->SetWorkspaceBytes({64, 128});
      op_desc->SetWorkspace({0, 64});
      // Mark second workspace as session scope (kSessionNoReuse = 1)
      (void)AttrUtils::SetListInt(op_desc, ATTR_NAME_WORKSPACE_MEMORY_NO_REUSE_SCOPE, std::vector<int32_t>{0, 1});
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();

  // Add DSA task
  auto *dsa_task = model_task_def->add_task();
  dsa_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_DSA));
  dsa_task->set_stream_id(0U);
  auto *dsa_def = dsa_task->mutable_dsa_task();
  // Find the DSA op's index in the graph
  int64_t dsa_op_index = 0;
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() != DATA) && (op_desc->GetType() != NETOUTPUT)) {
      dsa_op_index = op_desc->GetId();
      break;
    }
  }
  dsa_def->set_op_index(static_cast<uint32_t>(dsa_op_index));
  dsa_def->set_start(1);
  dsa_def->set_sqe_type(0);
  dsa_def->set_distribution_type(0);
  dsa_def->set_data_type(0);
  dsa_def->set_alg_type(0);
  dsa_def->set_input_vld(0);
  dsa_def->set_input_value_addr_flag(0);
  dsa_def->set_seed_value_or_ptr(1);
  dsa_def->set_random_count_value_or_ptr(1);
  dsa_def->set_input1_value_or_ptr(1);
  dsa_def->set_input2_value_or_ptr(1);
  auto *args = dsa_def->mutable_args();
  uint64_t seed_val = 0;
  args->set_seed_value_or_addr(reinterpret_cast<const char *>(&seed_val), sizeof(uint64_t));
  uint64_t count_val = 100;
  args->set_random_count_value_or_addr(reinterpret_cast<const char *>(&count_val), sizeof(uint64_t));
  uint64_t input1_val = 0;
  args->set_input1_value_or_addr(reinterpret_cast<const char *>(&input1_val), sizeof(uint64_t));
  uint64_t input2_val = 0;
  args->set_input2_value_or_addr(reinterpret_cast<const char *>(&input2_val), sizeof(uint64_t));

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 4096);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_SESSION_SCOPE_MEMORY_SIZE, 192);
  return ge_root_model;
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForDsa_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithDsaOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // DSA op comment header
  EXPECT_NE(load_run.find("// ============================= random_normal ==============================="),
            std::string::npos);
  // Input address declarations (4 inputs)
  EXPECT_NE(load_run.find("Om2Tensor op4_input0 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024),"),
            std::string::npos);
  EXPECT_NE(load_run.find("Om2Tensor op4_input1 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024),"),
            std::string::npos);
  EXPECT_NE(load_run.find("Om2Tensor op4_input2 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024),"),
            std::string::npos);
  EXPECT_NE(load_run.find("Om2Tensor op4_input3 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 1024),"),
            std::string::npos);
  // Output address declaration
  EXPECT_NE(load_run.find("Om2Tensor op4_output0 = BuildOm2Tensor(GET_ADDR(total_dev_mem_ptr_, 2048),"),
            std::string::npos);
  // Workspace address declarations (ws0 normal, ws1 session scope)
  EXPECT_NE(load_run.find("auto op4_ws0 = GET_ADDR(total_dev_mem_ptr_, 0);"), std::string::npos);
  EXPECT_NE(load_run.find("auto op4_ws1 = GET_ADDR(session_scope_mem_ptr_, 64);"), std::string::npos);
  // SQE struct declaration
  EXPECT_NE(load_run.find("rtStarsDsaSqe_t op4_dsa_sqe = {};"), std::string::npos);
  // SQE scalar fields
  EXPECT_NE(load_run.find("op4_dsa_sqe.sqeHeader.type = 0U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.start = 1U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.functionType = 0U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dataType = 0U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.algoType = 0U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.paramVldBitmap = 0U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.paramAddrValBitmap = 0U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.kernelCredit = 100;"), std::string::npos);
  // dsaCfgResultAddr (output_addrs_[0])
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgResultAddrLow"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgResultAddrHigh"), std::string::npos);
  EXPECT_NE(load_run.find("ValueToPtr(op4_output0.device_address)"), std::string::npos);
  // dsaCfgStateAddr (workspace_addrs_[0])
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgStateAddrLow"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgStateAddrHigh"), std::string::npos);
  // dsaCfgParamAddr (hbm entry dev_addr)
  EXPECT_NE(load_run.find("uint64_t op4_dsa_param_addr ="), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgParamAddrLow"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgParamAddrHigh"), std::string::npos);
  // dsaCfgSeed (immediate value)
  EXPECT_NE(load_run.find("uint64_t op4_dsa_seed = 0U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgSeedLow"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgSeedHigh"), std::string::npos);
  // dsaCfgNumber (immediate value)
  EXPECT_NE(load_run.find("uint64_t op4_dsa_count = 100U;"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgNumberLow"), std::string::npos);
  EXPECT_NE(load_run.find("op4_dsa_sqe.dsaCfgNumberHigh"), std::string::npos);
  // input1/input2 (immediate values, not addr mode)
  EXPECT_NE(load_run.find("uint64_t op4_dsa_input1 = 0U;"), std::string::npos);
  EXPECT_NE(load_run.find("uint64_t op4_dsa_input2 = 0U;"), std::string::npos);
  // IO address vector and memcpy
  EXPECT_NE(load_run.find("std::vector<uint64_t> op4_dsa_iow_addr = FlattenHostArgs("), std::string::npos);
  EXPECT_NE(load_run.find("FlattenHostArgs(op4_dsa_input1, op4_dsa_input2, op4_input0, op4_input1, op4_input2, "
                          "op4_input3, op4_output0)"),
            std::string::npos);
  EXPECT_NE(load_run.find("memcpy_s(args_table_.GetArgsInfo(0)->host_addr"), std::string::npos);
  EXPECT_NE(load_run.find("aclrtMemcpy(args_table_.GetArgsInfo(0)->dev_addr, 168,"), std::string::npos);
  // Task tag
  EXPECT_NE(load_run.find("rtSetTaskTag(\"random_normal\")"), std::string::npos);
  // KernelDsaTaskDistribute call
  EXPECT_NE(load_run.find("KernelDsaTaskDistribute(&op4_dsa_sqe"), std::string::npos);
  // KernelDsaTaskDistribute helper function
  EXPECT_NE(load_run.find("aclError KernelDsaTaskDistribute(const void *const sqe"), std::string::npos);
  // dump_flag variable for data dump support
  EXPECT_NE(load_run.find("dump_flag ="), std::string::npos);
  // GetIsDataDump call for dump flag calculation
  EXPECT_NE(load_run.find("GetIsDataDump("), std::string::npos);
  // ReportLaunchedOm2Task call for dump reporting
  EXPECT_NE(load_run.find("ReportLaunchedOm2Task("), std::string::npos);
  EXPECT_NE(load_run.find("model_id_, instance_handle_, 1U)"), std::string::npos);

  // Session scope memory should be allocated in InitResources
  EXPECT_NE(outputs[GeneratedFileIndex::kResourcesFile].find("aclrtMalloc"), std::string::npos);
  // Session scope memory should be freed in ReleaseResources
  EXPECT_NE(outputs[GeneratedFileIndex::kResourcesFile].find("aclrtFree"), std::string::npos);
  // Session scope memory base pointer should be in Om2Model class
  EXPECT_NE(outputs[GeneratedFileIndex::kInterfaceHeaderFile].find("session_scope_mem_ptr_"), std::string::npos);
}

struct GetRtAddressTestContext {
  OpDescPtr op_desc;
  RuntimeResourceSemantic runtime;
  ModelIoSemantic model_io;
  std::unordered_map<int64_t, std::string> weight_offset_to_varname;
  std::unordered_map<int64_t, std::string> fileconst_output_offset_to_varname;
  std::unordered_map<int64_t, OpInputEdges> op_id_to_input_edges;
  domi::TaskDef task_def;
  std::unique_ptr<TaskSemanticContributeContext> context;

  GetRtAddressTestContext() {
    op_desc = std::make_shared<OpDesc>("test_op", "TestOp");
    GeTensorDesc tensor_desc;
    op_desc->AddInputDesc(tensor_desc);
    op_desc->AddOutputDesc(tensor_desc);
    op_desc->SetInputOffset({1024});
    op_desc->SetOutputOffset({2048});
    runtime.logic_mem_base = 0x1000U;
    runtime.total_mem_size = 0x2000U;
    runtime.logic_weight_base = 0x5000U;
    runtime.total_weight_size = 0x1000U;
    runtime.var_size = 0U;
    OpInputEdges edges;
    edges.input_op_ids = {kInvalidOpId};
    edges.input_anchor_indices = {kInvalidAnchorIndex};
    edges.output_var_names = {"op0_output0"};
    op_id_to_input_edges[op_desc->GetId()] = edges;
    context = std::make_unique<TaskSemanticContributeContext>(TaskSemanticContributeContext{
        ModelTaskType::MODEL_TASK_KERNEL, task_def, 0, op_desc, &runtime, &model_io, nullptr, &weight_offset_to_varname,
        &fileconst_output_offset_to_varname, &op_id_to_input_edges, nullptr, nullptr, nullptr, nullptr});
  }
};

TEST_F(ProgramGeneratorUt, GetRtAddress_Placeholder_ReturnsSuccess) {
  GetRtAddressTestContext ctx;
  AddrSemantic addr_node;
  EXPECT_EQ(Om2ModelUtils::GetRtAddress(*ctx.context, std::numeric_limits<uintptr_t>::max(), addr_node, true, 0U),
            SUCCESS);
}

TEST_F(ProgramGeneratorUt, GetRtAddress_InMemRange_SetsOutputInstance) {
  GetRtAddressTestContext ctx;
  const uintptr_t logic_addr = ctx.runtime.logic_mem_base + 1024U;
  AddrSemantic addr_node;
  EXPECT_EQ(Om2ModelUtils::GetRtAddress(*ctx.context, logic_addr, addr_node, false, 0U), SUCCESS);
  EXPECT_EQ(addr_node.kind, AddrValueKind::kOutputInstance);
  EXPECT_EQ(addr_node.mem_offset, static_cast<int64_t>(1024U));
}

TEST_F(ProgramGeneratorUt, GetRtAddress_InWeightRange_SetsConstTensor) {
  GetRtAddressTestContext ctx;
  ctx.weight_offset_to_varname[256] = "const_256";
  const uintptr_t logic_addr = ctx.runtime.logic_weight_base + 256U;
  AddrSemantic addr_node;
  EXPECT_EQ(Om2ModelUtils::GetRtAddress(*ctx.context, logic_addr, addr_node, true, 0U), SUCCESS);
  EXPECT_EQ(addr_node.kind, AddrValueKind::kConstTensor);
  EXPECT_EQ(addr_node.mem_offset, 256);
  EXPECT_TRUE(addr_node.is_reused_from_upstream);
}

TEST_F(ProgramGeneratorUt, GetRtAddress_ZeroAddr_SetsEmptyAddr) {
  GetRtAddressTestContext ctx;
  AddrSemantic addr_node;
  EXPECT_EQ(Om2ModelUtils::GetRtAddress(*ctx.context, 0U, addr_node, true, 0U), SUCCESS);
  EXPECT_EQ(addr_node.kind, AddrValueKind::kEmptyAddr);
}

TEST_F(ProgramGeneratorUt, GetRtAddress_InvalidAddr_ReturnsParamInvalid) {
  GetRtAddressTestContext ctx;
  AddrSemantic addr_node;
  EXPECT_NE(Om2ModelUtils::GetRtAddress(*ctx.context, 0xFFFF0000U, addr_node, true, 0U), SUCCESS);
}

TEST_F(ProgramGeneratorUt, GetRtAddress_InConstantRange_SetsConstTensor) {
  GetRtAddressTestContext ctx;
  ctx.runtime.var_size = 1024U;
  ctx.runtime.logic_var_base = kMemoryVarLogicBase;
  const uintptr_t logic_addr = kMemoryVarLogicBase + 256U;
  const int64_t addr_key = static_cast<int64_t>(logic_addr);
  ctx.fileconst_output_offset_to_varname[addr_key] = "const_256";
  AddrSemantic addr_node;
  EXPECT_EQ(Om2ModelUtils::GetRtAddress(*ctx.context, logic_addr, addr_node, true, 0U), SUCCESS);
  EXPECT_EQ(addr_node.kind, AddrValueKind::kConstTensor);
  EXPECT_EQ(addr_node.symbol_hint, "const_256");
  EXPECT_EQ(addr_node.mem_offset, addr_key);
}

TEST_F(ProgramGeneratorUt, GetRtAddress_MatchedMemoryInfo_ReturnsParamInvalid) {
  GetRtAddressTestContext ctx;
  const uintptr_t logic_addr = 0x8000U;
  MemInfo mem_info(static_cast<int64_t>(0x8000U), static_cast<int64_t>(0x1000U), nullptr, false);
  mem_info.memory_type = RT_MEMORY_HBM;
  ctx.runtime.memory_infos[0] = mem_info;
  AddrSemantic addr_node;
  EXPECT_NE(Om2ModelUtils::GetRtAddress(*ctx.context, logic_addr, addr_node, true, 0U), SUCCESS);
}

GeRootModelPtr CreateGeRootModelWithAicoreTilingOp() {
  auto graph = gert::ShareGraph::AicoreStaticGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder
          .AddTaskDef(
              "Add",
              gert::AiCoreTaskDefFaker("add_stub")
                  .ArgsFormat(
                      "{i_instance0*}{i_instance1*}{o_instance0*}{ws0*}{event_addr}{ffts_addr}{overflow_addr}{t}"))
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
      (void)AttrUtils::SetBool(op_desc, GLOBALWORKSPACE_TYPE, true);
      (void)AttrUtils::SetBool(op_desc, "_memcheck", true);
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
  // Set ATTR_NAME_OP_RUN_INFO on both root graph and ge_model graph nodes
  // so CopyTilingDataIfNeeded enters run_info != nullptr branch
  auto set_tiling_on_graph = [](const ComputeGraphPtr &g) {
    for (const auto &node : g->GetAllNodes()) {
      auto op_desc = node->GetOpDesc();
      if ((op_desc != nullptr) && (op_desc->GetType() != DATA) && (op_desc->GetType() != NETOUTPUT)) {
        auto run_info = std::make_shared<optiling::utils::OpRunInfo>(8, false, 0);
        const std::string tiling_str = "test_tiling_data";
        run_info->AddTilingData(tiling_str.c_str(), tiling_str.size());
        (void)op_desc->SetExtAttr(ATTR_NAME_OP_RUN_INFO, run_info);
      }
    }
  };
  set_tiling_on_graph(compute_graph);
  const auto model_graph = ge_model->GetGraph();
  if (model_graph != nullptr) {
    set_tiling_on_graph(model_graph);
  }

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);

  return ge_root_model;
}

TEST_F(ProgramGeneratorUt, CopyTilingDataIfNeeded_RunInfoNotNull_SetsTilingData) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreTilingOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // CopyTilingDataIfNeeded enters run_info != nullptr branch, has_tiling_ is set,
  // tiling data variable and memcpy should be generated
  EXPECT_NE(load_run.find("_tiling"), std::string::npos);
  EXPECT_NE(load_run.find("ACL_MEMCPY_HOST_TO_DEVICE"), std::string::npos);
}

GeRootModelPtr CreateGeRootModelWithAicoreOpWithoutArgsFormat() {
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
  // No ArgsFormat set, so kernel_context.args_format() will be empty,
  // triggering BuildOrderedArgValuesWithoutArgsFormat
  auto ge_root_model =
      builder.AddTaskDef("Add", gert::AiCoreTaskDefFaker("add_stub")).FakeTbeBin({"Add"}).BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return nullptr;
    }
    if ((op_desc->GetType() == DATA)) {
      op_desc->SetOutputOffset({1024});
    } else if ((op_desc->GetType() == NETOUTPUT)) {
      op_desc->SetInputOffset({3072});
    } else {
      (void)AttrUtils::SetBool(op_desc, GLOBALWORKSPACE_TYPE, true);
      (void)AttrUtils::SetBool(op_desc, "_memcheck", true);
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      op_desc->SetWorkspaceBytes(std::vector<int64_t>(1, 64));
      op_desc->SetWorkspace(std::vector<int64_t>(1, 0));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  // Set ATTR_NAME_OP_RUN_INFO on graph nodes so has_tiling_ is set to true
  // and ConstructDfxInfo -> UpdateDfxArgsAndShapeSize is exercised
  auto set_tiling_on_graph = [](const ComputeGraphPtr &g) {
    for (const auto &node : g->GetAllNodes()) {
      auto op_desc = node->GetOpDesc();
      if ((op_desc != nullptr) && (op_desc->GetType() != DATA) && (op_desc->GetType() != NETOUTPUT)) {
        auto run_info = std::make_shared<optiling::utils::OpRunInfo>(8, false, 0);
        const std::string tiling_str = "test_tiling_data";
        run_info->AddTilingData(tiling_str.c_str(), tiling_str.size());
        (void)op_desc->SetExtAttr(ATTR_NAME_OP_RUN_INFO, run_info);
      }
    }
  };
  set_tiling_on_graph(compute_graph);
  const auto model_graph = ge_model->GetGraph();
  if (model_graph != nullptr) {
    set_tiling_on_graph(model_graph);
  }

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);

  return ge_root_model;
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForAicoreAssertDfxSetsL0RawFlag) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOpWithoutArgsFormat();
  auto set_assert_dfx = [](const ComputeGraphPtr &graph) {
    if (graph == nullptr) {
      return;
    }
    for (const auto &node : graph->GetDirectNode()) {
      auto op_desc = node->GetOpDesc();
      if ((op_desc != nullptr) && (op_desc->GetType() == "Add")) {
        (void)AttrUtils::SetListStr(op_desc, "_op_dfx_options", std::vector<std::string>{"assert"});
      }
    }
  };
  set_assert_dfx(ge_root_model->GetRootGraph());
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  set_assert_dfx(ge_model->GetGraph());
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  EXPECT_NE(load_run.find("Om2L0TaskRawInfo l0_info = {1U, op->task_data.aicore.l0.need_assert_or_printf"),
            std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForAicoreWithoutArgsFormat_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOpWithoutArgsFormat();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // BuildOrderedArgValuesWithoutArgsFormat generates FlattenHostArgs with input/output/workspace addrs
  EXPECT_NE(load_run.find("ordered_io_addrs"), std::string::npos);
  // Should contain KernelTaskDistribute call
  EXPECT_NE(load_run.find("KernelTaskDistribute"), std::string::npos);
  // args_table_.GetArgsInfo should be generated for the args table entry
  EXPECT_NE(load_run.find("DispatchOp"), std::string::npos);
  EXPECT_NE(load_run.find("ACL_MEMCPY_HOST_TO_DEVICE"), std::string::npos);
  EXPECT_NE(load_run.find("OM2_L0_ARG_TILING"), std::string::npos);
  EXPECT_EQ(load_run.find("OM2_L0_ARG_TILING, 0U, 32U, 0UL"), std::string::npos);
  // GLOBALWORKSPACE_TYPE branch: overflow_addr should be appended
  EXPECT_NE(load_run.find("overflow_addr_"), std::string::npos);
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

TEST_F(ProgramGeneratorUt, GenerateKernelRegistryForCustAicpu_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithCustAicpuOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &kernel_reg = outputs[GeneratedFileIndex::kKernelRegistryFile];
  std::cout << "=== kernel_reg content ===" << std::endl << kernel_reg << std::endl << "=== end ===" << std::endl;
  ASSERT_FALSE(kernel_reg.empty());
  EXPECT_NE(kernel_reg.find("struct CustAicpuRegisterInfo"), std::string::npos);
  EXPECT_NE(kernel_reg.find("std::string file"), std::string::npos);
  EXPECT_NE(kernel_reg.find("const char *op_type"), std::string::npos);
  EXPECT_NE(kernel_reg.find("const char *func_name"), std::string::npos);
  EXPECT_NE(kernel_reg.find(
                "aclError RegisterCustAicpuKernel(aclrtBinHandle &bin_handle, aclrtFuncHandle &func_handle, const "
                "CustAicpuRegisterInfo &register_info, std::unordered_map<std::string, BinDataInfo> &bin_info_map) {"),
            std::string::npos);
  EXPECT_NE(kernel_reg.find("auto &bin_info = bin_info_map[register_info.file];"), std::string::npos);
  EXPECT_NE(kernel_reg.find("aclrtBinaryLoadOptions load_options;"), std::string::npos);
  EXPECT_NE(kernel_reg.find("aclrtBinaryLoadOption option;"), std::string::npos);
  EXPECT_NE(kernel_reg.find("load_options.numOpt = 1;"), std::string::npos);
  EXPECT_NE(kernel_reg.find("load_options.options = &option;"), std::string::npos);
  EXPECT_NE(kernel_reg.find("option.type = ACL_RT_BINARY_LOAD_OPT_CPU_KERNEL_MODE;"), std::string::npos);
  EXPECT_NE(kernel_reg.find("option.value.cpuKernelMode = 2;"), std::string::npos);
  EXPECT_NE(kernel_reg.find(
                "OM2_CHK_STATUS(aclrtBinaryLoadFromData(bin_info.data, bin_info.size, &load_options, &bin_handle));"),
            std::string::npos);
  EXPECT_NE(kernel_reg.find("OM2_CHK_STATUS(aclrtRegisterCpuFunc(bin_handle, register_info.func_name, "
                            "register_info.op_type, &func_handle));"),
            std::string::npos);
  const size_t cust_aicpu_hash_id = std::hash<std::string>{}(std::string(64, '\0'));
  const std::string cust_aicpu_file_name = std::to_string(cust_aicpu_hash_id) + "_CustAicpuKernel";
  const std::string expected_reg_call =
      "OM2_CHK_STATUS(RegisterCustAicpuKernel(bin_handles_[0], func_handles_[0], {\"" + cust_aicpu_file_name +
      "\", \"Add\", \"name\"}, bin_info_map_));";
  EXPECT_NE(kernel_reg.find(expected_reg_call), std::string::npos);

  const auto &load_and_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  // std::cout << "=== load_and_run content ===" << std::endl << load_and_run << std::endl << "=== end ===" <<
  // std::endl;
  EXPECT_NE(
      load_and_run.find("std::vector<uint64_t> op2_iow_addr = FlattenHostArgs(op2_input0, op2_input1, op2_output0);"),
      std::string::npos);
  EXPECT_NE(load_and_run.find("const char *op2_args_str"), std::string::npos);
  EXPECT_NE(load_and_run.find("const char *op2_ext_info_str"), std::string::npos);
  EXPECT_NE(load_and_run.find("std::vector<uint8_t> op2_args;"), std::string::npos);
  EXPECT_NE(load_and_run.find("op2_args.resize"), std::string::npos);
  EXPECT_NE(load_and_run.find("AssembleAicpuExtInfo"), std::string::npos);
  EXPECT_NE(load_and_run.find("AssembleAicpuArgs"), std::string::npos);
  EXPECT_NE(load_and_run.find("LaunchKernelCfgHolder op2_cfg_holder;"), std::string::npos);
  EXPECT_NE(load_and_run.find("OM2_CHK_STATUS(AicpuKernelTaskDistribute(op2_args, args_table_.GetArgsInfo(0), "
                              "func_handles_[0], 8, stream_list_[0], &op2_cfg_holder.cfg));"),
            std::string::npos);
  EXPECT_NE(load_and_run.find("GetIsDataDump("), std::string::npos);
  EXPECT_NE(load_and_run.find("ReportLaunchedOm2Task("), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForKernelExTask_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithTfAicpuOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  // Verify that the generated code contains kernel_ex_task handling
  const auto &load_run_code = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  std::cout << "load_run_code :" << load_run_code << std::endl;
  ASSERT_FALSE(load_run_code.empty());
  EXPECT_NE(load_run_code.find("aclError TfAicpuKernelTaskDistribute(const std::vector<uint64_t> &io_addrs, ArgsInfo "
                               "*args_info, void *kernel_buf, uint32_t kernel_buf_size, aclrtFuncHandle func_handle, "
                               "uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg *config)"),
            std::string::npos);
  EXPECT_NE(
      load_run_code.find("aclError AssembleTfAicpuExSessionIdInfo(uint64_t *session_id, size_t op_kernel_size, "
                         "aclrtFuncHandle func_handle, uint32_t block_dim, aclrtStream stream, aclrtLaunchKernelCfg "
                         "*config, TfAiCpuExInfo *tf_ai_cpu_ex_info, std::vector<void *> &mem_ptrs)"),
      std::string::npos);
  EXPECT_NE(load_run_code.find(
                "aclError AssembleTfAicpuExKernelIdInfo(uint64_t *kernel_id, TfAiCpuExInfo *tf_ai_cpu_ex_info) {"),
            std::string::npos);
  EXPECT_NE(
      load_run_code.find("aclError AssembleTfAicpuExWorkSpaceAddrInfo(uint8_t *kernel_ex_task_info, size_t "
                         "kernel_ex_task_info_len, TfAiCpuExInfo *tf_ai_cpu_ex_info, std::vector<void *> &mem_ptrs)"),
      std::string::npos);
  EXPECT_NE(
      load_run_code.find(
          "aclError AssembleTfAicpuExInputOutputAddrInfo(ArgsInfo *args_info, TfAiCpuExInfo *tf_ai_cpu_ex_info) {"),
      std::string::npos);
  EXPECT_NE(
      load_run_code.find("aclError AssembleTfAicpuExExtInfo(uint8_t *kernel_ex_ext_info, size_t "
                         "kernel_ex_ext_info_len, TfAiCpuExInfo *tf_ai_cpu_ex_info, std::vector<void *> &mem_ptrs)"),
      std::string::npos);
  EXPECT_NE(load_run_code.find("aclError AssembleTfAicpuArgs(uint8_t *kernel_ex_args, size_t kernel_ex_args_len, void "
                               "*target_args_ptr, size_t target_args_len, TfAiCpuExInfo *tf_ai_cpu_ex_info)"),
            std::string::npos);
  EXPECT_NE(load_run_code.find("std::vector<uint8_t> op2_args;"), std::string::npos);
  EXPECT_NE(load_run_code.find("op2_args.resize(112);"), std::string::npos);
  EXPECT_NE(load_run_code.find("void *op2_kernel_buf = nullptr;"), std::string::npos);
  EXPECT_NE(load_run_code.find("TfAiCpuExInfo op2_tf_ai_cpu_ex_info;"), std::string::npos);
  EXPECT_NE(load_run_code.find("const char *op2_kernel_ex_args_str"), std::string::npos);
  EXPECT_NE(load_run_code.find("const char *op2_kernel_ex_task_info_str"), std::string::npos);
  EXPECT_NE(load_run_code.find("const char *op2_kernel_ex_ext_info_str"), std::string::npos);
  EXPECT_NE(load_run_code.find("std::vector<int64_t> op2_input0_shape"), std::string::npos);
  EXPECT_NE(load_run_code.find("Om2Tensor op2_input0 = BuildOm2Tensor("), std::string::npos);
  EXPECT_NE(load_run_code.find("Om2Tensor op2_input1 = BuildOm2Tensor("), std::string::npos);
  EXPECT_NE(load_run_code.find("Om2Tensor op2_output0 = BuildOm2Tensor("), std::string::npos);
  EXPECT_NE(load_run_code.find("FlattenHostArgs(op2_input0, op2_input1, op2_output0)"), std::string::npos);
  EXPECT_NE(load_run_code.find("OM2_CHK_STATUS(AssembleTfAicpuExSessionIdInfo"), std::string::npos);
  EXPECT_NE(load_run_code.find("OM2_CHK_STATUS(AssembleTfAicpuExKernelIdInfo"), std::string::npos);
  EXPECT_NE(load_run_code.find("OM2_CHK_STATUS(AssembleTfAicpuExWorkSpaceAddrInfo"), std::string::npos);
  EXPECT_NE(load_run_code.find("OM2_CHK_STATUS(AssembleTfAicpuExInputOutputAddrInfo"), std::string::npos);
  EXPECT_NE(load_run_code.find("OM2_CHK_STATUS(AssembleTfAicpuExExtInfo"), std::string::npos);
  EXPECT_NE(load_run_code.find("OM2_CHK_STATUS(AssembleTfAicpuArgs"), std::string::npos);
  EXPECT_NE(load_run_code.find("OM2_CHK_STATUS(TfAicpuKernelTaskDistribute"), std::string::npos);
  EXPECT_NE(load_run_code.find("GetIsDataDump("), std::string::npos);
  EXPECT_NE(load_run_code.find("ReportLaunchedOm2Task("), std::string::npos);
}

void AppendAsyncWait(std::string &out) {
  size_t len = sizeof(AsyncWaitInfo) + sizeof(AicpuExtInfo);
  vector<char> vec(len, 0);
  AicpuExtInfo *aicpu_ext_info = reinterpret_cast<AicpuExtInfo *>(vec.data());
  aicpu_ext_info->infoType = aicpu::FWKAdapter::FWK_ADPT_EXT_ASYNCWAIT;
  aicpu_ext_info->infoLen = sizeof(AsyncWaitInfo);
  AsyncWaitInfo async_wait = {};
  *(ge::PtrToPtr<char, AsyncWaitInfo>(aicpu_ext_info->infoMsg)) = async_wait;
  std::string s(vec.data(), len);
  out.append(s);
}

void AppendWorkSpaceInfo(std::string &out) {
  size_t len = sizeof(WorkSpaceInfo) + sizeof(AicpuExtInfo);
  vector<char> vec(len, 0);
  AicpuExtInfo *aicpu_ext_info = reinterpret_cast<AicpuExtInfo *>(vec.data());
  aicpu_ext_info->infoType = aicpu::FWKAdapter::FWK_ADPT_EXT_WORKSPACE_INFO;
  aicpu_ext_info->infoLen = sizeof(WorkSpaceInfo);
  WorkSpaceInfo workspace_info = {};
  *(ge::PtrToPtr<char, WorkSpaceInfo>(aicpu_ext_info->infoMsg)) = workspace_info;
  std::string s(vec.data(), len);
  out.append(s);
}

std::string GetFakeExtInfoWithAsyncWait() {
  std::string ext_info;
  AppendShape(aicpu::FWKAdapter::FWK_ADPT_EXT_INPUT_SHAPE, 2, ext_info);
  AppendShape(aicpu::FWKAdapter::FWK_ADPT_EXT_OUTPUT_SHAPE, 1, ext_info);
  AppendSessionInfo(ext_info);
  AppendAsyncWait(ext_info);
  AppendBitMap(ext_info);
  AppendUpdateAddr(ext_info);
  AppendTopicType(ext_info);
  return ext_info;
}

std::string GetFakeExtInfoWithWorkSpace() {
  std::string ext_info;
  AppendShape(aicpu::FWKAdapter::FWK_ADPT_EXT_INPUT_SHAPE, 2, ext_info);
  AppendShape(aicpu::FWKAdapter::FWK_ADPT_EXT_OUTPUT_SHAPE, 1, ext_info);
  AppendWorkSpaceInfo(ext_info);
  return ext_info;
}

TEST_F(ProgramGeneratorUt, UpdateInputShapeAndType_Success) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc input_desc;
  input_desc.SetShape(GeShape({1, 2, 3, 4}));
  input_desc.SetDataType(DT_FLOAT);

  ASSERT_EQ(handler.UpdateInputShapeAndType(0, input_desc), SUCCESS);
  ASSERT_EQ(handler.UpdateInputShapeAndType(1, input_desc), SUCCESS);
}

TEST_F(ProgramGeneratorUt, UpdateInputShapeAndType_IndexOutOfRange) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc input_desc;
  input_desc.SetShape(GeShape({1, 2, 3, 4}));
  input_desc.SetDataType(DT_FLOAT);

  ASSERT_EQ(handler.UpdateInputShapeAndType(2, input_desc), ACL_ERROR_GE_INTERNAL_ERROR);
  ASSERT_EQ(handler.UpdateInputShapeAndType(3, input_desc), ACL_ERROR_GE_INTERNAL_ERROR);
}

TEST_F(ProgramGeneratorUt, UpdateInputShapeAndTypeWithDims_Success) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc input_desc;
  input_desc.SetDataType(DT_FLOAT);
  std::vector<int64_t> dims = {1, 2, 3, 4};

  ASSERT_EQ(handler.UpdateInputShapeAndType(0, input_desc, dims), SUCCESS);
  ASSERT_EQ(handler.UpdateInputShapeAndType(1, input_desc, dims), SUCCESS);
}

TEST_F(ProgramGeneratorUt, UpdateInputShapeAndTypeWithDims_IndexOutOfRange) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc input_desc;
  input_desc.SetDataType(DT_FLOAT);
  std::vector<int64_t> dims = {1, 2, 3, 4};

  ASSERT_EQ(handler.UpdateInputShapeAndType(2, input_desc, dims), ACL_ERROR_GE_INTERNAL_ERROR);
  ASSERT_EQ(handler.UpdateInputShapeAndType(3, input_desc, dims), ACL_ERROR_GE_INTERNAL_ERROR);
}

TEST_F(ProgramGeneratorUt, UpdateOutputShapeAndType_Success) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc output_desc;
  output_desc.SetShape(GeShape({1, 2, 3, 4}));
  output_desc.SetDataType(DT_FLOAT);

  ASSERT_EQ(handler.UpdateOutputShapeAndType(0, output_desc), SUCCESS);
}

TEST_F(ProgramGeneratorUt, UpdateOutputShapeAndType_IndexOutOfRange) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc output_desc;
  output_desc.SetShape(GeShape({1, 2, 3, 4}));
  output_desc.SetDataType(DT_FLOAT);

  ASSERT_EQ(handler.UpdateOutputShapeAndType(1, output_desc), ACL_ERROR_GE_INTERNAL_ERROR);
  ASSERT_EQ(handler.UpdateOutputShapeAndType(2, output_desc), ACL_ERROR_GE_INTERNAL_ERROR);
}

TEST_F(ProgramGeneratorUt, UpdateOutputShapeAndTypeWithDims_Success) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc output_desc;
  output_desc.SetDataType(DT_FLOAT);
  std::vector<int64_t> dims = {1, 2, 3, 4};

  ASSERT_EQ(handler.UpdateOutputShapeAndType(0, output_desc, dims), SUCCESS);
}

TEST_F(ProgramGeneratorUt, UpdateOutputShapeAndTypeWithDims_IndexOutOfRange) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc output_desc;
  output_desc.SetDataType(DT_FLOAT);
  std::vector<int64_t> dims = {1, 2, 3, 4};

  ASSERT_EQ(handler.UpdateOutputShapeAndType(1, output_desc, dims), ACL_ERROR_GE_INTERNAL_ERROR);
  ASSERT_EQ(handler.UpdateOutputShapeAndType(2, output_desc, dims), ACL_ERROR_GE_INTERNAL_ERROR);
}

TEST_F(ProgramGeneratorUt, UpdateSessionInfo_Success) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  uint64_t session_id = 12345;
  uint64_t kernel_id = 67890;
  bool sess_flag = true;

  ASSERT_EQ(handler.UpdateSessionInfo(session_id, kernel_id, sess_flag), SUCCESS);
}

TEST_F(ProgramGeneratorUt, UpdateSessionInfo_NoSessionInfo) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfoWithWorkSpace();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  uint64_t session_id = 12345;
  uint64_t kernel_id = 67890;
  bool sess_flag = true;

  ASSERT_EQ(handler.UpdateSessionInfo(session_id, kernel_id, sess_flag), SUCCESS);
}

TEST_F(ProgramGeneratorUt, UpdateEventId_Success) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfoWithAsyncWait();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  uint32_t event_id = 100;

  ASSERT_EQ(handler.UpdateEventId(event_id), SUCCESS);
}

TEST_F(ProgramGeneratorUt, UpdateEventId_NoAsyncWait) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  uint32_t event_id = 100;

  ASSERT_EQ(handler.UpdateEventId(event_id), FAILED);
}

TEST_F(ProgramGeneratorUt, GetOutputShapeAndType_Success) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc output_desc;
  output_desc.SetShape(GeShape({1, 2, 3, 4}));
  output_desc.SetDataType(DT_FLOAT);
  ASSERT_EQ(handler.UpdateOutputShapeAndType(0, output_desc), SUCCESS);

  GeShape shape;
  DataType data_type;
  ASSERT_EQ(handler.GetOutputShapeAndType(0, shape, data_type), SUCCESS);
  EXPECT_EQ(shape.GetDims(), std::vector<int64_t>({1, 2, 3, 4}));
  EXPECT_EQ(data_type, DT_FLOAT);
}

TEST_F(ProgramGeneratorUt, GetOutputShapeAndType_IndexOutOfRange) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeShape shape;
  DataType data_type;
  ASSERT_EQ(handler.GetOutputShapeAndType(1, shape, data_type), ACL_ERROR_GE_INTERNAL_ERROR);
  ASSERT_EQ(handler.GetOutputShapeAndType(2, shape, data_type), ACL_ERROR_GE_INTERNAL_ERROR);
}

TEST_F(ProgramGeneratorUt, UpdateShapeAndType_DimsOverMax) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, static_cast<UnknowShapeOpType>(0));
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc input_desc;
  std::vector<int64_t> dims;
  for (size_t i = 0; i <= aicpu::FWKAdapter::kMaxShapeDims + 1; ++i) {
    dims.push_back(static_cast<int64_t>(i));
  }
  input_desc.SetDataType(DT_FLOAT);

  ASSERT_EQ(handler.UpdateInputShapeAndType(0, input_desc, dims), ACL_ERROR_GE_PARAM_INVALID);
}

TEST_F(ProgramGeneratorUt, UpdateOutputShapeAndType_DependShapeRange_Success) {
  om2::Om2AicpuExtInfoHandler handler("test_node", 2, 1, DEPEND_SHAPE_RANGE);
  std::string ext_info = GetFakeExtInfo();
  ASSERT_EQ(handler.Parse(ext_info), SUCCESS);

  GeTensorDesc output_desc;
  output_desc.SetShape(GeShape({-1, 2, -1, 4}));
  output_desc.SetDataType(DT_FLOAT);
  std::vector<std::pair<int64_t, int64_t>> range = {{1, 10}, {2, 2}, {1, 20}, {4, 4}};
  output_desc.SetShapeRange(range);

  ASSERT_EQ(handler.UpdateOutputShapeAndType(0, output_desc), SUCCESS);

  GeShape shape;
  DataType data_type;
  ASSERT_EQ(handler.GetOutputShapeAndType(0, shape, data_type), SUCCESS);
  EXPECT_EQ(shape.GetDim(0), 10);
  EXPECT_EQ(shape.GetDim(2), 20);
}

GeRootModelPtr CreateGeRootModelWithAllKernelOp() {
  auto graph = gert::ShareGraph::AicoreStaticGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model = builder
                           .AddTaskDef("add1", gert::AiCoreTaskDefFaker("add_stub")
                                                   .WithHandle()
                                                   .ArgsFormat("{i_instance0*}{i_instance1*}{o_instance0*}{ws0*}"))
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
      if (op_desc->GetName() == "add1") {
        (void)AttrUtils::SetStr(op_desc, "_kernelname", "add_stub");
      }
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  auto *model_task_def = ge_model->GetModelTaskDefPtr().get();
  if (model_task_def != nullptr) {
    for (int32_t i = 0; i < model_task_def->task_size(); ++i) {
      auto *task_def = model_task_def->mutable_task(i);
      if ((task_def != nullptr) && task_def->has_kernel_with_handle()) {
        task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_ALL_KERNEL));
        auto *kernel_with_handle = task_def->mutable_kernel_with_handle();
        auto *context = kernel_with_handle->mutable_context();
        context->set_kernel_type(static_cast<uint32_t>(ccKernelType::TE));
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

TEST_F(ProgramGeneratorUt, GenerateProgram_AllKernel_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAllKernelOp();
  ASSERT_NE(ge_root_model, nullptr);
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  for (const auto file_index : {GeneratedFileIndex::kKernelRegistryFile, GeneratedFileIndex::kResourcesFile,
                                GeneratedFileIndex::kArgsManagerFile, GeneratedFileIndex::kLoadingAndRunningFile,
                                GeneratedFileIndex::kInterfaceHeaderFile, GeneratedFileIndex::kCMakeListsFile}) {
    ASSERT_NE(outputs.find(file_index), outputs.end());
    ASSERT_FALSE(outputs[file_index].empty());
  }

  const auto &kernel_reg_source = outputs[GeneratedFileIndex::kKernelRegistryFile];
  EXPECT_NE(kernel_reg_source.find("ACL_RT_BINARY_MAGIC_ELF_VECTOR_CORE"), std::string::npos);
}

// Creates a model with an AICore op that has a separately-clean atomic task.
// This exercises the is_separately_clean_task_ path in KernelTaskCodeBuilder,
// where func_handle_key is built from ATOMIC_ATTR_TBE_KERNEL_NAME + "_atomic".
GeRootModelPtr CreateGeRootModelWithSeparatelyCleanAicoreOp() {
  auto graph = gert::ShareGraph::AicoreStaticGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  auto ge_root_model = builder
                           .AddTaskDef("Add", gert::AiCoreTaskDefFaker("add_stub")
                                                  .AtomicStubNum("add_atomic_stub")
                                                  .ArgsFormat("{i_instance0*}{i_instance1*}{o_instance0*}{ws0*}"))
                           .FakeTbeBin({gert::GeModelBuilder::TbeConfig("Add", true)})
                           .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();

  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      continue;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else if (op_desc->GetType() == "Add") {
      op_desc->AppendIrInput("x1", kIrInputRequired);
      op_desc->AppendIrInput("x2", kIrInputRequired);
      op_desc->AppendIrOutput("y", kIrOutputRequired);
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      op_desc->SetWorkspaceBytes(std::vector<int64_t>(1, 64));
      op_desc->SetWorkspace(std::vector<int64_t>(1, 0));
      // Enable IsNeedAtomicCleanTask -> true
      (void)AttrUtils::SetBool(op_desc, ATTR_NAME_NEED_GENTASK_ATOMIC, true);
    } else if (op_desc->GetType() == "Reshape") {
      op_desc->AppendIrInput("x", kIrInputRequired);
      op_desc->AppendIrInput("shape", kIrInputRequired);
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;

  // Make <op_name>_atomic_kernelname match _kernelname so IsSeparatelyCleanTask returns true.
  // SyncKernelNameFromOpDesc sets all task_defs' kernel_name to _kernelname,
  // so the attribute checked by IsSeparatelyCleanTask must equal _kernelname.
  auto sync_atomic_kernelname = [](const ComputeGraphPtr &g) {
    for (const auto &node : g->GetDirectNode()) {
      auto op_desc = node->GetOpDesc();
      if ((op_desc != nullptr) && (op_desc->GetType() == "Add")) {
        std::string kernel_name;
        if (AttrUtils::GetStr(op_desc, "_kernelname", kernel_name)) {
          (void)AttrUtils::SetStr(op_desc, op_desc->GetName() + "_atomic_kernelname", kernel_name);
        }
      }
    }
  };
  sync_atomic_kernelname(compute_graph);
  const auto model_graph = ge_model->GetGraph();
  if (model_graph != nullptr) {
    sync_atomic_kernelname(model_graph);
  }

  // HACK: GetMagic uses kAtomicPrefix + TVM_ATTR_NAME_MAGIC = "_atomictvm_magic"
  // (no underscore) to look up the atomic magic attribute, but FakeTbeBinToNodes
  // sets ATOMIC_ATTR_TVM_MAGIC = "_atomic_tvm_magic" (with underscore).
  // Set both keys so the atomic kernel binary registration succeeds.
  auto sync_atomic_tvm_magic = [](const ComputeGraphPtr &g) {
    for (const auto &node : g->GetDirectNode()) {
      auto op_desc = node->GetOpDesc();
      if ((op_desc != nullptr) && (op_desc->GetType() == "Add")) {
        (void)AttrUtils::SetStr(op_desc, "_atomictvm_magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC");
      }
    }
  };
  sync_atomic_tvm_magic(compute_graph);
  if (model_graph != nullptr) {
    sync_atomic_tvm_magic(model_graph);
  }

  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);

  return ge_root_model;
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSourceForSeparatelyCleanTask_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithSeparatelyCleanAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  // Verify all expected files are generated
  for (const auto file_index : {GeneratedFileIndex::kKernelRegistryFile, GeneratedFileIndex::kResourcesFile,
                                GeneratedFileIndex::kArgsManagerFile, GeneratedFileIndex::kLoadingAndRunningFile,
                                GeneratedFileIndex::kInterfaceHeaderFile, GeneratedFileIndex::kCMakeListsFile}) {
    ASSERT_NE(outputs.find(file_index), outputs.end());
    ASSERT_FALSE(outputs[file_index].empty());
  }

  // The kernel registry should contain the atomic kernel entry (with "_atomic" suffix)
  const auto &kernel_reg_source = outputs[GeneratedFileIndex::kKernelRegistryFile];
  EXPECT_NE(kernel_reg_source.find("_atomic"), std::string::npos);

  // The load_and_run source should be generated (func_handle_key resolved via ATOMIC_ATTR_TBE_KERNEL_NAME)
  const auto &load_run = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  EXPECT_NE(load_run.find("KernelTaskDistribute"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoggingFunctionality_Ok) {
  GeRootModelPtr ge_root_model = CreateGeRootModelWithAicoreOp();
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  // Interface header: logging includes
  const auto &interface_header = outputs[GeneratedFileIndex::kInterfaceHeaderFile];
  EXPECT_NE(interface_header.find("#include \"dlog_pub.h\""), std::string::npos);
  EXPECT_EQ(interface_header.find("#line 1"), std::string::npos) << ".h file should not have #line directive";

  // Interface header: logging macros
  EXPECT_NE(interface_header.find("#define OM2_LOGD"), std::string::npos);
  EXPECT_NE(interface_header.find("#define OM2_LOGI"), std::string::npos);
  EXPECT_NE(interface_header.find("#define OM2_LOGW"), std::string::npos);
  EXPECT_NE(interface_header.find("#define OM2_LOGE"), std::string::npos);

  // Interface header: CHK macros contain OM2_LOGE
  EXPECT_NE(interface_header.find("OM2_CHK_STATUS"), std::string::npos);
  EXPECT_NE(interface_header.find("OM2_LOGE"), std::string::npos);

  // Resources source: lifecycle logging + #line directive
  const auto &resources_source = outputs[GeneratedFileIndex::kResourcesFile];
  EXPECT_NE(resources_source.find("#line 1"), std::string::npos);
  EXPECT_NE(resources_source.find("Om2Model created"), std::string::npos);
  EXPECT_NE(resources_source.find("~Om2Model"), std::string::npos);
  EXPECT_NE(resources_source.find("InitResources begin"), std::string::npos);
  EXPECT_NE(resources_source.find("InitResources done"), std::string::npos);
  EXPECT_NE(resources_source.find("ReleaseResources begin"), std::string::npos);
  EXPECT_NE(resources_source.find("ReleaseResources done"), std::string::npos);

  // Kernel registry: logging + #line directive
  const auto &kernel_reg_source = outputs[GeneratedFileIndex::kKernelRegistryFile];
  EXPECT_NE(kernel_reg_source.find("#line 1"), std::string::npos);
  EXPECT_NE(kernel_reg_source.find("RegisterKernels begin"), std::string::npos);
  EXPECT_NE(kernel_reg_source.find("RegisterKernels done"), std::string::npos);

  // Load and run: logging + #line directive
  const auto &load_and_run_source = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  EXPECT_NE(load_and_run_source.find("#line 1"), std::string::npos);
  EXPECT_NE(load_and_run_source.find("Load begin"), std::string::npos);
  EXPECT_NE(load_and_run_source.find("Load done"), std::string::npos);
  EXPECT_NE(load_and_run_source.find("Run begin"), std::string::npos);
  EXPECT_NE(load_and_run_source.find("Run done"), std::string::npos);
  EXPECT_NE(load_and_run_source.find("RunAsync begin"), std::string::npos);
  EXPECT_NE(load_and_run_source.find("RunAsync done"), std::string::npos);

  // Makefile: include path for dlog_pub.h
  const auto &makefile = outputs[GeneratedFileIndex::kCMakeListsFile];
  EXPECT_NE(makefile.find("pkg_inc/base"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSource_NoTaskConcatOutput_Ok) {
  auto ge_root_model = CreateGeRootModelWithNoTaskConcatOutput();
  ASSERT_NE(ge_root_model, nullptr);
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_and_run_source = outputs[GeneratedFileIndex::kLoadingAndRunningFile];
  const auto &interface_source = outputs[GeneratedFileIndex::kInterfaceHeaderFile];

  // 每个 Run/RunAsync 作用域只声明一次输出 tensor。
  size_t first_decl =
      load_and_run_source.find("auto output_data_0_tensor = reinterpret_cast<gert::Tensor *>(output_data[0])");
  EXPECT_NE(first_decl, std::string::npos);
  // 第二次声明来自另一个方法，不应出现第三次。
  size_t second_decl = load_and_run_source.find("auto output_data_0_tensor", first_decl + 1);
  EXPECT_NE(second_decl, std::string::npos);  // 另一个方法中的声明
  size_t third_decl = load_and_run_source.find("auto output_data_0_tensor", second_decl + 1);
  EXPECT_EQ(third_decl, std::string::npos);  // 无第三次声明

  // 首段直接刷新输出地址。
  EXPECT_NE(load_and_run_source.find(
                "args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr()))"),
            std::string::npos);

  // 第二段刷新输出地址偏移。
  EXPECT_NE(load_and_run_source.find(
                "args_table_.UpdateHostArgs(3, (reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr()) + 512))"),
            std::string::npos);

  // 输出个数保持真实模型输出数。
  EXPECT_NE(interface_source.find("OUTPUT_NUM = 1"), std::string::npos);
  EXPECT_NE(interface_source.find("INPUT_NUM = 2"), std::string::npos);
}

TEST_F(ProgramGeneratorUt, GenerateLoadAndRunSource_NoTaskConcatReuseDimOne_CopyOnly) {
  auto ge_root_model = CreateGeRootModelWithNoTaskConcatOutputReuseDimOne();
  ASSERT_NE(ge_root_model, nullptr);
  auto generator = CreateProgramGenerator(ge_root_model);
  std::map<GeneratedFileIndex, std::string> outputs;
  ASSERT_EQ(GenerateProgramFiles(generator, outputs), SUCCESS);

  const auto &load_and_run_source = outputs[GeneratedFileIndex::kLoadingAndRunningFile];

  EXPECT_EQ(load_and_run_source.find(
                "args_table_.UpdateHostArgs(2, reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr()))"),
            std::string::npos);
  EXPECT_EQ(load_and_run_source.find(
                "args_table_.UpdateHostArgs(3, (reinterpret_cast<uintptr_t>(output_data_0_tensor->GetAddr()) + 512))"),
            std::string::npos);

  const std::string execute_call = "aclmdlRIExecute(model_handle_, stream_sync_timeout)";
  const std::string copy_call =
      "aclrtMemcpy(output_data_0_tensor->GetAddr(), output_data_0_tensor->GetSize(), "
      "dev_output0_ptr, output_data_0_tensor->GetSize(), ACL_MEMCPY_DEVICE_TO_DEVICE)";
  const auto execute_pos = load_and_run_source.find(execute_call);
  const auto copy_pos = load_and_run_source.find(copy_call);
  EXPECT_NE(execute_pos, std::string::npos);
  EXPECT_NE(copy_pos, std::string::npos);
  EXPECT_LT(execute_pos, copy_pos);
}

}  // namespace ge
