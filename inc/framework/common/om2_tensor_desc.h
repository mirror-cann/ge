/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_FRAMEWORK_COMMON_OM2_TENSOR_DESC_H_
#define AIR_CXX_FRAMEWORK_COMMON_OM2_TENSOR_DESC_H_

#include <vector>
#include <string>
#include <utility>

#include "framework/common/ge_visibility.h"
#include "graph/types.h"

namespace ge {

class VISIBILITY_EXPORT Om2TensorDesc {
 public:
  Om2TensorDesc();
  ~Om2TensorDesc() = default;

  Om2TensorDesc(const Om2TensorDesc &) = default;
  Om2TensorDesc(Om2TensorDesc &&) = default;
  Om2TensorDesc &operator=(const Om2TensorDesc &) = default;
  Om2TensorDesc &operator=(Om2TensorDesc &&) = default;

  // Setters
  void SetDataType(DataType data_type);
  void SetFormat(Format format);
  void SetShape(const std::vector<int64_t> &dims);
  void SetName(const std::string &name);
  void SetShapeRange(const std::vector<std::pair<int64_t, int64_t>> &shape_range);
  void SetSize(size_t size);

  // Getters
  DataType GetDataType() const;
  Format GetFormat() const;
  const std::vector<int64_t> &GetShape() const;
  const std::string &GetName() const;
  const std::vector<std::pair<int64_t, int64_t>> &GetShapeRange() const;
  size_t GetSize() const;

  // Additional getters for compatibility
  Format GetOriginFormat() const;
  const std::vector<int64_t> &GetOriginShape() const;

  // Computation methods
  int64_t GetElementNum() const;
  size_t GetByteSize() const;

 private:
 DataType data_type_ = DT_UNDEFINED;
 Format format_ = FORMAT_ND;
  std::vector<int64_t> dims_;
  std::string name_;
  std::vector<std::pair<int64_t, int64_t>> shape_range_;
  size_t size_;
};

}  // namespace ge

#endif  // AIR_CXX_FRAMEWORK_COMMON_OM2_TENSOR_DESC_H_

