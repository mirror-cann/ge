# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

if (BUILD_OPEN_PROJECT OR ENABLE_OPEN_SRC)
    include(cmake/function.cmake)
    find_cann_package(slog MODULE REQUIRED)
    find_cann_package(runtime MODULE REQUIRED)
    find_cann_package(mmpa MODULE REQUIRED)
    find_cann_package(msprof MODULE REQUIRED)
    find_cann_package(error_manager MODULE REQUIRED)
    find_cann_package(adump MODULE REQUIRED)
    find_cann_package(hccl MODULE REQUIRED)
    find_cann_package(aicpu MODULE REQUIRED)
    find_cann_package(unified_dlog MODULE REQUIRED)
    find_cann_package(platform MODULE REQUIRED)
    find_cann_package(ascendcl MODULE REQUIRED)
    if(NOT MDC_COMPILE_RUNTIME) # MDC运行态不需要下列依赖
        find_cann_package(cce MODULE REQUIRED)
        find_cann_package(datagw MODULE REQUIRED)
        find_cann_package(ascend_hal MODULE REQUIRED)
        find_cann_package(atrace MODULE REQUIRED)
    endif()

    # 使用medadef发布包编译
    find_cann_package(metadef MODULE REQUIRED)
    # 使用autofuse发布包编译
    find_cann_package(autofuse MODULE)
    set(PARSER_DIR ${CMAKE_CURRENT_LIST_DIR}/parser)
endif ()
