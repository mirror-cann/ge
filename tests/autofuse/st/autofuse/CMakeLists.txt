set(AUTOFUSION_ROOT_DIR ${CODE_ROOT_DIR}/autofuse)

file(GLOB_RECURSE AUTOFUSION_TEST_FILES "${CMAKE_CURRENT_SOURCE_DIR}/../../ut/autofuse/*.cpp")
list(REMOVE_ITEM AUTOFUSION_TEST_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../ut/autofuse/loop_lowering_manager_asc_backend_unittest.cpp")

add_executable(autofusion_st
#        test.cpp
        ${AUTOFUSION_TEST_FILES}
        fusion_strategy_solver_eagle_graph_test.cpp
        loop_lowering_manager_asc_backend_unittest.cpp
        )

set(ASCGEN_DT_COMPILE_OPTION
        -O0
        -g
        --coverage -fprofile-arcs -ftest-coverage
#        -fsanitize=address -fsanitize=leak -fsanitize-recover=address
        )

set(ASCGEN_COMMON_LINK_OPTION
#        -fsanitize=address -fsanitize=leak -fsanitize-recover=address
        -lrt -ldl -lgcov
        )

add_dependencies(autofusion_st metadef_graph_protos_obj)

target_compile_definitions(autofusion_st PRIVATE
        PROTOBUF_INLINE_NOT_IN_HEADERS=0
        google=ascend_private
        AUTOFUSE_USE_GE_METADEF
        )

target_compile_options(autofusion_st PRIVATE ${ASCGEN_DT_COMPILE_OPTION}
        -Werror=format
        -Wno-deprecated-declarations
        -Wall -Wfloat-equal
        -Wno-subobject-linkage
        )

target_include_directories(autofusion_st PUBLIC
        ${AUTOFUSION_ROOT_DIR}
        ${CODE_ROOT_DIR}/common
        ${CMAKE_CURRENT_SOURCE_DIR}/../../ut/autofuse
        ${CODE_ROOT_DIR}/../../../../
        ${CODE_ROOT_DIR}/../../../../tests/autofuse/framework/easy_graph/include
        ${CODE_ROOT_DIR}/../../../../tests/autofuse/framework/ge_graph_dsl/include
        ${CODE_ROOT_DIR}/../../../../tests/autofuse/framework/eager_style_graph_builder
        ${CODE_ROOT_DIR}/../../../../tests/graph_metadef/ut
        )

target_link_directories(autofusion_st PRIVATE
        ${ASCEND_INSTALL_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/devlib
        ${ASCEND_INSTALL_PATH}/${CMAKE_SYSTEM_PROCESSOR}-linux/lib64
)

target_link_options(autofusion_st PRIVATE
        -Wl,--disable-new-dtags
        )

target_link_libraries(autofusion_st ${ASCGEN_COMMON_LINK_OPTION}
        ge_metadef_headers
        easy_graph_b
        ge_graph_dsl_b
        autofuse_eager_graph_builder_lib
        atrace
        graph_base
        graph
        metadef
        aihac_symbolizer
        autofuse
        error_manager
        mmpa
        aihac_codegen_toolkit
        ascend_protobuf
        aihac_symbolizer
        GTestShared::gtest
        GTestShared::gtest_main
        asc_slog_stub
        autofuse_runtime_stub
        json
        -Wl,--no-as-needed
        ascend_hal
        platform
        aihac_ir
        aihac_ir_register
        aihac_symbolizer_af
        graph_af
        graph_base_af
        runtime
        ascend_trace
)

add_test(NAME autofusion_st COMMAND autofusion_st --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/st/autofusion_st.xml)
set_tests_properties(autofusion_st PROPERTIES LABELS "st;autofusion_st")
