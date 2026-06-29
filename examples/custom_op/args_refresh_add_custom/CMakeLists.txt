cmake_minimum_required(VERSION 3.14)
project(args_refresh_add_custom LANGUAGES CXX)

option(ARGS_REFRESH_BUILD_CUSTOM_OP "Build libcust_opapi.so" ON)
option(ARGS_REFRESH_BUILD_SESSION_RUN "Build session_run" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(COMMON_COMPILE_OPTIONS
  -Wall
  -Wextra
  -Wno-unused-parameter
)

set(PROJECT_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/output")
file(MAKE_DIRECTORY "${PROJECT_OUTPUT_DIR}")
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(OPP_OS_TYPE "windows")
else()
  set(OPP_OS_TYPE "linux")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" CMAKE_SYSTEM_PROCESSOR_LOWER)
if(CMAKE_SYSTEM_PROCESSOR_LOWER MATCHES "^(aarch64|arm64)$")
  set(OPP_CPU_TYPE "aarch64")
elseif(CMAKE_SYSTEM_PROCESSOR_LOWER MATCHES "^(x86_64|amd64)$")
  set(OPP_CPU_TYPE "x86_64")
else()
  set(OPP_CPU_TYPE "${CMAKE_SYSTEM_PROCESSOR_LOWER}")
endif()

set(CUSTOM_OP_OUTPUT_DIR "${PROJECT_OUTPUT_DIR}/op_graph/lib/${OPP_OS_TYPE}/${OPP_CPU_TYPE}")
file(MAKE_DIRECTORY "${CUSTOM_OP_OUTPUT_DIR}")
set(CUSTOM_OP_INCLUDE_DIR "${PROJECT_OUTPUT_DIR}/op_graph/include")
file(MAKE_DIRECTORY "${CUSTOM_OP_INCLUDE_DIR}")
set(OP_PROTO_HEADER_SOURCE_FILE "${CMAKE_SOURCE_DIR}/ge/add_custom.h")
configure_file("${OP_PROTO_HEADER_SOURCE_FILE}" "${CUSTOM_OP_INCLUDE_DIR}/add_custom.h" COPYONLY)

set(ASCEND_HOME_PATH_OVERRIDE "" CACHE PATH "Optional ASCEND_HOME_PATH override")
if(ASCEND_HOME_PATH_OVERRIDE)
  set(ASCEND_HOME_PATH "${ASCEND_HOME_PATH_OVERRIDE}")
else()
  set(ASCEND_HOME_PATH "$ENV{ASCEND_HOME_PATH}")
endif()

if(ASCEND_HOME_PATH)
  message(STATUS "ASCEND_HOME_PATH: ${ASCEND_HOME_PATH}")
else()
  message(WARNING "ASCEND_HOME_PATH is empty. Configure succeeds, but compilation requires a valid CANN toolkit path.")
endif()

add_definitions(-D__ASCEND_C__)

if(ARGS_REFRESH_BUILD_CUSTOM_OP)
  set(KERNEL_SOURCE_FILE "${CMAKE_SOURCE_DIR}/add_custom_kernel/add_custom.asc")
  configure_file("${KERNEL_SOURCE_FILE}" "${CUSTOM_OP_OUTPUT_DIR}/add_custom.asc" COPYONLY)

  add_library(cust_opapi SHARED
    ge/custom_op.cpp
    ge/utils/rtc_kernel_loader.cpp
  )
  target_compile_options(cust_opapi PRIVATE
    ${COMMON_COMPILE_OPTIONS}
  )
  target_compile_definitions(cust_opapi PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
  )

  if(ASCEND_HOME_PATH)
    target_include_directories(cust_opapi PRIVATE
      "${CMAKE_SOURCE_DIR}/add_custom_kernel"
      "${ASCEND_HOME_PATH}/include"
      "${ASCEND_HOME_PATH}/include/graph"
      "${ASCEND_HOME_PATH}/include/register"
      "${ASCEND_HOME_PATH}/include/external"
    )
    target_link_directories(cust_opapi PRIVATE
      "${ASCEND_HOME_PATH}/lib64"
    )
    target_link_libraries(cust_opapi PRIVATE
      -Wl,--no-as-needed
      ascendcl
      acl_rtc
      acl_rt
      ge_compiler
      ge_common
      ge_executor
      ge_runner
      ge_runner_v2
      runtime
      register
      gert
      pthread
      dl
      lowering
      -Wl,--as-needed
    )
  endif()

  set_target_properties(cust_opapi PROPERTIES
    OUTPUT_NAME "cust_opapi"
    LIBRARY_OUTPUT_DIRECTORY "${CUSTOM_OP_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CUSTOM_OP_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CUSTOM_OP_OUTPUT_DIR}"
  )

endif()

if(ARGS_REFRESH_BUILD_SESSION_RUN)
  add_executable(args_refresh_session_run
    session_run/main.cc
  )
  target_compile_options(args_refresh_session_run PRIVATE ${COMMON_COMPILE_OPTIONS})
  target_compile_definitions(args_refresh_session_run PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
  )

  if(ASCEND_HOME_PATH)
    target_include_directories(args_refresh_session_run PRIVATE
      "${CUSTOM_OP_INCLUDE_DIR}"
      "${CMAKE_SOURCE_DIR}/ge"
      "${ASCEND_HOME_PATH}/include"
      "${ASCEND_HOME_PATH}/include/graph"
      "${ASCEND_HOME_PATH}/include/ge"
      "${ASCEND_HOME_PATH}/opp/built-in/op_proto/inc"
    )
    target_link_directories(args_refresh_session_run PRIVATE "${ASCEND_HOME_PATH}/lib64")
    target_link_libraries(args_refresh_session_run PRIVATE
      graph
      ge_runner
      ge_compiler
      ascendcl
      graph_base
    )
  endif()

  set_target_properties(args_refresh_session_run PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
endif()
