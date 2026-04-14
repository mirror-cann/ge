set (CMAKE_CXX_FLAGS "-D_GLIBCXX_USE_CXX11_ABI=0 -D_FORTIFY_SOURCE=2 -fPIC -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack")
set (CMAKE_C_FLAGS   "-D_GLIBCXX_USE_CXX11_ABI=0 -D_FORTIFY_SOURCE=2 -fPIC -fstack-protector-all -Wl,-z,relro,-z,now,-z,noexecstack")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g -fsanitize=address -fsanitize=leak -fsanitize-recover=address")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address -fsanitize=leak -fsanitize-recover=address")

message(STATUS "Using environment variable LD_LIBRARY_PATH: $ENV{LD_LIBRARY_PATH}")
message(STATUS "Using environment variable ASCEND_INSTALL_LIB_PATH: ${ASCEND_INSTALL_LIB_PATH}")
set(CMAKE_BUILD_TYPE Debug)

set(TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../)
set(ATT_DIR ${TOP_DIR}/compiler/graph/optimize/autofuse/att)

add_compile_definitions(TOP_DIR=\"${TOP_DIR}\")
add_compile_definitions(ST_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}\")
add_compile_definitions(TILING_DATA_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/testcase/scenario/tiling_data\")

file(GLOB SOURCES
        # common stub
        ${TOP_DIR}/tests/autofuse/common/stub/stub_model_info.cpp
        ${TOP_DIR}/tests/autofuse/common/stub/stub_matmul_modelinfo.cpp
        ${TOP_DIR}/tests/autofuse/common/stub/stub_ffn_modelinfo.cpp
        # --- source_mirror: generator ---
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/preprocess/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass/l0_solver/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass/l2_solver/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass/general_solver/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass/src/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass_gen/l0_solver_gen/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass_gen/l2_solver_gen/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass_gen/general_solver_gen/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass_gen/axes_reorder_solver_gen/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass_gen/manager/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/test_generator.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/generator_utils/tiling_func/src/*.cpp
        # --- source_mirror: gen_model_info ---
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/gen_model_info_content/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/gen_model_info_content/api_tiling_gen/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/gen_model_info_content/api_tiling_gen/api/*.cpp
        # --- source_mirror: util ---
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/util/test_log/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/util/utils_test/*.cpp
        # --- scenario ---
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/matmul/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/concat/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/add_layer_norm/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/ffn/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/reduce_split_penalty/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/e2e_st_test/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/e2e_brc_buf/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/profiler_e2e/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/att_st_test/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/select_model/*.cpp
        # stub
        ${TOP_DIR}/tests/autofuse/st/att/testcase/stub/*.cpp
        ${TOP_DIR}/tests/autofuse/st/att/utils/*.cpp
)

# 排除依赖不可用头文件的测试（旧版本就不在编译列表中）
list(FILTER SOURCES EXCLUDE REGEX "test_add_layer_norm_e2e\\.cpp$")
# tiling_func/src/*.cpp not compiled in old version
list(FILTER SOURCES EXCLUDE REGEX "tiling_func/src/.+\\.cpp$")
# solver_pass/src/*.cpp 是源码镜像，旧版本从 ATT_DIR 编译
list(FILTER SOURCES EXCLUDE REGEX "solver_pass/src/.+\\.cpp$")

file(GLOB DEPENDS)

add_executable(att_st
        ${DEPENDS}
        ${SOURCES}
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/generator_utils/kernel_tiling/kernel_tiling.h
)
target_link_directories(att_st PRIVATE ${ASCEND_INSTALL_LIB_PATH})
target_include_directories(att_st PRIVATE
        ${TOP_DIR}/tests/autofuse/st/att/testcase
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/generator/solver_pass/general_solver
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/gen_model_info_content
        ${TOP_DIR}/tests/autofuse/st/att/testcase/source_mirror/util
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario
        ${TOP_DIR}/tests/autofuse/st/att/testcase/scenario/common
        ${TOP_DIR}/tests/autofuse/st/att/utils
        ${TOP_DIR}/tests/autofuse/common/stub
        ${TOP_DIR}
        ${TOP_DIR}/compiler/graph/optimize/autofuse
        ${TOP_DIR}/common
        ${TOP_DIR}/base
        ${TOP_DIR}/tests/autofuse/st/att/utils
        ${TOP_DIR}/tests/autofuse/depends/common/inc
        ${ATT_DIR}/util
        ${ATT_DIR}/common_utils
        ${ATT_DIR}/generator
        ${ATT_DIR}/generator/preprocess
        ${ATT_DIR}/generator/solver_pass
        ${ATT_DIR}/generator/solver_pass_gen
        ${ATT_DIR}/generator/cache
        ${ATT_DIR}/gen_model_info
        ${ATT_DIR}/gen_model_info
        ${ASCEND_INSTALL_PATH}/include
        ${CMAKE_BINARY_DIR}/proto/metadef_protos
        ${CMAKE_BINARY_DIR}/ascir_builtin_ops/ascir_ops.h
)

target_compile_options(att_st PRIVATE
        -g --coverage -fprofile-arcs -ftest-coverage
        -fPIC
        -fno-common
        -Wextra
        -Wfloat-equal
        -fpermissive
        )
target_compile_definitions(att_st PRIVATE ASCEND_INSTALL_PATH="${ASCEND_INSTALL_PATH}")

target_link_options(att_st PRIVATE
        -rdynamic
        -Wl,-Bsymbolic
        -Wl,--exclude-libs,All
        )

target_link_libraries(att_st
        GTest::gtest GTest::gtest_main -lgcov
        symengine
        Boost::boost
        atrace
        att
        asc_slog_stub
        autofuse_runtime_stub
        aihac_ir
        ascir_builtin_ops
        aihac_symbolizer
        ascir_ops_headers
        aihac_ir_register
        graph_base
        graph
        error_manager
        c_sec
        json
        ascend_protobuf
        )
add_dependencies(att_st ascir_builtin_ops)

add_test(NAME att_st COMMAND att_st --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/st/att_st.xml)
set_tests_properties(att_st PROPERTIES LABELS "st;att_st")