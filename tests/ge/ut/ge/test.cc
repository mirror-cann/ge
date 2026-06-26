/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

#include "common/debug/log.h"
#include "ge_graph_dsl/assert/check_utils.h"
#include "faker/space_registry_faker.h"
#include "graph/ge_local_context.h"
#include "register/optimization_option_registry.h"
#include "ge_running_env/dir_env.h"
#include "register/ops_kernel_builder_registry.h"

using namespace std;
using namespace ge;

extern "C" const char *__lsan_default_suppressions() {
    return "leak:_PyObject_Malloc\n"
           "leak:_PyObject_Realloc\n"
           "leak:PyType_GenericAlloc\n"
           "leak:PyType_Ready\n"
           "leak:PyObject_GC_New\n"
           "leak:PyObject_GC_NewVar\n"
           "leak:_PyObject_GC_Malloc\n"
           "leak:PyUnicode_New\n"
           "leak:Py_InitializeEx\n"
           "leak:PyImport_ImportModuleLevelObject\n"
           "leak:PyThread_allocate_lock\n"
           "leak:pybind11::detail::make_static_property_type\n"
           "leak:pybind11::detail::make_object_base_type\n"
           "leak:pybind11::detail::get_internals\n"
           "leak:pybind11::detail::get_local_internals\n"
           "leak:libpython\n";
}

int main(int argc, char **argv) {
  // init the logging
  testing::InitGoogleTest(&argc, argv);
  setenv("GE_PROFILING_TO_STD_OUT", "1", true);
  GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
  DirEnv::GetInstance().InitDir();
  CheckUtils::init();
  int ret = RUN_ALL_TESTS();
  OpsKernelBuilderRegistry::GetInstance().UnregisterAll();
  printf("finish ge ut\n");

  return ret;
}
