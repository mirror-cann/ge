# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

# 设置安装目录
set(PYGE_TEST_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/ge_py_install)
set(PYGE_TEST_NATIVE_FIXUP_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/ge_py_native_fixup.cmake)

file(WRITE ${PYGE_TEST_NATIVE_FIXUP_SCRIPT} [=[
function(ge_python_pass_artifacts_complete result)
    set(complete FALSE)
    set(artifact_root "${INSTALL_DIR}/ge/passes/python_pass_artifacts")
    if (IS_DIRECTORY "${artifact_root}")
        file(GLOB artifact_dirs LIST_DIRECTORIES true "${artifact_root}/*")
        foreach (artifact_dir IN LISTS artifact_dirs)
            if (IS_DIRECTORY "${artifact_dir}"
                    AND EXISTS "${artifact_dir}/manifest.json"
                    AND EXISTS "${artifact_dir}/libge_python_pass_bridge.so"
                    AND EXISTS "${artifact_dir}/_ge_pass_native.so")
                set(complete TRUE)
                break()
            endif ()
        endforeach ()
    endif ()
    set(${result} ${complete} PARENT_SCOPE)
endfunction()

ge_python_pass_artifacts_complete(artifact_complete)
if (artifact_complete)
    return()
endif ()

message(STATUS "GE python pass native artifacts are incomplete; extracting ${NATIVE_WHEEL}")
execute_process(
    COMMAND "${HI_PYTHON}" -m zipfile -e "${NATIVE_WHEEL}" "${INSTALL_DIR}"
    RESULT_VARIABLE unzip_result
    ERROR_VARIABLE unzip_error
)
if (NOT unzip_result EQUAL 0)
    message(FATAL_ERROR "Failed to extract ${NATIVE_WHEEL}: ${unzip_error}")
endif ()

ge_python_pass_artifacts_complete(artifact_complete)
if (NOT artifact_complete)
    message(FATAL_ERROR "Native wheel does not contain complete python pass artifacts: ${NATIVE_WHEEL}")
endif ()
]=])

# 安装 ge-py 包到指定目录
# so需要单独拷贝，因为ge_py的安装包中没有打包此so
# 运行环境下因为此so随着Run包安装，放在LD_LIBRARY_PATH中，所以运行环境无问题
add_custom_command(
        OUTPUT ${PYGE_TEST_INSTALL_DIR}/.installed
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${PYGE_TEST_INSTALL_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PYGE_TEST_INSTALL_DIR}
        COMMAND ${HI_PYTHON} -m pip install --target ${PYGE_TEST_INSTALL_DIR}
                ${CMAKE_BINARY_DIR}/api/python/ge/ge_py-0.0.1-py3-none-any.whl
                ${GE_PYTHON_PASS_NATIVE_WHEEL}
                --force-reinstall
        COMMAND ${CMAKE_COMMAND}
                -DINSTALL_DIR=${PYGE_TEST_INSTALL_DIR}
                -DNATIVE_WHEEL=${GE_PYTHON_PASS_NATIVE_WHEEL}
                -DHI_PYTHON=${HI_PYTHON}
                -P ${PYGE_TEST_NATIVE_FIXUP_SCRIPT}
        COMMAND ${HI_PYTHON} -m pip install --target ${PYGE_TEST_INSTALL_DIR} ${CMAKE_BINARY_DIR}/tests/ge/ut/ge/graph/eager_style_graph_builder/graph_construction_test/output/whl/es_ut_test-1.0.0-py3-none-any.whl --force-reinstall
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:eager_style_graph_builder_base> ${PYGE_TEST_INSTALL_DIR}/ge/_capi/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/tests/ge/ut/ge/graph/eager_style_graph_builder/graph_construction_test/output/lib64/libes_ut_test.so ${PYGE_TEST_INSTALL_DIR}/ge/_capi/
        COMMAND ${CMAKE_COMMAND} -E touch ${PYGE_TEST_INSTALL_DIR}/.installed
        DEPENDS ge_python_main_wheel ge_python_native_wheel es_ut_test
        COMMENT "Installing ge-py and es_ut_test packages for pyge_tests"
)

# 创建安装目标
add_custom_target(install_ge_py_for_tests DEPENDS ${PYGE_TEST_INSTALL_DIR}/.installed)
