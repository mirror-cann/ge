/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/model_serialize.h"
#include "graph/utils/graph_utils_ex.h"
#include "api/aclgrph/option_utils.h"
#include "api/atc/main_impl.h"
#include "api/atc/atc_flags.h"
#include "framework/omg/parser/parser_factory.h"
#include "framework/omg/parser/model_parser.h"
#include "framework/omg/parser/weights_parser.h"
#include "framework/common/framework_types_internal.h"
#include "graph/types.h"
#include "graph/ge_global_options.h"
#include <algorithm>
#include <climits>
#include <fstream>
#include <set>

#include "ge_running_env/atc_utils.h"
#include "proto/ge_ir.pb.h"
#include "framework/common/scope_guard.h"
#include "graph/ge_local_context.h"
#include "graph/passes/graph_builder_utils.h"
#include "dlog_pub.h"
#include "graph_metadef/common/plugin/plugin_manager.h"
#include "ge_runtime_stub/include/common/share_graph.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "register/optimization_option_registry.h"
#include "register/amct_interface.h"
#include "register/amct_registry.h"
#include "depends/mmpa/src/mmpa_stub.h"

using namespace domi;
using namespace ge;

DECLARE_bool(help);
DECLARE_int32(virtual_type);
DECLARE_string(model);
DECLARE_string(oo_level);
const char *const kEnvName = "ASCEND_OPP_PATH";
const string kOpsProto = "libopsproto_rt2.0.so";
const string kOpMaster = "libopmaster_rt2.0.so";
const string kInner = "built-in";
const string kOpsProtoPath = "/op_proto/lib/linux/x86_64/";
const string kOpMasterPath = "/op_impl/ai_core/tbe/op_tiling/lib/linux/x86_64/";

class UtestMain : public AtcTest {};
namespace {
class AtcFileFactory {
 public:
  static std::string GetFileRealName(const std::string &file_name);
  static std::string Generatefile1(const std::string &file_type, const std::string &file_name);
  static std::string GenerateModel(const std::string &file_type, const std::string &file_name,
                                   bool with_fusion = false);
  static void RemoveFile(const char *path) {
    remove(path);
  }
};

std::string AtcFileFactory::GetFileRealName(const std::string &file_name) {
  std::string pwd = __FILE__;
  std::size_t idx = pwd.find_last_of("/");
  pwd = pwd.substr(0, idx);
  return pwd + "/" + file_name;
}

std::string AtcFileFactory::Generatefile1(const std::string &file_type, const std::string &file_name) {
  std::string pwd = __FILE__;
  std::size_t idx = pwd.find_last_of("/");
  pwd = pwd.substr(0, idx);
  std::string om_file = pwd + "/" + file_name;
  return file_type + om_file;
}

std::string AtcFileFactory::GenerateModel(const std::string &file_type, const std::string &file_name,
                                          bool with_fusion) {
  std::string pwd = __FILE__;
  std::size_t idx = pwd.find_last_of("/");
  pwd = pwd.substr(0, idx);
  const std::string om_file = pwd + "/" + file_name;
  Model model("model_name", "custom version3.0");
  {
    auto computeGraph = std::make_shared<ComputeGraph>("graph_name");
    auto op_desc = std::make_shared<OpDesc>("test", "test");
    op_desc->AddInputDesc(GeTensorDesc(GeShape({12, 32, 64, 64}), FORMAT_NCHW, DT_FLOAT));
    computeGraph->AddNode(op_desc);
    auto out_put = std::make_shared<OpDesc>(NODE_NAME_NET_OUTPUT, NETOUTPUT);
    out_put->AddInputDesc(GeTensorDesc(GeShape({12, 32, 64, 64}), FORMAT_NCHW, DT_FLOAT));
    computeGraph->AddNode(out_put);
    model.SetGraph(computeGraph);
  }

  if (with_fusion) {
    std::vector<Buffer> b_list(2U);
    ge::proto::OpDef fusion_op_def;
    auto &fusion_attr_map = *fusion_op_def.mutable_attr();
    fusion_attr_map["fusion_scope"].set_i(0x00010000);
    const auto group_op_l1_id = fusion_op_def.SerializeAsString();
    b_list[0] = Buffer::CopyFrom(reinterpret_cast<const uint8_t *>(group_op_l1_id.data()), group_op_l1_id.length());
    fusion_attr_map["fusion_scope"].set_i(0x00000001);
    const auto group_op_ub_id = fusion_op_def.SerializeAsString();
    b_list[1] = Buffer::CopyFrom(reinterpret_cast<const uint8_t *>(group_op_ub_id.data()), group_op_ub_id.length());
    (void)AttrUtils::SetListBytes(model, MODEL_ATTR_FUSION_MODEL_DEF, b_list);
  }

  ModelSerialize serialize;
  proto::ModelDef model_def;
  serialize.SerializeModel(model, false, model_def);  // success
  GraphUtils::WriteProtoToTextFile(model_def, om_file.c_str());
  return file_type + om_file;
}

class StubTensorFlowModelParser : public domi::ModelParser {
 public:
  StubTensorFlowModelParser() {}
  ~StubTensorFlowModelParser() {}
  domi::Status Parse(const char *file, ge::Graph &graph) {
    DEF_GRAPH(g1) {
      CHAIN(NODE("cons1", "Const")->NODE("add1", "Add")->NODE("NetOutput", "NetOutput"));
    };
    graph = ToGeGraph(g1);
    return ge::SUCCESS;
  }
  domi::Status ParseFromMemory(const char *data, uint32_t size, ge::ComputeGraphPtr &graph) {
    return ge::SUCCESS;
  }
  domi::Status ParseFromMemory(const char *data, uint32_t size, ge::Graph &graph) {
    return ge::SUCCESS;
  }
  domi::Status ParseProto(const google::protobuf::Message *proto, ge::ComputeGraphPtr &graph) {
    return ge::SUCCESS;
  }
  domi::Status ParseProtoWithSubgraph(const google::protobuf::Message *proto, GetGraphCallback callback,
                                      ge::ComputeGraphPtr &graph) {
    return ge::SUCCESS;
  }
  ge::DataType ConvertToGeDataType(const uint32_t type) {
    return DT_FLOAT;
  }
  domi::Status ParseAllGraph(const google::protobuf::Message *root_proto, ge::ComputeGraphPtr &root_graph) {
    return ge::SUCCESS;
  }
};

REGISTER_MODEL_PARSER_CREATOR(TENSORFLOW, StubTensorFlowModelParser);
REGISTER_MODEL_PARSER_CREATOR(CAFFE, StubTensorFlowModelParser);
REGISTER_MODEL_PARSER_CREATOR(MINDSPORE, StubTensorFlowModelParser);
REGISTER_MODEL_PARSER_CREATOR(ONNX, StubTensorFlowModelParser);

class StubWeightsParser : public domi::WeightsParser {
 public:
  StubWeightsParser() {}
  ~StubWeightsParser() {}
  domi::Status Parse(const char *file, ge::Graph &graph) {
    return ge::SUCCESS;
  }
  domi::Status ParseFromMemory(const char *input, uint32_t length, ge::ComputeGraphPtr &graph) {
    return ge::SUCCESS;
  }
};
REGISTER_WEIGHTS_PARSER_CREATOR(CAFFE, StubWeightsParser);
REGISTER_WEIGHTS_PARSER_CREATOR(ONNX, StubWeightsParser);
REGISTER_WEIGHTS_PARSER_CREATOR(TENSORFLOW, StubWeightsParser);

static std::string ConstructOppEnv() {
  std::string path = GetModelPath();
  path = path.substr(0, path.rfind('/'));
  path = path.substr(0, path.rfind('/'));
  path = path.substr(0, path.rfind('/') + 1);
  std::string opp_path = path + "opp_oo_test/";
  system(("mkdir -p " + opp_path).c_str());
  mmSetEnv(kEnvName, opp_path.c_str(), 1);
  std::string scene_path = opp_path + "scene.info";
  system(("touch " + scene_path).c_str());
  system(("echo 'os=linux' > " + scene_path).c_str());
  system(("echo 'arch=x86_64' >> " + scene_path).c_str());
  system("pwd");
  std::string inner_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_proto_path).c_str());
  std::string inner_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_tiling_path).c_str());
  return opp_path;
}
}  // namespace

TEST_F(UtestMain, MainImplTest_socversion_and_mode_fail01) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=30",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend910B",
                  "--input_format=NCHW",
                  "--allow_hf32=true"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_socversion_and_mode_fail02) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend035",
                  "--input_format=NCHW",
                  "--allow_hf32=true"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_socversion_and_mode_match) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=30",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend035A",
                  "--input_format=NCHW",
                  "--allow_hf32=true"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_singleop) {
  std::string singleop_arg = AtcFileFactory::Generatefile1("--singleop=", "add_int.json");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "./");
  char *argv[] = {"atc", const_cast<char *>(singleop_arg.c_str()), const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\""};
  // 未打桩，st里面覆盖全流程，当前ut只是覆盖此文件相关代码
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
}

TEST_F(UtestMain, MainImplTest_singleop_allow_hf32) {
  std::string singleop_arg = AtcFileFactory::Generatefile1("--singleop=", "add_int.json");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "./");
  char *argv[] = {"atc", const_cast<char *>(singleop_arg.c_str()), const_cast<char *>(output_arg.c_str()),
                  "--allow_hf32=true", "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
}

TEST_F(UtestMain, MainImplTest_singleopFail) {
  std::string singleop_arg = AtcFileFactory::Generatefile1("--singleop=", "add_int.json");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "./");
  char *argv[] = {"atc", const_cast<char *>(singleop_arg.c_str()), const_cast<char *>(output_arg.c_str()),
                  "--display_model_info=1", "--soc_version=\"Ascend310\""};
  // --singleop与--display_model_info参数冲突
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);

  char *argv1[] = {"atc", const_cast<char *>(singleop_arg.c_str()), "--output=", "--display_model_info=0",
                   "--soc_version=\"Ascend310\""};
  // --singleop中--output参数必选
  ret = main_impl(sizeof(argv1) / sizeof(argv1[0]), argv1);
  EXPECT_EQ(ret, -1);
}

TEST_F(UtestMain, MainImplTest_log_level_err) {
  char *argv[] = {"atc", "--log=1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_mem_info) {
  char *argv[] = {"atc", "--auto_tune_mode=\"RA\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_generate_om_model_tf) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW",
                  "--allow_hf32=true"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(flgs::GetUserOptions().find("allow_hf32") != flgs::GetUserOptions().end());
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_global_options) {
  std::string opp_path = "./opp/";
  system(("mkdir -p " + opp_path).c_str());
  setenv(kEnvName, opp_path.c_str(), 1);

  std::string scene_path = opp_path + "scene.info";
  system(("touch " + scene_path).c_str());
  system(("echo 'os=linux' > " + scene_path).c_str());
  system(("echo 'arch=x86_64' >> " + scene_path).c_str());

  system("pwd");

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_x86_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_x86_proto_path).c_str());
  inner_x86_proto_path += kOpsProto;
  system(("touch " + inner_x86_proto_path).c_str());
  system(("echo 'ops proto x86 ' > " + inner_x86_proto_path).c_str());

  std::string inner_x86_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_x86_tiling_path).c_str());
  inner_x86_tiling_path += kOpMaster;
  system(("touch " + inner_x86_tiling_path).c_str());
  system(("echo 'op tiling_x86 ' > " + inner_x86_tiling_path).c_str());

  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string op_arg = AtcFileFactory::Generatefile1("--op_precision_mode=", "op_precision.ini");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  std::string build_config_arg = "--build_config=make -s CXX=c++";
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--deterministic=1",
                  const_cast<char *>(op_arg.c_str()),
                  "--input_format=NCHW",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64",
                  const_cast<char *>(build_config_arg.c_str()),
                  "--h2d_overlapped_with_compute=1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  auto &options = GetMutableGlobalOptions();
  auto it = options.find(ge::DETERMINISTIC);
  EXPECT_NE(it, options.end());
  if (it != options.end()) {
    EXPECT_EQ(it->second == "1", true);
  }
  it = options.find(ge::OP_PRECISION_MODE);
  EXPECT_NE(it, options.end());
  if (it != options.end()) {
    EXPECT_EQ(it->second == AtcFileFactory::GetFileRealName("op_precision.ini"), true);
  }
  it = options.find("ge.buildConfig");
  EXPECT_NE(it, options.end());
  if (it != options.end()) {
    EXPECT_EQ(it->second, "make -s CXX=c++");
  }
  // H2D overlap option flows from CLI -> SetAtcJitOptions -> GELib::Initialize -> global options
  it = options.find(ge::OPTION_H2D_OVERLAPPED_WITH_COMPUTE);
  EXPECT_NE(it, options.end());
  if (it != options.end()) {
    EXPECT_EQ(it->second, "1");
  }
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, CheckHostEnvOsAndHostEnvCpuStringValid) {
  EXPECT_EQ(CheckHostEnvOsAndHostEnvCpuStringValid("linux", "x86_64"), ge::SUCCESS);
  EXPECT_EQ(CheckHostEnvOsAndHostEnvCpuStringValid("linux", "aarch64"), ge::SUCCESS);
  EXPECT_EQ(CheckHostEnvOsAndHostEnvCpuStringValid("ubuntu", "x86_64"), ge::PARAM_INVALID);
  EXPECT_EQ(CheckHostEnvOsAndHostEnvCpuStringValid("linux#", "x86_64"), ge::PARAM_INVALID);
  EXPECT_EQ(CheckHostEnvOsAndHostEnvCpuStringValid("linux", "arm64"), ge::SUCCESS);
  EXPECT_EQ(CheckHostEnvOsAndHostEnvCpuStringValid("linux", "x86_64#"), ge::PARAM_INVALID);
}

TEST_F(UtestMain, MainImplTest_static_shape_invalid_host_env_cpu_failed_in_flag_check) {
  GetMutableGlobalOptions().clear();
  GetThreadLocalContext().SetGlobalOption({});

  const std::string original_host_env_os = FLAGS_host_env_os;
  const std::string original_host_env_cpu = FLAGS_host_env_cpu;

  std::string opp_path = "./opp/";
  system(("mkdir -p " + opp_path).c_str());
  setenv(kEnvName, opp_path.c_str(), 1);

  std::string scene_path = opp_path + "scene.info";
  system(("touch " + scene_path).c_str());
  system(("echo 'os=linux' > " + scene_path).c_str());
  system(("echo 'arch=x86_64' >> " + scene_path).c_str());

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_x86_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_x86_proto_path).c_str());
  inner_x86_proto_path += kOpsProto;
  system(("touch " + inner_x86_proto_path).c_str());
  system(("echo 'ops proto x86 ' > " + inner_x86_proto_path).c_str());

  std::string inner_x86_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_x86_tiling_path).c_str());
  inner_x86_tiling_path += kOpMaster;
  system(("touch " + inner_x86_tiling_path).c_str());
  system(("echo 'op tiling_x86 ' > " + inner_x86_tiling_path).c_str());

  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp_invalid_host_env_cpu");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--deterministic=1",
                  "--input_format=NCHW",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64#"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  FLAGS_host_env_os = original_host_env_os;
  FLAGS_host_env_cpu = original_host_env_cpu;
  EXPECT_NE(ret, 0);
  EXPECT_TRUE(GetMutableGlobalOptions().empty());
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp_invalid_host_env_cpu.om").c_str());
}

TEST_F(UtestMain, MainImplTest_generalized_build_mode_error) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW",
                  "--shape_generalized_build_mode=shape_precise1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_status_check_error) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW",
                  "--shape_generalized_build_mode=shape_precise",
                  "--status_check=3"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, -1);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_status_check_success) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW",
                  "--shape_generalized_build_mode=shape_generalized",
                  "--status_check=1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_allow_hf32_invalid) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW",
                  "--shape_generalized_build_mode=shape_generalized",
                  "--allow_hf32=1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_generate_om_model_caffe) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string invalid_weight_arg = AtcFileFactory::Generatefile1("--weight=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=0",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  const_cast<char *>(invalid_weight_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_generate_om_model_onnx) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=5",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_generate_om_model_autofuse) {
  setenv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  unsetenv("AUTOFUSE_FLAGS");
}

TEST_F(UtestMain, MainImplTest_convert_model_to_json_pb_dump_mode) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc",
                  "--mode=1",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(json_arg.c_str()),
                  "--dump_mode=1"

  };
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_1) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc",
                  "--mode=6",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(json_arg.c_str()),
                  "--dump_mode=1"

  };
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_convert_model_to_json_pb) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {
      "atc", "--mode=1", "--framework=3", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str())

  };
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_convert_model_to_json_txt_failed) {
  std::string om_arg = AtcFileFactory::Generatefile1(
      "--om=", "ge_proto_00000261_partition0_rank31_new_sub_graph102_SecondPartitioning.txt");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str()), "--mode=5"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);  // txt file not exist
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_convert_model_to_json_prototxt_failed) {
  std::string om_arg = AtcFileFactory::Generatefile1(
      "--om=", "ge_proto_00000261_partition0_rank31_new_sub_graph102_SecondPartitioning.prototxt");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str()), "--mode=5"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);  // txt file is invalid
}

TEST_F(UtestMain, MainImplTest_convert_empty_model_to_json_prototxt_failed) {
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc", const_cast<char *>(json_arg.c_str()), "--mode=5"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);  // txt file is empty
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_convert_model_to_json_txt_successful) {
  std::string om_arg = AtcFileFactory::GenerateModel("--om=", "tmp.txt");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str()), "--mode=5"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.txt").c_str());
}

TEST_F(UtestMain, MainImplTest_convert_model_to_json_with_fusion) {
  std::string om_arg = AtcFileFactory::GenerateModel("--om=", "tmp.txt", true);
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str()), "--mode=5"};
  EXPECT_EQ(main_impl(sizeof(argv) / sizeof(argv[0]), argv), 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.txt").c_str());
}

TEST_F(UtestMain, MainImplTest_generate_om_model_virtual_type) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--virtual_type=2",
                  "--framework=5",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, GeFlags_param_ok01) {
  char *argv[] = {"atc", "--virtual_type=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(UtestMain, GeFlags_param_ok02) {
  char *argv[] = {"atc", "--virtual_type=1"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_virtual_type, 1);
}

TEST_F(UtestMain, GeFlags_param_ok03) {
  char *argv[] = {"atc", "--model=model_value"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_model, "model_value");
}

TEST_F(UtestMain, GeFlags_param_ok04) {
  char *argv[] = {"atc", "--model", "model_value"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_model, "model_value");
}

TEST_F(UtestMain, GeFlags_param_help01) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help02) {
  FLAGS_help = false;
  char *argv[] = {"atc", "-help"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help03) {
  FLAGS_help = false;
  char *argv[] = {"atc", "-h"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help04) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=true"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help05) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=True"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help06) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=T"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help07) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=y"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help08) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=yes"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help09) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=1"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, true);
}

TEST_F(UtestMain, GeFlags_param_help10) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=false"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(UtestMain, GeFlags_param_help11) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=f"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(UtestMain, GeFlags_param_help12) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=n"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(UtestMain, GeFlags_param_help13) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=no"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(UtestMain, GeFlags_param_help14) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(UtestMain, GeFlags_param_help15) {
  FLAGS_help = false;
  char *argv[] = {"atc", "--help=invalid_bool"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_help, false);
}

TEST_F(UtestMain, GeFlags_param_include_dash) {
  char *argv[] = {"atc", "--virtual-type=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(UtestMain, GeFlags_param_single_minus) {
  char *argv[] = {"atc", "-virtual_type=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(UtestMain, GeFlags_flag_value_empty01) {
  char *argv[] = {"atc", "--virtual_type="};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_flag_value_empty02) {
  char *argv[] = {"atc", "--model="};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(UtestMain, GeFlags_no_flag_value) {
  char *argv[] = {"atc", "--virtual_type"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_data_type_error) {
  char *argv[] = {"atc", "--virtual_type=string_value"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_flag_name_error01) {
  char *argv[] = {"atc", "--virtual_type_err=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_flag_name_error02) {
  char *argv[] = {"atc", "--virtual_type_err"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_non_option_parameter01) {
  char *argv[] = {"atc", "virtual_type=0"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_non_option_parameter02) {
  char *argv[] = {"atc", "virtual_type=0", "--mode=0", "--framework=5"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_dump_mode_error) {
  char *argv[] = {"atc", "--dump_mode=3"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_status_check_error) {
  char *argv[] = {"atc", "--status_check=3"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_op_compiler_cache_mode_error) {
  char *argv[] = {"atc", "--op_compiler_cache_mode=\"atc\""};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_framework_error) {
  char *argv[] = {"atc", "--framework=6"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_op_debug_level_error) {
  char *argv[] = {"atc", "--op_debug_level=3.6"};
  int32_t ret = ge::flgs::ParseCommandLine(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_mode_1_framework_error) {
  char *argv[] = {
      "atc",
      "--mode=1",
      "--framework=1",
  };
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_framework_0_version_error) {
  std::string weight = AtcFileFactory::Generatefile1("--weight=", "add.pb");
  char *argv[] = {"atc", "--mode=0", "--framework=0", const_cast<char *>(weight.c_str()), "--soc_version=Ascend910B1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlagsFramework0_Fail_UnsupportedVersion) {
  std::string weight = AtcFileFactory::Generatefile1("--weight=", "add.pb");
  char *argv[] = {"atc", "--mode=0", "--framework=0", const_cast<char *>(weight.c_str()),
                  "--soc_version=Ascend910_9391"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_framework_0_weight_error) {
  char *argv[] = {"atc", "--mode=0", "--framework=0", "--weight=***", "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_framework_0_weight_none) {
  char *argv[] = {"atc", "--mode=0", "--framework=0", "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_framework_3_weight_warn) {
  std::string weight = AtcFileFactory::Generatefile1("--weight=", "add.pb");
  char *argv[] = {"atc", "--mode=0", "--framework=3", const_cast<char *>(weight.c_str()),
                  "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, GeFlags_framework_5_weight_warn) {
  std::string weight = AtcFileFactory::Generatefile1("--weight=", "add.pb");
  char *argv[] = {"atc", "--mode=0", "--framework=5", const_cast<char *>(weight.c_str()),
                  "--soc_version=\"Ascend310\""};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_status_check_success_3) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=3",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW",
                  "--shape_generalized_build_mode=shape_generalized",
                  "--status_check=1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_status_check_fail_mindspore) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=1",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--out_nodes=relu:0",
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--output_type=FP32",
                  "--status_check=0"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_status_check_fail_mindspore_2) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--model=3"
                  "--framework=1",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--out_nodes=relu:0",
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--output_type=FP32",
                  "--status_check=0",
                  "--cluster_config=fake.json"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

namespace {
ComputeGraphPtr MakeGraph() {
  ge::ut::GraphBuilder builder("graph");
  auto data = builder.AddNode("data", "Data", 1, 1);
  auto addn1 = builder.AddNode("addn1", "AddN", 1, 1);
  auto output = builder.AddNode("output", "NetOutput", 1, 1);
  builder.AddDataEdge(data, 0, addn1, 0);
  builder.AddDataEdge(addn1, 0, output, 0);
  return builder.GetGraph();
}
}  // namespace

TEST_F(UtestMain, MainImplTest_single_air_graph) {
  map<std::string, std::string> graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  map<std::string, std::string> sess_options = ge::GetThreadLocalContext().GetAllSessionOptions();
  map<std::string, std::string> global_options = ge::GetThreadLocalContext().GetAllGlobalOptions();
  GE_MAKE_GUARD(recover_graph_cfg, [&graph_options]() { GetThreadLocalContext().SetGraphOption(graph_options); });
  GE_MAKE_GUARD(recover_sess_cfg, [&sess_options]() { GetThreadLocalContext().SetSessionOption(sess_options); });
  GE_MAKE_GUARD(recover_global_cfg, [&global_options]() { GetThreadLocalContext().SetGlobalOption(global_options); });
  GE_MAKE_GUARD(recover_context_cfg, [&]() {
    auto &global_options_mutex = GetGlobalOptionsMutex();
    const std::lock_guard<std::mutex> lock(global_options_mutex);
    GetThreadLocalContext().SetGlobalOption(GetMutableGlobalOptions());
  });
  std::string opp_path = "./opp/";
  system(("mkdir -p " + opp_path).c_str());
  setenv(kEnvName, opp_path.c_str(), 1);

  std::string scene_path = opp_path + "scene.info";
  system(("touch " + scene_path).c_str());
  system(("echo 'os=linux' > " + scene_path).c_str());
  system(("echo 'arch=x86_64' >> " + scene_path).c_str());

  system("pwd");

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_x86_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_x86_proto_path).c_str());
  inner_x86_proto_path += kOpsProto;
  system(("touch " + inner_x86_proto_path).c_str());
  system(("echo 'ops proto x86 ' > " + inner_x86_proto_path).c_str());

  std::string inner_x86_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_x86_tiling_path).c_str());
  inner_x86_tiling_path += kOpMaster;
  system(("touch " + inner_x86_tiling_path).c_str());
  system(("echo 'op tiling_x86 ' > " + inner_x86_tiling_path).c_str());

  std::string model_1 = "model1";
  auto compute_graph = MakeGraph();
  compute_graph->TopologicalSorting();
  auto data = compute_graph->FindNode("data");
  EXPECT_NE(data, nullptr);
  EXPECT_NE(data->GetOpDesc()->MutableInputDesc(0), nullptr);
  data->GetOpDesc()->MutableInputDesc(0)->SetOriginShape(GeShape({1, 2, 3, 4, 5}));
  data->GetOpDesc()->MutableInputDesc(0)->SetOriginFormat(FORMAT_NC1HWC0);
  bool enable_storage_format_spread = true;
  (void)AttrUtils::SetBool(data->GetOpDesc(), "_enable_storage_format_spread", enable_storage_format_spread);
  bool is_origin_format_set = true;
  (void)AttrUtils::SetBool(data->GetOpDesc()->MutableInputDesc(0), ATTR_NAME_ORIGIN_FORMAT_IS_SET,
                           is_origin_format_set);
  Graph graph = ge::GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);
  graph.SaveToFile(model_1.c_str());
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=1",
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend910\"",
                  "--model=model1",
                  "--host_env_os=linux",
                  "--host_env_cpu=x86_64",
                  "--log=error",
                  "--allow_hf32=false"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  remove(model_1.c_str());
}

TEST_F(UtestMain, MainImplTest_MindSpore_Input_Shape) {
  map<std::string, std::string> graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  map<std::string, std::string> sess_options = ge::GetThreadLocalContext().GetAllSessionOptions();
  map<std::string, std::string> global_options = ge::GetThreadLocalContext().GetAllGlobalOptions();
  GE_MAKE_GUARD(recover_graph_cfg, [&graph_options]() { GetThreadLocalContext().SetGraphOption(graph_options); });
  GE_MAKE_GUARD(recover_sess_cfg, [&sess_options]() { GetThreadLocalContext().SetSessionOption(sess_options); });
  GE_MAKE_GUARD(recover_global_cfg, [&global_options]() { GetThreadLocalContext().SetGlobalOption(global_options); });
  GE_MAKE_GUARD(recover_context_cfg, [&]() {
    auto &global_options_mutex = GetGlobalOptionsMutex();
    const std::lock_guard<std::mutex> lock(global_options_mutex);
    GetThreadLocalContext().SetGlobalOption(GetMutableGlobalOptions());
  });

  std::string opp_path = "./opp/";
  system(("mkdir -p " + opp_path).c_str());
  setenv(kEnvName, opp_path.c_str(), 1);

  std::string scene_path = opp_path + "scene.info";
  system(("touch " + scene_path).c_str());
  system(("echo 'os=linux' > " + scene_path).c_str());
  system(("echo 'arch=x86_64' >> " + scene_path).c_str());

  system("pwd");

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_x86_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_x86_proto_path).c_str());
  inner_x86_proto_path += kOpsProto;
  system(("touch " + inner_x86_proto_path).c_str());
  system(("echo 'ops proto x86 ' > " + inner_x86_proto_path).c_str());

  std::string inner_x86_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_x86_tiling_path).c_str());
  inner_x86_tiling_path += kOpMaster;
  system(("touch " + inner_x86_tiling_path).c_str());
  system(("echo 'op tiling_x86 ' > " + inner_x86_tiling_path).c_str());

  std::string model_1 = "model1";
  auto compute_graph = MakeGraph();
  compute_graph->TopologicalSorting();
  Graph graph = ge::GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);
  graph.SaveToFile(model_1.c_str());
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=1",
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend910\"",
                  "--model=model1",
                  "--input_shape=data:-1,1",
                  "--dynamic_batch_size=1,2"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  remove(model_1.c_str());
}

TEST_F(UtestMain, MainImplTest_MindSpore_Invalid_Input_Shape) {
  map<std::string, std::string> graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  map<std::string, std::string> sess_options = ge::GetThreadLocalContext().GetAllSessionOptions();
  map<std::string, std::string> global_options = ge::GetThreadLocalContext().GetAllGlobalOptions();
  GE_MAKE_GUARD(recover_graph_cfg, [&graph_options]() { GetThreadLocalContext().SetGraphOption(graph_options); });
  GE_MAKE_GUARD(recover_sess_cfg, [&sess_options]() { GetThreadLocalContext().SetSessionOption(sess_options); });
  GE_MAKE_GUARD(recover_global_cfg, [&global_options]() { GetThreadLocalContext().SetGlobalOption(global_options); });
  GE_MAKE_GUARD(recover_context_cfg, [&]() {
    auto &global_options_mutex = GetGlobalOptionsMutex();
    const std::lock_guard<std::mutex> lock(global_options_mutex);
    GetThreadLocalContext().SetGlobalOption(GetMutableGlobalOptions());
  });

  std::string opp_path = "./opp/";
  system(("mkdir -p " + opp_path).c_str());
  setenv(kEnvName, opp_path.c_str(), 1);

  std::string scene_path = opp_path + "scene.info";
  system(("touch " + scene_path).c_str());
  system(("echo 'os=linux' > " + scene_path).c_str());
  system(("echo 'arch=x86_64' >> " + scene_path).c_str());

  system("pwd");

  std::string path_vendors = opp_path + "vendors";
  std::string path_config = path_vendors + "/config.ini";
  system(("mkdir -p " + path_vendors).c_str());
  system(("echo 'load_priority=customize' > " + path_config).c_str());

  std::string inner_x86_proto_path = opp_path + kInner + kOpsProtoPath;
  system(("mkdir -p " + inner_x86_proto_path).c_str());
  inner_x86_proto_path += kOpsProto;
  system(("touch " + inner_x86_proto_path).c_str());
  system(("echo 'ops proto x86 ' > " + inner_x86_proto_path).c_str());

  std::string inner_x86_tiling_path = opp_path + kInner + kOpMasterPath;
  system(("mkdir -p " + inner_x86_tiling_path).c_str());
  inner_x86_tiling_path += kOpMaster;
  system(("touch " + inner_x86_tiling_path).c_str());
  system(("echo 'op tiling_x86 ' > " + inner_x86_tiling_path).c_str());

  std::string model_1 = "model1";
  auto compute_graph = MakeGraph();
  compute_graph->TopologicalSorting();
  Graph graph = ge::GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);
  graph.SaveToFile(model_1.c_str());
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv_invalid_type[] = {"atc",
                               "--framework=1",
                               const_cast<char *>(output_arg.c_str()),
                               "--soc_version=\"Ascend910\"",
                               "--model=model1",
                               "--input_shape=addn1:-1,1",
                               "--dynamic_batch_size=1,2"};
  int32_t ret = main_impl(sizeof(argv_invalid_type) / sizeof(argv_invalid_type[0]), argv_invalid_type);
  EXPECT_NE(ret, 0);

  char *argv_invalid_name[] = {"atc",
                               "--framework=1",
                               const_cast<char *>(output_arg.c_str()),
                               "--soc_version=\"Ascend910\"",
                               "--model=model1",
                               "--input_shape=data1:-1,1",
                               "--dynamic_batch_size=1,2"};
  ret = main_impl(sizeof(argv_invalid_name) / sizeof(argv_invalid_name[0]), argv_invalid_name);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  remove(model_1.c_str());
}
namespace ge {
Status CallAmctInterface(ge::Graph &graph, std::map<std::string, std::string> &options);
}

char g_handleStub;
bool g_isError = false;
using amctStatus = int32_t;
// 待后续与原始流程一起删除
amctStatus amctGraphCalibration(ge::Graph &graph, const std::map<std::string, std::string> &options) {
  auto compute_graph = ge::GraphUtilsEx::GetComputeGraph(graph);
  if (compute_graph != nullptr) {
    return static_cast<amctStatus>(ge::GRAPH_NOT_CHANGED);
  } else {
    return static_cast<amctStatus>(ge::GRAPH_FAILED);
  }
}

class AmctCalibration : public IAmctCalibration {
 public:
  graphStatus Calibrate(Graph &graph, const std::map<std::string, std::string> &options) override {
    auto compute_graph = ge::GraphUtilsEx::GetComputeGraph(graph);
    if (compute_graph != nullptr) {
      return ge::GRAPH_SUCCESS;
    }
    return g_isError ? ge::GRAPH_FAILED : ge::GRAPH_SUCCESS;
  }
};

class MockMmpa : public ge::MmpaStubApiGe {
 public:
  void *DlOpen(const char *file_name, int32_t mode) override {
    if (string("libamctacl.so") == file_name) {
      return (void *)&g_handleStub;
    }
    return MmpaStubApiGe::DlOpen(file_name, mode);
  }

  void *DlSym(void *handle, const char *func_name) override {
    if (g_isError) {
      return nullptr;
    }
    if (std::string(func_name) == "amctGraphCalibration") {
      return (void *)&amctGraphCalibration;
    }
    return dlsym(handle, func_name);
  }
  int32_t DlClose(void *handle) override {
    return 0;
  }
};

TEST_F(UtestMain, MainImplTest_amct_interface) {
  MmpaStub::GetInstance().SetImpl(std::make_shared<MockMmpa>());
  Graph graph("test");
  std::map<std::string, std::string> options;
  auto ret = CallAmctInterface(graph, options);
  EXPECT_EQ(ret, 0);

  options[COMPRESSION_OPTIMIZE_CONF] = "path_test";
  REG_AMCT_CALIBRATION(AmctCalibration);  // 执行一次CallAmctInterface后就会unregister
  g_isError = true;
  ret = CallAmctInterface(graph, options);
  EXPECT_NE(ret, 0);

  g_isError = false;
  ret = CallAmctInterface(graph, options);
  EXPECT_NE(ret, 0);

  REG_AMCT_CALIBRATION(AmctCalibration);
  graph.SetValid();
  ret = CallAmctInterface(graph, options);
  EXPECT_EQ(ret, 0);
  MmpaStub::GetInstance().Reset();
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

TEST_F(UtestMain, MainImplTest_amct_interface_dlopen_fail) {
  MmpaStub::GetInstance().SetImpl(std::make_shared<MockMmpaDlOpenFail>());
  Graph graph("test");
  std::map<std::string, std::string> options;
  options[COMPRESSION_OPTIMIZE_CONF] = "path_test";

  (void)dlerror();
  auto ret = CallAmctInterface(graph, options);
  EXPECT_NE(ret, 0);

  (void)dlopen("nonexistent_lib_for_error.so", RTLD_NOW);
  ret = CallAmctInterface(graph, options);
  EXPECT_NE(ret, 0);

  MmpaStub::GetInstance().Reset();
}

TEST_F(UtestMain, GeFlags_oo_help) {
  char *argv[] = {"atc", "--help"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
}

TEST_F(UtestMain, GeFlags_oo_init) {
  const auto opp_path = ConstructOppEnv();
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend910B",
                  "--oo_level=O1",
                  "--oo_constant_folding=true"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_oo_level, "O1");
  std::string opt_value;
  EXPECT_EQ(GetThreadLocalContext().GetOo().GetValue(OO_CONSTANT_FOLDING, opt_value), ge::GRAPH_SUCCESS);
  EXPECT_EQ(opt_value, "true");
  system(("rm -rf " + opp_path).c_str());

  GetThreadLocalContext().SetGlobalOption({});
  GetThreadLocalContext().SetSessionOption({});
  GetThreadLocalContext().SetGraphOption({});
  GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
}

TEST_F(UtestMain, GeFlags_oo_init_param_valid) {
  const auto opp_path = ConstructOppEnv();
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  std::string opt_value;

  char *argv1[] = {"atc",
                   "--framework=3",
                   const_cast<char *>(om_arg.c_str()),
                   const_cast<char *>(output_arg.c_str()),
                   "--soc_version=Ascend910B",
                   "--oo_level=O1",
                   "--oo_constant_folding=T"};
  auto ret = main_impl(sizeof(argv1) / sizeof(argv1[0]), argv1);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_oo_level, "O1");
  EXPECT_EQ(GetThreadLocalContext().GetOo().GetValue(OO_CONSTANT_FOLDING, opt_value), ge::GRAPH_SUCCESS);
  EXPECT_EQ(opt_value, "true");

  char *argv2[] = {"atc",
                   "--framework=3",
                   const_cast<char *>(om_arg.c_str()),
                   const_cast<char *>(output_arg.c_str()),
                   "--soc_version=Ascend910B",
                   "--oo_level=O1",
                   "--oo_constant_folding=1"};
  ret = main_impl(sizeof(argv2) / sizeof(argv2[0]), argv2);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_oo_level, "O1");
  EXPECT_EQ(GetThreadLocalContext().GetOo().GetValue(OO_CONSTANT_FOLDING, opt_value), ge::GRAPH_SUCCESS);
  EXPECT_EQ(opt_value, "true");

  char *argv3[] = {"atc",
                   "--framework=3",
                   const_cast<char *>(om_arg.c_str()),
                   const_cast<char *>(output_arg.c_str()),
                   "--soc_version=Ascend910B",
                   "--oo_level=O1",
                   "--oo_constant_folding=0"};
  ret = main_impl(sizeof(argv3) / sizeof(argv3[0]), argv3);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_oo_level, "O1");
  EXPECT_EQ(GetThreadLocalContext().GetOo().GetValue(OO_CONSTANT_FOLDING, opt_value), ge::GRAPH_SUCCESS);
  EXPECT_EQ(opt_value, "true");

  char *argv4[] = {"atc",
                   "--framework=3",
                   const_cast<char *>(om_arg.c_str()),
                   const_cast<char *>(output_arg.c_str()),
                   "--soc_version=Ascend910B",
                   "--oo_level=O1",
                   "--oo_constant_folding=t1"};
  ret = main_impl(sizeof(argv4) / sizeof(argv4[0]), argv4);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_oo_level, "O1");

  GetThreadLocalContext().SetGlobalOption({});
  GetThreadLocalContext().SetSessionOption({});
  GetThreadLocalContext().SetGraphOption({});
  GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
}

TEST_F(UtestMain, GeFlags_oo_init_param_invalid) {
  GetThreadLocalContext().GetOo().Initialize({}, {});
  const auto opp_path = ConstructOppEnv();
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend910B",
                  "--oo_level=O4",
                  "--oo_constant_folding=true"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_oo_level, "O4");
  std::string opt_value;
  EXPECT_NE(GetThreadLocalContext().GetOo().GetValue(OO_CONSTANT_FOLDING, opt_value), ge::GRAPH_SUCCESS);

  char *argv4[] = {"atc",
                   "--framework=3",
                   const_cast<char *>(om_arg.c_str()),
                   const_cast<char *>(output_arg.c_str()),
                   "--soc_version=Ascend910B",
                   "--oo_level=O1",
                   "--oo_constant_folding=f1"};
  ret = main_impl(sizeof(argv4) / sizeof(argv4[0]), argv4);
  EXPECT_NE(ret, 0);
  EXPECT_EQ(FLAGS_oo_level, "O1");
  EXPECT_EQ(GetThreadLocalContext().GetOo().GetValue(OO_CONSTANT_FOLDING, opt_value), ge::GRAPH_SUCCESS);
  EXPECT_EQ(opt_value, "true");
  GetThreadLocalContext().SetGlobalOption({});
  GetThreadLocalContext().SetSessionOption({});
  GetThreadLocalContext().SetGraphOption({});
  GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
}

TEST_F(UtestMain, GeFlags_export_compile_stat_invalid) {
  GetThreadLocalContext().GetOo().Initialize({}, {});
  const auto opp_path = ConstructOppEnv();
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend910B",
                  "--export_compile_stat=3"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  std::string opt_value;
  EXPECT_EQ(GetThreadLocalContext().GetOption(OPTION_EXPORT_COMPILE_STAT, opt_value), ge::GRAPH_SUCCESS);

  GetThreadLocalContext().SetGlobalOption({});
  GetThreadLocalContext().SetSessionOption({});
  GetThreadLocalContext().SetGraphOption({});
  GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
}

TEST_F(UtestMain, GeFlags_export_compile_stat_valid) {
  GetThreadLocalContext().GetOo().Initialize({}, {});
  flgs::GetUserOptions().clear();
  const auto opp_path = ConstructOppEnv();
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend910B",
                  "--export_compile_stat=1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  std::string opt_value;
  EXPECT_EQ(GetThreadLocalContext().GetOption(OPTION_EXPORT_COMPILE_STAT, opt_value), ge::GRAPH_SUCCESS);
  EXPECT_EQ(opt_value, "1");

  GetThreadLocalContext().SetGlobalOption({});
  GetThreadLocalContext().SetSessionOption({});
  GetThreadLocalContext().SetGraphOption({});
  GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
}

// 校验输入hint shape的index校验失败
TEST_F(UtestMain, MainImplTest_generate_om_model_autofuse_dynamic_shape_index_invalid) {
  setenv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--input_hint_shape=-1:[2]",
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  unsetenv("AUTOFUSE_FLAGS");
}

// input_hint_shape校验失败，没有用[]
TEST_F(UtestMain, MainImplTest_generate_om_model_autofuse_dynamic_shape_shape_invalid) {
  setenv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--input_hint_shape=1:2",
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  unsetenv("AUTOFUSE_FLAGS");
}

// test input hint shape symbolic failed scenario
TEST_F(UtestMain, MainImplTest_generate_om_model_autofuse_dynamic_shape_symbolic_failed) {
  setenv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--input_shape=Placeholder:-1;Placeholder_1:-1",
                  "--input_hint_shape=0:[2];1:[3]",
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  unsetenv("AUTOFUSE_FLAGS");
}

// test input hint shape and input dynamic param failed
TEST_F(UtestMain, MainImplTest_generate_om_model_autofuse_hint_shape_with_dyna_param_failed) {
  setenv("AUTOFUSE_FLAGS", "--enable_autofuse=true", 1);
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--input_shape=Placeholder:-1;Placeholder_1:-1",
                  "--input_hint_shape=0:[3];1:[3]",
                  "--dynamic_dims=2;4;8;16",
                  "--soc_version=\"Ascend310\"",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  unsetenv("AUTOFUSE_FLAGS");
}

TEST_F(UtestMain, MainImplTest_Om2Mode_DynamicAipp_Rejected) {
  // Write a temporary dynamic AIPP config
  const std::string cfg_path = "/tmp/ut_main_om2_dynamic_aipp.cfg";
  {
    std::ofstream ofs(cfg_path);
    ofs << "aipp_op {\n"
        << "  aipp_mode: dynamic\n"
        << "  related_input_rank: 0\n"
        << "  max_src_image_size: 752640\n"
        << "}\n";
  }
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp_om2_dyn");
  std::string insert_conf_arg = "--insert_op_conf=" + cfg_path;
  char *argv[] = {"atc",
                  "--mode=7",  // GEN_OM2_MODEL
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  const_cast<char *>(insert_conf_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--host_env_os=linux",
                  "--host_env_cpu=aarch64",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);  // dynamic AIPP must be rejected in OM2 mode
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp_om2_dyn.om").c_str());
  AtcFileFactory::RemoveFile(cfg_path.c_str());
}

TEST_F(UtestMain, MainImplTest_Om2Mode_StaticAipp_NotBlocked) {
  // Write a temporary static AIPP config
  const std::string cfg_path = "/tmp/ut_main_om2_static_aipp.cfg";
  {
    std::ofstream ofs(cfg_path);
    ofs << "aipp_op {\n"
        << "  aipp_mode: static\n"
        << "  input_format: RGB888_U8\n"
        << "  related_input_rank: 0\n"
        << "  csc_switch: false\n"
        << "}\n";
  }
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp_om2_sta");
  std::string insert_conf_arg = "--insert_op_conf=" + cfg_path;
  char *argv[] = {"atc",
                  "--mode=7",  // GEN_OM2_MODEL
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  const_cast<char *>(insert_conf_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--host_env_os=linux",
                  "--host_env_cpu=aarch64",
                  "--input_format=NCHW"};
  // ValidateStaticAippOnly should pass (static config). The build may fail later
  // (no real model / OPP env), but the if-block in main_impl.cc is entered and
  // the success path of ValidateStaticAippOnly is exercised.
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);  // still fails (no real compilation env), but not at ValidateStaticAippOnly
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp_om2_sta.om").c_str());
  AtcFileFactory::RemoveFile(cfg_path.c_str());
}

namespace ge {
class GFlagUtils {
 public:
  static bool CheckPathWithName(const std::string &fileName);
  static Status CheckFlags();
};
std::set<std::string> &GetRawAppliedFlagNames();
std::map<std::string, std::string> &GetRawAppliedFlagOptions();
}  // namespace ge

TEST_F(UtestMain, MainImplTest_report_invalid_run_mode) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  char *argv[] = {"atc", "--mode=99", "--framework=3", const_cast<char *>(om_arg.c_str()), "--soc_version=Ascend910B"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_convert_json_om_empty) {
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc", "--mode=1", const_cast<char *>(json_arg.c_str())};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_convert_json_json_empty) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  char *argv[] = {"atc", "--mode=1", const_cast<char *>(om_arg.c_str())};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_convert_json_invalid_json_path) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  char *argv[] = {"atc", "--mode=1", const_cast<char *>(om_arg.c_str()), "--json=/nonexistent_dir/tmp.json"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_display_model_info_om_empty) {
  char *argv[] = {"atc", "--mode=6"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_display_model_info_om_invalid) {
  char *argv[] = {"atc", "--mode=6", "--om=/nonexistent/file.om"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_display_model_info_convert_om) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  char *argv[] = {"atc", "--mode=6", const_cast<char *>(om_arg.c_str())};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_convert_om_to_json) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc", "--mode=1", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str())};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_op_precision_mode_not_found) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--op_precision_mode=/nonexistent/precision.ini"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_cal_conf_not_found) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--cal_conf=/nonexistent/cal.conf"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_op_name_map_not_found) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--op_name_map=/nonexistent/op_name_map.txt"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_save_original_model_invalid) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--save_original_model=invalid"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_output_path_invalid) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  "--output=/nonexistent_dir/tmp",
                  "--soc_version=Ascend310",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_input_format_invalid_tf) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=ABCD"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_input_format_invalid_caffe) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string weight_arg = AtcFileFactory::Generatefile1("--weight=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=0",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  const_cast<char *>(weight_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=ABCD"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_input_format_invalid_onnx) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=5",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=ABCD"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_pbtxt_to_json_convert_fail) {
  std::string txt_path = AtcFileFactory::GetFileRealName("invalid_pbtxt.txt");
  {
    std::ofstream ofs(txt_path);
    ofs << "invalid content for pbtxt conversion test";
  }
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "invalid_pbtxt.txt");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc", "--mode=5", const_cast<char *>(om_arg.c_str()), const_cast<char *>(json_arg.c_str())};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(txt_path.c_str());
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_display_model_info_nano) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=30",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend035A",
                  "--input_format=NCHW",
                  "--display_model_info=1"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.exeom").c_str());
}

TEST_F(UtestMain, MainImplTest_static_model_ops_lower_limit) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--static_model_ops_lower_limit=10"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_atc_options_with_extra_flags) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--save_original_model=true",
                  "--input_fp16_nodes=somenode",
                  "--optimization_switch=O0",
                  "--static_model_ops_lower_limit=10"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp_original.om").c_str());
}

TEST_F(UtestMain, MainImplTest_singleop_with_optimization_switch) {
  std::string singleop_arg = AtcFileFactory::Generatefile1("--singleop=", "add_int.json");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "./");
  char *argv[] = {"atc", const_cast<char *>(singleop_arg.c_str()), const_cast<char *>(output_arg.c_str()),
                  "--soc_version=\"Ascend310\"", "--optimization_switch=O0"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_generate_infershape_json_input_format_fail) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc",
                  "--mode=1",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(json_arg.c_str()),
                  "--dump_mode=1",
                  "--input_format=ABCD"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_generate_infershape_json_run) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc",
                  "--mode=1",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(json_arg.c_str()),
                  "--dump_mode=1",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_check_report_with_ascend_work_path) {
  setenv("ASCEND_WORK_PATH", "/tmp", 1);
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  unsetenv("ASCEND_WORK_PATH");
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_om2_unsupported_flag) {
  FLAGS_mode = 7;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_om2_flag";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_save_original_model = "true";
  GetRawAppliedFlagOptions()["save_original_model"] = "true";
  GetRawAppliedFlagNames().insert("save_original_model");
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
  GetRawAppliedFlagOptions().clear();
  GetRawAppliedFlagNames().clear();
}

TEST_F(UtestMain, MainImplTest_check_flags_weight_not_found) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_weight";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_weight = "/nonexistent/weight_file";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_transfer_shape_range_failed) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_shape";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_input_shape = "data:1,2,3,4";
  FLAGS_input_shape_range = "data:[invalid_range_format]";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_dynamic_input_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_dyn";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_dynamic_batch_size = "invalid_batch";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_dynamic_dims_with_aipp) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_aipp";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_input_shape = "data:-1,32,32,3";
  FLAGS_dynamic_dims = "1;2;4";
  FLAGS_insert_op_conf = "somefile.cfg";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_modify_mixlist_failed) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_mix";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_modify_mixlist = "/nonexistent/mixlist.txt";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_output_path_too_long) {
  std::string long_path(static_cast<size_t>(PATH_MAX) + 1, 'a');
  EXPECT_FALSE(GFlagUtils::CheckPathWithName(long_path));
}

TEST_F(UtestMain, MainImplTest_output_no_filename) {
  EXPECT_FALSE(GFlagUtils::CheckPathWithName("tmp/"));
  EXPECT_FALSE(GFlagUtils::CheckPathWithName("tmp\\"));
}

TEST_F(UtestMain, MainImplTest_check_flags_compress_weight_conf_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_compress";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_enable_compress_weight = "true";
  FLAGS_compress_weight_conf = "/nonexistent/compress.conf";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_keep_dtype_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_dtype";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_keep_dtype = "/nonexistent/keep_dtype.txt";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_insert_op_conf_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_insert";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_insert_op_conf = "/nonexistent/insert_op.cfg";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_pyatc_module_command) {
  char *argv[] = {"python3 -m ge.pyatc", "--mode=99"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_pyatc_binary_command) {
  char *argv[] = {"pyatc", "--mode=99"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_raw_ge_options_file_not_found) {
  char *argv[] = {"atc", "--raw_ge_options=/nonexistent/options.json"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_atc_options_with_dynamic_dims) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--input_shape=data:-1,32,32,3",
                  "--dynamic_dims=1;2;4"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_atc_options_with_dynamic_image_size) {
  std::string om_arg = AtcFileFactory::Generatefile1("--model=", "add.pb");
  std::string output_arg = AtcFileFactory::Generatefile1("--output=", "tmp");
  char *argv[] = {"atc",
                  "--mode=0",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(output_arg.c_str()),
                  "--soc_version=Ascend310",
                  "--input_format=NCHW",
                  "--input_shape=data:1,3,-1,-1",
                  "--dynamic_image_size=32,32;64,64"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_NE(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.om").c_str());
}

TEST_F(UtestMain, MainImplTest_check_flags_buffer_optimize_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_buffer";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_buffer_optimize = "invalid_value";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_enable_single_stream_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_stream";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_enable_single_stream = "invalid";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_external_weight_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_extw";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_external_weight = "invalid";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_ac_parallel_enable_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_acpar";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_ac_parallel_enable = "invalid";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_tiling_schedule_optimize_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_tso";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_tiling_schedule_optimize = "invalid";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_attr_compression_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_attrc";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_enable_attr_compression = "invalid";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_disable_reuse_memory_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_drm";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_disable_reuse_memory = 999;
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_check_flags_implmode_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_impl";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_op_select_implmode = "invalid";
  FLAGS_optypelist_for_implmode = "some_op";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

TEST_F(UtestMain, MainImplTest_convert_fwk_model_to_json) {
  std::string om_arg = AtcFileFactory::Generatefile1("--om=", "add.pb");
  std::string json_arg = AtcFileFactory::Generatefile1("--json=", "tmp.json");
  char *argv[] = {"atc",
                  "--mode=1",
                  "--framework=3",
                  const_cast<char *>(om_arg.c_str()),
                  const_cast<char *>(json_arg.c_str()),
                  "--dump_mode=0"};
  int32_t ret = main_impl(sizeof(argv) / sizeof(argv[0]), argv);
  EXPECT_EQ(ret, 0);
  AtcFileFactory::RemoveFile(AtcFileFactory::Generatefile1("", "tmp.json").c_str());
}

TEST_F(UtestMain, MainImplTest_check_flags_quant_dumpable_invalid) {
  FLAGS_mode = 0;
  FLAGS_framework = 3;
  FLAGS_model = AtcFileFactory::GetFileRealName("add.pb");
  FLAGS_output = "tmp_qd";
  FLAGS_soc_version = "Ascend310";
  FLAGS_input_format = "NCHW";
  FLAGS_quant_dumpable = "invalid";
  Status ret = GFlagUtils::CheckFlags();
  EXPECT_NE(ret, 0);
}

namespace ge {
bool IsLegacySoFile(const std::string &file_path);
}

class UtestLegacySoPartition : public testing::Test {};

TEST_F(UtestLegacySoPartition, MixedFiles_LegacyMovedToEnd) {
  std::vector<std::string> fileList = {"/path/to/libop1.so", "/path/to/libop2_legacy.so", "/path/to/libop3.so",
                                       "/path/to/libop4_legacy.so", "/path/to/libop5.so"};
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);
  ASSERT_EQ(fileList.size(), 5u);
  EXPECT_EQ(fileList[0], "/path/to/libop1.so");
  EXPECT_EQ(fileList[1], "/path/to/libop3.so");
  EXPECT_EQ(fileList[2], "/path/to/libop5.so");
  EXPECT_EQ(fileList[3], "/path/to/libop2_legacy.so");
  EXPECT_EQ(fileList[4], "/path/to/libop4_legacy.so");
}

TEST_F(UtestLegacySoPartition, OnlyNormalFiles_NoReorder) {
  std::vector<std::string> fileList = {"/path/a.so", "/path/b.so", "/path/c.so"};
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);
  EXPECT_EQ(fileList[0], "/path/a.so");
  EXPECT_EQ(fileList[1], "/path/b.so");
  EXPECT_EQ(fileList[2], "/path/c.so");
}

TEST_F(UtestLegacySoPartition, OnlyLegacyFiles_AllAtEnd) {
  std::vector<std::string> fileList = {"/path/x_legacy.so", "/path/y_legacy.so"};
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);
  EXPECT_EQ(fileList[0], "/path/x_legacy.so");
  EXPECT_EQ(fileList[1], "/path/y_legacy.so");
}

TEST_F(UtestLegacySoPartition, EmptyList_NoCrash) {
  std::vector<std::string> fileList;
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);
  EXPECT_TRUE(fileList.empty());
}

TEST_F(UtestLegacySoPartition, ShortFileName_TreatedAsNonLegacy) {
  std::vector<std::string> fileList = {"a.so", "/path/x_legacy.so", "b.so"};
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);
  EXPECT_EQ(fileList[0], "a.so");
  EXPECT_EQ(fileList[1], "b.so");
  EXPECT_EQ(fileList[2], "/path/x_legacy.so");
}

TEST_F(UtestLegacySoPartition, StabilityPreserved_RelativeOrderMaintained) {
  std::vector<std::string> fileList = {"/path/first.so", "/path/alpha_legacy.so", "/path/second.so",
                                       "/path/beta_legacy.so", "/path/third.so"};
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);
  EXPECT_EQ(fileList[0], "/path/first.so");
  EXPECT_EQ(fileList[1], "/path/second.so");
  EXPECT_EQ(fileList[2], "/path/third.so");
  EXPECT_EQ(fileList[3], "/path/alpha_legacy.so");
  EXPECT_EQ(fileList[4], "/path/beta_legacy.so");
}

TEST_F(UtestLegacySoPartition, ExactLegacySoName_MovedToEnd) {
  std::vector<std::string> fileList = {"_legacy.so", "/path/normal.so"};
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);
  EXPECT_EQ(fileList[0], "/path/normal.so");
  EXPECT_EQ(fileList[1], "_legacy.so");
}

TEST_F(UtestLegacySoPartition, SimilarButNotLegacySuffix_NotMoved) {
  std::vector<std::string> fileList = {"/path/legacy.so", "/path/not_legacy.so.bak", "/path/real_legacy.so",
                                       "/path/_legacy.sox"};
  std::stable_partition(fileList.begin(), fileList.end(), ge::IsLegacySoFile);
  EXPECT_EQ(fileList[0], "/path/legacy.so");
  EXPECT_EQ(fileList[1], "/path/not_legacy.so.bak");
  EXPECT_EQ(fileList[2], "/path/_legacy.sox");
  EXPECT_EQ(fileList[3], "/path/real_legacy.so");
}
