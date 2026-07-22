/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "macro_utils/dt_public_scope.h"
#include "common/plugin/ge_make_unique_util.h"
#include "common/plugin/op_tiling_manager.h"
#include "common/plugin/tbe_plugin_manager.h"
#include "common/plugin/runtime_plugin_loader.h"

using namespace testing;
using namespace std;

namespace ge {
namespace {
const char *const kEnvName = "ASCEND_OPP_PATH";

class OppPathGuard {
 public:
  explicit OppPathGuard(const std::string &new_path) {
    const char *val = getenv(kEnvName);
    if (val != nullptr) {
      old_value_ = val;
      has_old_ = true;
    }
    setenv(kEnvName, new_path.c_str(), 1);
  }
  ~OppPathGuard() {
    if (has_old_) {
      setenv(kEnvName, old_value_.c_str(), 1);
    } else {
      unsetenv(kEnvName);
    }
  }

 private:
  std::string old_value_;
  bool has_old_ = false;
};
}  // namespace
class UtestPluginManager : public testing::Test {
 public:
  OpTilingManager tiling_manager;

 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(UtestPluginManager, op_tiling_manager_clear_handles) {
  auto test_so_name = "test_fail.so";
  tiling_manager.handles_[test_so_name] = nullptr;
  tiling_manager.ClearHandles();
  ASSERT_EQ(tiling_manager.handles_.size(), 0);
  string env_value = "test";
  setenv(kEnvName, env_value.c_str(), 0);
  const char *env = getenv(kEnvName);
  ASSERT_NE(env, nullptr);
}

TEST_F(UtestPluginManager, test_tbe_plugin_manager) {
  TBEPluginManager manager;
  manager.handles_vec_ = {nullptr};
  manager.ClearHandles_();
  string env_value = "test";
  setenv(kEnvName, env_value.c_str(), 0);
  const char *env = getenv(kEnvName);
  ASSERT_NE(env, nullptr);
}

TEST_F(UtestPluginManager, test_load_rt_plugin) {
  const std::string path = "./";
  ge::graphStatus ret = RuntimePluginLoader::GetInstance().Initialize(path);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
}

TEST_F(UtestPluginManager, test_load_plugin_so_with_framework_type) {
  std::map<std::string, std::string> options;
  options["ge.frameworkType"] = "3";
  TBEPluginManager::Instance().LoadPluginSo(options);
  TBEPluginManager::Instance().Finalize();
}

TEST_F(UtestPluginManager, test_init_preparation_with_framework_type) {
  std::map<std::string, std::string> options;
  options["ge.frameworkType"] = "0";
  TBEPluginManager::InitPreparation(options);
  TBEPluginManager::Instance().Finalize();
}

TEST_F(UtestPluginManager, test_find_parser_used_so_max_recursive_depth) {
  std::vector<std::string> file_list;
  std::string caffe_parser_path;
  TBEPluginManager::FindParserUsedSo("/tmp", file_list, caffe_parser_path, 20U);
  ASSERT_TRUE(file_list.empty());
}

TEST_F(UtestPluginManager, test_load_plugin_so_dlopen_fail) {
  const std::string temp_dir = "/tmp/tbe_plugin_dlopen_test_" + std::to_string(getpid());
  const std::string so_dir = temp_dir + "/framework/built-in/tensorflow";
  system(("rm -rf " + temp_dir).c_str());
  system(("mkdir -p " + so_dir).c_str());
  std::ofstream ofs(so_dir + "/invalid.so");
  ofs << "not a valid shared library";
  ofs.close();

  OppPathGuard guard(temp_dir);
  std::map<std::string, std::string> options;
  options["ge.frameworkType"] = "3";
  TBEPluginManager::Instance().LoadPluginSo(options);
  TBEPluginManager::Instance().Finalize();

  system(("rm -rf " + temp_dir).c_str());
}
}  // namespace ge
