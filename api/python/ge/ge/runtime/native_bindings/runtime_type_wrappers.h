/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef API_PYTHON_GE_GE_RUNTIME_NATIVE_BINDINGS_RUNTIME_TYPE_WRAPPERS_H_
#define API_PYTHON_GE_GE_RUNTIME_NATIVE_BINDINGS_RUNTIME_TYPE_WRAPPERS_H_

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "binding_common.h"
#include "exe_graph/runtime/shape.h"
#include "exe_graph/runtime/storage_format.h"
#include "exe_graph/runtime/storage_shape.h"
#include "exe_graph/runtime/tensor.h"
#include "graph/types.h"

namespace ge {
namespace python_runtime_native {
inline std::vector<int64_t> ShapeToDims(const gert::Shape &shape) {
  std::vector<int64_t> dims;
  const size_t dim_num = shape.GetDimNum();
  dims.reserve(dim_num);
  for (size_t index = 0U; index < dim_num; ++index) {
    dims.emplace_back(shape.GetDim(index));
  }
  return dims;
}

inline gert::Shape DimsToShape(const std::vector<int64_t> &dims) {
  if (dims.size() > gert::Shape::kMaxDimNum) {
    throw std::invalid_argument("shape dimension number exceeds gert::Shape::kMaxDimNum");
  }
  gert::Shape shape;
  for (const auto dim : dims) {
    (void)shape.AppendDim(dim);
  }
  return shape;
}

inline py::object MakeGraphTypeEnum(const char *enum_name, int32_t enum_value) {
  py::module_ graph_module = py::module_::import("ge.graph");
  return graph_module.attr(enum_name)(enum_value);
}

template <typename T>
class NativeObjectBase {
 public:
  NativeObjectBase(const NativeObjectBase &) = delete;
  NativeObjectBase &operator=(const NativeObjectBase &) = delete;
  NativeObjectBase(NativeObjectBase &&) = default;
  NativeObjectBase &operator=(NativeObjectBase &&) = default;
  virtual ~NativeObjectBase() {
    if (owns_validity_) {
      Invalidate();
    }
  }

  T *Get() const {
    EnsureValid();
    return ptr_;
  }

  const std::shared_ptr<bool> &GetValidity() const {
    EnsureValid();
    return valid_;
  }

 protected:
  explicit NativeObjectBase(std::unique_ptr<T> owned)
      : owned_(std::move(owned)), ptr_(owned_.get()), valid_(std::make_shared<bool>(true)), owns_validity_(true) {}

  NativeObjectBase(T *ptr, std::shared_ptr<bool> valid) : ptr_(ptr), valid_(std::move(valid)), owns_validity_(false) {}

  void Invalidate() {
    if (valid_ != nullptr) {
      *valid_ = false;
    }
    ptr_ = nullptr;
  }

  void EnsureValid() const {
    if ((valid_ == nullptr) || (!(*valid_)) || (ptr_ == nullptr)) {
      throw std::runtime_error("handle has expired");
    }
  }

 private:
  std::unique_ptr<T> owned_;
  T *ptr_{nullptr};
  std::shared_ptr<bool> valid_;
  bool owns_validity_{false};
};

class PYBIND11_EXPORT NativeShape : public NativeObjectBase<gert::Shape> {
 public:
  static NativeShape Borrow(gert::Shape *ptr, std::shared_ptr<bool> valid) {
    return NativeShape(ptr, std::move(valid));
  }

  std::vector<int64_t> GetDims() const {
    return ShapeToDims(*Get());
  }

  size_t GetDimNum() const {
    return Get()->GetDimNum();
  }

  int64_t GetDim(size_t index) const {
    const auto dim_num = Get()->GetDimNum();
    if (index >= dim_num) {
      throw std::invalid_argument("shape dimension index out of range");
    }
    return Get()->GetDim(index);
  }

  void SetDim(size_t index, int64_t value) const {
    if (index >= Get()->GetDimNum()) {
      throw std::invalid_argument("shape dimension index out of range");
    }
    Get()->SetDim(index, value);
  }

  NativeShape &AppendDim(int64_t value) {
    (void)Get()->AppendDim(value);
    return *this;
  }

  int64_t GetSize() const {
    return Get()->GetShapeSize();
  }

  bool IsScalar() const {
    return Get()->IsScalar();
  }

  void SetScalar() const {
    Get()->SetScalar();
  }

 private:
  NativeShape(gert::Shape *ptr, std::shared_ptr<bool> valid) : NativeObjectBase(ptr, std::move(valid)) {}
};

class PYBIND11_EXPORT NativeStorageShape : public NativeObjectBase<gert::StorageShape> {
 public:
  NativeStorageShape() : NativeObjectBase(std::make_unique<gert::StorageShape>()) {}

  NativeStorageShape(const std::vector<int64_t> &origin_shape, const std::vector<int64_t> &storage_shape)
      : NativeObjectBase(std::make_unique<gert::StorageShape>()) {
    Get()->MutableOriginShape() = DimsToShape(origin_shape);
    Get()->MutableStorageShape() = DimsToShape(storage_shape);
  }

  static NativeStorageShape Borrow(gert::StorageShape *ptr, std::shared_ptr<bool> valid) {
    return NativeStorageShape(ptr, std::move(valid));
  }

  NativeShape GetOriginShape() const {
    return NativeShape::Borrow(&Get()->MutableOriginShape(), GetValidity());
  }

  NativeShape GetStorageShape() const {
    return NativeShape::Borrow(&Get()->MutableStorageShape(), GetValidity());
  }

  void SetOriginShape(const NativeShape &shape) const {
    Get()->MutableOriginShape() = *shape.Get();
  }

  void SetStorageShape(const NativeShape &shape) const {
    Get()->MutableStorageShape() = *shape.Get();
  }

 private:
  NativeStorageShape(gert::StorageShape *ptr, std::shared_ptr<bool> valid) : NativeObjectBase(ptr, std::move(valid)) {}
};

class PYBIND11_EXPORT NativeExpandDimsType : public NativeObjectBase<gert::ExpandDimsType> {
 public:
  NativeExpandDimsType() : NativeObjectBase(std::make_unique<gert::ExpandDimsType>()) {}

  explicit NativeExpandDimsType(const std::string &rule)
      : NativeObjectBase(std::make_unique<gert::ExpandDimsType>(rule.empty() ? nullptr : rule.c_str())) {}

  explicit NativeExpandDimsType(int64_t rule) : NativeObjectBase(std::make_unique<gert::ExpandDimsType>(rule)) {}

  static NativeExpandDimsType Borrow(gert::ExpandDimsType *ptr, std::shared_ptr<bool> valid) {
    return NativeExpandDimsType(ptr, std::move(valid));
  }

  uint64_t GetFullSize() const {
    return Get()->GetFullSize();
  }

  bool IsExpandIndex(uint64_t index) const {
    return Get()->IsExpandIndex(index);
  }

  void SetExpandIndex(uint64_t index) const {
    Get()->SetExpandIndex(index);
  }

 private:
  NativeExpandDimsType(gert::ExpandDimsType *ptr, std::shared_ptr<bool> valid)
      : NativeObjectBase(ptr, std::move(valid)) {}
};

class PYBIND11_EXPORT NativeStorageFormat : public NativeObjectBase<gert::StorageFormat> {
 public:
  NativeStorageFormat()
      : NativeStorageFormat(static_cast<int32_t>(ge::FORMAT_ND), static_cast<int32_t>(ge::FORMAT_ND),
                            NativeExpandDimsType()) {}

  NativeStorageFormat(int32_t origin_format, int32_t storage_format, const NativeExpandDimsType &expand_dims_type)
      : NativeObjectBase(std::make_unique<gert::StorageFormat>(
            static_cast<ge::Format>(origin_format), static_cast<ge::Format>(storage_format), *expand_dims_type.Get())) {
  }

  static NativeStorageFormat Borrow(gert::StorageFormat *ptr, std::shared_ptr<bool> valid) {
    return NativeStorageFormat(ptr, std::move(valid));
  }

  py::object GetOriginFormat() const {
    return MakeGraphTypeEnum("Format", static_cast<int32_t>(Get()->GetOriginFormat()));
  }

  py::object GetStorageFormat() const {
    return MakeGraphTypeEnum("Format", static_cast<int32_t>(Get()->GetStorageFormat()));
  }

  NativeExpandDimsType GetExpandDimsType() const {
    return NativeExpandDimsType::Borrow(&Get()->MutableExpandDimsType(), GetValidity());
  }

  void SetOriginFormat(int32_t origin_format) const {
    Get()->SetOriginFormat(static_cast<ge::Format>(origin_format));
  }

  void SetStorageFormat(int32_t storage_format) const {
    Get()->SetStorageFormat(static_cast<ge::Format>(storage_format));
  }

  void SetExpandDimsType(const NativeExpandDimsType &expand_dims_type) const {
    Get()->SetExpandDimsType(*expand_dims_type.Get());
  }

 private:
  NativeStorageFormat(gert::StorageFormat *ptr, std::shared_ptr<bool> valid)
      : NativeObjectBase(ptr, std::move(valid)) {}
};

class PYBIND11_EXPORT NativeTensor : public NativeObjectBase<gert::Tensor> {
 public:
  static NativeTensor Borrow(gert::Tensor *ptr, std::shared_ptr<bool> valid) {
    return NativeTensor(ptr, std::move(valid));
  }

  uintptr_t GetAddr() const {
    return reinterpret_cast<uintptr_t>(Get()->GetAddr());
  }

  size_t GetSize() const {
    return Get()->GetSize();
  }

  int64_t GetShapeSize() const {
    return Get()->GetShapeSize();
  }

  NativeStorageShape GetShape() const {
    return NativeStorageShape::Borrow(&Get()->GetShape(), GetValidity());
  }

  NativeShape GetStorageShape() const {
    return NativeShape::Borrow(&Get()->MutableStorageShape(), GetValidity());
  }

  NativeShape GetOriginShape() const {
    return NativeShape::Borrow(&Get()->MutableOriginShape(), GetValidity());
  }

  NativeStorageFormat GetFormat() const {
    return NativeStorageFormat::Borrow(&Get()->MutableFormat(), GetValidity());
  }

  py::object GetStorageFormat() const {
    return MakeGraphTypeEnum("Format", static_cast<int32_t>(Get()->GetStorageFormat()));
  }

  py::object GetOriginFormat() const {
    return MakeGraphTypeEnum("Format", static_cast<int32_t>(Get()->GetOriginFormat()));
  }

  NativeExpandDimsType GetExpandDimsType() const {
    return NativeExpandDimsType::Borrow(&Get()->MutableFormat().MutableExpandDimsType(), GetValidity());
  }

  py::object GetDataType() const {
    return MakeGraphTypeEnum("DataType", static_cast<int32_t>(Get()->GetDataType()));
  }

  gert::TensorPlacement GetPlacement() const {
    return Get()->GetPlacement();
  }

 private:
  NativeTensor(gert::Tensor *ptr, std::shared_ptr<bool> valid) : NativeObjectBase(ptr, std::move(valid)) {}
};

}  // namespace python_runtime_native
}  // namespace ge

#endif  // API_PYTHON_GE_GE_RUNTIME_NATIVE_BINDINGS_RUNTIME_TYPE_WRAPPERS_H_
