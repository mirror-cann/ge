/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/common/om2_tensor_desc.h"

namespace ge {

Om2TensorDesc::Om2TensorDesc() : size_(0) {}

void Om2TensorDesc::SetDataType(DataType data_type) {
  data_type_ = data_type;
}

void Om2TensorDesc::SetFormat(Format format) {
  format_ = format;
}

void Om2TensorDesc::SetShape(const std::vector<int64_t> &dims) {
  dims_ = dims;
}

void Om2TensorDesc::SetName(const std::string &name) {
  name_ = name;
}

void Om2TensorDesc::SetShapeRange(const std::vector<std::pair<int64_t, int64_t>> &shape_range) {
  shape_range_ = shape_range;
}

void Om2TensorDesc::SetSize(size_t size) {
  size_ = size;
}

DataType Om2TensorDesc::GetDataType() const {
  return data_type_;
}

Format Om2TensorDesc::GetFormat() const {
  return format_;
}

const std::vector<int64_t> &Om2TensorDesc::GetShape() const {
  return dims_;
}

const std::string &Om2TensorDesc::GetName() const {
  return name_;
}

const std::vector<std::pair<int64_t, int64_t>> &Om2TensorDesc::GetShapeRange() const {
  return shape_range_;
}

size_t Om2TensorDesc::GetSize() const {
  return size_;
}

int64_t Om2TensorDesc::GetElementNum() const {
  if (dims_.empty()) {
    return 0;
  }
  int64_t element_num = 1;
  for (int64_t dim : dims_) {
    if (dim < 0) {
      return 0;  // Dynamic dimension
    }
    element_num *= dim;
  }
  return element_num;
}

size_t Om2TensorDesc::GetByteSize() const {
  return size_;
}

Format Om2TensorDesc::GetOriginFormat() const {
  return format_;
}

const std::vector<int64_t> &Om2TensorDesc::GetOriginShape() const {
  return dims_;
}

}  // namespace ge
