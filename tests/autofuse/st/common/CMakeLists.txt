set(ATRACE_ROOT_DIR ${CODE_ROOT_DIR}/../../../../base/common/atrace)
file(GLOB_RECURSE ATRACE_SRC_FILES CONFIGURE_DEPENDS
        "${ATRACE_ROOT_DIR}/*.cpp"
        )

file(GLOB_RECURSE COMMON_TEST_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(test_common_st
        test.cpp
        ${COMMON_TEST_SRCS}
        ${ATRACE_SRC_FILES}
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

target_include_directories(test_common_st PRIVATE
        ${CODE_ROOT_DIR}/../../../../base/common
)

target_compile_options(test_common_st PRIVATE ${ASCGEN_DT_COMPILE_OPTION}
        -Werror=format
        -Wno-deprecated-declarations
        -Wall -Wfloat-equal
        -Wno-subobject-linkage
)

target_link_options(test_common_st PRIVATE
        -Wl,--disable-new-dtags
)

target_link_libraries(test_common_st ${ASCGEN_COMMON_LINK_OPTION}
        -Wl,--no-as-needed
        atrace
        json
        mmpa_headers
        metadef_headers
        error_manager
        ascend_protobuf
        c_sec
        mmpa
        unified_dlog
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME test_common_st COMMAND test_common_st --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/st/test_common_st.xml)
set_tests_properties(test_common_st PROPERTIES LABELS "st;test_common_st")
