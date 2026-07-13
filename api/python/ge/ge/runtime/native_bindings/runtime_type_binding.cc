/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "bindings.h"
#include "runtime_type_wrappers.h"

namespace ge {
namespace python_runtime_native {
namespace {
void BindTensorPlacement(py::module_ &m) {
  py::enum_<gert::TensorPlacement>(m, "TensorPlacement", "Placement of gert::Tensor data")
      .value("ON_DEVICE_HBM", gert::TensorPlacement::kOnDeviceHbm)
      .value("ON_HOST", gert::TensorPlacement::kOnHost)
      .value("FOLLOWING", gert::TensorPlacement::kFollowing)
      .value("ON_DEVICE_P2P", gert::TensorPlacement::kOnDeviceP2p)
      .value("END", gert::TensorPlacement::kTensorPlacementEnd);
}

void BindShape(py::module_ &m) {
  py::class_<NativeShape>(m, "Shape", "Python view of gert::Shape")
      .def("get_dim", &NativeShape::GetDim, py::arg("index"))
      .def("set_dim", &NativeShape::SetDim, py::arg("index"), py::arg("value"))
      .def("append_dim", &NativeShape::AppendDim, py::arg("value"), py::return_value_policy::reference_internal)
      .def("set_scalar", &NativeShape::SetScalar)
      .def("is_scalar", &NativeShape::IsScalar)
      .def_property_readonly("dims", &NativeShape::GetDims)
      .def_property_readonly("dim_num", &NativeShape::GetDimNum)
      .def_property_readonly("size", &NativeShape::GetSize);
}

void BindStorageShape(py::module_ &m) {
  py::class_<NativeStorageShape>(m, "StorageShape", "Python view of gert::StorageShape")
      .def(py::init<>())
      .def(py::init<const std::vector<int64_t> &, const std::vector<int64_t> &>(), py::arg("origin_shape"),
           py::arg("storage_shape"))
      .def("set_origin_shape", &NativeStorageShape::SetOriginShape, py::arg("shape"))
      .def("set_storage_shape", &NativeStorageShape::SetStorageShape, py::arg("shape"))
      .def_property_readonly("origin_shape", &NativeStorageShape::GetOriginShape)
      .def_property_readonly("storage_shape", &NativeStorageShape::GetStorageShape);
}

void BindExpandDimsType(py::module_ &m) {
  py::class_<NativeExpandDimsType>(m, "ExpandDimsType", "Python view of gert::ExpandDimsType")
      .def(py::init<>())
      .def(py::init<const std::string &>(), py::arg("rule"))
      .def(py::init<int64_t>(), py::arg("rule"))
      .def("is_expand_index", &NativeExpandDimsType::IsExpandIndex, py::arg("index"))
      .def("set_expand_index", &NativeExpandDimsType::SetExpandIndex, py::arg("index"))
      .def_property_readonly("full_size", &NativeExpandDimsType::GetFullSize);
}

void BindStorageFormat(py::module_ &m) {
  py::class_<NativeStorageFormat>(m, "StorageFormat", "Python view of gert::StorageFormat")
      .def(py::init<>())
      .def(py::init<int32_t, int32_t, const NativeExpandDimsType &>(), py::arg("origin_format"),
           py::arg("storage_format"), py::arg("expand_dims_type"))
      .def("set_origin_format", &NativeStorageFormat::SetOriginFormat, py::arg("value"))
      .def("set_storage_format", &NativeStorageFormat::SetStorageFormat, py::arg("value"))
      .def("set_expand_dims_type", &NativeStorageFormat::SetExpandDimsType, py::arg("value"))
      .def_property_readonly("origin_format", &NativeStorageFormat::GetOriginFormat)
      .def_property_readonly("storage_format", &NativeStorageFormat::GetStorageFormat)
      .def_property_readonly("expand_dims_type", &NativeStorageFormat::GetExpandDimsType);
}

void BindTensor(py::module_ &m) {
  py::class_<NativeTensor>(m, "Tensor", "Python view of gert::Tensor")
      .def_property_readonly("addr", &NativeTensor::GetAddr)
      .def_property_readonly("size", &NativeTensor::GetSize)
      .def_property_readonly("shape_size", &NativeTensor::GetShapeSize)
      .def_property_readonly("shape", &NativeTensor::GetShape)
      .def_property_readonly("storage_shape", &NativeTensor::GetStorageShape)
      .def_property_readonly("origin_shape", &NativeTensor::GetOriginShape)
      .def_property_readonly("format", &NativeTensor::GetFormat)
      .def_property_readonly("storage_format", &NativeTensor::GetStorageFormat)
      .def_property_readonly("origin_format", &NativeTensor::GetOriginFormat)
      .def_property_readonly("expand_dims_type", &NativeTensor::GetExpandDimsType)
      .def_property_readonly("data_type", &NativeTensor::GetDataType)
      .def_property_readonly("placement", &NativeTensor::GetPlacement);
}
}  // namespace

void BindRuntimeTypes(py::module_ &m) {
  BindTensorPlacement(m);
  BindShape(m);
  BindStorageShape(m);
  BindExpandDimsType(m);
  BindStorageFormat(m);
  BindTensor(m);
}

}  // namespace python_runtime_native
}  // namespace ge
