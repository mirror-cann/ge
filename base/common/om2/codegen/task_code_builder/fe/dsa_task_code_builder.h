/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_FE_DSA_TASK_CODE_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_FE_DSA_TASK_CODE_GENERATOR_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"

namespace ge {

struct DsaSqeSemantic {
  // SQE scalar fields (filled in Contribute from DSATaskDef, aligns with v1 InitSqe)
  uint8_t sqe_type{0U};
  uint8_t start{0U};
  uint8_t distribution_type{0U};
  uint8_t data_type{0U};
  uint8_t alg_type{0U};
  uint8_t param_vld_bitmap{0U};
  uint8_t param_addr_val_bitmap{0U};

  // Seed: addr mode uses input_addrs_[1], immediate mode stores seed_value
  uint64_t seed_value{0U};
  bool seed_is_addr{false};
  // Random count: addr mode uses input_addrs_[0], immediate mode stores random_count_value
  uint64_t random_count_value{0U};
  bool random_count_is_addr{false};
  // Input1 for HBM workspace: addr mode uses input_addrs_[2], immediate mode stores input1_value
  uint64_t input1_value{0U};
  bool input1_is_addr{false};
  // Input2 for HBM workspace: addr mode uses input_addrs_[3], immediate mode stores input2_value
  uint64_t input2_value{0U};
  bool input2_is_addr{false};
};

class DSATaskCodeBuilder : public TaskCodeBuilder {
 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderDistribution(std::vector<BodyItem> &items) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;

 private:
  Status InitSqe(const domi::DSATaskDef &dsa_task);
  Status InitHbmArgsTable(TaskSemanticContributeContext &context);
  void RenderSqeAddrFields(const VarRef &sqe_var, std::vector<BodyItem> &items);
  void RenderSqeVarFields(const VarRef &sqe_var, std::vector<BodyItem> &items);
  void RenderHbmArgsCopy(const VarRef &sqe_var, std::vector<BodyItem> &items);
  void BuildIoArgsVars(std::vector<Arg> &io_args_vars, std::vector<BodyItem> &items);
  void RenderAddrLowHigh(const ExprRef &sqe_attr_low, const ExprRef &sqe_attr_high,
                         const std::string &addr_expr, std::vector<BodyItem> &items);

  DsaSqeSemantic dsa_sqe_;

  // Address semantics
  std::vector<AddrSemantic> input_addrs_;
  std::vector<AddrSemantic> output_addrs_;
  std::vector<AddrSemantic> workspace_addrs_;

  // HBM args table entry (for IO refresh)
  std::optional<ArgsTableEntrySemantic> hbm_entry_;
  uint64_t hbm_args_len_{0U};
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_FE_DSA_TASK_CODE_GENERATOR_H_
