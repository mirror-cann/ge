# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2026 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

set(AUTOFUSE_PROTO_DIR ${CODE_ROOT_DIR}/proto)
set(AUTOFUSE_PROTO_SRCS
    "${CMAKE_BINARY_DIR}/proto/ge_autofuse_metadef_protos_af/proto/af_ir.pb.cc"
    "${CMAKE_BINARY_DIR}/proto/ge_autofuse_metadef_protos_af/proto/ascendc_ir.pb.cc"
)
set(AUTOFUSE_PROTO_OBJ_SRCS
    "${CMAKE_BINARY_DIR}/proto/ge_autofuse_metadef_protos_af/proto/ascendc_ir.pb.cc"
)

if (NOT TARGET ge_autofuse_metadef_protos_af)
    set(AUTOFUSE_PROTO_LIST
        "${AUTOFUSE_PROTO_DIR}/af_ir.proto"
        "${AUTOFUSE_PROTO_DIR}/ascendc_ir.proto"
    )
    protobuf_generate(ge_autofuse_metadef_protos_af AUTOFUSE_PROTO_SRCS AUTOFUSE_PROTO_HDRS ${AUTOFUSE_PROTO_LIST} TARGET)
endif ()

if (NOT TARGET ge_autofuse_metadef_graph_protos_obj_af)
    add_library(ge_autofuse_metadef_graph_protos_obj_af OBJECT ${AUTOFUSE_PROTO_OBJ_SRCS})
    add_dependencies(ge_autofuse_metadef_graph_protos_obj_af ge_autofuse_metadef_protos_af)
    target_compile_definitions(ge_autofuse_metadef_graph_protos_obj_af PRIVATE
        google=ascend_private
        PROTOBUF_INLINE_NOT_IN_HEADERS=0
        _GLIBCXX_USE_CXX11_ABI=0
    )
    target_compile_options(ge_autofuse_metadef_graph_protos_obj_af PRIVATE
        -fPIC
        -O2
        -fno-common
        -Wextra
        -Wfloat-equal
    )
    target_link_libraries(ge_autofuse_metadef_graph_protos_obj_af PRIVATE ascend_protobuf)
endif ()
