file(GLOB_RECURSE OFFLINE_GRAPH_SRC CONFIGURE_DEPENDS "*.cpp")
add_library(offline_graph_showcase OBJECT ${OFFLINE_GRAPH_SRC})
target_link_libraries(offline_graph_showcase PUBLIC ${ES_MATH} ${GRAPH} ${GRAPH_BASE} ${C_SEC} ${EAGER_STYLE_GRAPH_BUILDER_BASE})
target_compile_options(offline_graph_showcase PRIVATE -g -O0)
