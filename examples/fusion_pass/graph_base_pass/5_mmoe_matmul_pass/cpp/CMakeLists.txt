cmake_minimum_required(VERSION 3.5.1)
project(mmoe_matmul_pass)

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)

if (DEFINED ENV{ASCEND_HOME_PATH})
    set(ASCEND_PATH $ENV{ASCEND_HOME_PATH})
else ()
    set(ASCEND_PATH /usr/local/Ascend/cann)
endif ()

set(PASS_SO_DIR pass_so_dir)
set(INCLUDE_DIR ${ASCEND_PATH}/include)

aux_source_directory(${PROJECT_SOURCE_DIR}/src SRC_LIST)

add_library(${PROJECT_NAME} SHARED ${SRC_LIST})

set_target_properties(${PROJECT_NAME} PROPERTIES output_name ${PROJECT_NAME})

target_compile_options(${PROJECT_NAME} PRIVATE -std=c++17 -g -Wall)
target_compile_definitions(${PROJECT_NAME} PRIVATE _GLIBCXX_USE_CXX11_ABI=0)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${ASCEND_PATH}/opp/built-in/op_proto/inc
    ${INCLUDE_DIR}/graph
    ${INCLUDE_DIR}/ge
    ${INCLUDE_DIR}/register
    ${INCLUDE_DIR}
    ${INCLUDE_DIR}/external
)

target_link_directories(${PROJECT_NAME} PRIVATE
    ${ASCEND_PATH}/lib64/stub
    ${ASCEND_PATH}/lib64
    ${ASCEND_PATH}/aarch64-linux/lib64
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    -Wl,--no-as-needed
    graph
    register
    -Wl,--as-needed
)

install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION ${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes
)

add_custom_target(clean_custom_pass
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ASCEND_PATH}/opp/vendors/${PASS_SO_DIR}/custom_fusion_passes/lib${PROJECT_NAME}.so
)
