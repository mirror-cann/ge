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

#include <cstdlib>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "framework/runtime/dump/model_dump_manager.h"
#include "framework/runtime/om2_model_executor.h"
#include "framework/runtime/rt_session.h"
#include "common/env_path.h"
#include "common/helper/om2/zip_archive_writer.h"
#include "common/path_utils.h"
#include "graph/utils/file_utils.h"
#include "mmpa/mmpa_api.h"
#include "graph_metadef/depends/checker/tensor_check_utils.h"
#include "ge/ge_error_codes.h"

namespace ge {
namespace {
constexpr const char *kOm2BaseName = "om2_model_executor_test";
constexpr const char *kModelName = "g1";
constexpr uint32_t kTestModelId = 9527U;
constexpr uintptr_t kFakeRtModelHandleValue = 0x12345678U;

struct ModelDataHolder {
  ge::ModelData model_data{};
  std::unique_ptr<char[]> buffer;
};

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

std::string MakeManifestJson() {
  return R"({
    "atc_command": "",
    "model_num": 1,
    "om2_version": "0"
})";
}

std::string MakeModelMetaJson() {
  return R"({
    "dynamic_batch_info": [],
    "dynamic_output_shape": [],
    "dynamic_type": 0,
    "inputs": [
        {
            "data_type": "DT_FLOAT",
            "format": "ND",
            "index": 0,
            "name": "data1",
            "shape": [1, 2, 3, 4],
            "shape_range": [],
            "shape_v2": [1, 2, 3, 4],
            "size": 0
        },
        {
            "data_type": "DT_FLOAT",
            "format": "NCHW",
            "index": 1,
            "name": "data2",
            "shape": [1, 1, 224, 224],
            "shape_range": [],
            "shape_v2": [1, 1, 224, 224],
            "size": 0
        }
    ],
    "name": "g1",
    "root_graph_name": "root_g1",
    "outputs": [
        {
            "data_type": "DT_FLOAT",
            "format": "ND",
            "index": 0,
            "name": "output_0_reshape1_0",
            "shape": [],
            "shape_range": [],
            "size": 4
        }
    ],
    "work_size": 2048,
    "user_designate_shape_order": []
})";
}

std::string MakeModelMetaJsonWithoutRootGraphName() {
  return R"({
    "dynamic_batch_info": [],
    "dynamic_output_shape": [],
    "dynamic_type": 0,
    "inputs": [],
    "name": "g1",
    "outputs": [],
    "work_size": 2048,
    "user_designate_shape_order": []
})";
}

std::string MakeInterfaceHeader() {
  return R"(#pragma once

#include <cstddef>
#include <cstdint>

namespace om2 {
struct FakeModel {
  uint64_t session_id;
};
}

extern "C" {
int Om2ModelCreate(void **model_handle, void **rt_model_handle, const char **bin_files, const void **bin_data,
                   size_t *bin_size, int bin_num, void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id, void *instance_handle);
int Om2ModelLoad(void **model_handle);
int Om2ModelRunAsync(void **model_handle, void *stream, int input_count, void **input_data, int output_count,
                     void **output_data);
int Om2ModelRun(void **model_handle, int input_count, void **input_data, int output_count, void **output_data, int32_t stream_sync_timeout);
int Om2ModelDestroy(void **model_handle);
}
)";
}

std::string MakeLoadAndRunCpp() {
  return R"(#include "g1_interface.h"

#include <cstdlib>
#include <fstream>
#include <new>
#include <string>

namespace {
constexpr uintptr_t kFakeRtModelHandleValue = 0x12345678U;

bool CheckWorkPtr(void *work_ptr) {
  const char *mode = std::getenv("OM2_EXPECT_WORK_PTR_MODE");
  if ((mode == nullptr) || (mode[0] == '\0')) {
    return true;
  }
  const std::string mode_str(mode);
  if (mode_str == "NON_NULL") {
    return work_ptr != nullptr;
  }
  if (mode_str == "EQUAL") {
    const char *value = std::getenv("OM2_EXPECT_WORK_PTR_VALUE");
    if (value == nullptr) {
      return false;
    }
    const auto expect_ptr = reinterpret_cast<void *>(std::stoull(value, nullptr, 16));
    return work_ptr == expect_ptr;
  }
  return false;
}

bool CheckConst0(void **constants) {
  const char *mode = std::getenv("OM2_EXPECT_CONST0_MODE");
  if ((mode == nullptr) || (mode[0] == '\0')) {
    return true;
  }
  if ((constants == nullptr) || (constants[0] == nullptr)) {
    return false;
  }
  const char *value = std::getenv("OM2_EXPECT_CONST0_FIRST_BYTE");
  if (value == nullptr) {
    return false;
  }
  const auto expect = static_cast<unsigned char>(std::stoul(value, nullptr, 10));
  return *(static_cast<unsigned char *>(constants[0])) == expect;
}

bool CheckConst0Ptr(void **constants) {
  const char *mode = std::getenv("OM2_EXPECT_CONST0_PTR_MODE");
  if ((mode == nullptr) || (mode[0] == '\0')) {
    return true;
  }
  if ((constants == nullptr) || (constants[0] == nullptr)) {
    return false;
  }
  const std::string mode_str(mode);
  if (mode_str == "EQUAL") {
    const char *value = std::getenv("OM2_EXPECT_CONST0_PTR_VALUE");
    if (value == nullptr) {
      return false;
    }
    const auto expect_ptr = reinterpret_cast<void *>(std::stoull(value, nullptr, 16));
    return constants[0] == expect_ptr;
  }
  return false;
}

bool CheckConstByIndex(void **constants, size_t index, const char *mode_env, const char *value_env) {
  const char *mode = std::getenv(mode_env);
  if ((mode == nullptr) || (mode[0] == '\0')) {
    return true;
  }
  if ((constants == nullptr) || (constants[index] == nullptr)) {
    return false;
  }
  const char *value = std::getenv(value_env);
  if (value == nullptr) {
    return false;
  }
  const auto expect = static_cast<unsigned char>(std::stoul(value, nullptr, 10));
  return *(static_cast<unsigned char *>(constants[index])) == expect;
}

bool CheckConstPtrEqual(void **constants) {
  const char *value = std::getenv("OM2_EXPECT_CONST1_CONST2_PTR_EQUAL");
  if ((value == nullptr) || (value[0] == '\0')) {
    return true;
  }
  if ((constants == nullptr) || (constants[1] == nullptr) || (constants[2] == nullptr)) {
    return false;
  }
  return constants[1] == constants[2];
}

bool CheckSessionId(uint64_t *session_id) {
  const char *value = std::getenv("OM2_EXPECT_SESSION_ID");
  if ((value == nullptr) || (value[0] == '\0')) {
    return true;
  }
  if (session_id == nullptr) {
    return false;
  }
  if (std::string(value) == "ANY") {
    return true;
  }
  const auto expect = static_cast<uint64_t>(std::stoull(value, nullptr, 10));
  return *session_id == expect;
}

bool CheckModelId(uint32_t model_id) {
  const char *value = std::getenv("OM2_EXPECT_MODEL_ID");
  if ((value == nullptr) || (value[0] == '\0')) {
    return true;
  }
  const auto expect = static_cast<uint32_t>(std::stoul(value, nullptr, 10));
  return model_id == expect;
}

bool CheckInstanceHandle(void *instance_handle) {
  const char *mode = std::getenv("OM2_EXPECT_INSTANCE_HANDLE_MODE");
  if ((mode == nullptr) || (mode[0] == '\0')) {
    return true;
  }
  return std::string(mode) == "NON_NULL" && instance_handle != nullptr;
}
}  // namespace

extern "C" int Om2ModelCreate(void **model_handle, void **rt_model_handle, const char **, const void **, size_t *, int,
                              void **constants, void *work_ptr, uint64_t *session_id, uint32_t model_id,
                              void *instance_handle) {
  if ((model_handle == nullptr) || (rt_model_handle == nullptr)) {
    return 1;
  }
  if (!CheckWorkPtr(work_ptr) || !CheckConst0(constants) || !CheckConst0Ptr(constants) ||
      !CheckConstByIndex(constants, 1U, "OM2_EXPECT_CONST1_MODE", "OM2_EXPECT_CONST1_FIRST_BYTE") ||
      !CheckConstByIndex(constants, 2U, "OM2_EXPECT_CONST2_MODE", "OM2_EXPECT_CONST2_FIRST_BYTE") ||
      !CheckConstPtrEqual(constants) ||
      !CheckSessionId(session_id) || !CheckModelId(model_id) || !CheckInstanceHandle(instance_handle)) {
    return 1;
  }
  auto *model = new (std::nothrow) om2::FakeModel();
  if (model == nullptr) {
    return 1;
  }
  model->session_id = (session_id == nullptr) ? 0UL : *session_id;
  *model_handle = model;
  *rt_model_handle = reinterpret_cast<void *>(kFakeRtModelHandleValue);
  const char *trace = std::getenv("OM2_CALL_TRACE");
  if (trace != nullptr) {
    std::ofstream ofs(trace, std::ios::app);
    ofs << "create\n";
  }
  return 0;
}

extern "C" int Om2ModelLoad(void **model_handle) {
  const char *trace = std::getenv("OM2_CALL_TRACE");
  if (trace != nullptr) {
    std::ofstream ofs(trace, std::ios::app);
    ofs << "load\n";
  }
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
  delete static_cast<om2::FakeModel *>(*model_handle);
  *model_handle = nullptr;
  return 0;
}
)";
}

std::string MakeEmptyCpp(const std::string &header_name) {
  return "#include \"" + header_name + "\"\n";
}

std::string MakeCMakeLists() {
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

std::string MakeConstantsConfigJson() {
  return R"({
    "internal_weight_size": 16,
    "consts": {
      "fc1_weight": {
        "file_name": "",
        "index": 0,
        "type": "INTERNAL",
        "offset": 0,
        "size": 16,
        "op_name": "fc1_weight"
      }
    }
  })";
}

std::string MakeIndividualConstantsConfigJson() {
  return R"({
    "internal_weight_size": 0,
    "consts": {
      "fc1_weight": {
        "file_name": "fc.bin",
        "index": 0,
        "type": "INDIVIDUAL",
        "offset": 1,
        "size": 2,
        "op_name": "fc1_weight"
      }
    }
  })";
}

std::string MakeIndividualConstantsConfigJsonWithZeroInternalWeightSize() {
  return R"({
    "internal_weight_size": 0,
    "consts": {
      "fc1_weight": {
        "file_name": "fc.bin",
        "index": 0,
        "type": "INDIVIDUAL",
        "offset": 1,
        "size": 2,
        "op_name": "fc1_weight"
      }
    }
  })";
}

std::string MakeCombinedConstantsConfigJson() {
  return R"({
    "internal_weight_size": 0,
    "consts": {
      "fc1_weight": {
        "file_name": "combined.bin",
        "index": 0,
        "type": "COMBINED",
        "offset": 1,
        "size": 2,
        "op_name": "fc1_weight"
      }
    }
  })";
}

std::string MakeMixedConstantsConfigJson() {
  return R"({
    "internal_weight_size": 16,
    "consts": {
      "fc0_weight": {
        "file_name": "",
        "index": 0,
        "type": "INTERNAL",
        "offset": 0,
        "size": 16,
        "op_name": "fc0_weight"
      },
      "fc1_weight": {
        "file_name": "mixed_fc.bin",
        "index": 1,
        "type": "INDIVIDUAL",
        "offset": 1,
        "size": 2,
        "op_name": "fc1_weight"
      },
      "fc2_weight": {
        "file_name": "mixed_combined.bin",
        "index": 2,
        "type": "COMBINED",
        "offset": 1,
        "size": 2,
        "op_name": "fc2_weight"
      }
    }
  })";
}

std::string MakeDuplicateIndividualConstantsConfigJson() {
  return R"({
    "internal_weight_size": 0,
    "consts": {
      "fc1_weight": {
        "file_name": "duplicate_fc.bin",
        "index": 1,
        "type": "INDIVIDUAL",
        "offset": 1,
        "size": 2,
        "op_name": "fc1_weight"
      },
      "fc2_weight": {
        "file_name": "duplicate_fc.bin",
        "index": 2,
        "type": "INDIVIDUAL",
        "offset": 1,
        "size": 2,
        "op_name": "fc2_weight"
      }
    }
  })";
}

std::string PtrToHexString(const void *ptr) {
  std::ostringstream oss;
  oss << std::hex << reinterpret_cast<uintptr_t>(ptr);
  return oss.str();
}

class EnvValueGuard {
 public:
  explicit EnvValueGuard(const char *name) : name_(name) {
    const char *value = std::getenv(name_.c_str());
    if (value != nullptr) {
      old_value_ = value;
      had_value_ = true;
    }
  }

  ~EnvValueGuard() {
    if (had_value_) {
      (void)setenv(name_.c_str(), old_value_.c_str(), 1);
    } else {
      (void)unsetenv(name_.c_str());
    }
  }

 private:
  std::string name_;
  std::string old_value_;
  bool had_value_ = false;
};

gert::Om2ModelLoadArg MakeOm2LoadArg() {
  gert::Om2ModelLoadArg load_arg;
  load_arg.device_id = 0;
  load_arg.model_id = kTestModelId;
  return load_arg;
}

std::vector<std::string> ReadTraceFile(const std::string &trace_file) {
  std::ifstream ifs(trace_file);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(ifs, line)) {
    lines.push_back(line);
  }
  return lines;
}
}  // namespace

class Om2ModelExecutorUt : public testing::Test {
 protected:
  void SetUp() override {
    unsetenv("OM2_EXPECT_WORK_PTR_MODE");
    unsetenv("OM2_EXPECT_WORK_PTR_VALUE");
    unsetenv("OM2_EXPECT_CONST0_MODE");
    unsetenv("OM2_EXPECT_CONST0_FIRST_BYTE");
    unsetenv("OM2_EXPECT_CONST0_PTR_MODE");
    unsetenv("OM2_EXPECT_CONST0_PTR_VALUE");
    unsetenv("OM2_EXPECT_CONST1_MODE");
    unsetenv("OM2_EXPECT_CONST1_FIRST_BYTE");
    unsetenv("OM2_EXPECT_CONST2_MODE");
    unsetenv("OM2_EXPECT_CONST2_FIRST_BYTE");
    unsetenv("OM2_EXPECT_CONST1_CONST2_PTR_EQUAL");
    unsetenv("OM2_EXPECT_SESSION_ID");
    unsetenv("OM2_EXPECT_MODEL_ID");
    unsetenv("OM2_EXPECT_INSTANCE_HANDLE_MODE");
    unsetenv("OM2_CALL_TRACE");
  }

  void TearDown() override {
    unsetenv("OM2_EXPECT_WORK_PTR_MODE");
    unsetenv("OM2_EXPECT_WORK_PTR_VALUE");
    unsetenv("OM2_EXPECT_CONST0_MODE");
    unsetenv("OM2_EXPECT_CONST0_FIRST_BYTE");
    unsetenv("OM2_EXPECT_CONST0_PTR_MODE");
    unsetenv("OM2_EXPECT_CONST0_PTR_VALUE");
    unsetenv("OM2_EXPECT_CONST1_MODE");
    unsetenv("OM2_EXPECT_CONST1_FIRST_BYTE");
    unsetenv("OM2_EXPECT_CONST2_MODE");
    unsetenv("OM2_EXPECT_CONST2_FIRST_BYTE");
    unsetenv("OM2_EXPECT_CONST1_CONST2_PTR_EQUAL");
    unsetenv("OM2_EXPECT_SESSION_ID");
    unsetenv("OM2_EXPECT_MODEL_ID");
    unsetenv("OM2_EXPECT_INSTANCE_HANDLE_MODE");
    unsetenv("OM2_CALL_TRACE");
  }

  static void SetUpTestSuite() {
    test_work_dir_ = EnvPath().GetOrCreateCaseTmpPath("Om2ModelExecutorUt");
    setenv("ASCEND_WORK_PATH", test_work_dir_.c_str(), 1);
    om2_file_path_ = PathUtils::Join({test_work_dir_, std::string(kOm2BaseName) + ".om2"});
    om2_fileconst_file_path_ = PathUtils::Join({test_work_dir_, std::string(kOm2BaseName) + "_fileconst.om2"});
    om2_combined_file_path_ = PathUtils::Join({test_work_dir_, std::string(kOm2BaseName) + "_combined.om2"});
    om2_mixed_file_path_ = PathUtils::Join({test_work_dir_, std::string(kOm2BaseName) + "_mixed.om2"});
    om2_duplicate_individual_file_path_ =
        PathUtils::Join({test_work_dir_, std::string(kOm2BaseName) + "_duplicate_individual.om2"});
    PrepareOm2File();
    PrepareFileConstOm2File();
    PrepareCombinedFileConstOm2File();
    PrepareMixedOm2File();
    PrepareDuplicateIndividualOm2File();
  }

  static void TearDownTestSuite() {
    unsetenv("ASCEND_WORK_PATH");
    EnvPath().RemoveRfCaseTmpPath("Om2ModelExecutorUt");
  }

  static void PrepareOm2File() {
    std::call_once(prepare_once_, []() {
      const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime"});
      const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
      const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});
      const std::string archive_constant_path = PathUtils::Join({test_work_dir_, "constant_0"});
      const std::string archive_constant_cfg_path = PathUtils::Join({test_work_dir_, "model_0_constants_config.json"});

      (void)PathUtils::RemoveDirectories(runtime_dir);
      ASSERT_EQ(CreateDir(runtime_dir), 0);
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
      WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

      const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
      const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
      RunCommandOrAssert(cmake_config_cmd);
      RunCommandOrAssert(cmake_build_cmd);
      ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

      WriteBinaryFile(archive_constant_path, std::vector<uint8_t>{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
                                                                  9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U});
      WriteTextFile(archive_constant_cfg_path, MakeConstantsConfigJson());

      ZipArchiveWriter zip_writer(om2_file_path_);
      ASSERT_TRUE(zip_writer.IsMemFileOpened());
      const auto manifest = MakeManifestJson();
      const auto model_meta = MakeModelMetaJson();
      ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
      ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt", PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h", PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp", PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp", PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp", PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp", PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
      ASSERT_TRUE(zip_writer.WriteFile("data/constants/constant_0", archive_constant_path, false));
      ASSERT_TRUE(zip_writer.WriteFile("data/constants/model_0_constants_config.json", archive_constant_cfg_path, false));
      ASSERT_TRUE(zip_writer.SaveModelDataToFile());
      ASSERT_EQ(mmAccess2(om2_file_path_.c_str(), M_F_OK), EOK);
    });
  }

  static void PrepareFileConstOm2File() {
    std::call_once(prepare_fileconst_once_, []() {
      const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime_fileconst"});
      const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
      const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});
      const std::string archive_constant_cfg_path = PathUtils::Join({test_work_dir_, "model_1_constants_config.json"});
      const std::string weight_dir = PathUtils::Join({test_work_dir_, "weight"});
      const std::string file_const_path = PathUtils::Join({weight_dir, "fc.bin"});

      (void)PathUtils::RemoveDirectories(runtime_dir);
      ASSERT_EQ(CreateDir(runtime_dir), 0);
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
      WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

      const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
      const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
      RunCommandOrAssert(cmake_config_cmd);
      RunCommandOrAssert(cmake_build_cmd);
      ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

      WriteTextFile(archive_constant_cfg_path, MakeIndividualConstantsConfigJson());
      WriteBinaryFile(file_const_path, {21U, 22U, 23U, 24U});

      ZipArchiveWriter zip_writer(om2_fileconst_file_path_);
      ASSERT_TRUE(zip_writer.IsMemFileOpened());
      const auto manifest = MakeManifestJson();
      const auto model_meta = MakeModelMetaJson();
      ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
      ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt", PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h", PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp", PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp", PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp", PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp", PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
      ASSERT_TRUE(zip_writer.WriteFile("data/constants/model_1_constants_config.json", archive_constant_cfg_path, false));
      ASSERT_TRUE(zip_writer.SaveModelDataToFile());
      ASSERT_EQ(mmAccess2(om2_fileconst_file_path_.c_str(), M_F_OK), EOK);
    });
  }

  static void PrepareCombinedFileConstOm2File() {
    std::call_once(prepare_combined_once_, []() {
      const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime_combined"});
      const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
      const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});
      const std::string archive_constant_cfg_path = PathUtils::Join({test_work_dir_, "model_2_constants_config.json"});
      const std::string weight_dir = PathUtils::Join({test_work_dir_, "weight"});
      const std::string file_const_path = PathUtils::Join({weight_dir, "combined.bin"});

      (void)PathUtils::RemoveDirectories(runtime_dir);
      ASSERT_EQ(CreateDir(runtime_dir), 0);
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
      WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

      const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
      const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
      RunCommandOrAssert(cmake_config_cmd);
      RunCommandOrAssert(cmake_build_cmd);
      ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

      WriteTextFile(archive_constant_cfg_path, MakeCombinedConstantsConfigJson());
      WriteBinaryFile(file_const_path, {41U, 42U, 43U, 44U});

      ZipArchiveWriter zip_writer(om2_combined_file_path_);
      ASSERT_TRUE(zip_writer.IsMemFileOpened());
      const auto manifest = MakeManifestJson();
      const auto model_meta = MakeModelMetaJson();
      ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
      ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt", PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h", PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp", PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp", PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp", PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp", PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
      ASSERT_TRUE(zip_writer.WriteFile("data/constants/model_2_constants_config.json", archive_constant_cfg_path, false));
      ASSERT_TRUE(zip_writer.SaveModelDataToFile());
      ASSERT_EQ(mmAccess2(om2_combined_file_path_.c_str(), M_F_OK), EOK);
    });
  }

  static void PrepareMixedOm2File() {
    std::call_once(prepare_mixed_once_, []() {
      const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime_mixed"});
      const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
      const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});
      const std::string archive_constant_path = PathUtils::Join({test_work_dir_, "constant_mixed_0"});
      const std::string archive_constant_cfg_path = PathUtils::Join({test_work_dir_, "model_3_constants_config.json"});
      const std::string weight_dir = PathUtils::Join({test_work_dir_, "weight"});
      const std::string individual_path = PathUtils::Join({weight_dir, "mixed_fc.bin"});
      const std::string combined_path = PathUtils::Join({weight_dir, "mixed_combined.bin"});

      (void)PathUtils::RemoveDirectories(runtime_dir);
      ASSERT_EQ(CreateDir(runtime_dir), 0);
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
      WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

      const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
      const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
      RunCommandOrAssert(cmake_config_cmd);
      RunCommandOrAssert(cmake_build_cmd);
      ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

      WriteBinaryFile(archive_constant_path, std::vector<uint8_t>{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U,
                                                                  9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U});
      WriteTextFile(archive_constant_cfg_path, MakeMixedConstantsConfigJson());
      WriteBinaryFile(individual_path, {61U, 62U, 63U, 64U});
      WriteBinaryFile(combined_path, {71U, 72U, 73U, 74U});

      ZipArchiveWriter zip_writer(om2_mixed_file_path_);
      ASSERT_TRUE(zip_writer.IsMemFileOpened());
      const auto manifest = MakeManifestJson();
      const auto model_meta = MakeModelMetaJson();
      ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
      ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt", PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h", PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp", PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp", PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp", PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp", PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
      ASSERT_TRUE(zip_writer.WriteFile("data/constants/constant_0", archive_constant_path, false));
      ASSERT_TRUE(zip_writer.WriteFile("data/constants/model_3_constants_config.json", archive_constant_cfg_path, false));
      ASSERT_TRUE(zip_writer.SaveModelDataToFile());
      ASSERT_EQ(mmAccess2(om2_mixed_file_path_.c_str(), M_F_OK), EOK);
    });
  }

  static void PrepareDuplicateIndividualOm2File() {
    std::call_once(prepare_duplicate_individual_once_, []() {
      const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime_duplicate_individual"});
      const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
      const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});
      const std::string archive_constant_cfg_path =
          PathUtils::Join({test_work_dir_, "model_duplicate_individual_constants_config.json"});
      const std::string weight_dir = PathUtils::Join({test_work_dir_, "weight"});
      const std::string individual_path = PathUtils::Join({weight_dir, "duplicate_fc.bin"});

      (void)PathUtils::RemoveDirectories(runtime_dir);
      ASSERT_EQ(CreateDir(runtime_dir), 0);
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
      WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
      WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

      const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
      const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
      RunCommandOrAssert(cmake_config_cmd);
      RunCommandOrAssert(cmake_build_cmd);
      ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

      WriteTextFile(archive_constant_cfg_path, MakeDuplicateIndividualConstantsConfigJson());
      WriteBinaryFile(individual_path, {101U, 102U, 103U, 104U});

      ZipArchiveWriter zip_writer(om2_duplicate_individual_file_path_);
      ASSERT_TRUE(zip_writer.IsMemFileOpened());
      const auto manifest = MakeManifestJson();
      const auto model_meta = MakeModelMetaJson();
      ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
      ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt",
                                       PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h",
                                       PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp",
                                       PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp",
                                       PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp",
                                       PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp",
                                       PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
      ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
      ASSERT_TRUE(zip_writer.WriteFile("data/constants/model_duplicate_individual_constants_config.json",
                                       archive_constant_cfg_path, false));
      ASSERT_TRUE(zip_writer.SaveModelDataToFile());
      ASSERT_EQ(mmAccess2(om2_duplicate_individual_file_path_.c_str(), M_F_OK), EOK);
    });
  }

  static ModelDataHolder LoadValidModelData() {
    PrepareOm2File();
    uint32_t model_buf_size = 0U;
    auto model_buf = GetBinDataFromFile(om2_file_path_, model_buf_size);
    EXPECT_NE(model_buf, nullptr);
    EXPECT_GT(model_buf_size, 0U);

    ModelDataHolder holder;
    holder.model_data.model_data = model_buf.get();
    holder.model_data.model_len = model_buf_size;
    holder.model_data.om_path = om2_file_path_;
    holder.buffer = std::move(model_buf);
    return holder;
  }

  static ModelDataHolder LoadValidFileConstModelData() {
    PrepareFileConstOm2File();
    uint32_t model_buf_size = 0U;
    auto model_buf = GetBinDataFromFile(om2_fileconst_file_path_, model_buf_size);
    EXPECT_NE(model_buf, nullptr);
    EXPECT_GT(model_buf_size, 0U);

    ModelDataHolder holder;
    holder.model_data.model_data = model_buf.get();
    holder.model_data.model_len = model_buf_size;
    holder.model_data.om_path = om2_fileconst_file_path_;
    holder.buffer = std::move(model_buf);
    return holder;
  }

  static ModelDataHolder LoadValidCombinedModelData() {
    PrepareCombinedFileConstOm2File();
    uint32_t model_buf_size = 0U;
    auto model_buf = GetBinDataFromFile(om2_combined_file_path_, model_buf_size);
    EXPECT_NE(model_buf, nullptr);
    EXPECT_GT(model_buf_size, 0U);

    ModelDataHolder holder;
    holder.model_data.model_data = model_buf.get();
    holder.model_data.model_len = model_buf_size;
    holder.model_data.om_path = om2_combined_file_path_;
    holder.buffer = std::move(model_buf);
    return holder;
  }

  static ModelDataHolder LoadValidMixedModelData() {
    PrepareMixedOm2File();
    uint32_t model_buf_size = 0U;
    auto model_buf = GetBinDataFromFile(om2_mixed_file_path_, model_buf_size);
    EXPECT_NE(model_buf, nullptr);
    EXPECT_GT(model_buf_size, 0U);

    ModelDataHolder holder;
    holder.model_data.model_data = model_buf.get();
    holder.model_data.model_len = model_buf_size;
    holder.model_data.om_path = om2_mixed_file_path_;
    holder.buffer = std::move(model_buf);
    return holder;
  }

  static ModelDataHolder LoadValidDuplicateIndividualModelData() {
    PrepareDuplicateIndividualOm2File();
    uint32_t model_buf_size = 0U;
    auto model_buf = GetBinDataFromFile(om2_duplicate_individual_file_path_, model_buf_size);
    EXPECT_NE(model_buf, nullptr);
    EXPECT_GT(model_buf_size, 0U);

    ModelDataHolder holder;
    holder.model_data.model_data = model_buf.get();
    holder.model_data.model_len = model_buf_size;
    holder.model_data.om_path = om2_duplicate_individual_file_path_;
    holder.buffer = std::move(model_buf);
    return holder;
  }

  static void ConstructIoTensors(std::vector<gert::Tensor> &input_tensors, std::vector<gert::Tensor> &output_tensors,
                                 std::vector<gert::Tensor *> &inputs, std::vector<gert::Tensor *> &outputs) {
    input_tensors.resize(2);
    output_tensors.resize(1);
    TensorCheckUtils::ConstructGertTensor(input_tensors[0], {2, 16}, DataType::DT_FLOAT, Format::FORMAT_ND);
    TensorCheckUtils::ConstructGertTensor(input_tensors[1], {2, 16}, DataType::DT_FLOAT, Format::FORMAT_ND);
    TensorCheckUtils::ConstructGertTensor(output_tensors[0], {2, 16}, DataType::DT_FLOAT, Format::FORMAT_ND);

    inputs = {&input_tensors[0], &input_tensors[1]};
    outputs = {&output_tensors[0]};
  }

  static std::string test_work_dir_;
  static std::string om2_file_path_;
  static std::string om2_fileconst_file_path_;
  static std::string om2_combined_file_path_;
  static std::string om2_mixed_file_path_;
  static std::string om2_duplicate_individual_file_path_;
  static std::once_flag prepare_once_;
  static std::once_flag prepare_fileconst_once_;
  static std::once_flag prepare_combined_once_;
  static std::once_flag prepare_mixed_once_;
  static std::once_flag prepare_duplicate_individual_once_;
};

std::string Om2ModelExecutorUt::test_work_dir_;
std::string Om2ModelExecutorUt::om2_file_path_;
std::string Om2ModelExecutorUt::om2_fileconst_file_path_;
std::string Om2ModelExecutorUt::om2_combined_file_path_;
std::string Om2ModelExecutorUt::om2_mixed_file_path_;
std::string Om2ModelExecutorUt::om2_duplicate_individual_file_path_;
std::once_flag Om2ModelExecutorUt::prepare_once_;
std::once_flag Om2ModelExecutorUt::prepare_fileconst_once_;
std::once_flag Om2ModelExecutorUt::prepare_combined_once_;
std::once_flag Om2ModelExecutorUt::prepare_mixed_once_;
std::once_flag Om2ModelExecutorUt::prepare_duplicate_individual_once_;

TEST_F(Om2ModelExecutorUt, load_invalid_model_data) {
  gert::Om2ModelExecutor executor;
  ModelData invalid_model_data{};
  auto load_arg = MakeOm2LoadArg();
  EXPECT_NE(executor.Load(invalid_model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_runtime_so_without_creating_workspace) {
  auto model_data_holder = LoadValidModelData();
  const std::string ascend_work_path = PathUtils::Join({test_work_dir_, "load_without_workspace"});
  const std::string workspace_root = PathUtils::Join({ascend_work_path, ".ascend_temp/.tmp_om2_workspace"});
  (void)PathUtils::RemoveDirectories(ascend_work_path);
  ASSERT_EQ(CreateDir(ascend_work_path), 0);

  EnvValueGuard ascend_work_path_guard("ASCEND_WORK_PATH");
  ASSERT_EQ(setenv("ASCEND_WORK_PATH", ascend_work_path.c_str(), 1), 0);

  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
  EXPECT_NE(mmAccess2(workspace_root.c_str(), M_F_OK), EOK);
}
TEST_F(Om2ModelExecutorUt, load_calls_model_load_after_model_create) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  const auto trace_file = PathUtils::Join({test_work_dir_, "dump_call_trace.txt"});
  (void)std::remove(trace_file.c_str());
  ASSERT_EQ(setenv("OM2_CALL_TRACE", trace_file.c_str(), 1), 0);

  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);

  EXPECT_EQ(ReadTraceFile(trace_file), std::vector<std::string>({"create", "load"}));
}

TEST_F(Om2ModelExecutorUt, load_fallbacks_root_graph_name_to_model_name_when_meta_missing) {
  const std::string om2_file_path = PathUtils::Join({test_work_dir_, "missing_root_graph_name.om2"});
  ZipArchiveWriter zip_writer(om2_file_path);
  ASSERT_TRUE(zip_writer.IsMemFileOpened());
  const auto manifest = MakeManifestJson();
  const auto model_meta = MakeModelMetaJsonWithoutRootGraphName();
  ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so",
                                   PathUtils::Join({test_work_dir_, "fake_runtime", "libg1_om2.so"}), false));
  const std::string constants_config = "{}";
  ASSERT_TRUE(zip_writer.WriteBytes("data/constants/model_0_constants_config.json", constants_config.data(),
                                    constants_config.size(), false));
  ASSERT_TRUE(zip_writer.SaveModelDataToFile());
  uint32_t model_buf_size = 0U;
  auto model_buf = GetBinDataFromFile(om2_file_path, model_buf_size);
  ASSERT_NE(model_buf, nullptr);
  ModelDataHolder holder;
  holder.model_data.model_data = model_buf.get();
  holder.model_data.model_len = model_buf_size;
  holder.model_data.om_path = om2_file_path;
  holder.buffer = std::move(model_buf);

  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  EXPECT_EQ(executor.Load(holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_generates_session_id_without_rt_session) {
  auto model_data_holder = LoadValidModelData();
  auto load_arg = MakeOm2LoadArg();
  ge::Status error_code = SUCCESS;
  ASSERT_EQ(setenv("OM2_EXPECT_SESSION_ID", "ANY", 1), 0);
  auto executor = gert::LoadOm2ExecutorFromData(model_data_holder.model_data, load_arg, error_code);
  EXPECT_EQ(error_code, SUCCESS);
  ASSERT_NE(executor, nullptr);
}

TEST_F(Om2ModelExecutorUt, load_uses_rt_session_id_when_rt_session_is_not_null) {
  auto model_data_holder = LoadValidModelData();
  auto load_arg = MakeOm2LoadArg();
  gert::RtSession rt_session(9527U);
  load_arg.rt_session = &rt_session;
  ge::Status error_code = SUCCESS;
  ASSERT_EQ(setenv("OM2_EXPECT_SESSION_ID", "9527", 1), 0);
  auto executor = gert::LoadOm2ExecutorFromData(model_data_holder.model_data, load_arg, error_code);
  EXPECT_EQ(error_code, SUCCESS);
  ASSERT_NE(executor, nullptr);
}

TEST_F(Om2ModelExecutorUt, load_passes_model_id_and_instance_handle_to_create) {
  auto model_data_holder = LoadValidModelData();
  auto load_arg = MakeOm2LoadArg();
  ge::Status error_code = SUCCESS;
  const auto expected_model_id = std::to_string(kTestModelId);
  ASSERT_EQ(setenv("OM2_EXPECT_MODEL_ID", expected_model_id.c_str(), 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_INSTANCE_HANDLE_MODE", "NON_NULL", 1), 0);
  auto executor = gert::LoadOm2ExecutorFromData(model_data_holder.model_data, load_arg, error_code);
  EXPECT_EQ(error_code, SUCCESS);
  ASSERT_NE(executor, nullptr);
}

TEST_F(Om2ModelExecutorUt, load_failed_when_device_id_is_not_set) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  load_arg.device_id = -1;
  EXPECT_NE(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok_with_external_work_ptr_and_internal_weight_from_archive) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  load_arg.work_ptr = reinterpret_cast<void *>(0x12345);
  load_arg.work_size = 2048U;
  ASSERT_EQ(setenv("OM2_EXPECT_WORK_PTR_MODE", "EQUAL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_WORK_PTR_VALUE", PtrToHexString(load_arg.work_ptr).c_str(), 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_FIRST_BYTE", "1", 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok_with_internal_work_ptr_and_external_device_weight) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  std::vector<uint8_t> device_weight(16U, 0U);
  auto load_arg = MakeOm2LoadArg();
  load_arg.weight_ptr = device_weight.data();
  load_arg.weight_size = device_weight.size();
  ASSERT_EQ(setenv("OM2_EXPECT_WORK_PTR_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_FIRST_BYTE", "1", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_PTR_MODE", "EQUAL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_PTR_VALUE", PtrToHexString(load_arg.weight_ptr).c_str(), 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_failed_when_external_work_ptr_size_too_small) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  load_arg.work_ptr = reinterpret_cast<void *>(0x12345);
  load_arg.work_size = 1024U;
  EXPECT_NE(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_failed_when_external_device_weight_size_too_small) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  std::vector<uint8_t> device_weight(8U, 0U);
  auto load_arg = MakeOm2LoadArg();
  load_arg.weight_ptr = device_weight.data();
  load_arg.weight_size = device_weight.size();
  EXPECT_NE(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok_with_individual_fileconst_from_file) {
  auto model_data_holder = LoadValidFileConstModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_FIRST_BYTE", "22", 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok_with_individual_fileconst_from_user_mem) {
  auto model_data_holder = LoadValidFileConstModelData();
  gert::Om2ModelExecutor executor;
  std::vector<uint8_t> user_mem{31U, 32U, 33U, 34U};
  auto load_arg = MakeOm2LoadArg();
  load_arg.file_constant_mems.push_back({"fc.bin", user_mem.data(), user_mem.size()});
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_FIRST_BYTE", "32", 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok_with_combined_fileconst_from_file) {
  auto model_data_holder = LoadValidCombinedModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_FIRST_BYTE", "42", 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok_with_combined_fileconst_from_user_mem) {
  auto model_data_holder = LoadValidCombinedModelData();
  gert::Om2ModelExecutor executor;
  std::vector<uint8_t> user_mem{51U, 52U, 53U, 54U};
  auto load_arg = MakeOm2LoadArg();
  load_arg.file_constant_mems.push_back({"combined.bin", user_mem.data(), user_mem.size()});
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_FIRST_BYTE", "52", 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok_with_mixed_consts_from_file) {
  auto model_data_holder = LoadValidMixedModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_FIRST_BYTE", "1", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST1_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST1_FIRST_BYTE", "62", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST2_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST2_FIRST_BYTE", "72", 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_ok_with_mixed_consts_and_external_resources) {
  auto model_data_holder = LoadValidMixedModelData();
  gert::Om2ModelExecutor executor;
  std::vector<uint8_t> device_weight(16U, 0U);
  std::vector<uint8_t> individual_mem{80U, 81U, 82U, 83U};
  std::vector<uint8_t> combined_mem{90U, 91U, 92U, 93U};
  auto load_arg = MakeOm2LoadArg();
  load_arg.work_ptr = reinterpret_cast<void *>(0x34567);
  load_arg.work_size = 2048U;
  load_arg.weight_ptr = device_weight.data();
  load_arg.weight_size = device_weight.size();
  load_arg.file_constant_mems.push_back({"mixed_fc.bin", individual_mem.data(), individual_mem.size()});
  load_arg.file_constant_mems.push_back({"mixed_combined.bin", combined_mem.data(), combined_mem.size()});
  ASSERT_EQ(setenv("OM2_EXPECT_WORK_PTR_MODE", "EQUAL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_WORK_PTR_VALUE", PtrToHexString(load_arg.work_ptr).c_str(), 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_FIRST_BYTE", "1", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_PTR_MODE", "EQUAL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST0_PTR_VALUE", PtrToHexString(load_arg.weight_ptr).c_str(), 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST1_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST1_FIRST_BYTE", "81", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST2_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST2_FIRST_BYTE", "91", 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, load_reuses_duplicate_individual_fileconst_in_same_load) {
  auto model_data_holder = LoadValidDuplicateIndividualModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(setenv("OM2_EXPECT_CONST1_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST1_FIRST_BYTE", "102", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST2_MODE", "NON_NULL", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST2_FIRST_BYTE", "102", 1), 0);
  ASSERT_EQ(setenv("OM2_EXPECT_CONST1_CONST2_PTR_EQUAL", "1", 1), 0);
  EXPECT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, run_before_load_failed) {
  gert::Om2ModelExecutor executor;
  std::vector<gert::Tensor> input_tensors;
  std::vector<gert::Tensor> output_tensors;
  std::vector<gert::Tensor *> inputs;
  std::vector<gert::Tensor *> outputs;
  ConstructIoTensors(input_tensors, output_tensors, inputs, outputs);
  EXPECT_NE(executor.Run(inputs, outputs), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, run_ok_after_load) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);

  std::vector<gert::Tensor> input_tensors;
  std::vector<gert::Tensor> output_tensors;
  std::vector<gert::Tensor *> inputs;
  std::vector<gert::Tensor *> outputs;
  ConstructIoTensors(input_tensors, output_tensors, inputs, outputs);
  EXPECT_EQ(executor.Run(inputs, outputs), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, run_async_before_load_failed) {
  gert::Om2ModelExecutor executor;
  std::vector<gert::Tensor> input_tensors;
  std::vector<gert::Tensor> output_tensors;
  std::vector<gert::Tensor *> inputs;
  std::vector<gert::Tensor *> outputs;
  ConstructIoTensors(input_tensors, output_tensors, inputs, outputs);
  EXPECT_NE(executor.RunAsync(nullptr, inputs, outputs), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, run_async_ok_after_load) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);

  std::vector<gert::Tensor> input_tensors;
  std::vector<gert::Tensor> output_tensors;
  std::vector<gert::Tensor *> inputs;
  std::vector<gert::Tensor *> outputs;
  ConstructIoTensors(input_tensors, output_tensors, inputs, outputs);
  EXPECT_EQ(executor.RunAsync(nullptr, inputs, outputs), SUCCESS);
}

TEST_F(Om2ModelExecutorUt, get_model_desc_info_ok) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);

  std::vector<ge::Om2TensorDesc> input_desc;
  std::vector<ge::Om2TensorDesc> output_desc;
  EXPECT_EQ(executor.GetModelDescInfo(input_desc, output_desc, false), SUCCESS);
  ASSERT_EQ(input_desc.size(), 2U);
  ASSERT_EQ(output_desc.size(), 1U);
  EXPECT_EQ(input_desc[0].GetName(), "data1");
  EXPECT_EQ(input_desc[1].GetName(), "data2");
  EXPECT_EQ(output_desc[0].GetName(), "output_0_reshape1_0");

  std::vector<ge::Om2TensorDesc> input_desc_v2;
  std::vector<ge::Om2TensorDesc> output_desc_v2;
  EXPECT_EQ(executor.GetModelDescInfo(input_desc_v2, output_desc_v2, true), SUCCESS);
  EXPECT_EQ(input_desc_v2.size(), input_desc.size());
  EXPECT_EQ(output_desc_v2.size(), output_desc.size());
}

TEST_F(Om2ModelExecutorUt, get_model_attrs_ok) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);

  std::vector<std::string> dynamic_output_shape;
  EXPECT_EQ(executor.GetModelAttrs(dynamic_output_shape), SUCCESS);
  EXPECT_TRUE(dynamic_output_shape.empty());
}

TEST_F(Om2ModelExecutorUt, get_dynamic_batch_info_ok) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);

  std::vector<std::vector<int64_t>> dynamic_batch_info;
  int32_t dynamic_type = -1;
  EXPECT_EQ(executor.GetDynamicBatchInfo(dynamic_batch_info, dynamic_type), SUCCESS);
  EXPECT_TRUE(dynamic_batch_info.empty());
  EXPECT_EQ(dynamic_type, 0);
}

TEST_F(Om2ModelExecutorUt, get_user_designate_shape_order_ok) {
  auto model_data_holder = LoadValidModelData();
  gert::Om2ModelExecutor executor;
  auto load_arg = MakeOm2LoadArg();
  ASSERT_EQ(executor.Load(model_data_holder.model_data, load_arg, 1U), SUCCESS);

  std::vector<std::string> user_designate_shape_order;
  EXPECT_EQ(executor.GetUserDesignateShapeOrder(user_designate_shape_order), SUCCESS);
  EXPECT_TRUE(user_designate_shape_order.empty());
}

TEST_F(Om2ModelExecutorUt, IsOm2Model_Ok_FromDataMultiSceneTest) {
  // invalid model data
  bool is_support = false;
  EXPECT_EQ(gert::IsOm2Model(nullptr, 10, is_support), ACL_ERROR_GE_PARAM_INVALID);

  // data size is too small
  const uint8_t data[] = {0x50, 0x4B};
  is_support = false;
  EXPECT_EQ(gert::IsOm2Model(data, 2, is_support), ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID);

  // valid magic
  const uint8_t valid_data[] = {0x50, 0x4B, 0x03, 0x04};
  is_support = false;
  EXPECT_EQ(gert::IsOm2Model(valid_data, 4, is_support), SUCCESS);
  EXPECT_TRUE(is_support);

  // invalid magic
  constexpr uint8_t invalid_data[] = {0x00, 0x00, 0x00, 0x00};
  is_support = false;
  EXPECT_EQ(gert::IsOm2Model(invalid_data, 4, is_support), SUCCESS);
  EXPECT_FALSE(is_support);
}

TEST_F(Om2ModelExecutorUt, IsOm2Model_Ok_FromFileMultiScene) {
  // file is not exist
  bool is_support = false;
  EXPECT_EQ(gert::IsOm2Model("/non/existent/file.om2", is_support), ACL_ERROR_GE_EXEC_MODEL_PATH_INVALID);

  // file size too small
  const std::string test_file = PathUtils::Join({test_work_dir_, "small_file.om2"});
  WriteBinaryFile(test_file, {0x50, 0x4B});
  is_support = false;
  EXPECT_EQ(gert::IsOm2Model(test_file.c_str(), is_support), ACL_ERROR_GE_EXEC_MODEL_DATA_SIZE_INVALID);

  // valid_magic
  PrepareOm2File();
  is_support = false;
  EXPECT_EQ(gert::IsOm2Model(om2_file_path_.c_str(), is_support), SUCCESS);
  EXPECT_TRUE(is_support);

  // invalid_magic
  const std::string test_file_invalid = PathUtils::Join({test_work_dir_, "invalid_magic.om2"});
  WriteBinaryFile(test_file, {0x00, 0x00, 0x00, 0x00});
  
  is_support = false;
  EXPECT_EQ(gert::IsOm2Model(test_file.c_str(), is_support), SUCCESS);
  EXPECT_FALSE(is_support);
}

TEST_F(Om2ModelExecutorUt, get_mem_and_weight_size_from_file_ok) {
  PrepareOm2File();
  size_t work_size = 0U;
  size_t weight_size = 0U;
  EXPECT_EQ(gert::GetOm2MemAndWeightSize(om2_file_path_, work_size, weight_size), SUCCESS);
  EXPECT_EQ(work_size, 2048U);
  EXPECT_EQ(weight_size, 16U);
}

TEST_F(Om2ModelExecutorUt, get_mem_and_weight_size_external_only_with_zero_internal_weight_size_ok) {
  const std::string om2_file_path = PathUtils::Join({test_work_dir_, "external_only_with_zero_internal_weight_size.om2"});
  ZipArchiveWriter zip_writer(om2_file_path);
  ASSERT_TRUE(zip_writer.IsMemFileOpened());
  const auto manifest = MakeManifestJson();
  const auto constants_config = MakeIndividualConstantsConfigJsonWithZeroInternalWeightSize();
  const auto model_meta = MakeModelMetaJson();
  ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/constants/model_0_constants_config.json",
                                    constants_config.data(), constants_config.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
  ASSERT_TRUE(zip_writer.SaveModelDataToFile());

  size_t work_size = 0U;
  size_t weight_size = 1024U;
  EXPECT_EQ(gert::GetOm2MemAndWeightSize(om2_file_path, work_size, weight_size), SUCCESS);
  EXPECT_EQ(work_size, 2048U);
  EXPECT_EQ(weight_size, 0U);
}

TEST_F(Om2ModelExecutorUt, get_mem_and_weight_size_from_mem_ok) {
  auto model_data_holder = LoadValidModelData();
  size_t work_size = 0U;
  size_t weight_size = 0U;
  EXPECT_EQ(gert::GetOm2MemAndWeightSize(model_data_holder.model_data.model_data,
                                         model_data_holder.model_data.model_len, work_size, weight_size), SUCCESS);
  EXPECT_EQ(work_size, 2048U);
  EXPECT_EQ(weight_size, 16U);
}

// 辅助函数：生成带属性的op_attr.json
static std::string MakeOpAttrJson() {
  return R"({
    "test_op": {
      "_datadump_original_op_names": {
        "type": "LIST_STRING",
        "value": ["original_op1", "original_op2"]
      }
    }
  })";
}

// 辅助函数：生成空op_attr.json
static std::string MakeEmptyOpAttrJson() {
  return "{}";
}

// 辅助函数：生成无效的JSON
static std::string MakeInvalidOpAttrJson() {
  return "invalid json content";
}

// 辅助函数：生成多个算子属性的op_attr.json
static std::string MakeMultipleOpAttrJson() {
  return R"({
    "op1": {
      "_datadump_original_op_names": {
        "type": "LIST_STRING",
        "value": ["orig1", "orig2"]
      },
      "_another_attr": {
        "type": "STRING",
        "value": "test_value"
      }
    },
    "op2": {
      "_datadump_original_op_names": {
        "type": "LIST_STRING",
        "value": ["orig3"]
      }
    }
  })";
}

TEST_F(Om2ModelExecutorUt, GetOpAttr_ValidOpAttrJson_ReturnsParsedMap) {
  // 创建包含op_attr.json的OM2文件
  const std::string om2_with_attr = PathUtils::Join({test_work_dir_, "om2_with_op_attr.om2"});
  const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime_attr"});
  const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
  const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});

  (void)PathUtils::RemoveDirectories(runtime_dir);
  ASSERT_EQ(CreateDir(runtime_dir), 0);
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
  WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

  const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
  const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
  RunCommandOrAssert(cmake_config_cmd);
  RunCommandOrAssert(cmake_build_cmd);
  ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

  ZipArchiveWriter zip_writer(om2_with_attr);
  ASSERT_TRUE(zip_writer.IsMemFileOpened());
  const auto manifest = MakeManifestJson();
  const auto model_meta = MakeModelMetaJson();
  const auto op_attr = MakeOpAttrJson();
  ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/debug/op_attr.json", op_attr.data(), op_attr.size(), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt", PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h", PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp", PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp", PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp", PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp", PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
  ASSERT_TRUE(zip_writer.SaveModelDataToFile());

  uint32_t model_buf_size = 0U;
  auto model_buf = GetBinDataFromFile(om2_with_attr, model_buf_size);
  ASSERT_NE(model_buf, nullptr);
  ASSERT_GT(model_buf_size, 0U);

  ModelDataHolder holder;
  holder.model_data.model_data = model_buf.get();
  holder.model_data.model_len = model_buf_size;
  holder.model_data.om_path = om2_with_attr;
  holder.buffer = std::move(model_buf);

  auto load_arg = MakeOm2LoadArg();
  ge::Status status;
  auto executor = gert::LoadOm2ExecutorFromData(holder.model_data, load_arg, status);
  ASSERT_EQ(status, SUCCESS);

  std::map<std::string, std::map<std::string, std::string>> op_attr_map;
  status = executor->GetOpAttr(op_attr_map);
  EXPECT_EQ(status, SUCCESS);

  // 验证map内容
  EXPECT_FALSE(op_attr_map.empty());
  EXPECT_TRUE(op_attr_map.find("test_op") != op_attr_map.end());
  EXPECT_TRUE(op_attr_map["test_op"].find("_datadump_original_op_names") != op_attr_map["test_op"].end());

  const std::string &value_str = op_attr_map["test_op"]["_datadump_original_op_names"];
  // value应该是JSON数组字符串格式
  EXPECT_EQ(value_str, "[\"original_op1\",\"original_op2\"]");
}

TEST_F(Om2ModelExecutorUt, GetOpAttr_EmptyOpAttrJson_ReturnsEmptyMap) {
  // 创建包含空op_attr.json的OM2文件
  const std::string om2_empty_attr = PathUtils::Join({test_work_dir_, "om2_empty_op_attr.om2"});
  const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime_empty_attr"});
  const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
  const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});

  (void)PathUtils::RemoveDirectories(runtime_dir);
  ASSERT_EQ(CreateDir(runtime_dir), 0);
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
  WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

  const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
  const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
  RunCommandOrAssert(cmake_config_cmd);
  RunCommandOrAssert(cmake_build_cmd);
  ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

  ZipArchiveWriter zip_writer(om2_empty_attr);
  ASSERT_TRUE(zip_writer.IsMemFileOpened());
  const auto manifest = MakeManifestJson();
  const auto model_meta = MakeModelMetaJson();
  const auto op_attr = MakeEmptyOpAttrJson();
  ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/debug/op_attr.json", op_attr.data(), op_attr.size(), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt", PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h", PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp", PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp", PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp", PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp", PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
  ASSERT_TRUE(zip_writer.SaveModelDataToFile());

  // 加载模型并验证GetOpAttr返回空map
  uint32_t model_buf_size = 0U;
  auto model_buf = GetBinDataFromFile(om2_empty_attr, model_buf_size);
  ASSERT_NE(model_buf, nullptr);
  ASSERT_GT(model_buf_size, 0U);

  ModelDataHolder holder;
  holder.model_data.model_data = model_buf.get();
  holder.model_data.model_len = model_buf_size;
  holder.model_data.om_path = om2_empty_attr;
  holder.buffer = std::move(model_buf);

  auto load_arg = MakeOm2LoadArg();
  ge::Status status;
  auto executor = gert::LoadOm2ExecutorFromData(holder.model_data, load_arg, status);
  ASSERT_EQ(status, SUCCESS);

  std::map<std::string, std::map<std::string, std::string>> op_attr_map;
  status = executor->GetOpAttr(op_attr_map);
  EXPECT_EQ(status, SUCCESS);
  EXPECT_TRUE(op_attr_map.empty());
}

TEST_F(Om2ModelExecutorUt, GetOpAttr_MissingOpAttrJson_ReturnsEmptyMap) {
  // 使用现有的om2_file_path_（不包含op_attr.json）
  PrepareOm2File();

  auto load_arg = MakeOm2LoadArg();
  ge::Status status;
  auto model_data_holder = LoadValidModelData();
  auto handle = gert::LoadOm2ExecutorFromData(model_data_holder.model_data, load_arg, status);
  ASSERT_EQ(status, SUCCESS);

  std::map<std::string, std::map<std::string, std::string>> op_attr_map;
  status = handle->GetOpAttr(op_attr_map);
  // 无op_attr.json时不报错，返回空map（fallback机制）
  EXPECT_EQ(status, SUCCESS);
  EXPECT_TRUE(op_attr_map.empty());
}

TEST_F(Om2ModelExecutorUt, GetOpAttr_InvalidOpAttrJson_ReturnsEmptyMap) {
  // 创建包含无效JSON的OM2文件
  const std::string om2_invalid_attr = PathUtils::Join({test_work_dir_, "om2_invalid_op_attr.om2"});
  const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime_invalid_attr"});
  const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
  const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});

  (void)PathUtils::RemoveDirectories(runtime_dir);
  ASSERT_EQ(CreateDir(runtime_dir), 0);
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
  WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

  const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
  const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
  RunCommandOrAssert(cmake_config_cmd);
  RunCommandOrAssert(cmake_build_cmd);
  ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

  ZipArchiveWriter zip_writer(om2_invalid_attr);
  ASSERT_TRUE(zip_writer.IsMemFileOpened());
  const auto manifest = MakeManifestJson();
  const auto model_meta = MakeModelMetaJson();
  const auto op_attr = MakeInvalidOpAttrJson();
  ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/debug/op_attr.json", op_attr.data(), op_attr.size(), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt", PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h", PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp", PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp", PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp", PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp", PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
  ASSERT_TRUE(zip_writer.SaveModelDataToFile());

  // 加载模型，无效JSON时fallback到空map
  uint32_t model_buf_size = 0U;
  auto model_buf = GetBinDataFromFile(om2_invalid_attr, model_buf_size);
  ASSERT_NE(model_buf, nullptr);
  ASSERT_GT(model_buf_size, 0U);

  ModelDataHolder holder;
  holder.model_data.model_data = model_buf.get();
  holder.model_data.model_len = model_buf_size;
  holder.model_data.om_path = om2_invalid_attr;
  holder.buffer = std::move(model_buf);

  auto load_arg = MakeOm2LoadArg();
  ge::Status status;
  auto handle = gert::LoadOm2ExecutorFromData(holder.model_data, load_arg, status);
  ASSERT_EQ(status, SUCCESS);

  std::map<std::string, std::map<std::string, std::string>> op_attr_map;
  status = handle->GetOpAttr(op_attr_map);
  EXPECT_EQ(status, SUCCESS);
  EXPECT_TRUE(op_attr_map.empty());
}

TEST_F(Om2ModelExecutorUt, ParseOpAttrJsonToMapInternal_MultipleAttrs_ParsesAllAttrs) {
  // 创建包含多个算子多个属性的OM2文件
  const std::string om2_multi_attr = PathUtils::Join({test_work_dir_, "om2_multi_op_attr.om2"});
  const std::string runtime_dir = PathUtils::Join({test_work_dir_, "fake_runtime_multi_attr"});
  const std::string build_dir = PathUtils::Join({runtime_dir, "build"});
  const std::string so_path = PathUtils::Join({runtime_dir, "libg1_om2.so"});

  (void)PathUtils::RemoveDirectories(runtime_dir);
  ASSERT_EQ(CreateDir(runtime_dir), 0);
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_interface.h"}), MakeInterfaceHeader());
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_resources.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), MakeEmptyCpp("g1_interface.h"));
  WriteTextFile(PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), MakeLoadAndRunCpp());
  WriteTextFile(PathUtils::Join({runtime_dir, "CMakeLists.txt"}), MakeCMakeLists());

  const std::string cmake_config_cmd = "cmake -S " + runtime_dir + " -B " + build_dir;
  const std::string cmake_build_cmd = "cmake --build " + build_dir + " -j1";
  RunCommandOrAssert(cmake_config_cmd);
  RunCommandOrAssert(cmake_build_cmd);
  ASSERT_EQ(mmAccess2(so_path.c_str(), M_F_OK), EOK);

  ZipArchiveWriter zip_writer(om2_multi_attr);
  ASSERT_TRUE(zip_writer.IsMemFileOpened());
  const auto manifest = MakeManifestJson();
  const auto model_meta = MakeModelMetaJson();
  const auto op_attr = MakeMultipleOpAttrJson();
  ASSERT_TRUE(zip_writer.WriteBytes("manifest.json", manifest.data(), manifest.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/model_meta.json", model_meta.data(), model_meta.size(), false));
  ASSERT_TRUE(zip_writer.WriteBytes("data/model_0/debug/op_attr.json", op_attr.data(), op_attr.size(), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/CMakeLists.txt", PathUtils::Join({runtime_dir, "CMakeLists.txt"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_interface.h", PathUtils::Join({runtime_dir, "g1_interface.h"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_resources.cpp", PathUtils::Join({runtime_dir, "g1_resources.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_kernel_reg.cpp", PathUtils::Join({runtime_dir, "g1_kernel_reg.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_args_manager.cpp", PathUtils::Join({runtime_dir, "g1_args_manager.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/g1_load_and_run.cpp", PathUtils::Join({runtime_dir, "g1_load_and_run.cpp"}), false));
  ASSERT_TRUE(zip_writer.WriteFile("data/model_0/runtime/libg1_om2.so", so_path, false));
  ASSERT_TRUE(zip_writer.SaveModelDataToFile());

  // 加载模型并验证所有属性都被解析
  uint32_t model_buf_size = 0U;
  auto model_buf = GetBinDataFromFile(om2_multi_attr, model_buf_size);
  ASSERT_NE(model_buf, nullptr);
  ASSERT_GT(model_buf_size, 0U);

  ModelDataHolder holder;
  holder.model_data.model_data = model_buf.get();
  holder.model_data.model_len = model_buf_size;
  holder.model_data.om_path = om2_multi_attr;
  holder.buffer = std::move(model_buf);

  auto load_arg = MakeOm2LoadArg();
  ge::Status status;
  auto handle = gert::LoadOm2ExecutorFromData(holder.model_data, load_arg, status);
  ASSERT_EQ(status, SUCCESS);

  std::map<std::string, std::map<std::string, std::string>> op_attr_map;
  status = handle->GetOpAttr(op_attr_map);
  EXPECT_EQ(status, SUCCESS);

  // 验证包含2个算子
  EXPECT_EQ(op_attr_map.size(), 2U);
  EXPECT_TRUE(op_attr_map.find("op1") != op_attr_map.end());
  EXPECT_TRUE(op_attr_map.find("op2") != op_attr_map.end());

  // op1有2个属性
  EXPECT_EQ(op_attr_map["op1"].size(), 2U);
  EXPECT_TRUE(op_attr_map["op1"].find("_datadump_original_op_names") != op_attr_map["op1"].end());
  EXPECT_EQ(op_attr_map["op1"]["_datadump_original_op_names"], "[\"orig1\",\"orig2\"]");

  // op2有1个属性
  EXPECT_EQ(op_attr_map["op2"].size(), 1U);
  EXPECT_EQ(op_attr_map["op2"]["_datadump_original_op_names"], "[\"orig3\"]");
}

TEST_F(Om2ModelExecutorUt, GetOpAttr_BeforeLoad_ReturnsError) {
  // 未加载模型前调用GetOpAttr应该返回错误
  auto executor = std::make_unique<gert::Om2ModelExecutor>();

  std::map<std::string, std::map<std::string, std::string>>op_attr_map;
  ge::Status status = executor->GetOpAttr(op_attr_map);
  // 未初始化，应该返回错误
  EXPECT_NE(status, SUCCESS);
  EXPECT_TRUE(op_attr_map.empty());
}
}  // namespace ge
