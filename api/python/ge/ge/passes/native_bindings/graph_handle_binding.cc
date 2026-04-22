/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "binding_utils.h"
#include "bindings.h"

namespace ge {
namespace python_pass_native {
namespace {
uintptr_t ReleaseGraph(py::object graph_obj) {
  auto *graph_ptr = BorrowGraphFromPython(graph_obj);
  if (graph_ptr == nullptr) {
    throw std::runtime_error("Graph handle is empty");
  }
  InvalidatePythonGraph(std::move(graph_obj));
  return reinterpret_cast<uintptr_t>(graph_ptr);
}
}  // namespace

void BindGraphHandleHelpers(py::module_ &m) {
  m.def("release_graph", &ReleaseGraph, py::arg("graph"));
}

}  // namespace python_pass_native
}  // namespace ge
