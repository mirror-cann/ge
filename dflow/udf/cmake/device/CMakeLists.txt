# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
include(func.cmake)
include(${GE_DIR}/cmake/ge_intf_pub_linux.cmake)
include(${GE_DIR}/cmake/air_headers.cmake)
add_cann_third_party(protobuf)
add_cann_third_party(seccomp)

find_cann_package(securec REQUIRED)
find_cann_package(hccl REQUIRED)
find_cann_package(mmpa REQUIRED)
find_cann_package(metadef REQUIRED)
find_cann_package(ascend_hal REQUIRED)
find_cann_package(slog REQUIRED)
find_cann_package(datagw REQUIRED)
find_cann_package(runtime REQUIRED)

add_subdirectory(${GE_DIR}/dflow/udf ${CMAKE_BINARY_DIR}/udf_device)

set(PACKAGE_TAR_FILE_NAME "cann-udf-compat.tar.gz")
set(PACKAGE_BIN_DIR "bin")
set(PACKAGE_LIB_DIR "lib64")

add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/${PACKAGE_TAR_FILE_NAME}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_BIN_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_LIB_DIR}

        # 将可执行文件复制到临时目录的 bin 下
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:udf_executor> ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_BIN_DIR}/

        # 将库文件复制到临时目录的 lib64 下
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:flow_func> ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_LIB_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:reader_writer> ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_LIB_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:built_in_flowfunc> ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_LIB_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:udf_profiling> ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_LIB_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:udf_dump> ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_LIB_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:alog> ${CMAKE_BINARY_DIR}/package_temp/${PACKAGE_LIB_DIR}/

        # 执行 tar 命令进行打包
        # WORKING_DIRECTORY 设置为临时目录的父目录，这样 tar 打包的路径就是相对的
        COMMAND tar -czf ${CMAKE_BINARY_DIR}/${PACKAGE_TAR_FILE_NAME} -C ${CMAKE_BINARY_DIR}/package_temp . --mode=750

        # 打包完成后，删除临时目录
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/package_temp

        # 明确指定这个命令依赖于哪些目标
        DEPENDS udf_executor flow_func reader_writer built_in_flowfunc udf_profiling udf_dump

        COMMENT "Creating udf package: ${CMAKE_BINARY_DIR}/${PACKAGE_TAR_FILE_NAME}"
)

add_custom_target(pack_udf_compat ALL
        DEPENDS ${CMAKE_BINARY_DIR}/${PACKAGE_TAR_FILE_NAME}
)

if (ENABLE_SIGN)
    sign_file(
            INPUT "${CMAKE_BINARY_DIR}/${PACKAGE_TAR_FILE_NAME}"
            CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/sign/sign.xml
            RESULT_VAR "udf_compat_sign_file"
            DEPENDS pack_udf_compat
    )

    install(FILES
            ${udf_compat_sign_file}
            DESTINATION compat
            PERMISSIONS OWNER_READ GROUP_READ
            COMPONENT dflow-executor
    )

    # pack Ascend-runtime_device-minios.tar.gz
    set(RUNTIME_PACKAGE_TAR_FILE_NAME "Ascend-runtime_device-minios.tar.gz")
    set(RUNTIME_PACKAGE_TAR_PACK_DIR "${CMAKE_BINARY_DIR}/runtime_package_temp")

    if(ENABLE_UNIFIED_BUILD)
        add_custom_command(OUTPUT ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/lib64/librs.so
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime
            COMMAND tar -zxf $<TARGET_PROPERTY:pack_cann_hccd_compat,TARGET_FILE> -C ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/
            DEPENDS pack_cann_hccd_compat
            VERBATIM
        )
    else()
        add_custom_command(OUTPUT ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/lib64/librs.so
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime
            COMMAND ${HI_PYTHON} ${GE_DIR}/scripts/signtool/image_extract/ci_img_headler.py -img ${ASCEND_INSTALL_PATH}/compat/cann-hccd-compat.tar.gz -raw ${RUNTIME_PACKAGE_TAR_PACK_DIR}/cann-hccd-compat.tar.gz --rcvr
            COMMAND tar -zxf ${RUNTIME_PACKAGE_TAR_PACK_DIR}/cann-hccd-compat.tar.gz -C ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/
        )
    endif()
 
    add_custom_target(unzip_hccd_compat
        DEPENDS ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/lib64/librs.so
    )

    add_custom_command(
            OUTPUT ${CMAKE_BINARY_DIR}/${RUNTIME_PACKAGE_TAR_FILE_NAME}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_BIN_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_LIB_DIR}

            # 将可执行文件复制到临时目录的 bin 下
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:udf_executor> ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_BIN_DIR}/

            # 将库文件复制到临时目录的 lib64 下
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:flow_func> ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_LIB_DIR}/
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:reader_writer> ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_LIB_DIR}/
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:built_in_flowfunc> ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_LIB_DIR}/
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:udf_profiling> ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_LIB_DIR}/
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:udf_dump> ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_LIB_DIR}/
            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:alog> ${RUNTIME_PACKAGE_TAR_PACK_DIR}/runtime/${PACKAGE_LIB_DIR}/

            # 执行 tar 命令进行打包
            # WORKING_DIRECTORY 设置为临时目录的父目录，这样 tar 打包的路径就是相对的
            COMMAND tar -czf ${CMAKE_BINARY_DIR}/${RUNTIME_PACKAGE_TAR_FILE_NAME} -C ${RUNTIME_PACKAGE_TAR_PACK_DIR}/ runtime --mode=750

            # 打包完成后，删除临时目录
            COMMAND ${CMAKE_COMMAND} -E remove_directory ${RUNTIME_PACKAGE_TAR_PACK_DIR}

            # 明确指定这个命令依赖于哪些目标
            DEPENDS unzip_hccd_compat udf_executor flow_func reader_writer built_in_flowfunc udf_profiling udf_dump

            COMMENT "Creating ${RUNTIME_PACKAGE_TAR_FILE_NAME} package: ${CMAKE_BINARY_DIR}/${RUNTIME_PACKAGE_TAR_FILE_NAME}"
    )
    add_custom_target(pack_runtime_device ALL
            DEPENDS ${CMAKE_BINARY_DIR}/${RUNTIME_PACKAGE_TAR_FILE_NAME}
    )
    sign_file(
            INPUT "${CMAKE_BINARY_DIR}/${RUNTIME_PACKAGE_TAR_FILE_NAME}"
            CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/sign/runtime_dev_sign.xml
            RESULT_VAR "runtime_device_sign_file"
            DEPENDS pack_runtime_device sign_cann_udf_compat
    )

    install(FILES
            ${runtime_device_sign_file}
            DESTINATION runtime/lib64
            PERMISSIONS OWNER_READ GROUP_READ
            COMPONENT dflow-executor
    )
else ()
    install(
            FILES "${CMAKE_BINARY_DIR}/${PACKAGE_TAR_FILE_NAME}"
            DESTINATION compat
            PERMISSIONS OWNER_READ GROUP_READ
            COMPONENT dflow-executor
    )
endif ()

install(TARGETS flow_func
    DESTINATION ${CMAKE_HOST_SYSTEM_PROCESSOR}-linux/devlib/device
    COMPONENT dflow-executor
)