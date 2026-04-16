/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifdef ASCEND_CI_LIMITED_PY37
#undef PyCFunction_NewEx
#endif

#include <memory>
#include <string>

#include "Python.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"
#include "graph/ascend_string.h"
#include "register/register_custom_pass.h"

#undef PYBIND11_CHECK_PYTHON_VERSION
#define PYBIND11_CHECK_PYTHON_VERSION

namespace ge {
namespace {
namespace py = pybind11;

std::string AscendStringToString(const AscendString &ascend_str) {
  return std::string(ascend_str.GetString());
}

AscendString StringToAscendString(const std::string &str) {
  return AscendString(str.c_str());
}

std::string GetOptionValueWrapper(CustomPassContext &context, const std::string &option_key) {
  AscendString option_value;
  const auto ret = context.GetOptionValue(StringToAscendString(option_key), option_value);
  if (ret != GRAPH_SUCCESS) {
    throw std::runtime_error("Failed to get option value for key: " + option_key);
  }
  return AscendStringToString(option_value);
}
}

PYBIND11_MODULE(_ge_pass_native, m) {
  py::class_<CustomPassContext>(m, "PassContext")
      .def("get_pass_name", [](CustomPassContext &context) -> std::string {
        return AscendStringToString(context.GetPassName());
      }, py::return_value_policy::reference_internal)
      .def("get_error_message", [](CustomPassContext &context) -> std::string {
        return AscendStringToString(context.GetErrorMessage());
      }, py::return_value_policy::reference_internal)
      .def("set_error_message", [](CustomPassContext &context, const std::string &error_message) {
        context.SetErrorMessage(StringToAscendString(error_message));
      })
      .def("set_pass_name", [](CustomPassContext &context, const std::string &pass_name) {
        context.SetPassName(StringToAscendString(pass_name));
      })
      .def("get_option_value", &GetOptionValueWrapper);
}
}  // namespace ge
