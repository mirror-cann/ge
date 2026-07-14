/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_BRIDGE_C_API_H_
#define CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_BRIDGE_C_API_H_

#include "ge/ge_api_types.h"

namespace ge {
namespace custom_op {
struct PythonCustomOpDescriptor;
struct PythonCustomOpCallbacks;

struct PythonCustomOpRegistrar {
  bool (*register_custom_op)(const PythonCustomOpDescriptor *desc, const PythonCustomOpCallbacks *callbacks);
};

struct PythonCustomOpBridgeArtifactConfig {
  const char *artifact_root;
  const char *native_module_path;
};

struct PythonCustomOpBridgeApi {
  uint32_t abi_version;
  Status (*set_artifact_config)(const PythonCustomOpBridgeArtifactConfig *config);
  Status (*register_custom_ops)(const PythonCustomOpRegistrar *registrar);
  void (*reset_bridge_state)();
  void (*shutdown_bridge)();
};

constexpr uint32_t kPythonCustomOpBridgeAbiVersion = 1U;
constexpr const char *kPythonCustomOpBridgeGetApiSymbol = "GeGetPythonCustomOpBridgeApi";
}  // namespace custom_op
}  // namespace ge

extern "C" __attribute__((visibility("default"))) const ge::custom_op::PythonCustomOpBridgeApi *
GeGetPythonCustomOpBridgeApi();

#endif  // CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_BRIDGE_C_API_H_
