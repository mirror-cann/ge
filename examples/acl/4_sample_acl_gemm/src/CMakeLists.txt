# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.5.1)
project(ACL_SAMPLE_ACL_GEMM_SRC)

add_compile_options(-std=c++17 -Wall -O2)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../../../out")

set(INC_PATH $ENV{ASCEND_HOME_PATH})
if (NOT DEFINED ENV{ASCEND_HOME_PATH})
  message(WARNING "please source the set_env.sh")
  set(INC_PATH "/usr/local/Ascend/cann")
endif ()

include_directories(${INC_PATH}/include/)
link_directories(${INC_PATH}/lib64 ${INC_PATH}/devlib)

add_executable(main sample_acl_gemm.cpp)
target_link_libraries(main ascendcl acl_cblas acl_op_executor nnopbase opapi opapi_nn stdc++)
