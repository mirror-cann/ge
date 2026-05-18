cmake_minimum_required(VERSION 3.16)
project(decompose_grouped_conv_to_splited_pass_python_es LANGUAGES CXX)

if (DEFINED ENV{ASCEND_HOME_PATH})
    set(ASCEND_PATH $ENV{ASCEND_HOME_PATH})
else ()
    set(ASCEND_PATH /usr/local/Ascend/cann) # 可根据实际修改
endif()

set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(
        _GLIBCXX_USE_CXX11_ABI=0
        google=ascend_private
)

message(STATUS "ASCEND_PATH: ${ASCEND_PATH}")

list(APPEND CMAKE_MODULE_PATH "${ASCEND_PATH}/include/ge/cmake")
find_package(GenerateEsPackage REQUIRED)

set(CMAKE_CURRENT_BINARY_DIR ${PROJECT_SOURCE_DIR}/build)
message(STATUS "Binary directory: ${CMAKE_CURRENT_BINARY_DIR}")
set(ES_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/es_output)
message(STATUS "ES_OUTPUT_PATH: ${ES_OUTPUT_PATH}")

include_directories(
        ${ASCEND_PATH}/include
        ${ASCEND_PATH}/include/graph
)

# 生成内置算子 Python ES API，使 replacement graph 使用与 C++ 样例一致的 Conv2D。
add_library(opgraph_all INTERFACE)
set_target_properties(opgraph_all PROPERTIES
        INTERFACE_LIBRARY_OUTPUT_DIRECTORY "${ASCEND_PATH}/opp/built-in/op_proto"
)
add_es_library_and_whl(
        ES_LINKABLE_AND_ALL_TARGET es_all
        OPP_PROTO_TARGET opgraph_all
        OUTPUT_PATH ${ES_OUTPUT_PATH}
)
