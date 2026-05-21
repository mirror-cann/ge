cmake_minimum_required(VERSION 3.16)
project(add_custom_zero_pass_python_es LANGUAGES CXX)

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

#生成自定义算子API及wheel
add_library(custom_op_proto SHARED
        ../cpp/proto/add_custom_proto.cc)
target_compile_definitions(custom_op_proto PRIVATE OP_PROTO_LIB)
target_compile_options(custom_op_proto PRIVATE
        -fvisibility=hidden
)
if (ENABLE_CROSS_COMPILE)
    target_link_directories(custom_op_proto PRIVATE
            ${CMAKE_COMPILE_COMPILER_LIBRARY}
            ${CMAKE_COMPILE_RUNTIME_LIBRARY}
    )
endif()

add_es_library_and_whl(
        ES_LINKABLE_AND_ALL_TARGET es_custom
        OPP_PROTO_TARGET custom_op_proto
        OUTPUT_PATH ${ES_OUTPUT_PATH}
)
