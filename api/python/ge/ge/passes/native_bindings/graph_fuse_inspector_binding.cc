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

#include <string>
#include <vector>

#include "ge/fusion/graph_fuse_inspector_utils.h"

namespace ge {
namespace python_pass_native {
namespace {
std::vector<GNode> ParseNodes(const py::iterable &node_objects, const char *const argument_name) {
  const py::object node_type = py::module_::import("ge.graph").attr("Node");
  std::vector<GNode> nodes;
  for (const py::handle node_object : node_objects) {
    if (!py::isinstance(node_object, node_type)) {
      throw py::type_error(std::string(argument_name) + " must contain only ge.graph.Node objects");
    }
    const auto *node = BorrowNodeFromPython(node_object);
    if (node == nullptr) {
      throw std::runtime_error("Node handle is empty");
    }
    nodes.emplace_back(*node);
  }
  return nodes;
}

py::tuple CanFuse(const py::iterable &node_objects) {
  const auto nodes = ParseNodes(node_objects, "nodes");
  AscendString failed_reason;
  const bool ok = fusion::GraphFuseInspectorUtils::CanFuse(nodes, failed_reason);
  const char *const reason = failed_reason.GetString();
  return py::make_tuple(ok, reason == nullptr ? "" : reason);
}

void ReportFuse(const py::iterable &nodes_before_objects, const py::iterable &nodes_after_objects,
                CustomPassContext &context) {
  const auto nodes_before = ParseNodes(nodes_before_objects, "nodes_before");
  const auto nodes_after = ParseNodes(nodes_after_objects, "nodes_after");
  const auto status = fusion::GraphFuseInspectorUtils::ReportFuse(nodes_before, nodes_after, context);
  if (status != SUCCESS) {
    const auto pass_name = context.GetPassName();
    const char *const pass_name_str = pass_name.GetString();
    const std::string message =
        "Failed to report fusion result, pass_name=" + std::string(pass_name_str == nullptr ? "" : pass_name_str) +
        ", status=" + std::to_string(static_cast<uint32_t>(status));
    context.SetErrorMessage(AscendString(message.c_str()));
    throw std::runtime_error(message);
  }
}
}  // namespace

void BindGraphFuseInspector(py::module_ &m) {
  m.def("can_fuse", &CanFuse, py::arg("nodes"), "Check whether nodes can be safely fused into one node");
  m.def("report_fuse", &ReportFuse, py::arg("nodes_before"), py::arg("nodes_after"), py::arg("context"),
        "Report the result of a graph fusion rewrite");
}

}  // namespace python_pass_native
}  // namespace ge
