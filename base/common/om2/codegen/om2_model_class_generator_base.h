/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_OM2_MODEL_CLASS_GENERATOR_BASE_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_OM2_MODEL_CLASS_GENERATOR_BASE_H_

#include "common/om2/codegen/code_generator_base.h"

namespace ge {
class Om2ModelClassGeneratorBase : public CodeGeneratorBase {
 public:
  explicit Om2ModelClassGeneratorBase(AstBuildContext &ast);
  ~Om2ModelClassGeneratorBase() override = default;

 protected:
  VarRef constants_;
  VarRef model_handle_;
  VarRef bin_handles_;
  VarRef func_handles_;
  VarRef stream_list_;
  VarRef notify_list_;
  VarRef event_list_;
  VarRef label_list_;
  VarRef aclrt_label_list_;
  VarRef total_dev_mem_ptr_;
  VarRef is_stream_list_bind_;
  VarRef bin_info_map_;
  VarRef args_table_;
  VarRef session_id_;
  VarRef kernel_id_;
  VarRef dev_ext_info_mem_ptrs_;
  VarRef label_switch_label_list_;
  VarRef label_goto_ex_index_values_;
  VarRef label_goto_args_;
  VarRef label_goto_ex_label_list_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_OM2_MODEL_CLASS_GENERATOR_BASE_H_
