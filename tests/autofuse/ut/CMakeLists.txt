set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
add_subdirectory(autofuse)
add_subdirectory(common)

add_executable(test_main test_main.cpp)

set(PEM_DAVINCI_PATH "${ASCEND_INSTALL_PATH}/x86_64-linux/simulator/dav_2201/lib/libpem_davinci.so")
if(EXISTS "${PEM_DAVINCI_PATH}")
    target_link_libraries(test_main PRIVATE ${PEM_DAVINCI_PATH})
endif()

target_link_libraries(test_main PRIVATE
    atrace
    error_manager
    aihac_symbolizer
    runtime
    metadef
    GTest::gtest
)
if(TARGET ascir_ops_headers)
    add_dependencies(test_main ascir_ops_headers)
endif()
gtest_discover_tests(test_main)