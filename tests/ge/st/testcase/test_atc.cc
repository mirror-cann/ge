/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "api/atc/main_impl.h"
#include "ge_running_env/path_utils.h"
#include "ge_running_env/atc_utils.h"
#include "ge_running_env/op_reg.h"
#include "graph/ge_local_context.h"
#include "graph/ge_global_options.h"
#include "graph/preprocess/insert_op/insert_aipp_op_util.h"

#include "utils/model_factory.h"
#include "parser/common/op_registration_tbe.h"
#include "ge_graph_dsl/assert/graph_assert.h"
#include "framework/omg/parser/parser_inner_ctx.h"
#include "utils/graph_utils.h"
#include "framework/common/framework_types_internal.h"
#include "init_ge.h"
#include "ge_running_env/ge_running_env_faker.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "common/env_path.h"
#include "depends/mmpa/src/mmpa_stub.h"
#include "faker/space_registry_faker.h"
#include "register/amct_interface.h"
#include "register/amct_registry.h"

#include <fstream>
#include <vector>

DECLARE_bool(help);
DECLARE_bool(raw_ge_options_ignore_unsupported);
DECLARE_int32(virtual_type);
DECLARE_string(model);
DECLARE_string(save_original_model);
DECLARE_string(static_model_ops_lower_limit);

namespace ge {
namespace {
constexpr const char *kStInt32Flag = "st_cmd_flag_int32";

void EnsureStCmdFlagRegistered() {
  static int32_t &int32_flag = flgs::RegisterParamInt32(kStInt32Flag, 0, "st cmd flag int32");
  (void)int32_flag;
}
}  // namespace

class AtcCommonSTest : public AtcTest {
  void SetUp() override {
    EnsureStCmdFlagRegistered();
    GeRunningEnvFaker::SetEnvForOfflineSoPack();
    const ::testing::TestInfo *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    test_case_name = test_info->test_case_name();
    test_work_dir = EnvPath().GetOrCreateCaseTmpPath(test_case_name);
    auto base_path = EnvPath().GetAirBasePath();
    std::string command = "find " + base_path + "/build_st -name " + "libfmk_parser.so";
    char retmsg[1024];
    (void)gert::SuperSystem(command.c_str(), retmsg, sizeof(retmsg));
    std::string fmk_path = retmsg;

    std::string cmd = "mkdir -p " + base_path + "/tests/ge/opp/built-in/framework/tensorflow/";
    system(cmd.c_str());
    cmd = "cp -rf " + fmk_path + " " + base_path + "/tests/ge/opp/built-in/framework/tensorflow/";
    system(cmd.c_str());
    dlopen(fmk_path.c_str(), RTLD_NOW | RTLD_GLOBAL);
  }

  void TearDown() override {
    // 用于重置Flags_<option_name>
    AtcTest::TearDown();
    EnvPath().RemoveRfCaseTmpPath(test_case_name);
  }

 public:
  std::string WriteRawOptionsJson(const std::string &file_name, const std::string &content) const {
    auto raw_dir = PathJoin(GetRunPath().c_str(), "temp");
    Mkdir(raw_dir.c_str());
    const auto raw_path = PathJoin(raw_dir.c_str(), file_name.c_str());
    std::ofstream raw_file(raw_path);
    raw_file << content;
    return raw_path;
  }

  std::string WriteAippConfig(const std::string &file_name, const std::string &aipp_mode) const {
    auto cfg_dir = PathJoin(GetRunPath().c_str(), "temp");
    Mkdir(cfg_dir.c_str());
    const auto cfg_path = PathJoin(cfg_dir.c_str(), file_name.c_str());
    std::ofstream cfg_file(cfg_path);
    cfg_file << "aipp_op {\n"
             << "  aipp_mode: " << aipp_mode << "\n"
             << "  related_input_rank: 0\n"
             << "  max_src_image_size: 752640\n"
             << "  input_format: RGB888_U8\n"
             << "  csc_switch: false\n"
             << "}\n";
    return cfg_path;
  }

  int32_t RunAtcWithRawOptions(const std::string &output_name, const std::string &raw_options_path,
                               const std::vector<std::string> &extra_args = {}, bool add_default_input_shape = true,
                               bool add_default_framework = true, bool add_default_soc_version = true) const {
    auto om_path = PathJoin(GetRunPath().c_str(), "temp");
    Mkdir(om_path.c_str());
    om_path = PathJoin(om_path.c_str(), output_name.c_str());

    std::vector<std::string> args = {"atc", "--model=st_run_data/origin_model/add.pb", "--output=" + om_path,
                                     "--input_format=NCHW", "--raw_ge_options=" + raw_options_path};
    if (add_default_framework) {
      args.push_back("--framework=3");
    }
    if (add_default_soc_version) {
      args.push_back("--soc_version=Ascend310");
    }
    if (add_default_input_shape) {
      args.push_back("--input_shape=Placeholder_1:1,256,256,3");
    }
    args.insert(args.end(), extra_args.begin(), extra_args.end());

    std::vector<char *> argv;
    argv.reserve(args.size());
    for (auto &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    return main_impl(static_cast<int32_t>(argv.size()), argv.data());
  }

  void ResetAtcFlagsForTest() {
    AtcTest::TearDown();
  }

  std::string test_case_name;
  std::string test_work_dir;
};

char g_handleStub;
bool g_dlsymError = false;
bool g_amctError = false;
using amctStatus = int32_t;
// 待后续与原始流程一起删除
amctStatus amctGraphCalibration(ge::Graph &graph, const std::map<std::string, std::string> &options) {
  if (g_amctError) {
    return static_cast<amctStatus>(ge::GRAPH_FAILED);
  } else {
    return static_cast<amctStatus>(ge::GRAPH_NOT_CHANGED);
  }
}

class MockMmpa : public ge::MmpaStubApiGe {
 public:
  void *DlOpen(const char *file_name, int32_t mode) override {
    if (string("libamctacl.so") == file_name) {
      return (void *)&g_handleStub;
    }

    if (string(file_name).find("liboptiling.so") != std::string::npos) {
      return (void *)&g_handleStub;
    }
    return MmpaStubApiGe::DlOpen(file_name, mode);
  }

  void *DlSym(void *handle, const char *func_name) override {
    if (g_dlsymError) {
      return nullptr;
    }
    if (std::string(func_name) == "amctGraphCalibration") {
      return (void *)&amctGraphCalibration;
    }
    return dlsym(handle, func_name);
  }
};

class AmctCalibrationTest : public IAmctCalibration {
 public:
  graphStatus Calibrate(Graph &graph, const std::map<std::string, std::string> &options) override {
    if (g_amctError) {
      return ge::GRAPH_FAILED;
    }
    return ge::GRAPH_SUCCESS;
  }
};

class AmctCalibrationTest2 : public IAmctCalibration {
 public:
  graphStatus Calibrate(Graph &graph, const std::map<std::string, std::string> &options) override {
    return ge::GRAPH_SUCCESS;
  }
};

TEST_F(AtcCommonSTest, pb_model_common_1) {
  std::vector<OpRegistrationData> registrationDatas = domi::OpRegistry::Instance()->registrationDatas;
  for (OpRegistrationData reg_data : registrationDatas) {
    if (reg_data.GetFrameworkType() == domi::TENSORFLOW) {
      (void)ge::OpRegistrationTbe::Instance()->Finalize(reg_data);
      (void)domi::OpRegistry::Instance()->Register(reg_data);
    }
  }

  GetParserContext().default_out_nodes.push_back(std::make_pair("add_test_1", 0));

  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string op_name_map = "--op_name_map=st_run_data/config/opname_map.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      //"--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
      const_cast<char *>(op_name_map.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_status_check_error) {
  std::vector<OpRegistrationData> registrationDatas = domi::OpRegistry::Instance()->registrationDatas;
  for (OpRegistrationData reg_data : registrationDatas) {
    if (reg_data.GetFrameworkType() == domi::TENSORFLOW) {
      (void)ge::OpRegistrationTbe::Instance()->Finalize(reg_data);
      (void)domi::OpRegistry::Instance()->Register(reg_data);
    }
  }

  GetParserContext().default_out_nodes.push_back(std::make_pair("add_test_1", 0));

  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string op_name_map = "--op_name_map=st_run_data/config/opname_map.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      //"--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--status_check=3",
      const_cast<char *>(op_name_map.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  MmpaStub::GetInstance().SetImpl(std::make_shared<MockMmpa>());
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  MmpaStub::GetInstance().Reset();
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_set_out_tensor_names) {
  std::vector<OpRegistrationData> registrationDatas = domi::OpRegistry::Instance()->registrationDatas;
  for (OpRegistrationData reg_data : registrationDatas) {
    if (reg_data.GetFrameworkType() == domi::TENSORFLOW) {
      (void)ge::OpRegistrationTbe::Instance()->Finalize(reg_data);
      (void)domi::OpRegistry::Instance()->Register(reg_data);
    }
  }

  GetParserContext().out_tensor_names.push_back("out_tensor");

  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string op_name_map = "--op_name_map=st_run_data/config/opname_map.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      //"--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
      const_cast<char *>(op_name_map.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, ConvertPbModel_Ok_SetOutNodeAndOutType) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_2");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string keep_dtype = "--keep_dtype=st_run_data/config/keep_dtype.cfg";
  domi::GetContext().final_out_nodes_map = {std::make_pair("add_test_1:0", std::make_pair("add_test_1", 0))};
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--out_nodes=add_test_1:0",
      "--soc_version=Ascend310",
      "--output_type=add_test_1:0:FP16",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--display_model_info=1",
      "--precision_mode=force_fp16",
      "--input_fp16_nodes=Placeholder_1",
      "--save_original_model=true",
      const_cast<char *>(keep_dtype.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  domi::GetContext().final_out_nodes_map.clear();
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

void MainImplSetUp() {
  std::string opp_path = "./opp/";
  system(("mkdir -p " + opp_path).c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);

  std::string scene_path = opp_path + "scene.info";
  system(("touch " + scene_path).c_str());
  system(("echo 'os=linux' > " + scene_path).c_str());
  system(("echo 'arch=x86_64' >> " + scene_path).c_str());

  system("pwd");

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_x86_proto_path = opp_path + "built-in" + "/op_proto/lib/linux/x86_64/";
  system(("mkdir -p " + inner_x86_proto_path).c_str());
  inner_x86_proto_path += "libopsproto_rt2.0.so";
  system(("touch " + inner_x86_proto_path).c_str());
  system(("echo 'ops proto x86 ' > " + inner_x86_proto_path).c_str());

  std::string inner_x86_tiling_path = opp_path + "built-in" + "/op_impl/ai_core/tbe/op_tiling/lib/linux/x86_64/";
  system(("mkdir -p " + inner_x86_tiling_path).c_str());
  inner_x86_tiling_path += "libopmaster_rt2.0.so";
  system(("touch " + inner_x86_tiling_path).c_str());
  system(("echo 'op tiling_x86 ' > " + inner_x86_tiling_path).c_str());
}

void MainImplTearDown() {
  std::string opp_path = "./opp/";
  system(("rm -r " + opp_path).c_str());
}
void CheckPrecisionModeParamValid_Failed_WhenValueInvalid(const std::string &om_arg, const std::string &output_arg) {
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--deterministic=1",
                  "--input_format=NCHW",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64",
                  "--precision_mode=invalid"};
  (void)main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  std::string ge_option;
  EXPECT_NE(ge::GetThreadLocalContext().GetOption(ge::PRECISION_MODE, ge_option), ge::GRAPH_SUCCESS);
}

void CheckPrecisionModeV2ParamValid_Failed_WhenValueInvalid(const std::string &om_arg, const std::string &output_arg) {
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--deterministic=1",
                  "--input_format=NCHW",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64",
                  "--precision_mode_v2=invalid"};
  (void)main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  std::string ge_option;
  EXPECT_NE(ge::GetThreadLocalContext().GetOption(ge::PRECISION_MODE_V2, ge_option), ge::GRAPH_SUCCESS);
}

void CheckPrecisionModev2ParamValid_Failed_WhenConfigBoth(const std::string &om_arg, const std::string &output_arg) {
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--deterministic=1",
                  "--input_format=NCHW",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64",
                  "--precision_mode_v2=fp16",
                  "--precision_mode=force_fp16"};
  (void)main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  std::string ge_option;
  EXPECT_NE(ge::GetThreadLocalContext().GetOption(ge::PRECISION_MODE_V2, ge_option), ge::GRAPH_SUCCESS);
  EXPECT_NE(ge::GetThreadLocalContext().GetOption(ge::PRECISION_MODE, ge_option), ge::GRAPH_SUCCESS);
  FLAGS_precision_mode = "";
  FLAGS_precision_mode_v2 = "";
}

void CheckPrecisionModeParamValid_Success(const std::string &om_arg, const std::string &output_arg) {
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--deterministic=1",
                  "--input_format=NCHW",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64",
                  "--precision_mode=force_fp16"};
  ge::GEFinalize();
  (void)main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  std::string ge_option;
  EXPECT_EQ(ge::GetThreadLocalContext().GetOption(ge::PRECISION_MODE, ge_option), ge::GRAPH_SUCCESS);
  EXPECT_STREQ(ge_option.c_str(), "force_fp16");
  FLAGS_precision_mode = "";
  FLAGS_precision_mode_v2 = "";
}

void CheckPrecisionModeV2ParamValid_Success(const std::string &om_arg, const std::string &output_arg) {
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--deterministic=1",
                  "--deterministic_level=2",
                  "--input_format=NCHW",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64",
                  "--precision_mode_v2=fp16"};
  (void)main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  std::string ge_option;
  EXPECT_EQ(ge::GetThreadLocalContext().GetOption(ge::PRECISION_MODE_V2, ge_option), ge::GRAPH_SUCCESS);
  EXPECT_STREQ(ge_option.c_str(), "fp16");
  EXPECT_EQ(ge::GetThreadLocalContext().GetOption("ge.deterministicLevel", ge_option), ge::GRAPH_SUCCESS);
  EXPECT_STREQ(ge_option.c_str(), "2");
}
std::string Generatefile(const std::string &file_type, const std::string &file_name) {
  std::string pwd = __FILE__;
  std::size_t idx = pwd.find_last_of("/");
  pwd = pwd.substr(0, idx);
  std::string om_file = pwd + "/" + file_name;
  return file_type + om_file;
}
// 这些用例需要操作文件，性能较差，为了提升性能，将用例合并。
TEST_F(AtcCommonSTest, CheckPrecisionModeAndPrecisionModeV2) {
  MainImplSetUp();
  std::string om_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = Generatefile("--output=", "tmp");
  ge::GetThreadLocalContext().SetGlobalOption({});
  ge::GetThreadLocalContext().SetSessionOption({});
  ge::GetThreadLocalContext().SetGraphOption({});
  CheckPrecisionModeParamValid_Failed_WhenValueInvalid(om_arg, output_arg);
  CheckPrecisionModeV2ParamValid_Failed_WhenValueInvalid(om_arg, output_arg);
  CheckPrecisionModev2ParamValid_Failed_WhenConfigBoth(om_arg, output_arg);
  CheckPrecisionModeParamValid_Success(om_arg, output_arg);
  CheckPrecisionModeV2ParamValid_Success(om_arg, output_arg);
  remove(Generatefile("", "tmp.om").c_str());
  MainImplTearDown();
}
TEST_F(AtcCommonSTest, pb_keep_dtype_invalid) {
  unsetenv("ASCEND_OPP_PATH");
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_2");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string keep_dtype = "--keep_dtype=invalid";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--out_nodes=add_test_1:0",
      "--soc_version=Ascend310",
      "--output_type=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--display_model_info=1",
      "--precision_mode=force_fp16",
      "--input_fp16_nodes=Placeholder_1",
      "--save_original_model=true",
      const_cast<char *>(keep_dtype.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_fp16_and_NCIHWC0) {
  ge::char_t current_path[MMPA_MAX_PATH] = {'\0'};
  getcwd(current_path, MMPA_MAX_PATH);
  mmSetEnv("ASCEND_WORK_PATH", current_path, 1);
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_2");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend310",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--input_fp16_nodes=Placeholder_1",
                  "--is_input_adjust_hw_layout=true",
                  "--is_output_adjust_hw_layout=true",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  std::string check_report_path = current_path;
  check_report_path += "/check_result.json";
  EXPECT_EQ(mmAccess(check_report_path.c_str()), EN_OK);
  unsetenv("ASCEND_WORK_PATH");
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
  CHECK_GRAPH(PreRunBegin) {
    EXPECT_EQ(graph->GetDirectNodesSize(), 4);
  };
}

TEST_F(AtcCommonSTest, keep_dtype_has_invalid_node) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string keep_dtype = "--keep_dtype=st_run_data/config/keep_dtype.cfg";

  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--sparsity=1",
                  "--allow_hf32=true",
                  const_cast<char *>(keep_dtype.c_str()),
                  "--status_check=0"};
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, keep_dtype_has_invalid_node_type) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string keep_dtype = "--keep_dtype=st_run_data/config/keep_dtype_invalid_nodetype.cfg";

  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--sparsity=1",
                  "--allow_hf32=true",
                  const_cast<char *>(keep_dtype.c_str()),
                  "--status_check=0"};
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_auto_tune_mode) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend310",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--auto_tune_mode=RL,GA",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_exeom_for_nano_mode) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_exeom_for_nano");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  "--mode=30",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--out_nodes=add_test_1:0",
                  "--framework=3",  // FrameworkType
                  "--soc_version=Ascend035",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--input_fp16_nodes=Placeholder_1",
                  "--is_input_adjust_hw_layout=true",
                  "--is_output_adjust_hw_layout=true",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(IsFile((om_path + ".dbg").c_str()), true);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_exeom_for_nano_mode_fail01) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_exeom_for_nano");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  "--mode=30",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--out_nodes=add_test_1:0",
                  "--framework=3",  // FrameworkType
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--input_fp16_nodes=Placeholder_1",
                  "--is_input_adjust_hw_layout=true",
                  "--is_output_adjust_hw_layout=true",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_exeom_for_nano_mode_fail02) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_exeom_for_nano");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--out_nodes=add_test_1:0",
                  "--framework=3",  // FrameworkType
                  "--soc_version=Ascend035",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--input_fp16_nodes=Placeholder_1",
                  "--is_input_adjust_hw_layout=true",
                  "--is_output_adjust_hw_layout=true",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_exeom_for_nano_mode_fail03) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      "--mode=30",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=0",  // FrameworkType
      "--soc_version=Ascend035",
      "--weight=",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_only_precheck) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string op_name_map = "--op_name_map=st_run_data/config/opname_map.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--mode=3",       // FrameworkType
      "--framework=3",  // FrameworkType
      "--weight=st_run_data/not_exist",
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
      const_cast<char *>(op_name_map.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}
TEST_F(AtcCommonSTest, pb_model_precheck_fail) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  "--model=3",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=relu:0",
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--output_type=FP32",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_amct_interface) {
  ReInitGe();

  // 注册具体的AmctCalibration
  REG_AMCT_CALIBRATION(AmctCalibrationTest);
  REG_AMCT_CALIBRATION(AmctCalibrationTest2);  // 仅保留首次注册的

  MmpaStub::GetInstance().SetImpl(std::make_shared<MockMmpa>());
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--status_check=0",
                  "--compression_optimize_conf=./"};
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_amct_interface_dlsymError) {
  ReInitGe();
  g_dlsymError = true;
  MmpaStub::GetInstance().SetImpl(std::make_shared<MockMmpa>());
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--status_check=0",
                  "--compression_optimize_conf=./"};
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  g_dlsymError = false;
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_amct_interface_amctError) {
  ReInitGe();
  g_amctError = true;
  MmpaStub::GetInstance().SetImpl(std::make_shared<MockMmpa>());
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--status_check=0",
                  "--compression_optimize_conf=./"};
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  g_amctError = false;
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_with_weight_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string op_name_map = "--op_name_map=st_run_data/config/opname_map.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--weight=st_run_data/not_exist",
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
      const_cast<char *>(op_name_map.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

// TEST_F(AtcCommonSTest, pb_model_generate_om_model_autofuse) {
//   mmSetEnv("ASCEND_OPP_PATH", (EnvPath().GetAscendInstallPath() + "/opp").c_str(), 1);
//   mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
//   ReInitGe();
//   auto om_path = PathJoin(GetRunPath().c_str(), "temp");
//   Mkdir(om_path.c_str());
//   om_path = PathJoin(om_path.c_str(), "pb_common_1");
//   std::string model_arg = "--model=st_run_data/origin_model/add.pb";
//   std::string output_arg = "--output="+om_path;
//   char *argv[] = {"atc",
//                   const_cast<char *>(model_arg.c_str()),
//                   const_cast<char *>(output_arg.c_str()),
//                   "--framework=3", // FrameworkType
//                   "--out_nodes=add_test_1:0",
//                   "--soc_version=Ascend910B2",
//                   "--output_type=FP32",
//                   "--input_shape=Placeholder_1:1,256,256,3",
//                   "--sparsity=1",
//                   "--allow_hf32=true",
//                   "--status_check=0"
//   };
//   DUMP_GRAPH_WHEN("PreRunBegin")
//   auto ret = main_impl(sizeof(argv)/sizeof(argv[0]), argv);
//   EXPECT_EQ(ret, 0);
//   ReInitGe(); // the main_impl will call GEFinalize, so re-init after call it
//   unsetenv("AUTOFUSE_FLAGS");
//   unsetenv("ASCEND_OPP_PATH");
// }

TEST_F(AtcCommonSTest, pb_model_sparse_weight) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--sparsity=1",
                  "--allow_hf32=true",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_weight_compress_both_exist) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string compress_weight_conf = "--compress_weight_conf=st_run_data/config/compress_weight_nodes.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--out_nodes=add_test_1:0",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--enable_compress_weight=true",
      const_cast<char *>(compress_weight_conf.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_weight_compress_conf_invalid) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string compress_weight_conf = "--compress_weight_conf=st_run_data/invalid";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--out_nodes=add_test_1:0",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--enable_compress_weight=true",
      const_cast<char *>(compress_weight_conf.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_weight_compress_enable_invalid) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--out_nodes=add_test_1:0",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--enable_compress_weight=invalid",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_out_node_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string op_name_map = "--op_name_map=st_run_data/config/opname_map.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--weight=st_run_data/not_exist",
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=invalid:0",
      "--input_shape=Placeholder_1:1,256,256,3",
      const_cast<char *>(op_name_map.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_out_node_leak_port) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string op_name_map = "--op_name_map=st_run_data/config/opname_map.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--weight=st_run_data/not_exist",
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1",
      "--input_shape=Placeholder_1:1,256,256,3",
      const_cast<char *>(op_name_map.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_out_node_port_not_digit) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string op_name_map = "--op_name_map=st_run_data/config/opname_map.cfg";
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--weight=st_run_data/not_exist",
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:a",
      "--input_shape=Placeholder_1:1,256,256,3",
      const_cast<char *>(op_name_map.c_str()),
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, onnx_model_common) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "onnx_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/test.onnx";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=5",  // FrameworkType
      "--weight=st_run_data/not_exist",
      "--out_nodes=Conv_0:0;Add_0:0",
      "--soc_version=Ascend310",
      "--output_type=Conv_0:0:FP32;Add_0:0:FP16",
      "--input_shape=x:1,3,640,640",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, onnx_model_common_2) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "onnx_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/test.onnx";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=5",  // FrameworkType
      "--weight=st_run_data/not_exist",
      "--out_nodes=Conv_0:0;Add_0:0",
      "--soc_version=Ascend310",
      "--input_format=NCHW",
      "--output_type=Conv_0:0:FP32;Add_0:0:FP16",
      "--input_shape=x:1,3,640,640",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, onnx_model_input_format_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "onnx_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/test.onnx";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=5",  // FrameworkType
      "--weight=st_run_data/not_exist",
      "--out_nodes=Conv_0:0;Add_0:0",
      "--soc_version=Ascend310",
      "--input_format=NC1HWC0",
      "--output_type=Conv_0:0:FP32;Add_0:0:FP16",
      "--input_shape=x:1,3,640,640",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, mindspore_model_common) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "ms_1");
  auto path = ModelFactory::GenerateModel_1(true, true);
  std::string model_arg = "--model=" + path;
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=1",  // FrameworkType
                  "--out_nodes=relu:0",
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--output_type=FP32",
                  "--status_check=0",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);

  char *argv_invalid_name[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=1",  // FrameworkType
      "--out_nodes=relu:0",
      "--soc_version=Ascend310",
      "--input_format=NCHW",
      "--output_type=FP32",
      "--status_check=0",
      "--host_env_os=linux",
      "--host_env_cpu=x86_64",
      "--input_shape=data_invalid:1,1",
  };
  ret = main_impl(sizeof(argv_invalid_name) / sizeof(argv_invalid_name[0]), argv_invalid_name);
  EXPECT_NE(ret, 0);

  char *argv_invalid_type[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=1",  // FrameworkType
      "--out_nodes=relu:0",
      "--soc_version=Ascend310",
      "--input_format=NCHW",
      "--output_type=FP32",
      "--status_check=0",
      "--host_env_os=linux",
      "--host_env_cpu=x86_64",
      "--input_shape=relu1:1,1",
  };
  ret = main_impl(sizeof(argv_invalid_type) / sizeof(argv_invalid_type[0]), argv_invalid_type);
  EXPECT_NE(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it

  CHECK_GRAPH(PreRunBegin) {
    EXPECT_EQ(graph->GetDirectNodesSize(), 5);
    auto data = graph->FindNode("data1");
    EXPECT_NE(data, nullptr);
    EXPECT_NE(data->GetOpDesc()->MutableInputDesc(0), nullptr);

    auto origin_shape = data->GetOpDesc()->MutableInputDesc(0)->GetOriginShape();
    auto origin_format = data->GetOpDesc()->MutableInputDesc(0)->GetOriginFormat();
    bool is_origin_format_set = false;
    (void)AttrUtils::GetBool(data->GetOpDesc()->MutableInputDesc(0), ATTR_NAME_ORIGIN_FORMAT_IS_SET,
                             is_origin_format_set);
    EXPECT_EQ(origin_format, FORMAT_NC1HWC0);
    EXPECT_TRUE(is_origin_format_set);
    EXPECT_EQ(origin_shape.GetDims(), std::vector<int64_t>({1, 2, 3, 4, 5}));
  };
}

TEST_F(AtcCommonSTest, TestAtc_Ok_MindsporeModelWithRefData) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "ms_1");
  auto path = ModelFactory::GenerateModel_refdata(false, false);
  std::string model_arg = "--model=" + path;
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=1",  // FrameworkType
                  "--soc_version=Ascend310P",
                  "--status_check=0",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

/*
 * -------------------------------------------------
 *             data
 *              |
 *            netouput
 * -------------------------------------------------
 * 测试步骤
 * 1.构造单个计算图1，data和netputput直连
 * 2.调用atc接口生成om
 * 预期结果
 * 1.图1·编译成功，PreRunAfterOptimize2后图内有三个节点（data/memcpy/netoutput）
 */
TEST_F(AtcCommonSTest, mindspore_model_data_to_netoutput) {
  // 设置环境变量
  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "ms_data_to_netoutput");
  auto path = ModelFactory::GenerateModel_data_to_netoutput(false, false);
  std::string model_arg = "--model=" + path;
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=1",  // FrameworkType
                  "--out_nodes=relu:0",
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--output_type=FP32",
                  "--status_check=0",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64",
                  "--log=debug"};

  DUMP_GRAPH_WHEN("PreRunAfterOptimize2")
  ge::GEFinalize();
  GeRunningEnvFaker ge_env;
  ge_env.InstallDefault();
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it

  CHECK_GRAPH(PreRunAfterOptimize2) {
    EXPECT_EQ(graph->GetDirectNodesSize(), 3);
  };
  // 清理环境变量
  mmSetEnv(kEnvValue, "", 1);
}

TEST_F(AtcCommonSTest, mindspore_model_atc_scalar_inputshape) {
  // 设置环境变量
  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "ms_data_to_netoutput");
  auto path = ModelFactory::GenerateModel_data_to_netoutput(false, false);
  std::string model_arg = "--model=" + path;
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=1",  // FrameworkType
                  "--out_nodes=relu:0",
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--output_type=FP32",
                  "--input_shape=data;data1:-1,3,2",
                  "--status_check=0",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64"};

  DUMP_GRAPH_WHEN("PreRunAfterOptimize2")
  ge::GEFinalize();
  GeRunningEnvFaker ge_env;
  ge_env.InstallDefault();
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  char *argv2[] = {"atc",
                   const_cast<char *>(model_arg.c_str()),
                   const_cast<char *>(output_arg.c_str()),
                   "--framework=1",  // FrameworkType
                   "--out_nodes=relu:0",
                   "--soc_version=Ascend310",
                   "--input_format=NCHW",
                   "--output_type=FP32",
                   "--input_shape=:;data1:-1,3,2",
                   "--status_check=0",
                   "--host_env_os=linux",
                   "--host_env_cpu=x86_64"};
  ret = main_impl(sizeof(argv2) / sizeof(argv2[0]), argv2);
  EXPECT_NE(ret, 0);
  char *argv3[] = {"atc",
                   const_cast<char *>(model_arg.c_str()),
                   const_cast<char *>(output_arg.c_str()),
                   "--framework=1",  // FrameworkType
                   "--out_nodes=relu:0",
                   "--soc_version=Ascend310",
                   "--input_format=NCHW",
                   "--output_type=FP32",
                   "--input_shape=data:-1,3,2;data1:",
                   "--status_check=0",
                   "--host_env_os=linux",
                   "--host_env_cpu=x86_64"};
  ret = main_impl(sizeof(argv3) / sizeof(argv3[0]), argv3);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it

  // 清理环境变量
  mmSetEnv(kEnvValue, "", 1);
}
// 校验输入hint shape的index校验失败
TEST_F(AtcCommonSTest, pb_model_generate_om_model_autofuse_shpae_index_invalid) {
  mmSetEnv("ASCEND_OPP_PATH", (EnvPath().GetAscendInstallPath() + "/opp").c_str(), 1);
  mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_hint_shape=-1:[2]",
                  "--sparsity=1",
                  "--allow_hf32=true",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
  unsetenv("AUTOFUSE_FLAGS");
  unsetenv("ASCEND_OPP_PATH");
}

// 校验输入hint shape不匹配，符号推到失败场景
TEST_F(AtcCommonSTest, pb_model_generate_om_model_autofuse_dyna_shape_failed) {
  mmSetEnv("ASCEND_OPP_PATH", (EnvPath().GetAscendInstallPath() + "/opp").c_str(), 1);
  mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:-1",
                  "--input_hint_shape=0:[2];1:[3]",
                  "--sparsity=1",
                  "--allow_hf32=true",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
  unsetenv("AUTOFUSE_FLAGS");
  unsetenv("ASCEND_OPP_PATH");
}

// test input hint shape and input dynamic param failed
TEST_F(AtcCommonSTest, pb_model_generate_om_model_hint_shape_with_dyna_param_failed) {
  mmSetEnv("ASCEND_OPP_PATH", (EnvPath().GetAscendInstallPath() + "/opp").c_str(), 1);
  mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:-1",
                  "--input_hint_shape=0:[3];1:[3]",
                  "--dynamic_dims=4;8;16;64",
                  "--sparsity=1",
                  "--allow_hf32=true",
                  "--status_check=0"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
  unsetenv("AUTOFUSE_FLAGS");
  unsetenv("ASCEND_OPP_PATH");
}

// depends on mindspore success
TEST_F(AtcCommonSTest, om_convert_to_json) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "ms_1_linux_x86_64");

  // test convert to json
  std::string om_arg = "--om=" + om_path + ".om";
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {"atc", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str()),
                                  "--mode=1", "--status_check=0"};
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(IsFile((om_path + ".json").c_str()), true);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, om_fusion_convert_to_json) {
  auto om_path = PathJoin(GetRunPath().c_str(), "models");
  om_path = PathJoin(om_path.c_str(), "ms1_1");

  // test convert to json
  std::string om_arg = "--om=" + om_path + ".txt";
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {"atc", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str()),
                                  "--mode=5"};
  EXPECT_EQ(main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv), SUCCESS);
  EXPECT_TRUE(IsFile((om_path + ".json").c_str()));
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, om_fusion_convert_to_json_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "models");
  om_path = PathJoin(om_path.c_str(), "ms1_1");

  // test convert to json
  std::string om_arg = "--om=" + om_path + ".prototxt";
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {"atc", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str()),
                                  "--mode=5"};
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, -1);  // invalid om
  EXPECT_TRUE(IsFile((om_path + ".json").c_str()));
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, empty_om_fusion_convert_to_json_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "models");
  om_path = PathJoin(om_path.c_str(), "ms1_1");

  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {"atc", const_cast<char *>(json_arg.c_str()), "--mode=5"};
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, om_convert_to_json_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "ms_1");

  // test convert to json
  std::string om_arg = "--om=st_run_data/origin_model/not_exist.om";
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {
      "atc",
      const_cast<char *>(om_arg.c_str()),
      const_cast<char *>(json_arg.c_str()),
      "--mode=1",
  };
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, om_display_info) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "ms_1_linux_x86_64");

  // test convert to json
  std::string om_arg = "--om=" + om_path + ".om";
  char *convert_to_json_argv[] = {"atc", const_cast<char *>(om_arg.c_str()), "--mode=6", "--status_check=0"};
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, origin_model_convert_to_json_without_dump_mode) {
  ReInitGe();
  std::string model_arg = "--om=st_run_data/origin_model/add.pb";
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "pb_json_1");
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {
      "atc",           const_cast<char *>(model_arg.c_str()), const_cast<char *>(json_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--mode=1",
  };
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(IsFile((om_path + ".json").c_str()), true);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, origin_model_convert_to_json_with_dump_mode) {
  ReInitGe();
  std::string model_arg = "--om=st_run_data/origin_model/add.pb";
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "pb_json_1");
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(json_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--mode=1",
      "--dump_mode=1",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(IsFile((om_path + ".json").c_str()), true);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, origin_model_convert_to_json_invalid_framework) {
  ReInitGe();
  std::string model_arg = "--om=st_run_data/origin_model/add.pb";
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "pb_json_1");
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(json_arg.c_str()),
      "--framework=10",  // FrameworkType
      "--mode=1",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, origin_model_convert_to_json_invalid_dump_mode) {
  ReInitGe();
  std::string model_arg = "--om=st_run_data/origin_model/add.pb";
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "pb_json_1");
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(json_arg.c_str()),
      "--framework=10",  // FrameworkType
      "--mode=1",
      "--dump_mode=10",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pbtxt_convert_to_json) {
  ReInitGe();
  std::string model_arg = "--om=st_run_data/origin_model/origin.txt";
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "pbtxt_json");
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(json_arg.c_str()),
      "--mode=5",
  };
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(IsFile((om_path + ".json").c_str()), true);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pbtxt_convert_to_json_file_not_exist) {
  ReInitGe();
  std::string model_arg = "--om=st_run_data/invalid";
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "pbtxt_json");
  std::string json_arg = "--json=" + om_path + ".json";
  char *convert_to_json_argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(json_arg.c_str()),
      "--mode=5",
  };
  auto ret = main_impl(sizeof(convert_to_json_argv) / sizeof(convert_to_json_argv[0]), convert_to_json_argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_shape_dynamic) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--soc_version=Ascend310",
                  "--output_type=FP32",
                  "--out_nodes=add_test_1:0",
                  "--input_shape_range=Placeholder_1:[-1]",
                  "--log=debug"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
  CHECK_GRAPH(PreRunBegin) {
    EXPECT_EQ(graph->GetDirectNodesSize(), 4);
  };
}

// TEST_F(AtcCommonSTest, pb_model_generate_om_model_autofuse_dyna_shape_suc) {
//   mmSetEnv("ASCEND_OPP_PATH", (EnvPath().GetAscendInstallPath() + "/opp").c_str(), 1);
//   mmSetEnv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
//   ReInitGe();
//   auto om_path = PathJoin(GetRunPath().c_str(), "temp");
//   Mkdir(om_path.c_str());
//   om_path = PathJoin(om_path.c_str(), "pb_common_1");
//   std::string model_arg = "--model=st_run_data/origin_model/add.pb";
//   std::string output_arg = "--output="+om_path;
//   char *argv[] = {"atc",
//                   const_cast<char *>(model_arg.c_str()),
//                   const_cast<char *>(output_arg.c_str()),
//                   "--framework=3", // FrameworkType
//                   "--out_nodes=add_test_1:0",
//                   "--soc_version=Ascend910B2",
//                   "--output_type=FP32",
//                   "--input_shape=Placeholder_1:-1",
//                   "--input_hint_shape=0:[1];1:[1]",
//                   "--sparsity=1",
//                   "--allow_hf32=true",
//                   "--status_check=0"
//   };
//   DUMP_GRAPH_WHEN("PreRunBegin")
//   auto ret = main_impl(sizeof(argv)/sizeof(argv[0]), argv);
//   EXPECT_EQ(ret, 0);
//   ReInitGe(); // the main_impl will call GEFinalize, so re-init after call it
//   unsetenv("AUTOFUSE_FLAGS");
//   unsetenv("ASCEND_OPP_PATH");
// }

TEST_F(AtcCommonSTest, pb_model_input_shape_range) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape_range=Placeholder_1:[-1]",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
  CHECK_GRAPH(PreRunBegin) {
    EXPECT_EQ(graph->GetDirectNodesSize(), 4);
  };
}

TEST_F(AtcCommonSTest, pb_model_input_shape_with_invalid_input_op) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_shape=add_test_1:[-1]",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape_range=add_test_1:[-1]",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_shape_range_node_not_exist) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape_range=invalid:[1~8,256,256,-1]",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_shape_range_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape_range=add_test_1:[1~-1,256,256,-1]",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_shape_range_lead_brace) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape_range=add_test_1:1~8,256,256,-1]",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_shape_range_lead_node) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape_range=:[1~8,256,256,-1]",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_shape_range_more_node) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_1");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape_range=a:b:[1~8,256,256,-1]",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_op_precision_mode_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--op_precision_mode=st_run_data/not_exist",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_modify_mixlist_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--modify_mixlist=st_run_data/not_exist",
      "--precision_mode=force_fp16",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_op_select_implmode_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--op_select_implmode=invalid",
      "--optypelist_for_implmode=invalid",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_optypelist_for_implmode_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--op_select_implmode=invalid",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_framework_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=10",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_framework_caffe_fail) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=0",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_output_exceed_max_len) {
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string om_path(4097, 'a');
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_output_not_file) {
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=st_run_data/";

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_tensorflow_format_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NC1HWC0",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_caffe_format_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=0",  // FrameworkType
      "--input_format=NHWC",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_caffe_model_failed) {
  auto base_path = PathJoin(GetRunPath().c_str(), "temp/");
  Mkdir(base_path.c_str());

  mmSetEnv("ASCEND_OPP_PATH", base_path.c_str(), 1);

  auto test_so_path = PathJoin(GetRunPath().c_str(), "temp/framework");
  Mkdir(test_so_path.c_str());

  test_so_path = PathJoin(test_so_path.c_str(), "built-in");
  Mkdir(test_so_path.c_str());

  test_so_path = PathJoin(test_so_path.c_str(), "caffe");
  Mkdir(test_so_path.c_str());

  auto command = "touch " + test_so_path + "/lib_caffe_parser.so";
  system(command.c_str());

  auto om_path = PathJoin(base_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=0",  // FrameworkType
      "--weight=st_run_data/origin_model/add.pb",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
  unsetenv("ASCEND_OPP_PATH");
}

TEST_F(AtcCommonSTest, pb_log_level_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--log=invalid",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, mode_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_abnormal");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--mode=10",
      "--out_nodes=add_test_1:0",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, single_op) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "singleop");
  std::string single_op = "--singleop=st_run_data/json/single_op/add_op.json";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(single_op.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--allow_hf32=true",
      "--soc_version=Ascend310",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

// TEST_F(AtcCommonSTest, single_op_output_invalid) {
//   std::string single_op = "--singleop=st_run_data/json/single_op/add_op.json";

//   char *argv[] = {"atc",
//                   const_cast<char *>(single_op.c_str()),
//                   "--soc_version=Ascend310",
//                   };
//   auto ret = main_impl(sizeof(argv)/sizeof(argv[0]), argv);
//   EXPECT_EQ(ret, -1);
//   ReInitGe(); // the main_impl will call GEFinalize, so re-init after call it
// }

TEST_F(AtcCommonSTest, single_op_op_precision_mode_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "singleop");
  std::string single_op = "--singleop=st_run_data/json/single_op/add_op.json";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(single_op.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--soc_version=Ascend310",
      "--op_precision_mode=st_run_data/not_exist",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, single_op_modify_mixlist_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "singleop");
  std::string single_op = "--singleop=st_run_data/json/single_op/add_op.json";
  std::string output_arg = "--output=" + om_path;

  char *argv[] = {
      "atc",
      const_cast<char *>(single_op.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--soc_version=Ascend310",
      "--modify_mixlist=st_run_data/not_exist",
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_is_input_adjust_hw_layout_invalid) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--out_nodes=add_test_1:0",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--input_fp16_nodes=Placeholder_1",
      "--is_input_adjust_hw_layout=invalid",
      "--is_output_adjust_hw_layout=true",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_fp16_nodes_not_exist) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--out_nodes=add_test_1:0",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--input_fp16_nodes=invalid",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_model_input_fp16_nodes_not_data) {
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--out_nodes=add_test_1:0",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--input_shape=Placeholder_1:1,256,256,3",
      "--input_fp16_nodes=add_test_1",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_input_shape_negative) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:-1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_input_shape_float) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1.1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_input_shape_node_not_exist) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=invalid:1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_input_shape_content_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=,:",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_input_shape_type_not_data) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=add_test_1:1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_input_shape_not_digit) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:a,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_input_shape_exceed) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:2147483648,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_output_type_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=invalid",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_output_type_node_not_exist) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=invalid:0:FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_output_type_content_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=invalid:FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_output_type_not_digit) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=invalid:a:FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, pb_output_type_port_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--output_type=invalid:-1:FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, virtual_type_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {
      "atc",
      const_cast<char *>(model_arg.c_str()),
      const_cast<char *>(output_arg.c_str()),
      "--framework=3",  // FrameworkType
      "--input_format=NCHW",
      "--soc_version=Ascend310",
      "--virtual_type=2",
      "--output_type=invalid:-1:FP32",
      "--out_nodes=add_test_1:0",
      "--input_shape=Placeholder_1:1,256,256,3",
  };
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, SingCheck_Param_Failed) {
  char *argv[] = {"atc", "--singleop=add_int.json", "--output=./", "--display_model_info=1",
                  "--soc_version=\"Ascend310\""};
  // --singleop与--display_model_info参数冲突
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);

  char *argv1[] = {"atc", "--singleop=add_int.json", "--output=", "--display_model_info=0",
                   "--soc_version=\"Ascend310\""};
  // --singleop中--output参数必选
  ret = main_impl(sizeof(argv1) / sizeof(argv1[0]), argv1);
  EXPECT_EQ(ret, -1);
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
}

TEST_F(AtcCommonSTest, GeFlags_param_ok01) {
  char *argv[] = {"atc", "--virtual_type=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_ok02) {
  char *argv[] = {"atc", "--virtual_type=1"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_virtual_type, 1);
}

TEST_F(AtcCommonSTest, GeFlags_param_ok03) {
  char *argv[] = {"atc", "--model=model_value"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_model, "model_value");
}

TEST_F(AtcCommonSTest, GeFlags_param_ok04) {
  char *argv[] = {"atc", "--model", "model_value"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_model, "model_value");
}

TEST_F(AtcCommonSTest, GeFlags_param_jit_compile_ok) {
  char *argv[] = {"atc", "--jit_compile=2"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(ge::flgs::GetUserOptions()["jit_compile"], "2");
}

TEST_F(AtcCommonSTest, GeFlags_param_optimization_switch_ok) {
  char *argv[] = {"atc", "--optimization_switch=PassA:on;PassB:off"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(ge::flgs::GetUserOptions()["optimization_switch"], "PassA:on;PassB:off");
}

TEST_F(AtcCommonSTest, GeFlags_param_static_model_ops_lower_limit_ok) {
  char *argv[] = {"atc", "--static_model_ops_lower_limit=-1"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(ge::flgs::GetUserOptions()["static_model_ops_lower_limit"], "-1");
}

TEST_F(AtcCommonSTest, GeFlags_param_raw_ge_options_ok) {
  char *argv[] = {"atc", "--raw_ge_options=/tmp/options.json"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(ge::flgs::GetUserOptions()["raw_ge_options"], "/tmp/options.json");
}

TEST_F(AtcCommonSTest, GeFlags_param_raw_ge_options_ignore_unsupported_ok) {
  char *argv[] = {"atc", "--raw_ge_options_ignore_unsupported=true"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(ge::flgs::GetUserOptions()["raw_ge_options_ignore_unsupported"], "true");
}

TEST_F(AtcCommonSTest, GeFlags_teardown_resets_raw_ge_options_flag_state) {
  char *argv[] = {"atc", "--raw_ge_options=/tmp/options.json", "--raw_ge_options_ignore_unsupported=true"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_TRUE(FLAGS_raw_ge_options_ignore_unsupported);

  EXPECT_EQ(ge::flgs::SetFlagValue("save_original_model", "yes"), ge::flgs::GF_SUCCESS);
  EXPECT_EQ(ge::flgs::SetFlagValue("static_model_ops_lower_limit", "-2"), ge::flgs::GF_SUCCESS);
  EXPECT_EQ(FLAGS_save_original_model, "yes");
  EXPECT_EQ(FLAGS_static_model_ops_lower_limit, "-2");

  ResetAtcFlagsForTest();

  EXPECT_FALSE(FLAGS_raw_ge_options_ignore_unsupported);
  EXPECT_TRUE(FLAGS_save_original_model.empty());
  EXPECT_TRUE(FLAGS_static_model_ops_lower_limit.empty());
  EXPECT_TRUE(FLAGS_raw_ge_options.empty());
  EXPECT_EQ(ge::flgs::GetUserOptions().count("raw_ge_options_ignore_unsupported"), 0U);
  EXPECT_EQ(ge::flgs::GetUserOptions().count("raw_ge_options"), 0U);
  EXPECT_EQ(ge::flgs::GetUserOptions().count("save_original_model"), 0U);
  EXPECT_EQ(ge::flgs::GetUserOptions().count("static_model_ops_lower_limit"), 0U);
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_unsupported_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_unsupported.json", R"({
    "compile options": {
      "graph": {
        "ge.unsupported.option": "1"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_unsupported", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_without_compile_options_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_without_compile_options.json", R"({
    "execute options": {
      "graph": {
        "ge.unsupported.option": "1"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_without_compile_options", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_not_exist_failed) {
  const auto raw_path = PathJoin(GetRunPath().c_str(), "raw_ge_options_not_exist.json");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_not_exist", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_invalid_json_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_invalid_json.json", "{ invalid json");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_invalid_json", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_unsupported_level_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_unsupported_level.json", R"({
    "compile options": {
      "execute": {
        "ge.jit_compile": "2"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_unsupported_level", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_level_not_object_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_level_not_object.json", R"({
    "compile options": {
      "graph": "ge.jit_compile=2"
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_level_not_object", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_empty_key_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_empty_key.json", R"({
    "compile options": {
      "graph": {
        "": "1"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_empty_key", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_non_string_value_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_non_string_value.json", R"({
    "compile options": {
      "graph": {
        "ge.jit_compile": 2
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_non_string_value", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_jit_compile_invalid_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_jit_compile_invalid.json", R"({
    "compile options": {
      "graph": {
        "ge.jit_compile": "3"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_jit_compile_invalid", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_static_model_ops_lower_limit_invalid) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_static_model_ops_lower_limit_invalid.json", R"({
    "compile options": {
      "graph": {
        "ge.exec.static_model_ops_lower_limit": "-2"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_static_model_ops_lower_limit_invalid", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_deterministic_invalid_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_deterministic_invalid.json", R"({
    "compile options": {
      "graph": {
        "ge.deterministic": "2"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_deterministic_invalid", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_virtual_type_invalid_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_virtual_type_invalid.json", R"({
    "compile options": {
      "graph": {
        "ge.virtual_type": "2"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_virtual_type_invalid", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_allow_hf32_invalid_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_allow_hf32_invalid.json", R"({
    "compile options": {
      "graph": {
        "ge.exec.allow_hf32": "maybe"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_allow_hf32_invalid", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_save_original_model_invalid_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_save_original_model_invalid.json", R"({
    "compile options": {
      "graph": {
        "ge.saveOriginalModel": "yes"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_save_original_model_invalid", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_output_type_node_format_success) {
  domi::GetContext().final_out_nodes_map = {std::make_pair("add_test_1:0", std::make_pair("add_test_1", 0))};
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_output_type_node_format.json", R"({
    "compile options": {
      "graph": {
        "ge.outputDatatype": "add_test_1:0:FP16"
      }
    }
  })");
  const int32_t ret =
      RunAtcWithRawOptions("raw_ge_options_output_type_node_format", raw_path, {"--out_nodes=add_test_1:0"});
  EXPECT_EQ(ret, 0);
  domi::GetContext().final_out_nodes_map.clear();
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_output_type_invalid_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_output_type_invalid.json", R"({
    "compile options": {
      "graph": {
        "ge.outputDatatype": "invalid"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_output_type_invalid", raw_path);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_input_shape_success) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_input_shape.json", R"({
    "compile options": {
      "graph": {
        "ge.inputShape": "Placeholder_1:1,256,256,3"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_input_shape", raw_path, {}, false);
  EXPECT_EQ(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_ignore_unsupported_success) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_ignore_unsupported_success.json", R"({
    "compile options": {
      "graph": {
        "ge.unsupported.option": "1",
        "ge.jit_compile": "2"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_ignore_unsupported_success", raw_path,
                                           {"--raw_ge_options_ignore_unsupported=true"});
  EXPECT_EQ(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_graph_overrides_session_global) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_graph_overrides_session_global.json", R"({
    "compile options": {
      "global": {
        "ge.exec.static_model_ops_lower_limit": "-2"
      },
      "session": {
        "ge.exec.static_model_ops_lower_limit": "-2"
      },
      "graph": {
        "ge.exec.static_model_ops_lower_limit": "-1"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_graph_overrides_session_global", raw_path);
  EXPECT_EQ(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_cli_overrides_raw) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_cli_overrides_raw.json", R"({
    "compile options": {
      "graph": {
        "ge.socVersion": "InvalidSoc"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_cli_overrides_raw", raw_path);
  EXPECT_EQ(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_required_soc_version_not_replaced_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_required_soc_version_not_replaced.json", R"({
    "compile options": {
      "graph": {
        "ge.socVersion": "Ascend310"
      }
    }
  })");
  const int32_t ret =
      RunAtcWithRawOptions("raw_ge_options_required_soc_version_not_replaced", raw_path, {}, true, true, false);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_required_framework_not_replaced_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_required_framework_not_replaced.json", R"({
    "compile options": {
      "graph": {
        "ge.frameworkType": "3"
      }
    }
  })");
  const int32_t ret =
      RunAtcWithRawOptions("raw_ge_options_required_framework_not_replaced", raw_path, {}, true, false, true);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_invalid_value_not_skipped_by_cli_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_invalid_value_not_skipped_by_cli.json", R"({
    "compile options": {
      "graph": {
        "ge.jit_compile": "3"
      }
    }
  })");
  const int32_t ret =
      RunAtcWithRawOptions("raw_ge_options_invalid_value_not_skipped_by_cli", raw_path, {"--jit_compile=2"});
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_execute_options_ignored) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_execute_options_ignored.json", R"({
    "compile options": {
      "graph": {
        "ge.jit_compile": "2"
      }
    },
    "execute options": {
      "graph": {
        "ge.unsupported.option": "1"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_execute_options_ignored", raw_path);
  EXPECT_EQ(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_optimization_switch_success) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_optimization_switch.json", R"({
    "compile options": {
      "graph": {
        "ge.optimizationSwitch": "PassA:on"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_optimization_switch", raw_path);
  EXPECT_EQ(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_display_model_info_success) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_display_model_info.json", R"({
    "compile options": {
      "graph": {
        "ge.display_model_info": "0"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_display_model_info", raw_path);
  EXPECT_EQ(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_raw_ge_options_om2_unsupported_option_failed) {
  const auto raw_path = WriteRawOptionsJson("raw_ge_options_om2_unsupported_option.json", R"({
    "compile options": {
      "graph": {
        "ge.saveOriginalModel": "true"
      }
    }
  })");
  const int32_t ret = RunAtcWithRawOptions("raw_ge_options_om2_unsupported_option", raw_path, {"--mode=7"});
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_om2_dynamic_aipp_rejected) {
  const auto cfg_path = WriteAippConfig("om2_dynamic_aipp.cfg", "dynamic");
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "om2_dynamic_aipp_rejected");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  std::string insert_conf_arg = "--insert_op_conf=" + cfg_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",
                  "--mode=7",
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  const_cast<char *>(insert_conf_arg.c_str())};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_om2_static_aipp_validate_passes) {
  EXPECT_EQ(InsertAippOpUtil::ValidateStaticAippOnly(""), SUCCESS);
  const auto cfg_path = WriteAippConfig("om2_static_aipp.cfg", "static");
  EXPECT_EQ(InsertAippOpUtil::ValidateStaticAippOnly(cfg_path), SUCCESS);
}

TEST_F(AtcCommonSTest, GeFlags_param_static_model_ops_lower_limit_invalid) {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "static_model_ops_lower_limit_invalid");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",
                  "--input_format=NCHW",
                  "--soc_version=Ascend310",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--static_model_ops_lower_limit=-2"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  ReInitGe();
}

TEST_F(AtcCommonSTest, GeFlags_param_err01) {
  char *argv[] = {"atc", "--dump_mode=3"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_err02) {
  char *argv[] = {"atc", "--status_check=3"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_err03) {
  char *argv[] = {"atc", "--op_compiler_cache_mode=\"atc\""};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_err04) {
  char *argv[] = {"atc", "--framework=6"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_err05) {
  char *argv[] = {"atc", "--op_debug_level=3.6"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_bool_type_error) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=invalid_bool"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_FALSE(FLAGS_help);
}

TEST_F(AtcCommonSTest, GeFlags_param_int32_type_error) {
  char *argv[] = {"atc", "--st_cmd_flag_int32=debug"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_display_model_info_range_error) {
  char *argv[] = {"atc", "--display_model_info=2"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_jit_compile_invalid) {
  char *argv[] = {"atc", "--jit_compile=3"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_help01) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(AtcCommonSTest, GeFlags_param_help02) {
  FLAGS_help = false;
  char *argv[] = {"atc", "-help"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

/*
TEST_F(AtcCommonSTest, GeFlags_param_help03) {
  FLAGS_help = false;
  char *argv[] = {"atc",
                  "-h"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}
*/

TEST_F(AtcCommonSTest, GeFlags_param_help04) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=true"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(AtcCommonSTest, GeFlags_param_help05) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=True"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(AtcCommonSTest, GeFlags_param_help06) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=T"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(AtcCommonSTest, GeFlags_param_help07) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=y"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(AtcCommonSTest, GeFlags_param_help08) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=yes"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(AtcCommonSTest, GeFlags_param_help09) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=1"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(AtcCommonSTest, GeFlags_param_help10) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=false"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(AtcCommonSTest, GeFlags_param_help11) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=f"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(AtcCommonSTest, GeFlags_param_help12) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=n"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(AtcCommonSTest, GeFlags_param_help13) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=no"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(AtcCommonSTest, GeFlags_param_help14) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(AtcCommonSTest, GeFlags_param_help15) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=invalid_bool"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(AtcCommonSTest, GeFlags_param_include_dash) {
  char *argv[] = {"atc", "--virtual-type=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_param_single_minus) {
  char *argv[] = {"atc", "-virtual_type=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_flag_value_empty01) {
  char *argv[] = {"atc", "--virtual_type="};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_flag_value_empty02) {
  char *argv[] = {"atc", "--model="};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_no_flag_value) {
  char *argv[] = {"atc", "--virtual_type"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_data_type_error) {
  char *argv[] = {"atc", "--virtual_type=string_value"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_flag_name_error01) {
  char *argv[] = {"atc", "--virtual_type_err=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_flag_name_error02) {
  char *argv[] = {"atc", "--virtual_type_err"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_non_option_parameter01) {
  char *argv[] = {"atc", "virtual_type=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_non_option_parameter02) {
  char *argv[] = {"atc", "virtual_type=0", "--mode=0", "--framework=5"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_mode_1_framework_error) {
  char *argv[] = {
      "atc",
      "--mode=1",
      "--framework=1",
  };
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_framework_0_weight_error) {
  char *argv[] = {"atc", "--mode=0", "--framework=0", "--weight=***", "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_framework_0_weight_none) {
  char *argv[] = {"atc", "--mode=0", "--framework=0", "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_framework_3_weight_warn) {
  std::string weight = "--weight=st_run_data/origin_model/add.pb";
  char *argv[] = {"atc", "--mode=0", "--framework=3", const_cast<char *>(weight.c_str()),
                  "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_framework_5_weight_warn) {
  std::string weight = "--weight=st_run_data/origin_model/add.pb";
  char *argv[] = {"atc", "--mode=0", "--framework=5", const_cast<char *>(weight.c_str()),
                  "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_no_param_framework_error) {
  char *argv[] = {"atc"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_singleop_framework_error) {
  char *argv[] = {
      "atc"
      "--singleop=op.json",
      "--framework=5"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_singleop_mode_error) {
  char *argv[] = {
      "atc"
      "--singleop=op.json",
      "--mode=1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, GeFlags_singleop_insertopconf_error) {
  char *argv[] = {
      "atc"
      "--singleop=op.json",
      "--insert_op_conf=op.cfg"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, CheckDisplayModelInfo_Failed) {
  dlog_setlevel(GE_MODULE_NAME, DLOG_DEBUG, 0);
  ReInitGe();

  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  om_path = PathJoin(om_path.c_str(), "ms_1_linux_x86_64");

  // test convert to json
  std::string om_arg = "--om=" + om_path + ".om";
  std::string json_arg = "--json=" + om_path + ".json";

  char *argv[] = {"atc",
                  "--mode=6",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(json_arg.c_str()),
                  "--dump_mode=1"

  };
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  ReInitGe();
  dlog_setlevel(GE_MODULE_NAME, DLOG_ERROR, 0);
}

TEST_F(AtcCommonSTest, GeFlags_set_input_hint_shpae_failed) {
  char *argv[] = {
      "atc"
      "--input_hint_shape=0:[3];1:[3]"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(AtcCommonSTest, TestAtc_Ok_Om2) {
  mmSetEnv("ASCEND_WORK_PATH", test_work_dir.c_str(), 1);
  const auto ascend_install_path = EnvPath().GetAscendInstallPath();
  setenv("ASCEND_HOME_PATH", ascend_install_path.c_str(), 1);
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_common_2");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",  // FrameworkType
                  "--mode=7",
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend310",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--input_fp16_nodes=Placeholder_1",
                  "--is_input_adjust_hw_layout=true",
                  "--is_output_adjust_hw_layout=true"};
  DUMP_GRAPH_WHEN("PreRunBegin")
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  unsetenv("ASCEND_WORK_PATH");
  unsetenv("ASCEND_HOME_PATH");
  ReInitGe();  // the main_impl will call GEFinalize, so re-init after call it
  CHECK_GRAPH(PreRunBegin) {
    EXPECT_EQ(graph->GetDirectNodesSize(), 4);
  };
}

class MockMmpaDlOpenFail : public ge::MmpaStubApiGe {
 public:
  void *DlOpen(const char *file_name, int32_t mode) override {
    if (string("libamctacl.so") == file_name) {
      return nullptr;
    }
    return MmpaStubApiGe::DlOpen(file_name, mode);
  }
  int32_t DlClose(void *handle) override {
    return 0;
  }
};

/**
 * 用例描述：测试CallAmctInterface中mmDlopen返回nullptr且mmDlerror返回nullptr时的处理
 * 预置条件：
 *   1. 使用MockMmpaDlOpenFail打桩mmDlopen，使libamctacl.so加载失败返回nullptr
 *   2. 提前调用dlerror()清除错误状态，使mmDlerror返回nullptr
 * 测试步骤：
 *   1. 构造atc命令行参数，包含--compression_optimize_conf=./触发AMCT流程
 *   2. 调用main_impl执行图编译
 * 预期结果：
 *   1. main_impl返回非0值（编译失败）
 */
TEST_F(AtcCommonSTest, pb_model_amct_interface_dlopen_fail_no_error) {
  ReInitGe();
  (void)dlerror();
  MmpaStub::GetInstance().SetImpl(std::make_shared<MockMmpaDlOpenFail>());
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_amct_dlopen_fail");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--status_check=0",
                  "--compression_optimize_conf=./"
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  MmpaStub::GetInstance().Reset();
  ReInitGe();
}

/**
 * 用例描述：测试CallAmctInterface中mmDlopen返回nullptr且mmDlerror返回有效错误信息时的处理
 * 预置条件：
 *   1. 使用MockMmpaDlOpenFail打桩mmDlopen，使libamctacl.so加载失败返回nullptr
 *   2. 先调用dlopen加载一个不存在的so，使dlerror()产生有效错误信息
 * 测试步骤：
 *   1. 构造atc命令行参数，包含--compression_optimize_conf=./触发AMCT流程
 *   2. 调用main_impl执行图编译
 * 预期结果：
 *   1. main_impl返回非0值（编译失败）
 */
TEST_F(AtcCommonSTest, pb_model_amct_interface_dlopen_fail_with_error) {
  ReInitGe();
  (void)dlopen("nonexistent_lib_for_error.so", RTLD_NOW);
  MmpaStub::GetInstance().SetImpl(std::make_shared<MockMmpaDlOpenFail>());
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_amct_dlopen_fail_err");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--status_check=0",
                  "--compression_optimize_conf=./"
  };
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  MmpaStub::GetInstance().Reset();
  ReInitGe();
}

TEST_F(AtcCommonSTest, pb_model_load_custom_op_with_legacy_so) {
  char_t orig_opp[MMPA_MAX_PATH] = {'\0'};
  bool had_opp = (mmGetEnv("ASCEND_OPP_PATH", orig_opp, MMPA_MAX_PATH) == EN_OK);

  std::string test_opp = PathJoin(GetRunPath().c_str(), "legacy_so_opp");
  std::string plugin_dir = test_opp + "/built-in/framework/tensorflow/";
  std::string lib_dir = test_opp + "/built-in/op_proto/lib/linux/x86_64/";
  std::string cmd = "mkdir -p " + plugin_dir + " && mkdir -p " + lib_dir;
  system(cmd.c_str());

  std::vector<std::string> fake_sos = {"libcustom_op1.so", "libcustom_op2_legacy.so",
                                        "libcustom_op3.so", "libcustom_op4_legacy.so"};
  for (const auto &so_name : fake_sos) {
    std::ofstream(plugin_dir + so_name) << "fake";
  }

  setenv("ASCEND_OPP_PATH", test_opp.c_str(), 1);
  ReInitGe();
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  om_path = PathJoin(om_path.c_str(), "pb_legacy_so");
  std::string model_arg = "--model=st_run_data/origin_model/add.pb";
  std::string output_arg = "--output=" + om_path;
  char *argv[] = {"atc",
                  const_cast<char *>(model_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--framework=3",
                  "--out_nodes=add_test_1:0",
                  "--soc_version=Ascend910B2",
                  "--output_type=FP32",
                  "--input_shape=Placeholder_1:1,256,256,3",
                  "--status_check=0"};
  auto ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);

  if (had_opp) {
    setenv("ASCEND_OPP_PATH", orig_opp, 1);
  } else {
    unsetenv("ASCEND_OPP_PATH");
  }
  cmd = "rm -rf " + test_opp;
  system(cmd.c_str());
  ReInitGe();
}
}  // namespace ge
