/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_MEMCPY_ADDR_ASYNC_TASK_CODE_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_MEMCPY_ADDR_ASYNC_TASK_CODE_BUILDER_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "graph/args_format_desc.h"
#include "graph/utils/args_format_desc_utils.h"

namespace ge {
struct MemcpyAddrBuildData {
  std::vector<OpArgDesc> ordered_args;       // IO 地址条目（不含 CUSTOM_VALUE）
  std::vector<OpArgDesc> custom_value_args;  // CUSTOM_VALUE 写回条目
  uint64_t dst_max{0U}, count{0U};
  uint32_t kind{0U}, align_offset{0U}, args_size{0U}, stream_id{0U};
  uint32_t args_table_idx{0U}, aligned_io_offset{0U};
};

class MemcpyAddrAsyncTaskCodeBuilder : public TaskCodeBuilder {
  static constexpr const char *kDispatchFuncName = "DispatchMemcpyAddrAsync";
  static constexpr OpDispatchType::Value kDispatchType = OpDispatchType::DISPATCH_MEMCPY_ADDR_ASYNC;

 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  std::string GetFuncName() const override;
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;
  Status RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) override;

 private:
  MemcpyAddrBuildData build_data_;
  void AppendOrderedArgValue(const AddrSemantic &semantic, uint64_t current_host_offset);
  void ResolveInternalIndex(TaskSemanticContributeContext &context);
  Status CalcArgSizes(const TaskSemanticContributeContext &context);
  Status BuildOrderedArgs(TaskSemanticContributeContext &context, const AddrSemantic &src_addr_node,
                          const AddrSemantic &dst_addr_node);
  void PopulateBuildData();
  Status RenderKernelDistributeFunc(std::vector<DeclNode *> &items);
  Status RenderDispatchFunc(std::vector<DeclNode *> &items);
  std::vector<BodyItem> RenderIoAddrResolveLoop(const VarRef &ctx, const ExprRef &memcpy_addr);
  Status RenderCustomValueWriteback(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                                    const ExprRef &args_table_idx);

  std::vector<AddrSemantic> ordered_arg_values_;
  std::vector<ArgDesc> arg_descs_;
  std::vector<size_t> arg_sizes_;
  uint32_t internal_index_{0U};

  std::string args_format_str_;

  ArgsTableEntrySemantic entry_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_RTS_MEMCPY_ADDR_ASYNC_TASK_CODE_BUILDER_H_
