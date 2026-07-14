/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_COMPILER_OFFLINE_ATC_FLAGS_H_
#define AIR_CXX_COMPILER_OFFLINE_ATC_FLAGS_H_

#include "cmd_flag_info.h"

DECLARE_string(model);
DECLARE_string(output);
DECLARE_int32(framework);
DECLARE_string(weight);
DECLARE_string(input_shape);
DECLARE_string(input_hint_shape);
DECLARE_string(input_shape_range);
DECLARE_bool(help);
DECLARE_string(cal_conf);
DECLARE_string(insert_op_conf);
DECLARE_string(op_name_map);
DECLARE_string(target);
DECLARE_string(om);
DECLARE_string(json);
DECLARE_int32(mode);
DECLARE_string(out_nodes);
DECLARE_string(op_precision_mode);
DECLARE_string(allow_hf32);
DECLARE_string(precision_mode);
DECLARE_string(precision_mode_v2);
DECLARE_string(modify_mixlist);
DECLARE_string(keep_dtype);
DECLARE_string(input_format);
DECLARE_string(check_report);
DECLARE_string(input_fp16_nodes);
DECLARE_string(is_output_adjust_hw_layout);
DECLARE_string(is_input_adjust_hw_layout);
DECLARE_string(output_type);
DECLARE_string(op_select_implmode);
DECLARE_string(optypelist_for_implmode);
DECLARE_string(singleop);
DECLARE_int32(disable_reuse_memory);
DECLARE_string(auto_tune_mode);
DECLARE_string(jit_compile);
DECLARE_string(optimization_switch);
DECLARE_string(static_model_ops_lower_limit);
DECLARE_string(h2d_overlapped_with_compute);
DECLARE_string(raw_ge_options);
DECLARE_bool(raw_ge_options_ignore_unsupported);
DECLARE_string(soc_version);
DECLARE_int32(virtual_type);
DECLARE_string(core_type);
DECLARE_string(aicore_num);
DECLARE_string(buffer_optimize);
DECLARE_string(fusion_switch_file);
DECLARE_string(compression_optimize_conf);
DECLARE_string(customize_dtypes);
DECLARE_string(op_debug_config);
DECLARE_string(save_original_model);
DECLARE_string(dynamic_batch_size);
DECLARE_string(dynamic_image_size);
DECLARE_string(dynamic_dims);
DECLARE_string(enable_small_channel);
DECLARE_string(enable_compress_weight);
DECLARE_string(enable_attr_compression);
DECLARE_string(compress_weight_conf);
DECLARE_int32(sparsity);
DECLARE_string(enable_single_stream);
DECLARE_string(ac_parallel_enable);
DECLARE_string(tiling_schedule_optimize);
DECLARE_string(log);
DECLARE_string(dump_mode);
DECLARE_int32(op_debug_level);
DECLARE_string(enable_scope_fusion_passes);
DECLARE_string(debug_dir);
DECLARE_string(status_check);
DECLARE_string(op_compiler_cache_dir);
DECLARE_string(op_compiler_cache_mode);
DECLARE_string(mdl_bank_path);
DECLARE_string(op_bank_path);
DECLARE_string(display_model_info);
DECLARE_string(device_id);
DECLARE_string(shape_generalized_build_mode);
DECLARE_string(atomic_clean_policy);
DECLARE_string(external_weight);
DECLARE_string(deterministic);
DECLARE_string(deterministic_level);
DECLARE_string(host_env_os);
DECLARE_string(host_env_cpu);
DECLARE_string(cluster_config);
DECLARE_string(hccl_sub_comm_config);
DECLARE_string(quant_dumpable);
DECLARE_string(is_weight_clip);
DECLARE_string(oo_level);
DECLARE_string(build_config);

#endif  // AIR_CXX_COMPILER_OFFLINE_ATC_FLAGS_H_
