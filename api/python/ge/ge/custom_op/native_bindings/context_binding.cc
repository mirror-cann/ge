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
#include "exe_graph/runtime/eager_op_execution_context.h"
#include "runtime/native_bindings/runtime_type_wrappers.h"

#include <memory>
#include <stdexcept>

namespace ge {
namespace python_custom_op_native {
namespace {
namespace runtime_native = ::ge::python_runtime_native;

class BorrowedEagerOpExecutionContext {
 public:
  explicit BorrowedEagerOpExecutionContext(gert::EagerOpExecutionContext *ctx)
      : ctx_(ctx), valid_(std::make_shared<bool>(true)) {}

  py::object GetInputTensor(size_t index) const {
    const auto *tensor = Get()->GetInputTensor(index);
    return CastRequiredTensor(tensor, "Failed to get input tensor");
  }

  size_t GetInputNum() const {
    return Get()->GetComputeNodeInputNum();
  }

  py::object GetRequiredInputTensor(size_t ir_index) const {
    const auto *tensor = Get()->GetRequiredInputTensor(ir_index);
    return CastRequiredTensor(tensor, "Failed to get required input tensor");
  }

  py::object GetOptionalInputTensor(size_t ir_index) const {
    const auto *tensor = Get()->GetOptionalInputTensor(ir_index);
    if (tensor == nullptr) {
      return py::none();
    }
    return CastTensor(tensor);
  }

  py::object GetDynamicInputTensor(size_t ir_index, size_t relative_index) const {
    const auto *tensor = Get()->GetDynamicInputTensor(ir_index, relative_index);
    return CastRequiredTensor(tensor, "Failed to get dynamic input tensor");
  }

  py::object GetOutputTensor(size_t index) const {
    const auto *tensor = Get()->GetOutputTensor(index);
    return CastRequiredTensor(tensor, "Failed to get output tensor");
  }

  py::object MakeOutputRefInput(size_t output_index, size_t input_index) const {
    auto *tensor = Get()->MakeOutputRefInput(output_index, input_index);
    if (tensor == nullptr) {
      throw std::runtime_error("Failed to make output reference input");
    }
    return py::cast(runtime_native::NativeTensor::Borrow(tensor, valid_));
  }

  py::object MallocOutputTensor(size_t index, const py::object &shape_obj, const py::object &format_obj,
                                int32_t dtype) const {
    const auto &shape = shape_obj.cast<const runtime_native::NativeStorageShape &>();
    const auto &format = format_obj.cast<const runtime_native::NativeStorageFormat &>();
    auto *tensor = Get()->MallocOutputTensor(index, *shape.Get(), *format.Get(), static_cast<ge::DataType>(dtype));
    if (tensor == nullptr) {
      throw std::runtime_error("Failed to malloc output tensor");
    }
    return py::cast(runtime_native::NativeTensor::Borrow(tensor, valid_));
  }

  uintptr_t MallocWorkSpace(size_t size) const {
    auto *workspace = Get()->MallocWorkSpace(size);
    if (workspace == nullptr) {
      throw std::runtime_error("Failed to malloc workspace");
    }
    return reinterpret_cast<uintptr_t>(workspace);
  }

  uintptr_t GetStream() const {
    return reinterpret_cast<uintptr_t>(Get()->GetStream());
  }

  void Invalidate() {
    if (valid_ != nullptr) {
      *valid_ = false;
    }
    ctx_ = nullptr;
  }

 private:
  gert::EagerOpExecutionContext *Get() const {
    if ((valid_ == nullptr) || (!(*valid_)) || (ctx_ == nullptr)) {
      throw std::runtime_error("Borrowed native object has expired");
    }
    return ctx_;
  }

  py::object CastTensor(const gert::Tensor *tensor) const {
    return py::cast(runtime_native::NativeTensor::Borrow(const_cast<gert::Tensor *>(tensor), valid_));
  }

  py::object CastRequiredTensor(const gert::Tensor *tensor, const char *message) const {
    if (tensor == nullptr) {
      throw std::runtime_error(message);
    }
    return CastTensor(tensor);
  }

  gert::EagerOpExecutionContext *ctx_{nullptr};
  std::shared_ptr<bool> valid_;
};

BorrowedEagerOpExecutionContext BorrowEagerOpExecutionContext(uintptr_t ctx_handle) {
  if (ctx_handle == 0U) {
    throw std::invalid_argument("ctx_handle is null");
  }
  return BorrowedEagerOpExecutionContext(reinterpret_cast<gert::EagerOpExecutionContext *>(ctx_handle));
}

}  // namespace

void BindEagerOpExecutionContext(py::module_ &m) {
  py::class_<BorrowedEagerOpExecutionContext>(m, "EagerOpExecutionContext",
                                              "Borrowed view of gert::EagerOpExecutionContext")
      .def("get_input_tensor", &BorrowedEagerOpExecutionContext::GetInputTensor, py::arg("index"))
      .def("get_input_num", &BorrowedEagerOpExecutionContext::GetInputNum)
      .def("get_required_input_tensor", &BorrowedEagerOpExecutionContext::GetRequiredInputTensor, py::arg("ir_index"))
      .def("get_optional_input_tensor", &BorrowedEagerOpExecutionContext::GetOptionalInputTensor, py::arg("ir_index"))
      .def("get_dynamic_input_tensor", &BorrowedEagerOpExecutionContext::GetDynamicInputTensor, py::arg("ir_index"),
           py::arg("relative_index"))
      .def("malloc_output_tensor", &BorrowedEagerOpExecutionContext::MallocOutputTensor, py::arg("index"),
           py::arg("shape"), py::arg("format"), py::arg("dtype"))
      .def("make_output_ref_input", &BorrowedEagerOpExecutionContext::MakeOutputRefInput, py::arg("output_index"),
           py::arg("input_index"))
      .def("malloc_workspace", &BorrowedEagerOpExecutionContext::MallocWorkSpace, py::arg("size"))
      .def("get_output_tensor", &BorrowedEagerOpExecutionContext::GetOutputTensor, py::arg("index"))
      .def("get_stream", &BorrowedEagerOpExecutionContext::GetStream)
      .def("_invalidate", &BorrowedEagerOpExecutionContext::Invalidate);
  m.def("_borrow_eager_op_execution_context", &BorrowEagerOpExecutionContext, py::arg("ctx_handle"));
}

}  // namespace python_custom_op_native
}  // namespace ge
