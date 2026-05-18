/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string>
#include <vector>

#include "ge/ge_utils.h"
#include "ge_api_c_wrapper_utils.h"
#include "graph/gnode.h"

using namespace ge;
using namespace ge::c_wrapper;

#ifdef __cplusplus
extern "C" {
#endif

Status GeApiWrapper_GeUtils_InferShape(const Graph *graph, const int64_t *dims, size_t dims_num,
                                       const size_t *shape_ranks, size_t shape_num) {
  GE_ASSERT_NOTNULL(graph);
  if (dims_num != 0U) {
    GE_ASSERT_NOTNULL(dims);
  }
  if (shape_num != 0U) {
    GE_ASSERT_NOTNULL(shape_ranks);
  }

  size_t expected_dims_num = 0U;
  for (size_t i = 0U; i < shape_num; ++i) {
    expected_dims_num += shape_ranks[i];
  }
  if (expected_dims_num != dims_num) {
    return FAILED;
  }

  std::vector<Shape> input_shapes;
  input_shapes.reserve(shape_num);
  size_t dim_offset = 0U;
  for (size_t i = 0U; i < shape_num; ++i) {
    const auto rank = shape_ranks[i];
    std::vector<int64_t> input_shape;
    input_shape.reserve(rank);
    for (size_t j = 0U; j < rank; ++j) {
      input_shape.emplace_back(dims[dim_offset + j]);
    }
    input_shapes.emplace_back(input_shape);
    dim_offset += rank;
  }
  return GeUtils::InferShape(*graph, input_shapes);
}

Status GeApiWrapper_GeUtils_CheckNodeSupportOnAicore(const GNode *node, bool *is_supported, char **unsupported_reason) {
  GE_ASSERT_NOTNULL(node);
  GE_ASSERT_NOTNULL(is_supported);
  GE_ASSERT_NOTNULL(unsupported_reason);
  *is_supported = false;
  *unsupported_reason = nullptr;

  AscendString reason;
  const auto ret = GeUtils::CheckNodeSupportOnAicore(*node, *is_supported, reason);
  if (reason.GetString() != nullptr) {
    *unsupported_reason = MallocCopyString(reason.GetString());
  }
  return ret;
}

void GeApiWrapper_GeUtils_FreeString(char *p) {
  SafeFree(p);
}

#ifdef __cplusplus
}
#endif
