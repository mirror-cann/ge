# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

add_library(_ge_pass_native MODULE passes_apis/pass_context_wrapper.cc)
target_include_directories(_ge_pass_native PRIVATE
        ${HI_PYTHON_INC}
        ${pybind11_INCLUDE_DIR}
        ${AIR_CODE_DIR}/inc/external
        ${METADEF_DIR}/inc/external
        ${METADEF_DIR}/pkg_inc
        ${CMAKE_SOURCE_DIR}/inc/graph_metadef/external
)

target_link_libraries(_ge_pass_native PRIVATE
        $<BUILD_INTERFACE:intf_pub>
        $<BUILD_INTERFACE:slog_headers>
        $<BUILD_INTERFACE:mmpa_headers>
        $<BUILD_INTERFACE:pybind_options>
        register
)

set_target_properties(_ge_pass_native
        PROPERTIES
        PREFIX ""
)

target_compile_definitions(_ge_pass_native PRIVATE
        PYBIND11_NO_ASSERT_GIL_HELD_INCREF_DECREF
)

target_compile_options(_ge_pass_native PRIVATE
        -O2
        -Xlinker -export-dynamic
)

target_link_options(_ge_pass_native PRIVATE
        -s
)

#set_target_properties(_ge_pass_native PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
