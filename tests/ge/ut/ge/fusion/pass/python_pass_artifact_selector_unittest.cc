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

#include "compiler/graph/fusion/pass/python_pass_artifact_selector.h"
#include "compiler/graph/fusion/pass/python_pass_bridge_loader_helper.h"

namespace ge {
namespace fusion {
namespace {
namespace selector = python_pass_artifact;
namespace loader_helper = python_pass_bridge_loader;

class ScopedTempTree {
 public:
  ScopedTempTree() {
    char dir_template[] = "/tmp/ge_pass_artifact_selector_ut_XXXXXX";
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
    return selector::JoinPath(root_, relative_path);
  }

  std::string MakeDir(const std::string &relative_path) {
    std::string current = root_;
    size_t start = 0U;
    while (start < relative_path.size()) {
      const auto end = relative_path.find('/', start);
      const auto part = relative_path.substr(start, end == std::string::npos ? end : end - start);
      if (!part.empty()) {
        current = selector::JoinPath(current, part);
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
    const auto dir = selector::DirName(relative_path);
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

std::string MakeManifest(const std::string &python_tag, const std::string &platform_tag,
                         const uint32_t bridge_abi) {
  return "{\n"
         "  \"python_tag\": \"" + python_tag + "\",\n"
         "  \"platform\": \"" + platform_tag + "\",\n"
         "  \"bridge_abi\": " + std::to_string(bridge_abi) + ",\n"
         "  \"artifacts\": {\n"
         "    \"bridge\": \"libge_python_pass_bridge.so\",\n"
         "    \"native\": \"_ge_pass_native.so\"\n"
         "  }\n"
         "}\n";
}

void PrepareArtifactSet(ScopedTempTree &tree, const std::string &artifact_dir,
                        const std::string &python_tag, const uint32_t bridge_abi) {
  const auto platform_tag = selector::CurrentPlatformTag();
  tree.WriteFile(selector::JoinPath(artifact_dir, "libge_python_pass_bridge.so"), "bridge");
  tree.WriteFile(selector::JoinPath(artifact_dir, "_ge_pass_native.so"), "native");
  tree.WriteFile(selector::JoinPath(artifact_dir, "manifest.json"),
                 MakeManifest(python_tag, platform_tag, bridge_abi));
}

struct FakeBridgeLoadState {
  std::string real_path;
  bool open_succeeds{true};
  bool symbol_exists{true};
  Status set_config_status{SUCCESS};
  selector::PythonRuntimeKey loaded_key;
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

selector::PythonRuntimeKey FakeResolveLoadedRuntimeKey() {
  return g_fake_state.loaded_key;
}

loader_helper::BridgeLoadDependencies MakeFakeBridgeLoadDependencies() {
  return loader_helper::BridgeLoadDependencies{
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
  g_fake_state = FakeBridgeLoadState {};
  g_fake_state.real_path = "/tmp/libge_python_pass_bridge.so";
  g_fake_state.loaded_key.python_tag = "cp313";
  g_fake_state.api = PythonFusionPassBridgeApi{
      1U,
      &FakeSetArtifactConfig,
      &FakeRegisterPasses,
      &FakeResetBridgeState,
      &FakeShutdownBridge,
  };
  g_fake_state.api_to_return = &g_fake_state.api;
}

selector::BridgeLibraryCandidate MakeBridgeCandidate() {
  return selector::BridgeLibraryCandidate{
      "libge_python_pass_bridge.so",
      "/tmp/ge/passes/python_pass_artifacts/cp313-linux-x86_64",
      "/tmp/ge/passes/python_pass_artifacts/cp313-linux-x86_64/_ge_pass_native.so",
  };
}

TEST(PythonPassArtifactSelectorTest, ParsePythonTagSupportsSupportedMinorVersions) {
  EXPECT_EQ(selector::ParsePythonTag("3.9.18"), "cp39");
  EXPECT_EQ(selector::ParsePythonTag("3.10.12"), "cp310");
  EXPECT_EQ(selector::ParsePythonTag("3.13.2 (main, Jan 1 2026)"), "cp313");
  EXPECT_EQ(selector::ParsePythonTag("3.14.0rc1"), "cp314");
}

TEST(PythonPassArtifactSelectorTest, ParsePythonTagRejectsInvalidVersionString) {
  EXPECT_EQ(selector::ParsePythonTag(nullptr), "");
  EXPECT_EQ(selector::ParsePythonTag("python 3.11"), "");
  EXPECT_EQ(selector::ParsePythonTag("3.x.1"), "");
  EXPECT_EQ(selector::ParsePythonTag(""), "");
}

TEST(PythonPassArtifactSelectorTest, PythonRuntimeKeyToStringHandlesResolvedAndUnresolvedState) {
  selector::PythonRuntimeKey runtime_key;
  EXPECT_EQ(runtime_key.ToString(), "unresolved");

  runtime_key.source = "PATH";
  EXPECT_EQ(runtime_key.ToString(), "PATH");

  runtime_key.python_tag = "cp313";
  runtime_key.version = "3.13.2";
  runtime_key.is_initialized = true;
  EXPECT_EQ(runtime_key.ToString(), "source[PATH], python_tag[cp313], initialized[1], version[3.13.2]");
}

TEST(PythonPassArtifactSelectorTest, PathHelpersHandleCommonLayouts) {
  EXPECT_EQ(selector::DirName("/a/b/c.so"), "/a/b");
  EXPECT_EQ(selector::DirName("/a"), "/");
  EXPECT_EQ(selector::DirName("c.so"), ".");
  EXPECT_EQ(selector::BaseName("/a/b/c.so"), "c.so");
  EXPECT_EQ(selector::BaseName("c.so"), "c.so");
  EXPECT_EQ(selector::JoinPath("", "c.so"), "c.so");
  EXPECT_EQ(selector::JoinPath("/a/b", "c.so"), "/a/b/c.so");
  EXPECT_EQ(selector::JoinPath("/a/b/", "c.so"), "/a/b/c.so");
  EXPECT_EQ(selector::ResolveRealPath(nullptr), "");
  EXPECT_EQ(selector::ResolveRealPath(""), "");
}

TEST(PythonPassArtifactSelectorTest, LoadArtifactManifestReturnsResolvedArtifactPaths) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  PrepareArtifactSet(tree, "artifact", "cp313", 1U);

  selector::PythonPassArtifactSet artifact;
  ASSERT_TRUE(selector::LoadArtifactManifest(tree.Path("artifact/manifest.json"), artifact));
  EXPECT_EQ(artifact.python_tag, "cp313");
  EXPECT_EQ(artifact.platform, selector::CurrentPlatformTag());
  EXPECT_EQ(artifact.bridge_abi, 1U);
  EXPECT_EQ(selector::BaseName(artifact.bridge_path), "libge_python_pass_bridge.so");
  EXPECT_EQ(selector::BaseName(artifact.native_module_path), "_ge_pass_native.so");
}

TEST(PythonPassArtifactSelectorTest, LoadArtifactManifestRejectsBrokenManifest) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.WriteFile("bad_json/manifest.json", "{");
  tree.WriteFile("missing_fields/manifest.json", "{\"python_tag\":\"cp313\"}");
  tree.WriteFile("bad_artifacts/manifest.json",
                 "{\"python_tag\":\"cp313\",\"platform\":\"" + selector::CurrentPlatformTag() +
                 "\",\"bridge_abi\":1,\"artifacts\":{\"bridge\":\"libge_python_pass_bridge.so\"}}");
  tree.WriteFile("missing_native/libge_python_pass_bridge.so", "bridge");
  tree.WriteFile("missing_native/manifest.json", MakeManifest("cp313", selector::CurrentPlatformTag(), 1U));

  selector::PythonPassArtifactSet artifact;
  EXPECT_FALSE(selector::LoadArtifactManifest(tree.Path("bad_json/manifest.json"), artifact));
  EXPECT_FALSE(selector::LoadArtifactManifest(tree.Path("missing_fields/manifest.json"), artifact));
  EXPECT_FALSE(selector::LoadArtifactManifest(tree.Path("bad_artifacts/manifest.json"), artifact));
  EXPECT_FALSE(selector::LoadArtifactManifest(tree.Path("not_exist/manifest.json"), artifact));
  EXPECT_FALSE(selector::LoadArtifactManifest(tree.Path("missing_native/manifest.json"), artifact));
}

TEST(PythonPassArtifactSelectorTest, BuildGePackageDirCandidatesSupportsRunPackageAndWheelLayout) {
  const ScopedEnvVar python_path(selector::kPythonPathEnvName, "");
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.MakeDir("run/lib64");
  tree.MakeDir("run/x86_64-linux/lib64");
  tree.MakeDir("run/python/site-packages/ge");
  tree.MakeDir("wheel/ge/_capi");
  tree.MakeDir("inplace/ge");

  const auto run_dirs = selector::BuildGePackageDirCandidates(tree.Path("run/lib64/libge_compiler.so"));
  ASSERT_EQ(run_dirs.size(), 1U);
  EXPECT_EQ(run_dirs[0], tree.Path("run/python/site-packages/ge"));

  const auto arch_run_dirs =
      selector::BuildGePackageDirCandidates(tree.Path("run/x86_64-linux/lib64/libge_compiler.so"));
  ASSERT_EQ(arch_run_dirs.size(), 1U);
  EXPECT_EQ(arch_run_dirs[0], tree.Path("run/python/site-packages/ge"));

  const auto wheel_dirs = selector::BuildGePackageDirCandidates(tree.Path("wheel/ge/_capi/graph.so"));
  ASSERT_EQ(wheel_dirs.size(), 1U);
  EXPECT_EQ(wheel_dirs[0], tree.Path("wheel/ge"));

  const auto inplace_dirs = selector::BuildGePackageDirCandidates(tree.Path("inplace/ge/graph.so"));
  ASSERT_EQ(inplace_dirs.size(), 1U);
  EXPECT_EQ(inplace_dirs[0], tree.Path("inplace/ge"));

  EXPECT_TRUE(selector::BuildGePackageDirCandidates("").empty());
}

TEST(PythonPassArtifactSelectorTest, BuildGePackageDirCandidatesUsesPythonPathFallback) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.MakeDir("py/site-packages/ge");
  const std::string python_path = tree.Path("missing") + ":" + tree.Path("py/site-packages") + ":" +
                                  tree.Path("py/site-packages/ge");
  const ScopedEnvVar scoped_python_path(selector::kPythonPathEnvName, python_path);

  const auto dirs = selector::BuildGePackageDirCandidates("");
  ASSERT_EQ(dirs.size(), 1U);
  EXPECT_EQ(dirs[0], tree.Path("py/site-packages/ge"));
}

TEST(PythonPassArtifactSelectorTest, BuildPrebuiltBridgeLibraryCandidatesReturnsMatchingArtifact) {
  const ScopedEnvVar python_path(selector::kPythonPathEnvName, "");
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.MakeDir("run/lib64");
  tree.WriteFile("run/lib64/ge_compiler.so", "loader");
  PrepareArtifactSet(tree, "run/python/site-packages/ge/passes/python_pass_artifacts/cp313-linux", "cp313", 1U);
  PrepareArtifactSet(tree, "run/python/site-packages/ge/passes/python_pass_artifacts/cp312-linux", "cp312", 1U);
  PrepareArtifactSet(tree, "run/python/site-packages/ge/passes/python_pass_artifacts/cp313-bad-abi", "cp313", 2U);

  selector::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp313";
  const auto candidates = selector::BuildPrebuiltBridgeLibraryCandidates(
      runtime_key, tree.Path("run/lib64/ge_compiler.so"), 1U);

  ASSERT_EQ(candidates.size(), 1U);
  EXPECT_EQ(selector::BaseName(candidates[0].bridge_path), "libge_python_pass_bridge.so");
  EXPECT_EQ(selector::BaseName(candidates[0].native_module_path), "_ge_pass_native.so");
  EXPECT_NE(candidates[0].artifact_root.find("cp313-linux"), std::string::npos);
}

TEST(PythonPassArtifactSelectorTest, BuildPrebuiltBridgeLibraryCandidatesSkipsInvalidManifest) {
  const ScopedEnvVar python_path(selector::kPythonPathEnvName, "");
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  tree.MakeDir("run/lib64");
  tree.WriteFile("run/lib64/ge_compiler.so", "loader");
  tree.WriteFile("run/python/site-packages/ge/passes/python_pass_artifacts/bad/manifest.json", "{");

  selector::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp313";
  const auto candidates = selector::BuildPrebuiltBridgeLibraryCandidates(
      runtime_key, tree.Path("run/lib64/ge_compiler.so"), 1U);
  EXPECT_TRUE(candidates.empty());
}

TEST(PythonPassArtifactSelectorTest, LoadBridgeCandidateFromArtifactRootLoadsMatchingGeneratedArtifact) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  PrepareArtifactSet(tree, "generated", "cp313", 1U);

  selector::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp313";
  const auto candidate = selector::LoadBridgeCandidateFromArtifactRoot(tree.Path("generated"), runtime_key, 1U);
  ASSERT_FALSE(candidate.bridge_path.empty());
  EXPECT_EQ(selector::BaseName(candidate.bridge_path), "libge_python_pass_bridge.so");
  EXPECT_EQ(selector::BaseName(candidate.native_module_path), "_ge_pass_native.so");
  EXPECT_EQ(candidate.artifact_root, selector::ResolveRealPath(tree.Path("generated").c_str()));
}

TEST(PythonPassArtifactSelectorTest, LoadBridgeCandidateFromArtifactRootRejectsMismatchedGeneratedArtifact) {
  ScopedTempTree tree;
  ASSERT_FALSE(tree.Root().empty());
  PrepareArtifactSet(tree, "generated", "cp313", 1U);

  selector::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp312";
  auto candidate = selector::LoadBridgeCandidateFromArtifactRoot(tree.Path("generated"), runtime_key, 1U);
  EXPECT_TRUE(candidate.bridge_path.empty());

  runtime_key.python_tag = "cp313";
  candidate = selector::LoadBridgeCandidateFromArtifactRoot(tree.Path("generated"), runtime_key, 2U);
  EXPECT_TRUE(candidate.bridge_path.empty());
  candidate = selector::LoadBridgeCandidateFromArtifactRoot("", runtime_key, 1U);
  EXPECT_TRUE(candidate.bridge_path.empty());
}

TEST(PythonPassArtifactSelectorTest, AppendMatchedArtifactCandidateRejectsPlatformMismatch) {
  selector::PythonRuntimeKey runtime_key;
  runtime_key.python_tag = "cp313";
  selector::PythonPassArtifactSet artifact;
  artifact.python_tag = "cp313";
  artifact.platform = "linux-other";
  artifact.bridge_abi = 1U;

  std::vector<selector::BridgeLibraryCandidate> candidates;
  selector::AppendMatchedArtifactCandidate(runtime_key, artifact, selector::CurrentPlatformTag(), 1U, candidates);
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
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(loader_helper::BridgeLoadStatus::kSuccess), "success");
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(loader_helper::BridgeLoadStatus::kInvalidDependency),
               "invalid dependency");
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(loader_helper::BridgeLoadStatus::kInvalidPath),
               "invalid path");
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(loader_helper::BridgeLoadStatus::kOpenFailed), "open failed");
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(loader_helper::BridgeLoadStatus::kRuntimeMismatch),
               "runtime mismatch");
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(loader_helper::BridgeLoadStatus::kMissingApiSymbol),
               "missing api symbol");
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(loader_helper::BridgeLoadStatus::kInvalidApi), "invalid api");
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(loader_helper::BridgeLoadStatus::kSetArtifactConfigFailed),
               "set artifact config failed");
  EXPECT_STREQ(loader_helper::BridgeLoadStatusToString(static_cast<loader_helper::BridgeLoadStatus>(999)),
               "unknown");
}

TEST(PythonPassBridgeLoaderHelperTest, RuntimeKeyCompatibilityAllowsUnknownAndRejectsMismatch) {
  selector::PythonRuntimeKey expected_key;
  selector::PythonRuntimeKey loaded_key;
  EXPECT_TRUE(loader_helper::IsRuntimeKeyCompatible(expected_key, loaded_key));

  expected_key.python_tag = "cp313";
  EXPECT_TRUE(loader_helper::IsRuntimeKeyCompatible(expected_key, loaded_key));

  loaded_key.python_tag = "cp313";
  EXPECT_TRUE(loader_helper::IsRuntimeKeyCompatible(expected_key, loaded_key));

  loaded_key.python_tag = "cp312";
  EXPECT_FALSE(loader_helper::IsRuntimeKeyCompatible(expected_key, loaded_key));
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
  const auto config = loader_helper::BuildArtifactConfig(candidate);
  ASSERT_NE(config.artifact_root, nullptr);
  ASSERT_NE(config.native_module_path, nullptr);
  EXPECT_STREQ(config.artifact_root, candidate.artifact_root.c_str());
  EXPECT_STREQ(config.native_module_path, candidate.native_module_path.c_str());
}

TEST(PythonPassBridgeLoaderHelperTest, TryLoadBridgeCandidateSucceedsAndSetsArtifactConfig) {
  ResetFakeBridgeLoadState();
  selector::PythonRuntimeKey expected_key;
  expected_key.python_tag = "cp313";
  loader_helper::LoadedBridgeCandidate loaded_bridge;

  const auto status = loader_helper::TryLoadBridgeCandidate(
      expected_key, MakeBridgeCandidate(), MakeFakeBridgeLoadDependencies(), loaded_bridge);

  EXPECT_EQ(status, loader_helper::BridgeLoadStatus::kSuccess);
  EXPECT_EQ(loaded_bridge.handle, static_cast<void *>(&g_fake_handle));
  EXPECT_EQ(loaded_bridge.api, &g_fake_state.api);
  EXPECT_EQ(loaded_bridge.real_path, g_fake_state.real_path);
  EXPECT_EQ(g_fake_state.looked_up_symbol, "FakeGetApi");
  EXPECT_EQ(g_fake_state.close_count, 0);
  EXPECT_NE(g_fake_state.config_artifact_root.find("cp313-linux"), std::string::npos);
  EXPECT_EQ(selector::BaseName(g_fake_state.config_native_module_path), "_ge_pass_native.so");
}

TEST(PythonPassBridgeLoaderHelperTest, TryLoadBridgeCandidateCoversFailureStatuses) {
  selector::PythonRuntimeKey expected_key;
  expected_key.python_tag = "cp313";
  loader_helper::LoadedBridgeCandidate loaded_bridge;
  const auto candidate = MakeBridgeCandidate();

  ResetFakeBridgeLoadState();
  auto deps = MakeFakeBridgeLoadDependencies();
  deps.open_library = nullptr;
  EXPECT_EQ(loader_helper::TryLoadBridgeCandidate(expected_key, candidate, deps, loaded_bridge),
            loader_helper::BridgeLoadStatus::kInvalidDependency);

  ResetFakeBridgeLoadState();
  g_fake_state.real_path.clear();
  EXPECT_EQ(loader_helper::TryLoadBridgeCandidate(
                expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            loader_helper::BridgeLoadStatus::kInvalidPath);
  EXPECT_EQ(g_fake_state.close_count, 0);

  ResetFakeBridgeLoadState();
  g_fake_state.open_succeeds = false;
  EXPECT_EQ(loader_helper::TryLoadBridgeCandidate(
                expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            loader_helper::BridgeLoadStatus::kOpenFailed);
  EXPECT_EQ(g_fake_state.close_count, 0);

  ResetFakeBridgeLoadState();
  g_fake_state.loaded_key.python_tag = "cp312";
  EXPECT_EQ(loader_helper::TryLoadBridgeCandidate(
                expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            loader_helper::BridgeLoadStatus::kRuntimeMismatch);
  EXPECT_EQ(g_fake_state.close_count, 1);

  ResetFakeBridgeLoadState();
  g_fake_state.symbol_exists = false;
  EXPECT_EQ(loader_helper::TryLoadBridgeCandidate(
                expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            loader_helper::BridgeLoadStatus::kMissingApiSymbol);
  EXPECT_EQ(g_fake_state.close_count, 1);

  ResetFakeBridgeLoadState();
  auto invalid_api = g_fake_state.api;
  invalid_api.shutdown_bridge = nullptr;
  g_fake_state.api_to_return = &invalid_api;
  EXPECT_EQ(loader_helper::TryLoadBridgeCandidate(
                expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            loader_helper::BridgeLoadStatus::kInvalidApi);
  EXPECT_EQ(g_fake_state.close_count, 1);

  ResetFakeBridgeLoadState();
  g_fake_state.set_config_status = FAILED;
  EXPECT_EQ(loader_helper::TryLoadBridgeCandidate(
                expected_key, candidate, MakeFakeBridgeLoadDependencies(), loaded_bridge),
            loader_helper::BridgeLoadStatus::kSetArtifactConfigFailed);
  EXPECT_EQ(g_fake_state.close_count, 1);
}

}  // namespace
}  // namespace fusion
}  // namespace ge
