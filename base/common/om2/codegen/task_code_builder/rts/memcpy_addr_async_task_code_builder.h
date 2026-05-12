/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_MEMCPY_ADDR_ASYNC_TASK_CODE_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_MEMCPY_ADDR_ASYNC_TASK_CODE_GENERATOR_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "graph/args_format_desc.h"
#include "graph/utils/args_format_desc_utils.h"

namespace ge {
class MemcpyAddrAsyncTaskCodeBuilder : public TaskCodeBuilder {
 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderDistribution(std::vector<BodyItem> &items) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;

 private:
  void AppendOrderedArgValue(const AddrSemantic &semantic, uint64_t current_host_offset);
  void ResolveInternalIndex(TaskSemanticContributeContext &context);
  Status CalcArgSizes(const TaskSemanticContributeContext &context);
  Status BuildOrderedArgs(TaskSemanticContributeContext &context,
                          const AddrSemantic &src_addr_node, const AddrSemantic &dst_addr_node);
  void CollectIoAddrVars(std::vector<BodyItem> &items, std::vector<Arg> &args_vars);
  void RenderCustomValueWriteback(std::vector<BodyItem> &items);

  std::vector<AddrSemantic> ordered_arg_values_;
  std::vector<ArgDesc> arg_descs_;
  std::vector<size_t> arg_sizes_;
  uint64_t dst_max_{0U};
  uint64_t count_{0U};
  uint32_t kind_{0U};
  uint32_t internal_index_{0U};
  size_t aligned_io_offset_{0U};

  // args_format 相关
  std::string args_format_str_;
  size_t args_size_{0UL};
  size_t align_offset_{0UL};  // 64字节对齐偏移

  // args_table（memcpy_addr_async 总是需要）
  ArgsTableEntrySemantic entry_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_MEMCPY_ADDR_ASYNC_TASK_CODE_GENERATOR_H_
