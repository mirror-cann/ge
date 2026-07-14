# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

add_library(custom_op_runtime SHARED
    "custom_op_loader.cc"
    "python_custom_op_adapter.cc"
    "python_custom_op_bridge_loader.cc"
)

target_compile_definitions(custom_op_runtime PRIVATE
    PROTOBUF_INLINE_NOT_IN_HEADERS=0
    google=ascend_private
)

target_compile_options(custom_op_runtime PRIVATE
    ${AIR_COMMON_DYNAMIC_COMPILE_OPTION}
)

target_include_directories(custom_op_runtime PRIVATE
    ${AIR_CODE_DIR}
    ${AIR_CODE_DIR}/inc/graph_metadef
)

target_include_directories(custom_op_runtime SYSTEM PRIVATE
    ${AIR_CODE_DIR}/base
    ${AIR_CODE_DIR}/inc
    ${AIR_CODE_DIR}/inc/framework
    ${AIR_CODE_DIR}/inc/graph_metadef/external
)

target_link_libraries(custom_op_runtime
    PRIVATE
        ge_intf_pub
        c_sec
        mmpa_headers
        graph
        register
        unified_dlog
        ge_common
        ge_common_base
        ge_metadef_headers
        error_manager_headers
        runtime_headers
        c_sec_headers
        json
        -ldl
)

add_library(ge_python_custom_op_bridge SHARED
    "python_custom_op_pybind_bridge.cc"
)

target_compile_definitions(ge_python_custom_op_bridge PRIVATE
    PROTOBUF_INLINE_NOT_IN_HEADERS=0
    google=ascend_private
)

target_compile_options(ge_python_custom_op_bridge PRIVATE
    ${AIR_COMMON_DYNAMIC_COMPILE_OPTION}
)

target_include_directories(ge_python_custom_op_bridge PRIVATE
    ${AIR_CODE_DIR}
    ${AIR_CODE_DIR}/inc/graph_metadef
)

target_include_directories(ge_python_custom_op_bridge SYSTEM PRIVATE
    ${AIR_CODE_DIR}/base
    ${AIR_CODE_DIR}/inc
    ${AIR_CODE_DIR}/inc/external
    ${AIR_CODE_DIR}/inc/framework
    ${AIR_CODE_DIR}/inc/graph_metadef/external
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/proto/graphengine_protos
    ${HI_PYTHON_INC}
    ${pybind11_INCLUDE_DIR}
)

target_link_libraries(ge_python_custom_op_bridge
    PRIVATE
        ge_intf_pub
        c_sec
        c_sec_headers
        graph
        register
        unified_dlog
        ge_python_embed
        ge_common
        ge_metadef_headers
        error_manager_headers
        mmpa_headers
        pybind_options
        runtime_headers
)
