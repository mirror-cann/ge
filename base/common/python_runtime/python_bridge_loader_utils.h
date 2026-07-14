/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_PYTHON_RUNTIME_PYTHON_BRIDGE_LOADER_UTILS_H_
#define BASE_COMMON_PYTHON_RUNTIME_PYTHON_BRIDGE_LOADER_UTILS_H_

#include <dlfcn.h>

#include <cstdint>
#include <string>

#include "common/python_runtime/python_artifact_utils.h"

namespace ge {
namespace python_bridge_loader {

constexpr uint32_t kBridgeLoadSuccess = 0U;

template <typename BridgeApiT>
struct LoadedBridgeCandidate {
  void *handle{nullptr};
  const BridgeApiT *api{nullptr};
  std::string real_path;
};

struct BridgeLoadDependencies {
  using RealPathFn = std::string (*)(const char *path);
  using OpenLibraryFn = void *(*)(const char *path, int flags);
  using CloseLibraryFn = int (*)(void *handle);
  using LookupSymbolFn = void *(*)(void *handle, const char *symbol);
  using ResolveRuntimeKeyFn = python_artifact::PythonRuntimeKey (*)();

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

template <typename ArtifactConfigT>
inline ArtifactConfigT BuildArtifactConfig(const python_artifact::BridgeLibraryCandidate &candidate) {
  return ArtifactConfigT{
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

template <typename BridgeApiT, typename ArtifactConfigT>
BridgeLoadStatus TryLoadBridgeCandidate(const python_artifact::PythonRuntimeKey &expected_key,
                                        const python_artifact::BridgeLibraryCandidate &candidate,
                                        const BridgeLoadDependencies &deps,
                                        bool (*is_api_valid)(const BridgeApiT *api, uint32_t expected_abi),
                                        LoadedBridgeCandidate<BridgeApiT> &loaded_bridge) {
  loaded_bridge = LoadedBridgeCandidate<BridgeApiT>{};
  if (!IsBridgeLoadDependenciesValid(deps) || (is_api_valid == nullptr)) {
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
  if (!python_artifact::IsRuntimeKeyCompatible(expected_key, loaded_key)) {
    CloseLibraryQuietly(deps, handle);
    return BridgeLoadStatus::kRuntimeMismatch;
  }

  using GetApiFn = const BridgeApiT *(*)();
  auto *get_api = reinterpret_cast<GetApiFn>(deps.lookup_symbol(handle, deps.api_symbol));
  if (get_api == nullptr) {
    CloseLibraryQuietly(deps, handle);
    return BridgeLoadStatus::kMissingApiSymbol;
  }

  const auto *api = get_api();
  if (!is_api_valid(api, deps.expected_abi)) {
    CloseLibraryQuietly(deps, handle);
    return BridgeLoadStatus::kInvalidApi;
  }

  const auto artifact_config = BuildArtifactConfig<ArtifactConfigT>(candidate);
  if (api->set_artifact_config(&artifact_config) != kBridgeLoadSuccess) {
    CloseLibraryQuietly(deps, handle);
    return BridgeLoadStatus::kSetArtifactConfigFailed;
  }

  loaded_bridge.handle = handle;
  loaded_bridge.api = api;
  loaded_bridge.real_path = real_path;
  return BridgeLoadStatus::kSuccess;
}

}  // namespace python_bridge_loader
}  // namespace ge

#endif  // BASE_COMMON_PYTHON_RUNTIME_PYTHON_BRIDGE_LOADER_UTILS_H_
