cmake_minimum_required(VERSION 3.14)
project(compilable_add_custom LANGUAGES CXX)

# 分别控制 GE 交付件、graph_build 程序、OM 执行程序的构建。
option(COMPILABLE_ADD_BUILD_CUSTOM_OP "Build libcust_opapi.so for GE" ON)
option(COMPILABLE_ADD_BUILD_GRAPH_BUILD "Build graph_build exporter for ATC" ON)
option(COMPILABLE_ADD_BUILD_MODEL_EXEC "Build OM execution demo" ON)

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

# 与 sample 运行时目录对齐：AIR、OM 保持在 output/ 下，自定义算子交付件按 OPP 目录规范输出。
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
set(KERNEL_SOURCE_FILE "${CMAKE_SOURCE_DIR}/ge/add_custom_kernel.asc")
set(OP_PROTO_HEADER_SOURCE_FILE "${CMAKE_SOURCE_DIR}/ge/add_custom.h")
configure_file("${KERNEL_SOURCE_FILE}" "${CUSTOM_OP_OUTPUT_DIR}/add_custom_kernel.asc" COPYONLY)
configure_file("${OP_PROTO_HEADER_SOURCE_FILE}" "${CUSTOM_OP_INCLUDE_DIR}/add_custom.h" COPYONLY)

# 默认从环境变量读取 CANN 安装路径，也支持通过 cache 变量手动覆盖。
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

if(COMPILABLE_ADD_BUILD_CUSTOM_OP)
  # GE 侧自定义算子交付件，供 ATC/GE 装载。
  # 这里将执行逻辑（custom_op.cpp）、编译辅助工具（compile_utils.cpp）
  # 和多 bin 序列化工具（kernel_binary_map_utils.cpp）
  # 与 shape/type 推导注册（infershape.cpp）
  # 一并编入同一个 libcust_opapi.so，便于按单一交付件给 GE/ATC 使用。
  add_library(cust_opapi SHARED
    ge/utils/compile_utils.cpp
    ge/custom_op.cpp
    ge/utils/kernel_binary_map_utils.cpp
    ge/infershape.cpp
  )
  target_compile_options(cust_opapi PRIVATE ${COMMON_COMPILE_OPTIONS})
  target_compile_definitions(cust_opapi PRIVATE _GLIBCXX_USE_CXX11_ABI=0)

  if(ASCEND_HOME_PATH)
    target_include_directories(cust_opapi PRIVATE
      "${ASCEND_HOME_PATH}/include"
      "${ASCEND_HOME_PATH}/include/graph"
      "${ASCEND_HOME_PATH}/include/register"
      "${ASCEND_HOME_PATH}/include/external"
    )
    target_link_directories(cust_opapi PRIVATE "${ASCEND_HOME_PATH}/lib64")
    target_link_libraries(cust_opapi PRIVATE
      -Wl,--no-as-needed
      ascendcl
      acl_rtc
      lowering
      register
      gert
      dl
      -Wl,--as-needed
    )
  endif()

  set_target_properties(cust_opapi PROPERTIES
    OUTPUT_NAME "cust_opapi"
    LIBRARY_OUTPUT_DIRECTORY "${CUSTOM_OP_OUTPUT_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CUSTOM_OP_OUTPUT_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CUSTOM_OP_OUTPUT_DIR}"
  )

  install(FILES "${KERNEL_SOURCE_FILE}"
    DESTINATION "${CUSTOM_OP_OUTPUT_DIR}"
  )
  install(FILES "${OP_PROTO_HEADER_SOURCE_FILE}"
    DESTINATION "${CUSTOM_OP_INCLUDE_DIR}"
  )

endif()

if(COMPILABLE_ADD_BUILD_GRAPH_BUILD)
  # graph_build 程序：运行后在 output/ 目录下导出 single_add.air。
  add_executable(compilable_add_graph_build
    graph_build/main.cc
  )
  target_compile_options(compilable_add_graph_build PRIVATE ${COMMON_COMPILE_OPTIONS})
  target_compile_definitions(compilable_add_graph_build PRIVATE
    _GLIBCXX_USE_CXX11_ABI=0
  )

  if(ASCEND_HOME_PATH)
    target_include_directories(compilable_add_graph_build PRIVATE
      "${CUSTOM_OP_INCLUDE_DIR}"
      "${ASCEND_HOME_PATH}/include"
      "${ASCEND_HOME_PATH}/include/graph"
      "${ASCEND_HOME_PATH}/include/ge"
      "${ASCEND_HOME_PATH}/opp/built-in/op_proto/inc"
    )
    target_link_directories(compilable_add_graph_build PRIVATE "${ASCEND_HOME_PATH}/lib64")
    target_link_libraries(compilable_add_graph_build PRIVATE
      graph
      ge_compiler
      ascendcl
      graph_base
    )
  endif()

  set_target_properties(compilable_add_graph_build PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
endif()

if(COMPILABLE_ADD_BUILD_MODEL_EXEC)
  # ACL 执行程序：负责加载命令行传入的 OM 路径并执行。
  add_executable(compilable_add_model_exec
    model_exec/main.cc
  )
  target_compile_options(compilable_add_model_exec PRIVATE ${COMMON_COMPILE_OPTIONS})
  target_compile_definitions(compilable_add_model_exec PRIVATE _GLIBCXX_USE_CXX11_ABI=0)

  if(ASCEND_HOME_PATH)
    target_include_directories(compilable_add_model_exec PRIVATE
      "${ASCEND_HOME_PATH}/include"
    )
    target_link_directories(compilable_add_model_exec PRIVATE "${ASCEND_HOME_PATH}/lib64")
    target_link_libraries(compilable_add_model_exec PRIVATE
      ascendcl
    )
  endif()

  set_target_properties(compilable_add_model_exec PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
  )
endif()
