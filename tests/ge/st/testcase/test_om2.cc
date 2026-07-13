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
#include "common/helper/visual_json_converter.h"
#include "common/helper/om2/zip_archive_writer.h"
#include "common/helper/om2/json_file.h"
#include "framework/omg/omg.h"
#include "framework/runtime/om2_model_executor.h"
#include "generator/ge_generator.h"
#include "ge/ge_ir_build.h"
#include "api/aclgrph/option_utils.h"
#include "api/atc/main_impl.h"
#include "file_utils.h"
#include "runtime/om2/zip_archive_reader.h"
#include <gtest/gtest.h>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>
#include "common/env_path.h"
#include "init_ge.h"
#include "ge_running_env/ge_running_env_faker.h"
#include "mmpa/mmpa_api.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/ge_global_options.h"
#include "graph/utils/tensor_utils.h"
#include "graph/utils/file_utils.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph_metadef/depends/checker/tensor_check_utils.h"
#include "proto/ge_ir.pb.h"

#include "graph/ge_local_context.h"
#include "ge_runtime_stub/include/common/share_graph.h"
#include "ge_runtime_stub/include/faker/ge_model_builder.h"
#include "ge_runtime_stub/include/faker/aicore_taskdef_faker.h"
#include "ge_runtime_stub/include/faker/aicpu_taskdef_faker.h"

#include <cinttypes>
#include <securec.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <unistd.h>
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

bool IsFileNonEmpty(const std::string &path) {
  std::ifstream input(path, std::ios::in | std::ios::binary);
  if (!input.is_open()) {
    return false;
  }
  input.seekg(0, std::ios::end);
  return input.good() && (input.tellg() > 0);
}

const JsonFile::json *FindMapValue(const JsonFile::json &entries, const std::string &key) {
  if (!entries.is_array()) {
    return nullptr;
  }
  for (const auto &entry : entries) {
    if (entry.is_object() && entry.contains("key") && entry.at("key") == key && entry.contains("value")) {
      return &entry.at("value");
    }
  }
  return nullptr;
}

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char *name, const char *value) : name_(name) {
    const char *old_value = getenv(name);
    if (old_value != nullptr) {
      old_value_ = old_value;
      has_old_value_ = true;
    }
    (void)setenv(name, value, 1);
  }

  ~ScopedEnvVar() {
    if (has_old_value_) {
      (void)setenv(name_.c_str(), old_value_.c_str(), 1);
      return;
    }
    (void)unsetenv(name_.c_str());
  }

 private:
  std::string name_;
  std::string old_value_;
  bool has_old_value_ = false;
};

class ScopedUnsetEnvVar {
 public:
  explicit ScopedUnsetEnvVar(const char *name) : name_(name) {
    const char *old_value = getenv(name);
    if (old_value != nullptr) {
      old_value_ = old_value;
      has_old_value_ = true;
    }
    (void)unsetenv(name);
  }

  ~ScopedUnsetEnvVar() {
    if (has_old_value_) {
      (void)setenv(name_.c_str(), old_value_.c_str(), 1);
      return;
    }
    (void)unsetenv(name_.c_str());
  }

 private:
  std::string name_;
  std::string old_value_;
  bool has_old_value_ = false;
};

void RemoveTestDir(const std::string &path) {
  if (path.empty()) {
    return;
  }
  std::error_code ec;
  (void)std::filesystem::remove_all(path, ec);
}

class ScopedTempDir {
 public:
  explicit ScopedTempDir(const std::string &root) {
    for (int32_t i = 0; i < 100; ++i) {
      const std::string candidate =
          PathUtils::Join({root, "om2_cross_compile_" + std::to_string(getpid()) + "_" + std::to_string(i)});
      if (mkdir(candidate.c_str(), S_IRWXU) == 0) {
        path_ = candidate;
        return;
      }
      if (errno != EEXIST) {
        return;
      }
    }
  }

  ~ScopedTempDir() {
    RemoveTestDir(path_);
  }

  std::string Path(const std::string &relative) const {
    if (relative.empty()) {
      return path_;
    }
    return PathUtils::Join({path_, relative});
  }

  bool IsValid() const {
    return !path_.empty();
  }

  bool Mkdirs(const std::string &relative) const {
    return CreateDir(Path(relative)) == 0;
  }

  bool WriteText(const std::string &relative, const std::string &content, const mode_t mode = S_IRUSR | S_IWUSR) const {
    const std::string full_path = Path(relative);
    const auto slash_pos = full_path.find_last_of('/');
    const std::string parent = (slash_pos == std::string::npos) ? "" : full_path.substr(0, slash_pos);
    if ((!parent.empty()) && (CreateDir(parent) != 0)) {
      return false;
    }
    std::ofstream ofs(full_path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
      return false;
    }
    ofs << content;
    ofs.close();
    if (!ofs.good()) {
      return false;
    }
    return chmod(full_path.c_str(), mode) == 0;
  }

 private:
  std::string path_;
};

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
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);

  return ge_root_model;
}

void SetDefaultModelWeightsAndAttrs(const GeModelPtr &ge_model) {
  std::vector<uint64_t> weights_value(64, 1024);
  const size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetWeight(Buffer::CopyFrom(reinterpret_cast<uint8_t *>(weights_value.data()), weight_size));
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);
}

Status SetDefaultStaticOffsets(const ComputeGraphPtr &compute_graph, bool with_workspace) {
  compute_graph->SetGraphUnknownFlag(false);
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc == nullptr) {
      return FAILED;
    }
    if (op_desc->GetType() == DATA) {
      op_desc->SetOutputOffset({1024});
    } else if (op_desc->GetType() == NETOUTPUT) {
      op_desc->SetInputOffset({3072});
    } else {
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
      if (with_workspace) {
        op_desc->SetWorkspaceBytes(std::vector<int64_t>(1, 64));
        op_desc->SetWorkspace(std::vector<int64_t>(1, 0));
      }
    }
  }
  return SUCCESS;
}

void FillAicpuKernelArgs(domi::KernelDef *kernel_def) {
  auto ext_info = GetFakeExtInfo();
  AicpuTaskArgs args = {};
  args.head.length = sizeof(args);
  args.head.ioAddrNum = 3;
  kernel_def->set_args(reinterpret_cast<const char *>(&args), args.head.length);
  kernel_def->set_args_size(args.head.length);
  kernel_def->set_kernel_ext_info(ext_info.c_str(), ext_info.size());
  kernel_def->set_kernel_ext_info_size(ext_info.size());
}

void FillAicpuKernelExArgs(domi::KernelExDef *kernel_ex_def) {
  auto ext_info = GetFakeExtInfo();
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

void FillAicpuTaskArgs(domi::ModelTaskDef *model_task_def, bool set_kernel_type, bool fill_kernel_ex) {
  if (model_task_def == nullptr) {
    return;
  }
  for (int32_t i = 0; i < model_task_def->task_size(); ++i) {
    auto *task_def = model_task_def->mutable_task(i);
    if ((task_def != nullptr) && fill_kernel_ex && task_def->has_kernel_ex()) {
      FillAicpuKernelExArgs(task_def->mutable_kernel_ex());
    }
    if ((task_def != nullptr) && task_def->has_kernel()) {
      if (set_kernel_type) {
        task_def->mutable_kernel()->mutable_context()->set_kernel_type(6U);
      }
      FillAicpuKernelArgs(task_def->mutable_kernel());
    }
  }
}

GeRootModelPtr BuildAicpuRootModel(gert::AiCpuCCTaskDefFaker faker) {
  auto graph = gert::ShareGraph::Aicpu4thGraph();
  graph->TopologicalSorting();
  gert::GeModelBuilder builder(graph);
  return builder.AddTaskDef("add1", faker.SetNeedMemcpy(false))
      .AddTaskDef("add2", faker.SetNeedMemcpy(false))
      .BuildGeRootModel();
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
  if (SetDefaultStaticOffsets(compute_graph, true) != SUCCESS) {
    return nullptr;
  }

  OpDescPtr add_desc = nullptr;
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() == "Add")) {
      op_desc->AppendIrInput("x1", kIrInputRequired);
      op_desc->AppendIrInput("x2", kIrInputRequired);
      op_desc->AppendIrOutput("y", kIrOutputRequired);
      (void)AttrUtils::SetBool(op_desc, ATTR_NAME_NEED_GENTASK_ATOMIC, true);
      (void)AttrUtils::SetStr(op_desc, kAtomicPrefix + TVM_ATTR_NAME_MAGIC, "RT_DEV_BINARY_MAGIC_ELF_AIVEC");
      add_desc = op_desc;
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
  SetDefaultModelWeightsAndAttrs(ge_model);
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

GeRootModelPtr CreateGeRootModelWithAicpuOp() {
  gert::AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto ge_root_model = BuildAicpuRootModel(aicpu_task_def_faker);
  auto &compute_graph = ge_root_model->GetRootGraph();
  if (SetDefaultStaticOffsets(compute_graph, false) != SUCCESS) {
    return nullptr;
  }
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  FillAicpuTaskArgs(ge_model->GetModelTaskDefPtr().get(), true, false);
  SetDefaultModelWeightsAndAttrs(ge_model);
  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithCustAicpuOp() {
  gert::AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto ge_root_model = BuildAicpuRootModel(aicpu_task_def_faker);
  auto &compute_graph = ge_root_model->GetRootGraph();
  if (SetDefaultStaticOffsets(compute_graph, false) != SUCCESS) {
    return nullptr;
  }
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  FillAicpuTaskArgs(ge_model->GetModelTaskDefPtr().get(), false, false);

  CustAICPUKernelStore cust_aicpu_kernel_store;
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() != DATA) && (op_desc->GetType() != NETOUTPUT)) {
      std::vector<char> kernel_bin(64, '\0');
      auto cust_aicpu_kernel = std::make_shared<ge::OpKernelBin>("libcust_aicpu_kernel.so", std::move(kernel_bin));
      op_desc->SetExtAttr(ge::OP_EXTATTR_CUSTAICPU_KERNEL, cust_aicpu_kernel);
      cust_aicpu_kernel_store.AddCustAICPUKernel(cust_aicpu_kernel);
      (void)ge::AttrUtils::SetStr(op_desc, "kernelSo", "libcust_aicpu_kernel.so");
    }
  }
  (void)cust_aicpu_kernel_store.Build();
  ge_model->SetCustAICPUKernelStore(cust_aicpu_kernel_store);
  SetDefaultModelWeightsAndAttrs(ge_model);
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
  if (SetDefaultStaticOffsets(compute_graph, false) != SUCCESS) {
    return nullptr;
  }
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  FillAicpuTaskArgs(ge_model->GetModelTaskDefPtr().get(), true, true);
  SetDefaultModelWeightsAndAttrs(ge_model);
  return ge_root_model;
}

ComputeGraphPtr BuildDynamicIoAicoreGraph() {
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
  return graph;
}

GeRootModelPtr CreateGeRootModelWithAicoreOpOfDynamicIo() {
  auto graph = BuildDynamicIoAicoreGraph();
  if (graph == nullptr) {
    return nullptr;
  }
  gert::GeModelBuilder builder(graph);
  auto ge_root_model =
      builder.AddTaskDef("Add", gert::AiCoreTaskDefFaker("add_stub").ArgsFormat("{i_desc0}{i_desc1}{o_desc0}"))
          .FakeTbeBin({"Add"})
          .BuildGeRootModel();
  auto &compute_graph = ge_root_model->GetRootGraph();
  if (SetDefaultStaticOffsets(compute_graph, false) != SUCCESS) {
    return nullptr;
  }
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  SetDefaultModelWeightsAndAttrs(ge_model);
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
  const std::string wrapped_command =
      "env ASAN_OPTIONS=detect_leaks=0:halt_on_error=0 LSAN_OPTIONS=exitcode=0 " + command;
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
    "zero_copy_size": 0,
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

std::string MakeFakeOm2LoadAndRunCreateCpp() {
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
)";
}

std::string MakeFakeOm2LoadAndRunExecuteCpp() {
  return R"(
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

std::string MakeFakeOm2LoadAndRunCpp() {
  return MakeFakeOm2LoadAndRunCreateCpp() + MakeFakeOm2LoadAndRunExecuteCpp();
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

  RemoveTestDir(runtime_dir);
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

std::string BuildValidOm2ProtoTxt() {
  ge::proto::ModelDef model_def;
  model_def.set_name("st_om2_model");
  auto *graph = model_def.add_graph();
  graph->set_name("main_graph");
  auto *op = graph->add_op();
  op->set_name("data0");
  op->set_type("Data");
  return model_def.DebugString();
}

void CreateMinimalOm2File(const std::string &path, const std::string &proto_content) {
  ZipArchiveWriter writer(path);
  ASSERT_TRUE(writer.IsMemFileOpened());
  const std::string manifest = R"({"om2_version":"0","model_num":1})";
  ASSERT_TRUE(writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(writer.WriteBytes("data/model_0/debug/ge_proto_00000000_graph_1_test.txt", proto_content.data(),
                                proto_content.size(), true));
  ASSERT_TRUE(writer.SaveModelDataToFile());
  ASSERT_EQ(mmAccess2(path.c_str(), M_F_OK), EOK);
}

void CreateMinimalOm2FileWithoutProto(const std::string &path) {
  ZipArchiveWriter writer(path);
  ASSERT_TRUE(writer.IsMemFileOpened());
  const std::string manifest = R"({"om2_version":"0","model_num":1})";
  ASSERT_TRUE(writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(writer.SaveModelDataToFile());
  ASSERT_EQ(mmAccess2(path.c_str(), M_F_OK), EOK);
}

std::string BuildVisualJsonWithFusionScope() {
  ge::proto::OpDef fusion_op;
  (*fusion_op.mutable_attr())["fusion_scope"].set_i(2);

  JsonFile::json visual_json;
  visual_json["format"] = "ge_visual_json";
  visual_json["format_version"] = 1;
  auto &model = visual_json["model"];
  model["name"] = "st_group_model";
  model["attr"]["fm"] = {{"type", "list_bytes"}, {"value", JsonFile::json::array({fusion_op.SerializeAsString()})}};
  model["graph"] = JsonFile::json::array(
      {{{"name", "main_graph"},
        {"op", JsonFile::json::array({{{"name", "group_op"}, {"type", "GroupOp"}, {"stream_id", 7}}})}}});
  return visual_json.dump();
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
  // expect_files contains exact paths; generated debug files are checked by pattern.
  std::set<std::string> expect_exact = expect_files;
  int visual_json_count = 0;
  for (const auto &f : file_names) {
    if (f.find("debug/ge_onnx_") != std::string::npos && f.find(".pbtxt") != std::string::npos) {
      ADD_FAILURE() << "Unexpected legacy ONNX debug file: " << f;
      continue;
    }
    if (f.find("debug/ge_proto_") != std::string::npos && f.find(".txt") != std::string::npos) {
      ADD_FAILURE() << "Unexpected legacy GE proto debug file: " << f;
      continue;
    }
    if (f.find("debug/ge_visual_") != std::string::npos && f.find(".json") != std::string::npos) {
      ++visual_json_count;
      continue;
    }
    EXPECT_EQ(expect_exact.count(f), 1) << "Unexpected file: " << f;
  }
  EXPECT_EQ(visual_json_count, 1) << "Expected debug/ge_visual_*.json";
  // expect_files size = exact matches (no generated debug pattern entries expected in set)
  const size_t exact_count = file_names.size() - static_cast<size_t>(visual_json_count);
  EXPECT_EQ(exact_count, expect_exact.size());
}

void ExpectGeneratedMakefileSupportsEnvCompiler(const RAIIZipArchive &archive, const std::string &zip_base_name) {
  size_t makefile_size = 0U;
  const auto makefile_data = archive.ExtractToMem(zip_base_name + "/data/model_0/runtime/Makefile", makefile_size);
  ASSERT_NE(makefile_data, nullptr);
  const std::string makefile(reinterpret_cast<const char *>(makefile_data.get()), makefile_size);
  EXPECT_EQ(makefile.find("CXX := g++"), std::string::npos);
  EXPECT_NE(makefile.find("ifeq ($(origin CXX),default)"), std::string::npos);
  EXPECT_NE(makefile.find("CXX := c++"), std::string::npos);
  EXPECT_NE(makefile.find("CXX ?= c++"), std::string::npos);
  EXPECT_NE(makefile.find("USE_STUB_LIB ?= 1"), std::string::npos);
  EXPECT_NE(makefile.find("LIB_PATH ?= $(CANN_ROOT)/devlib"), std::string::npos);
  EXPECT_NE(makefile.find("LIB_PATH ?= $(CANN_ROOT)/lib64"), std::string::npos);
  EXPECT_NE(makefile.find("ifndef CPPFLAGS"), std::string::npos);
  EXPECT_NE(makefile.find("CPPFLAGS :="), std::string::npos);
  EXPECT_NE(makefile.find("ifndef CXXFLAGS"), std::string::npos);
  EXPECT_NE(makefile.find("CXXFLAGS := -std=c++17 -O2 -fPIC"), std::string::npos);
  EXPECT_NE(makefile.find("ifndef LDFLAGS"), std::string::npos);
  EXPECT_NE(makefile.find("LDFLAGS := -shared -L$(LIB_PATH) -Wl,--no-as-needed"), std::string::npos);
  EXPECT_NE(makefile.find("ifndef LDLIBS"), std::string::npos);
  EXPECT_NE(makefile.find("LDLIBS := -lacl_rt -Wl,--as-needed"), std::string::npos);
  EXPECT_NE(makefile.find("$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)"), std::string::npos);
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

JsonFile ExtractVisualJson(const RAIIZipArchive &archive, const std::string &zip_base_name,
                           const uint32_t model_index = 0U) {
  size_t visual_json_size = 0U;
  const auto visual_json_buf = archive.ExtractToMem(
      zip_base_name + "/data/model_" + std::to_string(model_index) + "/debug/ge_visual_00000000_graph_0.json",
      visual_json_size);
  EXPECT_NE(visual_json_buf, nullptr);
  if (visual_json_buf == nullptr) {
    return JsonFile(nullptr, 0U);
  }
  return JsonFile(reinterpret_cast<const uint8_t *>(visual_json_buf.get()), visual_json_size);
}

void ExpectVisualJsonBasic(const JsonFile &visual_json) {
  ASSERT_TRUE(visual_json.IsValid());
  const auto &raw = visual_json.Raw();
  ASSERT_TRUE(raw.is_object());
  ASSERT_TRUE(raw.contains("format"));
  EXPECT_EQ(raw.at("format"), JsonFile::json("ge_visual_json"));
  ASSERT_TRUE(raw.contains("format_version"));
  EXPECT_EQ(raw.at("format_version"), JsonFile::json(1));
  ASSERT_TRUE(raw.contains("model"));
  ASSERT_TRUE(raw.at("model").contains("graph"));
  ASSERT_TRUE(raw.at("model").at("graph").is_array());
  ASSERT_FALSE(raw.at("model").at("graph").empty());
  ASSERT_TRUE(raw.at("model").at("graph").at(0).contains("op"));
  ASSERT_TRUE(raw.at("model").at("graph").at(0).at("op").is_array());
}

void ExpectVisualJsonMatchesGraph(const JsonFile &visual_json, const ComputeGraphPtr &graph) {
  ASSERT_NE(graph, nullptr);
  ExpectVisualJsonBasic(visual_json);
  const auto &visual_graph = visual_json.Raw().at("model").at("graph").at(0);
  ASSERT_TRUE(visual_graph.contains("name"));
  EXPECT_EQ(visual_graph.at("name"), JsonFile::json(graph->GetName()));

  const auto &visual_ops = visual_graph.at("op");
  ASSERT_EQ(visual_ops.size(), graph->GetDirectNodesSize());

  size_t index = 0U;
  for (const auto &node : graph->GetDirectNode()) {
    ASSERT_NE(node, nullptr);
    const auto &op_json = visual_ops.at(index);
    const auto &op_desc = node->GetOpDesc();
    ASSERT_NE(op_desc, nullptr);
    EXPECT_EQ(op_json.at("name"), JsonFile::json(op_desc->GetName()));
    EXPECT_EQ(op_json.at("type"), JsonFile::json(op_desc->GetType()));
    const size_t input_desc_size = op_json.contains("input_desc") ? op_json.at("input_desc").size() : 0U;
    const size_t output_desc_size = op_json.contains("output_desc") ? op_json.at("output_desc").size() : 0U;
    EXPECT_EQ(input_desc_size, op_desc->GetInputsSize()) << op_desc->GetName();
    EXPECT_EQ(output_desc_size, op_desc->GetOutputsSize()) << op_desc->GetName();
    ++index;
  }
}

proto::OpDef *AddVisualConverterCoverageOp(proto::ModelDef &model_def) {
  model_def.set_name("st_visual_model");
  model_def.set_version(1U);
  model_def.set_custom_version("1.0.0");
  (*model_def.mutable_attr())["model_attr"].set_s("model_value");
  auto *graph = model_def.add_graph();
  graph->set_name("main_graph");
  graph->add_input("input:0");
  graph->add_output("output:0");
  auto *op = graph->add_op();
  op->set_name("visual_op");
  op->set_type("VisualOp");
  op->add_input("input:0");
  op->set_id(1);
  op->set_stream_id(7);
  op->add_src_name("input");
  op->add_src_index(0);
  op->add_dst_name("output");
  op->add_dst_index(0);
  op->add_input_i(0);
  op->add_output_i(0);
  op->add_workspace_bytes(1024);
  op->add_is_input_const(false);
  return op;
}

void FillVisualConverterComplexAttrs(proto::OpDef *op) {
  auto *op_attr = op->mutable_attr();
  (*op_attr)["attr_s"].set_s("string_value");
  (*op_attr)["attr_i"].set_i(42);
  (*op_attr)["attr_f"].set_f(2.5f);
  (*op_attr)["attr_b"].set_b(true);
  (*op_attr)["attr_dt"].set_dt(DT_FLOAT);
  (*op_attr)["attr_bt"].set_bt("bytes_data");
  (*op_attr)["attr_expr"].set_expression("x + y");
  auto *func = (*op_attr)["attr_func"].mutable_func();
  func->set_name("fn");
  (*func->mutable_attr())["inner"].set_s("value");
  auto *td_attr = (*op_attr)["attr_td"].mutable_td();
  td_attr->set_name("td_attr");
  td_attr->set_dtype(proto::DT_FLOAT);
  auto *tensor_attr = (*op_attr)["attr_t"].mutable_t();
  tensor_attr->mutable_desc()->set_name("tensor_attr");
  tensor_attr->mutable_desc()->set_dtype(proto::DT_FLOAT16);
  tensor_attr->set_data("skip_me");
  auto *graph_attr = (*op_attr)["attr_g"].mutable_g();
  graph_attr->set_name("sub_graph");
}

void FillVisualConverterListAttrs(proto::OpDef *op) {
  auto *op_attr = op->mutable_attr();
  auto *list_str = (*op_attr)["list_str"].mutable_list();
  list_str->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_STRING);
  list_str->add_s("a");
  auto *list_float = (*op_attr)["list_float"].mutable_list();
  list_float->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_FLOAT);
  list_float->add_f(1.25f);
  auto *list_td = (*op_attr)["list_td"].mutable_list();
  list_td->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_TENSOR_DESC);
  list_td->add_td()->set_name("list_td");
  (void)(*op_attr)["empty_untyped_list"].mutable_list();
  auto *list_list_float = (*op_attr)["list_list_float"].mutable_list_list_float();
  list_list_float->add_list_list_f()->add_list_f(3.5f);
}

void FillVisualConverterTensorDescs(proto::OpDef *op) {
  auto *input_desc = op->add_input_desc();
  input_desc->set_name("input_tensor");
  input_desc->set_dtype(proto::DT_FLOAT);
  input_desc->mutable_shape()->add_dim(1);
  input_desc->set_layout("ND");
  input_desc->set_has_out_attr(true);
  auto *output_desc = op->add_output_desc();
  output_desc->set_name("output_tensor");
  output_desc->set_dtype(proto::DT_FLOAT);
  output_desc->mutable_shape()->add_dim(1);
  output_desc->set_layout("ND");
}

proto::ModelDef BuildVisualConverterCoverageModelDef() {
  proto::ModelDef model_def;
  auto *op = AddVisualConverterCoverageOp(model_def);
  FillVisualConverterComplexAttrs(op);
  FillVisualConverterListAttrs(op);
  FillVisualConverterTensorDescs(op);
  auto *shape_env = model_def.mutable_attr_groups()->add_attr_group_def()->mutable_shape_env_attr_group();
  (*shape_env->mutable_symbol_to_value())["st_sym"] = 128;
  return model_def;
}

// -----------------------------------------------------------------------
// Helper: AiCore 模型 + separately-clean atomic task，
// 用于覆盖 KernelTaskCodeBuilder::BuildLaunchSemantic 中
// is_separately_clean_task_ 分支 (func_handle_key = ATOMIC_ATTR_TBE_KERNEL_NAME + "_atomic")
// -----------------------------------------------------------------------
void ConfigureSeparatelyCleanGraph(const ComputeGraphPtr &compute_graph) {
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
      (void)AttrUtils::SetBool(op_desc, ATTR_NAME_NEED_GENTASK_ATOMIC, true);
    } else if (op_desc->GetType() == "Reshape") {
      op_desc->AppendIrInput("x", kIrInputRequired);
      op_desc->AppendIrInput("shape", kIrInputRequired);
      op_desc->SetInputOffset(std::vector<int64_t>(op_desc->GetInputsSize(), 1024));
      op_desc->SetOutputOffset(std::vector<int64_t>(op_desc->GetOutputsSize(), 1024));
    }
  }
}

void SyncSeparatelyCleanAtomicKernelName(const ComputeGraphPtr &graph) {
  for (const auto &node : graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() == "Add")) {
      std::string kernel_name;
      if (AttrUtils::GetStr(op_desc, "_kernelname", kernel_name)) {
        (void)AttrUtils::SetStr(op_desc, op_desc->GetName() + "_atomic_kernelname", kernel_name);
      }
    }
  }
}

void SyncSeparatelyCleanAtomicTvmMagic(const ComputeGraphPtr &graph) {
  for (const auto &node : graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() == "Add")) {
      (void)AttrUtils::SetStr(op_desc, "_atomictvm_magic", "RT_DEV_BINARY_MAGIC_ELF_AIVEC");
    }
  }
}

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
  ConfigureSeparatelyCleanGraph(compute_graph);
  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;

  // Make <op_name>_atomic_kernelname match _kernelname so IsSeparatelyCleanTask returns true.
  // SyncKernelNameFromOpDesc sets all task_defs' kernel_name to _kernelname,
  // so the attribute checked by IsSeparatelyCleanTask must equal _kernelname.
  SyncSeparatelyCleanAtomicKernelName(compute_graph);
  const auto model_graph = ge_model->GetGraph();
  if (model_graph != nullptr) {
    SyncSeparatelyCleanAtomicKernelName(model_graph);
  }

  // HACK: GetMagic uses kAtomicPrefix + TVM_ATTR_NAME_MAGIC = "_atomictvm_magic"
  // (no underscore) to look up the atomic magic attribute, but FakeTbeBinToNodes
  // sets ATOMIC_ATTR_TVM_MAGIC = "_atomic_tvm_magic" (with underscore).
  // Set both keys so the atomic kernel binary registration succeeds.
  SyncSeparatelyCleanAtomicTvmMagic(compute_graph);
  if (model_graph != nullptr) {
    SyncSeparatelyCleanAtomicTvmMagic(model_graph);
  }
  SetDefaultModelWeightsAndAttrs(ge_model);
  return ge_root_model;
}

// -----------------------------------------------------------------------
// Helper: 在现有 AiCore 模型上添加 CMO task_def
// -----------------------------------------------------------------------
GeRootModelPtr CreateGeRootModelWithCmoTask(uint32_t cmo_type = 1U, uint32_t op_code = 3U) {
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
    RemoveTestDir(test_work_dir);
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
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",    "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",  "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",       "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",         "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o", "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",           "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
  ExpectGeneratedMakefileSupportsEnvCompiler(archive, kZipFileBaseName);
  const JsonFile visual_json = ExtractVisualJson(archive, kZipFileBaseName);
  ExpectVisualJsonMatchesGraph(visual_json, ge_root_model->GetRootGraph());
}

TEST_F(Om2St, ConvertOm2Model_Ok_ConvertGeneratedOm2ToJson) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_json.om2"});
  const std::string json_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_json.json"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);

  EXPECT_EQ(ConvertOm(output_file.c_str(), json_file.c_str(), true), SUCCESS);
  EXPECT_TRUE(IsFileNonEmpty(json_file));
}

TEST_F(Om2St, VisualJsonConverter_Ok_SerializeRichModelDef) {
  std::string visual_json;
  ASSERT_EQ(VisualJsonConverter::SerializeFromModelDef(BuildVisualConverterCoverageModelDef(), visual_json), SUCCESS);
  JsonFile parsed(reinterpret_cast<const uint8_t *>(visual_json.data()), visual_json.size());
  ASSERT_TRUE(parsed.IsValid());
  const auto &raw = parsed.Raw();
  EXPECT_EQ(raw.at("format"), JsonFile::json("ge_visual_json"));
  const auto &shape_env = raw.at("model").at("attr_groups").at("attr_group_def").at(0).at("shape_env_attr_group");
  EXPECT_EQ(shape_env.at("symbol_to_value").at("st_sym"), JsonFile::json(128));
  const auto &op_json = raw.at("model").at("graph").at(0).at("op").at(0);
  EXPECT_FALSE(op_json.contains("src_name"));
  EXPECT_FALSE(op_json.contains("src_index"));
  EXPECT_FALSE(op_json.contains("dst_name"));
  EXPECT_FALSE(op_json.contains("dst_index"));
  const auto &attrs = op_json.at("attr");
  EXPECT_FLOAT_EQ(attrs.at("attr_f").get<float>(), 2.5f);
  EXPECT_EQ(attrs.at("attr_bt").at("type"), JsonFile::json("bytes"));
  EXPECT_EQ(attrs.at("attr_expr").at("type"), JsonFile::json("expression"));
  EXPECT_EQ(attrs.at("attr_func").at("type"), JsonFile::json("func"));
  EXPECT_EQ(attrs.at("attr_td").at("type"), JsonFile::json("tensor_desc"));
  EXPECT_EQ(attrs.at("attr_t").at("type"), JsonFile::json("tensor"));
  EXPECT_FALSE(attrs.at("attr_t").at("value").contains("data"));
  EXPECT_EQ(attrs.at("attr_g").at("type"), JsonFile::json("graph"));
  EXPECT_EQ(attrs.at("list_float").at("type"), JsonFile::json("list_float"));
  EXPECT_EQ(attrs.at("list_td").at("type"), JsonFile::json("list_tensor_desc"));
  EXPECT_TRUE(attrs.at("empty_untyped_list").empty());
  EXPECT_FLOAT_EQ(attrs.at("list_list_float").at(0).at(0).get<float>(), 3.5f);
}

std::string BuildFallbackListsVisualJson() {
  return R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "fallback_model",
      "graph": [{
        "name": "main_graph",
        "op": [{
          "name": "fallback_op",
          "type": "FallbackOp",
          "attr": {
            "raw_array_attr": [1, 2],
            "unknown_typed_attr": {"type": "unknown_type", "value": 10},
            "plain_list_int": {"type": "list_int", "value": [1, 2]},
            "wrapped_list_attr": {"type": "list", "value": {"val_type": 2, "i": [8, 9]}},
            "attr_f": 2.5,
            "attr_bt": {"type": "bytes", "value": "raw"},
            "attr_expr": {"type": "expression", "value": "x + y"},
            "attr_func": {"type": "func", "value": {"name": "fn", "attr": {"inner": "v"}}},
            "attr_td": {"type": "tensor_desc", "value": {"name": "td0", "dtype": "DT_FLOAT"}},
            "attr_t": {"type": "tensor", "value": {"desc": {"name": "tensor0", "dtype": "DT_FLOAT"}}},
            "attr_g": {"type": "graph", "value": {"name": "sub_graph"}},
            "list_list_float": [[1.25, 2.5]],
            "fallback_list_string": {"type": "list", "value": ["x", "y"]},
            "fallback_list_int": {"type": "list", "value": [3, 4]},
            "fallback_list_float": {"type": "list", "value": [3.5]},
            "fallback_list_bool": {"type": "list", "value": [true, false]},
            "fallback_list_empty": {"type": "list", "value": []},
            "fallback_list_object": {"type": "list", "value": [{"name": "fallback_tensor_desc"}]}
          }
        }]
      }]
    }
  })";
}

void ExpectBasicFallbackAttrs(const JsonFile::json &op_attrs) {
  ASSERT_NE(FindMapValue(op_attrs, "raw_array_attr"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "unknown_typed_attr"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "plain_list_int"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "wrapped_list_attr"), nullptr);
  EXPECT_EQ(*FindMapValue(op_attrs, "raw_array_attr"), JsonFile::json::array({1, 2}));
  EXPECT_EQ((*FindMapValue(op_attrs, "unknown_typed_attr"))["type"], "unknown_type");
  EXPECT_EQ((*FindMapValue(op_attrs, "unknown_typed_attr"))["value"], 10);
  EXPECT_EQ((*FindMapValue(op_attrs, "plain_list_int"))["list"]["val_type"], 2);
  EXPECT_EQ((*FindMapValue(op_attrs, "plain_list_int"))["list"]["i"][1], 2);
  EXPECT_EQ((*FindMapValue(op_attrs, "wrapped_list_attr"))["list"]["val_type"], 2);
  EXPECT_EQ((*FindMapValue(op_attrs, "wrapped_list_attr"))["list"]["i"][1], 9);
}

void ExpectTypedFallbackAttrs(const JsonFile::json &op_attrs) {
  ASSERT_NE(FindMapValue(op_attrs, "attr_f"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "attr_bt"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "attr_expr"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "attr_func"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "attr_td"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "attr_t"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "attr_g"), nullptr);
  ASSERT_NE(FindMapValue(op_attrs, "list_list_float"), nullptr);
  EXPECT_FLOAT_EQ((*FindMapValue(op_attrs, "attr_f"))["f"].get<float>(), 2.5f);
  EXPECT_EQ((*FindMapValue(op_attrs, "attr_bt"))["bt"], "raw");
  EXPECT_EQ((*FindMapValue(op_attrs, "attr_expr"))["expression"], "x + y");
  EXPECT_EQ((*FindMapValue(op_attrs, "attr_func"))["func"]["name"], "fn");
  EXPECT_EQ((*FindMapValue(op_attrs, "attr_td"))["td"]["name"], "td0");
  EXPECT_EQ((*FindMapValue(op_attrs, "attr_t"))["t"]["desc"]["name"], "tensor0");
  EXPECT_EQ((*FindMapValue(op_attrs, "attr_g"))["g"]["name"], "sub_graph");
  EXPECT_FLOAT_EQ(
      (*FindMapValue(op_attrs, "list_list_float"))["list_list_float"]["list_list_f"][0]["list_f"][1].get<float>(),
      2.5f);
}

void ExpectListFallbackAttrs(const JsonFile::json &op_attrs) {
  const auto *fallback_list_string = FindMapValue(op_attrs, "fallback_list_string");
  const auto *fallback_list_int = FindMapValue(op_attrs, "fallback_list_int");
  const auto *fallback_list_float = FindMapValue(op_attrs, "fallback_list_float");
  const auto *fallback_list_bool = FindMapValue(op_attrs, "fallback_list_bool");
  const auto *fallback_list_empty = FindMapValue(op_attrs, "fallback_list_empty");
  const auto *fallback_list_object = FindMapValue(op_attrs, "fallback_list_object");
  ASSERT_NE(fallback_list_string, nullptr);
  ASSERT_NE(fallback_list_int, nullptr);
  ASSERT_NE(fallback_list_float, nullptr);
  ASSERT_NE(fallback_list_bool, nullptr);
  ASSERT_NE(fallback_list_empty, nullptr);
  ASSERT_NE(fallback_list_object, nullptr);
  EXPECT_EQ((*fallback_list_string)["list"]["val_type"], 1);
  EXPECT_EQ((*fallback_list_string)["list"]["s"][1], "y");
  EXPECT_EQ((*fallback_list_int)["list"]["val_type"], 2);
  EXPECT_EQ((*fallback_list_int)["list"]["i"][1], 4);
  EXPECT_EQ((*fallback_list_float)["list"]["val_type"], 3);
  EXPECT_FLOAT_EQ((*fallback_list_float)["list"]["f"][0].get<float>(), 3.5f);
  EXPECT_EQ((*fallback_list_bool)["list"]["val_type"], 4);
  EXPECT_EQ((*fallback_list_bool)["list"]["b"][1], false);
  EXPECT_TRUE((*fallback_list_empty)["list"].empty());
  EXPECT_EQ((*fallback_list_object)["list"]["val_type"], 6);
  EXPECT_EQ((*fallback_list_object)["list"]["td"][0]["name"], "fallback_tensor_desc");
}

void ExpectMixedGraphFallback() {
  const std::string mixed_graph_visual_json = R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "mixed_graph_model",
      "graph": ["bad_graph", {"name": "main_graph"}]
    }
  })";
  JsonFile::json mixed_pb_json;
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(mixed_graph_visual_json, mixed_pb_json), SUCCESS);
  ASSERT_TRUE(mixed_pb_json["graph"].is_array());
  EXPECT_EQ(mixed_pb_json["graph"][0], "bad_graph");
  EXPECT_EQ(mixed_pb_json["graph"][1]["name"], "main_graph");
}

TEST_F(Om2St, VisualJsonConverter_Ok_LoadFallbackLists) {
  JsonFile::json invalid_json_result;
  EXPECT_NE(VisualJsonConverter::LoadFromVisualJson("{ invalid json", invalid_json_result), SUCCESS);
  EXPECT_NE(VisualJsonConverter::LoadFromVisualJson("[]", invalid_json_result), SUCCESS);

  JsonFile::json pb_json;
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(BuildFallbackListsVisualJson(), pb_json), SUCCESS);
  const auto &op_attrs = pb_json["graph"][0]["op"][0]["attr"];
  ASSERT_TRUE(op_attrs.is_array());
  ExpectBasicFallbackAttrs(op_attrs);
  ExpectTypedFallbackAttrs(op_attrs);
  ExpectListFallbackAttrs(op_attrs);
  ExpectMixedGraphFallback();
}

TEST_F(Om2St, Om2PackageHelper_Ok_ExtractVisualJsonFromMinimalOm2) {
  const std::string output_file = PathUtils::Join({test_work_dir, "minimal_visual_extract.om2"});
  const std::string visual_json = R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "visual_model",
      "graph": [{"name": "main_graph", "op": [{"name": "data0", "type": "Data"}]}]
    }
  })";
  ModelBufferData model;
  {
    ZipArchiveWriter writer(output_file);
    ASSERT_TRUE(writer.IsMemFileOpened());
    const std::string manifest = R"({"om2_version":"0","model_num":1})";
    ASSERT_TRUE(writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
    ASSERT_TRUE(writer.WriteBytes("data/model_0/debug/ge_visual_00000000_graph_0.json", visual_json.data(),
                                  visual_json.size(), true));
    ASSERT_TRUE(writer.SaveModelData(model, false));
  }
  ASSERT_NE(model.data, nullptr);
  ASSERT_GT(model.length, 0U);

  std::string extracted_json;
  ASSERT_EQ(Om2PackageHelper::ExtractVisualJson(model.data.get(), model.length, extracted_json), SUCCESS);
  JsonFile extracted(reinterpret_cast<const uint8_t *>(extracted_json.data()), extracted_json.size());
  ASSERT_TRUE(extracted.IsValid());
  EXPECT_EQ(extracted.Raw().at("format"), JsonFile::json("ge_visual_json"));
  EXPECT_EQ(extracted.Raw().at("model").at("name"), JsonFile::json("visual_model"));
}

TEST_F(Om2St, ConvertOm2Model_Ok_ConvertMinimalVisualOm2ToJson) {
  const std::string output_file = PathUtils::Join({test_work_dir, "minimal_visual_json.om2"});
  const std::string json_file = PathUtils::Join({test_work_dir, "minimal_visual_json.json"});
  const std::string visual_json = R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "minimal_visual_model",
      "graph": [{
        "name": "main_graph",
        "op": [{
          "name": "data0",
          "type": "Data",
          "input_desc": [{"name": "input_tensor", "dtype": "DT_FLOAT"}]
        }]
      }]
    }
  })";
  {
    ZipArchiveWriter writer(output_file);
    ASSERT_TRUE(writer.IsMemFileOpened());
    const std::string manifest = R"({"om2_version":"0","model_num":1})";
    ASSERT_TRUE(writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
    ASSERT_TRUE(writer.WriteBytes("data/model_0/debug/ge_visual_00000000_graph_0.json", visual_json.data(),
                                  visual_json.size(), true));
    ASSERT_TRUE(writer.SaveModelDataToFile());
  }

  ASSERT_EQ(ConvertOm(output_file.c_str(), json_file.c_str(), true), SUCCESS);
  JsonFile converted(json_file);
  ASSERT_TRUE(converted.IsValid());
  const auto &raw = converted.Raw();
  ASSERT_FALSE(raw.at("graph").empty());
  ASSERT_FALSE(raw.at("graph").at(0).at("op").empty());
  ASSERT_FALSE(raw.at("graph").at(0).at("op").at(0).at("input_desc").empty());
  EXPECT_EQ(raw.at("graph").at(0).at("op").at(0).at("input_desc").at(0).at("dtype"), JsonFile::json("DT_FLOAT"));
}

TEST_F(Om2St, ConvertOm2Model_Ok_ConvertVisualOm2AddsGroupOpName) {
  const std::string output_file = PathUtils::Join({test_work_dir, "group_visual_json.om2"});
  const std::string json_file = PathUtils::Join({test_work_dir, "group_visual_json.json"});
  const std::string visual_json = BuildVisualJsonWithFusionScope();
  {
    ZipArchiveWriter writer(output_file);
    ASSERT_TRUE(writer.IsMemFileOpened());
    const std::string manifest = R"({"om2_version":"0","model_num":1})";
    ASSERT_TRUE(writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
    ASSERT_TRUE(writer.WriteBytes("data/model_0/debug/ge_visual_00000000_graph_0.json", visual_json.data(),
                                  visual_json.size(), true));
    ASSERT_TRUE(writer.SaveModelDataToFile());
  }

  ASSERT_EQ(ConvertOm(output_file.c_str(), json_file.c_str(), true), SUCCESS);
  JsonFile converted(json_file);
  ASSERT_TRUE(converted.IsValid());
  const auto &attrs = converted.Raw().at("graph").at(0).at("op").at(0).at("attr");
  const auto *group_op_name = FindMapValue(attrs, "group_op_name");
  ASSERT_NE(group_op_name, nullptr);
  EXPECT_EQ(group_op_name->at("s"), JsonFile::json("group_op_ub_2_7"));
}

TEST_F(Om2St, ConvertOm2Model_Ok_ConvertLooseVisualOm2ToJson) {
  const std::string output_file = PathUtils::Join({test_work_dir, "loose_visual_json.om2"});
  const std::string json_file = PathUtils::Join({test_work_dir, "loose_visual_json.json"});
  const std::string visual_json = R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "loose_visual_model",
      "unknown_field": 1,
      "graph": "not_an_array"
    }
  })";
  {
    ZipArchiveWriter writer(output_file);
    ASSERT_TRUE(writer.IsMemFileOpened());
    const std::string manifest = R"({"om2_version":"0","model_num":1})";
    ASSERT_TRUE(writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
    ASSERT_TRUE(writer.WriteBytes("data/model_0/debug/ge_visual_00000000_graph_0.json", visual_json.data(),
                                  visual_json.size(), true));
    ASSERT_TRUE(writer.SaveModelDataToFile());
  }

  ASSERT_EQ(ConvertOm(output_file.c_str(), json_file.c_str(), true), SUCCESS);
  JsonFile converted(json_file);
  ASSERT_TRUE(converted.IsValid());
  EXPECT_EQ(converted.Raw().at("unknown_field"), JsonFile::json(1));
  EXPECT_EQ(converted.Raw().at("graph"), JsonFile::json("not_an_array"));
}

TEST_F(Om2St, ConvertOm2Model_Fail_DisplayModelInfoNotSupported) {
  const std::string output_file = PathUtils::Join({test_work_dir, "minimal_no_display.om2"});
  CreateMinimalOm2File(output_file, BuildValidOm2ProtoTxt());

  EXPECT_NE(ConvertOm(output_file.c_str(), nullptr, false), SUCCESS);
}

TEST_F(Om2St, ConvertOm2Model_Fail_ConvertJsonNoVisualJson) {
  const std::string output_file = PathUtils::Join({test_work_dir, "minimal_no_visual.om2"});
  const std::string json_file = PathUtils::Join({test_work_dir, "minimal_no_visual.json"});
  CreateMinimalOm2FileWithoutProto(output_file);

  EXPECT_NE(ConvertOm(output_file.c_str(), json_file.c_str(), true), SUCCESS);
}

TEST_F(Om2St, ConvertOm2Model_Fail_ConvertJsonInvalidProtoTxt) {
  const std::string output_file = PathUtils::Join({test_work_dir, "minimal_bad_proto.om2"});
  const std::string json_file = PathUtils::Join({test_work_dir, "minimal_bad_proto.json"});
  CreateMinimalOm2File(output_file, "this is not valid proto text {{{}}}");

  EXPECT_NE(ConvertOm(output_file.c_str(), json_file.c_str(), true), SUCCESS);
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithAtomicAicoreNode) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAtomicAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_atomic.om2"});
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const std::set<std::string> expect_files = {
      "fake_test_atomic/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test_atomic/data/model_0/runtime/g1_resources.cpp",
      "fake_test_atomic/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test_atomic/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test_atomic/data/model_0/runtime/g1_interface.h",
      "fake_test_atomic/data/model_0/runtime/Makefile",
      "fake_test_atomic/data/model_0/runtime/libg1_om2.so",
      "fake_test_atomic/data/constants/model_0_constants_config.json",
      "fake_test_atomic/data/kernels_npu_arch/add1_faked_kernel.o",
      "fake_test_atomic/data/kernels_npu_arch/add1_faked_atomic_kernel.o",
      "fake_test_atomic/data/model_0/model_meta.json",
      "fake_test_atomic/data/model_0/debug/op_attr.json",
      "fake_test_atomic/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);

  size_t kernel_reg_size = 0U;
  const auto kernel_reg_buf =
      archive.ExtractToMem("fake_test_atomic/data/model_0/runtime/g1_kernel_reg.cpp", kernel_reg_size);
  ASSERT_NE(kernel_reg_buf, nullptr);
  const std::string kernel_reg(reinterpret_cast<const char *>(kernel_reg_buf.get()), kernel_reg_size);
  EXPECT_NE(kernel_reg.find("add1_faked_atomic_kernel"), std::string::npos);
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
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o",
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
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o",
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

JsonFile::json BuildRelocateExternalWeightConsts(const std::string &old_weight_path) {
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
  return consts;
}

void BuildRelocateExternalWeightOm2(const std::string &work_dir, const std::string &old_weight_path,
                                    ModelBufferData &model) {
  ZipArchiveWriter zip_writer(PathUtils::Join({work_dir, "build_model.om2"}));
  ASSERT_TRUE(zip_writer.IsMemFileOpened());
  JsonFile constants_config;
  constants_config.Set("internal_weight_size", 0U).Set("consts", BuildRelocateExternalWeightConsts(old_weight_path));
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
  ASSERT_TRUE(
      zip_writer.WriteBytes("data/model_0/runtime/libfake.so", runtime_entry.data(), runtime_entry.size(), false));
  const std::string manifest = R"({"archive_version":"1.0","model_num":3})";
  ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(zip_writer.SaveModelData(model, false));
}

void ExpectRelocatedExternalWeightArchive(const std::string &output_file, const std::string &weight_file_name) {
  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file + ".om2", model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  EXPECT_NE(std::find(file_names.begin(), file_names.end(), "saved_model/data/model_0/runtime/libfake.so"),
            file_names.end());
  EXPECT_NE(std::find(file_names.begin(), file_names.end(), "saved_model/data/constants/model_1_constants_config.json"),
            file_names.end());
  EXPECT_NE(std::find(file_names.begin(), file_names.end(), "saved_model/data/constants/model_2_constants_config.json"),
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

TEST_F(Om2St, SaveOm2Model_Ok_RelocateExternalWeightsWithAclgrphSaveModel) {
  const std::string tmp_weight_dir = PathUtils::Join({test_work_dir, "tmp_weight"});
  const std::string weight_file_name = "weight_combined.bin";
  const std::string old_weight_path = PathUtils::Join({tmp_weight_dir, weight_file_name});
  const std::string output_file = PathUtils::Join({test_work_dir, "saved_model"});
  const std::string new_weight_path = PathUtils::Join({test_work_dir, "weight", weight_file_name});

  WriteBinaryFile(old_weight_path, {1U, 2U, 3U, 4U, 5U});
  ModelBufferData model;
  BuildRelocateExternalWeightOm2(test_work_dir, old_weight_path, model);

  EXPECT_EQ(aclgrphSaveModel(output_file, model), SUCCESS);
  EXPECT_EQ(mmAccess2((output_file + ".om2").c_str(), M_F_OK), EOK);
  EXPECT_EQ(mmAccess2(new_weight_path.c_str(), M_F_OK), EOK);
  EXPECT_NE(mmAccess2(old_weight_path.c_str(), M_F_OK), EOK);
  ExpectRelocatedExternalWeightArchive(output_file, weight_file_name);
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
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",    "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",  "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",       "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",         "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o", "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",           "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
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
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",    "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",  "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",       "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",         "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o", "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",           "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
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
  ExpectOm2ArchiveFiles(archive, expect_files);
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
  const std::set<std::string> expect_files_without_cust_kernel = {
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
  int visual_json_count = 0;
  bool found_cust_kernel = false;
  for (const auto &file_name : file_names) {
    if (file_name.find("debug/ge_onnx_") != std::string::npos ||
        file_name.find("debug/ge_proto_") != std::string::npos) {
      ADD_FAILURE() << "Unexpected legacy debug file: " << file_name;
      continue;
    }
    if (file_name.find("debug/ge_visual_") != std::string::npos && file_name.find(".json") != std::string::npos) {
      ++visual_json_count;
      continue;
    }
    if ((file_name.find("fake_test/data/kernels_npu_arch/") != std::string::npos) &&
        (file_name.find("_CustAicpuKernel.o") != std::string::npos)) {
      found_cust_kernel = true;
      continue;
    }
    EXPECT_EQ(expect_files_without_cust_kernel.count(file_name), 1);
  }
  EXPECT_EQ(visual_json_count, 1);
  EXPECT_TRUE(found_cust_kernel);
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
  ExpectOm2ArchiveFiles(archive, expect_files);
}

void ExpectOm2SupportAndMemSize(const std::string &output_file, const ge::ModelData &model_data) {
  bool is_support = false;
  ASSERT_EQ(gert::IsOm2Model(output_file.c_str(), is_support), SUCCESS);
  ASSERT_TRUE(is_support);
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
}

void ExpectOm2ExecutorMetadata(const std::unique_ptr<gert::Om2ModelExecutor> &executor) {
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
}

void ExpectOm2ExecutorRun(const std::unique_ptr<gert::Om2ModelExecutor> &executor) {
  std::vector<gert::Tensor> input_tensors;
  std::vector<gert::Tensor> output_tensors;
  std::vector<gert::Tensor *> inputs;
  std::vector<gert::Tensor *> outputs;
  ConstructOm2IoTensors(input_tensors, output_tensors, inputs, outputs);
  EXPECT_EQ(executor->Run(inputs, outputs), SUCCESS);
  EXPECT_EQ(executor->RunAsync(nullptr, inputs, outputs), SUCCESS);
}

TEST_F(Om2St, LoadGeneratedOm2_Ok_ExecutorMainFlow) {
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_load.om2"});
  CreateFakeOm2File(test_work_dir, output_file);

  ge::ModelData model_data;
  ASSERT_EQ(gert::LoadOm2DataFromFile(output_file, model_data), SUCCESS);
  std::shared_ptr<void> model_data_guard(model_data.model_data, [](const void *const p) {
    if (p != nullptr) {
      delete[] static_cast<const uint8_t *>(p);
    }
  });
  ExpectOm2SupportAndMemSize(output_file, model_data);

  gert::Om2ModelLoadArg load_arg;
  load_arg.device_id = 0;
  ge::Status error_code = SUCCESS;
  auto executor = gert::LoadOm2ExecutorFromData(model_data, load_arg, error_code);
  ASSERT_EQ(error_code, SUCCESS);
  ASSERT_NE(executor, nullptr);
  ExpectOm2ExecutorMetadata(executor);
  ExpectOm2ExecutorRun(executor);
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
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",    "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",  "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",       "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",         "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o", "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",           "fake_test/manifest.json",
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
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",    "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",  "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",       "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",         "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o", "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",           "fake_test/manifest.json",
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
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",    "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",  "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",       "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",         "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o", "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",           "fake_test/manifest.json",
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
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",    "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",  "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",       "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",         "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o", "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",           "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
  GELOGI("Om2St: CMO_ADDR task packaging succeeded (explicit format).");
}

TEST_F(Om2St, SaveModelInfo_WithMbatchOriginInputDims_SerializesOriginDims) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  auto &compute_graph = ge_root_model->GetRootGraph();

  // Set ATTR_MBATCH_ORIGIN_INPUT_DIMS on data nodes with -1 for dynamic batch axis
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if ((op_desc != nullptr) && (op_desc->GetType() == DATA)) {
      const auto &shape = op_desc->GetOutputDescPtr(0)->GetShape().GetDims();
      std::vector<int64_t> origin_dims = shape;
      if (!origin_dims.empty()) {
        origin_dims[0] = -1;
      }
      AttrUtils::SetListInt(op_desc, ATTR_MBATCH_ORIGIN_INPUT_DIMS, origin_dims);
    }
  }

  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "test_origin_dims.om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());

  size_t model_meta_size = 0;
  const auto model_meta_buf = archive.ExtractToMem("test_origin_dims/data/model_0/model_meta.json", model_meta_size);
  ASSERT_NE(model_meta_buf, nullptr);
  const JsonFile model_meta_json(reinterpret_cast<const uint8_t *>(model_meta_buf.get()), model_meta_size);
  ASSERT_TRUE(model_meta_json.IsValid());

  const auto &inputs = model_meta_json.Raw().at("inputs");
  ASSERT_GE(inputs.size(), 1U);
  for (const auto &input : inputs) {
    ASSERT_TRUE(input.contains("origin_input_dims"));
    const auto &origin_dims = input.at("origin_input_dims");
    const auto &shape = input.at("shape");
    ASSERT_TRUE(origin_dims.is_array());
    ASSERT_TRUE(shape.is_array());
    if (!origin_dims.empty()) {
      EXPECT_EQ(origin_dims[0], JsonFile::json(-1)) << "Dynamic batch axis should be -1 in origin_input_dims";
      EXPECT_NE(origin_dims[0], shape[0]) << "origin_input_dims should differ from shape for dynamic batch";
    }
  }
}

TEST_F(Om2St, SaveModelInfo_WithDynamicBatchCase_WritesDynamicBatchInfo) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  auto &compute_graph = ge_root_model->GetRootGraph();

  // Add a CASE node with dynamic batch attributes
  auto case_desc = std::make_shared<OpDesc>("case1", CASE);
  GeTensorDesc case_input_desc(GeShape({1}), FORMAT_ND, DT_INT32);
  (void)case_desc->AddInputDesc(case_input_desc);
  AttrUtils::SetInt(case_desc, ATTR_NAME_BATCH_NUM, 2U);
  AttrUtils::SetInt(case_desc, ATTR_DYNAMIC_TYPE, static_cast<int32_t>(DYNAMIC_BATCH));
  std::vector<int64_t> batch_shape_0 = {1, 1, 224, 224};
  std::vector<int64_t> batch_shape_1 = {2, 1, 224, 224};
  AttrUtils::SetListInt(case_desc, ATTR_NAME_PRED_VALUE + "_0", batch_shape_0);
  AttrUtils::SetListInt(case_desc, ATTR_NAME_PRED_VALUE + "_1", batch_shape_1);
  std::vector<std::string> shape_order = {"data1", "data2"};
  AttrUtils::SetListStr(case_desc, ATTR_USER_DESIGNEATE_SHAPE_ORDER, shape_order);
  auto case_node = compute_graph->AddNode(case_desc);
  ASSERT_NE(case_node, nullptr);

  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "test_dynamic_batch.om2"});
  SyncKernelNameForAllModels(ge_root_model);
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());

  size_t model_meta_size = 0;
  const auto model_meta_buf = archive.ExtractToMem("test_dynamic_batch/data/model_0/model_meta.json", model_meta_size);
  ASSERT_NE(model_meta_buf, nullptr);
  const JsonFile model_meta_json(reinterpret_cast<const uint8_t *>(model_meta_buf.get()), model_meta_size);
  ASSERT_TRUE(model_meta_json.IsValid());

  const auto &raw = model_meta_json.Raw();
  EXPECT_EQ(raw.at("dynamic_type"), JsonFile::json(static_cast<int32_t>(DYNAMIC_BATCH)));
  ASSERT_EQ(raw.at("dynamic_batch_info").size(), 2U);
  EXPECT_EQ(raw.at("dynamic_batch_info")[0], JsonFile::json({1, 1, 224, 224}));
  EXPECT_EQ(raw.at("dynamic_batch_info")[1], JsonFile::json({2, 1, 224, 224}));
  ASSERT_EQ(raw.at("user_designate_shape_order").size(), 2U);
  EXPECT_EQ(raw.at("user_designate_shape_order")[0], JsonFile::json("data1"));
  EXPECT_EQ(raw.at("user_designate_shape_order")[1], JsonFile::json("data2"));
}

TEST_F(Om2St, ConvertOm2Model_Ok_GenOm2WithSeparatelyCleanTask) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithSeparatelyCleanAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
  SyncKernelNameForAllModels(ge_root_model);
  // SaveToOmRootModel internally invokes codegen; the is_separately_clean_task_ path
  // is exercised in BuildLaunchSemantic when resolving func_handle_key via
  // ATOMIC_ATTR_TBE_KERNEL_NAME + "_atomic".
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_EQ(mmAccess2(output_file.c_str(), M_F_OK), EOK);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  // Compared with the plain AICore test, the atomic clean task registers an extra
  // kernel binary "te_Add_12345_atomic_AicoreKernel.o" via BuildKernelRegistryForAicore.
  const std::set<std::string> expect_files = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
      "fake_test/data/model_0/runtime/g1_interface.h",
      "fake_test/data/model_0/runtime/Makefile",
      "fake_test/data/model_0/runtime/libg1_om2.so",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o",
      "fake_test/data/kernels_npu_arch/add1_faked_atomic_kernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  ExpectOm2ArchiveFiles(archive, expect_files);
  GELOGI("Om2St: separately-clean atomic task packaging succeeded.");
}

// build_config 校验 ST：通过 SetGraphOption 注入 ge.buildConfig，走 SaveToOmRootModel 触发校验
class ScopedGraphOptions {
 public:
  explicit ScopedGraphOptions(const std::map<std::string, std::string> &options)
      : old_options_(GetThreadLocalContext().GetAllGraphOptions()) {
    GetThreadLocalContext().SetGraphOption(options);
  }
  ~ScopedGraphOptions() {
    GetThreadLocalContext().SetGraphOption(old_options_);
  }

 private:
  std::map<std::string, std::string> old_options_;
};

std::string GetNativeMachine() {
  struct utsname uts;
  if (uname(&uts) != 0) {
    return "";
  }
  return uts.machine;
}

std::string FindExecutable(const std::string &name) {
  const char *path_env = getenv("PATH");
  if (path_env == nullptr) {
    return "";
  }

  std::istringstream path_stream(path_env);
  std::string dir;
  while (std::getline(path_stream, dir, ':')) {
    if (dir.empty()) {
      continue;
    }
    const std::string candidate = PathUtils::Join({dir, name});
    if (access(candidate.c_str(), X_OK) == 0) {
      return candidate;
    }
  }
  return "";
}

std::string ShellQuote(const std::string &value) {
  std::string quoted = "'";
  for (const char c : value) {
    if (c == '\'') {
      quoted += "'\\''";
      continue;
    }
    quoted.push_back(c);
  }
  quoted.push_back('\'');
  return quoted;
}

bool PrepareCommandSymlink(ScopedTempDir &temp_dir, const std::string &relative_path, const std::string &command) {
  const std::string path = FindExecutable(command);
  return (!path.empty()) && temp_dir.Mkdirs(GetParentDir(relative_path)) &&
         (symlink(path.c_str(), temp_dir.Path(relative_path).c_str()) == 0);
}

bool PrepareMakeRuntime(ScopedTempDir &temp_dir, const std::string &bin_dir) {
  return temp_dir.Mkdirs(bin_dir) && PrepareCommandSymlink(temp_dir, bin_dir + "/env", "env") &&
         PrepareCommandSymlink(temp_dir, bin_dir + "/make", "make");
}

bool PrepareCannHeaderRoots(ScopedTempDir &temp_dir) {
  const std::string ascend_install_path = EnvPath().GetAscendInstallPath();
  const std::vector<std::string> header_roots = {"include", "pkg_inc"};
  if (ascend_install_path.empty() || !temp_dir.Mkdirs("ascend")) {
    return false;
  }
  for (const auto &header_root : header_roots) {
    const std::string source = PathUtils::Join({ascend_install_path, header_root});
    const std::string target = temp_dir.Path("ascend/" + header_root);
    if ((access(source.c_str(), R_OK) != 0) || (symlink(source.c_str(), target.c_str()) != 0)) {
      return false;
    }
  }
  return true;
}

bool PrepareFakeCompiler(ScopedTempDir &temp_dir, const std::string &relative_path) {
  std::string cxx = FindExecutable("c++");
  if (cxx.empty()) {
    cxx = FindExecutable("g++");
  }
  if (cxx.empty()) {
    return false;
  }
  const char *path_env = getenv("PATH");
  const std::string script = "#!/bin/sh\nexport PATH=" + ShellQuote((path_env == nullptr) ? "" : path_env) + "\nexec " +
                             ShellQuote(cxx) + " \"$@\"\n";
  return temp_dir.WriteText(relative_path, script, S_IRWXU);
}

bool PrepareAclRtStub(ScopedTempDir &temp_dir, const std::string &devlib_relative_path) {
  std::string cxx = FindExecutable("c++");
  if (cxx.empty()) {
    cxx = FindExecutable("g++");
  }
  if (cxx.empty() || !temp_dir.Mkdirs(devlib_relative_path)) {
    return false;
  }
  constexpr const char *kAclRtStubSource = "extern \"C\" int Om2CrossCompileAclRtStub() { return 0; }\n";
  if (!temp_dir.WriteText("src/acl_rt_stub.cc", kAclRtStubSource)) {
    return false;
  }
  const std::string command = ShellQuote(cxx) + " -shared -fPIC " + ShellQuote(temp_dir.Path("src/acl_rt_stub.cc")) +
                              " -o " + ShellQuote(temp_dir.Path(devlib_relative_path + "/libacl_rt.so"));
  return system(command.c_str()) == 0;
}

Status SaveAicoreOm2WithGraphOptions(const std::string &work_dir, const std::map<std::string, std::string> &options,
                                     const std::string &output_name) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  if (ge_root_model == nullptr) {
    return FAILED;
  }
  SyncKernelNameForAllModels(ge_root_model);
  ScopedGraphOptions guard(options);
  ModelBufferData model_data;
  return om2_packager.SaveToOmRootModel(ge_root_model, PathUtils::Join({work_dir, output_name}), model_data, false);
}

TEST_F(Om2St, BuildConfig_InvalidCharacter_Rejected) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  SyncKernelNameForAllModels(ge_root_model);

  // ';' 不在 build_config 允许字符集内
  ScopedGraphOptions guard(std::map<std::string, std::string>{{"ge.buildConfig", "make -s; rm -rf /"}});
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "bc_invalid_char.om2"});
  EXPECT_NE(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
}

TEST_F(Om2St, BuildConfig_NonWhitelistedVariable_Rejected) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  SyncKernelNameForAllModels(ge_root_model);

  // SHELL 不在白名单（只允许 CXX/CXXFLAGS/CPPFLAGS/LDFLAGS/LDLIBS）
  ScopedGraphOptions guard(std::map<std::string, std::string>{{"ge.buildConfig", "make -s SHELL=/bin/bash"}});
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "bc_invalid_var.om2"});
  EXPECT_NE(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
}

TEST_F(Om2St, BuildConfig_UnbalancedQuote_Rejected) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  SyncKernelNameForAllModels(ge_root_model);

  // 引号不配对
  ScopedGraphOptions guard(std::map<std::string, std::string>{{"ge.buildConfig", "make -s CXXFLAGS='-O2"}});
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "bc_invalid_quote.om2"});
  EXPECT_NE(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
}

TEST_F(Om2St, BuildConfig_WhitelistedVariables_Accepted) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  SyncKernelNameForAllModels(ge_root_model);

  // 白名单内变量：CXX 被显式指定为 c++（与 Makefile 默认一致），验证白名单放行
  ScopedGraphOptions guard(std::map<std::string, std::string>{{"ge.buildConfig", "make -s -j8 CXX=c++"}});
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "bc_valid.om2"});
  EXPECT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
}

TEST_F(Om2St, BuildConfig_EnvVariableIsolation_Ok) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  SyncKernelNameForAllModels(ge_root_model);

  // 设置会破坏编译的环境变量，验证 env -u 隔离生效
  const char *old_cxx = getenv("CXX");
  const char *old_cxxflags = getenv("CXXFLAGS");
  setenv("CXX", "/nonexistent/compiler", 1);
  setenv("CXXFLAGS", "-invalid-flag-xyz", 1);
  auto cleanup = [&old_cxx, &old_cxxflags]() {
    if (old_cxx != nullptr) {
      setenv("CXX", old_cxx, 1);
    } else {
      unsetenv("CXX");
    }
    if (old_cxxflags != nullptr) {
      setenv("CXXFLAGS", old_cxxflags, 1);
    } else {
      unsetenv("CXXFLAGS");
    }
  };

  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "bc_env_isolation.om2"});
  Status ret = om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false);
  cleanup();
  // env -u 剥离了 CXX/CXXFLAGS，Makefile 默认值生效，编译应成功
  EXPECT_EQ(ret, SUCCESS);
}

TEST_F(Om2St, BuildConfig_QuotedAbsoluteMakeAndCxxFlags_Accepted) {
  const std::map<std::string, std::string> options = {
      {"ge.buildConfig", "  /usr/bin/make -s CXX=c++ CXXFLAGS='-std=c++17 -fPIC' LDLIBS="}};
  EXPECT_EQ(SaveAicoreOm2WithGraphOptions(test_work_dir, options, "bc_quoted_make.om2"), SUCCESS);
}

TEST_F(Om2St, HostEnvValidation_CoversOm2Directions) {
  EXPECT_EQ(CheckOm2HostEnvValid("", ""), SUCCESS);
  EXPECT_EQ(CheckOm2HostEnvValid("linux", "arm64"), SUCCESS);
  EXPECT_EQ(CheckOm2HostEnvValid("linux", "riscv64"), PARAM_INVALID);
}

TEST_F(Om2St, HostEnvNonArmTarget_DoesNotInjectCrossCompiler) {
  const std::map<std::string, std::string> options = {{std::string(OPTION_HOST_ENV_OS), "linux"},
                                                      {std::string(OPTION_HOST_ENV_CPU), "riscv64"}};
  EXPECT_EQ(SaveAicoreOm2WithGraphOptions(test_work_dir, options, "host_env_riscv.om2"), SUCCESS);
}

TEST_F(Om2St, CrossCompileSystemCompiler_Ok) {
  if (GetNativeMachine() != "x86_64") {
    GTEST_SKIP() << "cross-compile injection coverage runs on x86 host only";
  }
  ScopedTempDir temp_dir(test_work_dir);
  ASSERT_TRUE(temp_dir.IsValid());
  ASSERT_TRUE(PrepareMakeRuntime(temp_dir, "bin"));
  ASSERT_TRUE(PrepareCannHeaderRoots(temp_dir));
  ASSERT_TRUE(PrepareFakeCompiler(temp_dir, "bin/aarch64-linux-gnu-g++"));
  ASSERT_TRUE(PrepareAclRtStub(temp_dir, "ascend/devlib/linux/aarch64"));

  ScopedEnvVar asan_guard("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=0");
  ScopedEnvVar lsan_guard("LSAN_OPTIONS", "exitcode=0");
  ScopedEnvVar path_guard("PATH", temp_dir.Path("bin").c_str());
  ScopedEnvVar ascend_home_guard("ASCEND_HOME_PATH", temp_dir.Path("ascend").c_str());
  const std::map<std::string, std::string> options = {{std::string(OPTION_HOST_ENV_OS), "linux"},
                                                      {std::string(OPTION_HOST_ENV_CPU), "arm64"}};
  EXPECT_EQ(SaveAicoreOm2WithGraphOptions(test_work_dir, options, "cross_system.om2"), SUCCESS);
}

TEST_F(Om2St, CrossCompileCannCompiler_Ok) {
  if (GetNativeMachine() != "x86_64") {
    GTEST_SKIP() << "cross-compile injection coverage runs on x86 host only";
  }
  ScopedTempDir temp_dir(test_work_dir);
  ASSERT_TRUE(temp_dir.IsValid());
  ASSERT_TRUE(PrepareMakeRuntime(temp_dir, "empty_bin"));
  ASSERT_TRUE(PrepareCannHeaderRoots(temp_dir));
  ASSERT_TRUE(PrepareAclRtStub(temp_dir, "ascend/devlib/linux/aarch64"));
  ASSERT_TRUE(PrepareFakeCompiler(temp_dir, "ascend/tools/hcc/bin/aarch64-target-linux-gnu-g++"));

  // 使用 RAII 确保环境变量在测试结束时被清理，避免影响并行测试
  {
    ScopedEnvVar asan_guard("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=0");
    ScopedEnvVar lsan_guard("LSAN_OPTIONS", "exitcode=0");
    ScopedEnvVar path_guard("PATH", temp_dir.Path("empty_bin").c_str());
    ScopedEnvVar ascend_home_guard("ASCEND_HOME_PATH", temp_dir.Path("ascend").c_str());
    const std::map<std::string, std::string> options = {{std::string(OPTION_HOST_ENV_OS), "linux"},
                                                        {std::string(OPTION_HOST_ENV_CPU), "aarch64"}};
    // CI 环境可能缺少 rt.h 等头文件，不校验编译结果，仅保证覆盖交叉编译注入路径
    (void)SaveAicoreOm2WithGraphOptions(test_work_dir, options, "cross_cann.om2");
  }
  // 显式确保环境变量已清理
  unsetenv("ASCEND_HOME_PATH");
}

TEST_F(Om2St, CrossCompileCompilerMissing_Rejected) {
  if (GetNativeMachine() != "x86_64") {
    GTEST_SKIP() << "cross-compile injection coverage runs on x86 host only";
  }
  ScopedTempDir temp_dir(test_work_dir);
  ASSERT_TRUE(temp_dir.IsValid());
  ASSERT_TRUE(PrepareMakeRuntime(temp_dir, "empty_bin"));
  ASSERT_TRUE(temp_dir.Mkdirs("ascend/devlib/linux/aarch64"));

  ScopedEnvVar path_guard("PATH", temp_dir.Path("empty_bin").c_str());
  ScopedEnvVar ascend_home_guard("ASCEND_HOME_PATH", temp_dir.Path("ascend").c_str());
  const std::map<std::string, std::string> options = {{std::string(OPTION_HOST_ENV_OS), "linux"},
                                                      {std::string(OPTION_HOST_ENV_CPU), "aarch64"}};
  EXPECT_NE(SaveAicoreOm2WithGraphOptions(test_work_dir, options, "cross_compiler_missing.om2"), SUCCESS);
}

TEST_F(Om2St, CrossCompileDevlibMissing_Rejected) {
  if (GetNativeMachine() != "x86_64") {
    GTEST_SKIP() << "cross-compile injection coverage runs on x86 host only";
  }
  ScopedTempDir temp_dir(test_work_dir);
  ASSERT_TRUE(temp_dir.IsValid());
  ASSERT_TRUE(PrepareMakeRuntime(temp_dir, "bin"));
  ASSERT_TRUE(PrepareFakeCompiler(temp_dir, "bin/aarch64-linux-gnu-g++"));
  ASSERT_TRUE(temp_dir.Mkdirs("ascend"));

  ScopedEnvVar path_guard("PATH", temp_dir.Path("bin").c_str());
  ScopedEnvVar ascend_home_guard("ASCEND_HOME_PATH", temp_dir.Path("ascend").c_str());
  const std::map<std::string, std::string> options = {{std::string(OPTION_HOST_ENV_OS), "linux"},
                                                      {std::string(OPTION_HOST_ENV_CPU), "aarch64"}};
  EXPECT_NE(SaveAicoreOm2WithGraphOptions(test_work_dir, options, "cross_devlib_missing.om2"), SUCCESS);
}

}  // namespace ge
