/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_TASK_CODE_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_TASK_CODE_BUILDER_H_

#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/ast/ast_nodes.h"
#include "common/om2/codegen/om2_model_class_generator_base.h"
#include "common/om2/codegen/om2_codegen_types.h"
#include "graph/op_desc.h"
#include "proto/task.pb.h"

namespace ge {
struct IoAddrRefreshRecord {
  uint64_t compile_state_io_addr_offset{0U};
  uint64_t host_offset{0U};
};

struct TaskSemanticContributeContext {
  ModelTaskType task_type;
  const domi::TaskDef &task_def;
  int64_t op_index{kInvalidOpIndex};
  OpDescPtr op_desc;
  const RuntimeResourceSemantic *runtime{nullptr};
  ModelIoSemantic *model_io{nullptr};
  const std::unordered_map<std::string, uint32_t> *func_handle_indices{nullptr};
  const std::unordered_map<int64_t, std::string> *weight_offset_to_varname{nullptr};
  const std::unordered_map<int64_t, std::string> *fileconst_output_offset_to_varname{nullptr};
  std::unordered_map<int64_t, OpInputEdges> *op_id_to_input_edges{nullptr};
  std::unordered_map<uint32_t, uint32_t> *op_index_to_count_map{nullptr};
  uint64_t *next_args_table_index{nullptr};
  uint64_t *next_host_args_offset{nullptr};
  uint32_t *aicpu_task_count{nullptr};
};

class TaskCodeBuilder : public Om2ModelClassGeneratorBase {
 public:
  explicit TaskCodeBuilder(AstBuildContext &ast) : Om2ModelClassGeneratorBase(ast) {}
  ~TaskCodeBuilder() override = default;

  virtual int64_t ParseOpIndex(const domi::TaskDef &task_def) {
    (void)task_def;
    return -1L;
  }

  virtual Status Contribute(TaskSemanticContributeContext &context) {
    FillTaskSemanticHeader(context, header_);
    return SUCCESS;
  }

  virtual Status RenderInitResource(std::vector<BodyItem> &items) {
    (void)items;
    return SUCCESS;
  }

  virtual Status RenderDistribution(std::vector<BodyItem> &items) = 0;

  virtual Status RenderDistHelper(std::vector<DeclNode *> &items) = 0;

  const ArgsTableEntrySemantic *GetArgsTableEntry() const {
    return args_table_entry_;
  }

  const std::vector<IoAddrRefreshRecord> &GetIoAddrRefreshRecords() const {
    return io_addr_refresh_records_;
  }

  ModelTaskType GetTaskType() const {
    return header_.task_type;
  }

 protected:
  static void FillTaskSemanticHeader(const TaskSemanticContributeContext &context, TaskSemanticHeader &header) {
    header.task_type = context.task_type;
    header.op_index = context.op_index;
    header.stream_id = context.task_def.stream_id();
    if (context.op_desc != nullptr) {
      header.op_name = context.op_desc->GetName();
      header.op_type = context.op_desc->GetType();
    }
  }

  TaskSemanticHeader header_;
  const ArgsTableEntrySemantic *args_table_entry_{nullptr};
  std::vector<IoAddrRefreshRecord> io_addr_refresh_records_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_TASK_CODE_BUILDER_H_
