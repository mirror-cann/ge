# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

function(resolve_python_include python_executable output_var)
    execute_process(COMMAND ${python_executable} -c "import sysconfig; print(sysconfig.get_path('include') or sysconfig.get_config_var('INCLUDEPY') or '')"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE include_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (result)
        set(${output_var} "" PARENT_SCOPE)
    else ()
        set(${output_var} ${include_dir} PARENT_SCOPE)
    endif ()
endfunction()

function(resolve_python_library python_executable output_var)
    execute_process(
            COMMAND ${python_executable} -c "import os, sysconfig; version = sysconfig.get_config_var('VERSION') or ''; libdir = sysconfig.get_config_var('LIBDIR') or ''; candidates = [sysconfig.get_config_var('LDLIBRARY'), sysconfig.get_config_var('INSTSONAME'), sysconfig.get_config_var('LIBRARY')]; candidates.extend([f'libpython{version}.so.1.0' if version else '', f'libpython{version}.so' if version else '']); seen = []; [seen.append(item) for item in candidates if item and item not in seen]; matches = [os.path.join(libdir, item) for item in seen if libdir and os.path.exists(os.path.join(libdir, item))]; shared = next((item for item in matches if '.so' in os.path.basename(item)), ''); print(shared or (matches[0] if matches else ''))"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE library_path
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (result)
        set(${output_var} "" PARENT_SCOPE)
    else ()
        set(${output_var} ${library_path} PARENT_SCOPE)
    endif ()
endfunction()

function(resolve_pybind11_include python_executable output_var)
    execute_process(COMMAND ${python_executable} -c "import pybind11; print(pybind11.get_include())"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE include_dir
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (result)
        set(${output_var} "" PARENT_SCOPE)
    else ()
        set(${output_var} ${include_dir} PARENT_SCOPE)
    endif ()
endfunction()

set(HI_PYTHON_EMBED_LIBRARIES "")

if (BUILD_OPEN_PROJECT OR ENABLE_OPEN_SRC)
    set(Python3_EXECUTABLE ${HI_PYTHON})
    find_package(Python3 COMPONENTS Interpreter Development)
    if (Python3_FOUND)
        set(HI_PYTHON_INC ${Python3_INCLUDE_DIRS})
        set(HI_PYTHON_EMBED_LIBRARIES ${Python3_LIBRARIES})
        cmake_print_variables(HI_PYTHON_INC)
    else ()
        resolve_python_include(${HI_PYTHON} HI_PYTHON_INC)
        resolve_python_library(${HI_PYTHON} HI_PYTHON_EMBED_LIBRARIES)
        if (NOT HI_PYTHON_INC)
            message(WARNING "no include dir found for python:${HI_PYTHON}.")
        endif ()
    endif ()
    # make sure pybind11 cmake can be use in find_package
    execute_process(COMMAND ${HI_PYTHON} -m pybind11 --cmakedir OUTPUT_VARIABLE pybind11_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)
    find_package(pybind11 CONFIG REQUIRED)
else ()
    resolve_python_include(${HI_PYTHON} HI_PYTHON_INC)
    resolve_python_library(${HI_PYTHON} HI_PYTHON_EMBED_LIBRARIES)
    resolve_pybind11_include(${HI_PYTHON} pybind11_INCLUDE_DIR)
endif ()

if (NOT TARGET ge_python_embed)
    add_library(ge_python_embed INTERFACE)
endif ()

if (TARGET Python3::Python)
    target_link_libraries(ge_python_embed INTERFACE Python3::Python)
elseif (HI_PYTHON_EMBED_LIBRARIES)
    target_link_libraries(ge_python_embed INTERFACE ${HI_PYTHON_EMBED_LIBRARIES})
else ()
    message(WARNING "no python embed library found for python:${HI_PYTHON}.")
endif ()

if (HI_PYTHON_INC)
    target_include_directories(ge_python_embed INTERFACE ${HI_PYTHON_INC})
endif ()

cmake_print_variables(HI_PYTHON_INC)
cmake_print_variables(pybind11_INCLUDE_DIR)
cmake_print_variables(HI_PYTHON_EMBED_LIBRARIES)
