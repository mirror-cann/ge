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
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "common/python_runtime/python_artifact_utils.h"
#include "common/python_runtime/python_bridge_loader_utils.h"
#include "compiler/graph/fusion/pass/python_pass_bridge_loader_helper.h"

namespace ge {
namespace fusion {
namespace {
namespace artifact = ::ge::python_artifact;
namespace bridge_loader = ::ge::python_bridge_loader;
namespace loader_helper = python_pass_bridge_loader;
constexpr const char *kPythonPassArtifactsRelativePath = "passes/python_pass_artifacts";

class ScopedTempTree {
 public:
  ScopedTempTree() {
    char dir_template[] = "/tmp/ge_python_runtime_artifact_utils_ut_XXXXXX";
    const auto *created_dir = mkdtemp(dir_template);
    root_ = (created_dir == nullptr) ? std::string() : std::string(created_dir);
  }

  ~ScopedTempTree() {
    for (auto file_iter = files_.rbegin(); file_iter != files_.rend(); ++file_iter) {
      (void)remove(file_iter->c_str());
    }
    for (auto dir_iter = dirs_.rbegin(); dir_iter != dirs_.rend(); ++dir_iter) {
      (void)rmdir(dir_iter->c_str());
    }
    if (!root_.empty()) {
      (void)rmdir(root_.c_str());
    }
  }

  const std::string &Root() const {
    return root_;
  }

  std::string Path(const std::string &relative_path) const {
    return artifact::JoinPath(root_, relative_path);
  }

  std::string MakeDir(const std::string &relative_path) {
    std::string current = root_;
    size_t start = 0U;
    while (start < relative_path.size()) {
      const auto end = relative_path.find('/', start);
      const auto part = relative_path.substr(start, end == std::string::npos ? end : end - start);
      if (!part.empty()) {
        current = artifact::JoinPath(current, part);
        if (mkdir(current.c_str(), 0700) == 0) {
          dirs_.push_back(current);
        }
      }
      if (end == std::string::npos) {
        break;
      }
      start = end + 1U;
    }
    return current;
  }

  std::string WriteFile(const std::string &relative_path, const std::string &content) {
    const auto dir = artifact::DirName(relative_path);
    if ((dir != ".") && (dir != relative_path)) {
      (void)MakeDir(dir);
    }
    const auto file_path = Path(relative_path);
    std::ofstream output(file_path);
    output << content;
    files_.push_back(file_path);
    return file_path;
  }

 private:
  std::string root_;
  std::vector<std::string> dirs_;
  std::vector<std::string> files_;
};

class ScopedEnvVar {
 public:
  ScopedEnvVar(const char *name, const std::string &value) : name_(name) {
    const char *old_value = std::getenv(name_.c_str());
    if (old_value != nullptr) {
      has_old_value_ = true;
      old_value_ = old_value;
    }
    (void)setenv(name_.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvVar() {
    if (has_old_value_) {
      (void)setenv(name_.c_str(), old_value_.c_str(), 1);
    } else {
      (void)unsetenv(name_.c_str());
    }
  }

 private:
  std::string name_;
  bool has_old_value_{false};
  std::string old_value_;
};

std::string MakeManifest(const std::string &python_tag, const std::string &platform_tag, const uint32_t bridge_abi) {
  return "{\n"
         "  \"python_tag\": \"" +
         python_tag +
         "\",\n"
         "  \"platform\": \"" +
         platform_tag +
         "\",\n"
         "  \"bridge_abi\": " +
         std::to_string(bridge_abi) +
         ",\n"
         "  \"artifacts\": {\n"
         "    \"bridge\": \"libge_python_pass_bridge.so\",\n"
         "    \"native\": \"_ge_pass_native.so\"\n"
         "  }\n"
         "}\n";
}

void PrepareArtifactSet(ScopedTempTree &tree, const std::string &artifact_dir, const std::string &python_tag,
                        const uint32_t bridge_abi) {
  const auto platform_tag = artifact::CurrentPlatformTag();
  tree.WriteFile(artifact::JoinPath(artifact_dir, "libge_python_pass_bridge.so"), "bridge");
  tree.WriteFile(artifact::JoinPath(artifact_dir, "_ge_pass_native.so"), "native");
  tree.WriteFile(artifact::JoinPath(artifact_dir, "manifest.json"), MakeManifest(python_tag, platform_tag, bridge_abi));
}

struct FakeBridgeLoadState {
  std::string real_path;
  bool open_succeeds{true};
  bool symbol_exists{true};
  Status set_config_status{SUCCESS};
  artifact::PythonRuntimeKey loaded_key;
  PythonFusionPassBridgeApi api{};
  const PythonFusionPassBridgeApi *api_to_return{nullptr};
  int close_count{0};
  std::string config_artifact_root;
  std::string config_native_module_path;
  std::string looked_up_symbol;
};

int g_fake_handle = 0;
FakeBridgeLoadState g_fake_state;

Status FakeSetArtifactConfig(const PythonFusionPassBridgeArtifactConfig *config) {
  g_fake_state.config_artifact_root =
      ((config == nullptr) || (config->artifact_root == nullptr)) ? "" : config->artifact_root;
  g_fake_state.config_native_module_path =
      ((config == nullptr) || (config->native_module_path == nullptr)) ? "" : config->native_module_path;
  return g_fake_state.set_config_status;
}

Status FakeRegisterPasses(const PythonFusionPassRegistrar *) {
  return SUCCESS;
}

void FakeResetBridgeState() {}

void FakeShutdownBridge() {}

const PythonFusionPassBridgeApi *GetFakeBridgeApi() {
  return g_fake_state.api_to_return;
}

std::string FakeRealPath(const char *) {
  return g_fake_state.real_path;
}

void *FakeOpenLibrary(const char *, int) {
  return g_fake_state.open_succeeds ? static_cast<void *>(&g_fake_handle) : nullptr;
}

int FakeCloseLibrary(void *) {
  ++g_fake_state.close_count;
  return 0;
}

void *FakeLookupSymbol(void *, const char *symbol) {
  g_fake_state.looked_up_symbol = (symbol == nullptr) ? "" : symbol;
  if (!g_fake_state.symbol_exists) {
    return nullptr;
  }
  return reinterpret_cast<void *>(&GetFakeBridgeApi);
}

artifact::PythonRuntimeKey FakeResolveLoadedRuntimeKey() {
  return g_fake_state.loaded_key;
}

bridge_loader::BridgeLoadDependencies MakeFakeBridgeLoadDependencies() {
  return bridge_loader::BridgeLoadDependencies{
      &FakeRealPath,
      &FakeOpenLibrary,
      &FakeCloseLibrary,
      &FakeLookupSymbol,
      &FakeResolveLoadedRuntimeKey,
      "FakeGetApi",
      1U,
      0,
  };
}

void ResetFakeBridgeLoadState() {
  g_fake_state = FakeBridgeLoadState{};
  g_fake_state.real_path = "/tmp/libge_python_pass_bridge.so";
  g_fake_state.loaded_key.python_tag = "cp313";
  g_fake_state.api = PythonFusionPassBridgeApi{
      1U, &FakeSetArtifactConfig, &FakeRegisterPasses, &FakeResetBridgeState, &FakeShutdownBridge,
  };
  g_fake_state.api_to_return = &g_fake_state.api;
}

artifact::BridgeLibraryCandidate MakeBridgeCandidate() {
  return artifact::BridgeLibraryCandidate{
      "libge_python_pass_bridge.so",
      "/tmp/ge/passes/python_pass_artifacts/cp313-linux-x86_64",
      "/tmp/ge/passes/python_pass_artifacts/cp313-linux-x86_64/_ge_pass_native.so",
  };
}

bridge_loader::BridgeLoadStatus TryLoadBridgeCandidateForUt(
    const artifact::PythonRuntimeKey &expected_key, const artifact::BridgeLibraryCandidate &candidate,
    const bridge_loader::BridgeLoadDependencies &deps,
    bridge_loader::LoadedBridgeCandidate<PythonFusionPassBridgeApi> &loaded_bridge) {
  return bridge_loader::TryLoadBridgeCandidate<PythonFusionPassBridgeApi, PythonFusionPassBridgeArtifactConfig>(
      expected_key, candidate, deps, &loader_helper::IsBridgeApiValid, loaded_bridge);
}

TEST(PythonPassArtifactSelectorTest, ParsePythonTagSupportsSupportedMinorVersions) {
  EXPECT_EQ(artifact::ParsePythonTag("3.9.18"), "cp39");
  EXPECT_EQ(artifact::ParsePythonTag("3.10.12"), "cp310");
  EXPECT_EQ(artifact::ParsePythonTag("3.13.2 (main, Jan 1 2026)"), "cp313");
  EXPECT_EQ(artifact::ParsePythonTag("3.14.0rc1"), "cp314");
}

TEST(PythonPassArtifactSelectorTest, ParsePythonTagRejectsInvalidVersionString) {
  EXPECT_EQ(artifact::ParsePythonTag(nullptr), "");
  EXPECT_EQ(artifact::ParsePythonTag("python 3.11"), "");
  EXPECT_EQ(artifact::ParsePythonTag("3.x.1"), "");
  EXPECT_EQ(artifact::ParsePythonTag(""), "");
}

TEST(PythonPassArtifactSelectorTest, PythonRuntimeKeyToStringHandlesResolvedAndUnresolvedState) {
  artifact::PythonRuntimeKey runtime_key;
  EXPECT_EQ(runtime_key.ToString(), "unresolved");

  runtime_key.source = "PATH";
  EXPECT_EQ(runtime_key.ToString(), "PATH");

  runtime_key.python_tag = "cp313";
  runtime_key.version = "3.13.2";
  runtime_key.is_initialized = true;
  EXPECT_EQ(runtime_key.ToString(), "source[PATH], python_tag[cp313], initialized[1], version[3.13.2]");
}

TEST(PythonPassArtifactSelectorTest, PathHelpersHandleCommonLayouts) {
  EXPECT_EQ(artifact::DirName("/a/b/c.so"), "/a/b");
  EXPECT_EQ(artifact::DirName("/a"), "/");
  EXPECT_EQ(artifact::DirName("c.so"), ".");
  EXPECT_EQ(artifact::BaseName("/a/b/c.so"), "c.so");
  EXPECT_EQ(artifact::BaseName("c.so"), "c.so");
  EXPECT_EQ(artifact::JoinPath("", "c.so"), "c.so");
  EXPECT_EQ(artifact::JoinPath("/a/b", "c.so"), "/a/b/c.so");
  EXPECT_EQ(artifact::JoinPath("/a/b/", "c.so"), "/a/b/c.so");
  EXPECT_EQ(RealPath(nullptr), "");
  EXPECT_EQ(RealPath(""), "");
}

TEST(PythonPassArtifactSelectorTest, LoadArtifactManifestReturnsResolvedArtifactPaths) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  PrepareArtifactSet(tree, "artifact", "cp313", 1U);

  artifact::PythonArtifactSet artifact;
  ASSERT_TRUE(artifact::LoadArtifactManifest(tree.Path("artifact/manifest.json"), artifact));
  EXPECT_EQ(artifact.python_tag, "cp313");
  EXPECT_EQ(artifact.platform, artifact::CurrentPlatformTag());
  EXPECT_EQ(artifact.bridge_abi, 1U);
  EXPECT_EQ(artifact::BaseName(artifact.bridge_path), "libge_python_pass_bridge.so");
  EXPECT_EQ(artifact::BaseName(artifact.native_module_path), "_ge_pass_native.so");
}

TEST(PythonPassArtifactSelectorTest, LoadArtifactManifestRejectsBrokenManifest) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.WriteFile("bad_json/manifest.json", "{");
  tree.WriteFile("missing_fields/manifest.json", "{\"python_tag\":\"cp313\"}");
  tree.WriteFile("bad_artifacts/manifest.json",
                 "{\"python_tag\":\"cp313\",\"platform\":\"" + artifact::CurrentPlatformTag() +
                     "\",\"bridge_abi\":1,\"artifacts\":{\"bridge\":\"libge_python_pass_bridge.so\"}}");
  tree.WriteFile("missing_native/libge_python_pass_bridge.so", "bridge");
  tree.WriteFile("missing_native/manifest.json", MakeManifest("cp313", artifact::CurrentPlatformTag(), 1U));

  artifact::PythonArtifactSet artifact;
  EXPECT_FALSE(artifact::LoadArtifactManifest(tree.Path("bad_json/manifest.json"), artifact));
  EXPECT_FALSE(artifact::LoadArtifactManifest(tree.Path("missing_fields/manifest.json"), artifact));
  EXPECT_FALSE(artifact::LoadArtifactManifest(tree.Path("bad_artifacts/manifest.json"), artifact));
  EXPECT_FALSE(artifact::LoadArtifactManifest(tree.Path("not_exist/manifest.json"), artifact));
  EXPECT_FALSE(artifact::LoadArtifactManifest(tree.Path("missing_native/manifest.json"), artifact));
}

TEST(PythonPassArtifactSelectorTest, BuildGePackageDirCandidatesSupportsRunPackageAndWheelLayout) {
  const ScopedEnvVar python_path(artifact::kPythonPathEnvName, "");
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.MakeDir("run/lib64");
  tree.MakeDir("run/x86_64-linux/lib64");
  tree.MakeDir("run/python/site-packages/ge");
  tree.MakeDir("wheel/ge/_capi");
  tree.MakeDir("inplace/ge");

  const auto run_dirs = artifact::BuildGePackageDirCandidates(tree.Path("run/lib64/libge_compiler.so"));
  ASSERT_EQ(run_dirs.size(), 1U);
  EXPECT_EQ(run_dirs[0], tree.Path("run/python/site-packages/ge"));

  const auto arch_run_dirs =
      artifact::BuildGePackageDirCandidates(tree.Path("run/x86_64-linux/lib64/libge_compiler.so"));
  ASSERT_EQ(arch_run_dirs.size(), 1U);
  EXPECT_EQ(arch_run_dirs[0], tree.Path("run/python/site-packages/ge"));

  const auto wheel_dirs = artifact::BuildGePackageDirCandidates(tree.Path("wheel/ge/_capi/graph.so"));
  ASSERT_EQ(wheel_dirs.size(), 1U);
  EXPECT_EQ(wheel_dirs[0], tree.Path("wheel/ge"));

  const auto inplace_dirs = artifact::BuildGePackageDirCandidates(tree.Path("inplace/ge/graph.so"));
  ASSERT_EQ(inplace_dirs.size(), 1U);
  EXPECT_EQ(inplace_dirs[0], tree.Path("inplace/ge"));

  EXPECT_TRUE(artifact::BuildGePackageDirCandidates("").empty());
}

TEST(PythonPassArtifactSelectorTest, BuildGePackageDirCandidatesUsesPythonPathFallback) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.MakeDir("py/site-packages/ge");
  const std::string python_path =
      tree.Path("missing") + ":" + tree.Path("py/site-packages") + ":" + tree.Path("py/site-packages/ge");
  const ScopedEnvVar scoped_python_path(artifact::kPythonPathEnvName, python_path);

  const auto dirs = artifact::BuildGePackageDirCandidates("");
  ASSERT_EQ(dirs.size(), 1U);
  EXPECT_EQ(dirs[0], tree.Path("py/site-packages/ge"));
}

TEST(PythonPassArtifactSelectorTest, BuildPrebuiltBridgeLibraryCandidatesReturnsMatchingArtifact) {
  const ScopedEnvVar python_path(artifact::kPythonPathEnvName, "");
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.MakeDir("run/lib64");
  tree.WriteFile("run/lib64/ge_compiler.so", "loader");
  PrepareArtifactSet(tree, "run/python/site-packages/ge/passes/python_pass_artifacts/cp313-linux", "cp313", 1U);
  PrepareArtifactSet(tree, "run/python/site-packages/ge/passes/python_pass_artifacts/cp312-linux", "cp312", 1U);
  PrepareArtifactSet(tree, "run/python/site-packages/ge/passes/python_pass_artifacts/cp313-bad-abi", "cp313", 2U);

  artifact::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp313";
  const auto candidates = artifact::BuildPrebuiltBridgeLibraryCandidates(
      runtime_key, tree.Path("run/lib64/ge_compiler.so"), kPythonPassArtifactsRelativePath, 1U);

  ASSERT_EQ(candidates.size(), 1U);
  EXPECT_EQ(artifact::BaseName(candidates[0].bridge_path), "libge_python_pass_bridge.so");
  EXPECT_EQ(artifact::BaseName(candidates[0].native_module_path), "_ge_pass_native.so");
  EXPECT_NE(candidates[0].artifact_root.find("cp313-linux"), std::string::npos);
}

TEST(PythonPassArtifactSelectorTest, BuildPrebuiltBridgeLibraryCandidatesSkipsInvalidManifest) {
  const ScopedEnvVar python_path(artifact::kPythonPathEnvName, "");
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.MakeDir("run/lib64");
  tree.WriteFile("run/lib64/ge_compiler.so", "loader");
  tree.WriteFile("run/python/site-packages/ge/passes/python_pass_artifacts/bad/manifest.json", "{");

  artifact::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp313";
  const auto candidates = artifact::BuildPrebuiltBridgeLibraryCandidates(
      runtime_key, tree.Path("run/lib64/ge_compiler.so"), kPythonPassArtifactsRelativePath, 1U);
  EXPECT_TRUE(candidates.empty());
}

TEST(PythonPassArtifactSelectorTest, LoadBridgeCandidateFromArtifactRootLoadsMatchingGeneratedArtifact) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  PrepareArtifactSet(tree, "generated", "cp313", 1U);

  artifact::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp313";
  const auto candidate = artifact::LoadBridgeCandidateFromArtifactRoot(tree.Path("generated"), runtime_key, 1U);
  ASSERT_FALSE(candidate.bridge_path.empty());
  EXPECT_EQ(artifact::BaseName(candidate.bridge_path), "libge_python_pass_bridge.so");
  EXPECT_EQ(artifact::BaseName(candidate.native_module_path), "_ge_pass_native.so");
  EXPECT_EQ(candidate.artifact_root, RealPath(tree.Path("generated").c_str()));
}

TEST(PythonPassArtifactSelectorTest, LoadBridgeCandidateFromArtifactRootRejectsMismatchedGeneratedArtifact) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  PrepareArtifactSet(tree, "generated", "cp313", 1U);

  artifact::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp312";
  auto candidate = artifact::LoadBridgeCandidateFromArtifactRoot(tree.Path("generated"), runtime_key, 1U);
  EXPECT_TRUE(candidate.bridge_path.empty());

  runtime_key.python_tag = "cp313";
  candidate = artifact::LoadBridgeCandidateFromArtifactRoot(tree.Path("generated"), runtime_key, 2U);
  EXPECT_TRUE(candidate.bridge_path.empty());
  candidate = artifact::LoadBridgeCandidateFromArtifactRoot("", runtime_key, 1U);
  EXPECT_TRUE(candidate.bridge_path.empty());
}

TEST(PythonPassArtifactSelectorTest, AppendMatchedArtifactCandidateRejectsPlatformMismatch) {
  artifact::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp313";
  artifact::PythonArtifactSet artifact;
  artifact.python_tag = "cp313";
  artifact.platform = "linux-other";
  artifact.bridge_abi = 1U;

  std::vector<artifact::BridgeLibraryCandidate> candidates;
  artifact::AppendMatchedArtifactCandidate(runtime_key, artifact, artifact::CurrentPlatformTag(), 1U, candidates);
  EXPECT_TRUE(candidates.empty());
}

TEST(PythonPassBridgeLoaderHelperTest, LineHelpersParseProbeOutput) {
  EXPECT_EQ(loader_helper::FirstLine("cp313\n3.13.2\n"), "cp313");
  EXPECT_EQ(loader_helper::FirstLine("cp313"), "cp313");
  EXPECT_EQ(loader_helper::SecondLine("cp313\n3.13.2\n"), "3.13.2");
  EXPECT_EQ(loader_helper::SecondLine("cp313\n3.13.2"), "3.13.2");
  EXPECT_EQ(loader_helper::SecondLine("cp313"), "");
}

TEST(PythonPassBridgeLoaderHelperTest, BridgeLoadStatusToStringCoversAllStatuses) {
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(bridge_loader::BridgeLoadStatus::kSuccess), "success");
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(bridge_loader::BridgeLoadStatus::kInvalidDependency),
               "invalid dependency");
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(bridge_loader::BridgeLoadStatus::kInvalidPath), "invalid path");
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(bridge_loader::BridgeLoadStatus::kOpenFailed), "open failed");
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(bridge_loader::BridgeLoadStatus::kRuntimeMismatch),
               "runtime mismatch");
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(bridge_loader::BridgeLoadStatus::kMissingApiSymbol),
               "missing api symbol");
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(bridge_loader::BridgeLoadStatus::kInvalidApi), "invalid api");
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(bridge_loader::BridgeLoadStatus::kSetArtifactConfigFailed),
               "set artifact config failed");
  EXPECT_STREQ(bridge_loader::BridgeLoadStatusToString(static_cast<bridge_loader::BridgeLoadStatus>(999)), "unknown");
}

TEST(PythonPassBridgeLoaderHelperTest, RuntimeKeyCompatibilityAllowsUnknownAndRejectsMismatch) {
  artifact::PythonRuntimeKey expected_key;
  artifact::PythonRuntimeKey loaded_key;
  EXPECT_TRUE(artifact::IsRuntimeKeyCompatible(expected_key, loaded_key));

  expected_key.python_tag = "cp313";
  EXPECT_TRUE(artifact::IsRuntimeKeyCompatible(expected_key, loaded_key));

  loaded_key.python_tag = "cp313";
  EXPECT_TRUE(artifact::IsRuntimeKeyCompatible(expected_key, loaded_key));

  loaded_key.python_tag = "cp312";
  EXPECT_FALSE(artifact::IsRuntimeKeyCompatible(expected_key, loaded_key));
}

TEST(PythonPassBridgeLoaderHelperTest, BridgeApiValidationChecksAbiAndRequiredCallbacks) {
  ResetFakeBridgeLoadState();
  EXPECT_TRUE(loader_helper::IsBridgeApiValid(&g_fake_state.api, 1U));
  EXPECT_FALSE(loader_helper::IsBridgeApiValid(nullptr, 1U));

  auto invalid_api = g_fake_state.api;
  invalid_api.abi_version = 2U;
  EXPECT_FALSE(loader_helper::IsBridgeApiValid(&invalid_api, 1U));

  invalid_api = g_fake_state.api;
  invalid_api.register_passes = nullptr;
  EXPECT_FALSE(loader_helper::IsBridgeApiValid(&invalid_api, 1U));
}

TEST(PythonPassBridgeLoaderHelperTest, BuildArtifactConfigSetsCandidateConfig) {
  const auto candidate = MakeBridgeCandidate();
  const auto config = bridge_loader::BuildArtifactConfig<PythonFusionPassBridgeArtifactConfig>(candidate);
  ASSERT_NE(config.artifact_root, nullptr);
  ASSERT_NE(config.native_module_path, nullptr);
  EXPECT_STREQ(config.artifact_root, candidate.artifact_root.c_str());
  EXPECT_STREQ(config.native_module_path, candidate.native_module_path.c_str());
}

TEST(PythonPassBridgeLoaderHelperTest, TryLoadBridgeCandidateSucceedsAndSetsArtifactConfig) {
  ResetFakeBridgeLoadState();
  artifact::PythonRuntimeKey expected_key;
  expected_key.python_tag = "cp313";
  bridge_loader::LoadedBridgeCandidate<PythonFusionPassBridgeApi> loaded_bridge;

  const auto status =
      TryLoadBridgeCandidateForUt(expected_key, MakeBridgeCandidate(), MakeFakeBridgeLoadDependencies(), loaded_bridge);

  EXPECT_EQ(status, bridge_loader::BridgeLoadStatus::kSuccess);
  EXPECT_EQ(loaded_bridge.handle, static_cast<void *>(&g_fake_handle));
  EXPECT_EQ(loaded_bridge.api, &g_fake_state.api);
  EXPECT_EQ(loaded_bridge.real_path, g_fake_state.real_path);
  EXPECT_EQ(g_fake_state.looked_up_symbol, "FakeGetApi");
  EXPECT_EQ(g_fake_state.close_count, 0);
  EXPECT_NE(g_fake_state.config_artifact_root.find("cp313-linux"), std::string::npos);
  EXPECT_EQ(artifact::BaseName(g_fake_state.config_native_module_path), "_ge_pass_native.so");
}

TEST(PythonPassBridgeLoaderHelperTest, TryLoadBridgeCandidateCoversFailureStatuses) {
  artifact::PythonRuntimeKey expected_key;
  expected_key.python_tag = "cp313";
  bridge_loader::LoadedBridgeCandidate<PythonFusionPassBridgeApi> loaded_bridge;
  const auto candidate = MakeBridgeCandidate();

  ResetFakeBridgeLoadState();
  auto deps = MakeFakeBridgeLoadDependencies();
  deps.open_library = nullptr;
  EXPECT_EQ(TryLoadBridgeCandidateForUt(expected_key, candidate, deps, loaded_bridge),
            bridge_loader::BridgeLoadStatus::kInvalidDependency);

  ResetFakeBridgeLoadState();
  g_fake_state.real_path.clear();
  EXPECT_EQ(TryLoadBridgeCandidateForUt(expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            bridge_loader::BridgeLoadStatus::kInvalidPath);
  EXPECT_EQ(g_fake_state.close_count, 0);

  ResetFakeBridgeLoadState();
  g_fake_state.open_succeeds = false;
  EXPECT_EQ(TryLoadBridgeCandidateForUt(expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            bridge_loader::BridgeLoadStatus::kOpenFailed);
  EXPECT_EQ(g_fake_state.close_count, 0);

  ResetFakeBridgeLoadState();
  g_fake_state.loaded_key.python_tag = "cp312";
  EXPECT_EQ(TryLoadBridgeCandidateForUt(expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            bridge_loader::BridgeLoadStatus::kRuntimeMismatch);
  EXPECT_EQ(g_fake_state.close_count, 1);

  ResetFakeBridgeLoadState();
  g_fake_state.symbol_exists = false;
  EXPECT_EQ(TryLoadBridgeCandidateForUt(expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            bridge_loader::BridgeLoadStatus::kMissingApiSymbol);
  EXPECT_EQ(g_fake_state.close_count, 1);

  ResetFakeBridgeLoadState();
  auto invalid_api = g_fake_state.api;
  invalid_api.shutdown_bridge = nullptr;
  g_fake_state.api_to_return = &invalid_api;
  EXPECT_EQ(TryLoadBridgeCandidateForUt(expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            bridge_loader::BridgeLoadStatus::kInvalidApi);
  EXPECT_EQ(g_fake_state.close_count, 1);

  ResetFakeBridgeLoadState();
  g_fake_state.set_config_status = FAILED;
  EXPECT_EQ(TryLoadBridgeCandidateForUt(expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            bridge_loader::BridgeLoadStatus::kSetArtifactConfigFailed);
  EXPECT_EQ(g_fake_state.close_count, 1);
}

}  // namespace
}  // namespace fusion
}  // namespace ge
