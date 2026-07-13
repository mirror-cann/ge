/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <ge_running_env/fake_op.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/opskernel/ops_kernel_info_types.h"
#include "engines/custom_engine/custom_ops_kernel_builder.h"
#include "exe_graph/runtime/annotated_args_context.h"
#include "faker/space_registry_faker.h"
#include "framework/common/taskdown_common.h"
#include "ge/ge_ir_build.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "ge_common/ge_common_api_types.h"
#include "graph/custom_op.h"
#include "graph/custom_op_factory.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/ge_global_options.h"
#include "graph/ge_local_context.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph_metadef/common/ge_common/util.h"
#include "ge_running_env/ge_running_env_faker.h"
#include "init_ge.h"
#include "register/optimization_option_registry.h"
#include "tests/depends/mmpa/src/mmpa_stub.h"
#include "utils/bench_env.h"
#include "utils/mock_ops_kernel_builder.h"

namespace ge {
namespace {
constexpr const char *kAnnotatedArgsMobileOpType = "StAnnotatedArgsMobileOp";
constexpr const char *kAnnotatedArgsWorkspaceOpType = "StAnnotatedArgsWorkspaceOp";
constexpr const char *kAnnotatedArgsNoAddLaunchOpType = "StAnnotatedArgsNoAddLaunchOp";
constexpr const char *kAnnotatedArgsMultipleLaunchesOpType = "StAnnotatedArgsMultipleLaunchesOp";
constexpr const char *kAnnotatedArgsCopyMoveOpType = "StAnnotatedArgsCopyMoveOp";
constexpr const char *kAnnotatedArgsDeclareFailOpType = "StAnnotatedArgsDeclareFailOp";
constexpr const char *kAnnotatedArgsMobileTargetOs = "linux";
constexpr const char *kAnnotatedArgsMobileTargetCpu = "x86_64";
constexpr const char *kAscendOppPathEnv = "ASCEND_OPP_PATH";
constexpr const char *kAscendCustomOppPathEnv = "ASCEND_CUSTOM_OPP_PATH";
constexpr const char *kAnnotatedArgsMobileOppPath = "st_offline_launch_mobile_opp";
constexpr const char *kAnnotatedArgsMobileCustomSoName = "libst_offline_launch_mobile_should_not_pack.so";
constexpr const char *kAnnotatedArgsMobileCustomSoPayloadMarker = "ST_OFFLINE_LAUNCH_MOBILE_CUSTOM_SO_PAYLOAD_MARKER";
constexpr size_t kMobileModelFileHeaderSize = 256U;
constexpr size_t kMobilePartitionTableHeaderSize = sizeof(uint32_t);
constexpr size_t kMobilePartitionMemInfoSize = sizeof(uint32_t) * 3U;
constexpr uint32_t kMobileModelDefPartition = 0U;
constexpr uint32_t kMobileWeightsDataPartition = 1U;
constexpr uint32_t kMobileTaskInfoPartition = 2U;
constexpr uint32_t kMobileTaskGraphPartition = 3U;
constexpr uint32_t kMobileSignatureInfoPartition = 4U;
constexpr uint32_t kMobileModelConfigPartition = 5U;
constexpr uint32_t kMobileAippCustomInfoPartition = 6U;
constexpr uint32_t kMobileEncryptInfoPartition = 7U;
constexpr uint32_t kMobileWeightInfoPartition = 8U;
constexpr uint32_t kStandardOmCustomOpsPartition = static_cast<uint32_t>(ge::CUSTOM_OPS);

void MockGenerateTask() {
  auto aicore_func = [](const ge::Node &node, RunContext &context, std::vector<domi::TaskDef> &tasks) -> Status {
    (void)context;
    if (node.GetType() == CONSTANT) {
      return SUCCESS;
    }

    auto op_desc = node.GetOpDesc();
    op_desc->SetOpKernelLibName("AiCoreLib");
    ge::AttrUtils::SetStr(op_desc, ge::TVM_ATTR_NAME_MAGIC, "RT_DEV_BINARY_MAGIC_ELF");
    ge::AttrUtils::SetStr(op_desc, ge::ATTR_NAME_KERNEL_BIN_ID, op_desc->GetName() + "_fake_id");
    const char kernel_bin[] = "kernel_bin";
    std::vector<char> buffer(kernel_bin, kernel_bin + strlen(kernel_bin));
    ge::OpKernelBinPtr kernel_bin_ptr = std::make_shared<ge::OpKernelBin>("test", std::move(buffer));
    op_desc->SetExtAttr(ge::OP_EXTATTR_NAME_TBE_KERNEL, kernel_bin_ptr);
    size_t arg_size = 100;
    std::vector<uint8_t> args(arg_size, 0);
    domi::TaskDef task_def;
    task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_KERNEL));
    auto kernel_info = task_def.mutable_kernel();
    kernel_info->set_args(args.data(), args.size());
    kernel_info->set_args_size(arg_size);
    kernel_info->mutable_context()->set_kernel_type(static_cast<uint32_t>(ccKernelType::TE));
    kernel_info->set_kernel_name(node.GetName());
    kernel_info->set_block_dim(1);
    uint16_t args_offset[2] = {0};
    kernel_info->mutable_context()->set_args_offset(args_offset, 2 * sizeof(uint16_t));
    kernel_info->mutable_context()->set_op_index(node.GetOpDesc()->GetId());

    for (int32_t i = 0; i < 3; i++) {
      tasks.emplace_back(task_def);
    }
    return SUCCESS;
  };

  MockForGenerateTask("AiCoreLib", aicore_func);
}

void PrepareCleanAclgrphBuildForSt() {
  aclgrphBuildFinalize();
  ge::GEFinalize();
  BenchEnv::Init();
}

std::map<AscendString, AscendString> MakeAnnotatedArgsMobileInitOptionsForSt() {
  std::map<AscendString, AscendString> init_options;
  init_options.emplace(ge::OPTION_EXEC_HCCL_FLAG, "0");
  init_options.emplace(ge::OPTION_HOST_ENV_OS, kAnnotatedArgsMobileTargetOs);
  init_options.emplace(ge::OPTION_HOST_ENV_CPU, kAnnotatedArgsMobileTargetCpu);
  init_options.emplace(ge::configure_option::SOC_VERSION, "KirinX90");
  return init_options;
}

std::map<std::string, std::string> SnapshotMutableGlobalOptionsForMobileSt() {
  auto &global_options_mutex = GetGlobalOptionsMutex();
  const std::lock_guard<std::mutex> lock(global_options_mutex);
  return GetMutableGlobalOptions();
}

void RestoreMutableGlobalOptionsForMobileSt(const std::map<std::string, std::string> &global_options) {
  auto &global_options_mutex = GetGlobalOptionsMutex();
  const std::lock_guard<std::mutex> lock(global_options_mutex);
  GetMutableGlobalOptions() = global_options;
}

void RefreshOptimizationOptionsForMobileSt() {
  (void)GetThreadLocalContext().GetOo().Initialize(GetThreadLocalContext().GetAllOptions(),
                                                   OptionRegistry::GetInstance().GetRegisteredOptTable());
}

void MergeAnnotatedArgsMobileInitOptionsForSt(const std::map<AscendString, AscendString> &init_options) {
  auto &global_options_mutex = GetGlobalOptionsMutex();
  const std::lock_guard<std::mutex> lock(global_options_mutex);
  auto &global_options = GetMutableGlobalOptions();
  for (const auto &option : init_options) {
    if ((option.first.GetString() != nullptr) && (option.second.GetString() != nullptr)) {
      global_options[option.first.GetString()] = option.second.GetString();
    }
  }
  GetThreadLocalContext().SetGlobalOption(global_options);
  RefreshOptimizationOptionsForMobileSt();
}

struct ScopedGeOptionsForMobileSt {
  ScopedGeOptionsForMobileSt()
      : graph_options(GetThreadLocalContext().GetAllGraphOptions()),
        session_options(GetThreadLocalContext().GetAllSessionOptions()),
        global_options(GetThreadLocalContext().GetAllGlobalOptions()),
        mutable_global_options(SnapshotMutableGlobalOptionsForMobileSt()) {
    RestoreMutableGlobalOptionsForMobileSt({});
    (void)GetThreadLocalContext().SetGraphOption({});
    (void)GetThreadLocalContext().SetSessionOption({});
    (void)GetThreadLocalContext().SetGlobalOption({});
    RefreshOptimizationOptionsForMobileSt();
  }

  ~ScopedGeOptionsForMobileSt() {
    RestoreMutableGlobalOptionsForMobileSt(mutable_global_options);
    (void)GetThreadLocalContext().SetGraphOption(graph_options);
    (void)GetThreadLocalContext().SetSessionOption(session_options);
    (void)GetThreadLocalContext().SetGlobalOption(global_options);
    RefreshOptimizationOptionsForMobileSt();
  }

  std::map<std::string, std::string> graph_options;
  std::map<std::string, std::string> session_options;
  std::map<std::string, std::string> global_options;
  std::map<std::string, std::string> mutable_global_options;
};

struct ScopedFileForMobileSt {
  explicit ScopedFileForMobileSt(std::vector<std::string> files_in) : files(std::move(files_in)) {}

  ~ScopedFileForMobileSt() {
    for (const auto &file : files) {
      (void)std::remove(file.c_str());
    }
  }

  std::vector<std::string> files;
};

class ScopedEnvVarForMobileSt {
 public:
  explicit ScopedEnvVarForMobileSt(std::string env_name) : env_name_(std::move(env_name)) {
    const char *env_value = std::getenv(env_name_.c_str());
    if (env_value != nullptr) {
      has_old_value_ = true;
      old_value_ = env_value;
    }
  }

  ~ScopedEnvVarForMobileSt() {
    if (has_old_value_) {
      (void)mmSetEnv(env_name_.c_str(), old_value_.c_str(), 1);
      return;
    }
    (void)unsetenv(env_name_.c_str());
  }

 private:
  std::string env_name_;
  std::string old_value_;
  bool has_old_value_ = false;
};

class ScopedDirForMobileSt {
 public:
  explicit ScopedDirForMobileSt(std::string dir_path) : dir_path_(std::move(dir_path)) {
    Cleanup();
  }

  ~ScopedDirForMobileSt() {
    Cleanup();
  }

 private:
  void Cleanup() const {
    if (!dir_path_.empty()) {
      (void)mmRmdir(dir_path_.c_str());
    }
  }

  std::string dir_path_;
};

class ScopedAnnotatedArgsOppForSt {
 public:
  ScopedAnnotatedArgsOppForSt() : opp_dir_guard_(kAnnotatedArgsMobileOppPath), opp_env_guard_(kAscendOppPathEnv) {
    const std::string supported_host_dir = std::string(kAnnotatedArgsMobileOppPath) + "/built-in/op_proto/lib/" +
                                           kAnnotatedArgsMobileTargetOs + "/" + kAnnotatedArgsMobileTargetCpu;
    ready_ = (CreateDirectory(supported_host_dir) == 0) &&
             (mmSetEnv(kAscendOppPathEnv, kAnnotatedArgsMobileOppPath, 1) == EN_OK);
  }

  bool IsReady() const {
    return ready_;
  }

 private:
  ScopedDirForMobileSt opp_dir_guard_;
  ScopedEnvVarForMobileSt opp_env_guard_;
  bool ready_ = false;
};

class ScopedRealCustomOpsKernelBuilderForSt {
 public:
  ScopedRealCustomOpsKernelBuilderForSt() {
    const auto &builders = OpsKernelBuilderRegistry::GetInstance().GetAll();
    inserted_ = builders.find(kCustomOpKernelLibName) == builders.end();
    if (inserted_) {
      OpsKernelBuilderRegistry::GetInstance().Register(kCustomOpKernelLibName,
                                                       std::make_shared<custom::CustomOpsKernelBuilder>());
    }
  }

  ~ScopedRealCustomOpsKernelBuilderForSt() {
    if (inserted_) {
      OpsKernelBuilderRegistry::GetInstance().Unregister(kCustomOpKernelLibName);
    }
  }

 private:
  bool inserted_ = false;
};

struct MobilePartitionMemInfoForSt {
  uint32_t type;
  uint32_t mem_offset;
  uint32_t mem_size;
};

class StAnnotatedArgsMobileOp : public AnnotatedArgsOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x31U, 0x32U, 0x33U, 0x34U};
    const auto input = ctx.GetInputTensor(0U);
    const auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()}, gert::OutputAddr{0U, output->GetAddr()},
                                   uint64_t{1U});
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"st_offline_launch_mobile_kernel", kBin, sizeof(kBin), 1U, ctx.GetStreamId()},
        std::move(args));
  }
};

class StAnnotatedArgsWorkspaceOp : public AnnotatedArgsOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x61U, 0x62U, 0x63U, 0x64U};
    const auto input = ctx.GetInputTensor(0U);
    const auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    const auto workspace = ctx.MallocWorkSpace(100U);
    GE_ASSERT_NOTNULL(workspace.addr);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()}, gert::OutputAddr{0U, output->GetAddr()},
                                   workspace);
    return ctx.AddLaunch(gert::AnnotatedKernelLaunchInfo{"st_offline_launch_workspace_kernel", kBin, sizeof(kBin), 1U,
                                                         ctx.GetStreamId()},
                         std::move(args));
  }
};

class StAnnotatedArgsNoAddLaunchOp : public AnnotatedArgsOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }
};

class StAnnotatedArgsMultipleLaunchesOp : public AnnotatedArgsOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin0[] = {0x41U, 0x42U};
    static const uint8_t kBin1[] = {0x51U, 0x52U};
    const auto input = ctx.GetInputTensor(0U);
    const auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    gert::AnnotatedKernelArgs args0(gert::InputAddr{0U, input->GetAddr()});
    GE_ASSERT_SUCCESS(ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"st_offline_launch_multi_kernel0", kBin0, sizeof(kBin0), 1U, ctx.GetStreamId()},
        std::move(args0)));
    gert::AnnotatedKernelArgs args1(gert::OutputAddr{0U, output->GetAddr()});
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"st_offline_launch_multi_kernel1", kBin1, sizeof(kBin1), 1U, ctx.GetStreamId()},
        std::move(args1));
  }
};

class StAnnotatedArgsCopyMoveOp : public AnnotatedArgsOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x81U, 0x82U, 0x83U};
    const auto input = ctx.GetInputTensor(0U);
    const auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);

    gert::AnnotatedKernelArgs original(gert::InputAddr{0U, input->GetAddr()}, gert::OutputAddr{0U, output->GetAddr()},
                                       uint64_t{42U});
    gert::AnnotatedKernelArgs copied(original);
    gert::AnnotatedKernelArgs moved(std::move(copied));

    gert::AnnotatedKernelArgs assigned;
    assigned = original;
    gert::AnnotatedKernelArgs move_assigned;
    move_assigned = std::move(assigned);
    move_assigned = std::move(move_assigned);

    return ctx.AddLaunch(gert::AnnotatedKernelLaunchInfo{"st_offline_launch_copy_move_kernel", kBin, sizeof(kBin), 1U,
                                                         ctx.GetStreamId()},
                         std::move(move_assigned));
  }
};

class StAnnotatedArgsDeclareFailOp : public AnnotatedArgsOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    (void)ctx;
    return GRAPH_FAILED;
  }
};

void RegisterAnnotatedArgsOpForSt(const char *const op_type, std::function<std::unique_ptr<BaseCustomOp>()> creator) {
  const auto ret = CustomOpFactory::RegisterCustomOpCreator(op_type, std::move(creator));
  EXPECT_TRUE((ret == GRAPH_SUCCESS) || (ret == GRAPH_FAILED));
}

void RegisterAnnotatedArgsInferForSt(const char *const op_type) {
  gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2(true);
  auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  ASSERT_NE(space_registry, nullptr);
  auto op_impl_func = space_registry->CreateOrGetOpImpl(op_type);
  ASSERT_NE(op_impl_func, nullptr);

  op_impl_func->infer_shape = [](gert::InferShapeContext *context) -> graphStatus {
    GE_ASSERT_NOTNULL(context);
    const auto input_shape = context->GetInputShape(0U);
    auto output_shape = context->GetOutputShape(0U);
    GE_ASSERT_NOTNULL(input_shape);
    GE_ASSERT_NOTNULL(output_shape);
    output_shape->SetDimNum(0U);
    for (size_t dim = 0UL; dim < input_shape->GetDimNum(); ++dim) {
      output_shape->AppendDim(input_shape->GetDim(dim));
    }
    return GRAPH_SUCCESS;
  };
  op_impl_func->infer_datatype = [](gert::InferDataTypeContext *context) -> graphStatus {
    GE_ASSERT_NOTNULL(context);
    return context->SetOutputDataType(0U, context->GetInputDataType(0U));
  };
  op_impl_func->infer_shape_range = [](gert::InferShapeRangeContext *context) -> graphStatus {
    GE_ASSERT_NOTNULL(context);
    auto input_shape_range = context->GetInputShapeRange(0U);
    auto output_shape_range = context->GetOutputShapeRange(0U);
    GE_ASSERT_NOTNULL(input_shape_range);
    GE_ASSERT_NOTNULL(output_shape_range);
    output_shape_range->SetMin(const_cast<gert::Shape *>(input_shape_range->GetMin()));
    output_shape_range->SetMax(const_cast<gert::Shape *>(input_shape_range->GetMax()));
    return GRAPH_SUCCESS;
  };
}

Graph BuildAnnotatedArgsMobileGraphForSt(const char *const op_type) {
  auto data = OP_CFG(DATA).Attr(ATTR_NAME_INDEX, 0).InCnt(1).OutCnt(1).TensorDesc(FORMAT_ND, DT_FLOAT, {2, 2});
  auto custom_op = OP_CFG(op_type).InCnt(1).OutCnt(1).TensorDesc(FORMAT_ND, DT_FLOAT, {2, 2});
  auto net_output = OP_CFG(NETOUTPUT).InCnt(1).OutCnt(1).TensorDesc(FORMAT_ND, DT_FLOAT, {2, 2});
  DEF_GRAPH(graph) {
    CHAIN(NODE("data", data)->EDGE(0, 0)->NODE("custom_op", custom_op)->EDGE(0, 0)->NODE("net_output", net_output));
  };
  auto compute_graph = ToComputeGraph(graph);
  if (compute_graph != nullptr) {
    auto custom_node = compute_graph->FindNode("custom_op");
    if ((custom_node != nullptr) && (custom_node->GetOpDesc() != nullptr)) {
      auto op_desc = custom_node->GetOpDesc();
      op_desc->SetOpEngineName(kEngineNameCustom);
      op_desc->SetOpKernelLibName(kCustomOpKernelLibName);
      (void)AttrUtils::SetStr(op_desc, ATTR_NAME_ENGINE_NAME_FOR_LX, kEngineNameCustom);
      (void)AttrUtils::SetStr(op_desc, ATTR_NAME_KKERNEL_LIB_NAME_FOR_LX, kCustomOpKernelLibName);
      (void)AttrUtils::SetStr(op_desc, ATTR_NAME_OP_SPECIFIED_ENGINE_NAME, kEngineNameCustom);
      (void)AttrUtils::SetStr(op_desc, ATTR_NAME_OP_SPECIFIED_KERNEL_LIB_NAME, kCustomOpKernelLibName);
    }
  }
  return GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);
}

Graph BuildAnnotatedArgsMobileGraphForSt() {
  return BuildAnnotatedArgsMobileGraphForSt(kAnnotatedArgsMobileOpType);
}

bool ReadUint32ValueForSt(const std::vector<char> &data, const size_t offset, uint32_t &value) {
  if ((offset + sizeof(value)) > data.size()) {
    return false;
  }
  value = 0U;
  for (size_t i = 0U; i < sizeof(value); ++i) {
    value |= static_cast<uint32_t>(static_cast<uint8_t>(data[offset + i])) << (i * 8U);
  }
  return true;
}

bool ReadFileForSt(const std::string &file_path, std::vector<char> &data) {
  std::ifstream file(file_path.c_str(), std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }
  const auto file_size_pos = file.tellg();
  if (file_size_pos < std::streampos(0)) {
    return false;
  }
  data.resize(static_cast<size_t>(file_size_pos));
  (void)file.seekg(0, std::ios::beg);
  (void)file.read(data.data(), static_cast<std::streamsize>(data.size()));
  return file.gcount() == static_cast<std::streamsize>(data.size());
}

bool FileContainsStringForSt(const std::string &file_path, const std::string &target) {
  std::vector<char> data;
  if (!ReadFileForSt(file_path, data)) {
    return false;
  }
  return std::search(data.cbegin(), data.cend(), target.cbegin(), target.cend()) != data.cend();
}

bool WriteFakeCustomOpSoForSt(const std::string &so_path) {
  std::vector<uint8_t> elf_header(64U, 0U);
  elf_header[0] = 0x7FU;
  elf_header[1] = static_cast<uint8_t>('E');
  elf_header[2] = static_cast<uint8_t>('L');
  elf_header[3] = static_cast<uint8_t>('F');
  elf_header[4] = 2U;
  elf_header[5] = 1U;
  elf_header[6] = 1U;
  elf_header[16] = 3U;
  constexpr uint16_t kElfMachineX86_64 = 62U;
  elf_header[18] = static_cast<uint8_t>(kElfMachineX86_64 & 0xFFU);
  elf_header[19] = static_cast<uint8_t>((kElfMachineX86_64 >> 8U) & 0xFFU);

  std::ofstream file(so_path.c_str(), std::ios::binary | std::ios::trunc);
  if (!file.good()) {
    return false;
  }
  file.write(reinterpret_cast<const char *>(elf_header.data()), static_cast<std::streamsize>(elf_header.size()));
  file.write(kAnnotatedArgsMobileCustomSoPayloadMarker, strlen(kAnnotatedArgsMobileCustomSoPayloadMarker));
  return file.good();
}

std::vector<MobilePartitionMemInfoForSt> ParseMobilePartitionsForSt(const std::string &omc_file) {
  std::vector<MobilePartitionMemInfoForSt> partitions;
  std::vector<char> data;
  if (!ReadFileForSt(omc_file, data) || data.empty()) {
    return partitions;
  }

  uint32_t partition_num = 0U;
  if (!ReadUint32ValueForSt(data, kMobileModelFileHeaderSize, partition_num)) {
    return partitions;
  }
  constexpr uint32_t kMaxMobilePartitionNumForSt = 16U;
  if (partition_num > kMaxMobilePartitionNumForSt) {
    return partitions;
  }

  const size_t table_size =
      kMobilePartitionTableHeaderSize + (kMobilePartitionMemInfoSize * static_cast<size_t>(partition_num));
  if ((kMobileModelFileHeaderSize + table_size) > data.size()) {
    return partitions;
  }

  size_t partition_offset = kMobileModelFileHeaderSize + kMobilePartitionTableHeaderSize;
  for (uint32_t i = 0U; i < partition_num; ++i) {
    MobilePartitionMemInfoForSt partition{};
    if (!ReadUint32ValueForSt(data, partition_offset, partition.type) ||
        !ReadUint32ValueForSt(data, partition_offset + sizeof(uint32_t), partition.mem_offset) ||
        !ReadUint32ValueForSt(data, partition_offset + (sizeof(uint32_t) * 2U), partition.mem_size)) {
      return {};
    }
    partitions.emplace_back(partition);
    partition_offset += kMobilePartitionMemInfoSize;
  }
  return partitions;
}

bool HasPartitionTypeForSt(const std::vector<MobilePartitionMemInfoForSt> &partitions, const uint32_t type) {
  return std::any_of(partitions.cbegin(), partitions.cend(),
                     [type](const MobilePartitionMemInfoForSt &partition) { return partition.type == type; });
}

bool IsExpectedMobilePartitionTypeForSt(const uint32_t type) {
  return (type == kMobileModelDefPartition) || (type == kMobileWeightsDataPartition) ||
         (type == kMobileTaskInfoPartition) || (type == kMobileTaskGraphPartition) ||
         (type == kMobileSignatureInfoPartition) || (type == kMobileModelConfigPartition) ||
         (type == kMobileAippCustomInfoPartition) || (type == kMobileEncryptInfoPartition) ||
         (type == kMobileWeightInfoPartition);
}

graphStatus InitializeAnnotatedArgsMobileBuildForSt() {
  auto init_options = MakeAnnotatedArgsMobileInitOptionsForSt();
  GeRunningEnvFaker env;
  env.InstallDefault();
  const auto ret = aclgrphBuildInitialize(init_options);
  if (ret == SUCCESS) {
    MergeAnnotatedArgsMobileInitOptionsForSt(init_options);
  }
  return ret;
}
}  // namespace

class GeIrBuildAnnotatedArgsTest : public testing::Test {
 protected:
  void SetUp() {
    MockGenerateTask();
  }

  void TearDown() {
    OpsKernelBuilderRegistry::GetInstance().Unregister("AiCoreLib");
    ReInitGe();
  }
};

TEST_F(GeIrBuildAnnotatedArgsTest, AnnotatedArgsMobileBuildDoesNotWriteCustomOpsPartition) {
  PrepareCleanAclgrphBuildForSt();
  ScopedGeOptionsForMobileSt ge_options_guard;
  ScopedAnnotatedArgsOppForSt opp_guard;
  ASSERT_TRUE(opp_guard.IsReady());
  const std::string output_file = "online";
  const std::string omc_file = output_file + "c";
  ScopedFileForMobileSt output_file_guard({output_file, omc_file, output_file + ".om"});
  const std::string custom_opp_path = "st_offline_launch_mobile_custom_opp";
  ScopedDirForMobileSt custom_opp_dir_guard(custom_opp_path);
  ScopedEnvVarForMobileSt custom_opp_env_guard(kAscendCustomOppPathEnv);
  const std::string target_so_dir =
      custom_opp_path + "/op_graph/lib/" + kAnnotatedArgsMobileTargetOs + "/" + kAnnotatedArgsMobileTargetCpu;
  ASSERT_EQ(CreateDirectory(target_so_dir), 0);
  const std::string target_so_path = target_so_dir + "/" + kAnnotatedArgsMobileCustomSoName;
  ASSERT_TRUE(WriteFakeCustomOpSoForSt(target_so_path));
  ASSERT_EQ(mmSetEnv(kAscendCustomOppPathEnv, custom_opp_path.c_str(), 1), EN_OK);
  (void)std::remove(output_file.c_str());
  (void)std::remove(omc_file.c_str());
  (void)std::remove((output_file + ".om").c_str());

  RegisterAnnotatedArgsOpForSt(kAnnotatedArgsMobileOpType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<StAnnotatedArgsMobileOp>();
  });
  RegisterAnnotatedArgsInferForSt(kAnnotatedArgsMobileOpType);

  ASSERT_EQ(InitializeAnnotatedArgsMobileBuildForSt(), SUCCESS);
  ScopedRealCustomOpsKernelBuilderForSt custom_builder_guard;

  auto graph = BuildAnnotatedArgsMobileGraphForSt();
  std::map<AscendString, AscendString> build_options;
  build_options.emplace(ge::ir_option::INPUT_FORMAT, "ND");
  ModelBufferData model_buffer_data{};
  EXPECT_EQ(aclgrphBuildModel(graph, build_options, model_buffer_data), SUCCESS);
  aclgrphBuildFinalize();

  const auto partitions = ParseMobilePartitionsForSt(omc_file);
  ASSERT_FALSE(partitions.empty());
  EXPECT_TRUE(HasPartitionTypeForSt(partitions, kMobileModelDefPartition));
  EXPECT_TRUE(HasPartitionTypeForSt(partitions, kMobileWeightsDataPartition));
  EXPECT_TRUE(HasPartitionTypeForSt(partitions, kMobileTaskInfoPartition));
  for (const auto &partition : partitions) {
    EXPECT_TRUE(IsExpectedMobilePartitionTypeForSt(partition.type))
        << "Unexpected mobile partition type: " << partition.type;
  }
  EXPECT_FALSE(HasPartitionTypeForSt(partitions, kStandardOmCustomOpsPartition));
  EXPECT_FALSE(FileContainsStringForSt(omc_file, kAnnotatedArgsMobileCustomSoName));
  EXPECT_FALSE(FileContainsStringForSt(omc_file, kAnnotatedArgsMobileCustomSoPayloadMarker));
}

TEST_F(GeIrBuildAnnotatedArgsTest, AnnotatedArgsMobileBuildSupportsWorkspaceProbeAndFinalTaskGeneration) {
  PrepareCleanAclgrphBuildForSt();
  ScopedGeOptionsForMobileSt ge_options_guard;
  ScopedAnnotatedArgsOppForSt opp_guard;
  ASSERT_TRUE(opp_guard.IsReady());
  const std::string output_file = "online";
  const std::string omc_file = output_file + "c";
  ScopedFileForMobileSt output_file_guard({output_file, omc_file, output_file + ".om"});
  (void)std::remove(output_file.c_str());
  (void)std::remove(omc_file.c_str());
  (void)std::remove((output_file + ".om").c_str());
  RegisterAnnotatedArgsOpForSt(kAnnotatedArgsWorkspaceOpType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<StAnnotatedArgsWorkspaceOp>();
  });
  RegisterAnnotatedArgsInferForSt(kAnnotatedArgsWorkspaceOpType);

  ASSERT_EQ(InitializeAnnotatedArgsMobileBuildForSt(), SUCCESS);
  ScopedRealCustomOpsKernelBuilderForSt custom_builder_guard;

  auto graph = BuildAnnotatedArgsMobileGraphForSt(kAnnotatedArgsWorkspaceOpType);
  std::map<AscendString, AscendString> build_options;
  build_options.emplace(ge::ir_option::INPUT_FORMAT, "ND");
  ModelBufferData model_buffer_data{};
  EXPECT_EQ(aclgrphBuildModel(graph, build_options, model_buffer_data), SUCCESS);
  aclgrphBuildFinalize();

  const auto partitions = ParseMobilePartitionsForSt(omc_file);
  ASSERT_FALSE(partitions.empty());
  EXPECT_TRUE(HasPartitionTypeForSt(partitions, kMobileTaskInfoPartition));
}

TEST_F(GeIrBuildAnnotatedArgsTest, AnnotatedArgsMobileBuildFailsForInvalidLaunchPlan) {
  struct InvalidLaunchPlanCase {
    const char *op_type;
    std::function<std::unique_ptr<BaseCustomOp>()> creator;
  };
  const std::vector<InvalidLaunchPlanCase> invalid_cases = {
      {kAnnotatedArgsNoAddLaunchOpType,
       []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<StAnnotatedArgsNoAddLaunchOp>(); }},
      {kAnnotatedArgsMultipleLaunchesOpType,
       []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<StAnnotatedArgsMultipleLaunchesOp>(); }},
  };

  for (const auto &invalid_case : invalid_cases) {
    PrepareCleanAclgrphBuildForSt();
    ScopedGeOptionsForMobileSt ge_options_guard;
    ScopedAnnotatedArgsOppForSt opp_guard;
    ASSERT_TRUE(opp_guard.IsReady());
    RegisterAnnotatedArgsOpForSt(invalid_case.op_type, invalid_case.creator);
    RegisterAnnotatedArgsInferForSt(invalid_case.op_type);

    ASSERT_EQ(InitializeAnnotatedArgsMobileBuildForSt(), SUCCESS);
    ScopedRealCustomOpsKernelBuilderForSt custom_builder_guard;

    auto graph = BuildAnnotatedArgsMobileGraphForSt(invalid_case.op_type);
    std::map<AscendString, AscendString> build_options;
    build_options.emplace(ge::ir_option::INPUT_FORMAT, "ND");
    ModelBufferData model_buffer_data{};
    EXPECT_NE(aclgrphBuildModel(graph, build_options, model_buffer_data), SUCCESS);
    EXPECT_EQ(model_buffer_data.data, nullptr);
    aclgrphBuildFinalize();
  }
}

TEST_F(GeIrBuildAnnotatedArgsTest, AnnotatedArgsMobileBuildExercisesAnnotatedArgsCopyAndMoveSemantics) {
  PrepareCleanAclgrphBuildForSt();
  ScopedGeOptionsForMobileSt ge_options_guard;
  ScopedAnnotatedArgsOppForSt opp_guard;
  ASSERT_TRUE(opp_guard.IsReady());
  RegisterAnnotatedArgsOpForSt(kAnnotatedArgsCopyMoveOpType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<StAnnotatedArgsCopyMoveOp>();
  });
  RegisterAnnotatedArgsInferForSt(kAnnotatedArgsCopyMoveOpType);

  ASSERT_EQ(InitializeAnnotatedArgsMobileBuildForSt(), SUCCESS);
  ScopedRealCustomOpsKernelBuilderForSt custom_builder_guard;

  auto graph = BuildAnnotatedArgsMobileGraphForSt(kAnnotatedArgsCopyMoveOpType);
  std::map<AscendString, AscendString> build_options;
  build_options.emplace(ge::ir_option::INPUT_FORMAT, "ND");
  ModelBufferData model_buffer_data{};
  EXPECT_EQ(aclgrphBuildModel(graph, build_options, model_buffer_data), SUCCESS);
  aclgrphBuildFinalize();
}

TEST_F(GeIrBuildAnnotatedArgsTest, AnnotatedArgsMobileBuildFailsWhenDeclareLaunchArgsFails) {
  PrepareCleanAclgrphBuildForSt();
  ScopedGeOptionsForMobileSt ge_options_guard;
  ScopedAnnotatedArgsOppForSt opp_guard;
  ASSERT_TRUE(opp_guard.IsReady());
  RegisterAnnotatedArgsOpForSt(kAnnotatedArgsDeclareFailOpType, []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<StAnnotatedArgsDeclareFailOp>();
  });
  RegisterAnnotatedArgsInferForSt(kAnnotatedArgsDeclareFailOpType);

  ASSERT_EQ(InitializeAnnotatedArgsMobileBuildForSt(), SUCCESS);
  ScopedRealCustomOpsKernelBuilderForSt custom_builder_guard;

  auto graph = BuildAnnotatedArgsMobileGraphForSt(kAnnotatedArgsDeclareFailOpType);
  std::map<AscendString, AscendString> build_options;
  build_options.emplace(ge::ir_option::INPUT_FORMAT, "ND");
  ModelBufferData model_buffer_data{};
  EXPECT_NE(aclgrphBuildModel(graph, build_options, model_buffer_data), SUCCESS);
  EXPECT_EQ(model_buffer_data.data, nullptr);
  aclgrphBuildFinalize();
}

}  // namespace ge
