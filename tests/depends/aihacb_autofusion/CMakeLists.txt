# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

file(GLOB_RECURSE ASC_SRC_FILES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        "src/*.cc"
        "${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/*.cpp"
        "${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/lowering/*.cpp"
        "${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/can_fuse/*.cpp"
        "${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/post_process/*.cpp"
        "${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/utils/*.cpp"
)


add_library(ascir_ops_headers INTERFACE)
target_include_directories(ascir_ops_headers INTERFACE
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/ascir
        )

add_library(aihacb_autofusion_stub SHARED
        ${ASC_SRC_FILES}
        )

target_compile_definitions(aihacb_autofusion_stub PRIVATE
        PROTOBUF_INLINE_NOT_IN_HEADERS=0
        google=ascend_private
        AUTOFUSE_USE_GE_METADEF
        )

target_compile_options(aihacb_autofusion_stub PRIVATE
        -include ${CMAKE_CURRENT_SOURCE_DIR}/src/fix_af_attr_store.h
        )

target_link_options(aihacb_autofusion_stub PRIVATE
        $<$<BOOL:${ENABLE_GCOV}>:--coverage>
        )

target_include_directories(aihacb_autofusion_stub BEFORE PRIVATE
        ${AIR_CODE_DIR}/inc
        ${AIR_CODE_DIR}/inc/graph_metadef
        ${AIR_CODE_DIR}/inc/graph_metadef/graph
        ${GE_METADEF_DIR}
        )

target_include_directories(aihacb_autofusion_stub PRIVATE
        "src/"
        ${AIR_CODE_DIR}/
        ${METADEF_DIR}/inc/
        ${GE_METADEF_INC_DIR}/
        ${GE_METADEF_INC_DIR}/graph/ascendc_ir
        ${GE_METADEF_INC_DIR}/graph/ascendc_ir/ascendc_ir_core
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/ascir
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/ascir/meta
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/ascir/utils
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/common
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/graph_metadef
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/graph_metadef/graph
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/graph_metadef/graph/ascendc_ir
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/graph_metadef/graph/ascendc_ir/ascendc_ir_core
        ${AIR_CODE_DIR}/
        ${AIR_CODE_DIR}/compiler
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/common
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/lowering
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/can_fuse
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/post_process
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse/utils
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/inc
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/att/
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/ascir/meta
        ${CMAKE_BINARY_DIR}/proto/metadef_protos
        ${CMAKE_BINARY_DIR}/proto/graphengine_protos
        ${ASCEND_INSTALL_PATH}/include/experiment/runtime
        ${ASCEND_INSTALL_PATH}/include/experiment/msprof
        ${ASCEND_INSTALL_PATH}/include/toolchain
        )

target_include_directories(aihacb_autofusion_stub INTERFACE
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/graph_metadef/graph
        )

add_library(aihac_codegen_toolkit SHARED IMPORTED)
set_target_properties(aihac_codegen_toolkit PROPERTIES
        IMPORTED_LOCATION "${ASCEND_INSTALL_PATH}/lib64/libaihac_codegen.so"
        INTERFACE_LINK_LIBRARIES "ascend_hal_stub;atrace_share")

target_link_libraries(aihacb_autofusion_stub PRIVATE
        intf_llt_pub
        ge_metadef_headers
        slog_headers
        mmpa_headers
        msprof_headers
        runtime_headers
        json
        ascend_protobuf
        c_sec
        ascend_protobuf
        graph
        unified_dlog
        graph_base
        ge_running_env
        aihac_codegen_toolkit
        )

#stub_module(aihacb_autofusion aihacb_autofusion_stub)
stub_module(aihac_autofusion aihacb_autofusion_stub)
