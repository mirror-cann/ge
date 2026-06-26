# -----------------------------------------------------------------------------------------------------------
# Copyright (c) 2025 Huawei Technologies Co., Ltd.
# This program is free software, you can redistribute it and/or modify it under the terms and conditions of
# CANN Open Software License Agreement Version 2.0 (the "License").
# Please refer to the License for details. You may not use this file except in compliance with the License.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
# See LICENSE in the root of the software repository for the full text of the License.
# -----------------------------------------------------------------------------------------------------------
enable_testing()

file(GLOB_RECURSE PROTO_LIST RELATIVE ${CMAKE_CURRENT_LIST_DIR}
        "${METADEF_PROTO_DIR}/task.proto"
        "${METADEF_PROTO_DIR}/ge_ir.proto"
        "${METADEF_PROTO_DIR}/om.proto"
        "${METADEF_PROTO_DIR}/insert_op.proto"
        "${METADEF_PROTO_DIR}/flow_model.proto"
        "${METADEF_PROTO_DIR}/ge_ir_mobile.proto"
        "${METADEF_PROTO_DIR}/task_mobile.proto")

protobuf_generate(fe_ut PROTO_SRCS PROTO_HDRS ${PROTO_LIST})

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugin/opskernel/fe_config")
configure_file(
    "${AIR_CODE_DIR}/compiler/engines/nn_engine/optimizer/fe_config/fe.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/plugin/opskernel/fe_config/fe.ini"
    COPYONLY
)

# Shared library for common test utilities
add_library(fe_ut_common STATIC
    ${FE_SRC_CC_LIST}
    ${PROTO_HDRS}
    ${FE_TEST_ROOT_PATH}/graph_constructor/fe_llt_utils.cc
    ${FE_TEST_ROOT_PATH}/graph_constructor/pass_manager.cc
    ${FE_TEST_ROOT_PATH}/graph_constructor/ub_pass_manager.cc
    ${FE_TEST_ROOT_PATH}/graph_constructor/graph_builder_utils.cc
    ${FE_TEST_ROOT_PATH}/graph_constructor/graph_constructor.cc
)

target_include_directories(fe_ut_common PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${FE_ROOT_PATH}/executor
    ${FE_ROOT_PATH}/inc
    ${FE_ROOT_PATH}/optimizer
    ${FE_ROOT_PATH}/utils
    ${FE_ROOT_PATH}/fusion
    ${FE_ROOT_PATH}/opskernel
    ${AIR_CODE_DIR}/compiler/engines/ffts_engine
    ${AIR_CODE_DIR}/compiler/engines/ffts_engine/inc
    ${AIR_CODE_DIR}/compiler/opcompiler/inc
    ${AIR_CODE_DIR}/tests/engines/nn_engine/depends
    ${AIR_CODE_DIR}/tests/engines/nn_engine/depends/ops_stub
    ${AIR_CODE_DIR}/tests/engines/nn_engine/graph_constructor
    ${AIR_CODE_DIR}/
    ${AIR_CODE_DIR}/inc
    ${AIR_CODE_DIR}/base
    ${AIR_CODE_DIR}/inc/external
    ${AIR_CODE_DIR}/inc/framework
    ${AIR_CODE_DIR}/inc/graph_metadef/exe_graph
    ${AIR_CODE_DIR}/compiler/graph/fusion/pass
    ${CMAKE_BINARY_DIR}/proto
    ${CMAKE_BINARY_DIR}/proto/fe_ut
    ${AIR_CODE_DIR}/compiler/opcompiler/op_compile_adapter/source
    ${AIR_CODE_DIR}/tests/engines/nn_engine/ut/stub
    ${AIR_CODE_DIR}/tests/engines/nn_engine/ut/testcase/op_compile_adapter/stub
    ${FE_TEST_ROOT_PATH}
)
target_compile_options(fe_ut_common PUBLIC
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer -g>
    -D__OPTIMIZER_KT_TEST__
    -DDAVINCI_CLOUD
    -Dgoogle=ascend_private
    -fPIC
)
target_compile_definitions(fe_ut_common PUBLIC
    $<$<STREQUAL:${BUILD_OPEN_PROJECT},True>:ONLY_COMPILE_BLUE>
)
target_link_directories(fe_ut_common PUBLIC
    ${ASCEND_INSTALL_PATH}/lib64
    ${ASCEND_INSTALL_PATH}/devlib
)
target_link_libraries(fe_ut_common PUBLIC
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address>
    air_headers
    metadef_headers
    slog_headers
    mmpa_headers
    cce_headers
    msprof_headers
    runtime_headers
    op_compile_adapter_headers
    -Wl,--no-as-needed
    -lrt -ldl -lpthread -lgcov
    $<BUILD_INTERFACE:atrace_headers>
    atrace_stub
    exe_graph
    lowering_pkg
    graph_pkg
    metadef
    register_pkg
    graph_base_pkg
    opp_registry
    op_tiling_rt2_stub
    ge_common_pkg
    ge_compiler_pkg
    runtime_stub_fe
    ascend_protobuf
    c_sec
    platform
    compress
    error_manager
    json
    c_sec_static
    mmpa_stub
    slog_stub
    unified_dlog
    cann_kb_stub
    easy_graph
    ge_graph_dsl
    intf_llt_pub
    GTestShared::gtest
    GTestShared::gtest_main
    GTestShared::gmock
    mockcpp
)

# ============================================================================
# Executable 1: ut_fe_graph_optimizer (32 test files)
# ============================================================================
add_executable(ut_fe_graph_optimizer
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_config_manager/fusion_config_parser_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_config_manager/fusion_priority_manager_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_config_manager/fusion_priority_manager_precise_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_config_manager/fusion_priority_manager_error_path_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fe_report_error_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fe_report_error_coverage_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/tbe_adapter/tbe_op_store_adapter_precise_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/tbe_adapter/tbe_op_store_adapter_error_path_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/fusion_common/fusion_pass_manager_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/fusion_common/graph_node_map_util_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/dynamic_shape_optimizer/fuzzy_compiler/fuzzy_generalize_utest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/dynamic_shape_optimizer/model_binary_compiler/model_binary_compiler_utest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/checker/concat_optimize_checker_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/stridedwrite_optimizer_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/stridedread_optimizer_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/split_c_to_n_optimize_uinttest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/conv_concat_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/split_conv_concat_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/refresh_cube_c0_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_bnhost_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_cast_relu_cast_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_batchnorm_bninfer_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_logsoftmaxgrad_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_matmul_biasadd_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_matmul_cast_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_pass_conv_weight_compress_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_swap_merge_cast_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_concat_tile_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_depthwise_insert_transdata_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_strided_slice_d_to_split_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_concat_c_optimize_fusion_pass_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/pass_fusion_manager/optimize_original_graph_process_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/fusion_engine_graph_replace_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/graph_fusion/graph_matcher_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/shape_format_transfer/trans_node_implementation/trans_node_reformat_generator_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/weight_prefetch/weight_prefetch_utils_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_fusion/graph_fusion_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/fusion_engine_coverage.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/fusion_engine_fe_graph_optimizer_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/graph_optimizer/check_graph_cycle_unittest.cc
)
target_link_libraries(ut_fe_graph_optimizer PRIVATE -Wl,--whole-archive fe_ut_common -Wl,--no-whole-archive)
add_test(NAME ut_fe_graph_optimizer COMMAND ut_fe_graph_optimizer --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/ut/ut_fe_graph_optimizer.xml)
set_tests_properties(ut_fe_graph_optimizer PROPERTIES LABELS "ut;fe;ut_fe_graph_optimizer" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")

# ============================================================================
# Executable 2: ut_fe_fusion (33 test files)
# fusion_manager + fusion_rule + ub_fusion + format_selector
# ============================================================================
add_executable(ut_fe_fusion
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    # fusion_manager (9 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_configuration.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_platform_utils.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_fusion_graph_merge_util.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_fusion_manager.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_graph_comm.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_itf.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_op_info_common.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_pass_manager.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_manager/ut_tensor_compute_util.cc
    # fusion_rule (6 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_rule_manager/fusion_rule_manager_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_rule_manager/fusion_rule_manager_uttest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_rule_parser/fusion_rule_cycle_detection_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_rule_parser/fusion_rule_json_anchor_uttest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_rule_parser/fusion_rule_json_edge_uttest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/fusion_rule_parser/fusion_rule_json_outer_uttest.cc
    # ub_fusion (12 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/automatic_ub_fusion_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/auto_buffer_fusion_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/fusion_stub.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/scopeallocator_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/buffer_fusion_cycle_detection_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/tbe_l1_fusion_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/tbe_ub_fusion_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/ub_slice_info_unittest/aipp_conv_slice_info_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/ub_slice_info_unittest/conv_dequant_slice_info_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/ub_slice_info_unittest/conv_dequants16_slice_info_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/ub_slice_info_unittest/conv_requants16_slice_info_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ub_fusion/ub_slice_info_unittest/ub_pass_slice_info_manager_ut.cc
    # format_selector (6 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/format_selector/format_dtype_selector_manager_broadcast_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/format_selector/format_dtype_selector_manager_customize_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/format_selector/format_dtype_selector_manager_kernel_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/format_selector/reduce/format_selector_reduce_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/format_selector/reduce/format_selector_reduce_nz_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/format_selector/format_dtype_selector_manager_fz_3d_unittest.cc
)
target_link_libraries(ut_fe_fusion PRIVATE -Wl,--whole-archive fe_ut_common -Wl,--no-whole-archive)
add_test(NAME ut_fe_fusion COMMAND ut_fe_fusion --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/ut/ut_fe_fusion.xml)
set_tests_properties(ut_fe_fusion PROPERTIES LABELS "ut;fe;ut_fe_fusion" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")

# ============================================================================
# Executable 3: ut_fe_adapter_ops (34 test files)
# adapter + op_compiler + op_judge + ops_kernel + task_builder + cmo
# ============================================================================
add_executable(ut_fe_adapter_ops
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    # adapter (7 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/tbe_info/tbe_time_estimator_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/adapter_itf/op_store_adapter_manage_unitest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/adapter_itf/task_builder_adapter_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/tbe_adapter/tbe_task_builder_adapter_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/tbe_adapter/tbe_op_store_adapter_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/tbe_info/tbe_single_op_info_assemble_unitest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/adapter/tbe_info/tbe_info_assembler_unittest.cc
    # op_compiler (4 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_compiler/fusion_engine_op_compiler_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_compiler/fusion_engine_tbe_compiler_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_compiler/fusion_engine_tbe_json_parser_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_compiler/fusion_engine_sgt_tbe_compiler_unittest.cc
    # op_judge (9 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/fusion_engine_op_judge_function_op_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/fusion_engine_op_judge_unittest_conv3d_related.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/fusion_engine_op_judge_unittest_fz_group.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/fusion_engine_op_judge_unittest_fz_lstm.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/fusion_engine_op_judge_unittest_rnn.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/fusion_engine_op_judge_unittest_new.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/fusion_engine_op_judge_unittest_precision_mode.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/fusion_engine_op_judge_unknown_shape_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_judge/op_format_dtype_update_desc_base_ut.cc
    # ops_kernel (9 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_builder/ops_kernel_builder_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_builder/dsa_ops_kernel_builder_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_store/check_attr_support_utest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_store/check_param_type_utest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_store/json_util_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_store/op_info_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_store/ops_kernel_info_store_singleop_compile_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_store/ops_kernel_info_store_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_kernel_store/ops_kernel_info_store_unittest_2.cc
    # task_builder (5 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/task_builder/args_format_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/task_builder/task_builder_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/task_builder/superkernel_task_builder_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/task_builder/aicore_calc_op_running_param_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/cmo_task_builder/cmo_task_builder_unittest.cc
    # cmo (5 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/cmo/generate_cmo_type_base_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/cmo/generate_cmo_type_prefetch_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/cmo/generate_cmo_type_writeback_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/cmo/generate_cmo_type_invalid_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/cmo/generate_cmo_type_manager_ut.cc
)
target_link_libraries(ut_fe_adapter_ops PRIVATE -Wl,--whole-archive fe_ut_common -Wl,--no-whole-archive)
add_test(NAME ut_fe_adapter_ops COMMAND ut_fe_adapter_ops --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/ut/ut_fe_adapter_ops.xml)
set_tests_properties(ut_fe_adapter_ops PROPERTIES LABELS "ut;fe;ut_fe_adapter_ops" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")

# ============================================================================
# Executable 4: ut_fe_shape_common (22 test files)
# shape_format + heavy_format + common
# ============================================================================
add_executable(ut_fe_shape_common
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    # shape_format (9 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/fusion_engine_trans_op_insert_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/expand_dims_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/transfer_shape_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/fusion_engine_trans_op_insert_unknown_shape_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/trans_op_insert_complex_by_original_format_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/trans_op_insert_complex_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/trans_op_insert_complex_unittest_2.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/trans_op_insert_fz_3d_with_reshape.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/shape_format_transfer/trans_op_insert_RNN.cc
    # heavy_format (7 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/heavy_format_propagation/ut_heavy_format_propagation.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/heavy_format_propagation/ut_dsl_test.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/heavy_format_propagation/ut_heavy_format_propagation_complex.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/heavy_format_propagation/ut_heavy_format_propagation_format_continous.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/heavy_format_propagation/ut_heavy_format_propagation_fzg.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/heavy_format_propagation/ut_heavy_format_propagation_graph_constructor.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/heavy_format_propagation/ut_heavy_format_propagation_ts_op.cc
    # common (9 files)
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/dump_util_utest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/math_util_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/fe_fp16_utest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/graph_common_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/fusion_op_comm_utest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/format/axis_name_util_utest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/format/range_axis_util_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/fusion_statistic/buffer_fusion_info_collector_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/common/ut_lxfusion_json_util.cc
)
target_link_libraries(ut_fe_shape_common PRIVATE -Wl,--whole-archive fe_ut_common -Wl,--no-whole-archive)
add_test(NAME ut_fe_shape_common COMMAND ut_fe_shape_common --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/ut/ut_fe_shape_common.xml)
set_tests_properties(ut_fe_shape_common PROPERTIES LABELS "ut;fe;ut_fe_shape_common" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")

# ============================================================================
# Executable 5: ut_fe_others (11 test files)
# ============================================================================
add_executable(ut_fe_others
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/dsa_optimizer/fusion_engine_dsa_graph_optimizer_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ffts/ffts_task_builder_adapter_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/l2_optimizer/fusion_engine_l2_optimzer_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/lx_fusion_optimizer/lx_fusion_optimizer_unit_test.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_calculator/ut_op_calculator.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/ops_store/ut_ops_kernel_manager.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/param_calculate/tensorsize_calculator_ut.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_setter/op_slice_info_setter_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/op_setter/op_setter_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/trace_handle_manager/trace_handle_manager_unittest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_engine/update_axis/op_axis_update_desc_unittest.cc
)
target_link_libraries(ut_fe_others PRIVATE -Wl,--whole-archive fe_ut_common -Wl,--no-whole-archive)
add_test(NAME ut_fe_others COMMAND ut_fe_others --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/ut/ut_fe_others.xml)
set_tests_properties(ut_fe_others PROPERTIES LABELS "ut;fe;ut_fe_others" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")
