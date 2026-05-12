/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/common/helper/model_save_helper.h"
#include "common/helper/om2/zip_archive_writer.h"
#include "runtime/om2/zip_archive_reader.h"
#include "common/om2/codegen/om2_codegen.h"
#include "common/om2/codegen/om2_codegen_types.h"
#define private public
#include "framework/common/helper/om2_package_helper.h"
#undef private
#include "framework/common/types.h"
#include "common/helper/om2/json_file.h"
#include "common/model/ge_model.h"
#include "file_utils.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "common/env_path.h"
#include "mmpa/mmpa_api.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/tensor_utils.h"
#include "graph/utils/file_utils.h"
#include <cstdio>
#include <sstream>
#include <system_error>

#include "ge_runtime_stub/include/common/share_graph.h"
#include "ge_runtime_stub/include/faker/ge_model_builder.h"
#include "ge_runtime_stub/include/faker/aicore_taskdef_faker.h"

namespace ge {
namespace {
constexpr const char *kTmpDir = "/tmp";
constexpr const char *kOm2DumpDir = "/tmp/.tmp_om2_workspace";

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
      if (op_desc->GetType() == "Add") {
        op_desc->SetIsInputConst({true, true});
        auto input_desc0 = op_desc->MutableInputDesc(0);
        auto input_desc1 = op_desc->MutableInputDesc(1);
        if ((input_desc0 == nullptr) || (input_desc1 == nullptr)) {
          return nullptr;
        }
        TensorUtils::SetDataOffset(*input_desc0, 0);
        TensorUtils::SetDataOffset(*input_desc1, 200704);
      }
    }
  }

  const auto ge_model = ge_root_model->GetSubgraphInstanceNameToModel().begin()->second;
  std::vector<uint8_t> weights_value(401408, 1U);
  const size_t weight_size = weights_value.size();
  ge_model->SetWeight(Buffer::CopyFrom(weights_value.data(), weight_size));

  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 2048);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_WEIGHT_SIZE, weight_size);
  (void)AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 1);

  return ge_root_model;
}

GeRootModelPtr CreateGeRootModelWithFileConstOp() {
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

GeRootModelPtr CreateInvalidGeRootModel() {
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
  return ge_root_model;
}

bool IsTmpDirExists() {
  std::error_code ec;
  return std::filesystem::is_directory(kTmpDir, ec) && !ec;
}

std::string GetOm2DumpPath(const std::string &file_name) {
  return std::string(kOm2DumpDir) + "/" + file_name;
}

bool ReadOm2DumpFile(const std::string &file_name, std::string &content) {
  std::ifstream input(GetOm2DumpPath(file_name), std::ios::in | std::ios::binary);
  if (!input.is_open()) {
    return false;
  }
  std::ostringstream oss;
  oss << input.rdbuf();
  content = oss.str();
  return true;
}

void RemoveOm2DumpFile(const std::string &file_name) {
  (void)std::remove(GetOm2DumpPath(file_name).c_str());
}

}  // namespace

class Om2PackageHelperUt : public testing::Test {
 public:
  void SetUp() override {
    const ::testing::TestInfo *test_info = ::testing::UnitTest::GetInstance()->current_test_info();
    test_case_name = test_info->test_case_name();  // Om2PackageHelperUt
    test_work_dir = EnvPath().GetOrCreateCaseTmpPath(test_case_name);
    const auto ascend_install_path = EnvPath().GetAscendInstallPath();
    setenv("ASCEND_HOME_PATH", ascend_install_path.c_str(), 1);
    setenv("ASCEND_WORK_PATH", test_work_dir.c_str(), 1);
    om2_workspace_base_dir_ = std::filesystem::path(test_work_dir) / ".ascend_temp" / ".tmp_om2_workspace";
    std::filesystem::create_directories(om2_workspace_base_dir_);
    asan_guard_ = std::make_unique<ScopedEnvVar>("ASAN_OPTIONS", "detect_leaks=0:halt_on_error=0");
    lsan_guard_ = std::make_unique<ScopedEnvVar>("LSAN_OPTIONS", "exitcode=0");
  }
  void TearDown() override {
    lsan_guard_.reset();
    asan_guard_.reset();
    EnvPath().RemoveRfCaseTmpPath(test_case_name);
    unsetenv("ASCEND_HOME_PATH");
    unsetenv("ASCEND_WORK_PATH");
  }

 public:
  std::string test_case_name;
  std::string test_work_dir;
  const std::string kZipFileBaseName = "fake_test";
  std::filesystem::path om2_workspace_base_dir_;

 private:
  std::unique_ptr<ScopedEnvVar> asan_guard_;
  std::unique_ptr<ScopedEnvVar> lsan_guard_;
};

TEST_F(Om2PackageHelperUt, ConvertOm2Model_Ok_GenOm2WithAicoreNode) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + ".om2"});
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
      "fake_test/data/constants/constant_0",
      "fake_test/data/constants/model_0_constants_config.json",
      "fake_test/data/kernels_npu_arch/add1_faked_kernel.o",
      "fake_test/data/model_0/model_meta.json",
      "fake_test/data/model_0/debug/op_attr.json",
      "fake_test/manifest.json",
  };
  EXPECT_EQ(file_names.size(), expect_files.size());
  for (const auto &file_name : file_names) {
    EXPECT_EQ(expect_files.count(file_name), 1);
  }
  const std::vector<std::string> cpp_entries = {
      "fake_test/data/model_0/runtime/g1_kernel_reg.cpp",
      "fake_test/data/model_0/runtime/g1_resources.cpp",
      "fake_test/data/model_0/runtime/g1_args_manager.cpp",
      "fake_test/data/model_0/runtime/g1_load_and_run.cpp",
  };
  for (const auto &cpp_entry : cpp_entries) {
    size_t cpp_size = 0;
    const auto cpp_buf = archive.ExtractToMem(cpp_entry, cpp_size);
    ASSERT_NE(cpp_buf, nullptr);
    const std::string cpp_content(reinterpret_cast<const char *>(cpp_buf.get()), cpp_size);
    EXPECT_NE(cpp_content.find("#include \"g1_interface.h\""), std::string::npos) << cpp_entry;
    EXPECT_EQ(cpp_content.find("/proc/self/fd/"), std::string::npos) << cpp_entry;
  }
  size_t makefile_size = 0;
  const auto makefile_buf = archive.ExtractToMem("fake_test/data/model_0/runtime/Makefile", makefile_size);
  ASSERT_NE(makefile_buf, nullptr);
  const std::string makefile_content(reinterpret_cast<const char *>(makefile_buf.get()), makefile_size);
  EXPECT_NE(makefile_content.find("TARGET := libg1_om2.so"), std::string::npos);
  EXPECT_NE(makefile_content.find("SRC_FILES := g1_resources.cpp g1_kernel_reg.cpp"), std::string::npos);
  EXPECT_EQ(makefile_content.find("/proc/self/fd/"), std::string::npos);
  EXPECT_EQ(makefile_content.find("CXXFLAGS += -x c++"), std::string::npos);

  size_t manifest_size = 0;
  const auto manifest_buf = archive.ExtractToMem("fake_test/manifest.json", manifest_size);
  ASSERT_NE(manifest_buf, nullptr);
  const JsonFile manifest_json(reinterpret_cast<const uint8_t *>(manifest_buf.get()), manifest_size);
  ASSERT_TRUE(manifest_json.IsValid());
  std::string atc_command;
  ASSERT_TRUE(manifest_json.Get("atc_command", atc_command));
  EXPECT_EQ(atc_command, "");
  int model_num = 0;
  ASSERT_TRUE(manifest_json.Get("model_num", model_num));
  EXPECT_EQ(model_num, 1);
  std::string om2_version;
  ASSERT_TRUE(manifest_json.Get("om2_version", om2_version));
  EXPECT_EQ(om2_version, "0");

  size_t model_meta_size = 0;
  const auto model_meta_buf = archive.ExtractToMem("fake_test/data/model_0/model_meta.json", model_meta_size);
  ASSERT_NE(model_meta_buf, nullptr);
  const JsonFile model_meta_json(reinterpret_cast<const uint8_t *>(model_meta_buf.get()), model_meta_size);
  ASSERT_TRUE(model_meta_json.IsValid());
  EXPECT_EQ(model_meta_json.Raw().at("name"), JsonFile::json("g1"));
  EXPECT_EQ(model_meta_json.Raw().at("dynamic_batch_info"), JsonFile::json::array());
  EXPECT_EQ(model_meta_json.Raw().at("dynamic_output_shape"), JsonFile::json::array());
  EXPECT_EQ(model_meta_json.Raw().at("dynamic_type"), JsonFile::json(0));
  EXPECT_EQ(model_meta_json.Raw().at("work_size"), JsonFile::json(2048));
  EXPECT_EQ(model_meta_json.Raw().at("user_designate_shape_order"), JsonFile::json::array());

  const JsonFile::json expected_inputs = JsonFile::json::array({
                                                                   {{"data_type", "DT_FLOAT"},
                                                                       {"format", "ND"},
                                                                       {"index", 0},
                                                                       {"name", "data1"},
                                                                       {"shape", JsonFile::json::array({1, 2, 3, 4})},
                                                                       {"shape_range", JsonFile::json::array()},
                                                                       {"shape_v2", JsonFile::json::array({1, 2, 3, 4})},
                                                                       {"size", 0}},
                                                                   {{"data_type", "DT_FLOAT"},
                                                                       {"format", "NCHW"},
                                                                       {"index", 1},
                                                                       {"name", "data2"},
                                                                       {"shape", JsonFile::json::array({1, 1, 224, 224})},
                                                                       {"shape_range", JsonFile::json::array()},
                                                                       {"shape_v2", JsonFile::json::array({1, 1, 224, 224})},
                                                                       {"size", 0}},
                                                               });
  EXPECT_EQ(model_meta_json.Raw().at("inputs"), expected_inputs);

  const JsonFile::json expected_outputs = JsonFile::json::array({
                                                                    {{"data_type", "DT_FLOAT"},
                                                                        {"format", "ND"},
                                                                        {"index", 0},
                                                                        {"name", "output_0_reshape1_0"},
                                                                        {"shape", JsonFile::json::array()},
                                                                        {"shape_range", JsonFile::json::array()},
                                                                        {"size", 4}},
                                                                });
  EXPECT_EQ(model_meta_json.Raw().at("outputs"), expected_outputs);

  size_t constants_config_size = 0;
  const auto constants_config_buf =
      archive.ExtractToMem("fake_test/data/constants/model_0_constants_config.json", constants_config_size);
  ASSERT_NE(constants_config_buf, nullptr);
  const JsonFile constants_json(reinterpret_cast<const uint8_t *>(constants_config_buf.get()), constants_config_size);
  ASSERT_TRUE(constants_json.IsValid());
  EXPECT_EQ(constants_json.Raw().at("internal_weight_size"), JsonFile::json(401408));
  EXPECT_FALSE(constants_json.Raw().contains("weight_size"));
  ASSERT_TRUE(constants_json.Raw().contains("consts"));
  const auto &consts = constants_json.Raw().at("consts");
  ASSERT_TRUE(consts.is_object());
  ASSERT_EQ(consts.size(), 2U);
  ASSERT_TRUE(consts.contains("constant_0"));
  ASSERT_TRUE(consts.contains("constant_1"));
  EXPECT_EQ(consts.at("constant_0").at("index"), JsonFile::json(0));
  EXPECT_EQ(consts.at("constant_0").at("type"), JsonFile::json("INTERNAL"));
  EXPECT_FALSE(consts.at("constant_0").contains("external"));
  EXPECT_EQ(consts.at("constant_0").at("file_name"), JsonFile::json(""));
  EXPECT_EQ(consts.at("constant_0").at("op_name"), JsonFile::json(""));
  EXPECT_EQ(consts.at("constant_0").at("offset"), JsonFile::json(0));
  EXPECT_EQ(consts.at("constant_0").at("size"), JsonFile::json(200704));
  EXPECT_EQ(consts.at("constant_1").at("index"), JsonFile::json(1));
  EXPECT_EQ(consts.at("constant_1").at("type"), JsonFile::json("INTERNAL"));
  EXPECT_FALSE(consts.at("constant_1").contains("external"));
  EXPECT_EQ(consts.at("constant_1").at("file_name"), JsonFile::json(""));
  EXPECT_EQ(consts.at("constant_1").at("op_name"), JsonFile::json(""));
  EXPECT_EQ(consts.at("constant_1").at("offset"), JsonFile::json(200704));
  EXPECT_EQ(consts.at("constant_1").at("size"), JsonFile::json(200704));
}

TEST_F(Om2PackageHelperUt, SaveConstants_Ok_SkipEmptyFileName) {
  const std::string output_file = PathUtils::Join({test_work_dir, "fake_constants_only.om2"});
  auto zip_writer = std::make_shared<ZipArchiveWriter>(output_file);
  ASSERT_TRUE(zip_writer->IsMemFileOpened());

  auto ge_model = std::make_shared<GeModel>();
  std::vector<uint8_t> weights_value(16U, 1U);
  ge_model->SetWeight(Buffer::CopyFrom(weights_value.data(), weights_value.size()));

  std::vector<Om2ConstMeta> const_metas = {
      {0U, "INTERNAL", "", 0, 8, ""},
      {1U, "COMBINED", "weight_combined.bin", 4, 8, "file_const"},
  };
  ASSERT_EQ(Om2PackageHelper::SaveConstants(zip_writer, ge_model, 0UL, const_metas), SUCCESS);
  ASSERT_TRUE(zip_writer->SaveModelDataToFile());

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());

  size_t constants_config_size = 0;
  const auto constants_config_buf =
      archive.ExtractToMem("fake_constants_only/data/constants/model_0_constants_config.json", constants_config_size);
  ASSERT_NE(constants_config_buf, nullptr);
  const JsonFile constants_json(reinterpret_cast<const uint8_t *>(constants_config_buf.get()), constants_config_size);
  ASSERT_TRUE(constants_json.IsValid());
  EXPECT_EQ(constants_json.Raw().at("internal_weight_size"), JsonFile::json(0));
  EXPECT_FALSE(constants_json.Raw().contains("weight_size"));
  ASSERT_TRUE(constants_json.Raw().contains("consts"));
  const auto &consts = constants_json.Raw().at("consts");
  ASSERT_TRUE(consts.contains("constant_0"));
  const auto &internal_const = consts.at("constant_0");
  EXPECT_EQ(internal_const.at("type"), JsonFile::json("INTERNAL"));
  EXPECT_EQ(internal_const.at("file_name"), JsonFile::json(""));
  EXPECT_EQ(internal_const.at("op_name"), JsonFile::json(""));

  ASSERT_TRUE(consts.contains("file_const"));
  const auto &file_const = consts.at("file_const");
  EXPECT_EQ(file_const.at("type"), JsonFile::json("COMBINED"));
  EXPECT_EQ(file_const.at("file_name"), JsonFile::json("weight_combined.bin"));
}

TEST_F(Om2PackageHelperUt, ConvertOm2Model_Fail_GenFailedAndRemoveOm2File) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateInvalidGeRootModel();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, kZipFileBaseName + "_invalid.om2"});
  ASSERT_NE(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);
  ASSERT_NE(mmAccess2(output_file.c_str(), M_F_OK), EOK);
}

TEST_F(Om2PackageHelperUt, Om2CodegenAndCompile_Fail_DumpGeneratedFiles) {
  const std::vector<std::string> dump_files = {
      "g1_kernel_reg.cpp",
      "g1_resources.cpp",
      "g1_args_manager.cpp",
      "g1_load_and_run.cpp",
      "g1_interface.h",
      "Makefile",
  };
  for (const auto &file_name : dump_files) {
    RemoveOm2DumpFile(file_name);
  }

  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);
  const auto &name_to_ge_model = ge_root_model->GetSubgraphInstanceNameToModel();
  ASSERT_FALSE(name_to_ge_model.empty());
  const auto &ge_model = name_to_ge_model.begin()->second;
  ASSERT_NE(ge_model, nullptr);
  ASSERT_NE(ge_model->GetModelTaskDefPtr(), nullptr);
  ASSERT_GT(ge_model->GetModelTaskDefPtr()->task_size(), 0);
  auto *kernel_def = ge_model->GetModelTaskDefPtr()->mutable_task(0)->mutable_kernel();
  ASSERT_NE(kernel_def, nullptr);
  kernel_def->set_kernel_name("bad\"kernel_name");

  Om2CodegenArtifacts artifacts;
  Om2ConstMetas const_metas;
  Om2Codegen codegen;
  ASSERT_NE(codegen.Om2CodegenAndCompile(ge_model, artifacts, const_metas), SUCCESS);

  if (!IsTmpDirExists()) {
    return;
  }

  std::string kernel_reg_content;
  ASSERT_TRUE(ReadOm2DumpFile("g1_kernel_reg.cpp", kernel_reg_content));
  EXPECT_NE(kernel_reg_content.find("bad\"kernel_name"), std::string::npos);
  EXPECT_NE(kernel_reg_content.find("struct AicoreRegisterInfo"), std::string::npos);
  EXPECT_NE(kernel_reg_content.find("aclError Om2Model::RegisterKernels()"), std::string::npos);

  std::string makefile_content;
  ASSERT_TRUE(ReadOm2DumpFile("Makefile", makefile_content));
  EXPECT_NE(makefile_content.find("TARGET := libg1_om2.so"), std::string::npos);
  EXPECT_NE(makefile_content.find("SRC_FILES := g1_resources.cpp g1_kernel_reg.cpp"), std::string::npos);

  for (const auto &file_name : dump_files) {
    RemoveOm2DumpFile(file_name);
  }
}

TEST_F(Om2PackageHelperUt, ConvertOm2Model_Ok_GenOm2WithFileConstMeta) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithFileConstOp();
  ASSERT_NE(ge_root_model, nullptr);
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "fake_fileconst.om2"});
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);

  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());
  const auto file_names = archive.ListFiles();
  EXPECT_EQ(std::find(file_names.begin(), file_names.end(), "fake_fileconst/data/constants/constant_0"),
            file_names.end());

  size_t constants_config_size = 0;
  const auto constants_config_buf =
      archive.ExtractToMem("fake_fileconst/data/constants/model_0_constants_config.json", constants_config_size);
  ASSERT_NE(constants_config_buf, nullptr);
  const JsonFile constants_json(reinterpret_cast<const uint8_t *>(constants_config_buf.get()), constants_config_size);
  ASSERT_TRUE(constants_json.IsValid());
  EXPECT_EQ(constants_json.Raw().at("internal_weight_size"), JsonFile::json(0));
  EXPECT_FALSE(constants_json.Raw().contains("weight_size"));
  ASSERT_TRUE(constants_json.Raw().contains("consts"));
  const auto &consts = constants_json.Raw().at("consts");
  ASSERT_TRUE(consts.is_object());
  ASSERT_EQ(consts.size(), 2U);
  ASSERT_TRUE(consts.contains("constant_1"));
  ASSERT_TRUE(consts.contains("data2"));

  const auto &internal_const = consts.at("constant_1");
  EXPECT_EQ(internal_const.at("index"), JsonFile::json(1));
  EXPECT_EQ(internal_const.at("type"), JsonFile::json("INTERNAL"));
  EXPECT_EQ(internal_const.at("file_name"), JsonFile::json(""));
  EXPECT_EQ(internal_const.at("op_name"), JsonFile::json(""));
  EXPECT_EQ(internal_const.at("offset"), JsonFile::json(0));
  EXPECT_EQ(internal_const.at("size"), JsonFile::json(200704));

  const auto &file_const = consts.at("data2");
  EXPECT_EQ(file_const.at("op_name"), JsonFile::json("data2"));
  EXPECT_EQ(file_const.at("index"), JsonFile::json(0));
  EXPECT_EQ(file_const.at("type"), JsonFile::json("COMBINED"));
  EXPECT_EQ(file_const.at("file_name"), JsonFile::json("weight_combined.bin"));
  EXPECT_EQ(file_const.at("offset"), JsonFile::json(64));
  EXPECT_EQ(file_const.at("size"), JsonFile::json(200704));
}

TEST_F(Om2PackageHelperUt, SaveOpAttrJson_WithAttr_GenValidOpAttrJson) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);

  auto compute_graph = ge_root_model->GetRootGraph();
  ASSERT_NE(compute_graph, nullptr);

  // 找到算子并设置属性
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc != nullptr && op_desc->GetType() != DATA && op_desc->GetType() != NETOUTPUT) {
      std::vector<std::string> original_op_names = {"original_op1", "original_op2"};
      AttrUtils::SetListStr(op_desc, ATTR_NAME_DATA_DUMP_ORIGIN_OP_NAMES, original_op_names);
    }
  }

  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "test_op_attr.om2"});
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);

  // 解压并验证op_attr.json
  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());

  size_t op_attr_size = 0;
  const auto op_attr_buf = archive.ExtractToMem("test_op_attr/data/model_0/debug/op_attr.json", op_attr_size);
  ASSERT_NE(op_attr_buf, nullptr);

  const JsonFile op_attr_json(reinterpret_cast<const uint8_t *>(op_attr_buf.get()), op_attr_size);
  ASSERT_TRUE(op_attr_json.IsValid());

  // 验证JSON结构
  const auto &raw_json = op_attr_json.Raw();
  EXPECT_TRUE(raw_json.is_object());
  EXPECT_TRUE(raw_json.contains("add1"));

  const auto &op_attr = raw_json.at("add1");
  EXPECT_TRUE(op_attr.is_object());
  EXPECT_TRUE(op_attr.contains("_datadump_original_op_names"));

  const auto &attr_value = op_attr.at("_datadump_original_op_names");
  EXPECT_EQ(attr_value.at("type"), JsonFile::json("LIST_STRING"));
  EXPECT_TRUE(attr_value.at("value").is_array());

  const auto &value_array = attr_value.at("value");
  EXPECT_EQ(value_array.size(), 2U);
  EXPECT_EQ(value_array[0], JsonFile::json("original_op1"));
  EXPECT_EQ(value_array[1], JsonFile::json("original_op2"));
}

TEST_F(Om2PackageHelperUt, SaveOpAttrJson_NoAttr_GenEmptyOpAttrJson) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);

  // 不设置任何属性
  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "test_empty_attr.om2"});
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);

  // 解压并验证op_attr.json为空对象
  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());

  size_t op_attr_size = 0;
  const auto op_attr_buf = archive.ExtractToMem("test_empty_attr/data/model_0/debug/op_attr.json", op_attr_size);
  ASSERT_NE(op_attr_buf, nullptr);

  const JsonFile op_attr_json(reinterpret_cast<const uint8_t *>(op_attr_buf.get()), op_attr_size);
  ASSERT_TRUE(op_attr_json.IsValid());

  // 验证JSON为空对象 {}
  const auto &raw_json = op_attr_json.Raw();
  EXPECT_TRUE(raw_json.is_object());
  EXPECT_EQ(raw_json.size(), 0U);
}

TEST_F(Om2PackageHelperUt, SaveOpAttrJson_EmptyOriginalOpNames_GenValidOpAttrJson) {
  Om2PackageHelper om2_packager;
  const auto ge_root_model = CreateGeRootModelWithAicoreOp();
  ASSERT_NE(ge_root_model, nullptr);

  auto compute_graph = ge_root_model->GetRootGraph();
  ASSERT_NE(compute_graph, nullptr);

  // 设置空列表属性
  for (const auto &node : compute_graph->GetDirectNode()) {
    auto op_desc = node->GetOpDesc();
    if (op_desc != nullptr && op_desc->GetType() != DATA && op_desc->GetType() != NETOUTPUT) {
      std::vector<std::string> empty_op_names = {};
      AttrUtils::SetListStr(op_desc, ATTR_NAME_DATA_DUMP_ORIGIN_OP_NAMES, empty_op_names);
      break;
    }
  }

  ModelBufferData model_data;
  const std::string output_file = PathUtils::Join({test_work_dir, "test_empty_list_attr.om2"});
  ASSERT_EQ(om2_packager.SaveToOmRootModel(ge_root_model, output_file, model_data, false), SUCCESS);

  // 解压并验证op_attr.json
  uint32_t model_buf_size = 0;
  const auto model_buf = GetBinDataFromFile(output_file, model_buf_size);
  RAIIZipArchive archive(reinterpret_cast<const uint8_t *>(model_buf.get()), model_buf_size);
  ASSERT_TRUE(archive.IsGood());

  size_t op_attr_size = 0;
  const auto op_attr_buf = archive.ExtractToMem("test_empty_list_attr/data/model_0/debug/op_attr.json", op_attr_size);
  ASSERT_NE(op_attr_buf, nullptr);

  const JsonFile op_attr_json(reinterpret_cast<const uint8_t *>(op_attr_buf.get()), op_attr_size);
  ASSERT_TRUE(op_attr_json.IsValid());

  // 验证value为空数组 []
  const auto &raw_json = op_attr_json.Raw();
  EXPECT_TRUE(raw_json.contains("add1"));
  const auto &attr_value = raw_json.at("add1").at("_datadump_original_op_names");
  EXPECT_TRUE(attr_value.at("value").is_array());
  EXPECT_EQ(attr_value.at("value").size(), 0U);
}
}  // namespace ge
