/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_OM2_CODEGEN_MODEL_BUILDER_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_OM2_CODEGEN_MODEL_BUILDER_H_

#include <map>
#include <unordered_map>

#include "common/model/ge_model.h"
#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_codegen_types.h"

namespace ge {
struct InputModelIoItem {
  uint32_t index{0U};
  int64_t memory_offset{0};
  size_t visit_order{0U};
};

struct OutputModelIoItem {
  uint32_t index{0U};
  int64_t memory_offset{0};
  int64_t addr_offset{0};
  bool is_addr_refreshable{true};
};

class Om2CodegenModelBuilder {
 public:
  static Status CreateTaskCodeBuilders(const GeModelPtr &model, AstBuildContext &ast,
                                       std::vector<TaskCodeBuilderPtr> &task_builders, Om2CodegenModel &codegen_model);
  Status Build(const GeModelPtr &model, const std::vector<TaskCodeBuilderPtr> &task_builders,
               Om2CodegenModel &codegen_model, Om2ConstMetas &const_metas);

 private:
  Status BuildOpDescLookup(const GeModelPtr &model);
  Status BuildOpInputEdges(const GeModelPtr &model);
  Status BuildModelInfo(const GeModelPtr &model, Om2CodegenModel &codegen_model) const;
  Status BuildRuntimeResource(const GeModelPtr &model, Om2CodegenModel &codegen_model) const;
  Status BuildModelIo(const GeModelPtr &model, Om2CodegenModel &codegen_model) const;
  Status CollectModelIoItems(Om2CodegenModel &codegen_model, const ComputeGraphPtr &compute_graph,
                             std::vector<InputModelIoItem> &input_items,
                             std::vector<OutputModelIoItem> &output_items) const;
  Status CollectNetOutputIoItems(const Node &node, const OpDescPtr &op_desc,
                                  uint32_t &next_model_output_index,
                                  std::vector<OutputModelIoItem> &output_items,
                                  std::set<int64_t> &io_offsets) const;
  Status BuildConstInputs(const GeModelPtr &model, const std::vector<TaskCodeBuilderPtr> &task_builders,
                          Om2CodegenModel &codegen_model, Om2ConstMetas &const_metas);
  Status BuildFileConstInputs(const GeModelPtr &model, Om2CodegenModel &codegen_model, Om2ConstMetas &const_metas);
  Status BuildKernelRegistry(const GeModelPtr &model, const std::vector<TaskCodeBuilderPtr> &task_builders,
                             Om2CodegenModel &codegen_model);
  Status BuildTaskSemantics(const GeModelPtr &model, const std::vector<TaskCodeBuilderPtr> &task_builders,
                            Om2CodegenModel &codegen_model);
  Status AggregateArgsTable(const std::vector<TaskCodeBuilderPtr> &task_builders, Om2CodegenModel &codegen_model) const;
  Status BuildHostArgsOffsets(const std::multimap<uint64_t, uint64_t> &io_addr_offset_map,
                              Om2CodegenModel &codegen_model) const;
  Status CollectConstInputsFromOp(const OpDescPtr &op_desc, Om2CodegenModel &codegen_model, Om2ConstMetas &const_metas);
  Status UpdateStreamFlag(const GeModelPtr &model, Om2CodegenModel &codegen_model) const;
  Status InitStreamActive(const OpDescPtr &op_desc, std::set<uint32_t> &active_stream_indication) const;
  Status InitStreamSwitch(const OpDescPtr &op_desc, std::set<uint32_t> &active_stream_indication) const;
  std::vector<MemInfo> GetAllMemoryTypeSize(const GeModelPtr &model) const;
  static void ReportUnsupportedTask(TaskCodeBuilderPtr &task_builder, domi::TaskDef *const task_def,
                                    std::unordered_map<int64_t, OpDescPtr> &op_desc_by_index, ModelTaskType task_type);
  static Status BuildKernelRegistryForAicore(Om2CodegenModel &codegen_model, const OpDescPtr &op_desc,
                                             ModelTaskType task_type);
  static Status BuildKernelRegistryForAicpu(Om2CodegenModel &codegen_model, const domi::TaskDef &task_def,
                                            const std::string &op_type, const std::string &kernel_name,
                                            const std::string &aicpu_kernel_sign);
  static Status BuildKernelRegistryForCustAicpu(Om2CodegenModel &codegen_model, const OpDescPtr &op_desc,
                                                const std::string &op_type, const std::string &kernel_name,
                                                const std::string &kernel_sign);
  static Status BuildKernelRegistryForTFAicpu(Om2CodegenModel &codegen_model, const std::string &op_type,
                                              const std::string &tf_aicpu_kernel_sign);
  static Status BuildKernelRegistryForTFAicpuSession(Om2CodegenModel &codegen_model, const std::string &op_type,
                                                     const std::string &tf_aicpu_kernel_sign);
  std::unordered_map<int64_t, OpDescPtr> op_desc_by_index_;
  std::unordered_map<int64_t, OpInputEdges> op_id_to_input_edges_;
  std::unordered_map<int64_t, std::string> weight_offset_to_varname_;
  std::unordered_map<int64_t, std::string> fileconst_output_offset_to_varname_;
  std::unordered_map<uint32_t, uint32_t> op_index_to_count_map_;
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_OM2_CODEGEN_MODEL_BUILDER_H_
