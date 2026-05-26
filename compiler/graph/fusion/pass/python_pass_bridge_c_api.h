/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_PYTHON_PASS_BRIDGE_C_API_H
#define CANN_GRAPH_ENGINE_PYTHON_PASS_BRIDGE_C_API_H

#include "ge/ge_api_error_codes.h"
#include "ge/ge_api_types.h"

namespace ge {
namespace fusion {
struct PythonPassDescriptor;
struct PythonFusionPassCallbacks;

struct PythonFusionPassRegistrar {
  bool (*register_pass)(const PythonPassDescriptor *pass_desc,
                        const PythonFusionPassCallbacks *callbacks);
};

struct PythonFusionPassBridgeArtifactConfig {
  const char *artifact_root;
  const char *native_module_path;
};

struct PythonFusionPassBridgeApi {
  uint32_t abi_version;
  Status (*set_artifact_config)(const PythonFusionPassBridgeArtifactConfig *config);
  Status (*register_passes)(const PythonFusionPassRegistrar *registrar);
  void (*reset_bridge_state)();
  void (*shutdown_bridge)();
};

constexpr uint32_t kPythonFusionPassBridgeAbiVersion = 1U;
constexpr const char *kPythonFusionPassBridgeGetApiSymbol = "GeGetPythonFusionPassBridgeApi";
}  // namespace fusion
}  // namespace ge

extern "C" __attribute__((visibility("default")))
const ge::fusion::PythonFusionPassBridgeApi *GeGetPythonFusionPassBridgeApi();

#endif  // CANN_GRAPH_ENGINE_PYTHON_PASS_BRIDGE_C_API_H
