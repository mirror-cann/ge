/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <memory>
#include <vector>

#include "binding_utils.h"
#include "bindings.h"
#include "ge/fusion/pattern.h"

namespace ge {
namespace python_pass_native {
namespace {
using ::ge::fusion::Pattern;

class NativePattern {
 public:
  explicit NativePattern(py::object graph_obj) {
    auto *graph_ptr = BorrowGraphFromPython(graph_obj);
    if (graph_ptr == nullptr) {
      throw std::runtime_error("Pattern graph handle is empty");
    }
    try {
      pattern_ = std::make_unique<Pattern>(std::move(*graph_ptr));
    } catch (...) {
      delete graph_ptr;
      throw;
    }
    delete graph_ptr;
    InvalidatePythonGraph(std::move(graph_obj));
  }

  NativePattern &CaptureTensor(const py::object &source_obj, const py::object &index_obj) {
    EnsureValid();
    pattern_->CaptureTensor(ParseNodeIo(source_obj, index_obj));
    return *this;
  }

  py::list GetCapturedTensors() const {
    EnsureValid();
    std::vector<NodeIo> captured_tensors;
    const auto ret = pattern_->GetCapturedTensors(captured_tensors);
    if (ret != GRAPH_SUCCESS) {
      throw std::runtime_error("Failed to get captured tensors from Pattern");
    }
    py::list result;
    for (const auto &captured_tensor : captured_tensors) {
      result.append(BuildPythonNodeIo(captured_tensor));
    }
    return result;
  }

  uintptr_t Release() {
    EnsureValid();
    return reinterpret_cast<uintptr_t>(pattern_.release());
  }

  bool IsValid() const {
    return pattern_ != nullptr;
  }

 private:
  void EnsureValid() const {
    if (pattern_ == nullptr) {
      throw std::runtime_error("Pattern handle has been released");
    }
  }

  std::unique_ptr<Pattern> pattern_;
};
}  // namespace

void BindPattern(py::module_ &m) {
  py::class_<NativePattern>(m, "Pattern")
      .def(py::init<py::object>(), py::arg("graph"))
      .def("capture_tensor", &NativePattern::CaptureTensor, py::return_value_policy::reference_internal,
           py::arg("source"), py::arg("index") = py::none())
      .def("get_captured_tensors", &NativePattern::GetCapturedTensors)
      .def("release", &NativePattern::Release)
      .def("is_valid", &NativePattern::IsValid);
}

}  // namespace python_pass_native
}  // namespace ge
