# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------

#[[
  module - the name of export imported target
  name   - find the library name
  path   - find the library path
#]]
function(find_module module name)
    if (TARGET ${module})
        return()
    endif ()

    set(options)
    set(oneValueArgs)
    set(multiValueArgs)
    cmake_parse_arguments(MODULE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(path ${MODULE_UNPARSED_ARGUMENTS})
    unset(${module}_LIBRARY_DIR CACHE)
    find_library(${module}_LIBRARY_DIR NAMES ${name} NAMES_PER_DIR PATHS ${path}
            PATH_SUFFIXES lib
            )

    message(STATUS "find ${name} location ${${module}_LIBRARY_DIR}")
    if ("${${module}_LIBRARY_DIR}" STREQUAL "${module}_LIBRARY_DIR-NOTFOUND")
        message(FATAL_ERROR "${name} not found in ${path}")
    endif ()

    add_library(${module} SHARED IMPORTED)
    set_target_properties(${module} PROPERTIES
            IMPORTED_LOCATION ${${module}_LIBRARY_DIR}
            )
endfunction()

function(protobuf_generate comp c_var h_var)
    if (NOT ARGN)
        message(SEND_ERROR "Error: protobuf_generate() called without any proto files")
        return()
    endif ()
    set(${c_var})
    set(${h_var})
    set(_add_target FALSE)
    set(_protoc_grogram $<TARGET_FILE:host_protoc>)

    set(extra_option "")
    foreach (arg ${ARGN})
        if ("${arg}" MATCHES "--proto_path")
            set(extra_option ${arg})
        endif ()
        if ("${arg}" STREQUAL "TARGET")
            set(_add_target TRUE)
        endif ()
        if ("${arg}" STREQUAL "TENSORFLOW_PROTOC")
            set(_protoc_grogram ${PROTOC_TENSORFLOW_PROGRAM})
        endif ()
    endforeach ()

    foreach (file ${ARGN})
        if ("${file}" MATCHES "--proto_path")
            continue()
        endif ()

        if ("${file}" STREQUAL "TARGET")
            continue()
        endif ()

        if ("${file}" STREQUAL "TENSORFLOW_PROTOC")
            continue()
        endif ()

        get_filename_component(abs_file ${file} ABSOLUTE)
        get_filename_component(file_name ${file} NAME_WE)
        get_filename_component(file_dir ${abs_file} PATH)
        get_filename_component(parent_subdir ${file_dir} NAME)

        if ("${parent_subdir}" STREQUAL "proto")
            set(proto_output_path ${CMAKE_BINARY_DIR}/proto/${comp}/proto)
        else ()
            set(proto_output_path ${CMAKE_BINARY_DIR}/proto/${comp}/proto/${parent_subdir})
        endif ()
        list(APPEND ${c_var} "${proto_output_path}/${file_name}.pb.cc")
        list(APPEND ${h_var} "${proto_output_path}/${file_name}.pb.h")

        add_custom_command(
                OUTPUT "${proto_output_path}/${file_name}.pb.cc" "${proto_output_path}/${file_name}.pb.h"
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${proto_output_path}"
                COMMAND ${CMAKE_COMMAND} -E echo "generate proto cpp_out ${comp} by ${abs_file}"
                COMMAND ${_protoc_grogram} -I${file_dir} ${extra_option} --cpp_out=${proto_output_path} ${abs_file}
                DEPENDS ${abs_file} host_protoc
                COMMENT "Running C++ protocol buffer compiler on ${file}" VERBATIM)
    endforeach ()

    if (_add_target)
        add_custom_target(
            ${comp} DEPENDS ${${c_var}} ${${h_var}}
        )
    endif ()

    set_source_files_properties(${${c_var}} ${${h_var}} PROPERTIES GENERATED TRUE)
    set(${c_var} ${${c_var}} PARENT_SCOPE)
    set(${h_var} ${${h_var}} PARENT_SCOPE)
endfunction()

function(protobuf_generate_grpc comp c_var h_var)
    if (NOT ARGN)
        MESSAGE(SEND_ERROR "Error:protobuf_generate_grpc() called without any proto files")
        return()
    endif ()
    set(${c_var})
    set(${h_var})

    set(extra_option "")
    foreach (arg ${ARGN})
        if ("${arg}" MATCHES "--proto_path")
            set(extra_option ${arg})
        endif ()
    endforeach ()

    foreach (file ${ARGN})
        if ("${file}" MATCHES "--proto_path")
            continue()
        endif ()
        get_filename_component(abs_file ${file} ABSOLUTE)
        get_filename_component(file_name ${file} NAME_WE)
        get_filename_component(file_dir ${abs_file} PATH)
        get_filename_component(parent_subdir ${file_dir} NAME)
        if ("${parent_subdir}" STREQUAL "proto")
            set(proto_output_path ${CMAKE_BINARY_DIR}/proto_grpc/${comp}/proto)
        else ()
            set(proto_output_path ${CMAKE_BINARY_DIR}/proto_grpc/${comp}/proto_grpc/${parent_subdir})
        endif ()
        list(APPEND ${c_var} "${proto_output_path}/${file_name}.pb.cc")
        list(APPEND ${c_var} "${proto_output_path}/${file_name}.grpc.pb.cc")
        list(APPEND ${h_var} "${proto_output_path}/${file_name}.pb.h")
        list(APPEND ${h_var} "${proto_output_path}/${file_name}.grpc.pb.h")

        add_custom_command(
                OUTPUT "${proto_output_path}/${file_name}.pb.cc" "${proto_output_path}/${file_name}.pb.h"
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${proto_output_path}"
                COMMAND $<TARGET_FILE:host_protoc> -I${file_dir} ${extra_option} --cpp_out=${proto_output_path} ${abs_file}
                COMMAND $<TARGET_FILE:host_protoc> -I${file_dir} ${extra_option} --grpc_out=${proto_output_path} --plugin=protoc-gen-grpc=$<TARGET_FILE:grpc_cpp_plugin> ${abs_file}
                DEPENDS ${abs_file} host_protoc grpc_cpp_plugin
                COMMENT "Running C++ protocol buffer compiler on ${file}" VERBATIM)
    endforeach ()

    set_source_files_properties(${${c_var}} ${${h_var}} PROPERTIES GENERATED TRUE)
    set(${c_var} ${${c_var}} PARENT_SCOPE)
    set(${h_var} ${${h_var}} PARENT_SCOPE)
endfunction()

function(protobuf_generate_py comp py_var)
    if (NOT ARGN)
        message(SEND_ERROR "Error: protobuf_generate_py() called without any proto files")
        return()
    endif ()
    set(${py_var})
    set(_add_target FALSE)

    foreach (file ${ARGN})
        if ("${file}" STREQUAL "TARGET")
            set(_add_target TRUE)
            continue()
        endif ()

        get_filename_component(abs_file ${file} ABSOLUTE)
        get_filename_component(file_name ${file} NAME_WE)
        get_filename_component(file_dir ${abs_file} PATH)
        get_filename_component(parent_subdir ${file_dir} NAME)

        if ("${parent_subdir}" STREQUAL "proto")
            set(proto_output_path ${CMAKE_BINARY_DIR}/proto/${comp}/proto)
        else ()
            set(proto_output_path ${CMAKE_BINARY_DIR}/proto/${comp}/proto/${parent_subdir})
        endif ()
        list(APPEND ${py_var} "${proto_output_path}/${file_name}_pb2.py")

        add_custom_command(
                OUTPUT "${proto_output_path}/${file_name}_pb2.py"
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${proto_output_path}"
                COMMAND ${CMAKE_COMMAND} -E echo "generate proto cpp_out ${comp} by ${abs_file}"
                COMMAND $<TARGET_FILE:host_protoc> -I${file_dir} --python_out=${proto_output_path} ${abs_file}
                DEPENDS ${abs_file}
                COMMENT "Running PYTHON protocol buffer compiler on ${file}" VERBATIM )
    endforeach ()

    if (_add_target)
        add_custom_target(
            ${comp} DEPENDS ${${py_var}}
        )
    endif ()

    set_source_files_properties(${${py_var}} PROPERTIES GENERATED TRUE)
    set(${py_var} ${${py_var}} PARENT_SCOPE)
endfunction()

macro(find_package_if_target_not_exists pkg)
    if (TARGET ${pkg})
        message(STATUS "SN_DEBUG package ${pkg} target exists")
    else ()
        find_cann_package(${pkg} ${ARGN})
    endif ()
endmacro()

function(target_clone_compile_and_link_options original_library target_library)
    get_target_property(linkOpts ${original_library} LINK_OPTIONS)
    get_target_property(compileOptions ${original_library} COMPILE_OPTIONS)

    if (linkOpts)
        target_link_options(${target_library} PRIVATE
                ${linkOpts}
                )
    endif()
    if (compileOptions)
        target_compile_options(${target_library} PRIVATE
                ${compileOptions}
                )
    endif()
endfunction()
function(to_absolute_path input_sources source_dir out_arg)
    set(output_sources)
    FOREACH(source_file ${${input_sources}})
        if(IS_ABSOLUTE ${source_file})
            list(APPEND output_sources ${source_file})
        else()
            if(${source_file} MATCHES ".cc$")
                list(APPEND output_sources ${${source_dir}}/${source_file})
            else()
                list(APPEND output_sources ${source_file})
            endif()
        endif()
    ENDFOREACH()
    set(${out_arg} ${output_sources} PARENT_SCOPE)
endfunction()


function(target_clone_compile_and_link_options original_library target_library)
    get_target_property(linkOpts ${original_library} LINK_OPTIONS)
    get_target_property(compileOptions ${original_library} COMPILE_OPTIONS)


    if (linkOpts)
        target_link_options(${target_library} PRIVATE
                ${linkOpts}
                )
    endif()
    if (compileOptions)
        target_compile_options(${target_library} PRIVATE
                ${compileOptions}
                )
    endif()
endfunction()

function(target_clone original_library target_library libray_type)
    get_target_property(sourceFiles ${original_library} SOURCES)
    get_target_property(sourceDir ${original_library} SOURCE_DIR)
    to_absolute_path(sourceFiles sourceDir absolute_sources_files)
    add_library(${target_library} ${libray_type}
            ${absolute_sources_files}
            )


    get_target_property(linkLibs ${original_library} LINK_LIBRARIES)
    get_target_property(includeDirs ${original_library} INCLUDE_DIRECTORIES)
    target_include_directories(${target_library} PRIVATE
            ${includeDirs}
            )


    target_link_libraries(${target_library} PRIVATE
            ${linkLibs}
            )
    get_target_property(compileDefinitions ${original_library} COMPILE_DEFINITIONS)
    if (compileDefinitions)
        target_compile_definitions(${target_library} PRIVATE
                ${compileDefinitions}
                )
    endif()
    target_clone_compile_and_link_options(${original_library} ${target_library})
endfunction()

# 设置公共参数
macro(set_common_params)
    if(ENABLE_OPEN_SRC OR BUILD_OPEN_PROJECT)
        # install时不添加OPTIONAL选项，以保证打包产物完整
        set(INSTALL_OPTIONAL)
    endif()

    if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
        # 单仓编译
        set(TOPLEVEL_PROJECT ON)
    else()
        # 多仓联编
        set(TOPLEVEL_PROJECT OFF)
    endif()
endmacro()
