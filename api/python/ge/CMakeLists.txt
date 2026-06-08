# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

add_subdirectory(ge_api_c_wrapper)
add_subdirectory(ge/passes)
file(GLOB_RECURSE GE_API_C_WRAPPER CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.py")

execute_process(
         COMMAND ${HI_PYTHON} -c "import sys; print(f'cp{sys.version_info.major}{sys.version_info.minor}')"
         OUTPUT_VARIABLE GE_PYTHON_PASS_PYTHON_TAG
         OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
         COMMAND ${HI_PYTHON} -c "import sys; print(sys.version.split()[0])"
         OUTPUT_VARIABLE GE_PYTHON_PASS_PYTHON_VERSION
         OUTPUT_STRIP_TRAILING_WHITESPACE
)
string(TOLOWER "${CMAKE_SYSTEM_NAME}" GE_PYTHON_PASS_SYSTEM_NAME)
set(GE_PYTHON_PASS_PLATFORM_TAG "${GE_PYTHON_PASS_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
string(REPLACE "-" "_" GE_PYTHON_PASS_WHEEL_PLATFORM "${GE_PYTHON_PASS_PLATFORM_TAG}")
set(GE_PYTHON_PASS_ARTIFACT_SET_NAME "${GE_PYTHON_PASS_PYTHON_TAG}-${GE_PYTHON_PASS_PLATFORM_TAG}")
set(GE_PYTHON_MAIN_WHEEL ${CMAKE_CURRENT_BINARY_DIR}/ge_py-0.0.1-py3-none-any.whl)
set(GE_PYTHON_MAIN_WHEEL_DIR ${CMAKE_CURRENT_BINARY_DIR}/wheel)
set(GE_PYTHON_PASS_ARTIFACT_DIR
        ${CMAKE_CURRENT_BINARY_DIR}/native_artifacts/${GE_PYTHON_PASS_ARTIFACT_SET_NAME})
set(GE_PYTHON_PASS_BRIDGE_ABI_VERSION 1)
set(GE_PYTHON_PASS_ARTIFACT_MANIFEST ${CMAKE_CURRENT_BINARY_DIR}/python_pass_artifact_manifest.json)
set(GE_PYTHON_PASS_NATIVE_WHEEL
        ${CMAKE_CURRENT_BINARY_DIR}/ge_py_pass_bridge-0.0.1-${GE_PYTHON_PASS_PYTHON_TAG}-${GE_PYTHON_PASS_PYTHON_TAG}-${GE_PYTHON_PASS_WHEEL_PLATFORM}.whl)
set(GE_PYTHON_PASS_NATIVE_WHEEL ${GE_PYTHON_PASS_NATIVE_WHEEL} CACHE INTERNAL "GE python pass native artifact wheel")
set(GE_PYTHON_PASS_NATIVE_MATRIX_DIST_DIR ${CMAKE_CURRENT_BINARY_DIR}/native_wheel_matrix_dist)
set(GE_PYTHON_PASS_MATRIX_COMPILER_ARGS "--cxx-compiler" "${CMAKE_CXX_COMPILER}")
if (CMAKE_CXX_COMPILER_LAUNCHER)
    list(APPEND GE_PYTHON_PASS_MATRIX_COMPILER_ARGS
            "--cxx-compiler-launcher" "${CMAKE_CXX_COMPILER_LAUNCHER}")
endif()
file(WRITE ${GE_PYTHON_PASS_ARTIFACT_MANIFEST}
        "{\n"
        "  \"python_tag\": \"${GE_PYTHON_PASS_PYTHON_TAG}\",\n"
        "  \"python_version\": \"${GE_PYTHON_PASS_PYTHON_VERSION}\",\n"
        "  \"platform\": \"${GE_PYTHON_PASS_PLATFORM_TAG}\",\n"
        "  \"bridge_abi\": ${GE_PYTHON_PASS_BRIDGE_ABI_VERSION},\n"
        "  \"build_python\": {\n"
        "    \"executable\": \"${HI_PYTHON}\",\n"
        "    \"version\": \"${GE_PYTHON_PASS_PYTHON_VERSION}\",\n"
        "    \"include\": \"${HI_PYTHON_INC}\",\n"
        "    \"libpython\": \"${HI_PYTHON_EMBED_LIBRARIES}\",\n"
        "    \"pybind11_include\": \"${pybind11_INCLUDE_DIR}\",\n"
        "    \"link_python\": true\n"
        "  },\n"
        "  \"artifacts\": {\n"
        "    \"bridge\": \"libge_python_pass_bridge.so\",\n"
        "    \"native\": \"_ge_pass_native.so\"\n"
        "  }\n"
        "}\n"
)
set(GE_PYTHON_PASS_FALLBACK_CODEGEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/python_pass_fallback_codegen)
set(GE_PYTHON_PASS_FALLBACK_CODEGEN_CONFIG
        ${GE_PYTHON_PASS_FALLBACK_CODEGEN_DIR}/build_config.json)
set(GE_PYTHON_PASS_FALLBACK_CODEGEN_SOURCES_PY
        ${GE_PYTHON_PASS_FALLBACK_CODEGEN_DIR}/_sources.py)
set(GE_PYTHON_PASS_FALLBACK_CODEGEN_INIT_PY
        ${GE_PYTHON_PASS_FALLBACK_CODEGEN_DIR}/__init__.py)
set(GE_PYTHON_PASS_BRIDGE_SOURCES
        ${CMAKE_SOURCE_DIR}/compiler/graph/fusion/pass/python_pass_pybind_bridge.cc)
set(GE_PYTHON_PASS_BRIDGE_HEADERS
        ${CMAKE_SOURCE_DIR}/compiler/graph/fusion/pass/python_pass_pybind_bridge.h
        ${CMAKE_SOURCE_DIR}/compiler/graph/fusion/pass/python_pass_adapter.h
        ${CMAKE_SOURCE_DIR}/compiler/graph/fusion/pass/pass_registry.h
        ${CMAKE_SOURCE_DIR}/compiler/graph/fusion/pass/python_pass_bridge_c_api.h)

add_custom_target(ge_python ALL)
add_custom_target(ge_python_main_wheel DEPENDS ${GE_PYTHON_MAIN_WHEEL})
add_custom_target(ge_python_native_wheel DEPENDS ${GE_PYTHON_PASS_NATIVE_WHEEL})
add_custom_target(ge_python_pass_native_wheel_matrix
         COMMAND ${HI_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/python_pass_native_build/build_python_pass_native_matrix.py
            --source-dir ${AIR_CODE_DIR}
            --build-dir ${CMAKE_BINARY_DIR}
            --work-dir ${CMAKE_CURRENT_BINARY_DIR}/native_wheel_matrix_build
            --dist-dir ${GE_PYTHON_PASS_NATIVE_MATRIX_DIST_DIR}
            --platform-tag ${GE_PYTHON_PASS_PLATFORM_TAG}
            --wheel-platform ${GE_PYTHON_PASS_WHEEL_PLATFORM}
            --bridge-abi ${GE_PYTHON_PASS_BRIDGE_ABI_VERSION}
            --current-tag ${GE_PYTHON_PASS_PYTHON_TAG}
            --current-wheel ${GE_PYTHON_PASS_NATIVE_WHEEL}
            ${GE_PYTHON_PASS_MATRIX_COMPILER_ARGS}
         COMMENT "Build GE python pass native wheels for all supported Python versions found in PATH"
)
add_custom_target(ge_python_pass_fallback_codegen
        DEPENDS ${GE_PYTHON_PASS_FALLBACK_CODEGEN_CONFIG}
                ${GE_PYTHON_PASS_FALLBACK_CODEGEN_SOURCES_PY}
                ${GE_PYTHON_PASS_FALLBACK_CODEGEN_INIT_PY})
add_dependencies(ge_python_main_wheel ge_python_pass_fallback_codegen)
add_dependencies(ge_python_pass_native_wheel_matrix ge_python_native_wheel)
add_dependencies(ge_python ge_python_main_wheel ge_python_pass_native_wheel_matrix)
add_custom_command(
         OUTPUT ${GE_PYTHON_PASS_FALLBACK_CODEGEN_CONFIG}
                ${GE_PYTHON_PASS_FALLBACK_CODEGEN_SOURCES_PY}
                ${GE_PYTHON_PASS_FALLBACK_CODEGEN_INIT_PY}
         COMMAND ${HI_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/python_pass_native_build/gen_fallback_resources.py
            --output-dir ${GE_PYTHON_PASS_FALLBACK_CODEGEN_DIR}
            --bridge-abi ${GE_PYTHON_PASS_BRIDGE_ABI_VERSION}
            --bridge-source ${GE_PYTHON_PASS_BRIDGE_SOURCES}
            --bridge-header ${GE_PYTHON_PASS_BRIDGE_HEADERS}
            --native-source ${GE_PYTHON_PASS_NATIVE_SOURCES}
            --native-header ${GE_PYTHON_PASS_NATIVE_HEADERS}
         DEPENDS ${GE_PYTHON_PASS_BRIDGE_SOURCES}
                 ${GE_PYTHON_PASS_BRIDGE_HEADERS}
                 ${GE_PYTHON_PASS_NATIVE_SOURCES}
                 ${GE_PYTHON_PASS_NATIVE_HEADERS}
                 ${CMAKE_CURRENT_SOURCE_DIR}/python_pass_native_build/gen_fallback_resources.py
         COMMAND_EXPAND_LISTS
         COMMENT "Generate GE python pass fallback codegen resources"
)
add_custom_command(
         OUTPUT ${GE_PYTHON_MAIN_WHEEL}
         COMMAND echo "Start to build ge-py wheel package"
         && rm -rf ${GE_PYTHON_MAIN_WHEEL_DIR}
         && mkdir -p ${GE_PYTHON_MAIN_WHEEL_DIR}
         && cp -r ${CMAKE_CURRENT_SOURCE_DIR}/setup.py ${GE_PYTHON_MAIN_WHEEL_DIR}/setup.py
         && cp -r ${CMAKE_CURRENT_SOURCE_DIR}/MANIFEST.in ${GE_PYTHON_MAIN_WHEEL_DIR}/
         && cp -r ${CMAKE_CURRENT_SOURCE_DIR}/ge ${GE_PYTHON_MAIN_WHEEL_DIR}/
         && ${CMAKE_COMMAND} -E remove ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/passes/CMakeLists.txt
         && ${CMAKE_COMMAND} -E remove_directory ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/passes/native_bindings
         && ${CMAKE_COMMAND} -E remove_directory ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/passes/python_pass_artifacts
         && ${CMAKE_COMMAND} -E remove_directory ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/passes/fallback_codegen
         && ${CMAKE_COMMAND} -E copy_directory ${GE_PYTHON_PASS_FALLBACK_CODEGEN_DIR} ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/passes/fallback_codegen
         && find ${GE_PYTHON_MAIN_WHEEL_DIR}/ge -name __pycache__ -type d -exec rm -rf {} +
         && cp $<TARGET_FILE:graph_wrapper> ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/_capi/
         && cp $<TARGET_FILE:ge_runtime_wrapper> ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/_capi/
         && cp $<TARGET_FILE:offline_compile_wrapper> ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/_capi/
         && cp $<TARGET_FILE:pyatc_wrapper> ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/_capi/
         && cp $<TARGET_FILE:ge_utils_wrapper> ${GE_PYTHON_MAIN_WHEEL_DIR}/ge/_capi/
         && cd ${GE_PYTHON_MAIN_WHEEL_DIR}
         && ${HI_PYTHON} setup.py bdist_wheel >/dev/null
         && cp -f dist/ge_py-0.0.1-py3-none-any.whl ${GE_PYTHON_MAIN_WHEEL}
         && echo "Build ge-py wheel package end"
         DEPENDS eager_style_graph_builder_base ge_api_c_wrapper graph_wrapper
                 ${GE_API_C_WRAPPER}
                 ${GE_PYTHON_PASS_FALLBACK_CODEGEN_CONFIG}
 )
add_custom_command(
         OUTPUT ${GE_PYTHON_PASS_NATIVE_WHEEL}
         COMMAND echo "Start to build ge-py-pass-bridge native wheel package"
         && echo "Build ${GE_PYTHON_PASS_PYTHON_TAG} native artifacts with python[${HI_PYTHON}], version[${GE_PYTHON_PASS_PYTHON_VERSION}], include[${HI_PYTHON_INC}], libpython[${HI_PYTHON_EMBED_LIBRARIES}], pybind11[${pybind11_INCLUDE_DIR}], link_python[yes], rpath_python[no]"
         && rm -rf ${GE_PYTHON_PASS_ARTIFACT_DIR}
         && mkdir -p ${GE_PYTHON_PASS_ARTIFACT_DIR}
         && cp ${GE_PYTHON_PASS_ARTIFACT_MANIFEST} ${GE_PYTHON_PASS_ARTIFACT_DIR}/manifest.json
         && cp $<TARGET_FILE:ge_python_pass_bridge> ${GE_PYTHON_PASS_ARTIFACT_DIR}/libge_python_pass_bridge.so
         && cp $<TARGET_FILE:_ge_pass_native> ${GE_PYTHON_PASS_ARTIFACT_DIR}/_ge_pass_native.so
         && ${HI_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/python_pass_native_build/build_python_pass_native_wheel.py
            --artifact-dir ${GE_PYTHON_PASS_ARTIFACT_DIR}
            --python-tag ${GE_PYTHON_PASS_PYTHON_TAG}
            --platform-tag ${GE_PYTHON_PASS_PLATFORM_TAG}
            --wheel-platform ${GE_PYTHON_PASS_WHEEL_PLATFORM}
            --output-dir ${CMAKE_CURRENT_BINARY_DIR}
         && echo "Build ge-py-pass-bridge native wheel package end"
         DEPENDS _ge_pass_native ge_python_pass_bridge ${GE_PYTHON_PASS_ARTIFACT_MANIFEST}
                 ${CMAKE_CURRENT_SOURCE_DIR}/python_pass_native_build/build_python_pass_native_wheel.py
 )
install(FILES ${GE_PYTHON_MAIN_WHEEL} ${INSTALL_OPTIONAL}
        DESTINATION ${INSTALL_LIBRARY_DIR}
)
install(DIRECTORY ${GE_PYTHON_PASS_NATIVE_MATRIX_DIST_DIR}/ ${INSTALL_OPTIONAL}
        DESTINATION ${INSTALL_LIBRARY_DIR}
        FILES_MATCHING PATTERN "ge_py_pass_bridge-*.whl"
        PATTERN "_build_ge_py_pass_bridge_*" EXCLUDE
)
