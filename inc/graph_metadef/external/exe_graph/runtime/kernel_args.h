/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef METADEF_INC_EXE_GRAPH_RUNTIME_KERNEL_ARGS_H_
#define METADEF_INC_EXE_GRAPH_RUNTIME_KERNEL_ARGS_H_

#include <cstddef>
#include <cstdint>
#include "graph/types.h"

namespace gert {

using Placement = ge::Placement;

/// Kernel launch arguments with memory placement info
struct KernelArgs {
  void *args_data;      ///< Pointer to argument data
  size_t args_size;     ///< Size of argument data in bytes
  uint64_t reserved[4]; ///< Reserved for future extension
  Placement placement;  ///< Memory placement (host or device)
  uint8_t placement_reserved_[4]; ///< Reserved field, 4-byte aligned for Placement

  KernelArgs()
      : args_data(nullptr), args_size(0U), reserved{},
        placement(ge::kPlacementHost), placement_reserved_{} {}
};

}  // namespace gert

#endif  // METADEF_INC_EXE_GRAPH_RUNTIME_KERNEL_ARGS_H_
