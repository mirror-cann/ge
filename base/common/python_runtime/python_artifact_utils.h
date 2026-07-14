/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_PYTHON_RUNTIME_PYTHON_ARTIFACT_UTILS_H_
#define BASE_COMMON_PYTHON_RUNTIME_PYTHON_ARTIFACT_UTILS_H_

#include <dirent.h>
#include <dlfcn.h>
#include <sys/utsname.h>

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "file_utils.h"
#include "nlohmann/json.hpp"

namespace ge {
namespace python_artifact {

constexpr const char *kArtifactManifestName = "manifest.json";
constexpr const char *kGePackageRelativePath = "python/site-packages/ge";
constexpr const char *kPythonPathEnvName = "PYTHONPATH";
constexpr const char *kPyIsInitializedSymbol = "Py_IsInitialized";
constexpr const char *kPyGetVersionSymbol = "Py_GetVersion";

struct BridgeLibraryCandidate {
  std::string bridge_path;
  std::string artifact_root;
  std::string native_module_path;
};

struct PythonArtifactSet {
  std::string root;
  std::string manifest_path;
  std::string python_tag;
  std::string platform;
  uint32_t bridge_abi{0U};
  std::string bridge_path;
  std::string native_module_path;
};

struct PythonRuntimeKey {
  bool has_python_symbols{false};
  bool is_initialized{false};
  std::string python_tag;
  std::string version;
  std::string python_command;
  std::string source;

  std::string ToString() const {
    if (python_tag.empty() && version.empty()) {
      return source.empty() ? "unresolved" : source;
    }
    return "source[" + (source.empty() ? "unknown" : source) + "], python_tag[" +
           (python_tag.empty() ? "unknown" : python_tag) + "], initialized[" + (is_initialized ? "1" : "0") +
           "], version[" + version + "]";
  }
};

inline bool IsRuntimeKeyCompatible(const PythonRuntimeKey &expected_key, const PythonRuntimeKey &loaded_key) {
  return expected_key.python_tag.empty() || loaded_key.python_tag.empty() ||
         (loaded_key.python_tag == expected_key.python_tag);
}

inline bool ParseLeadingNumber(const char *&cursor, int &value) {
  if ((cursor == nullptr) || (std::isdigit(static_cast<unsigned char>(*cursor)) == 0)) {
    return false;
  }
  int result = 0;
  constexpr int base = 10;
  while (std::isdigit(static_cast<unsigned char>(*cursor)) != 0) {
    result = (result * base) + (static_cast<unsigned char>(*cursor) - static_cast<unsigned char>('0'));
    ++cursor;
  }
  value = result;
  return true;
}

inline std::string ParsePythonTag(const char *version) {
  if (version == nullptr) {
    return "";
  }
  const char *cursor = version;
  int major = 0;
  int minor = 0;
  if (!ParseLeadingNumber(cursor, major) || (*cursor != '.')) {
    return "";
  }
  ++cursor;
  if (!ParseLeadingNumber(cursor, minor)) {
    return "";
  }
  return "cp" + std::to_string(major) + std::to_string(minor);
}

inline PythonRuntimeKey ResolveLoadedPythonRuntimeKey() {
  using PyIsInitializedFn = int (*)();
  using PyGetVersionFn = const char *(*)();
  auto *py_is_initialized = reinterpret_cast<PyIsInitializedFn>(dlsym(RTLD_DEFAULT, kPyIsInitializedSymbol));
  auto *py_get_version = reinterpret_cast<PyGetVersionFn>(dlsym(RTLD_DEFAULT, kPyGetVersionSymbol));
  PythonRuntimeKey runtime_key;
  runtime_key.has_python_symbols = (py_is_initialized != nullptr) && (py_get_version != nullptr);
  if (!runtime_key.has_python_symbols) {
    runtime_key.source = "unresolved(no loaded Py_* symbols)";
    return runtime_key;
  }
  runtime_key.source = "loaded Py_* symbols";
  runtime_key.is_initialized = (py_is_initialized() != 0);
  const char *version = py_get_version();
  runtime_key.version = (version == nullptr) ? "" : version;
  runtime_key.python_tag = ParsePythonTag(version);
  return runtime_key;
}

inline std::string DirName(const std::string &path) {
  const auto pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return ".";
  }
  if (pos == 0U) {
    return "/";
  }
  return path.substr(0U, pos);
}

inline std::string BaseName(const std::string &path) {
  const auto pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return path;
  }
  return path.substr(pos + 1U);
}

inline std::string JoinPath(const std::string &dir, const std::string &name) {
  if (dir.empty()) {
    return name;
  }
  if (dir.back() == '/') {
    return dir + name;
  }
  return dir + "/" + name;
}

inline std::string CurrentPlatformTag() {
  struct utsname uts{};
  if ((uname(&uts) != 0) || (uts.machine[0] == '\0')) {
    return "linux-unknown";
  }
  return "linux-" + std::string(uts.machine);
}

inline bool ReadTextFile(const std::string &file_path, std::string &content) {
  const auto real_path = RealPath(file_path.c_str());
  if (real_path.empty()) {
    return false;
  }
  std::ifstream input(real_path);
  if (!input.is_open()) {
    return false;
  }
  content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
  return true;
}

inline bool GetJsonString(const nlohmann::json &json_obj, const char *field_name, std::string &value) {
  const auto iter = json_obj.find(field_name);
  if ((iter == json_obj.end()) || (!iter->is_string())) {
    return false;
  }
  value = iter->get<std::string>();
  return !value.empty();
}

inline bool GetJsonUint32(const nlohmann::json &json_obj, const char *field_name, uint32_t &value) {
  const auto iter = json_obj.find(field_name);
  if ((iter == json_obj.end()) || (!iter->is_number_unsigned())) {
    return false;
  }
  value = iter->get<uint32_t>();
  return true;
}

inline bool LoadArtifactManifest(const std::string &manifest_path, PythonArtifactSet &artifact) {
  std::string manifest_content;
  if (!ReadTextFile(manifest_path, manifest_content)) {
    return false;
  }

  nlohmann::json manifest;
  try {
    manifest = nlohmann::json::parse(manifest_content);
  } catch (const nlohmann::json::exception &err) {
    (void)err;
    return false;
  }

  const auto real_manifest_path = RealPath(manifest_path.c_str());
  artifact = PythonArtifactSet{};
  artifact.manifest_path = real_manifest_path;
  artifact.root = DirName(real_manifest_path);
  std::string bridge_rel_path;
  std::string native_rel_path;
  if (!GetJsonString(manifest, "python_tag", artifact.python_tag) ||
      !GetJsonString(manifest, "platform", artifact.platform) ||
      !GetJsonUint32(manifest, "bridge_abi", artifact.bridge_abi)) {
    return false;
  }
  const auto artifact_iter = manifest.find("artifacts");
  if ((artifact_iter == manifest.end()) || (!artifact_iter->is_object()) ||
      !GetJsonString(*artifact_iter, "bridge", bridge_rel_path) ||
      !GetJsonString(*artifact_iter, "native", native_rel_path)) {
    return false;
  }

  artifact.bridge_path = RealPath(JoinPath(artifact.root, bridge_rel_path).c_str());
  artifact.native_module_path = RealPath(JoinPath(artifact.root, native_rel_path).c_str());
  if (artifact.bridge_path.empty() || artifact.native_module_path.empty()) {
    return false;
  }
  return true;
}

inline void AppendManifestCandidatesFromRoot(const std::string &root_path, std::vector<std::string> &manifest_paths) {
  const auto real_root = RealPath(root_path.c_str());
  if (real_root.empty()) {
    return;
  }

  const auto root_manifest_path = JoinPath(real_root, kArtifactManifestName);
  if (!RealPath(root_manifest_path.c_str()).empty()) {
    manifest_paths.emplace_back(root_manifest_path);
  }

  DIR *dir = opendir(real_root.c_str());
  if (dir == nullptr) {
    return;
  }
  while (true) {
    const auto *entry = readdir(dir);
    if (entry == nullptr) {
      break;
    }
    const std::string name = entry->d_name;
    if ((name == ".") || (name == "..")) {
      continue;
    }
    const auto child_manifest_path = JoinPath(JoinPath(real_root, name), kArtifactManifestName);
    if (!RealPath(child_manifest_path.c_str()).empty()) {
      manifest_paths.emplace_back(child_manifest_path);
    }
  }
  (void)closedir(dir);
}

inline void AppendUniquePath(const std::string &path, std::vector<std::string> &paths) {
  const auto real_path = RealPath(path.c_str());
  if (real_path.empty()) {
    return;
  }
  for (const auto &existing_path : paths) {
    if (existing_path == real_path) {
      return;
    }
  }
  paths.emplace_back(real_path);
}

inline void AppendPythonPathGePackageDirCandidate(const std::string &python_path_entry,
                                                  std::vector<std::string> &ge_package_dirs) {
  if (python_path_entry.empty()) {
    return;
  }
  if (BaseName(python_path_entry) == "ge") {
    AppendUniquePath(python_path_entry, ge_package_dirs);
  }
  AppendUniquePath(JoinPath(python_path_entry, "ge"), ge_package_dirs);
}

inline void AppendPythonPathGePackageDirCandidates(std::vector<std::string> &ge_package_dirs) {
  const char *python_path = std::getenv(kPythonPathEnvName);
  if ((python_path == nullptr) || (python_path[0] == '\0')) {
    return;
  }

  const std::string paths(python_path);
  size_t start = 0U;
  while (start <= paths.size()) {
    const auto end = paths.find(':', start);
    AppendPythonPathGePackageDirCandidate(paths.substr(start, end - start), ge_package_dirs);
    if (end == std::string::npos) {
      break;
    }
    start = end + 1U;
  }
}

inline std::vector<std::string> BuildGePackageDirCandidates(const std::string &loader_library_path) {
  std::vector<std::string> ge_package_dirs;
  if (!loader_library_path.empty()) {
    const auto lib_dir = DirName(loader_library_path);
    const auto install_root = DirName(lib_dir);
    AppendUniquePath(JoinPath(install_root, kGePackageRelativePath), ge_package_dirs);
    if (BaseName(lib_dir) == "lib64") {
      // GE run packages put C++ libs under <cann>/<arch>/lib64 and Python packages under <cann>/python.
      AppendUniquePath(JoinPath(DirName(install_root), kGePackageRelativePath), ge_package_dirs);
    }
    if (BaseName(lib_dir) == "_capi") {
      AppendUniquePath(DirName(lib_dir), ge_package_dirs);
    }
    if (BaseName(lib_dir) == "ge") {
      AppendUniquePath(lib_dir, ge_package_dirs);
    }
  }
  AppendPythonPathGePackageDirCandidates(ge_package_dirs);
  return ge_package_dirs;
}

inline std::vector<std::string> BuildArtifactManifestCandidates(const std::string &loader_library_path,
                                                                const std::string &artifact_relative_path) {
  std::vector<std::string> manifest_paths;
  if (artifact_relative_path.empty()) {
    return manifest_paths;
  }
  for (const auto &ge_package_dir : BuildGePackageDirCandidates(loader_library_path)) {
    AppendManifestCandidatesFromRoot(JoinPath(ge_package_dir, artifact_relative_path), manifest_paths);
  }
  return manifest_paths;
}

inline void AppendMatchedArtifactCandidate(const PythonRuntimeKey &runtime_key, const PythonArtifactSet &artifact,
                                           const std::string &platform_tag, const uint32_t expected_bridge_abi,
                                           std::vector<BridgeLibraryCandidate> &candidates) {
  if (artifact.python_tag != runtime_key.python_tag) {
    return;
  }
  if (artifact.platform != platform_tag) {
    return;
  }
  if (artifact.bridge_abi != expected_bridge_abi) {
    return;
  }
  candidates.push_back(BridgeLibraryCandidate{
      artifact.bridge_path,
      artifact.root,
      artifact.native_module_path,
  });
}

inline BridgeLibraryCandidate LoadBridgeCandidateFromArtifactRoot(const std::string &artifact_root,
                                                                  const PythonRuntimeKey &runtime_key,
                                                                  const uint32_t expected_bridge_abi) {
  if (artifact_root.empty() || runtime_key.python_tag.empty()) {
    return BridgeLibraryCandidate{};
  }
  PythonArtifactSet artifact;
  if (!LoadArtifactManifest(JoinPath(artifact_root, kArtifactManifestName), artifact)) {
    return BridgeLibraryCandidate{};
  }
  std::vector<BridgeLibraryCandidate> candidates;
  AppendMatchedArtifactCandidate(runtime_key, artifact, CurrentPlatformTag(), expected_bridge_abi, candidates);
  if (candidates.empty()) {
    return BridgeLibraryCandidate{};
  }
  return candidates.front();
}

inline std::vector<BridgeLibraryCandidate> BuildPrebuiltBridgeLibraryCandidates(
    const PythonRuntimeKey &runtime_key, const std::string &loader_library_path,
    const std::string &artifact_relative_path, const uint32_t expected_bridge_abi) {
  std::vector<BridgeLibraryCandidate> candidates;
  if (runtime_key.python_tag.empty() || artifact_relative_path.empty()) {
    return candidates;
  }

  const auto platform_tag = CurrentPlatformTag();
  for (const auto &manifest_path : BuildArtifactManifestCandidates(loader_library_path, artifact_relative_path)) {
    PythonArtifactSet artifact_set;
    if (!LoadArtifactManifest(manifest_path, artifact_set)) {
      continue;
    }
    AppendMatchedArtifactCandidate(runtime_key, artifact_set, platform_tag, expected_bridge_abi, candidates);
  }
  return candidates;
}

}  // namespace python_artifact
}  // namespace ge

#endif  // BASE_COMMON_PYTHON_RUNTIME_PYTHON_ARTIFACT_UTILS_H_
