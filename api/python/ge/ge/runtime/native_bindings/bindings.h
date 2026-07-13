/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef API_PYTHON_GE_GE_RUNTIME_NATIVE_BINDINGS_BINDINGS_H_
#define API_PYTHON_GE_GE_RUNTIME_NATIVE_BINDINGS_BINDINGS_H_

#include "binding_common.h"

namespace ge {
namespace python_runtime_native {

void BindRuntimeTypes(py::module_ &m);

}  // namespace python_runtime_native
}  // namespace ge

#endif  // API_PYTHON_GE_GE_RUNTIME_NATIVE_BINDINGS_BINDINGS_H_
