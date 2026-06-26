cmake_minimum_required(VERSION 3.16)

project(samples)
set(ASCEND_INSTALL_PATH $ENV{ASCEND_HOME_PATH})
set(ASCEND_LIB_DIR ${ASCEND_INSTALL_PATH}/lib64)
message(STATUS "ASCEND_LIB_DIR = ${ASCEND_LIB_DIR}")

set(CMAKE_CXX_STANDARD 17)
add_compile_definitions(
        _GLIBCXX_USE_CXX11_ABI=0
        google=ascend_private
)

include_directories(${ASCEND_INSTALL_PATH})
include_directories(${ASCEND_INSTALL_PATH}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_PREFIX_PATH ${ASCEND_LIB_DIR} ${CMAKE_PREFIX_PATH})

find_library(ES_MATH NAMES es_math REQUIRED)
find_library(GRAPH NAMES graph REQUIRED)
find_library(GRAPH_BASE NAMES graph_base REQUIRED)
find_library(EAGER_STYLE_GRAPH_BUILDER_BASE NAMES eager_style_graph_builder_base REQUIRED)
find_library(C_SEC NAMES c_sec REQUIRED)
find_library(GE_COMPILE NAMES ge_compiler REQUIRED)
find_library(GE_RUNNER NAMES ge_runner REQUIRED)
find_library(ACL_RT NAMES acl_rt REQUIRED)
find_library(ACL_MDL NAMES acl_mdl REQUIRED)

add_subdirectory(src)
add_executable(offline_compile_sample main.cpp)
target_link_options(offline_compile_sample PRIVATE -Wl,--no-as-needed)
target_link_libraries(offline_compile_sample PRIVATE offline_graph_showcase ${ES_MATH} ${GE_RUNNER} ${GE_COMPILE} ${ACL_RT} ${ACL_MDL})
