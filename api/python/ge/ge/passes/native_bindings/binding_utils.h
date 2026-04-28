/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef API_PYTHON_GE_GE_PASSES_NATIVE_BINDINGS_BINDING_UTILS_H_
#define API_PYTHON_GE_GE_PASSES_NATIVE_BINDINGS_BINDING_UTILS_H_

#include <memory>
#include <stdexcept>
#include <string>

#include "binding_common.h"
#include "ge/fusion/pattern.h"
#include "graph/ascend_string.h"
#include "graph/graph.h"
#include "graph/gnode.h"

namespace ge {
namespace python_pass_native {

using ::ge::fusion::NodeIo;

inline std::string AscendStringToString(const AscendString &ascend_str) {
  return std::string(ascend_str.GetString());
}

inline AscendString StringToAscendString(const std::string &str) {
  return AscendString(str.c_str());
}

inline uintptr_t ExtractPointerValue(const py::handle &handle_obj) {
  if (handle_obj.is_none()) {
    return 0U;
  }
  if (py::isinstance<py::int_>(handle_obj)) {
    return handle_obj.cast<uintptr_t>();
  }
  if (py::hasattr(handle_obj, "value")) {
    py::object value_obj = handle_obj.attr("value");
    if (!value_obj.is_none()) {
      return value_obj.cast<uintptr_t>();
    }
  }
  throw std::runtime_error("Unsupported pointer handle object");
}

inline py::object BuildPointerObject(uintptr_t pointer_value) {
  py::module_ ctypes_module = py::module_::import("ctypes");
  return ctypes_module.attr("c_void_p")(py::int_(pointer_value));
}

inline Graph *BorrowGraphFromPython(const py::handle &graph_obj) {
  if (!py::hasattr(graph_obj, "_handle")) {
    throw std::runtime_error("Graph object must carry _handle");
  }
  return reinterpret_cast<Graph *>(ExtractPointerValue(graph_obj.attr("_handle")));
}

inline GNode *BorrowNodeFromPython(const py::handle &node_obj) {
  if (!py::hasattr(node_obj, "_handle")) {
    throw std::runtime_error("Node object must carry _handle");
  }
  return reinterpret_cast<GNode *>(ExtractPointerValue(node_obj.attr("_handle")));
}

inline void InvalidatePythonGraph(py::object graph_obj) {
  if (py::hasattr(graph_obj, "_owns_handle")) {
    graph_obj.attr("_owns_handle") = py::bool_(false);
  }
  if (py::hasattr(graph_obj, "_handle")) {
    graph_obj.attr("_handle") = py::none();
  }
  if (py::hasattr(graph_obj, "_owner")) {
    graph_obj.attr("_owner") = py::none();
  }
}

inline py::object BuildPythonNode(const GNode &node) {
  // new 失败, python会捕获异常
  auto node_copy = std::make_unique<GNode>(node);
  py::module_ graph_module = py::module_::import("ge.graph");
  py::object node_type = graph_module.attr("Node");
  py::object python_node =
      node_type.attr("_create_from")(BuildPointerObject(reinterpret_cast<uintptr_t>(node_copy.get())),
                                     py::bool_(true));
  (void)node_copy.release();
  return python_node;
}

inline py::object BuildPythonNodeIo(const NodeIo &node_output) {
  py::module_ pattern_module = py::module_::import("ge.passes.pattern");
  py::object node_io_type = pattern_module.attr("NodeIo");
  return node_io_type(BuildPythonNode(node_output.node), py::int_(node_output.index));
}

inline NodeIo ParseNodeIo(const py::handle &source_obj, const py::object &index_obj) {
  py::object node_obj;
  int64_t output_index = 0;
  if (py::hasattr(source_obj, "node") && py::hasattr(source_obj, "index") && index_obj.is_none()) {
    node_obj = source_obj.attr("node");
    output_index = source_obj.attr("index").cast<int64_t>();
  } else {
    node_obj = py::reinterpret_borrow<py::object>(source_obj);
    output_index = index_obj.is_none() ? 0L : index_obj.cast<int64_t>();
  }
  auto *node_ptr = BorrowNodeFromPython(node_obj);
  if (node_ptr == nullptr) {
    throw std::runtime_error("Node handle is empty");
  }
  return NodeIo{*node_ptr, output_index};
}

}  // namespace python_pass_native
}  // namespace ge

#endif  // API_PYTHON_GE_GE_PASSES_NATIVE_BINDINGS_BINDING_UTILS_H_
