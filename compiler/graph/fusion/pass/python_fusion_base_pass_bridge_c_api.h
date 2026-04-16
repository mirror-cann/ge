/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_PYTHON_FUSION_BASE_PASS_BRIDGE_C_API_H
#define CANN_GRAPH_ENGINE_PYTHON_FUSION_BASE_PASS_BRIDGE_C_API_H

#include <cstdint>

#include "python_fusion_base_pass_adapter.h"
#include "python_fusion_base_pass_pybind_bridge.h"

namespace ge {
namespace fusion {
struct PythonFusionBasePassRegistrar {
  bool (*register_pass)(const PythonPassDescriptor *pass_desc,
                        const PythonFusionBasePassCallbacks *callbacks);
};

struct PythonFusionBasePassBridgeApi {
  uint32_t abi_version;
  Status (*register_fusion_base_passes)(const PythonFusionBasePassRegistrar *registrar);
  void (*reset_bridge_state)();
  void (*shutdown_bridge)();
};

constexpr uint32_t kPythonFusionBasePassBridgeAbiVersion = 1U;
constexpr const char *kPythonFusionBasePassBridgeGetApiSymbol = "GeGetPythonFusionBasePassBridgeApi";
}  // namespace fusion
}  // namespace ge

extern "C" __attribute__((visibility("default")))
const ge::fusion::PythonFusionBasePassBridgeApi *GeGetPythonFusionBasePassBridgeApi();

#endif  // CANN_GRAPH_ENGINE_PYTHON_FUSION_BASE_PASS_BRIDGE_C_API_H
