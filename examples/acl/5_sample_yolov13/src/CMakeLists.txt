# Copyright (c) Huawei Technologies Co., Ltd. 2015. All rights reserved.

cmake_minimum_required(VERSION 3.5.1)

project(ACL_YOLOV13)

add_compile_options(-std=c++17)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "../../../out")
set(CMAKE_CXX_FLAGS_DEBUG "-fPIC -O0 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-fPIC -O2 -Wall")

set(INC_PATH $ENV{ASCEND_HOME_PATH})

if (NOT DEFINED ENV{ASCEND_HOME_PATH})
    message(WARNING "please source the set_env.sh")
    set(INC_PATH "/usr/local/Ascend/cann")
    message(STATUS "set default INC_PATH: ${INC_PATH}")
else ()
    message(STATUS "set INC_PATH: ${INC_PATH}")
endif ()

set(LIB_PATH "${INC_PATH}/devlib")
message(STATUS "set LIB_PATH: ${LIB_PATH}")

include_directories(
    ${INC_PATH}/include/
    ../inc/
)

link_directories(
    ${LIB_PATH}
)

add_executable(yolov13_main
        ../../common/utils.cpp
        ../../common/sampleModel.cpp
        sample_yolov13.cpp)

target_link_libraries(yolov13_main acl_rt acl_mdl stdc++)

install(TARGETS yolov13_main DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
