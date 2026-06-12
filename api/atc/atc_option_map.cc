/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "atc_option_map.h"

#include <utility>

#include "external/ge_common/ge_common_api_types.h"
#include "common/ge_common/ge_types.h"

namespace {
using namespace ge::configure_option;

const std::pair<std::string, std::string> kAtcGeOptionToCliName[] = {
    {ge::ir_option::INPUT_FORMAT, "--input_format"},
    {ge::ir_option::INPUT_SHAPE, "--input_shape"},
    {ge::INPUT_SHAPE, "--input_shape"},
    {ge::INPUT_SHAPE_RANGE, "--input_shape_range"},
    {ge::ir_option::OP_NAME_MAP, "--op_name_map"},
    {"target", "--target"},
    {"check_report", "--check_report"},
    {ge::ir_option::IS_INPUT_ADJUST_HW_LAYOUT, "--is_input_adjust_hw_layout"},
    {ge::ir_option::IS_OUTPUT_ADJUST_HW_LAYOUT, "--is_output_adjust_hw_layout"},
    {ge::ir_option::ENABLE_SCOPE_FUSION_PASSES, "--enable_scope_fusion_passes"},
    {ge::ir_option::DYNAMIC_BATCH_SIZE, "--dynamic_batch_size"},
    {ge::ir_option::DYNAMIC_IMAGE_SIZE, "--dynamic_image_size"},
    {ge::ir_option::DYNAMIC_DIMS, "--dynamic_dims"},
    {ge::OP_PRECISION_MODE, "--op_precision_mode"},
    {ge::PRECISION_MODE, "--precision_mode"},
    {ge::SOC_VERSION, "--soc_version"},
    {ge::VIRTUAL_TYPE, "--virtual_type"},
    {ge::CORE_TYPE, "--core_type"},
    {ge::AICORE_NUM, "--aicore_num"},
    {ge::OP_SELECT_IMPL_MODE, "--op_select_implmode"},
    {ge::OPTYPELIST_FOR_IMPLMODE, "--optypelist_for_implmode"},
    {ge::OP_DEBUG_LEVEL, "--op_debug_level"},
    {ge::DEBUG_DIR, "--debug_dir"},
    {ge::OP_COMPILER_CACHE_DIR, "--op_compiler_cache_dir"},
    {ge::OP_COMPILER_CACHE_MODE, "--op_compiler_cache_mode"},
    {ge::MDL_BANK_PATH_FLAG, "--mdl_bank_path"},
    {ge::OP_BANK_PATH_FLAG, "--op_bank_path"},
    {ge::DISPLAY_MODEL_INFO, "--display_model_info"},
    {ge::TUNE_DEVICE_IDS, "--device_id"},
    {ge::MODIFY_MIXLIST, "--modify_mixlist"},
    {ge::ENABLE_SMALL_CHANNEL, "--enable_small_channel"},
    {ge::ENABLE_SPARSE_MATRIX_WEIGHT, "--sparsity"},
    {ge::ATOMIC_CLEAN_POLICY, "--atomic_clean_policy"},
    {ge::EXTERNAL_WEIGHT, "--external_weight"},
    {ge::DETERMINISTIC, "--deterministic"},
    {"ge.deterministicLevel", "--deterministic_level"},
    {ge::CUSTOMIZE_DTYPES, "--customize_dtypes"},
    {"keep_dtype", "--keep_dtype"},
    {"compress_weight_conf", "--compress_weight_conf"},
    {ge::FRAMEWORK_TYPE, "--framework"},
    {ge::CALIBRATION_CONF_FILE, "--cal_conf"},
    {ge::OUTPUT_NODE_NAME, "--out_nodes"},
    {ge::INSERT_OP_FILE, "--insert_op_conf"},
    {ge::PRECISION_MODE_V2, "--precision_mode_v2"},
    {ge::ALLOW_HF32, "--allow_hf32"},
    {ge::OUTPUT_DATATYPE, "--output_type"},
    {ge::INPUT_FP16_NODES, "--input_fp16_nodes"},
    {ge::OPTION_EXEC_DISABLE_REUSED_MEMORY, "--disable_reuse_memory"},
    {ge::AUTO_TUNE_MODE, "--auto_tune_mode"},
    {ge::BUFFER_OPTIMIZE, "--buffer_optimize"},
    {ge::FUSION_SWITCH_FILE, "--fusion_switch_file"},
    {ge::COMPRESSION_OPTIMIZE_CONF, "--compression_optimize_conf"},
    {ge::OP_DEBUG_CONFIG, "--op_debug_config"},
    {ge::ENABLE_COMPRESS_WEIGHT, "--enable_compress_weight"},
    {ge::ENABLE_ATTR_COMPRESSION, "--enable_attr_compression"},
    {ge::ENABLE_SINGLE_STREAM, "--enable_single_stream"},
    {ge::AC_PARALLEL_ENABLE, "--ac_parallel_enable"},
    {ge::TILING_SCHEDULE_OPTIMIZE, "--tiling_schedule_optimize"},
    {"log", "--log"},
    {"dump_mode", "--dump_mode"},
    {ge::STATUS_CHECK, "--status_check"},
    {ge::SAVE_ORIGINAL_MODEL, "--save_original_model"},
    {ge::SHAPE_GENERALIZED_BUILD_MODE, "--shape_generalized_build_mode"},
    {ge::OPTION_HOST_ENV_OS, "--host_env_os"},
    {ge::OPTION_HOST_ENV_CPU, "--host_env_cpu"},
    {ge::QUANT_DUMPABLE, "--quant_dumpable"},
    {"ge.is_weight_clip", "--is_weight_clip"},
    {ge::CLUSTER_CONFIG, "--cluster_config"},
    {ge::HCCL_SUB_COMM_CONFIG, "--hccl_sub_comm_config"},
    {ge::OO_LEVEL, "--oo_level"},
    {ge::INPUT_HINT_SHAPE, "--input_hint_shape"},
    {ge::JIT_COMPILE, "--jit_compile"},
    {ge::OPTIMIZATION_SWITCH, "--optimization_switch"},
    {ge::OPTION_STATIC_MODEL_OPS_LOWER_LIMIT, "--static_model_ops_lower_limit"},
};
}  // namespace

namespace ge {
std::map<std::string, std::string> BuildAtcGeOptionToCliNameMap() {
  std::map<std::string, std::string> option_name_map;
  for (const auto &option_to_cli_name : kAtcGeOptionToCliName) {
    option_name_map.emplace(option_to_cli_name.first, option_to_cli_name.second);
  }
  return option_name_map;
}
}  // namespace ge
