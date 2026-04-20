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

#include <cstdint>

#include "python_pass_adapter.h"
#include "python_pass_pybind_bridge.h"

namespace ge {
namespace fusion {
struct PythonFusionPassRegistrar {
  bool (*register_pass)(const PythonPassDescriptor *pass_desc,
                        const PythonFusionPassCallbacks *callbacks);
};

struct PythonFusionPassBridgeApi {
  uint32_t abi_version;
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
