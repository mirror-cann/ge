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

protobuf_generate(fe_st PROTO_SRCS PROTO_HDRS ${PROTO_LIST})

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/plugin/opskernel/fe_config")
configure_file(
    "${AIR_CODE_DIR}/compiler/engines/nn_engine/optimizer/fe_config/fe.ini"
    "${CMAKE_CURRENT_BINARY_DIR}/plugin/opskernel/fe_config/fe.ini"
    COPYONLY
)

# Shared library for common test utilities
add_library(fe_st_common STATIC
    ${FE_SRC_CC_LIST}
    ${PROTO_HDRS}
    ${FE_TEST_ROOT_PATH}/graph_constructor/fe_llt_utils.cc
    ${FE_TEST_ROOT_PATH}/graph_constructor/pass_manager.cc
    ${FE_TEST_ROOT_PATH}/graph_constructor/ub_pass_manager.cc
    ${FE_TEST_ROOT_PATH}/graph_constructor/graph_builder_utils.cc
    ${FE_TEST_ROOT_PATH}/graph_constructor/graph_constructor.cc
    # ${CMAKE_CURRENT_LIST_DIR}/stub/Python.cc
    # ${CMAKE_CURRENT_LIST_DIR}/stub/Python_stub.cc
    # ${CMAKE_CURRENT_LIST_DIR}/stub/te_llt_utils.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/stub/common_stub.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/stub/example_stest_stub.cc
)

target_include_directories(fe_st_common PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    ${FE_ROOT_PATH}/executor
    ${ASCEND_INSTALL_PATH}/include
    ${FE_ROOT_PATH}/inc
    ${FE_ROOT_PATH}/optimizer
    ${FE_ROOT_PATH}/utils
    ${FE_ROOT_PATH}/fusion
    ${FE_ROOT_PATH}/opskernel
    ${AIR_CODE_DIR}/compiler/opcompiler/op_compile_adapter/source

    #ge_compiler_depends
    ${AIR_CODE_DIR}/
    ${AIR_CODE_DIR}/inc
    ${AIR_CODE_DIR}/inc/external
    ${AIR_CODE_DIR}/inc/framework
    ${AIR_CODE_DIR}/inc/graph_metadef/exe_graph
    ${AIR_CODE_DIR}/compiler
    ${AIR_CODE_DIR}/compiler/opcompiler/inc
    ${AIR_CODE_DIR}/base

    ${AIR_CODE_DIR}/compiler/graph/fusion/pass
    ${AIR_CODE_DIR}/compiler/engines/ffts_engine
    ${AIR_CODE_DIR}/compiler/engines/ffts_engine/inc
    ${AIR_CODE_DIR}/tests/engines/nn_engine/depends
    ${AIR_CODE_DIR}/tests/engines/nn_engine/depends/ops_stub
    ${AIR_CODE_DIR}/tests/engines/nn_engine/graph_constructor
    ${GTEST_INCLUDE}
    ${CMAKE_BINARY_DIR}/proto/fe_st
    ${FE_TEST_ROOT_PATH}
    ${AIR_CODE_DIR}/compiler/opcompiler/op_compile_adapter/source
    ${AIR_CODE_DIR}/tests/engines/nn_engine/st/stub
    ${AIR_CODE_DIR}/tests/engines/nn_engine/st/testcase/op_compile_adapter/stub
    )

target_compile_options(fe_st_common PUBLIC
    $<$<STREQUAL:${ENABLE_ASAN},true>:-fsanitize=address -fsanitize-recover=address,all -fno-omit-frame-pointer -g>
    -D__OPTIMIZER_KT_TEST__
    -DDAVINCI_CLOUD
    -Dgoogle=ascend_private
    -fPIC
)

target_compile_definitions(fe_st_common PUBLIC
    $<$<STREQUAL:${BUILD_OPEN_PROJECT},True>:ONLY_COMPILE_BLUE>
)

target_link_directories(fe_st_common PUBLIC
    ${ASCEND_INSTALL_PATH}/lib64
    ${ASCEND_INSTALL_PATH}/devlib
)

target_link_libraries(fe_st_common PUBLIC
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
# Executable 1: st_fe_graph_optimizer — graph_optimizer (pass_fusion_manager,
# optimize_original_graph, dynamic_shape_optimizer, etc.) + process tests
# ============================================================================
add_executable(st_fe_graph_optimizer
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    # graph_optimizer/graph_fusion/pass_fusion_manager
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/stridedwrite_optimizer_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/stridedread_optimizer_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/checker/concat_optimize_checker_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/split_c_to_n_optimize_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/conv_concat_fusion_pass_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/node_optimize/split_conv_concat_fusion_pass_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/refresh_cube_c0_fusion_pass_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_bnhost_fusion_pass_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_batchnorm_bninfer_fusion_pass_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_concat_tile_fusion_pass_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_derelu_fusion_pass_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_logsoftmaxgrad_fusion_pass_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_matmul_biasadd_pass_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_matmul_cast_fusion_pass_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_pass_conv_weight_compress_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_cast_relu_cast_pass_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_depthwise_insert_transdata_fusion_pass_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_strided_slice_d_to_split_fusion_pass_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_concat_c_optimize_fusion_pass_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/pass_fusion_manager/fusion_engine_user_semantic_inference_pass_st.cc
    # graph_optimizer/graph_fusion
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/fusion_engine_graph_replace_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/graph_matcher_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/graph_fusion_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/graph_fusion/st_check_graph_cycle.cc
    # graph_optimizer/fusion_common
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/fusion_common/graph_node_map_util_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/fusion_common/fusion_pass_manager_st.cc
    # graph_optimizer/dynamic_shape_optimizer
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/dynamic_shape_optimizer/fuzzy_compiler/fuzzy_generalize_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/dynamic_shape_optimizer/model_binary_compiler/model_binary_compiler_st.cc
    # graph_optimizer/weight_prefetch
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/weight_prefetch/weight_prefetch_utils_st.cc
    # graph_optimizer coverage
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/fusion_engine_coverage.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/fusion_engine_fe_graph_optimizer_stest.cc
    # compile_with_* process
    ${CMAKE_CURRENT_LIST_DIR}/testcase/compile_with_lxfusion/compile_with_lxfusion_process_test.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/compile_with_buffer_fusion/compile_with_buffer_fusion_proccess_910b_test.cc
    # process tests
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/optimize_original_graph_process_test.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/optimize_original_graph_process_310b_test.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/optimize_original_graph_process_910b_test.cc
)
target_link_libraries(st_fe_graph_optimizer PRIVATE -Wl,--whole-archive fe_st_common -Wl,--no-whole-archive)
add_test(NAME st_fe_graph_optimizer COMMAND st_fe_graph_optimizer --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/st/st_fe_graph_optimizer.xml)
set_tests_properties(st_fe_graph_optimizer PROPERTIES LABELS "st;fe;st_fe_graph_optimizer" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")

# ============================================================================
# Executable 2: st_fe_fusion — fusion_manager + fusion_rule + ub_fusion + format_selector
# ============================================================================
add_executable(st_fe_fusion
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    # fusion_manager
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_manager/st_itf.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_manager/st_fusion_manager.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_manager/st_platform_utils.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_manager/st_configuration.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_manager/st_fusion_graph_merge_util.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_manager/st_graph_comm.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_manager/st_op_info_common.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_manager/st_tensor_compute_util.cc
    # fusion_rule
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_rule_manager/fusion_rule_manager_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_rule_parser/fusion_rule_cycle_detection_stest.cc
    # fusion_config_manager
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_config_manager/fusion_config_parser_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/fusion_config_manager/fusion_priority_manager_stest.cc
    # format_selector
    ${CMAKE_CURRENT_LIST_DIR}/testcase/format_selector/format_dtype_selector_manager_stest.cc
    # ub_fusion
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/ub_pass_slice_info/aipp_conv_slice_info_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/ub_pass_slice_info/conv_dequant_slice_info_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/ub_pass_slice_info/conv_dequants16_slice_info_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/ub_pass_slice_info/conv_requants16_slice_info_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/ub_pass_slice_info/ub_pass_slice_info_manager_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/auto_buffer_fusion_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/fusion_stub.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/network_topology_ub_fusion_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/automatic_ub_fusion_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/scopeallocator_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/buffer_fusion_cycle_detection_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/tbe_l1_fusion_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ub_fusion/tbe_ub_fusion_st.cc
)
target_link_libraries(st_fe_fusion PRIVATE -Wl,--whole-archive fe_st_common -Wl,--no-whole-archive)
add_test(NAME st_fe_fusion COMMAND st_fe_fusion --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/st/st_fe_fusion.xml)
set_tests_properties(st_fe_fusion PROPERTIES LABELS "st;fe;st_fe_fusion" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")

# ============================================================================
# Executable 3: st_fe_adapter_ops — adapter + op_compiler + op_judge + ops_kernel +
# task_builder + cmo + dsa + ffts + trace_handle + op_compile_adapter +
# superkernel_plus + compile_with_*
# ============================================================================
add_executable(st_fe_adapter_ops
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    # adapter
    ${CMAKE_CURRENT_LIST_DIR}/testcase/adapter/tbe_adapter/tbe_op_store_adapter_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/adapter/tbe_adapter/tbe_info_assembler_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/adapter/tbe_adapter/tbe_task_builder_adapter_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/adapter/task_builder_adapter_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/adapter/tbe_adapter/tbe_single_op_info_assembler_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/adapter/tbe_adapter/tbe_time_estimator_stest.cc
    # op_compiler
    ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compiler/fusion_engine_op_compiler_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compiler/fusion_engine_op_compiler_stest_2.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compiler/fusion_engine_tbe_json_parser_stest.cc
    # op_judge
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/op_judge/stest_op_judge_new.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/op_judge/fusion_engine_op_judge_stest_rnn.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/op_judge/fusion_engine_op_judge_stest_conv3d_related.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/op_judge/fusion_engine_op_judge_stest_fz_lstm.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/op_judge/stest_op_judge_by_customize_dtype.cc
    # ops_kernel_store
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ops_kernel_store/check_attr_support_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ops_kernel_store/check_param_type_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ops_kernel_store/json_util_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ops_kernel_store/op_info_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ops_kernel_store/ops_kernel_info_store_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ops_kernel_store/ops_kernel_info_store_stest_2.cc
    # ops_kernel_builder
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ops_kernel_builder/ops_kernel_builder_st.cc
    # task_builder
    ${CMAKE_CURRENT_LIST_DIR}/testcase/task_builder/task_builder_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/task_builder/superkernel_task_builder_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/task_builder/aicore_calc_op_running_param_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/task_builder/args_format_stest.cc
    # cmo
    ${CMAKE_CURRENT_LIST_DIR}/testcase/cmo/generate_cmo_type_base_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/cmo/generate_cmo_type_prefetch_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/cmo/generate_cmo_type_writeback_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/cmo/generate_cmo_type_invalid_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/cmo/generate_cmo_type_manager_st.cc
    # cmo_task_builder
    ${CMAKE_CURRENT_LIST_DIR}/testcase/cmo_task_builder/cmo_task_builder_stest.cc
    # dsa_optimizer
    ${CMAKE_CURRENT_LIST_DIR}/testcase/dsa_optimizer/fusion_engine_dsa_graph_optimizer_stest.cc
    # ffts
    ${CMAKE_CURRENT_LIST_DIR}/testcase/ffts/ffts_task_builder_adapter_stest.cc
    # trace_handle_manager
    ${CMAKE_CURRENT_LIST_DIR}/testcase/trace_handle_manager/trace_handle_manager_st.cc
    # op_compile_adapter
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/api/fusion_api_st.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/cache/te_cache_manager_st.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/common/te_config_info_st.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/te_fusion_base.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/te_fusion_binary_st.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/te_fusion_build_file_st.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/te_fusion_dfx_st.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/te_fusion_fuzzy_build.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/te_fusion_py_obj_st.cc
    # ${CMAKE_CURRENT_LIST_DIR}/testcase/op_compile_adapter/testcase/te_fusion_test.cc
    # superkernel_plus process
    ${CMAKE_CURRENT_LIST_DIR}/testcase/superkernel_plus/superkernel_plus_process_test.cc
)
target_link_libraries(st_fe_adapter_ops PRIVATE -Wl,--whole-archive fe_st_common op_compile_adapter_headers -Wl,--no-whole-archive)
add_test(NAME st_fe_adapter_ops COMMAND st_fe_adapter_ops --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/st/st_fe_adapter_ops.xml)
set_tests_properties(st_fe_adapter_ops PROPERTIES LABELS "st;fe;st_fe_adapter_ops" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")

# ============================================================================
# Executable 4: st_fe_shape_common — shape_format/trans_op_insert + heavy_format +
# common + op_setter + update_axis + param_calculate + l2 + nano + op_calculator
# ============================================================================
add_executable(st_fe_shape_common
    ${CMAKE_CURRENT_LIST_DIR}/testcase/main_common.cc
    # shape_format_transfer
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/shape_format_transfer/trans_node_implementation/trans_node_reformat_generator_st.cc
    # trans_op_insert
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/trans_op_insert/trans_op_insert_complex_by_original_format_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/trans_op_insert/trans_op_insert_complex_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/trans_op_insert/trans_op_insert_complex_stest_2.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/trans_op_insert/trans_op_insert_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/trans_op_insert/trans_op_insert_RNN.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/trans_op_insert/trans_op_insert_fz_3d_with_reshape.cc
    # heavy_format_propagation
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/heavy_format_propagation/st_heavy_format_propagation.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/heavy_format_propagation/st_heavy_format_propagation_fzg.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/heavy_format_propagation/st_heavy_format_propagation_complex.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/heavy_format_propagation/st_heavy_format_propagation_ts_function_op.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/heavy_format_propagation/st_heavy_format_propagation_format_continous.cc
    # common
    ${CMAKE_CURRENT_LIST_DIR}/testcase/common/fusion_op_comm_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/common/fusion_statistic/buffer_fusion_info_collector_st.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/common/st_lxfusion_json_util.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/common/dump_util_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/common/fe_report_error_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/common/math_util_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/common/axis_name_util_stest.cc
    # op_setter
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/op_setter/op_setter_stest.cc
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/op_setter/op_slice_info_stest.cc
    # update_axis
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/update_axis/op_axis_update_desc_stest.cc
    # param_calculate
    ${CMAKE_CURRENT_LIST_DIR}/testcase/param_calculate/tensorsize_calculator_st.cc
    # l2_optimizer
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/fusion_engine_l2_optimzer_stest.cc
    # nano process
    ${CMAKE_CURRENT_LIST_DIR}/testcase/nano/nano_process_test.cc
    # op_calculator
    ${CMAKE_CURRENT_LIST_DIR}/testcase/graph_optimizer/op_calculator/st_op_calculator.cc
)
target_link_libraries(st_fe_shape_common PRIVATE -Wl,--whole-archive fe_st_common -Wl,--no-whole-archive)
add_test(NAME st_fe_shape_common COMMAND st_fe_shape_common --gtest_output=xml:${CMAKE_INSTALL_PREFIX}/report/st/st_fe_shape_common.xml)
set_tests_properties(st_fe_shape_common PROPERTIES LABELS "st;fe;st_fe_shape_common" ENVIRONMENT "AIR_CODE_DIR=${CMAKE_SOURCE_DIR}")
