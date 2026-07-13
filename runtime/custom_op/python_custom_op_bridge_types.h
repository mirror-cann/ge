/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_BRIDGE_TYPES_H_
#define CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_BRIDGE_TYPES_H_

#include <string>

#include "graph/custom_op/capability.h"
#include "graph/ge_error_codes.h"

namespace gert {
class EagerOpExecutionContext;
}

namespace ge {
namespace custom_op {
struct PythonCustomOpDescriptor {
  std::string descriptor_key;
  std::string op_type;
  CustomOpCapabilityMask capabilities{0U};
};

using PythonCustomOpHolderCreateFn = void *(*)(const PythonCustomOpDescriptor *desc);
using PythonCustomOpHolderDestroyFn = void (*)(void *holder);
using PythonCustomOpExecuteFn = graphStatus (*)(const void *holder, gert::EagerOpExecutionContext *ctx);

struct PythonCustomOpCallbacks {
  PythonCustomOpHolderCreateFn create{nullptr};
  PythonCustomOpHolderDestroyFn destroy{nullptr};
  PythonCustomOpExecuteFn execute{nullptr};

  bool IsValid(CustomOpCapabilityMask capabilities) const {
    const auto supported_capabilities = static_cast<CustomOpCapabilityMask>(CustomOpCapability::kEagerExecute);
    if ((capabilities == 0U) || ((capabilities & (~supported_capabilities)) != 0U)) {
      return false;
    }
    if ((create == nullptr) || (destroy == nullptr)) {
      return false;
    }
    if (HasCustomOpCapability(capabilities, CustomOpCapability::kEagerExecute) && (execute == nullptr)) {
      return false;
    }
    return true;
  }
};
}  // namespace custom_op
}  // namespace ge

#endif  // CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_BRIDGE_TYPES_H_
