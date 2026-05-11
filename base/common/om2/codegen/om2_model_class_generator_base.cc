/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/om2_model_class_generator_base.h"

namespace ge {
Om2ModelClassGeneratorBase::Om2ModelClassGeneratorBase(AstBuildContext &ast)
    : CodeGeneratorBase(ast),
      constants_(ast.Var("void **", "constants_")),
      model_handle_(ast.Var("aclmdlRI", "model_handle_")),
      bin_handles_(ast.Var("std::vector<aclrtBinHandle>", "bin_handles_")),
      func_handles_(ast.Var("std::vector<aclrtFuncHandle>", "func_handles_")),
      stream_list_(ast.Var("std::vector<aclrtStream>", "stream_list_")),
      notify_list_(ast.Var("std::vector<aclrtNotify>", "notify_list_")),
      event_list_(ast.Var("std::vector<aclrtEvent>", "event_list_")),
      label_list_(ast.Var("std::vector<aclrtLabel>", "label_list_")),
      aclrt_label_list_(ast.Var("aclrtLabelList", "aclrt_label_list_")),
      total_dev_mem_ptr_(ast.Var("void *", "total_dev_mem_ptr_")),
      is_stream_list_bind_(ast.Var("bool", "is_stream_list_bind_")),
      bin_info_map_(ast.Var("std::unordered_map<std::string, BinDataInfo>", "bin_info_map_")),
      args_table_(ast.Var("Om2ArgsTable", "args_table_")),
      session_id_(ast.Var("uint64_t *", "session_id_")),
      kernel_id_(ast.Var("uint64_t", "kernel_id_")),
      dev_ext_info_mem_ptrs_(ast.Var("std::vector<void *>", "dev_ext_info_mem_ptrs_")),
      label_switch_label_list_(ast_.Var("std::map<uint32_t, aclrtLabelList>", "label_switch_label_list_")),
      label_goto_ex_index_values_(ast_.Var("std::vector<void *>", "label_goto_ex_index_values_")),
      label_goto_args_(ast_.Var("std::map<uint32_t, std::pair<void *, uint32_t>>", "label_goto_args_")),
      label_goto_ex_label_list_(ast_.Var("std::map<uint32_t, aclrtLabelList>", "label_goto_ex_label_list_")),
      mem_event_id_mem_map_(ast_.Var("std::map<uint32_t, void *>", "mem_event_id_mem_map_")),
      overflow_addr_(ast_.Var("void *", "overflow_addr_")),
      dev_dynamic_mem_ptrs_(ast_.Var("std::vector<void *>", "dev_dynamic_mem_ptrs_")),
      session_scope_mem_ptr_(ast_.Var("void *", "session_scope_mem_ptr_")) {}
}  // namespace ge
