set(GE_API_C_WRAPPER_DIR "${AIR_CODE_DIR}/api/python/ge/ge_api_c_wrapper")

set(GRAPH_WRAPPER_SRCS
        "${GE_API_C_WRAPPER_DIR}/c_anyvalue.cc"
        "${GE_API_C_WRAPPER_DIR}/c_graph.cc"
        "${GE_API_C_WRAPPER_DIR}/c_gnode.cc"
        "${GE_API_C_WRAPPER_DIR}/c_tensor.cc"
)

set(GE_RUNTIME_WRAPPER_SRCS
        "${GE_API_C_WRAPPER_DIR}/c_geapi.cc"
        "${GE_API_C_WRAPPER_DIR}/c_session.cc"
        "${GE_API_C_WRAPPER_DIR}/c_tensor_runtime.cc"
)

set(OFFLINE_COMPILE_WRAPPER_SRCS
        "${GE_API_C_WRAPPER_DIR}/c_offline_compile.cc"
)

set(PYATC_WRAPPER_SRCS
        "${GE_API_C_WRAPPER_DIR}/c_pyatc.cc"
)

set(GE_API_C_WRAPPER_COMMON_LINK_LIBS
        intf_pub
        slog_headers
        mmpa_headers
        metadef_headers
        c_sec
        graph
        graph_base
        -lrt
        -ldl
        ${AIR_COMMON_LINK_OPTION}
        air_headers
        $<$<CONFIG:Release>:-s>
)

############ graph_wrapper ############
add_library(graph_wrapper SHARED ${GRAPH_WRAPPER_SRCS})
target_compile_options(graph_wrapper PRIVATE ${AIR_COMMON_DYNAMIC_COMPILE_OPTION})
target_include_directories(graph_wrapper SYSTEM PRIVATE
        ${AIR_CODE_DIR}/base
)
target_link_libraries(graph_wrapper PRIVATE
        ${GE_API_C_WRAPPER_COMMON_LINK_LIBS}
)

############ ge_runtime_wrapper ############
add_library(ge_runtime_wrapper SHARED ${GE_RUNTIME_WRAPPER_SRCS})
target_compile_options(ge_runtime_wrapper PRIVATE ${AIR_COMMON_DYNAMIC_COMPILE_OPTION})
target_include_directories(ge_runtime_wrapper SYSTEM PRIVATE
        ${AIR_CODE_DIR}/base
)
target_link_libraries(ge_runtime_wrapper PRIVATE
        ${GE_API_C_WRAPPER_COMMON_LINK_LIBS}
        runtime_headers
        msprof_headers
        ge_runner
)

############ offline_compile_wrapper ############
add_library(offline_compile_wrapper SHARED ${OFFLINE_COMPILE_WRAPPER_SRCS})
target_compile_options(offline_compile_wrapper PRIVATE ${AIR_COMMON_DYNAMIC_COMPILE_OPTION})
target_include_directories(offline_compile_wrapper SYSTEM PRIVATE
        ${AIR_CODE_DIR}/base
)
target_link_libraries(offline_compile_wrapper PRIVATE
        ${GE_API_C_WRAPPER_COMMON_LINK_LIBS}
        ge_compiler
)

############ pyatc_wrapper ############
add_library(pyatc_wrapper SHARED ${PYATC_WRAPPER_SRCS})
target_compile_options(pyatc_wrapper PRIVATE ${AIR_COMMON_DYNAMIC_COMPILE_OPTION})
target_include_directories(pyatc_wrapper SYSTEM PRIVATE
        ${AIR_CODE_DIR}/api/atc
)
target_link_options(pyatc_wrapper PRIVATE
        -Wl,-Bsymbolic
)
target_link_libraries(pyatc_wrapper PRIVATE
        intf_pub
        atc_static
        -lrt
        -ldl
        ${AIR_COMMON_LINK_OPTION}
        $<$<CONFIG:Release>:-s>
)

add_custom_target(ge_api_c_wrapper
        DEPENDS graph_wrapper ge_runtime_wrapper offline_compile_wrapper pyatc_wrapper
)
