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

#include "python_pass_bridge_c_api.h"

namespace ge {
namespace fusion {
namespace python_pass_bridge_loader {

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

inline bool IsBridgeApiValid(const PythonFusionPassBridgeApi *api, const uint32_t expected_abi) {
  return (api != nullptr) && (api->abi_version == expected_abi) && (api->set_artifact_config != nullptr) &&
         (api->register_passes != nullptr) && (api->reset_bridge_state != nullptr) && (api->shutdown_bridge != nullptr);
}

}  // namespace python_pass_bridge_loader
}  // namespace fusion
}  // namespace ge

#endif  // GE_COMPILER_GRAPH_FUSION_PASS_PYTHON_PASS_BRIDGE_LOADER_HELPER_H_
