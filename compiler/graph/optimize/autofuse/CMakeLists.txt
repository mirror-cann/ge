set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fstack-protector-all -Wl,-z,now")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-all -Wl,-z,now")

if(NOT MDC_COMPILE_RUNTIME)
    add_compile_options(-D_GLIBCXX_USE_CXX11_ABI=0)
else()
    add_compile_options(-D_GLIBCXX_USE_CXX11_ABI=${USE_CXX11_ABI})
endif()

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
set(ASCEND_ROOT ${ASCEND_INSTALL_PATH}) # e.g. /usr/local/Ascend/ascend-toolkit/latest
set(ASCEND_INSTALL_PATH ${ASCEND_INSTALL_PATH})

set(CODE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if (ENABLE_OPEN_SRC)
    set(GE_METADEF_DIR ${CODE_ROOT_DIR}/../../../../graph_metadef)
    set(GE_METADEF_INC_DIR ${CODE_ROOT_DIR}/../../../../inc/graph_metadef)

    if (NOT DEFINED CANN_3RD_LIB_PATH)
        set(CANN_3RD_LIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/output/third_party)
    endif ()

    if (NOT EXISTS "${CANN_3RD_LIB_PATH}")
        message(FATAL_ERROR "Error: Directory \"${CANN_3RD_LIB_PATH}\" does not exist!\n"
                "Please using build_third_party.sh compile first.")
    endif ()
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
# Keep the dedicated autofuse ascendc_ir directories ahead of GE metadef paths so
# bare includes like "attr_store.h" resolve to the af-namespace package headers.
# Keep generic "graph/..." lookups on GE metadef first under AUTOFUSE_USE_GE_METADEF.
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
include_directories(${AIR_CODE_DIR}/inc)
include_directories(${ASCEND_ROOT}/include)
include_directories(${ASCEND_INSTALL_PATH}/pkg_inc)
include_directories(${ASCEND_INSTALL_PATH}/pkg_inc/base)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/common)
include_directories(${ASCEND_INSTALL_PATH}/include/autofuse/utils)
if (NOT ENABLE_OPEN_SRC)
    include_directories(${TOP_DIR}/air/inc/)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/att)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/autofuse)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/autofuse/utils)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/common)
    #include_directories(${TOP_DIR}/graph-autofusion/autofuse/inc/graph_metadef/)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/inc/graph_metadef/graph/ascendc_ir/ascendc_ir_core)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/inc/)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/inc/fusion)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/ascir/meta)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/inc/autofuse_frame)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/inc/graph_metadef/graph)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/inc/graph_metadef/graph/utils)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/graph_metadef/graph)
    include_directories(${TOP_DIR}/graph-autofusion/autofuse/graph_metadef/third_party/transformer/inc)
endif ()

if(DEFINED BUILD_COMPONENT)
    include_directories(${ASCEND_ROOT}/pkg_inc)
    include_directories(${ASCEND_ROOT}/pkg_inc/runtime)
    include_directories(${ASCEND_ROOT}/pkg_inc/profiling)
else()
    include_directories(${ASCEND_ROOT}/include/experiment)
    include_directories(${ASCEND_ROOT}/include/experiment/runtime)
    include_directories(${ASCEND_ROOT}/include/experiment/msprof)
endif()
include_directories(${CMAKE_BINARY_DIR}/proto/metadef_protos)
include_directories(${CMAKE_BINARY_DIR}/proto/metadef_protos_af)
include_directories(${CMAKE_BINARY_DIR}/ascir_builtin_ops)

if (ENABLE_OPEN_SRC)
    add_cann_third_party(json)
    if(NOT BUILD_PKG_COMPONENT)
        find_package(GTest CONFIG REQUIRED)
    endif()
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

# Common depends
add_library(depends INTERFACE)
target_link_directories(depends INTERFACE
    ${ASCEND_ROOT}/x86_64-linux/devlib)

add_library(aihac_codegen_toolkit SHARED IMPORTED)
set_target_properties(aihac_codegen_toolkit PROPERTIES
    IMPORTED_LOCATION "${ASCEND_INSTALL_PATH}/lib64/libaihac_codegen.so")

if (DEFINED RUN_TEST)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
endif ()

add_subdirectory(autofuse)

# #########################以下内容是生成一个autofuse的so################################
install(TARGETS aihac_autofusion
        LIBRARY DESTINATION lib ${INSTALL_OPTIONAL}
        ARCHIVE DESTINATION lib ${INSTALL_OPTIONAL}
        RUNTIME DESTINATION bin ${INSTALL_OPTIONAL}
)

if (NOT ENABLE_OPEN_SRC)
    install_package(
        PACKAGE ascgen
        TARGETS aihac_autofusion
        DESTINATION ${INSTALL_INCLUDE_DIR}/ascgen
    )
endif ()

if (NOT DEFINED BUILD_COMPONENT AND ENABLE_OPEN_SRC)
    set(ARCH_LINX_PATH "${CMAKE_SYSTEM_PROCESSOR}-linux")

    install(TARGETS aihac_autofusion
        LIBRARY DESTINATION ${ARCH_LINX_PATH}/lib64 ${INSTALL_OPTIONAL} COMPONENT packages EXCLUDE_FROM_ALL
        ARCHIVE DESTINATION ${ARCH_LINX_PATH}/lib64 ${INSTALL_OPTIONAL} COMPONENT packages EXCLUDE_FROM_ALL
        RUNTIME DESTINATION ${ARCH_LINX_PATH}/bin ${INSTALL_OPTIONAL} COMPONENT packages EXCLUDE_FROM_ALL
    )


    set(version_info "${ASCEND_INSTALL_PATH}/runtime/version.info")
    set(install_script_dir "${CMAKE_CURRENT_BINARY_DIR}/install_scripts/")

    add_custom_target(generate_install_script_autofuse ALL
        COMMAND rm -rf ${install_script_dir}
        COMMAND cp -rf ${ASCEND_INSTALL_PATH}/toolkit/tools/ascend_project/open_install_scripts ${install_script_dir}
        COMMAND chmod -R u+w ${install_script_dir}
        COMMAND echo "base_package=compiler" > ${install_script_dir}/version.info
        COMMAND echo "backup_dir=autofuse" >> ${install_script_dir}/version.info
        COMMAND grep -iE "'^(version|version_dir)='" ${version_info} >> ${install_script_dir}/version.info
    )

    install(DIRECTORY ${install_script_dir}
        DESTINATION . ${INSTALL_OPTIONAL}
        FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_READ
        COMPONENT . EXCLUDE_FROM_ALL
    )

    set(CPACK_COMPONENTS_ALL ".;packages")
    set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
    set(CPACK_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
    set(CPACK_PACKAGE_DESCRIPTION "CPack autofuse project")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CPack autofuse project")
    set(CPACK_PACKAGE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/package)
    set(CPACK_PACKAGE_FILE_NAME "CANN-autofuse-linux.${CMAKE_SYSTEM_PROCESSOR}.run")
    set(CPACK_GENERATOR External)
    set(CPACK_CMAKE_GENERATOR "Unix Makefiles")
    set(CPACK_EXTERNAL_ENABLE_STAGING TRUE)
    set(CPACK_EXTERNAL_PACKAGE_SCRIPT ${ASCEND_INSTALL_PATH}/toolkit/tools/op_project_templates/ascendc/customize/cmake/makeself.cmake)
    set(CPACK_EXTERNAL_BUILT_PACKAGES ${CPACK_PACKAGE_DIRECTORY}/_CPack_Packages/Linux/External/${CPACK_PACKAGE_FILE_NAME}/${CPACK_PACKAGE_FILE_NAME})
    include(CPack)
endif()
