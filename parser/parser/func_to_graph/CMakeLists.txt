# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

############ libfunc2graph.so ############
add_library(func2graph SHARED
        $<TARGET_OBJECTS:parser_graph_library_proto_obj>
        $<TARGET_OBJECTS:parser_tensorflow_protos_obj>
        func2graph.cc)

target_include_directories(func2graph PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(func2graph PRIVATE
        google=ascend_private
)

target_compile_options(func2graph PRIVATE
        -D_GLIBCXX_USE_CXX11_ABI=0
        -fstack-protector-all
        -fvisibility-inlines-hidden
)

target_link_libraries(func2graph PUBLIC
        ge_intf_pub
        parser_headers
        ascend_protobuf_static
)
