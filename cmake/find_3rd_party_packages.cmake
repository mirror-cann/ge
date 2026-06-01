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
    if(BUILD_COMPONENT)
        message(STATUS "find third party packages in divided mode, component:${BUILD_COMPONENT}")
        # 1. 三个组件全部都需要
        add_cann_third_party(json)
        add_cann_third_party(zlib)
        add_cann_third_party(openssl)
        add_cann_third_party(protobuf)
        find_package(SymEngine CONFIG REQUIRED)

        # 2. ge-executor 或 dflow-executor
        if("ge-executor" IN_LIST BUILD_COMPONENT OR "dflow-executor" IN_LIST BUILD_COMPONENT)
            add_cann_third_party(grpc)
            find_package(gRPC CONFIG REQUIRED)
            find_package(Boost CONFIG REQUIRED)
        endif()
    else()
        message(STATUS "find third party packages in normal mode")
        find_package(Threads)
        add_cann_third_party(json)
        add_cann_third_party(openssl)
        add_cann_third_party(zlib)
        add_cann_third_party(protobuf)
        add_cann_third_party(grpc)
        find_package(gRPC CONFIG REQUIRED)
        find_package(SymEngine CONFIG REQUIRED)
        find_package(Boost CONFIG REQUIRED)
    endif()
endif ()
