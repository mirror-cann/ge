/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_CMO_ADDR_TASK_CODE_GENERATOR_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_CMO_ADDR_TASK_CODE_GENERATOR_H_

#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "graph/args_format_desc.h"

namespace ge {
class CmoAddrTaskCodeBuilder : public TaskCodeBuilder {
 public:
  using TaskCodeBuilder::TaskCodeBuilder;
  Status Contribute(TaskSemanticContributeContext &context) override;
  Status RenderDistribution(std::vector<BodyItem> &items) override;
  Status RenderDistHelper(std::vector<DeclNode *> &items) override;
  int64_t ParseOpIndex(const domi::TaskDef &task_def) override;

 private:
  Status BuildOrderedArgs(TaskSemanticContributeContext &context, const AddrSemantic &src_addr_node);
  void AppendOrderedArgValue(const AddrSemantic &semantic, uint64_t current_host_offset);
  Status CollectIoAddrVars(std::vector<BodyItem> &items, std::vector<Arg> &args_vars);
  void RenderCustomValueWriteback(std::vector<BodyItem> &items);
  std::string BuildAutoArgsFormat(const TaskSemanticContributeContext &context);

  std::string args_format_str_;
  std::vector<ArgDesc> arg_descs_;
  std::vector<AddrSemantic> ordered_arg_values_;

  // + ------------ + --------- + ------- + --- +
  // | align_offset | io_offset | src | |64B |
  // + ------------ + --------- + ------- + --- +
  //                |<- args_size_    ->|
  // + ------------ + --------- + ------- + --- +
  // |<-          total_args_size_         ->|

  std::vector<size_t> arg_sizes_;
  size_t args_size_{0U};
  size_t align_offset_{0U};
  size_t io_offset_{0U};
  size_t total_args_size_{0U};
  uint32_t cmo_op_code_{0U};
  std::optional<ArgsTableEntrySemantic> entry_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_GENERATOR_RTS_CMO_ADDR_TASK_CODE_GENERATOR_H_
