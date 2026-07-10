/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_CMO_ADDR_TASK_CODE_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_CMO_ADDR_TASK_CODE_BUILDER_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "graph/args_format_desc.h"

namespace ge {
struct CmoAddrBuildData {
  std::vector<OpArgDesc> ordered_args;       // IO 地址条目（不含 CUSTOM_VALUE）
  std::vector<OpArgDesc> custom_value_args;  // CUSTOM_VALUE 写回条目
  uint32_t args_size{0U}, align_offset{0U}, cmo_op_code{0U};
  uint32_t stream_id{0U}, args_table_idx{0U};
  uint32_t io_offset{0U}, args_info_num{0U};
};

class CmoAddrTaskCodeBuilder : public TaskCodeBuilder {
  static constexpr const char *kDispatchFuncName = "DispatchCmoAddr";
  static constexpr OpDispatchType::Value kDispatchType = OpDispatchType::DISPATCH_CMO_ADDR;

 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  std::string GetFuncName() const override;
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;
  Status RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) override;

 private:
  CmoAddrBuildData build_data_;
  Status BuildOrderedArgs(TaskSemanticContributeContext &context, const AddrSemantic &src_addr_node);
  void AppendOrderedArgValue(const AddrSemantic &semantic, uint64_t current_host_offset);
  std::string BuildAutoArgsFormat(const TaskSemanticContributeContext &context) const;

  // RenderDistHelper sub-functions
  Status RenderKernelDistributeFunc(std::vector<DeclNode *> &items);
  Status RenderDispatchFunc(std::vector<DeclNode *> &items);
  Status RenderKernelLaunch(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                            const ExprRef &dev_addr_off, const ExprRef &host_addr_off);
  Status RenderArgsWriteback(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx, const VarRef &iow_addr,
                             const ExprRef &args_table_idx);
  void RenderCustomValueWriteback(std::vector<BodyItem> &body, const VarRef &ctx, const ExprRef &args_table_idx);

  std::string args_format_str_;
  std::vector<ArgDesc> arg_descs_;
  std::vector<AddrSemantic> ordered_arg_values_;

  std::vector<size_t> arg_sizes_;
  std::optional<ArgsTableEntrySemantic> entry_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_CMO_ADDR_TASK_CODE_BUILDER_H_
