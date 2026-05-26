cmake_minimum_required(VERSION 3.16.1)
project(autofuse)

enable_testing()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
add_compile_options(-D_GLIBCXX_USE_CXX11_ABI=0)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all -Wl,-z,now")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all -Wl,-z,now")

if (EXISTS "${CMAKE_SOURCE_DIR}/.dev_env")
    # 存在`.dev_env`文件说明是蓝区开发环境，并且调用了`prepare_dev_env.sh`完成了开发环境准备
    # 除了设置好`.dev_env`中有的环境变量外，还会设置其他几个蓝区开发的必要变量
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.dev_env")
        set(ENABLE_OPEN_SRC True)
        # Read the file into a list of lines
        file(STRINGS "${CMAKE_SOURCE_DIR}/.dev_env" DEV_ENV_LINES)

        # Process each line
        foreach (line IN LISTS DEV_ENV_LINES)
            string(STRIP "${line}" line)
            if (line MATCHES "^([^=]+)=(.*)$")
                set(var_name "${CMAKE_MATCH_1}")
                set(var_value "${CMAKE_MATCH_2}")
                # Remove any leading/trailing whitespace
                string(STRIP "${var_name}" var_name)
                string(STRIP "${var_value}" var_value)
                # Set the variable
                set(${var_name} ${var_value})
            endif ()
        endforeach ()

        set(ASCEND_INSTALL_PATH ${ASCEND_INSTALL_PATH}/latest)
    endif ()
endif ()
set(CODE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../compiler/graph/optimize/autofuse)
set(ASCEND_ROOT ${ASCEND_INSTALL_PATH}) # e.g. /usr/local/Ascend/ascend-toolkit/latest

set(ASCEND_INSTALL_PATH ${ASCEND_INSTALL_PATH})

if (ENABLE_OPEN_SRC)
    if (NOT DEFINED CANN_3RD_LIB_PATH)
        set(CANN_3RD_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/output/third_party)
    endif ()

    if (NOT EXISTS "${CANN_3RD_LIB_PATH}")
        message(FATAL_ERROR "Error: Directory \"${CANN_3RD_LIB_PATH}\" does not exist!\n"
                "Please using build_third_party.sh compile first.")
    endif ()
    #    include(../../cmake/test_utils.cmake)
    set(GE_METADEF_DIR ${CODE_ROOT_DIR}/../../../../graph_metadef)
    set(GE_METADEF_INC_DIR ${CODE_ROOT_DIR}/../../../../inc/graph_metadef)
else ()
    set(GE_METADEF_DIR ${TOP_DIR}/air/graph_metadef)
    set(GE_METADEF_INC_DIR ${TOP_DIR}/air/inc/graph_metadef)
endif ()

if (NOT DEFINED CMAKE_MODULE_PATH)
    set(CMAKE_MODULE_PATH
            ${CANN_3RD_LIB_PATH}/cmake/modules
            ${GE_METADEF_DIR}/cmake/modules
            ${CODE_ROOT_DIR}/cmake/third_party/scripts/modules
    )
endif()

if (NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH
            ${CANN_3RD_LIB_PATH}/json
            ${CANN_3RD_LIB_PATH}/openssl
            ${CANN_3RD_LIB_PATH}/protoc
            ${CANN_3RD_LIB_PATH}/protoc_grpc
            ${CANN_3RD_LIB_PATH}/grpc
            ${CANN_3RD_LIB_PATH}/protobuf_static
            ${CANN_3RD_LIB_PATH}/ascend_protobuf
            ${CANN_3RD_LIB_PATH}/ascend_protobuf_static
            ${CANN_3RD_LIB_PATH}/gtest_shared/lib64/cmake/GTest
            ${CANN_3RD_LIB_PATH}/gtest
            ${CANN_3RD_LIB_PATH}/benchmark
            ${CANN_3RD_LIB_PATH}/pybind11
            ${CANN_3RD_LIB_PATH}/symengine/lib/cmake/symengine
            ${CANN_3RD_LIB_PATH}/boost/lib/cmake/Boost-1.87.0
    )
endif()


link_directories(${ASCEND_ROOT}/lib64)
include_directories(${AIR_CODE_DIR}/inc)
include_directories(${AIR_CODE_DIR}/inc/graph_metadef)
include_directories(${AIR_CODE_DIR}/inc/graph_metadef/graph)
include_directories(${AIR_CODE_DIR}/inc/graph_metadef/external)
include_directories(${AIR_CODE_DIR}/inc/graph_metadef/external/graph)
include_directories(${AIR_CODE_DIR}/inc/graph_metadef/external/exe_graph)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/graph_metadef/graph/ascendc_ir/ascendc_ir_core)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/graph_metadef/graph/ascendc_ir)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/ascir)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/ascir/meta)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/ascir/utils)
include_directories(${GE_METADEF_INC_DIR}/external)
include_directories(${GE_METADEF_INC_DIR}/external/graph)
include_directories(${GE_METADEF_INC_DIR})
include_directories(${GE_METADEF_INC_DIR}/graph)
include_directories(${GE_METADEF_INC_DIR}/graph/utils)
include_directories(${GE_METADEF_DIR}/graph)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/graph_metadef/graph)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/graph_metadef)
include_directories(${CODE_ROOT_DIR}/inc)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/common)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/utils)
include_directories(${ASCEND_ROOT}/include/experiment)
include_directories(${ASCEND_ROOT}/include/experiment/runtime)
include_directories(${ASCEND_ROOT}/include/experiment/msprof)
include_directories(${CMAKE_BINARY_DIR}/proto/metadef_protos)
include_directories(${ASCEND_INSTALL_PATH}/pkg_inc)
include_directories(${ASCEND_INSTALL_PATH}/pkg_inc/base)
include_directories(${ASCEND_INSTALL_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/include/external)



find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
if (ENABLE_OPEN_SRC)
    add_cann_third_party(json)
    find_package(GTest CONFIG REQUIRED)
    find_package(Boost CONFIG REQUIRED)
    find_package(SymEngine CONFIG REQUIRED)
    find_package(ascend_protobuf_shared MODULE REQUIRED)
    find_package(unified_dlog)
    find_package(mmpa)
    find_package(runtime)
    find_package(atrace)
endif ()

include(GoogleTest)
set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)

if (NOT TARGET aihac_codegen_toolkit)
    add_library(aihac_codegen_toolkit SHARED IMPORTED)
    set_target_properties(aihac_codegen_toolkit PROPERTIES
            IMPORTED_LOCATION "${ASCEND_INSTALL_PATH}/lib64/libaihac_codegen.so"
            INTERFACE_LINK_LIBRARIES "ascend_hal_stub;atrace_share")
endif ()

add_subdirectory(depends/slog)
add_subdirectory(depends/trace)
add_subdirectory(depends/runtime)
add_subdirectory(depends/common)
add_subdirectory(framework)
add_subdirectory(st)
add_subdirectory(ut)

include(CMakePrintHelpers)
message(STATUS "Variables in project:")
cmake_print_variables(CMAKE_PREFIX_PATH)
cmake_print_variables(INSTALL_BASE_DIR)
cmake_print_variables(ENABLE_OPEN_SRC)

if (ENABLE_OPEN_SRC)
    find_package(protoc MODULE REQUIRED)
    find_package(ascend_protobuf_static MODULE REQUIRED)
endif ()

######################################autofuse##########################################
set(AUTOFUSE_DIR ${CODE_ROOT_DIR}/autofuse)
file(GLOB_RECURSE AUTOFUSE_SOURCES "${AUTOFUSE_DIR}/*.cpp")

add_library(autofuse SHARED
   ${AUTOFUSE_SOURCES}
)
target_compile_definitions(autofuse PRIVATE
        PROTOBUF_INLINE_NOT_IN_HEADERS=0
        google=ascend_private
        AUTOFUSE_USE_GE_METADEF
)

target_include_directories(autofuse PUBLIC
    ${AUTOFUSE_DIR}
    ${CODE_ROOT_DIR}
    ${CODE_ROOT_DIR}/autofuse
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/proto/metadef_protos
)

target_compile_options(autofuse PRIVATE -fPIC -Werror -fno-common -Wextra -Wfloat-equal -O0)

if (ENABLE_OPEN_SRC)
    target_link_libraries(autofuse PUBLIC
        -lgcov
        mmpa_headers
        metadef_headers
        ascend_protobuf
        c_sec
        json
        atrace
        unified_dlog
        error_manager
        graph_base
        graph
        aihac_symbolizer
        autofuse_runtime_stub
        aihac_codegen_toolkit
    )
else ()
    target_link_libraries(autofuse PUBLIC
        graph
        unified_dlog
        atrace_share
        graph_base
        mmpa_headers
        metadef_headers
        atrace_headers
        error_manager
        ascend_protobuf
        c_sec
        json
        aihac_symbolizer
        aihac_codegen
        runtime_headers
    )
endif ()

