/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_MINIDAG_DAG_MINIDAG_DAG_TYPES_H_
#define GE_GRAPH_BUILD_DAG_MINIDAG_DAG_MINIDAG_DAG_TYPES_H_

#include <cstdint>

namespace minidag {
enum class graphStatus {
  SUCCESS = 0,
  FAILED = 1,
  INVALID_NODE = 2,
  INVALID_EDGE = 3,
  DUPLICATE_NODE = 4,
  NODE_NOT_FOUND = 5
};

struct DeviceResourceInfo {
  // Use -1 as default, means no limit.
  int64_t cube_core_num = -1;
  int64_t vector_core_num = -1;
};

constexpr int64_t INVALID_STREAM_ID = -1;
constexpr int64_t INVALID_TOPO_ID = -1;
}  // namespace minidag
#endif  // GE_GRAPH_BUILD_DAG_MINIDAG_DAG_MINIDAG_DAG_TYPES_H_
