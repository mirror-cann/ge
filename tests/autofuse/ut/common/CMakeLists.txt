set(ATRACE_ROOT_DIR ${CODE_ROOT_DIR}/../../../../base/common/atrace)
file(GLOB_RECURSE ATRACE_SRC_FILES CONFIGURE_DEPENDS
        "${ATRACE_ROOT_DIR}/*.cpp"
        "${ATRACE_ROOT_DIR}/*.cc"
)

file(GLOB_RECURSE COMMON_TEST_SRCS CONFIGURE_DEPENDS "*.cpp" "*.cc")
add_executable(test_common
        test.cpp
        ${COMMON_TEST_SRCS}
        ${ATRACE_SRC_FILES}
)

target_include_directories(test_common PRIVATE
        ${CODE_ROOT_DIR}
        ${CODE_ROOT_DIR}/common
        ${CODE_ROOT_DIR}/../../../../base/common
)

set(ASCGEN_DT_COMPILE_OPTION
        -O0
        -g
        --coverage -fprofile-arcs -ftest-coverage
)

target_compile_options(test_common PRIVATE ${ASCGEN_DT_COMPILE_OPTION}
    -Werror=format
    -Wno-deprecated-declarations
    -Wall -Wfloat-equal
    -Wno-subobject-linkage
)

target_link_options(test_common PRIVATE
    -Wl,--disable-new-dtags
)

target_link_libraries(test_common PRIVATE
    GTestShared::gtest
    GTestShared::gmock
    GTestShared::gtest_main
    -lgcov
    -Wl,--no-as-needed
    asc_slog_stub
    autofuse_runtime_stub
    atrace
    json
    error_manager
    metadef
    ascend_protobuf
    c_sec
    mmpa
)