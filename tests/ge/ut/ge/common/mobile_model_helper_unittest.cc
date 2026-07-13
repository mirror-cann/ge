/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include <gtest/gtest.h>

#include "framework/common/helper/mobile_model_helper.h"
#include "framework/omg/ge_init.h"
#include "framework/common/framework_types_internal.h"

#include "graph/custom_op_factory.h"

#include "common/env_path.h"
#include "common/op_so_store/op_so_store_utils.h"
#include "common/plugin/plugin_manager.h"
#include "mmpa/mmpa_api.h"
#include "graph_metadef/common/ge_common/util.h"
#include "common/model/ge_model.h"
#include "common/opskernel/ops_kernel_info_types.h"

#include "graph/buffer/buffer_impl.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "securec.h"
#include "graph/utils/tensor_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/ge_local_context.h"
#include "proto/task.pb.h"

namespace {

std::vector<char> CreateStubBin() {
  auto ascend_install_path = ge::EnvPath().GetAscendInstallPath();
  GELOGI("ascend_install_path: %s.", ascend_install_path.c_str());
  std::string op_bin_path = ascend_install_path + "/fwkacllib/lib64/switch_by_index.o";
  std::vector<char> buf;
  std::ifstream file(op_bin_path.c_str(), std::ios::binary | std::ios::in);
  if (!file.is_open()) {
    GELOGW("file: %s does not exist or is unaccessible.", op_bin_path.c_str());
    return buf;
  }
  GE_MAKE_GUARD(file_guard, [&file]() { (void)file.close(); });
  const std::streampos begin = file.tellg();
  (void)file.seekg(0, std::ios::end);
  const std::streampos end = file.tellg();
  auto buf_len = static_cast<uint32_t>(end - begin);
  buf.resize(buf_len + 1);
  (void)file.seekg(0, std::ios::beg);
  (void)file.read(const_cast<char *>(buf.data()), buf_len);
  return buf;
}

ge::NodePtr CreateNode(ge::ComputeGraph &graph, const std::string &name, const std::string &type, int in_num,
                       int out_num, ge::DataType data_type = ge::DT_INT64) {
  ge::OpDescPtr op_desc = std::make_shared<ge::OpDesc>(name, type);
  op_desc->SetStreamId(0);
  static int32_t index = 0;
  op_desc->SetId(index++);

  ge::GeTensorDesc tensor(ge::GeShape({8}), ge::FORMAT_ND, data_type);
  ge::TensorUtils::SetSize(tensor, 64);
  std::vector<int64_t> input_offset;
  for (int i = 0; i < in_num; i++) {
    op_desc->AddInputDesc(tensor);
    input_offset.emplace_back(index * 64 + i * 64);
  }
  op_desc->SetInputOffset(input_offset);

  std::vector<int64_t> output_offset;
  for (int i = 0; i < out_num; i++) {
    op_desc->AddOutputDesc(tensor);
    output_offset.emplace_back(index * 64 + in_num * 64 + i * 64);
  }
  op_desc->SetOutputOffset(output_offset);

  op_desc->SetWorkspace({});
  op_desc->SetWorkspaceBytes({});

  const static std::set<std::string> kGeLocalTypes{"Data", "Constant", "NetOutput"};
  op_desc->SetOpKernelLibName((kGeLocalTypes.count(type) > 0U) ? "DNN_VM_GE_LOCAL_OP_STORE" : "DNN_VM_RTS_OP_STORE");
  return graph.AddNode(op_desc);
}

ge::GeRootModelPtr GenGeRootModel(bool is_invaild_data_type = false, const std::string &custom_op_type = "Add") {
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("test");

  ge::NodePtr input0_node = CreateNode(*graph, "input0", "Data", 1, 1);
  ge::OpDescPtr input0_op_desc = input0_node->GetOpDesc();
  EXPECT_NE(input0_op_desc, nullptr);

  ge::NodePtr input1_node = CreateNode(*graph, "input1", "Data", 1, 1);
  ge::OpDescPtr input1_op_desc = input1_node->GetOpDesc();
  EXPECT_NE(input1_op_desc, nullptr);

  // not support data type
  if (is_invaild_data_type) {
    ge::NodePtr input2_node = CreateNode(*graph, "input2", "Data", 1, 1, ge::DT_FLOAT4_E1M2);
    ge::OpDescPtr input2_op_desc = input2_node->GetOpDesc();
    EXPECT_NE(input2_op_desc, nullptr);
  }

  ge::NodePtr add_node = CreateNode(*graph, "add", custom_op_type, 2, 1);
  ge::OpDescPtr add_op_desc = add_node->GetOpDesc();
  EXPECT_NE(add_op_desc, nullptr);

  std::string test_float_attr_str = "test_float_attr";
  float test_float_value = 1.0;
  ge::AttrUtils::SetFloat(add_op_desc, test_float_attr_str, test_float_value);

  std::string test_bytes_attr_str = "test_bytes_attr";
  ge::Buffer test_bytes_buffer(10, 1);
  ge::AttrUtils::SetBytes(add_op_desc, test_bytes_attr_str, test_bytes_buffer);

  std::string test_list_attr_str = "test_list_attr";
  std::vector<int> test_list_attr_value = {1, 2, 3, 4};
  ge::AttrUtils::SetListInt(add_op_desc, test_list_attr_str, test_list_attr_value);

  std::string test_list_list_int_attr_str = "test_list_list_int_attr";
  std::vector<std::vector<int64_t>> test_list_list_int_value = {{1, 2, 3}, {1, 2, 3}};
  ge::AttrUtils::SetListListInt(add_op_desc, test_list_list_int_attr_str, test_list_list_int_value);

  std::string test_list_list_float_attr_str = "test_list_list_float_attr";
  std::vector<std::vector<float>> test_list_list_float_value = {{1.1, 2.34, 3.4}, {1.1, 2.8, 3.5}};
  ge::AttrUtils::SetListListFloat(add_op_desc, test_list_list_float_attr_str, test_list_list_float_value);

  ge::GeAttrValue::NAMED_ATTRS test_named_attrs;
  std::string named_attr_str0 = "named_attr_test_str";
  std::string named_attr_str_value = "testtest";
  ge::AttrUtils::SetStr(test_named_attrs, named_attr_str0, named_attr_str_value);
  std::string named_attr_str1 = "named_attr_test_int";
  int named_attr_int_value = 1232;
  ge::AttrUtils::SetInt(test_named_attrs, named_attr_str1, named_attr_int_value);
  std::string named_attr = "named_attr";
  (void)ge::AttrUtils::SetNamedAttrs(add_op_desc, named_attr, test_named_attrs);

  std::string dynamic_inputs_indexes_str = "_dynamic_inputs_indexes";
  std::vector<std::vector<int64_t>> dynamic_inputs_indexes = {{0, 1}};
  ge::AttrUtils::SetListListInt(add_op_desc, dynamic_inputs_indexes_str, dynamic_inputs_indexes);

  std::string dynamic_outputs_indexes_str = "_dynamic_outputs_indexes";
  std::vector<std::vector<int64_t>> dynamic_outputs_indexes = {{0}};
  ge::AttrUtils::SetListListInt(add_op_desc, dynamic_outputs_indexes_str, dynamic_outputs_indexes);

  const std::string kernel_name = "test";
  std::string kernel_name_str = "_kernelname";
  ge::AttrUtils::SetStr(add_op_desc, kernel_name_str, kernel_name);

  ge::NodePtr netoutput_node = CreateNode(*graph, "output0", "NetOutput", 1, 1);
  ge::OpDescPtr netoutput_op_desc = netoutput_node->GetOpDesc();
  EXPECT_NE(netoutput_op_desc, nullptr);

  ge::GeModelPtr ge_model = std::make_shared<ge::GeModel>();
  ge_model->SetGraph(graph);
  ge_model->SetName("test_model");

  ge::TBEKernelStore tbe_kernel_store;
  const auto kernel = ge::MakeShared<ge::OpKernelBin>(kernel_name, CreateStubBin());
  tbe_kernel_store.AddTBEKernel(kernel);
  ge_model->SetTBEKernelStore(tbe_kernel_store);

  std::shared_ptr<domi::ModelTaskDef> model_task = std::make_shared<domi::ModelTaskDef>();
  ge_model->SetModelTaskDef(model_task);

  domi::TaskDef *task_def0 = model_task->add_task();
  task_def0->set_type(static_cast<uint32_t>(ge::ModelTaskType::MODEL_TASK_KERNEL));
  task_def0->set_stream_id(add_op_desc->GetStreamId());
  domi::KernelDef *kernel_def0 = task_def0->mutable_kernel();
  kernel_def0->set_stub_func("stub_func_add");
  kernel_def0->set_args_size(64);
  std::string args(64, '1');
  kernel_def0->set_args(args.data(), 64);
  kernel_def0->set_kernel_name("test");

  domi::KernelContext *context0 = kernel_def0->mutable_context();
  context0->set_kernel_type(0);
  context0->set_op_index(add_op_desc->GetId());
  uint16_t args_offset[9] = {0};
  context0->set_args_offset(args_offset, 9 * sizeof(uint16_t));
  std::string args_format = "{i_desc0}{o_desc0}";
  context0->set_args_format(args_format);

  ge::GeRootModelPtr ge_root_model = std::make_shared<ge::GeRootModel>();
  EXPECT_EQ(ge_root_model->Initialize(graph), ge::SUCCESS);
  ge_root_model->SetModelName(ge_model->GetName());
  ge_root_model->SetSubgraphInstanceNameToModel(graph->GetName(), ge_model);
  return ge_root_model;
}

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
constexpr const char *kAscendCustomOppPathEnv = "ASCEND_CUSTOM_OPP_PATH";
constexpr const char *kMobileCustomOpSoPayloadMarker = "MOBILE_CUSTOM_OP_SO_PAYLOAD_MARKER";
constexpr uint32_t kStandardOmCustomOpsPartition = static_cast<uint32_t>(ge::CUSTOM_OPS);

struct MobilePartitionMemInfoForTest {
  uint32_t type;
  uint32_t mem_offset;
  uint32_t mem_size;
};

struct MobileOmcFileGuardForTest {
  MobileOmcFileGuardForTest(const std::string &output_file_name, const std::string &omc_file_name)
      : output_file(output_file_name), omc_file(omc_file_name) {}

  ~MobileOmcFileGuardForTest() {
    (void)std::remove(output_file.c_str());
    (void)std::remove(omc_file.c_str());
  }

  std::string output_file;
  std::string omc_file;
};

std::string GetCompileArchForTest() {
#if defined(__aarch64__) || defined(__arm64__)
  return "aarch64";
#elif defined(__x86_64__) || defined(__amd64__)
  return "x86_64";
#else
  return "";
#endif
}

std::string GetCrossCompileTargetArchForTest() {
  std::string current_env_os;
  std::string current_env_cpu;
  ge::PluginManager::GetCurEnvPackageOsAndCpuType(current_env_os, current_env_cpu);
  if (current_env_cpu.empty()) {
    current_env_cpu = GetCompileArchForTest();
  }
  std::transform(current_env_cpu.begin(), current_env_cpu.end(), current_env_cpu.begin(),
                 [](char ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); });
  return ((current_env_cpu == "aarch64") || (current_env_cpu == "arm64")) ? "x86_64" : "aarch64";
}

class MobileHostEnvGuardForTest {
 public:
  MobileHostEnvGuardForTest(const std::string &host_env_os, const std::string &host_env_cpu) {
    (void)ge::GetThreadLocalContext().GetOption(ge::OPTION_HOST_ENV_OS, old_host_env_os_);
    (void)ge::GetThreadLocalContext().GetOption(ge::OPTION_HOST_ENV_CPU, old_host_env_cpu_);
    std::map<std::string, std::string> options;
    options[ge::OPTION_HOST_ENV_OS] = host_env_os;
    options[ge::OPTION_HOST_ENV_CPU] = host_env_cpu;
    (void)ge::GetThreadLocalContext().SetGlobalOption(options);
  }

  ~MobileHostEnvGuardForTest() {
    std::map<std::string, std::string> options;
    options[ge::OPTION_HOST_ENV_OS] = old_host_env_os_;
    options[ge::OPTION_HOST_ENV_CPU] = old_host_env_cpu_;
    (void)ge::GetThreadLocalContext().SetGlobalOption(options);
  }

 private:
  std::string old_host_env_os_;
  std::string old_host_env_cpu_;
};

class MobileEnvVarGuardForTest {
 public:
  explicit MobileEnvVarGuardForTest(std::string env_name) : env_name_(std::move(env_name)) {
    const char *env_value = std::getenv(env_name_.c_str());
    if (env_value != nullptr) {
      has_old_value_ = true;
      old_value_ = env_value;
    }
  }

  ~MobileEnvVarGuardForTest() {
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

class MobileTmpDirGuardForTest {
 public:
  explicit MobileTmpDirGuardForTest(std::string dir_path) : dir_path_(std::move(dir_path)) {
    Cleanup();
  }

  ~MobileTmpDirGuardForTest() {
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

bool GetExpectedElfMachineForCpuForTest(const std::string &target_cpu, uint16_t &expected_machine) {
  std::string normalized_target_cpu = target_cpu;
  std::transform(normalized_target_cpu.begin(), normalized_target_cpu.end(), normalized_target_cpu.begin(),
                 [](char ch) { return static_cast<char>(std::tolower(static_cast<unsigned char>(ch))); });
  if ((normalized_target_cpu == "x86_64") || (normalized_target_cpu == "amd64")) {
    expected_machine = 62U;
    return true;
  }
  if ((normalized_target_cpu == "aarch64") || (normalized_target_cpu == "arm64")) {
    expected_machine = 183U;
    return true;
  }
  return false;
}

bool WriteElfHeaderForTest(const std::string &so_path, const uint16_t e_machine) {
  std::vector<uint8_t> elf_header(64U, 0U);
  elf_header[0] = 0x7FU;
  elf_header[1] = static_cast<uint8_t>('E');
  elf_header[2] = static_cast<uint8_t>('L');
  elf_header[3] = static_cast<uint8_t>('F');
  elf_header[4] = 2U;
  elf_header[5] = 1U;
  elf_header[6] = 1U;
  elf_header[16] = 3U;
  elf_header[18] = static_cast<uint8_t>(e_machine & 0xFFU);
  elf_header[19] = static_cast<uint8_t>((e_machine >> 8U) & 0xFFU);

  std::ofstream file(so_path, std::ios::binary | std::ios::trunc);
  if (!file.good()) {
    return false;
  }
  file.write(reinterpret_cast<const char *>(elf_header.data()), static_cast<std::streamsize>(elf_header.size()));
  file.write(kMobileCustomOpSoPayloadMarker, strlen(kMobileCustomOpSoPayloadMarker));
  return file.good();
}

bool ReadUint32Value(const std::vector<char> &data, const size_t offset, uint32_t &value) {
  if ((offset > data.size()) || (data.size() - offset < sizeof(value))) {
    return false;
  }
  return memcpy_s(&value, sizeof(value), data.data() + offset, sizeof(value)) == EOK;
}

std::vector<MobilePartitionMemInfoForTest> ParseMobilePartitions(const std::string &omc_file) {
  std::vector<MobilePartitionMemInfoForTest> partitions;
  std::ifstream file(omc_file.c_str(), std::ios::binary | std::ios::ate);
  EXPECT_TRUE(file.is_open());
  if (!file.is_open()) {
    return partitions;
  }

  const std::streampos file_size_pos = file.tellg();
  EXPECT_GT(file_size_pos, std::streampos(0));
  if (file_size_pos <= std::streampos(0)) {
    return partitions;
  }
  const auto file_size = static_cast<size_t>(file_size_pos);
  std::vector<char> data(file_size);
  (void)file.seekg(0, std::ios::beg);
  (void)file.read(data.data(), static_cast<std::streamsize>(data.size()));
  EXPECT_EQ(file.gcount(), static_cast<std::streamsize>(data.size()));
  if (file.gcount() != static_cast<std::streamsize>(data.size())) {
    return partitions;
  }

  EXPECT_GE(data.size(), kMobileModelFileHeaderSize + kMobilePartitionTableHeaderSize);
  if (data.size() < kMobileModelFileHeaderSize + kMobilePartitionTableHeaderSize) {
    return partitions;
  }

  uint32_t partition_num = 0U;
  const bool has_partition_num = ReadUint32Value(data, kMobileModelFileHeaderSize, partition_num);
  EXPECT_TRUE(has_partition_num);
  if (!has_partition_num) {
    return partitions;
  }

  constexpr uint32_t kMaxMobilePartitionNumForTest = 16U;
  EXPECT_LE(partition_num, kMaxMobilePartitionNumForTest);
  if (partition_num > kMaxMobilePartitionNumForTest) {
    return partitions;
  }

  const size_t table_size =
      kMobilePartitionTableHeaderSize + (kMobilePartitionMemInfoSize * static_cast<size_t>(partition_num));
  EXPECT_LE(kMobileModelFileHeaderSize + table_size, data.size());
  if (kMobileModelFileHeaderSize + table_size > data.size()) {
    return partitions;
  }

  size_t partition_offset = kMobileModelFileHeaderSize + kMobilePartitionTableHeaderSize;
  for (uint32_t i = 0U; i < partition_num; ++i) {
    MobilePartitionMemInfoForTest partition{};
    EXPECT_TRUE(ReadUint32Value(data, partition_offset, partition.type));
    EXPECT_TRUE(ReadUint32Value(data, partition_offset + sizeof(uint32_t), partition.mem_offset));
    EXPECT_TRUE(ReadUint32Value(data, partition_offset + (sizeof(uint32_t) * 2U), partition.mem_size));
    partitions.emplace_back(partition);
    partition_offset += kMobilePartitionMemInfoSize;
  }
  return partitions;
}

bool HasPartitionType(const std::vector<MobilePartitionMemInfoForTest> &partitions, const uint32_t type) {
  for (const auto &partition : partitions) {
    if (partition.type == type) {
      return true;
    }
  }
  return false;
}

bool FileContainsStringForTest(const std::string &file_path, const std::string &target) {
  std::ifstream file(file_path.c_str(), std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return false;
  }
  const auto file_size_pos = file.tellg();
  if (file_size_pos <= std::streampos(0)) {
    return target.empty();
  }
  std::vector<char> data(static_cast<size_t>(file_size_pos));
  (void)file.seekg(0, std::ios::beg);
  (void)file.read(data.data(), static_cast<std::streamsize>(data.size()));
  if (file.gcount() != static_cast<std::streamsize>(data.size())) {
    return false;
  }
  return std::search(data.cbegin(), data.cend(), target.cbegin(), target.cend()) != data.cend();
}

bool IsExpectedMobilePartitionType(const uint32_t type) {
  return (type == kMobileModelDefPartition) || (type == kMobileWeightsDataPartition) ||
         (type == kMobileTaskInfoPartition) || (type == kMobileTaskGraphPartition) ||
         (type == kMobileSignatureInfoPartition) || (type == kMobileModelConfigPartition) ||
         (type == kMobileAippCustomInfoPartition) || (type == kMobileEncryptInfoPartition) ||
         (type == kMobileWeightInfoPartition);
}

class MobileNoSoPortableOpForTest : public ge::EagerExecuteOp, public ge::PortableOp {
 public:
  ge::graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    buffer = {1U, 2U, 3U, 4U};
    return ge::GRAPH_SUCCESS;
  }

  ge::graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return ge::GRAPH_SUCCESS;
  }
};

void RegisterMobileNoSoPortableOpForTest(const std::string &op_type) {
  const auto ret = ge::CustomOpFactory::RegisterCustomOpCreator(
      op_type.c_str(),
      []() -> std::unique_ptr<ge::BaseCustomOp> { return std::make_unique<MobileNoSoPortableOpForTest>(); });
  EXPECT_TRUE((ret == ge::GRAPH_SUCCESS) || (ret == ge::GRAPH_FAILED));
}

}  // namespace

namespace ge {

class UtestMobileModelHelper : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(UtestMobileModelHelper, ModelToMobile) {
  std::string output_file = "mobile_model.om";
  ModelBufferData model;
  MobileModelHelper model_save_helper;
  GeRootModelPtr ge_root_model = GenGeRootModel();
  EXPECT_EQ(model_save_helper.SaveToOmRootModel(ge_root_model, output_file, model, false), ge::SUCCESS);
  EXPECT_NE(model_save_helper.SaveToOmRootModel(ge_root_model, output_file, model, true), ge::SUCCESS);
  GeRootModelPtr ge_root_model_invaild_data_type = GenGeRootModel(true);
  EXPECT_NE(model_save_helper.SaveToOmRootModel(ge_root_model_invaild_data_type, output_file, model, false),
            ge::SUCCESS);
  system("rm -rf mobile_model.omc");
}

TEST_F(UtestMobileModelHelper, SaveToOmRootModelDoesNotWriteSoOrCustomOpsPartitions) {
  const std::string test_case_name = "UtestMobileModelHelper_SaveToOmRootModelDoesNotWriteSoOrCustomOpsPartitions";
  EnvPath().RemoveRfCaseTmpPath(test_case_name);
  const std::string test_work_dir = EnvPath().GetOrCreateCaseTmpPath(test_case_name);
  ASSERT_FALSE(test_work_dir.empty());
  const MobileTmpDirGuardForTest test_work_dir_guard(test_work_dir);

  const std::string output_file = test_work_dir + "/mobile_custom_op_no_so.om";
  const std::string omc_file = output_file + "c";
  (void)std::remove(output_file.c_str());
  (void)std::remove(omc_file.c_str());
  const MobileOmcFileGuardForTest clean_file_guard(output_file, omc_file);

  const std::string op_type = "MobileNoSoPortableOpForTest";
  RegisterMobileNoSoPortableOpForTest(op_type);
  ASSERT_TRUE(CustomOpFactory::IsExistOp(op_type.c_str()));
  BaseCustomOp *custom_op = CustomOpFactory::CreateOrGetCustomOp(op_type.c_str());
  ASSERT_NE(custom_op, nullptr);
  ASSERT_NE(dynamic_cast<PortableOp *>(custom_op), nullptr);

  ModelBufferData model;
  MobileModelHelper model_save_helper;
  GeRootModelPtr ge_root_model = GenGeRootModel(false, op_type);
  ASSERT_NE(ge_root_model, nullptr);
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  const std::string target_arch = GetCrossCompileTargetArchForTest();
  ASSERT_FALSE(target_arch.empty());
  const MobileHostEnvGuardForTest host_env_guard("linux", target_arch);
  const std::string custom_opp_path = test_work_dir + "/custom_opp";
  const MobileTmpDirGuardForTest custom_opp_dir_guard(custom_opp_path);
  const MobileEnvVarGuardForTest custom_opp_env_guard(kAscendCustomOppPathEnv);
  const std::string target_so_dir = custom_opp_path + "/op_graph/lib/linux/" + target_arch;
  ASSERT_EQ(CreateDirectory(target_so_dir), 0);
  const std::string target_so_path = target_so_dir + "/libmobile_custom_op_no_so_ut.so";
  uint16_t expected_machine = 0U;
  ASSERT_TRUE(GetExpectedElfMachineForCpuForTest(target_arch, expected_machine));
  ASSERT_TRUE(WriteElfHeaderForTest(target_so_path, expected_machine));
  ASSERT_EQ(mmSetEnv(kAscendCustomOppPathEnv, custom_opp_path.c_str(), 1), EN_OK);

  ASSERT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), ge::SUCCESS);
  EXPECT_TRUE(OpSoStoreUtils::IsSoBinType(ge_root_model->GetSoInOmFlag(), SoBinType::kCustomOp));
  EXPECT_FALSE(ge_root_model->GetCustomOpSoSet().empty());
  EXPECT_TRUE(std::any_of(
      ge_root_model->GetCustomOpSoSet().cbegin(), ge_root_model->GetCustomOpSoSet().cend(),
      [](const std::string &so_path) { return so_path.find("libmobile_custom_op_no_so_ut.so") != std::string::npos; }));

  ASSERT_EQ(model_save_helper.SaveToOmRootModel(ge_root_model, output_file, model, false), ge::SUCCESS);

  const auto partitions = ParseMobilePartitions(omc_file);
  ASSERT_FALSE(partitions.empty());
  EXPECT_TRUE(HasPartitionType(partitions, kMobileModelDefPartition));
  EXPECT_TRUE(HasPartitionType(partitions, kMobileWeightsDataPartition));
  EXPECT_TRUE(HasPartitionType(partitions, kMobileTaskInfoPartition));
  for (const auto &partition : partitions) {
    EXPECT_TRUE(IsExpectedMobilePartitionType(partition.type))
        << "Unexpected mobile partition type: " << partition.type;
  }
  EXPECT_FALSE(HasPartitionType(partitions, kStandardOmCustomOpsPartition));
  EXPECT_FALSE(FileContainsStringForTest(omc_file, "libmobile_custom_op_no_so_ut.so"));
  EXPECT_FALSE(FileContainsStringForTest(omc_file, kMobileCustomOpSoPayloadMarker));
}
}  // namespace ge
