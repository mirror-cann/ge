
file(GLOB_RECURSE AUTO_FUSE_TEST_SRCS CONFIGURE_DEPENDS "*.cc")
add_executable(autofuse_test
        ${AUTO_FUSE_TEST_SRCS}
        "${AIR_CODE_DIR}/tests/ge/st/testcase/autofuse/test_symbolic_shape_inference.cc"
        "${AIR_CODE_DIR}/tests/ge/st/testcase/autofuse/test_symbolic_shape_compute.cc"
)

target_compile_options(autofuse_test PRIVATE
        -g
        -Wno-deprecated-declarations
        -Wall -Wfloat-equal -Werror
        -fno-access-control
        -include ${AIR_CODE_DIR}/tests/depends/aihacb_autofusion/src/fix_af_attr_store.h
)

target_compile_definitions(autofuse_test PRIVATE
        google=ascend_private
        AUTOFUSE_USE_GE_METADEF
)

target_include_directories(autofuse_test BEFORE PRIVATE
        ${AIR_CODE_DIR}/inc/graph_metadef
        ${AIR_CODE_DIR}/inc/graph_metadef/graph
        ${AIR_CODE_DIR}/graph_metadef
)
target_include_directories(autofuse_test PRIVATE
        ${AIR_CODE_DIR}
        ${AIR_CODE_DIR}/base/
        ${AIR_CODE_DIR}/compiler/
        ${AIR_CODE_DIR}/tests/depends/
        ${AIR_CODE_DIR}/inc
        ${AIR_CODE_DIR}/inc/framework/
        ${AIR_CODE_DIR}/inc/graph_metadef
        ${AIR_CODE_DIR}/tests/depends/op_stub
        ${AIR_CODE_DIR}/tests/depends/aihacb_autofusion/src
        ${AIR_CODE_DIR}/tests/framework/ge_graph_dsl/include
        ${AIR_CODE_DIR}/tests/framework/easy_graph/include
        ${AIR_CODE_DIR}/tests/framework/ge_runtime_stub/include
        ${AIR_CODE_DIR}/tests/ge/ut/ge/graph/optimize/symbolic
        ${AIR_CODE_DIR}/compiler/graph/optimize/symbolic/infer_symbolic_shape
        ${AIR_CODE_DIR}/compiler/graph/optimize/symbolic/infer_symbolic_shape/infer
        ${AIR_CODE_DIR}/tests/framework/ge_running_env/include
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/inc
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/autofuse
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/graph
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/ascir
        ${AIR_CODE_DIR}/compiler/graph/optimize/autofuse/att/
        ${ASCGEN_DIR}
        ${ASCGEN_DIR}/autofuse
        ${CMAKE_BINARY_DIR}/tests/depends/aihacb_autofusion/
        ${CMAKE_BINARY_DIR}/proto/data_flow_protos
        ${CMAKE_BINARY_DIR}/proto/ge_autofuse_metadef_protos_af
        ${METADEF_DIR}/base/registry
        ${METADEF_DIR}/register
        ${CMAKE_BINARY_DIR}/graph_metadef/graph/ascendc_ir/generator
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/graph_metadef
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/graph_metadef/graph
        ${ASCEND_INSTALL_PATH}/pkg_inc/autofuse/ascir
)

target_link_libraries(autofuse_test PRIVATE
        st_stubs
        ge_common
        ge_common_base
        ge_graph_dsl
        ge_runtime_stub
        ge_running_env
        ge_compiler
        aihacb_autofusion_stub
        ${ASCEND_INSTALL_PATH}/lib64/libaihac_ir.so
        ge_runner
        gert
        ge_metadef_headers
        ascendcl_stub
        GTestShared::gtest
        GTestShared::gtest_main
        GTestShared::gmock
        GTestShared::gmock_main
        es_ge_test
        intf_llt_pub
        -Wl,--whole-archive
        rt2_registry_static
        -Wl,--no-whole-archive
        json
        ge_running_env ge_runtime_stub GTestShared::gtest GTestShared::gtest_main GTestShared::gmock_main ${COMMON_SHARED_LIBRARIES} -lrt -ldl
        -Wl,--no-as-needed GTestShared::gmock -Wl,--as-needed
)

target_link_options(autofuse_test PRIVATE
        -rdynamic
        -Wl,-Bsymbolic
        -Wl,--disable-new-dtags
        )

add_dependencies(autofuse_test gert_op_impl ge_autofuse_metadef_protos_af)

add_test(NAME autofuse_test COMMAND autofuse_test --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/st/autofuse_test.xml)
set_tests_properties(autofuse_test PROPERTIES
        LABELS "st;st_hetero;autofuse_test"
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0"
)

include(CTest)
enable_testing()
add_test(NAME test COMMAND autofuse_test)
