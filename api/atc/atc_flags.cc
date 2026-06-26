/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "atc_flags.h"

DEFINE_string(model, "", "The model file.");
DEFINE_string(output, "", "The output file path&name.");
DEFINE_int32(framework, -1, "Framework type(0:Caffe; 1:MindSpore; 3:Tensorflow; 5:Onnx).");
DEFINE_string(weight, "", "Optional; weight file. Required when framework is Caffe.");

DEFINE_string(input_shape, "",
              "Optional; shape of input data. Required when framework is caffe "
              "or TensorFLow or MindSpore or Onnx. "
              "Format: \"input_name1:n1,c1,h1,w1;input_name2:n2,c2,h2,w2\""
              " or \"input_name1:n1~n2,c1,h1,w1;input_name2:n3~n4,c2,h2,w2\"");
DEFINE_string(input_hint_shape, "",
              "Optional; shape hint of input data."
              "Format: \"index1:[n1,c1,h1,w1];index2:[n2,c2,h2,w2]\"");
DEFINE_string(input_shape_range, "",
              "Deprecated; input_shape_range is deprecated and will be removed in future version. "
              "please use input_shape instead. "
              "shape range of input data. Required when framework is caffe "
              "or TensorFLow or Onnx. "
              "Format: \"input_name1:[n1~n2,c1,h1,w1];input_name2:[n2~n3,c2,h2,w2]\"");
DEFINE_bool(help, false, "show this help message");
DEFINE_string(cal_conf, "", "Optional; the calibration config file.");

DEFINE_string(insert_op_conf, "", "Optional; the config file to insert new op, for example AIPP op.");
DEFINE_string(op_name_map, "", "Optional; custom op name mapping file.");

DEFINE_string(target, "", "Optional; mini.");

DEFINE_string(om, "", "The model file to be converted to json.");
DEFINE_string(json, "", "The output json file path&name which is converted from a model.");
DEFINE_int32(mode, 0,
             "Optional; run mode, 0(default): model => framework model; 1: "
             "framework model => json; 3: only pre-check; 5: txt => json;"
             "30: convert original graph to execute-om for nano(offline model)");

DEFINE_string(out_nodes, "",
              "Optional; output nodes designated by users."
              "Format: \"node_name1:0;node_name1:1;node_name2:0\"");

DEFINE_string(op_precision_mode, "", "Optional; operator precision mode configuration file path.");

DEFINE_string(allow_hf32, "", "Optional; enable hf32. false: disable; true: enable.");

DEFINE_string(precision_mode, "",
              "Optional; precision mode."
              "Support force_fp16, force_fp32, cube_fp16in_fp32out, allow_mix_precision, allow_fp32_to_fp16, "
              "must_keep_origin_dtype, allow_mix_precision_fp16, allow_mix_precision_bf16, allow_fp32_to_bf16.");

DEFINE_string(precision_mode_v2, "",
              "Optional; precision mode v2."
              "Support fp16, origin, cube_fp16in_fp32out, mixed_float16, mixed_bfloat16, "
              "cube_hif8, mixed_hif8.");

DEFINE_string(modify_mixlist, "", "Optional; operator mixed precision configuration file path.");

DEFINE_string(keep_dtype, "",
              "Optional; config file to specify the precision used by the operator during compilation.");

DEFINE_string(input_format, "",
              "Optional; input_format, format of input data, NCHW;NHWC."
              "Format:\"NHWC\"");

DEFINE_string(check_report, "check_result.json", "Optional; the pre-checking report file.");

DEFINE_string(input_fp16_nodes, "",
              "Optional; input node datatype is fp16 and format is NC1HWC0."
              "Format:\"node_name1;node_name2\"");

DEFINE_string(is_output_adjust_hw_layout, "",
              "Optional; Net output node's datatype is fp16 and format is "
              "NC1HWC0, or not."
              "Format:\"false,true,false,true\"");

DEFINE_string(is_input_adjust_hw_layout, "",
              "Optional; Input node's datatype is fp16 and format is "
              "NC1HWC0, or not."
              "Format:\"false,true,false,true\"");

DEFINE_string(output_type, "",
              "Optional; output type! "
              "Support FP32,FP16,INT8,INT16,UINT16,UINT8,INT32,INT64,UINT32,UINT64,DOUBLE.");

DEFINE_string(op_select_implmode, "",
              "Optional; op select implmode! "
              "Support high_precision, high_performance, "
              "high_precision_for_all, high_performance_for_all.");

DEFINE_string(optypelist_for_implmode, "",
              "Optional; Nodes need use implmode selected in op_select_implmode "
              "Format:\"node_name1,node_name2\"");

DEFINE_string(singleop, "", "Optional; If set, generate single op model with the given json file.");

DEFINE_int32(disable_reuse_memory, 0, "Optional; If set to 1, disable reuse memory when generating if.");

DEFINE_string(auto_tune_mode, "", "Optional; Set tune mode.");

DEFINE_string(jit_compile, "1",
              "Optional; set jit compile mode. "
              "0: use binary first; 1(default): compile online; "
              "2: compile online for static shape and use binary for dynamic shape.");

DEFINE_string(optimization_switch, "",
              "Optional; set graph optimization pass switch. "
              "Format: \"pass_name1:on;pass_name2:off\".");

DEFINE_string(static_model_ops_lower_limit, "",
              "Optional; set the lower limit of static subgraph op count in dynamic shape partition. "
              "The value must be an integer greater than or equal to -1.");

DEFINE_string(raw_ge_options, "", "Optional; raw GE options json file path. Only \"compile options\" will be parsed.");

DEFINE_bool(
    raw_ge_options_ignore_unsupported, false,
    "Optional; ignore unsupported raw GE options. false(default): report error; true: ignore unsupported options.");

DEFINE_string(soc_version, "", "The soc version.");
DEFINE_int32(virtual_type, 0, "Optional; enable virtualization. 0(default): disable; 1: enable");

DEFINE_string(core_type, "AiCore", "Optional; If set to VectorCore, only use vector core.");

DEFINE_string(aicore_num, "", "Optional; Set aicore num.");

DEFINE_string(buffer_optimize, "l2_optimize", "Optional; buffer optimize");

DEFINE_string(fusion_switch_file, "", "Optional; Set fusion switch file path.");

DEFINE_string(compression_optimize_conf, "", "Optional; Set compression optimize conf path.");

DEFINE_string(customize_dtypes, "", "Optional; Set customize dtypes path.");

DEFINE_string(op_debug_config, "", "Optional; switch for op debug config such as Operator memory detection");

DEFINE_string(save_original_model, "", "Optional; enable output original offline model. false(default)");

DEFINE_string(dynamic_batch_size, "",
              "Optional; If set, generate dynamic multi batch model. "
              "Different batch sizes are split by ','."
              "dynamic_batch_size, dynamic_image_size and dynamic_dims can only be set one.");

DEFINE_string(dynamic_image_size, "",
              "Optional; If set, generate dynamic multi image size model."
              "Different groups of image size are split by ';',"
              "while different dimensions of each group are split by ','."
              "dynamic_batch_size, dynamic_image_size and dynamic_dims can only be set one.");

DEFINE_string(dynamic_dims, "",
              "Optional; If set, generate dynamic input size model. "
              "Different groups of size are split by ';', while different dimensions of each group are split by ','."
              "dynamic_batch_size, dynamic_image_size and dynamic_dims can only be set one.");

DEFINE_string(enable_small_channel, "0", "Optional; If set to 1, small channel is enabled.");

DEFINE_string(enable_compress_weight, "false",
              "Optional; enable compress weight. true: enable; false(default): disable");

DEFINE_string(enable_attr_compression, "true",
              "Optional; Enable or disable attribute compression in model saving. "
              "Values: true (default, enabled) or false (disabled).");

DEFINE_string(compress_weight_conf, "", "Optional; the config file to compress weight.");

DEFINE_int32(sparsity, 0, "Optional; enable structured sparse. 0(default): disable; 1: enable");

DEFINE_string(enable_single_stream, "", "Optional; enable single stream. true: enable; false(default): disable");

DEFINE_string(ac_parallel_enable, "0",
              "Optional; enable engines such as Aicpu to parallel with other engines in dynamic shape graphs. "
              "1: enable; 0(default): disable");

DEFINE_string(tiling_schedule_optimize, "0",
              "Optional; enable tiling schedule optimize. 1: enable; 0(default): disable");

DEFINE_string(log, "null", "Optional; generate atc log. Support debug, info, warning, error, null.");

DEFINE_string(dump_mode, "0", "Optional; generate infershape json,only support 1, 0.");

DEFINE_int32(op_debug_level, 0,
             "Optional; configure debug level of compiler. 0(default): close debug; "
             "1: open TBE compiler, export ccec file and TBE instruction mapping file; 2: open ccec compiler; "
             "3: disable debug, and keep generating kernel file (.o and .json); 4: disable debug, "
             "keep generation kernel file (.o and .json) and generate the operator CCE file (.cce) "
             "and the UB fusion computing description file (.json)");
DEFINE_string(enable_scope_fusion_passes, "",
              "Optional; validate the non-general scope fusion pass,"
              "multiple names can be set and separated by ','.");
DEFINE_string(debug_dir, "", "Optional; the path to save the intermediate files of operator compilation.");

DEFINE_string(status_check, "0", "Optional; switch for status check such as overflow.");

DEFINE_string(op_compiler_cache_dir, "", "Optional; the path to cache operator compilation files.");

DEFINE_string(op_compiler_cache_mode, "disable", "Optional; choose the operator compiler cache mode.");

DEFINE_string(mdl_bank_path, "", "Optional; model bank path");

DEFINE_string(op_bank_path, "", "Optional; op bank path");

DEFINE_string(display_model_info, "0", "Optional; display model info");

DEFINE_string(device_id, "0", "Optional; user device id");

DEFINE_string(shape_generalized_build_mode, "shape_precise",
              "Optional; "
              "For selecting the mode of shape generalization when build graph. "
              "shape_generalized: Shape will be generalized during graph build. "
              "shape_precise: Shape will not be generalized, use precise shape. "
              "Default is shape_precise.");

DEFINE_string(atomic_clean_policy, "0",
              "Optional; "
              "For selecting the atomic op clean memory policy. "
              "0: centralized clean. "
              "1: separate clean. "
              "Default is 0.");

DEFINE_string(external_weight, "0",
              "Optional; "
              "For converting const to file constant, and saving weight to file. "
              "0: save weight in om. "
              "1: save weight in file. "
              "2: save all weights in one file. "
              "Default is 0.");

DEFINE_string(deterministic, "0",
              "Optional; "
              "For deterministic calculation"
              "0: deterministic off. "
              "1: deterministic on. "
              "Default is 0.");

DEFINE_string(deterministic_level, "0",
              "Optional; "
              "For deterministic and strong consistency calculation"
              "0: deterministic off. "
              "1: deterministic on. "
              "2: strong consistency on. "
              "Default is 0.");

DEFINE_string(host_env_os, "",
              "Optional;"
              "OS type of the target execution environment");

DEFINE_string(host_env_cpu, "",
              "Optional;"
              "CPU type of the target execution environment");

DEFINE_string(cluster_config, "",
              "Optional;"
              "target execute logic device info to generate hccl tasks.");

DEFINE_string(hccl_sub_comm_config, "",
              "Optional;"
              "HCCL sub-communication configuration file path.");

DEFINE_string(quant_dumpable, "",
              "Optional;"
              "Ensure that the input and output of quant nodes can be dumped. 1: enable; 0(default): disable.");

DEFINE_string(
    is_weight_clip, "1",
    "Optional;"
    "Ensure weight is finite by clipped when its datatype is floating-point data. 1(default): enable; 0: disable.");

DEFINE_string(oo_level, "O3", "Optional; The optimization level of the graph optimizer");
DEFINE_string(build_config, "",
              "Optional; make command template for generated source compilation. "
              "Format: \"make -s -j16 CXX=/usr/bin/clang++\".");
