/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <string>

#include "binding_utils.h"
#include "bindings.h"
#include "ge/fusion/match_result.h"

namespace ge {
namespace python_pass_native {
namespace {
using ::ge::fusion::MatchResult;

class BorrowedMatchResult {
 public:
  explicit BorrowedMatchResult(MatchResult *match_result) : match_result_(match_result) {}

  py::list GetMatchedNodes() const {
    EnsureValid();
    py::list result;
    for (const auto &matched_node : match_result_->GetMatchedNodes()) {
      result.append(BuildPythonNode(matched_node));
    }
    return result;
  }

  py::object GetCapturedTensor(size_t capture_index) const {
    EnsureValid();
    NodeIo node_output;
    const auto ret = match_result_->GetCapturedTensor(capture_index, node_output);
    if (ret != GRAPH_SUCCESS) {
      throw std::runtime_error("Failed to get captured tensor from MatchResult");
    }
    return BuildPythonNodeIo(node_output);
  }

  std::string GetPatternGraphName() const {
    EnsureValid();
    AscendString graph_name;
    if (match_result_->GetPatternGraph().GetName(graph_name) != GRAPH_SUCCESS) {
      throw std::runtime_error("Failed to get pattern graph name from MatchResult");
    }
    return AscendStringToString(graph_name);
  }

  std::string ToString() const {
    EnsureValid();
    return AscendStringToString(match_result_->ToAscendString());
  }

  void Invalidate() {
    valid_ = false;
    match_result_ = nullptr;
  }

 private:
  void EnsureValid() const {
    if ((!valid_) || (match_result_ == nullptr)) {
      throw std::runtime_error("MatchResult handle has expired");
    }
  }

  MatchResult *match_result_{nullptr};
  bool valid_{true};
};

BorrowedMatchResult BorrowMatchResult(uintptr_t match_result_handle) {
  if (match_result_handle == 0U) {
    throw std::runtime_error("MatchResult handle cannot be null");
  }
  return BorrowedMatchResult(reinterpret_cast<MatchResult *>(match_result_handle));
}
}  // namespace

void BindMatchResult(py::module_ &m) {
  py::class_<BorrowedMatchResult>(m, "MatchResult")
      .def("get_matched_nodes", &BorrowedMatchResult::GetMatchedNodes)
      .def("get_captured_tensor", &BorrowedMatchResult::GetCapturedTensor, py::arg("capture_index"))
      .def("get_pattern_graph_name", &BorrowedMatchResult::GetPatternGraphName)
      .def("_invalidate", &BorrowedMatchResult::Invalidate)
      .def("__str__", &BorrowedMatchResult::ToString);

  m.def("borrow_match_result", &BorrowMatchResult, py::arg("match_result_handle"));
}

}  // namespace python_pass_native
}  // namespace ge
