/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_GRAPH_UTILS_TENSOR_UTILS_EX_H_
#define INC_GRAPH_UTILS_TENSOR_UTILS_EX_H_

#include "graph/ge_tensor.h"

namespace ge {
class TensorUtilsEx {
 public:
  /**
   * @brief Calculate tensor memory size with automatic alignment padding.
   *
   * This function calculates the tensor memory size with automatic alignment padding
   * added, ensuring the memory allocation meets hardware alignment requirements.
   * Compared to GetTensorMemorySizeInBytes, this function additionally considers
   * the actual SoC padding size.
   *
   * @param [in] desc_temp Tensor descriptor containing shape, data type, format, etc.
   * @param [out] size_temp Calculated memory size in bytes, including alignment padding.
   * @return ge::GRAPH_SUCCESS on success; ge::GRAPH_FAILED on failure.
   *
   * @note Recommended to use this function instead of GetTensorMemorySizeInBytes.
   */
  static ge::graphStatus GetTensorMemorySizeInBytesWithAutoPadding(const GeTensorDesc &desc_temp, int64_t &size_temp);

  /**
    * @brief Get the padding size for memory allocation.
    *
    * This function queries the SoC specification to get the actual padding size.
    * If the query fails, it returns the default padding size (32 bytes).
    * The result is cached for subsequent calls.
    *
    * @return The padding size in bytes.
    */
  static int64_t GetPaddingSize();
};
}  // namespace ge
#endif  // INC_GRAPH_UTILS_TENSOR_UTILS_EX_H_
