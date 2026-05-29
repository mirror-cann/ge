# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.14)
include(../fetch_cann_cmake.cmake)
project (ThirdPartyPkg[CXX])

set(CMAKE_VERBOSE_MAKEFILE True)
set(CMAKE_MODULE_PATH
    ${CMAKE_CURRENT_LIST_DIR}/build/modules
)

include(CMakePrintHelpers)
include(CMakePackageConfigHelpers)

message(STATUS "[ThirdPartyLib] Variables for third_party packages:")
cmake_print_variables(CANN_3RD_LIB_PATH)
cmake_print_variables(CMAKE_BUILD_COMPONENT)
cmake_print_variables(ENABLE_BUILD_DEVICE)
cmake_print_variables(USE_CXX11_ABI)
cmake_print_variables(CMAKE_TOOLCHAIN_FILE)
cmake_print_variables(LLVM_PATH)
cmake_print_variables(CMAKE_MODULE_PATH)

if(ENABLE_BUILD_DEVICE)
    # 非MDC编译流程
    set(USE_CXX11_ABI 0)
else()
    string(FIND "${CMAKE_TOOLCHAIN_FILE}" "ubuntu18.04-x86_64-llvm-toolchain.cmake" SUBSTRING_FOUND)
    if(SUBSTRING_FOUND GREATER_EQUAL 0)
        set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY false)
        # MDC编译运行态
        message("[MDC compile] Thirdparty build, MDC compile runtime state.")
        add_compile_definitions(_GLIBCXX_USE_CXX11_ABI=${USE_CXX11_ABI})
    else()
        # MDC编译开发态
        message("[MDC compile] Thirdparty build, MDC compile develop state.")
        set(USE_CXX11_ABI 0)
    endif()
endif()

set(INSTALL_BASE_DIR "")
set(INSTALL_LIBRARY_DIR lib)
set(INSTALL_RUNTIME_DIR bin)
set(INSTALL_INCLUDE_DIR include)
set(INSTALL_CONFIG_DIR cmake)
set(CMAKE_INSTALL_LIBDIR lib)
set(FORCE_REBUILD_CANN_3RD false)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif(CCACHE_PROGRAM)

if (NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH
        ${CANN_3RD_LIB_PATH}/protoc
        ${CANN_3RD_LIB_PATH}/protobuf_static
        ${CANN_3RD_LIB_PATH}/ascend_protobuf
        ${CANN_3RD_LIB_PATH}/ascend_protobuf_static
    )
endif()

message(STATUS "[ThirdPartyLib][${CMAKE_BUILD_COMPONENT}] third-party process")
if(CMAKE_BUILD_COMPONENT STREQUAL "ge-compiler")
    add_cann_third_party(json)
    add_cann_third_party(openssl)
    add_cann_third_party(abseil-cpp)
    find_package(protoc MODULE REQUIRED)
    find_package(ascend_protobuf_shared MODULE REQUIRED)
    find_package(ascend_protobuf_static MODULE REQUIRED)
    add_cann_third_party(boost)
    add_cann_third_party(symengine)
    add_cann_third_party(zlib)
    set(selected_targets
        ascend_protobuf_build
        ascend_protobuf_static_build
        third_party_json
        openssl_project
        protoc_build
        third_party_boost
        symengine_build
        zlib_bin_build
    )
elseif(CMAKE_BUILD_COMPONENT STREQUAL "ge-executor")
    include(build/modules/grpc.cmake)
    add_cann_third_party(abseil-cpp)
    find_package(protobuf_static MODULE REQUIRED)
    add_cann_third_party(cares)
    add_cann_third_party(re2)
    add_cann_third_party(zlib)
    add_cann_third_party(openssl)
    find_package(ascend_protobuf_shared MODULE REQUIRED)
    find_package(ascend_protobuf_static MODULE REQUIRED)
    add_cann_third_party(json)
    find_package(protoc MODULE REQUIRED)
    add_cann_third_party(boost)
    add_cann_third_party(symengine)
    set(selected_targets
        ascend_protobuf_build
        ascend_protobuf_static_build
        grpc_build
        third_party_json
        protobuf_static_build
        protoc_build
        protoc_grpc_build
        zlib_bin_build
        third_party_boost
        symengine_build
    )
else()
    include(build/modules/grpc.cmake)
    add_cann_third_party(abseil-cpp)
    find_package(protobuf_static MODULE REQUIRED)
    add_cann_third_party(cares)
    add_cann_third_party(re2)
    add_cann_third_party(zlib)
    add_cann_third_party(openssl)
    add_cann_third_party(boost)
    add_cann_third_party(symengine)
    find_package(ascend_protobuf_shared MODULE REQUIRED)
    find_package(ascend_protobuf_static MODULE REQUIRED)
    add_cann_third_party(json)
    find_package(protoc MODULE REQUIRED)
    # abseil_build,cares_build,re2_build只在被依赖时编译,不主动编译
    set(selected_targets
        ascend_protobuf_build
        ascend_protobuf_static_build
        grpc_build
        third_party_json
        openssl_project
        protobuf_static_build
        protoc_build
        protoc_grpc_build
        zlib_bin_build
        third_party_boost
        symengine_build
    )
endif()

if(CMAKE_BUILD_COMPONENT STREQUAL "LLT")
    add_cann_third_party(benchmark)
    add_cann_third_party(gtest_shared)
    set(selected_targets
        ${selected_targets}
        benchmark_build
        gtest_shared_build
    )
endif()

message(STATUS "[ThirdPartyLib] Building default targets")
add_custom_target(select_targets)
foreach(dep ${selected_targets})
    if(TARGET ${dep})
        add_dependencies(select_targets ${dep})
        message(STATUS "[ThirdPartyLib] add dependence:${dep} success.")
    else()
        message(STATUS "[ThirdPartyLib] add dependence:${dep} ignored.")
    endif()
endforeach()

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/scripts/modules
        DESTINATION ${CANN_3RD_LIB_PATH}/cmake
)