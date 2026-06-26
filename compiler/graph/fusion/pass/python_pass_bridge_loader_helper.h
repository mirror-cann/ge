/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_COMPILER_GRAPH_FUSION_PASS_PYTHON_PASS_BRIDGE_LOADER_HELPER_H_
#define GE_COMPILER_GRAPH_FUSION_PASS_PYTHON_PASS_BRIDGE_LOADER_HELPER_H_

#include <cstdint>
#include <string>

#include "ge/ge_api_error_codes.h"
#include "python_pass_artifact_selector.h"
#include "python_pass_bridge_c_api.h"

namespace ge {
namespace fusion {
namespace python_pass_bridge_loader {

struct LoadedBridgeCandidate {
  void *handle{nullptr};
  const PythonFusionPassBridgeApi *api{nullptr};
  std::string real_path;
};

struct BridgeLoadDependencies {
  using RealPathFn = std::string (*)(const char *path);
  using OpenLibraryFn = void *(*)(const char *path, int flags);
  using CloseLibraryFn = int (*)(void *handle);
  using LookupSymbolFn = void *(*)(void *handle, const char *symbol);
  using ResolveRuntimeKeyFn = python_pass_artifact::PythonRuntimeKey (*)();

  RealPathFn real_path{nullptr};
  OpenLibraryFn open_library{nullptr};
  CloseLibraryFn close_library{nullptr};
  LookupSymbolFn lookup_symbol{nullptr};
  ResolveRuntimeKeyFn resolve_runtime_key{nullptr};
  const char *api_symbol{nullptr};
  uint32_t expected_abi{0U};
  int open_flags{0};
};

enum class BridgeLoadStatus {
  kSuccess,
  kInvalidDependency,
  kInvalidPath,
  kOpenFailed,
  kRuntimeMismatch,
  kMissingApiSymbol,
  kInvalidApi,
  kSetArtifactConfigFailed,
};

inline const char *BridgeLoadStatusToString(const BridgeLoadStatus status) {
  switch (status) {
    case BridgeLoadStatus::kSuccess:
      return "success";
    case BridgeLoadStatus::kInvalidDependency:
      return "invalid dependency";
    case BridgeLoadStatus::kInvalidPath:
      return "invalid path";
    case BridgeLoadStatus::kOpenFailed:
      return "open failed";
    case BridgeLoadStatus::kRuntimeMismatch:
      return "runtime mismatch";
    case BridgeLoadStatus::kMissingApiSymbol:
      return "missing api symbol";
    case BridgeLoadStatus::kInvalidApi:
      return "invalid api";
    case BridgeLoadStatus::kSetArtifactConfigFailed:
      return "set artifact config failed";
    default:
      return "unknown";
  }
}

inline std::string BuildBridgeLoadErrorSuffix(const BridgeLoadStatus status, const char *open_error) {
  if ((status != BridgeLoadStatus::kOpenFailed) || (open_error == nullptr) || (open_error[0] == '\0')) {
    return "";
  }
  return std::string(", dlerror[") + open_error + "]";
}

inline std::string FirstLine(const std::string &content) {
  const auto pos = content.find('\n');
  return (pos == std::string::npos) ? content : content.substr(0U, pos);
}

inline std::string SecondLine(const std::string &content) {
  const auto first_end = content.find('\n');
  if (first_end == std::string::npos) {
    return "";
  }
  const auto second_end = content.find('\n', first_end + 1U);
  if (second_end == std::string::npos) {
    return content.substr(first_end + 1U);
  }
  return content.substr(first_end + 1U, second_end - first_end - 1U);
}

inline bool IsRuntimeKeyCompatible(const python_pass_artifact::PythonRuntimeKey &expected_key,
                                   const python_pass_artifact::PythonRuntimeKey &loaded_key) {
  return expected_key.python_tag.empty() || loaded_key.python_tag.empty() ||
         (loaded_key.python_tag == expected_key.python_tag);
}

inline bool IsBridgeApiValid(const PythonFusionPassBridgeApi *api, const uint32_t expected_abi) {
  return (api != nullptr) && (api->abi_version == expected_abi) && (api->set_artifact_config != nullptr) &&
         (api->register_passes != nullptr) && (api->reset_bridge_state != nullptr) && (api->shutdown_bridge != nullptr);
}

inline PythonFusionPassBridgeArtifactConfig BuildArtifactConfig(
    const python_pass_artifact::BridgeLibraryCandidate &candidate) {
  return PythonFusionPassBridgeArtifactConfig{
      candidate.artifact_root.empty() ? nullptr : candidate.artifact_root.c_str(),
      candidate.native_module_path.empty() ? nullptr : candidate.native_module_path.c_str(),
  };
}

inline bool IsBridgeLoadDependenciesValid(const BridgeLoadDependencies &deps) {
  return (deps.real_path != nullptr) && (deps.open_library != nullptr) && (deps.close_library != nullptr) &&
         (deps.lookup_symbol != nullptr) && (deps.resolve_runtime_key != nullptr) && (deps.api_symbol != nullptr);
}

inline void CloseLibraryQuietly(const BridgeLoadDependencies &deps, void *handle) {
  if ((handle != nullptr) && (deps.close_library != nullptr)) {
    (void)deps.close_library(handle);
  }
}

inline BridgeLoadStatus TryLoadBridgeCandidate(const python_pass_artifact::PythonRuntimeKey &expected_key,
                                               const python_pass_artifact::BridgeLibraryCandidate &candidate,
                                               const BridgeLoadDependencies &deps,
                                               LoadedBridgeCandidate &loaded_bridge) {
  loaded_bridge = LoadedBridgeCandidate{};
  if (!IsBridgeLoadDependenciesValid(deps)) {
    return BridgeLoadStatus::kInvalidDependency;
  }

  const auto real_path = deps.real_path(candidate.bridge_path.c_str());
  if (real_path.empty()) {
    return BridgeLoadStatus::kInvalidPath;
  }

  void *handle = deps.open_library(real_path.c_str(), deps.open_flags);
  if (handle == nullptr) {
    return BridgeLoadStatus::kOpenFailed;
  }

  const auto loaded_key = deps.resolve_runtime_key();
  if (!IsRuntimeKeyCompatible(expected_key, loaded_key)) {
    CloseLibraryQuietly(deps, handle);
    return BridgeLoadStatus::kRuntimeMismatch;
  }

  using GetApiFn = const PythonFusionPassBridgeApi *(*)();
  auto *get_api = reinterpret_cast<GetApiFn>(deps.lookup_symbol(handle, deps.api_symbol));
  if (get_api == nullptr) {
    CloseLibraryQuietly(deps, handle);
    return BridgeLoadStatus::kMissingApiSymbol;
  }

  const auto *api = get_api();
  if (!IsBridgeApiValid(api, deps.expected_abi)) {
    CloseLibraryQuietly(deps, handle);
    return BridgeLoadStatus::kInvalidApi;
  }

  const auto artifact_config = BuildArtifactConfig(candidate);
  if (api->set_artifact_config(&artifact_config) != SUCCESS) {
    CloseLibraryQuietly(deps, handle);
    return BridgeLoadStatus::kSetArtifactConfigFailed;
  }

  loaded_bridge.handle = handle;
  loaded_bridge.api = api;
  loaded_bridge.real_path = real_path;
  return BridgeLoadStatus::kSuccess;
}

}  // namespace python_pass_bridge_loader
}  // namespace fusion
}  // namespace ge

#endif  // GE_COMPILER_GRAPH_FUSION_PASS_PYTHON_PASS_BRIDGE_LOADER_HELPER_H_
