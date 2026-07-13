/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef API_PYTHON_GE_GE_CUSTOM_OP_NATIVE_BINDINGS_BINDING_COMMON_H_
#define API_PYTHON_GE_GE_CUSTOM_OP_NATIVE_BINDINGS_BINDING_COMMON_H_

#include "pybind11/detail/common.h"
#ifdef ASCEND_CI_LIMITED_PY37
#undef PyCFunction_NewEx
#endif
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#undef PYBIND11_CHECK_PYTHON_VERSION
#define PYBIND11_CHECK_PYTHON_VERSION

namespace ge {
namespace python_custom_op_native {
namespace py = pybind11;
}  // namespace python_custom_op_native
}  // namespace ge

#endif  // API_PYTHON_GE_GE_CUSTOM_OP_NATIVE_BINDINGS_BINDING_COMMON_H_
