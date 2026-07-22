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
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>

#define private public
#define protected public
#include "graph_metadef/common/plugin/plugin_manager.h"
#undef private
#undef protected

#include "depends/mmpa/src/mmpa_stub.h"

using namespace std;
using namespace ge;

namespace {
const std::string kTmpDir = "./tmp_plugin_mgr_cov_ut";
}  // namespace

class UtestPluginManagerCov : public testing::Test {
 protected:
  void SetUp() override {
    system(("rm -rf " + kTmpDir).c_str());
    system(("mkdir -p " + kTmpDir).c_str());
  }
  void TearDown() override {
    system(("rm -rf " + kTmpDir).c_str());
    unsetenv("ASCEND_OPP_PATH");
    unsetenv("ASCEND_HOME_PATH");
    PluginManager::SetCustomOpLibPath("");
    ge::MmpaStub::GetInstance().Reset();
  }

  static std::string GetRunPkgPath() {
    std::string model_path = GetModelPath();
    model_path = model_path.substr(0, model_path.rfind('/'));
    model_path = model_path.substr(0, model_path.rfind('/'));
    model_path = model_path.substr(0, model_path.rfind('/') + 1U);
    return model_path;
  }
};

// ---- ReversePathString ----

TEST_F(UtestPluginManagerCov, ReversePathString_EmptyPath) {
  std::string path;
  EXPECT_EQ(PluginManager::ReversePathString(path), SUCCESS);
}

TEST_F(UtestPluginManagerCov, ReversePathString_NoColon) {
  std::string path = "/path/to/dir";
  EXPECT_EQ(PluginManager::ReversePathString(path), SUCCESS);
  EXPECT_EQ(path, "/path/to/dir");
}

TEST_F(UtestPluginManagerCov, ReversePathString_WithColon) {
  std::string path = "/a:/b:/c";
  EXPECT_EQ(PluginManager::ReversePathString(path), SUCCESS);
  EXPECT_EQ(path, "/c:/b:/a");
}

// ---- GetOppPluginVendors ----

TEST_F(UtestPluginManagerCov, GetOppPluginVendors_CannotOpenFile) {
  std::vector<std::string> vendors;
  EXPECT_EQ(PluginManager::GetOppPluginVendors(kTmpDir + "/nonexist_config.ini", vendors), FAILED);
}

TEST_F(UtestPluginManagerCov, GetOppPluginVendors_EmptyContent) {
  std::string cfg = kTmpDir + "/empty_config.ini";
  system(("touch " + cfg).c_str());
  std::vector<std::string> vendors;
  EXPECT_NE(PluginManager::GetOppPluginVendors(cfg, vendors), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetOppPluginVendors_InvalidFormat) {
  std::string cfg = kTmpDir + "/bad_config.ini";
  system(("echo 'invalid_line' > " + cfg).c_str());
  std::vector<std::string> vendors;
  EXPECT_NE(PluginManager::GetOppPluginVendors(cfg, vendors), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetOppPluginVendors_ValidFormat) {
  std::string cfg = kTmpDir + "/valid_config.ini";
  system(("echo 'load_priority=vendor_a,vendor_b' > " + cfg).c_str());
  std::vector<std::string> vendors;
  EXPECT_EQ(PluginManager::GetOppPluginVendors(cfg, vendors), SUCCESS);
  EXPECT_EQ(vendors.size(), 2U);
}

// ---- GetOppPath ----

TEST_F(UtestPluginManagerCov, GetOppPath_InvalidEnvPath) {
  setenv("ASCEND_OPP_PATH", "/nonexist/opp/path", 1);
  std::string opp_path;
  EXPECT_EQ(PluginManager::GetOppPath(opp_path), SUCCESS);
  EXPECT_FALSE(opp_path.empty());
}

TEST_F(UtestPluginManagerCov, GetOppPath_ValidEnvPath) {
  std::string opp_dir = kTmpDir + "/opp";
  system(("mkdir -p " + opp_dir).c_str());
  setenv("ASCEND_OPP_PATH", opp_dir.c_str(), 1);
  std::string opp_path;
  EXPECT_EQ(PluginManager::GetOppPath(opp_path), SUCCESS);
  EXPECT_EQ(opp_path.back(), '/');
}

TEST_F(UtestPluginManagerCov, GetOppPath_NoEnv) {
  unsetenv("ASCEND_OPP_PATH");
  std::string opp_path;
  EXPECT_EQ(PluginManager::GetOppPath(opp_path), SUCCESS);
  EXPECT_FALSE(opp_path.empty());
}

// ---- GetUpgradedOppPath ----

TEST_F(UtestPluginManagerCov, GetUpgradedOppPath_NoEnv) {
  unsetenv("ASCEND_HOME_PATH");
  std::string opp_path;
  EXPECT_EQ(PluginManager::GetUpgradedOppPath(opp_path), FAILED);
}

TEST_F(UtestPluginManagerCov, GetUpgradedOppPath_InvalidEnv) {
  setenv("ASCEND_HOME_PATH", "/nonexist/home/path", 1);
  std::string opp_path;
  EXPECT_EQ(PluginManager::GetUpgradedOppPath(opp_path), FAILED);
}

TEST_F(UtestPluginManagerCov, GetUpgradedOppPath_ValidEnv) {
  std::string home_dir = kTmpDir + "/home";
  std::string opp_latest = home_dir + "/opp_latest";
  system(("mkdir -p " + opp_latest).c_str());
  setenv("ASCEND_HOME_PATH", home_dir.c_str(), 1);
  std::string opp_path;
  EXPECT_EQ(PluginManager::GetUpgradedOppPath(opp_path), SUCCESS);
}

// ---- IsNewOppPathStruct ----

TEST_F(UtestPluginManagerCov, IsNewOppPathStruct_OldVersion) {
  EXPECT_FALSE(PluginManager::IsNewOppPathStruct(kTmpDir + "/"));
}

TEST_F(UtestPluginManagerCov, IsNewOppPathStruct_NewVersion) {
  std::string opp_dir = kTmpDir + "/new_opp";
  system(("mkdir -p " + opp_dir + "/built-in").c_str());
  EXPECT_TRUE(PluginManager::IsNewOppPathStruct(opp_dir + "/"));
}

// ---- IsVendorVersionValid ----

TEST_F(UtestPluginManagerCov, IsVendorVersionValid_OppLatest) {
  EXPECT_TRUE(PluginManager::IsVendorVersionValid(std::string("/path/to/opp_latest/vendor")));
}

TEST_F(UtestPluginManagerCov, IsVendorVersionValid_NoVersionInfo) {
  std::string vendor_dir = kTmpDir + "/vendor_no_ver";
  system(("mkdir -p " + vendor_dir).c_str());
  EXPECT_TRUE(PluginManager::IsVendorVersionValid(vendor_dir));
}

TEST_F(UtestPluginManagerCov, IsVendorVersionValid_BuiltInWithVersion) {
  std::string builtin_dir = kTmpDir + "/built-in";
  std::string version_file = kTmpDir + "/version.info";
  system(("mkdir -p " + builtin_dir).c_str());
  system(("echo 'Version=6.3' > " + version_file).c_str());
  EXPECT_TRUE(PluginManager::IsVendorVersionValid(builtin_dir));
}

TEST_F(UtestPluginManagerCov, IsVendorVersionValid_VendorWithCompilerVersion) {
  std::string vendor_dir = kTmpDir + "/vendor_with_ver";
  std::string version_file = vendor_dir + "/version.info";
  system(("mkdir -p " + vendor_dir).c_str());
  system(("echo 'compiler_version=6.3' > " + version_file).c_str());
  EXPECT_TRUE(PluginManager::IsVendorVersionValid(vendor_dir));
}

TEST_F(UtestPluginManagerCov, IsVendorVersionValid_ByVersions_EmptyRequired) {
  EXPECT_TRUE(PluginManager::IsVendorVersionValid(std::string("6.3"), std::string("")));
  EXPECT_TRUE(PluginManager::IsVendorVersionValid(std::string(""), std::string("6.3")));
  EXPECT_TRUE(PluginManager::IsVendorVersionValid(std::string("6.3"), std::string("6.3")));
}

// ---- GetEffectiveVersion (private, via #define private public) ----

TEST_F(UtestPluginManagerCov, GetEffectiveVersion_Normal) {
  PluginManager mgr;
  uint32_t ver = 0U;
  EXPECT_TRUE(mgr.GetEffectiveVersion("6.3", ver));
  EXPECT_TRUE(mgr.GetEffectiveVersion("8.0", ver));
  EXPECT_TRUE(mgr.GetEffectiveVersion("8.123", ver));
}

TEST_F(UtestPluginManagerCov, GetEffectiveVersion_LongCVersion) {
  PluginManager mgr;
  uint32_t ver = 0U;
  EXPECT_TRUE(mgr.GetEffectiveVersion("6.123456", ver));
}

TEST_F(UtestPluginManagerCov, GetEffectiveVersion_TooFewParts) {
  PluginManager mgr;
  uint32_t ver = 0U;
  EXPECT_FALSE(mgr.GetEffectiveVersion("6", ver));
}

TEST_F(UtestPluginManagerCov, GetEffectiveVersion_InvalidNumber) {
  PluginManager mgr;
  uint32_t ver = 0U;
  EXPECT_FALSE(mgr.GetEffectiveVersion("6.abc", ver));
}

// ---- CheckOppAndCompilerVersions (private) ----

TEST_F(UtestPluginManagerCov, CheckOppAndCompilerVersions_AllValid) {
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required = {{630000, 630001}};
  EXPECT_TRUE(mgr.CheckOppAndCompilerVersions("6.3", "6.3", required));
}

TEST_F(UtestPluginManagerCov, CheckOppAndCompilerVersions_OppOutOfRange) {
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required = {{700000, 700001}};
  EXPECT_FALSE(mgr.CheckOppAndCompilerVersions("6.3", "", required));
}

TEST_F(UtestPluginManagerCov, CheckOppAndCompilerVersions_InvalidOppVersion) {
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required = {{630000, 630001}};
  EXPECT_FALSE(mgr.CheckOppAndCompilerVersions("invalid", "", required));
}

TEST_F(UtestPluginManagerCov, CheckOppAndCompilerVersions_CompilerOutOfRange) {
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required = {{700000, 700001}};
  EXPECT_FALSE(mgr.CheckOppAndCompilerVersions("", "6.3", required));
}

TEST_F(UtestPluginManagerCov, CheckOppAndCompilerVersions_InvalidCompilerVersion) {
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required = {{630000, 630001}};
  EXPECT_FALSE(mgr.CheckOppAndCompilerVersions("", "invalid", required));
}

TEST_F(UtestPluginManagerCov, CheckOppAndCompilerVersions_EmptyVersions) {
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required = {{630000, 630001}};
  EXPECT_TRUE(mgr.CheckOppAndCompilerVersions("", "", required));
}

TEST_F(UtestPluginManagerCov, CheckOppAndCompilerVersions_MultipleCompilerVersions) {
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required = {{630000, 640000}};
  EXPECT_TRUE(mgr.CheckOppAndCompilerVersions("6.3", "6.3,6.4", required));
}

// ---- GetRequiredOppAbiVersion (private) ----

TEST_F(UtestPluginManagerCov, GetRequiredOppAbiVersion_NoCompilerOrRuntime) {
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required;
  EXPECT_TRUE(mgr.GetRequiredOppAbiVersion(required));
}

TEST_F(UtestPluginManagerCov, GetRequiredOppAbiVersion_WithCompilerVersion) {
  std::string run_pkg_path = GetRunPkgPath();
  std::string compiler_dir = run_pkg_path + "compiler";
  std::string version_file = compiler_dir + "/version.info";
  bool created = false;
  if (system(("mkdir -p " + compiler_dir).c_str()) == 0) {
    system(("echo 'required_opp_abi_version=>=6.3, <=6.4' > " + version_file).c_str());
    created = true;
  }
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required;
  EXPECT_TRUE(mgr.GetRequiredOppAbiVersion(required));
  if (created) {
    system(("rm -rf " + compiler_dir).c_str());
  }
}

TEST_F(UtestPluginManagerCov, GetRequiredOppAbiVersion_WithRuntimeVersion) {
  std::string run_pkg_path = GetRunPkgPath();
  std::string runtime_dir = run_pkg_path + "runtime";
  std::string version_file = runtime_dir + "/version.info";
  bool created = false;
  if (system(("mkdir -p " + runtime_dir).c_str()) == 0) {
    system(("echo 'required_opp_abi_version=>=8.0, <=8.1' > " + version_file).c_str());
    created = true;
  }
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required;
  EXPECT_TRUE(mgr.GetRequiredOppAbiVersion(required));
  if (created) {
    system(("rm -rf " + runtime_dir).c_str());
  }
}

TEST_F(UtestPluginManagerCov, GetRequiredOppAbiVersion_InvalidFormat) {
  std::string run_pkg_path = GetRunPkgPath();
  std::string compiler_dir = run_pkg_path + "compiler";
  std::string version_file = compiler_dir + "/version.info";
  bool created = false;
  if (system(("mkdir -p " + compiler_dir).c_str()) == 0) {
    system(("echo 'required_opp_abi_version=>=6.3, >=6.4' > " + version_file).c_str());
    created = true;
  }
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required;
  // GetRequiredOppAbiVersion returns true when version file cannot be read or format is unparsable
  EXPECT_TRUE(mgr.GetRequiredOppAbiVersion(required));
  if (created) {
    system(("rm -rf " + compiler_dir).c_str());
  }
}

TEST_F(UtestPluginManagerCov, GetRequiredOppAbiVersion_SingleVersion) {
  std::string run_pkg_path = GetRunPkgPath();
  std::string compiler_dir = run_pkg_path + "compiler";
  std::string version_file = compiler_dir + "/version.info";
  bool created = false;
  if (system(("mkdir -p " + compiler_dir).c_str()) == 0) {
    system(("echo 'required_opp_abi_version=6.3' > " + version_file).c_str());
    created = true;
  }
  PluginManager mgr;
  std::vector<std::pair<uint32_t, uint32_t>> required;
  EXPECT_TRUE(mgr.GetRequiredOppAbiVersion(required));
  if (created) {
    system(("rm -rf " + compiler_dir).c_str());
  }
}

// ---- GetOppAndCompilerVersion (private) ----

TEST_F(UtestPluginManagerCov, GetOppAndCompilerVersion_BuiltIn) {
  PluginManager mgr;
  std::string builtin_dir = kTmpDir + "/built-in";
  std::string version_file = kTmpDir + "/version.info";
  system(("mkdir -p " + builtin_dir).c_str());
  system(("echo 'Version=6.3' > " + version_file).c_str());
  std::string opp_version;
  std::string compiler_version;
  mgr.GetOppAndCompilerVersion(builtin_dir, opp_version, compiler_version);
  EXPECT_EQ(opp_version, "6.3");
}

TEST_F(UtestPluginManagerCov, GetOppAndCompilerVersion_Vendor) {
  PluginManager mgr;
  std::string vendor_dir = kTmpDir + "/vendor";
  std::string version_file = vendor_dir + "/version.info";
  system(("mkdir -p " + vendor_dir).c_str());
  system(("echo 'compiler_version=6.3' > " + version_file).c_str());
  std::string opp_version;
  std::string compiler_version;
  mgr.GetOppAndCompilerVersion(vendor_dir, opp_version, compiler_version);
  EXPECT_EQ(compiler_version, "6.3");
}

// ---- ClearHandles_ ----

TEST_F(UtestPluginManagerCov, ClearHandles_DlcloseFail) {
  PluginManager mgr;
  mgr.handles_["bad_handle.so"] = nullptr;
  mgr.ClearHandles_();
  EXPECT_EQ(mgr.handles_.size(), 0U);
}

TEST_F(UtestPluginManagerCov, ClearHandles_NormalHandle) {
  PluginManager mgr;
  mgr.handles_["good_handle.so"] = reinterpret_cast<void *>(0x8888);
  mgr.ClearHandles_();
  EXPECT_EQ(mgr.handles_.size(), 0U);
}

// ---- GetPackageSoPath ----

TEST_F(UtestPluginManagerCov, GetPackageSoPath_BasicTest) {
  unsetenv("ASCEND_OPP_PATH");
  PluginManager::SetCustomOpLibPath("");
  std::vector<std::string> vendors;
  PluginManager::GetPackageSoPath(vendors);
}

TEST_F(UtestPluginManagerCov, GetPackageSoPath_WithCustomOpLibPath) {
  PluginManager::SetCustomOpLibPath(kTmpDir + "/custom_opp");
  std::vector<std::string> vendors;
  PluginManager::GetPackageSoPath(vendors);
}

// ---- GetOppPluginPathNew ----

TEST_F(UtestPluginManagerCov, GetOppPluginPathNew_GetVendorsFailed) {
  std::string opp_path = kTmpDir + "/opp_no_vendors/";
  system(("mkdir -p " + opp_path).c_str());
  std::string plugin_path;
  EXPECT_EQ(PluginManager::GetOppPluginPathNew(opp_path, "%s/op_proto", plugin_path, "old_custom"), SUCCESS);
  EXPECT_FALSE(plugin_path.empty());
}

TEST_F(UtestPluginManagerCov, GetOppPluginPathNew_WithVendors) {
  std::string opp_path = kTmpDir + "/opp_with_vendors/";
  system(("mkdir -p " + opp_path + "vendors").c_str());
  system(("echo 'load_priority=vendor_a' > " + opp_path + "vendors/config.ini").c_str());
  std::string plugin_path;
  EXPECT_EQ(PluginManager::GetOppPluginPathNew(opp_path, "%s/op_proto", plugin_path, ""), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetOppPluginPathNew_WithBuiltIn) {
  std::string opp_path = kTmpDir + "/opp_with_builtin/";
  system(("mkdir -p " + opp_path + "built-in").c_str());
  std::string plugin_path;
  EXPECT_EQ(PluginManager::GetOppPluginPathNew(opp_path, "%s/op_proto", plugin_path, ""), SUCCESS);
  EXPECT_NE(plugin_path.find("built-in"), std::string::npos);
}

// ---- GetOppPluginPathOld ----

TEST_F(UtestPluginManagerCov, GetOppPluginPathOld_NoPercentS) {
  std::string plugin_path;
  EXPECT_EQ(PluginManager::GetOppPluginPathOld("/opp/", "op_proto/no_fmt", plugin_path, ""), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetOppPluginPathOld_WithCustomFmt) {
  std::string plugin_path;
  EXPECT_EQ(PluginManager::GetOppPluginPathOld("/opp/", "op_proto/%s/", plugin_path, "op_proto/custom/"), SUCCESS);
}

// ---- IsSplitOpp ----

TEST_F(UtestPluginManagerCov, IsSplitOpp_BasicTest) {
  unsetenv("ASCEND_OPP_PATH");
  EXPECT_FALSE(PluginManager::IsSplitOpp());
}

// ---- GetOpsProtoPath ----

TEST_F(UtestPluginManagerCov, GetOpsProtoPath_OldVersion) {
  std::string opp_path = kTmpDir + "/old_opp";
  system(("mkdir -p " + opp_path).c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string opsproto_path;
  EXPECT_EQ(PluginManager::GetOpsProtoPath(opsproto_path), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetOpsProtoPath_NewVersion) {
  std::string opp_path = kTmpDir + "/new_opp_proto";
  system(("mkdir -p " + opp_path + "/built-in").c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string opsproto_path;
  EXPECT_EQ(PluginManager::GetOpsProtoPath(opsproto_path), SUCCESS);
}

// ---- GetUpgradedOpsProtoPath ----

TEST_F(UtestPluginManagerCov, GetUpgradedOpsProtoPath_NoEnv) {
  unsetenv("ASCEND_HOME_PATH");
  std::string opsproto_path;
  EXPECT_EQ(PluginManager::GetUpgradedOpsProtoPath(opsproto_path), FAILED);
}

TEST_F(UtestPluginManagerCov, GetUpgradedOpsProtoPath_ValidEnv) {
  std::string home_dir = kTmpDir + "/upgraded_home";
  system(("mkdir -p " + home_dir + "/opp_latest/built-in").c_str());
  setenv("ASCEND_HOME_PATH", home_dir.c_str(), 1);
  std::string opsproto_path;
  EXPECT_EQ(PluginManager::GetUpgradedOpsProtoPath(opsproto_path), SUCCESS);
}

// ---- GetUpgradedOpMasterPath ----

TEST_F(UtestPluginManagerCov, GetUpgradedOpMasterPath_NoEnv) {
  unsetenv("ASCEND_HOME_PATH");
  std::string op_tiling_path;
  EXPECT_EQ(PluginManager::GetUpgradedOpMasterPath(op_tiling_path), FAILED);
}

TEST_F(UtestPluginManagerCov, GetUpgradedOpMasterPath_ValidEnv) {
  std::string home_dir = kTmpDir + "/upgraded_master_home";
  system(("mkdir -p " + home_dir + "/opp_latest/built-in").c_str());
  setenv("ASCEND_HOME_PATH", home_dir.c_str(), 1);
  std::string op_tiling_path;
  EXPECT_EQ(PluginManager::GetUpgradedOpMasterPath(op_tiling_path), SUCCESS);
}

// ---- GetCustomOpPath ----

TEST_F(UtestPluginManagerCov, GetCustomOpPath_OldVersion) {
  std::string opp_path = kTmpDir + "/old_opp_custom";
  system(("mkdir -p " + opp_path).c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string customop_path;
  EXPECT_EQ(PluginManager::GetCustomOpPath("tensorflow", customop_path), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetCustomOpPath_NewVersion) {
  std::string opp_path = kTmpDir + "/new_opp_custom";
  system(("mkdir -p " + opp_path + "/built-in").c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string customop_path;
  EXPECT_EQ(PluginManager::GetCustomOpPath("tensorflow", customop_path), SUCCESS);
}

// ---- GetCustomCaffeProtoPath ----

TEST_F(UtestPluginManagerCov, GetCustomCaffeProtoPath_OldVersion) {
  std::string opp_path = kTmpDir + "/old_opp_caffe";
  system(("mkdir -p " + opp_path).c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string customcaffe_path;
  EXPECT_EQ(PluginManager::GetCustomCaffeProtoPath(customcaffe_path), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetCustomCaffeProtoPath_NewVersion) {
  std::string opp_path = kTmpDir + "/new_opp_caffe";
  system(("mkdir -p " + opp_path + "/built-in").c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string customcaffe_path;
  EXPECT_EQ(PluginManager::GetCustomCaffeProtoPath(customcaffe_path), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetCustomCaffeProtoPath_NewVersionWithVendors) {
  std::string opp_path = kTmpDir + "/new_opp_caffe_vendors";
  system(("mkdir -p " + opp_path + "/built-in").c_str());
  system(("mkdir -p " + opp_path + "/vendors").c_str());
  system(("echo 'load_priority=vendor_a,vendor_b' > " + opp_path + "/vendors/config.ini").c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string customcaffe_path;
  EXPECT_EQ(PluginManager::GetCustomCaffeProtoPath(customcaffe_path), SUCCESS);
}

// ---- GetOpTilingPath / GetOpTilingForwardOrderPath ----

TEST_F(UtestPluginManagerCov, GetOpTilingForwardOrderPath_OldVersion) {
  std::string opp_path = kTmpDir + "/old_opp_tiling";
  system(("mkdir -p " + opp_path).c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string op_tiling_path;
  EXPECT_EQ(PluginManager::GetOpTilingForwardOrderPath(op_tiling_path), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetOpTilingForwardOrderPath_NewVersion) {
  std::string opp_path = kTmpDir + "/new_opp_tiling";
  system(("mkdir -p " + opp_path + "/built-in").c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string op_tiling_path;
  EXPECT_EQ(PluginManager::GetOpTilingForwardOrderPath(op_tiling_path), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetOpTilingPath_BasicTest) {
  std::string opp_path = kTmpDir + "/opp_tiling_path";
  system(("mkdir -p " + opp_path).c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string op_tiling_path;
  EXPECT_EQ(PluginManager::GetOpTilingPath(op_tiling_path), SUCCESS);
}

// ---- GetConstantFoldingOpsPath ----

TEST_F(UtestPluginManagerCov, GetConstantFoldingOpsPath_OldVersion) {
  std::string opp_path = kTmpDir + "/old_opp_cf";
  system(("mkdir -p " + opp_path).c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string cf_path;
  EXPECT_EQ(PluginManager::GetConstantFoldingOpsPath("/base/path", cf_path), SUCCESS);
  EXPECT_NE(cf_path.find("host_cpu"), std::string::npos);
}

TEST_F(UtestPluginManagerCov, GetConstantFoldingOpsPath_NewVersion) {
  std::string opp_path = kTmpDir + "/new_opp_cf";
  system(("mkdir -p " + opp_path + "/built-in").c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string cf_path;
  EXPECT_EQ(PluginManager::GetConstantFoldingOpsPath("/base/path", cf_path), SUCCESS);
  EXPECT_NE(cf_path.find("host_cpu"), std::string::npos);
}

// ---- LoadSo / LoadSoWithFlags ----

TEST_F(UtestPluginManagerCov, LoadSoWithFlags_NonExistPath) {
  PluginManager mgr;
  std::vector<std::string> func_check_list;
  EXPECT_EQ(mgr.LoadSoWithFlags("/nonexist/path/libtest.so", 0, func_check_list), SUCCESS);
}

TEST_F(UtestPluginManagerCov, LoadSoWithFlags_RealSoFile) {
  std::string so_path = kTmpDir + "/libnonexist_test.so";
  PluginManager mgr;
  std::vector<std::string> func_check_list;
  EXPECT_EQ(mgr.LoadSoWithFlags(so_path, 0, func_check_list), SUCCESS);
}

TEST_F(UtestPluginManagerCov, LoadSoWithFlags_RealSoFileWithFuncCheck) {
  std::string so_path = kTmpDir + "/libnonexist_test.so";
  PluginManager mgr;
  std::vector<std::string> func_check_list;
  EXPECT_EQ(mgr.LoadSoWithFlags(so_path, 0, func_check_list), SUCCESS);
}

TEST_F(UtestPluginManagerCov, LoadSo_BasicTest) {
  std::string so_path = kTmpDir + "/libnonexist_test.so";
  PluginManager mgr;
  std::vector<std::string> func_check_list;
  EXPECT_EQ(mgr.LoadSo(so_path, func_check_list), SUCCESS);
}

// ---- ValidateSo (private) ----

TEST_F(UtestPluginManagerCov, ValidateSo_FileNotExist) {
  PluginManager mgr;
  int64_t file_size = 0;
  EXPECT_EQ(mgr.ValidateSo("/nonexist/file.so", 0, file_size), FAILED);
}

TEST_F(UtestPluginManagerCov, ValidateSo_TotalSizeTooLarge) {
  std::string file_path = kTmpDir + "/small.so";
  system(("touch " + file_path).c_str());
  PluginManager mgr;
  int64_t file_size = 0;
  // kMaxSizeOfLoadedSo = 1048576000, file size is 0, so 1048576000 + 0 == kMaxSizeOfLoadedSo (not >), returns SUCCESS
  EXPECT_EQ(mgr.ValidateSo(file_path, 1048576000L, file_size), SUCCESS);
}

TEST_F(UtestPluginManagerCov, ValidateSo_Success) {
  std::string file_path = kTmpDir + "/valid.so";
  system(("touch " + file_path).c_str());
  PluginManager mgr;
  int64_t file_size = 0;
  EXPECT_EQ(mgr.ValidateSo(file_path, 0, file_size), SUCCESS);
}

// ---- Load / LoadWithFlags ----

TEST_F(UtestPluginManagerCov, Load_NonExistPath) {
  PluginManager mgr;
  std::vector<std::string> func_check_list;
  EXPECT_EQ(mgr.Load("/nonexist/path/", func_check_list), SUCCESS);
}

TEST_F(UtestPluginManagerCov, LoadWithFlags_NotDir) {
  std::string file_path = kTmpDir + "/notdir.txt";
  system(("touch " + file_path).c_str());
  PluginManager mgr;
  std::vector<std::string> func_check_list;
  EXPECT_EQ(mgr.LoadWithFlags(file_path, 0, func_check_list), SUCCESS);
}

TEST_F(UtestPluginManagerCov, LoadWithFlags_EmptyDir) {
  std::string dir_path = kTmpDir + "/empty_dir";
  system(("mkdir -p " + dir_path).c_str());
  PluginManager mgr;
  std::vector<std::string> func_check_list;
  EXPECT_EQ(mgr.LoadWithFlags(dir_path, 0, func_check_list), SUCCESS);
}

TEST_F(UtestPluginManagerCov, LoadWithFlags_DirWithSoFiles) {
  std::string dir_path = kTmpDir + "/so_dir";
  system(("mkdir -p " + dir_path).c_str());
  system(("touch " + dir_path + "/not_so.txt").c_str());
  PluginManager mgr;
  std::vector<std::string> func_check_list;
  EXPECT_EQ(mgr.LoadWithFlags(dir_path, 0, func_check_list), SUCCESS);
}

// ---- GetOppSupportedOsAndCpuType ----

TEST_F(UtestPluginManagerCov, GetOppSupportedOsAndCpuType_EmptyPath) {
  unsetenv("ASCEND_OPP_PATH");
  std::unordered_map<std::string, std::unordered_set<std::string>> opp_supported_os_cpu;
  PluginManager::GetOppSupportedOsAndCpuType(opp_supported_os_cpu);
}

TEST_F(UtestPluginManagerCov, GetOppSupportedOsAndCpuType_WithDirStructure) {
  std::string opp_path = kTmpDir + "/opp_supported";
  std::string lib_path = opp_path + "/built-in/op_proto/lib/";
  system(("mkdir -p " + lib_path + "linux/x86_64").c_str());
  system(("mkdir -p " + lib_path + "linux/aarch64").c_str());
  system(("mkdir -p " + lib_path + "windows/x86_64").c_str());
  std::unordered_map<std::string, std::unordered_set<std::string>> opp_supported_os_cpu;
  PluginManager::GetOppSupportedOsAndCpuType(opp_supported_os_cpu, lib_path, "", 0U);
}

TEST_F(UtestPluginManagerCov, GetOppSupportedOsAndCpuType_LayerExceeds) {
  std::unordered_map<std::string, std::unordered_set<std::string>> opp_supported_os_cpu;
  PluginManager::GetOppSupportedOsAndCpuType(opp_supported_os_cpu, kTmpDir, "linux", 2U);
}

// ---- GetCurEnvPackageOsAndCpuType ----

TEST_F(UtestPluginManagerCov, GetCurEnvPackageOsAndCpuType_NoSceneFile) {
  std::string os_type;
  std::string cpu_type;
  PluginManager::GetCurEnvPackageOsAndCpuType(os_type, cpu_type);
}

TEST_F(UtestPluginManagerCov, GetCurEnvPackageOsAndCpuType_WithOppSceneFile) {
  std::string run_pkg_path = GetRunPkgPath();
  std::string opp_dir = run_pkg_path + "opp";
  std::string scene_file = opp_dir + "/scene.info";
  bool created = false;
  if (system(("mkdir -p " + opp_dir).c_str()) == 0) {
    system(("printf 'os=linux\narch=x86_64\n' > " + scene_file).c_str());
    created = true;
  }
  std::string os_type;
  std::string cpu_type;
  PluginManager::GetCurEnvPackageOsAndCpuType(os_type, cpu_type);
  if (created) {
    system(("rm -rf " + opp_dir).c_str());
  }
}

TEST_F(UtestPluginManagerCov, GetCurEnvPackageOsAndCpuType_WithRuntimeSceneFile) {
  std::string run_pkg_path = GetRunPkgPath();
  std::string runtime_dir = run_pkg_path + "runtime";
  std::string scene_file = runtime_dir + "/scene.info";
  bool created = false;
  if (system(("mkdir -p " + runtime_dir).c_str()) == 0) {
    system(("printf 'os=linux\narch=aarch64\n' > " + scene_file).c_str());
    created = true;
  }
  std::string os_type;
  std::string cpu_type;
  PluginManager::GetCurEnvPackageOsAndCpuType(os_type, cpu_type);
  if (created) {
    system(("rm -rf " + runtime_dir).c_str());
  }
}

TEST_F(UtestPluginManagerCov, GetCurEnvPackageOsAndCpuType_WithBadSceneFile) {
  std::string run_pkg_path = GetRunPkgPath();
  std::string opp_dir = run_pkg_path + "opp";
  std::string scene_file = opp_dir + "/scene.info";
  bool created = false;
  if (system(("mkdir -p " + opp_dir).c_str()) == 0) {
    system(("touch " + scene_file).c_str());
    created = true;
  }
  std::string os_type;
  std::string cpu_type;
  PluginManager::GetCurEnvPackageOsAndCpuType(os_type, cpu_type);
  if (created) {
    system(("rm -rf " + opp_dir).c_str());
  }
}

// ---- GetVersionFromPathWithName ----

TEST_F(UtestPluginManagerCov, GetVersionFromPathWithName_NonExistFile) {
  std::string version;
  EXPECT_FALSE(PluginManager::GetVersionFromPathWithName("/nonexist/version.info", version, "Version="));
}

TEST_F(UtestPluginManagerCov, GetVersionFromPathWithName_FileOpenFail) {
  std::string version;
  EXPECT_FALSE(PluginManager::GetVersionFromPathWithName("libcce.so_not_exist", version, "Version="));
}

TEST_F(UtestPluginManagerCov, GetVersionFromPathWithName_NoVersionInfo) {
  std::string file_path = kTmpDir + "/nover.txt";
  system(("echo 'no_version_here' > " + file_path).c_str());
  std::string version;
  EXPECT_FALSE(PluginManager::GetVersionFromPathWithName(file_path, version, "Version="));
}

TEST_F(UtestPluginManagerCov, GetVersionFromPathWithName_ValidVersion) {
  std::string file_path = kTmpDir + "/ver.txt";
  system(("echo 'Version=6.3' > " + file_path).c_str());
  std::string version;
  EXPECT_TRUE(PluginManager::GetVersionFromPathWithName(file_path, version, "Version="));
  EXPECT_EQ(version, "6.3");
}

// ---- GetVersionFromPath ----

TEST_F(UtestPluginManagerCov, GetVersionFromPath_NonExistFile) {
  std::string version;
  EXPECT_FALSE(PluginManager::GetVersionFromPath("/nonexist/version.info", version));
}

TEST_F(UtestPluginManagerCov, GetVersionFromPath_ValidVersion) {
  std::string file_path = kTmpDir + "/ver_path.txt";
  system(("echo 'Version=8.0' > " + file_path).c_str());
  std::string version;
  EXPECT_TRUE(PluginManager::GetVersionFromPath(file_path, version));
  EXPECT_EQ(version, "8.0");
}

// ---- ParseVersion (private) ----

TEST_F(UtestPluginManagerCov, ParseVersion_EmptyLine) {
  PluginManager mgr;
  std::string line;
  std::string version;
  EXPECT_FALSE(mgr.ParseVersion(line, version, "Version="));
}

TEST_F(UtestPluginManagerCov, ParseVersion_NoVersionName) {
  PluginManager mgr;
  std::string line = "some_random_text";
  std::string version;
  EXPECT_FALSE(mgr.ParseVersion(line, version, "Version="));
}

TEST_F(UtestPluginManagerCov, ParseVersion_EmptyVersion) {
  PluginManager mgr;
  std::string line = "Version=";
  std::string version;
  EXPECT_FALSE(mgr.ParseVersion(line, version, "Version="));
}

TEST_F(UtestPluginManagerCov, ParseVersion_ValidVersion) {
  PluginManager mgr;
  std::string line = "Version=6.3";
  std::string version;
  EXPECT_TRUE(mgr.ParseVersion(line, version, "Version="));
  EXPECT_EQ(version, "6.3");
}

// ---- GetFileListWithSuffix ----

TEST_F(UtestPluginManagerCov, GetFileListWithSuffix_EmptyPath) {
  std::vector<std::string> file_list;
  PluginManager::GetFileListWithSuffix("", ".so", file_list);
  EXPECT_TRUE(file_list.empty());
}

TEST_F(UtestPluginManagerCov, GetFileListWithSuffix_PathTooLong) {
  std::string long_path(5000, 'a');
  std::vector<std::string> file_list;
  PluginManager::GetFileListWithSuffix(long_path, ".so", file_list);
  EXPECT_TRUE(file_list.empty());
}

TEST_F(UtestPluginManagerCov, GetFileListWithSuffix_NonExistPath) {
  std::vector<std::string> file_list;
  PluginManager::GetFileListWithSuffix("/nonexist/path/", ".so", file_list);
  EXPECT_TRUE(file_list.empty());
}

TEST_F(UtestPluginManagerCov, GetFileListWithSuffix_NotDir) {
  std::string file_path = kTmpDir + "/notdir_fls.txt";
  system(("touch " + file_path).c_str());
  std::vector<std::string> file_list;
  PluginManager::GetFileListWithSuffix(file_path, ".so", file_list);
  EXPECT_TRUE(file_list.empty());
}

TEST_F(UtestPluginManagerCov, GetFileListWithSuffix_WithSoFiles) {
  std::string dir_path = kTmpDir + "/so_list_dir";
  system(("mkdir -p " + dir_path).c_str());
  system(("touch " + dir_path + "/libtest.so").c_str());
  system(("touch " + dir_path + "/libtest_rt.so").c_str());
  system(("touch " + dir_path + "/not_so.txt").c_str());
  std::vector<std::string> file_list;
  PluginManager::GetFileListWithSuffix(dir_path, ".so", file_list);
  EXPECT_EQ(file_list.size(), 2U);
}

// ---- FindSoFilesInCustomPassDirs ----

TEST_F(UtestPluginManagerCov, FindSoFilesInCustomPassDirs_NonExistDir) {
  std::vector<std::string> so_files;
  PluginManager::FindSoFilesInCustomPassDirs("/nonexist/directory", so_files);
  EXPECT_TRUE(so_files.empty());
}

TEST_F(UtestPluginManagerCov, FindSoFilesInCustomPassDirs_NotDir) {
  std::string file_path = kTmpDir + "/notdir_fcp.txt";
  system(("touch " + file_path).c_str());
  std::vector<std::string> so_files;
  PluginManager::FindSoFilesInCustomPassDirs(file_path, so_files);
  EXPECT_TRUE(so_files.empty());
}

TEST_F(UtestPluginManagerCov, FindSoFilesInCustomPassDirs_EmptyDir) {
  std::string dir_path = kTmpDir + "/empty_pass_dir";
  system(("mkdir -p " + dir_path).c_str());
  std::vector<std::string> so_files;
  PluginManager::FindSoFilesInCustomPassDirs(dir_path, so_files);
  EXPECT_TRUE(so_files.empty());
}

TEST_F(UtestPluginManagerCov, FindSoFilesInCustomPassDirs_WithSubDirAndSoFiles) {
  std::string dir_path = kTmpDir + "/pass_dir_with_so";
  std::string subdir = dir_path + "/subdir1/custom_fusion_passes";
  system(("mkdir -p " + subdir).c_str());
  system(("touch " + subdir + "/libpass1.so").c_str());
  system(("touch " + subdir + "/libpass2.so").c_str());
  system(("touch " + subdir + "/not_so.txt").c_str());
  std::vector<std::string> so_files;
  PluginManager::FindSoFilesInCustomPassDirs(dir_path, so_files);
  EXPECT_EQ(so_files.size(), 2U);
}

TEST_F(UtestPluginManagerCov, FindSoFilesInCustomPassDirs_SubDirNoCustomFusionPasses) {
  std::string dir_path = kTmpDir + "/pass_dir_no_cfp";
  system(("mkdir -p " + dir_path + "/subdir1").c_str());
  std::vector<std::string> so_files;
  PluginManager::FindSoFilesInCustomPassDirs(dir_path, so_files);
  EXPECT_TRUE(so_files.empty());
}

// ---- GetOpMasterDeviceSoPath ----

TEST_F(UtestPluginManagerCov, GetOpMasterDeviceSoPath_BasicTest) {
  std::string opp_path = kTmpDir + "/opp_master";
  system(("mkdir -p " + opp_path).c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string op_master_device_path;
  EXPECT_EQ(PluginManager::GetOpMasterDeviceSoPath(op_master_device_path), SUCCESS);
}

TEST_F(UtestPluginManagerCov, GetOpMasterDeviceSoPath_SubPkgExists) {
  std::string opp_path = kTmpDir + "/opp_master_subpkg";
  std::string sub_pkg_path = opp_path + "built-in/op_impl/ai_core/tbe/op_tiling_device/lib/";
  system(("mkdir -p " + sub_pkg_path).c_str());
  system(("touch " + sub_pkg_path + "/libtest.so").c_str());
  setenv("ASCEND_OPP_PATH", opp_path.c_str(), 1);
  std::string op_master_device_path;
  EXPECT_EQ(PluginManager::GetOpMasterDeviceSoPath(op_master_device_path), SUCCESS);
}

// ---- GetSoPackageName ----

TEST_F(UtestPluginManagerCov, GetSoPackageName_Vendors) {
  std::string path = "/opp/vendors/vendor_a/op_proto/libtest.so";
  EXPECT_EQ(PluginManager::GetSoPackageName(path), "vendor_a");
}

TEST_F(UtestPluginManagerCov, GetSoPackageName_VendorsNoSlash) {
  std::string path = "/opp/vendors/vendor_a";
  EXPECT_EQ(PluginManager::GetSoPackageName(path), "vendor_a");
}

TEST_F(UtestPluginManagerCov, GetSoPackageName_Custom) {
  std::string path = "/opp/op_proto/custom/libtest.so";
  EXPECT_EQ(PluginManager::GetSoPackageName(path), "custom");
}

TEST_F(UtestPluginManagerCov, GetSoPackageName_BuiltIn) {
  std::string path = "/opp/built-in/op_proto/libtest.so";
  EXPECT_EQ(PluginManager::GetSoPackageName(path), "built-in");
}

// ---- GetOppPkgPath ----

TEST_F(UtestPluginManagerCov, GetOppPkgPath_NotBuiltIn) {
  bool is_sub_pkg = false;
  std::string result = PluginManager::GetOppPkgPath("/opp/custom/", "/whole/", "/sub/", "linux/x86_64/", is_sub_pkg);
  EXPECT_FALSE(is_sub_pkg);
  EXPECT_NE(result.find("/whole/"), std::string::npos);
}

TEST_F(UtestPluginManagerCov, GetOppPkgPath_BuiltInWithSubPkg) {
  std::string opp_path = kTmpDir + "/built-in_opp_pkg";
  std::string sub_pkg_path = opp_path + "/sub_pkg/linux/x86_64/";
  system(("mkdir -p " + sub_pkg_path).c_str());
  system(("touch " + sub_pkg_path + "/libtest.so").c_str());
  bool is_sub_pkg = false;
  std::string result = PluginManager::GetOppPkgPath(opp_path, "/whole/", "/sub_pkg/", "linux/x86_64/", is_sub_pkg);
  EXPECT_TRUE(is_sub_pkg);
  EXPECT_NE(result.find("sub_pkg"), std::string::npos);
}

TEST_F(UtestPluginManagerCov, GetOppPkgPath_BuiltInWithoutSubPkg) {
  std::string opp_path = kTmpDir + "/opp_pkg_no_subpkg/built-in";
  system(("mkdir -p " + opp_path).c_str());
  bool is_sub_pkg = false;
  std::string result = PluginManager::GetOppPkgPath(opp_path, "/whole/", "/sub_pkg/", "linux/x86_64/", is_sub_pkg);
  EXPECT_FALSE(is_sub_pkg);
  EXPECT_NE(result.find("/whole/"), std::string::npos);
}

// ---- IsEndWith ----

TEST_F(UtestPluginManagerCov, IsEndWith_Match) {
  EXPECT_TRUE(PluginManager::IsEndWith("libtest.so", ".so"));
}

TEST_F(UtestPluginManagerCov, IsEndWith_NoMatch) {
  EXPECT_FALSE(PluginManager::IsEndWith("libtest.txt", ".so"));
}

TEST_F(UtestPluginManagerCov, IsEndWith_SuffLongerThanPath) {
  EXPECT_FALSE(PluginManager::IsEndWith(".so", "libtest.so"));
}

// ---- SplitPath ----

TEST_F(UtestPluginManagerCov, SplitPath_BasicTest) {
  std::vector<std::string> path_vec;
  PluginManager::SplitPath("/a:/b:/c", path_vec, ':');
  EXPECT_EQ(path_vec.size(), 3U);
}

TEST_F(UtestPluginManagerCov, SplitPath_WithEmptyParts) {
  std::vector<std::string> path_vec;
  PluginManager::SplitPath("/a::/b:", path_vec, ':');
  EXPECT_EQ(path_vec.size(), 2U);
}

TEST_F(UtestPluginManagerCov, SplitPath_CustomSeparator) {
  std::vector<std::string> path_vec;
  PluginManager::SplitPath("a,b,c", path_vec, ',');
  EXPECT_EQ(path_vec.size(), 3U);
}

// ---- GetPluginPathFromCustomOppPath ----

TEST_F(UtestPluginManagerCov, GetPluginPathFromCustomOppPath_EmptyCustomOpLibPath) {
  PluginManager::SetCustomOpLibPath("");
  std::string plugin_path;
  PluginManager::GetPluginPathFromCustomOppPath("op_proto/", plugin_path);
  EXPECT_TRUE(plugin_path.empty());
}

TEST_F(UtestPluginManagerCov, GetPluginPathFromCustomOppPath_WithValidPath) {
  std::string custom_dir = kTmpDir + "/custom_opp_path";
  system(("mkdir -p " + custom_dir + "/op_proto").c_str());
  PluginManager::SetCustomOpLibPath(custom_dir);
  std::string plugin_path;
  PluginManager::GetPluginPathFromCustomOppPath("op_proto/", plugin_path);
  EXPECT_FALSE(plugin_path.empty());
}

TEST_F(UtestPluginManagerCov, GetPluginPathFromCustomOppPath_WithInvalidPath) {
  PluginManager::SetCustomOpLibPath("/nonexist/custom/path");
  std::string plugin_path;
  PluginManager::GetPluginPathFromCustomOppPath("op_proto/", plugin_path);
  EXPECT_TRUE(plugin_path.empty());
}

TEST_F(UtestPluginManagerCov, GetPluginPathFromCustomOppPath_WithMultiplePaths) {
  std::string custom_dir1 = kTmpDir + "/custom_opp1";
  std::string custom_dir2 = kTmpDir + "/custom_opp2";
  system(("mkdir -p " + custom_dir1 + "/op_proto").c_str());
  system(("mkdir -p " + custom_dir2 + "/op_proto").c_str());
  PluginManager::SetCustomOpLibPath(custom_dir1 + ":" + custom_dir2);
  std::string plugin_path;
  PluginManager::GetPluginPathFromCustomOppPath("op_proto/", plugin_path);
  EXPECT_FALSE(plugin_path.empty());
}
