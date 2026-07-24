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

#include "ge/fusion/graph_rewriter.h"
#include "ge/fusion/subgraph_boundary.h"

namespace ge {
namespace python_pass_native {
namespace {
using ::ge::fusion::NodeIo;
using ::ge::fusion::SubgraphBoundary;
using ::ge::fusion::SubgraphInput;
using ::ge::fusion::SubgraphOutput;
using ::ge::fusion::SubgraphRewriter;

NodeIo BuildNodeIo(const py::handle &node_obj, int64_t index) {
  auto *node_ptr = BorrowNodeFromPython(node_obj);
  if (node_ptr == nullptr) {
    throw std::runtime_error("Node handle is empty");
  }
  return NodeIo{*node_ptr, index};
}

void ReplaceWithContext(const SubgraphBoundary &boundary, const py::handle &replacement_graph,
                        CustomPassContext &context) {
  auto *graph_ptr = BorrowGraphFromPython(replacement_graph);
  if (graph_ptr == nullptr) {
    throw std::runtime_error("Graph handle is empty");
  }
  const auto status = SubgraphRewriter::Replace(boundary, *graph_ptr, context);
  if (status != SUCCESS) {
    const auto pass_name = context.GetPassName();
    const char *const pass_name_str = pass_name.GetString();
    const std::string message = "SubgraphRewriter::Replace with context failed, pass_name=" +
                                std::string(pass_name_str == nullptr ? "" : pass_name_str) +
                                ", status=" + std::to_string(static_cast<uint32_t>(status));
    context.SetErrorMessage(AscendString(message.c_str()));
    throw std::runtime_error(message);
  }
}

void BindSubgraphRewriter(py::module_ &m) {
  py::class_<SubgraphRewriter>(m, "SubgraphRewriter", "Subgraph rewriter")
      .def_static(
          "replace",
          [](const SubgraphBoundary &boundary, const py::handle &replacement_graph) -> uint32_t {
            auto *graph_ptr = BorrowGraphFromPython(replacement_graph);
            if (graph_ptr == nullptr) {
              throw std::runtime_error("Graph handle is empty");
            }
            // SubgraphRewriter::Replace(const Graph&) will copy the replacement graph internally.
            return static_cast<uint32_t>(SubgraphRewriter::Replace(boundary, *graph_ptr));
          },
          py::arg("boundary"), py::arg("replacement"), "Execute subgraph replacement")
      .def_static("replace", &ReplaceWithContext, py::arg("boundary"), py::arg("replacement"), py::kw_only(),
                  py::arg("context"), "Execute subgraph replacement with fusion inspection and reporting");
}
}  // namespace

constexpr size_t kNodeInputTupleSize = 2UL;

void BindGraphRewriter(py::module_ &m) {
  py::class_<SubgraphInput>(m, "SubgraphInput",
                            "Describe a subgraph input (one input can correspond to multiple node inputs on boundary)")
      .def(py::init<>())
      .def(py::init([](py::iterable node_inputs) {
        std::vector<NodeIo> inputs;
        for (py::handle item : node_inputs) {
          if (!py::isinstance<py::tuple>(item) || (py::len(item) != kNodeInputTupleSize)) {
            throw std::runtime_error("SubgraphInput expects iterable of (node, index)");
          }
          py::tuple tup = py::reinterpret_borrow<py::tuple>(item);
          inputs.emplace_back(BuildNodeIo(tup[0], tup[1].cast<int64_t>()));
        }
        return SubgraphInput(std::move(inputs));
      }))
      .def(
          "add_input",
          [](SubgraphInput &self, const py::handle &node, int64_t out_index) -> uint32_t {
            return static_cast<uint32_t>(self.AddInput(BuildNodeIo(node, out_index)));
          },
          py::arg("node"), py::arg("out_index"), "Append an input anchor");

  py::class_<SubgraphOutput>(m, "SubgraphOutput", "Describe a subgraph output")
      .def(py::init<>())
      .def(py::init(
               [](const py::handle &node, int64_t out_index) { return SubgraphOutput(BuildNodeIo(node, out_index)); }),
           py::arg("node"), py::arg("out_index"))
      .def(
          "set_output",
          [](SubgraphOutput &self, const py::handle &node, int64_t out_index) -> uint32_t {
            return static_cast<uint32_t>(self.SetOutput(BuildNodeIo(node, out_index)));
          },
          py::arg("node"), py::arg("out_index"), "Set output anchor");

  py::class_<SubgraphBoundary>(m, "SubgraphBoundary", "Describe input/output boundary of subgraph to be replaced")
      .def(py::init<>())
      .def(
          "add_input",
          [](SubgraphBoundary &self, int64_t index, const SubgraphInput &input) -> uint32_t {
            return static_cast<uint32_t>(self.AddInput(index, input));
          },
          py::arg("index"), py::arg("input"), "Bind the index-th boundary input to SubgraphInput")
      .def(
          "add_output",
          [](SubgraphBoundary &self, int64_t index, const SubgraphOutput &output) -> uint32_t {
            return static_cast<uint32_t>(self.AddOutput(index, output));
          },
          py::arg("index"), py::arg("output"), "Bind the index-th boundary output to SubgraphOutput");

  BindSubgraphRewriter(m);
}

}  // namespace python_pass_native
}  // namespace ge
