# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

if (NOT DEFINED GE_METADEF_INC_DIR)
    # 开发环境编译兼容处理
    set(METADEF_DIR ${TOP_DIR}/metadef)
    set(GE_METADEF_DIR ${TOP_DIR}/air/graph_metadef)
    set(GE_METADEF_INC_DIR ${TOP_DIR}/air/inc/graph_metadef)
    set(AIR_CODE_DIR ${TOP_DIR}/air)
    set(RUNTIME_CODE_DIR ${TOP_DIR}/runtime)
else ()
    if(NOT TARGET metadef_headers)
        add_library(metadef_headers INTERFACE)
    endif()
endif ()

# 当 metadef_headers 为 IMPORTED target时，INTERFACE_INCLUDE_DIRECTORIES 作为-isystem使用，gcc 在所有 -I 之后搜索
# 导致先搜到af空间的auto_fuse头文件，与ge仓本地的auto_fuse不兼容，导致编译失败。
# 重定义一个ge_metadef_headers，使ge仓内部头文件路径使用-I，保证在Findmetadef引入的autofuse路径之前被搜索到。
get_target_property(_mh_imported metadef_headers IMPORTED)
if(_mh_imported)
    set_property(TARGET metadef_headers APPEND PROPERTY INTERFACE_LINK_LIBRARIES ge_metadef_headers)
else()
# 影响sdk包导出cmake，增加时必须包含BUILD_INTERFACE
target_include_directories(metadef_headers INTERFACE
    $<BUILD_INTERFACE:${METADEF_DIR}/inc/external>
    $<BUILD_INTERFACE:${METADEF_DIR}/inc/external/base>
    $<BUILD_INTERFACE:${METADEF_DIR}/inc/external/exe_graph>
    $<BUILD_INTERFACE:${METADEF_DIR}/inc/external/exe_graph/runtime>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/graph>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/base>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/graph>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/common>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/graph/utils>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/common/ge_common>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/external>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/base>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/graph>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/exe_graph>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/experiment/metadef>
    $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/register>
    $<BUILD_INTERFACE:${AIR_CODE_DIR}/inc>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/common>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/exe_graph>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/external>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/external/graph>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/external/exe_graph>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/external/op_common>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/graph/utils>
    $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/register>
    $<BUILD_INTERFACE:${RUNTIME_CODE_DIR}/include/external>

    # --------------------start------------------------
    $<BUILD_INTERFACE:${GE_METADEF_DIR}>
    $<BUILD_INTERFACE:${GE_METADEF_DIR}/third_party/transformer>
    $<BUILD_INTERFACE:${GE_METADEF_DIR}/third_party/transformer/inc>
    $<BUILD_INTERFACE:${GE_METADEF_DIR}/third_party/transformer/src>
    $<BUILD_INTERFACE:${GE_METADEF_DIR}/exe_graph>
    $<BUILD_INTERFACE:${GE_METADEF_DIR}/graph>
    $<BUILD_INTERFACE:${GE_METADEF_DIR}/register>
    $<BUILD_INTERFACE:${GE_METADEF_DIR}/register/op_tiling>
    # ---------------------end-----------------------
)
endif()

# 解决与cmake仓中的metadef_headers冲突；ge_metadef_headers优先使用GE仓中的路径，保证先于af仓中的头文件被搜索到
if(NOT TARGET ge_metadef_headers)
    add_library(ge_metadef_headers INTERFACE)
    target_link_libraries(ge_metadef_headers INTERFACE metadef_headers)
    target_include_directories(ge_metadef_headers INTERFACE
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/graph>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/common>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/exe_graph>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/external>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/external/graph>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/external/exe_graph>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/external/op_common>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/graph/utils>
        $<BUILD_INTERFACE:${GE_METADEF_INC_DIR}/register>
        $<BUILD_INTERFACE:${AIR_CODE_DIR}/inc>
        $<BUILD_INTERFACE:${GE_METADEF_DIR}>
        $<BUILD_INTERFACE:${GE_METADEF_DIR}/third_party/transformer>
        $<BUILD_INTERFACE:${GE_METADEF_DIR}/third_party/transformer/inc>
        $<BUILD_INTERFACE:${GE_METADEF_DIR}/third_party/transformer/src>
        $<BUILD_INTERFACE:${GE_METADEF_DIR}/exe_graph>
        $<BUILD_INTERFACE:${GE_METADEF_DIR}/graph>
        $<BUILD_INTERFACE:${GE_METADEF_DIR}/register>
        $<BUILD_INTERFACE:${GE_METADEF_DIR}/register/op_tiling>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/base>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/graph>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/common>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/pkg_inc/common/ge_common>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/external>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/base>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/graph>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/exe_graph>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/experiment/metadef>
        $<BUILD_INTERFACE:${ASCEND_INSTALL_PATH}/include/register>
    )
endif()

message(STATUS "inc graph_metadef")
get_target_property(INCLUDE_PATHS metadef_headers INTERFACE_INCLUDE_DIRECTORIES)
foreach(line IN LISTS INCLUDE_PATHS)
    message(STATUS "metadef_headers ${line}")
endforeach()
