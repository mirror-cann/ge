/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_FE_DSA_TASK_CODE_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_FE_DSA_TASK_CODE_BUILDER_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"

namespace ge {

struct DsaBuildData {
  std::vector<OpArgDesc> ordered_args;
  int64_t op_desc_id{0};
  uint32_t sqe_type{0U}, sqe_size{0U}, stream_id{0U};
  uint8_t dump_flag{0U};
  uint32_t start{0U}, distribution_type{0U}, data_type{0U}, alg_type{0U};
  uint32_t param_vld_bitmap{0U}, param_addr_val_bitmap{0U};
  uint64_t seed_value{0U};
  bool seed_is_addr{false};
  uint64_t random_count_value{0U};
  bool random_count_is_addr{false};
  uint64_t input1_value{0U};
  bool input1_is_addr{false};
  uint64_t input2_value{0U};
  bool input2_is_addr{false};
  uint32_t hbm_table_index{0U}, hbm_args_size{0U};
  uint32_t idx_output{0U}, state_addr_idx{0U}, idx_seed{0U}, idx_count{0U};
  uint32_t idx_input1{0U}, idx_input2{0U};
  uint32_t num_iov_entries{0U};
  bool state_from_workspace{false};
  bool has_input2{true};
  uint32_t task_type{0U};
  std::vector<uint8_t> sqe_raw_data;
};

class DSATaskCodeBuilder : public TaskCodeBuilder {
  static constexpr const char *kDispatchFuncName = "DispatchDsa";
  static constexpr OpDispatchType::Value kDispatchType = OpDispatchType::DISPATCH_DSA;

 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;
  std::string GetFuncName() const override;
  Status RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) override;

 private:
  DsaBuildData build_data_;
  Status InitSqe(const domi::DSATaskDef &dsa_task);
  Status InitHbmArgsTable(TaskSemanticContributeContext &context);
  void InitBuildDataFields(uint32_t task_type);
  FunctionDef *RenderKernelDsaTaskDistribute() const;
  Status RenderDispatchFunc(std::vector<DeclNode *> &items);
  Status RenderDispatchFuncSetup(std::vector<BodyItem> &body, const VarRef &ctx, const ExprRef &dsa_data,
                                 const VarRef &addrs);
  Status RenderSqeScalars(std::vector<BodyItem> &body, const ExprRef &dsa_data, const VarRef &dsa_sqe);
  Status RenderSqeAddrFields(std::vector<BodyItem> &body, const ExprRef &dsa_data, const VarRef &ctx,
                             const VarRef &dsa_sqe, const VarRef &addrs);
  Status RenderHbmIoArgs(std::vector<BodyItem> &body, const ExprRef &dsa_data, const VarRef &ctx, const VarRef &addrs);
  Status RenderDispatchFuncLaunch(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                                  const ExprRef &dsa_data, const VarRef &sqe);
  Status RenderDispatchFuncReport(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                                  const ExprRef &dsa_data, const VarRef &addrs);
  Status RenderDispatchFuncReportIo(std::vector<BodyItem> &body, const ExprRef &dsa_data, const VarRef &addrs,
                                    const VarRef &dsa_io_tensors, const VarRef &dsa_report_inputs,
                                    const VarRef &dsa_report_outputs, const VarRef &dsa_report_ws_addrs,
                                    const VarRef &dsa_report_ws_sizes);
  Status RenderDispatchFuncReportSubmit(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                                        const ExprRef &dsa_data, const VarRef &dsa_report_inputs,
                                        const VarRef &dsa_report_outputs, const VarRef &dsa_report_ws_addrs,
                                        const VarRef &dsa_report_ws_sizes);

  // Address semantics
  std::vector<AddrSemantic> input_addrs_;
  std::vector<AddrSemantic> output_addrs_;
  std::vector<AddrSemantic> workspace_addrs_;

  // HBM args table entry (for IO refresh)
  std::optional<ArgsTableEntrySemantic> hbm_entry_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_FE_DSA_TASK_CODE_BUILDER_H_
