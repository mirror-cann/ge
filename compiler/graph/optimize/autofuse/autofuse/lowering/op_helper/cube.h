/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_COMPILER_GRAPH_OPTIMIZE_AUTOFUSE_LOWERING_CUBE_H_
#define AIR_CXX_COMPILER_GRAPH_OPTIMIZE_AUTOFUSE_LOWERING_CUBE_H_

#include <cstdint>
#include <string>
#include <vector>
#include "graph/types.h"

namespace ge {
struct MatMulAttr {
  int64_t offset_x{0};
  ge::DataType output_dtype{ge::DataType::DT_MAX};
  bool transpose_x1{false};
  bool transpose_x2{false};
  int64_t enable_hf32{0x1};
  bool has_bias{false};
  bool has_offset_w{false};
  bool adj_x1{false};
  bool adj_x2{false};
  bool is_batch{false};
};

struct Conv2DAttr {
  ge::DataType output_dtype = ge::DataType::DT_MAX;
  std::vector<int64_t> strides;
  std::vector<int64_t> pads;
  std::vector<int64_t> dilations;
  int64_t groups = 1;
  std::string data_format = "NCHW";
  int64_t offset_x = 0;
  std::string pad_mode = "SPECIFIC";
  bool enable_hf32 = false;
  bool has_bias = false;
  bool has_offset_w = false;
};
}  // namespace ge

#endif  // AIR_CXX_COMPILER_GRAPH_OPTIMIZE_AUTOFUSE_LOWERING_CUBE_H_