project(UpdateModelParam_dav_2201)

# 为update_model_param.h设置语言为CXX
set_source_files_properties(
    update_model_param.h PROPERTIES LANGUAGE CXX
)

if(BUILD_PKG_COMPONENT
    AND "ge-executor" IN_LIST BUILD_COMPONENT
    AND NOT ENABLE_MOD_EXT)
    set(CCEC_PATH ${ASCEND_INSTALL_PATH}/tools/ccec_compiler/bin)
    find_cann_package(asc_devkit MODULE REQUIRED)

    add_custom_target(runtime_update_model_param_dav_2201 ALL
        COMMAND ${CCEC_PATH}/ccec -c -O3 -x cce -DTILING_KEY_VAR=0UL -Dupdate_model_param=UpdateModelParam
            --cce-disable-kernel-global-attr-check --cce-auto-sync -mllvm -api-deps-filter --cce-aicore-arch=dav-c220-vec
            --cce-aicore-only -mllvm -cce-aicore-stack-size=0x8000 -mllvm -cce-aicore-function-stack-size=0x8000
            -mllvm -cce-aicore-record-overflow=false -mllvm -cce-aicore-addr-transform
            -mllvm -cce-aicore-dcci-insert-for-scalar=false -std=c++17
            "-I$<JOIN:$<TARGET_PROPERTY:asc_kernel_headers,INTERFACE_INCLUDE_DIRECTORIES>,;-I>"
            "-I$<JOIN:$<TARGET_PROPERTY:kernel_tiling_headers,INTERFACE_INCLUDE_DIRECTORIES>,;-I>"
            ${CMAKE_CURRENT_SOURCE_DIR}/update_model_param.h
            -o ${CMAKE_CURRENT_BINARY_DIR}/UpdateModelParam_0_dav_2201.o
        COMMAND ${CCEC_PATH}/ld.lld -m aicorelinux -r -Ttext=0
            -x ${CMAKE_CURRENT_BINARY_DIR}/UpdateModelParam_0_dav_2201.o -static
            -o ${CMAKE_CURRENT_BINARY_DIR}/UpdateModelParam_dav_2201.o
        COMMAND ${CCEC_PATH}/ld.lld -m aicorelinux -Ttext=0 ${CMAKE_CURRENT_BINARY_DIR}/UpdateModelParam_dav_2201.o
            -static -o ${CMAKE_CURRENT_BINARY_DIR}/UpdateModelParam_dav_2201.o
        DEPENDS kernel_tiling_headers
        COMMAND_EXPAND_LISTS VERBATIM
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/UpdateModelParam_dav_2201.o
        DESTINATION ${INSTALL_LIBRARY_DIR} ${INSTALL_OPTIONAL}
    )
else()
    add_custom_target(runtime_update_model_param_dav_2201)
endif()
