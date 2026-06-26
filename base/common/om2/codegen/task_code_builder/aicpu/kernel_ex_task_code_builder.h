/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_AICPU_KERNEL_EX_TASK_CODE_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_AICPU_KERNEL_EX_TASK_CODE_BUILDER_H_

#include "common/om2/codegen/om2_aicpu_ext_info_handler.h"
#include "common/om2/codegen/task_code_builder/task_code_builder.h"

namespace ge {
class KernelExTaskCodeBuilder : public TaskCodeBuilder {
 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  Status RenderDistribution(std::vector<BodyItem> &items) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  Status Contribute(TaskSemanticContributeContext &context) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;

 private:
  static std::string SerializeBytesToOctalString(const std::vector<uint8_t> &buffer);
  Status InitIowAddrRefreshInfo(uint64_t current_offset);
  Status InitLaunchInfo(const TaskSemanticContributeContext &context);
  Status InitTaskExtInfo(const TaskSemanticContributeContext &context);
  Status InitTaskExInfo(const TaskSemanticContributeContext &context);
  Status InitArgsTableInfo(const TaskSemanticContributeContext &context);
  Status RenderGetAddrInfo(std::vector<BodyItem> &items, std::vector<Arg> &flatten_args_vars) const;
  Status AppendOm2TensorAddrInfo(const AddrSemantic &addr, const char *addr_type, std::vector<BodyItem> &items,
                                 std::vector<Arg> &flatten_args_vars) const;
  Expr *BuildLaunchConfigExpr(const LaunchConfigSemantic &launch_config, Arg is_data_dump = {}) const;
  VarRef AppendLaunchConfigSetup(size_t op_index, std::vector<BodyItem> &items, Arg is_data_dump = {}) const;
  FunctionDef *BuildAssembleTfAicpuArgs() const;
  FunctionDef *BuildTfAicpuKernelTaskDistribute() const;
  FunctionDef *BuildAssembleTfAicpuExSessionIdInfo() const;
  FunctionDef *BuildAssembleTfAicpuExKernelIdInfo() const;
  FunctionDef *BuildAssembleTfAicpuExWorkSpaceAddrInfo() const;
  FunctionDef *BuildAssembleTfAicpuExInputOutputAddrInfo() const;
  FunctionDef *BuildAssembleTfAicpuExExtInfo() const;

  KernelTaskSemantic semantic_;
  rtMemType_t mem_type_{RT_MEMORY_HBM};
  int32_t deploy_type_flag_{0};
  aclrtMemcpyKind memcpy_kind_{ACL_MEMCPY_HOST_TO_DEVICE};
  std::string task_def_kernel_ex_args_;
  size_t task_def_kernel_ex_args_size_;
  std::string task_def_kernel_ex_task_info_;
  size_t task_def_kernel_ex_task_info_size_;
  std::string task_def_kernel_ex_ext_info_;
  size_t task_def_kernel_ex_ext_info_size_;
  size_t ext_info_len_;
  uint8_t *ext_info_;
  std::vector<uint8_t> ext_info_buffer_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_AICPU_KERNEL_EX_TASK_CODE_BUILDER_H_
