/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef METADEF_CXX_INC_GRAPH_CUSTOM_OP_CAPABILITY_H_
#define METADEF_CXX_INC_GRAPH_CUSTOM_OP_CAPABILITY_H_

#include <cstdint>

namespace ge {
enum class CustomOpCapability : uint32_t {
  kEagerExecute = 1U << 0U,
  kCompilable = 1U << 1U,
  kShapeInfer = 1U << 2U,
  kPortable = 1U << 3U,
  kArgsUpdater = 1U << 4U,
  kAnnotatedArgs = 1U << 5U,
};

using CustomOpCapabilityMask = uint32_t;

inline bool HasCustomOpCapability(CustomOpCapabilityMask capabilities, CustomOpCapability capability) {
  return (capabilities & static_cast<CustomOpCapabilityMask>(capability)) != 0U;
}

inline void AddCustomOpCapability(CustomOpCapabilityMask &capabilities, CustomOpCapability capability) {
  capabilities |= static_cast<CustomOpCapabilityMask>(capability);
}
}  // namespace ge

#endif  // METADEF_CXX_INC_GRAPH_CUSTOM_OP_CAPABILITY_H_
