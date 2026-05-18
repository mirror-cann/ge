file(GLOB_RECURSE AUTOFUSE_DT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/ut/autofuse/*.cpp")

target_sources(autofusion_st PRIVATE
        ${AUTOFUSE_DT_FILES}
)

target_sources(autofusion_ut PRIVATE
        ${AUTOFUSE_DT_FILES}
)
