/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/om2_codegen_model_builder.h"

#include <map>
#include <unordered_map>

#include "common/file_constant_utils/file_constant_utils.h"
#include "common/ge_common/string_util.h"
#include "common/om2/codegen/om2_codegen_utils.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "graph/utils/op_type_utils.h"
#include "graph/utils/tensor_utils.h"
#include "framework/common/framework_types_internal.h"
#include "graph/op_kernel_bin.h"
#include "common/op_tiling/op_tiling_rt2.h"

namespace ge {
namespace {
constexpr uint32_t kTrueBranchStreamCount = 1U;
const std::string kTfSessionTask = "TfSessionTask";

bool CompareInputModelIoItem(const InputModelIoItem &lhs, const InputModelIoItem &rhs) {
  if (lhs.index != rhs.index) {
    return lhs.index < rhs.index;
  }
  return lhs.visit_order < rhs.visit_order;
}

}  // namespace

Status Om2CodegenModelBuilder::BuildHostArgsOffsets(const std::multimap<uint64_t, uint64_t> &io_addr_offset_map,
                                                    Om2CodegenModel &codegen_model) const {
  auto &host_args_offsets = codegen_model.args_table.host_args_offsets;
  host_args_offsets.reserve(codegen_model.model_io.entries.size());
  for (const auto &entry : codegen_model.model_io.entries) {
    const auto io_addr_offset = static_cast<uint64_t>(entry.memory_offset);
    std::vector<uint64_t> host_offsets;
    const auto ranges = io_addr_offset_map.equal_range(io_addr_offset);
    for (auto it = ranges.first; it != ranges.second; ++it) {
      host_offsets.push_back(it->second);
    }
    std::sort(host_offsets.begin(), host_offsets.end());
    host_args_offsets.push_back(std::move(host_offsets));
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::CollectConstInputsFromOp(const OpDescPtr &op_desc, Om2CodegenModel &codegen_model,
                                                        Om2ConstMetas &const_metas) {
  GE_ASSERT_NOTNULL(op_desc);
  const vector_bit_t &v_is_input_const = op_desc->GetIsInputConst();
  for (size_t input_idx = 0U; input_idx < op_desc->GetAllInputsSize(); ++input_idx) {
    if ((input_idx >= v_is_input_const.size()) || !v_is_input_const[input_idx]) {
      continue;
    }
    const auto tensor_desc = op_desc->MutableInputDesc(static_cast<uint32_t>(input_idx));
    if (tensor_desc == nullptr) {
      continue;
    }
    int64_t data_offset = 0;
    GE_CHK_STATUS(TensorUtils::GetDataOffset(*tensor_desc, data_offset));
    if (weight_offset_to_varname_.find(data_offset) != weight_offset_to_varname_.end()) {
      continue;
    }
    int64_t tensor_size = 0;
    GE_CHK_STATUS(TensorUtils::GetTensorSizeInBytes(*tensor_desc, tensor_size));
    const size_t const_index = const_metas.size();
    const std::string var_name = "const_" + std::to_string(const_index);
    ConstInputEntry entry;
    entry.const_index = const_index;
    entry.var_name = var_name;
    GE_ASSERT_SUCCESS(Om2ModelUtils::BuildInputTensorInfo(tensor_desc, entry.tensor_info));
    codegen_model.const_inputs.push_back(std::move(entry));
    weight_offset_to_varname_.emplace(data_offset, var_name);
    const_metas.push_back(Om2ConstMeta{const_index,
                                       "INTERNAL",
                                       "",
                                       "",
                                       data_offset,
                                       tensor_size,
                                       ""});
  }
  return SUCCESS;
}

void Om2CodegenModelBuilder::ReportUnsupportedTask(TaskCodeBuilderPtr &task_builder,  domi::TaskDef *const task_def,
                                               std::unordered_map<int64_t, OpDescPtr> &op_desc_by_index,
                                               const ModelTaskType &task_type) {
  const auto op_index =
      (task_builder == nullptr) ? kInvalidOpIndex : task_builder->ParseOpIndex(*task_def);
  if (task_builder == nullptr || op_index == kInvalidOpIndex) {
    REPORT_INNER_ERR_MSG("E19999", "Unsupported task type %d", static_cast<int32_t>(task_type));
    GELOGE(FAILED, "[OM2] Unsupported task type %d, task def %s", static_cast<int32_t>(task_type),
            task_def->ShortDebugString().c_str());
  } else {
    const auto op_desc_it = op_desc_by_index.find(op_index);
    if (op_desc_it == op_desc_by_index.end() || op_desc_it->second == nullptr) {
      REPORT_INNER_ERR_MSG("E19999", "Unsupported task type %d, op_index=%" PRId64,
                           static_cast<int32_t>(task_type), op_index);
      GELOGE(FAILED, "[OM2] Unsupported task type %d, op_index=%" PRId64 ", task def %s",
             static_cast<int32_t>(task_type), op_index, task_def->ShortDebugString().c_str());
    } else {
      const auto &op_desc = op_desc_it->second;
      REPORT_INNER_ERR_MSG("E19999", "Unsupported task type %d for op %s, op_index=%" PRId64,
                            static_cast<int32_t>(task_type), op_desc->GetName().c_str(), op_index);
      GELOGE(FAILED, "[OM2] Unsupported task type %d for op %s, op type %s, op_index=%" PRId64 ", task def %s",
             static_cast<int32_t>(task_type), op_desc->GetName().c_str(), op_desc->GetTypePtr(), op_index,
             task_def->ShortDebugString().c_str());
    }
  }
}

Status Om2CodegenModelBuilder::CreateTaskCodeBuilders(const GeModelPtr &model, AstBuildContext &ast,
                                                      std::vector<TaskCodeBuilderPtr> &task_builders,
                                                      Om2CodegenModel &codegen_model) {
  GE_ASSERT_NOTNULL(model);
  const auto &model_task_def = model->GetModelTaskDefPtr();
  GE_ASSERT_NOTNULL(model_task_def);
  const auto compute_graph = model->GetGraph();
  GE_ASSERT_NOTNULL(compute_graph);
  std::unordered_map<int64_t, OpDescPtr> op_desc_by_index;
  for (const auto &node : compute_graph->GetAllNodes()) {
    GE_ASSERT_NOTNULL(node);
    const auto &op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    op_desc_by_index.emplace(op_desc->GetId(), op_desc);
  }

  const size_t task_size = static_cast<size_t>(model_task_def->task_size());
  task_builders.resize(task_size);
  for (size_t i = 0UL; i < task_size; ++i) {
    domi::TaskDef *const task_def = model_task_def->mutable_task(static_cast<int32_t>(i));
    GE_ASSERT_NOTNULL(task_def);
    const auto task_type = static_cast<ModelTaskType>(task_def->type());
    auto &task_builder = task_builders.at(i);
    task_builder = TaskCodeBuilderFactory::Instance().Create(task_type, ast);
    if (!Om2CodegenUtils::IsSupportedTask(task_type)) {
      ReportUnsupportedTask(task_builder, task_def, op_desc_by_index, task_type);
      return FAILED;
    }
    GE_ASSERT_NOTNULL(task_builder, "Failed to create task code builder from type %d, task index %zu",
                      task_def->type(), i);
    if (task_type == ModelTaskType::MODEL_TASK_STREAM_LABEL_SWITCH_BY_INDEX) {
      codegen_model.runtime.has_label_switch = true;
    }
    if (task_type == ModelTaskType::MODEL_TASK_STREAM_LABEL_GOTO) {
      codegen_model.runtime.has_label_goto = true;
    }
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::Build(const GeModelPtr &model, const std::vector<TaskCodeBuilderPtr> &task_builders,
                                     Om2CodegenModel &codegen_model, Om2ConstMetas &const_metas) {
  op_desc_by_index_.clear();
  op_id_to_input_edges_.clear();
  weight_offset_to_varname_.clear();
  fileconst_output_offset_to_varname_.clear();
  op_index_to_count_map_.clear();
  const_metas.clear();
  GE_ASSERT_SUCCESS(BuildOpDescLookup(model));
  GE_ASSERT_SUCCESS(BuildOpInputEdges(model));
  GE_ASSERT_SUCCESS(BuildModelInfo(model, codegen_model));
  GE_ASSERT_SUCCESS(BuildRuntimeResource(model, codegen_model));
  GE_ASSERT_SUCCESS(BuildModelIo(model, codegen_model));
  GE_ASSERT_SUCCESS(BuildFileConstInputs(model, codegen_model, const_metas));
  GE_ASSERT_SUCCESS(BuildConstInputs(model, task_builders, codegen_model, const_metas));
  GE_ASSERT_SUCCESS(BuildKernelRegistry(model, task_builders, codegen_model));
  GE_ASSERT_SUCCESS(BuildTaskSemantics(model, task_builders, codegen_model));
  GE_ASSERT_SUCCESS(AggregateArgsTable(task_builders, codegen_model));
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildOpDescLookup(const GeModelPtr &model) {
  GE_ASSERT_NOTNULL(model);
  const auto compute_graph = model->GetGraph();
  GE_ASSERT_NOTNULL(compute_graph);
  for (const auto &node : compute_graph->GetAllNodes()) {
    GE_ASSERT_NOTNULL(node);
    const auto &op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    op_desc_by_index_.emplace(op_desc->GetId(), op_desc);
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildOpInputEdges(const GeModelPtr &model) {
  GE_ASSERT_NOTNULL(model);
  const auto compute_graph = model->GetGraph();
  GE_ASSERT_NOTNULL(compute_graph);
  for (const auto &node : compute_graph->GetAllNodes()) {
    const auto &op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    const int64_t op_id = op_desc->GetId();
    OpInputEdges edges;
    const size_t all_inputs_size = op_desc->GetAllInputsSize();
    edges.input_op_ids.resize(all_inputs_size, kInvalidOpId);
    edges.input_anchor_indices.resize(all_inputs_size, kInvalidAnchorIndex);
    edges.output_var_names.resize(op_desc->GetOutputsSize());
    op_id_to_input_edges_[op_id] = edges;
  }
  for (const auto &node : compute_graph->GetAllNodes()) {
    const auto &op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    const int64_t op_id = op_desc->GetId();
    const auto &in_anchors = node->GetAllInDataAnchorsPtr();
    for (size_t i = 0U; i < in_anchors.size(); ++i) {
      InDataAnchor *in_anchor = in_anchors[i];
      if (in_anchor == nullptr) {
        continue;
      }
      const OutDataAnchorPtr peer_out_anchor = in_anchor->GetPeerOutAnchor();
      if (peer_out_anchor == nullptr) {
        continue;
      }
      Node *src_node = peer_out_anchor->GetOwnerNodeBarePtr();
      GE_ASSERT_NOTNULL(src_node);
      const auto &src_op_desc = src_node->GetOpDesc();
      if (src_op_desc == nullptr) {
        continue;
      }
      const int64_t src_op_id = src_op_desc->GetId();
      const int32_t src_anchor_idx = peer_out_anchor->GetIdx();
      auto it = op_id_to_input_edges_.find(op_id);
      GE_ASSERT_TRUE(it != op_id_to_input_edges_.end(), "[OM2] Op id %" PRId64 " not found in mapping", op_id);
      GE_ASSERT_TRUE(i < it->second.input_op_ids.size(),
                     "[OM2] Input index %zu out of range for op_id %" PRId64, i, op_id);
      it->second.input_op_ids[i] = src_op_id;
      it->second.input_anchor_indices[i] = src_anchor_idx;
    }
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildModelInfo(const GeModelPtr &model, Om2CodegenModel &codegen_model) const {
  GE_ASSERT_NOTNULL(model);
  codegen_model.model_name = model->GetName();
  return SUCCESS;
}

std::vector<MemInfo> Om2CodegenModelBuilder::GetAllMemoryTypeSize(const GeModelPtr &model) const {
  std::vector<MemInfo> all_mem_info;
  // hbm先不适配

  MemInfo p2p_mem_info{};
  (void)AttrUtils::GetInt(model, ATTR_MODEL_P2P_MEMORY_SIZE, p2p_mem_info.memory_size);
  p2p_mem_info.memory_type = RT_MEMORY_P2P_DDR;
  p2p_mem_info.memory_key = "_p";
  all_mem_info.emplace_back(std::move(p2p_mem_info));

  MemInfo session_scope_mem_info{};
  (void)AttrUtils::GetInt(model, ATTR_MODEL_SESSION_SCOPE_MEMORY_SIZE, session_scope_mem_info.memory_size);
  session_scope_mem_info.memory_type = (kSessionScopeMemoryMask | RT_MEMORY_HBM);
  all_mem_info.emplace_back(std::move(session_scope_mem_info));

  MemInfo host_mem_info{};
  (void)AttrUtils::GetInt(model, MODEL_ATTR_HOST_MEMORY_SIZE, host_mem_info.memory_size);
  (void)AttrUtils::GetInt(model, MODEL_ATTR_TASK_GEN_HOST_BASE_ADDR, host_mem_info.logic_memory_base);
  host_mem_info.memory_type = RT_MEMORY_HOST;
  host_mem_info.memory_key = "_h";
  all_mem_info.emplace_back(std::move(host_mem_info));

  MemInfo host_svm_mem_info{};
  (void)AttrUtils::GetInt(model, MODEL_ATTR_HOST_SVM_SIZE, host_svm_mem_info.memory_size);
  (void)AttrUtils::GetInt(model, MODEL_ATTR_TASK_GEN_HOST_SVM_BASE_ADDR, host_svm_mem_info.logic_memory_base);
  host_svm_mem_info.memory_type = RT_MEMORY_HOST_SVM;
  host_svm_mem_info.memory_key = "_svm";
  all_mem_info.emplace_back(std::move(host_svm_mem_info));
  return all_mem_info;
}

Status Om2CodegenModelBuilder::BuildRuntimeResource(const GeModelPtr &model, Om2CodegenModel &codegen_model) const {
  GE_ASSERT_NOTNULL(model);
  (void)AttrUtils::GetInt(model, ATTR_MODEL_MEMORY_SIZE, codegen_model.runtime.total_mem_size);
  (void)AttrUtils::GetInt(model, ATTR_MODEL_WEIGHT_SIZE, codegen_model.runtime.total_weight_size);
  (void)AttrUtils::GetInt(model, ATTR_MODEL_STREAM_NUM, codegen_model.runtime.stream_num);
  (void)AttrUtils::GetInt(model, ATTR_MODEL_NOTIFY_NUM, codegen_model.runtime.notify_num);
  (void)AttrUtils::GetInt(model, ATTR_MODEL_EVENT_NUM, codegen_model.runtime.event_num);
  (void)AttrUtils::GetInt(model, ATTR_MODEL_LABEL_NUM, codegen_model.runtime.label_num);
  (void)AttrUtils::GetInt(model, MODEL_ATTR_TASK_GEN_BASE_ADDR, codegen_model.runtime.logic_mem_base);
  (void)AttrUtils::GetInt(model, ATTR_MODEL_VAR_SIZE, codegen_model.runtime.var_size);
  (void)AttrUtils::GetInt(model, ATTR_MODEL_TASK_GEN_VAR_ADDR, codegen_model.runtime.logic_var_base);
  (void)AttrUtils::GetInt(model, MODEL_ATTR_TASK_GEN_WEIGHT_ADDR, codegen_model.runtime.logic_weight_base);
  codegen_model.runtime.memory_infos.clear();

  const auto &memory_info_vec = GetAllMemoryTypeSize(model);
  for (auto &i : memory_info_vec) {
    if (i.memory_type != RT_MEMORY_HBM) {
      codegen_model.runtime.memory_infos[i.memory_type] = i;
    }
  }
  codegen_model.runtime.stream_flag_values.assign(codegen_model.runtime.stream_num, "RT_STREAM_PERSISTENT");
  codegen_model.runtime.bind_flag_values.assign(codegen_model.runtime.stream_num, "RT_HEAD_STREAM");
  UpdateStreamFlag(model, codegen_model);
  return SUCCESS;
}

Status Om2CodegenModelBuilder::UpdateStreamFlag(const GeModelPtr &model, Om2CodegenModel &codegen_model) const {
  std::vector<int64_t> huge_stream_list;
  (void)AttrUtils::GetListInt(model, ATTR_MODEL_HUGE_STREAM_LIST, huge_stream_list);
  const std::set<int64_t> huge_streams(huge_stream_list.begin(), huge_stream_list.end());

  std::set<uint32_t> active_stream_indication;
  const ComputeGraphPtr compute_graph = model->GetGraph();
  GE_ASSERT_NOTNULL(compute_graph);
  for (const auto &node : compute_graph->GetAllNodes()) {
    const auto &op_desc = node->GetOpDesc();
    GE_CHECK_NOTNULL(op_desc);
    const auto &op_type = op_desc->GetType();
    if (op_type == STREAMACTIVE) {
      GE_ASSERT_SUCCESS(InitStreamActive(op_desc, active_stream_indication));
      continue;
    }
    if (op_type == STREAMSWITCH) {
      GE_ASSERT_SUCCESS(InitStreamSwitch(op_desc, active_stream_indication));
      continue;
    }
  }

  for (uint32_t i = 0U; i < codegen_model.runtime.stream_num; ++i) {
    if (huge_streams.count(static_cast<int32_t>(i)) > 0U) {
      GELOGI("[OM2]Stream %u is huge stream.", i);
      codegen_model.runtime.stream_flag_values[i] = "RT_STREAM_PERSISTENT|RT_STREAM_HUGE";
    }
    if (active_stream_indication.count(i) > 0U) {
      codegen_model.runtime.bind_flag_values[i] = "RT_INVALID_FLAG";
    }
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::InitStreamActive(const OpDescPtr &op_desc,
                                                std::set<uint32_t> &active_stream_indication) const {
  if (op_desc->HasAttr(ATTR_NAME_SWITCH_BRANCH_NODE_LABEL)) {
    std::vector<uint32_t> active_stream_list;
    if (!AttrUtils::GetListInt(op_desc, ATTR_NAME_ACTIVE_STREAM_LIST, active_stream_list)) {
      REPORT_INNER_ERR_MSG("E19999", "[Get][Attr] active_stream_list in op:%s(%s) failed.",
                         op_desc->GetName().c_str(), op_desc->GetType().c_str());
      GELOGE(INTERNAL_ERROR, "[Get][Attr] active_stream_list in op:%s(%s) failed.",
             op_desc->GetName().c_str(), op_desc->GetType().c_str());
      return INTERNAL_ERROR;
    }

    for (size_t j = 0U; j < active_stream_list.size(); ++j) {
      (void)active_stream_indication.insert(active_stream_list[j]);
      GELOGI("[OM2]flowctrl_op_index_map node:%s, active_stream_id=%u.", op_desc->GetName().c_str(), active_stream_list[j]);
    }
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::InitStreamSwitch(const OpDescPtr &op_desc,
                                                std::set<uint32_t> &active_stream_indication) const {
  std::vector<uint32_t> active_stream_list;
  GE_LOGI_IF(!AttrUtils::GetListInt(op_desc, ATTR_NAME_ACTIVE_STREAM_LIST, active_stream_list),
             "GetInt active_stream_list failed.");
  if (active_stream_list.size() != kTrueBranchStreamCount) {
    REPORT_INNER_ERR_MSG("E19999", "[Check][Param] Attr: active_stream_list.size:%zu in op:%s(%s) != 1, "
        "check invalid", active_stream_list.size(), op_desc->GetName().c_str(), op_desc->GetType().c_str());
    GELOGE(INTERNAL_ERROR, "[Check][Param] Attr: active_stream_list.size:%zu in op:%s(%s) != 1",
           active_stream_list.size(), op_desc->GetName().c_str(), op_desc->GetType().c_str());
    return INTERNAL_ERROR;
  }

  const uint32_t true_stream_id = active_stream_list.front();
  (void)active_stream_indication.insert(true_stream_id);
  GELOGI("[OM2]flowctrl_op_index_map node:%s, true_stream_id=%u.", op_desc->GetName().c_str(), true_stream_id);

  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildModelIo(const GeModelPtr &model, Om2CodegenModel &codegen_model) const {
  GE_ASSERT_NOTNULL(model);
  const auto compute_graph = model->GetGraph();
  GE_ASSERT_NOTNULL(compute_graph);
  std::vector<InputModelIoItem> input_items;
  std::vector<OutputModelIoItem> output_items;
  GE_ASSERT_SUCCESS(CollectModelIoItems(codegen_model, compute_graph, input_items, output_items));
  std::set<int64_t> input_offsets;
  for (const auto &item : input_items) {
    input_offsets.insert(item.memory_offset);
  }
  for (const auto &item : output_items) {
    if (input_offsets.count(item.memory_offset) > 0U) {
      REPORT_INNER_ERR_MSG("E19999", "[OM2] memory_offset %" PRId64 " is both input and output, which is not supported.",
                           item.memory_offset);
      GELOGE(PARAM_INVALID, "[OM2] memory_offset %" PRId64 " is both input and output, which is not supported.",
             item.memory_offset);
      return PARAM_INVALID;
    }
  }
  std::stable_sort(input_items.begin(), input_items.end(), CompareInputModelIoItem);
  uint32_t update_host_args_index = 0U;
  for (const auto &item : input_items) {
    codegen_model.model_io.entries.push_back(ModelIoEntry{item.index, item.memory_offset, update_host_args_index, true});
    ++update_host_args_index;
  }
  for (const auto &item : output_items) {
    codegen_model.model_io.entries.push_back(
        ModelIoEntry{item.index, item.memory_offset, update_host_args_index, false});
    ++update_host_args_index;
  }
  codegen_model.model_io.input_count = static_cast<uint32_t>(input_items.size());
  codegen_model.model_io.output_count = static_cast<uint32_t>(output_items.size());
  return SUCCESS;
}

Status Om2CodegenModelBuilder::CollectModelIoItems(Om2CodegenModel &codegen_model, const ComputeGraphPtr &compute_graph,
                                                   std::vector<InputModelIoItem> &input_items,
                                                   std::vector<OutputModelIoItem> &output_items) const {
  size_t input_visit_order = 0U;
  for (const auto &node : compute_graph->GetDirectNode()) {
    GE_ASSERT_NOTNULL(node);
    const auto &op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    if (OpTypeUtils::IsDataNode(op_desc->GetType())) {
      uint32_t index = static_cast<uint32_t>(input_items.size());
      if (AttrUtils::GetInt(op_desc, ATTR_NAME_INDEX, index)) {
        GELOGD("[OM2] Get data node attr index %u, node=%s", index, node->GetName().c_str());
      }
      const auto output_offsets = op_desc->GetOutputOffset();
      GE_ASSERT_TRUE(!output_offsets.empty(), "[OM2] Data node output offset is empty, node=%s", node->GetName().c_str());
      input_items.push_back(InputModelIoItem{index, output_offsets[0], input_visit_order});
      codegen_model.model_io.io_offsets.emplace(output_offsets[0]);
      ++input_visit_order;
      continue;
    }

    if (!OpTypeUtils::IsGraphOutputNode(op_desc->GetType())) {
      continue;
    }
    const auto input_offsets = op_desc->GetInputOffset();
    for (size_t i = 0U; i < op_desc->GetAllInputsSize(); ++i) {
      const auto tensor_desc = op_desc->MutableInputDesc(static_cast<uint32_t>(i));
      if (tensor_desc == nullptr) {
        GELOGD("[OM2] Op: %s, Index: %zu, has no input", op_desc->GetName().c_str(), i);
        continue;
      }
      GE_ASSERT_TRUE(i < input_offsets.size(), "[OM2] NetOutput input offset is out of range, node=%s, index=%zu",
                     node->GetName().c_str(), i);
      output_items.push_back(OutputModelIoItem{static_cast<uint32_t>(output_items.size()), input_offsets[i]});
      codegen_model.model_io.io_offsets.emplace(input_offsets[i]);
    }
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildConstInputs(const GeModelPtr &model,
                                                const std::vector<TaskCodeBuilderPtr> &task_builders,
                                                Om2CodegenModel &codegen_model, Om2ConstMetas &const_metas) {
  GE_ASSERT_NOTNULL(model);
  const auto &model_task_def = model->GetModelTaskDefPtr();
  GE_ASSERT_NOTNULL(model_task_def);

  const size_t task_size = static_cast<size_t>(model_task_def->task_size());
  for (size_t i = 0U; i < task_size; ++i) {
    GE_ASSERT_TRUE(i < task_builders.size(), "[OM2] Task builder index %zu is out of range, size=%zu", i,
                   task_builders.size());
    const auto &task_builder = task_builders[i];
    GE_ASSERT_NOTNULL(task_builder, "[OM2] Task builder from task index %zu has not been created.", i);
    const auto &task_def = model_task_def->task(static_cast<int32_t>(i));
    const int64_t op_index = task_builder->ParseOpIndex(task_def);
    if (op_index == kInvalidOpIndex) {
      continue;
    }
    const auto op_desc_it = op_desc_by_index_.find(op_index);
    GE_ASSERT_TRUE(op_desc_it != op_desc_by_index_.end(), "[OM2] Failed to find op_desc by op_index %" PRId64 ".",
                   op_index);
    GE_ASSERT_SUCCESS(CollectConstInputsFromOp(op_desc_it->second, codegen_model, const_metas));
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildFileConstInputs(const GeModelPtr &model, Om2CodegenModel &codegen_model,
                                                    Om2ConstMetas &const_metas) {
  GE_ASSERT_NOTNULL(model);
  const auto compute_graph = model->GetGraph();
  GE_ASSERT_NOTNULL(compute_graph);
  std::map<std::string, std::string> file_id_to_path_map;
  GE_ASSERT_SUCCESS(FileConstantUtils::GetFileIdToPathMapFromOption(file_id_to_path_map));
  std::string combined_file_path;
  bool has_file_const = false;
  bool is_combined = true;
  std::vector<size_t> file_const_meta_indices;
  for (const auto &node : compute_graph->GetAllNodes()) {
    GE_ASSERT_NOTNULL(node);
    const auto &op_desc = node->GetOpDesc();
    GE_ASSERT_NOTNULL(op_desc);
    if (op_desc->GetType() != FILECONSTANT) {
      continue;
    }
    std::string file_path;
    size_t offset = 0U;
    size_t length = 0U;
    GE_ASSERT_SUCCESS(FileConstantUtils::GetFilePath(op_desc, file_id_to_path_map, file_path, offset, length));
    if (!has_file_const) {
      combined_file_path = file_path;
      has_file_const = true;
    } else if (is_combined && (combined_file_path != file_path)) {
      is_combined = false;
      for (const auto meta_index : file_const_meta_indices) {
        const_metas[meta_index].type = "INDIVIDUAL";
      }
    }
    const auto output_desc = op_desc->MutableOutputDesc(0U);
    GE_ASSERT_NOTNULL(output_desc);
    int64_t tensor_size = 0;
    GE_ASSERT_SUCCESS(TensorUtils::GetTensorSizeInBytes(*output_desc, tensor_size));
    const auto output_offsets = op_desc->GetOutputOffset();
    GE_ASSERT_TRUE(!output_offsets.empty(), "[OM2] FileConstant output offset is empty, op=%s",
                   op_desc->GetName().c_str());
    const size_t const_index = const_metas.size();
    const std::string var_name = "const_" + std::to_string(const_index);
    ConstInputEntry entry;
    entry.const_index = const_index;
    entry.var_name = var_name;
    GE_ASSERT_SUCCESS(Om2ModelUtils::BuildOutputTensorInfo(output_desc, entry.tensor_info));
    codegen_model.const_inputs.push_back(std::move(entry));
    fileconst_output_offset_to_varname_.emplace(output_offsets[0U], var_name);
    const_metas.push_back(Om2ConstMeta{const_index, is_combined ? "COMBINED" : "INDIVIDUAL",
                                       StringUtils::GetFileName(file_path), file_path, static_cast<int64_t>(offset),
                                       tensor_size, op_desc->GetName()});
    file_const_meta_indices.push_back(const_index);
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildKernelRegistry(const GeModelPtr &model,
                                                   const std::vector<TaskCodeBuilderPtr> &task_builders,
                                                   Om2CodegenModel &codegen_model) {
  GE_ASSERT_NOTNULL(model);
  const auto &model_task_def = model->GetModelTaskDefPtr();
  GE_ASSERT_NOTNULL(model_task_def);

  const size_t task_size = static_cast<size_t>(model_task_def->task_size());
  bool need_registry_tf_session_task = false;
  for (size_t i = 0U; i < task_size; ++i) {
    const auto &task_def = model_task_def->task(static_cast<int32_t>(i));
    const auto task_type = static_cast<ModelTaskType>(task_def.type());
    if (!Om2CodegenUtils::RequireBinaryKernel(task_type)) {
      continue;
    }
    GE_ASSERT_TRUE(i < task_builders.size(), "[OM2] Task builder index %zu is out of range, size=%zu", i,
                   task_builders.size());
    const auto &task_builder = task_builders[i];
    GE_ASSERT_NOTNULL(task_builder, "[OM2] Task builder from type %d and task index %zu has not been created.",
                      task_def.type(), i);
    const int64_t op_index = task_builder->ParseOpIndex(task_def);
    GE_ASSERT_TRUE(op_index != kInvalidOpIndex, "[OM2] Invalid op_index parsed for task index %zu, type=%d.", i,
                   task_def.type());

    const auto op_desc_it = op_desc_by_index_.find(op_index);
    GE_ASSERT_TRUE(op_desc_it != op_desc_by_index_.end(), "[OM2] Failed to find op_desc by op_index %" PRId64 ".",
                   op_index);
    const auto &op_desc = op_desc_it->second;
    GE_ASSERT_NOTNULL(op_desc);

    if (task_type == ModelTaskType::MODEL_TASK_KERNEL_EX) {
      const std::string op_type = op_desc->GetType();
      const std::string tf_aicpu_kernel_sign = op_type;
      if (codegen_model.kernel_registry.func_handle_indices.find(tf_aicpu_kernel_sign) !=
          codegen_model.kernel_registry.func_handle_indices.end()) {
        continue;
      }
      GE_ASSERT_SUCCESS(BuildKernelRegistryForTFAicpu(codegen_model, op_type, tf_aicpu_kernel_sign));
      need_registry_tf_session_task = true;
      continue;
    }
    const auto kernel_type = Om2CodegenUtils::IsAllKernel(task_type)
                                 ? task_def.kernel_with_handle().context().kernel_type()
                                 : task_def.kernel().context().kernel_type();
    const bool is_aicore = Om2CodegenUtils::IsAllKernel(task_type) ||
                           Om2CodegenUtils::IsAICoreKernel(static_cast<ge::ccKernelType>(kernel_type));

    std::string kernel_name;
    if (is_aicore) {
      GE_ASSERT_SUCCESS(BuildKernelRegistryForAicore(codegen_model, op_desc, task_type));
      continue;
    }

    kernel_name = task_def.kernel().kernel_name();
    if (static_cast<ge::ccKernelType>(kernel_type) == ge::ccKernelType::AI_CPU) {
      const std::string op_type = op_desc->GetType();
      const std::string aicpu_kernel_sign = op_type + kernel_name;
      if (codegen_model.kernel_registry.func_handle_indices.find(aicpu_kernel_sign) !=
          codegen_model.kernel_registry.func_handle_indices.end()) {
        continue;
      }
      GE_ASSERT_SUCCESS(BuildKernelRegistryForAicpu(codegen_model, task_def, op_type, kernel_name,
                                                    aicpu_kernel_sign));
      continue;
    }

    if (static_cast<ge::ccKernelType>(kernel_type) == ge::ccKernelType::CUST_AI_CPU) {
      const std::string op_type = op_desc->GetType();
      const std::string cust_aicpu_kernel_sign = op_type + kernel_name;
      if (codegen_model.kernel_registry.func_handle_indices.find(cust_aicpu_kernel_sign) !=
          codegen_model.kernel_registry.func_handle_indices.end()) {
        continue;
      }
      GE_ASSERT_SUCCESS(BuildKernelRegistryForCustAicpu(codegen_model, op_desc, op_type, kernel_name,
                                                        cust_aicpu_kernel_sign));
      continue;
    }

    REPORT_INNER_ERR_MSG("E19999", "Unsupported task type %d", static_cast<int32_t>(task_type));
    GELOGE(FAILED, "[OM2] Unsupported task type %d, task def %s", static_cast<int32_t>(task_type),
           task_def.ShortDebugString().c_str());
    return FAILED;
  }
  if (need_registry_tf_session_task) {
    GE_ASSERT_SUCCESS(BuildKernelRegistryForTFAicpuSession(codegen_model, kTfSessionTask, kTfSessionTask));
  }
  codegen_model.runtime.kernel_bin_num = static_cast<uint32_t>(codegen_model.kernel_registry.binaries.size());
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildKernelRegistryForAicore(Om2CodegenModel &codegen_model, const OpDescPtr &op_desc,
                                                            ModelTaskType task_type) {
  const auto kernel_name_ptr = AttrUtils::GetStr(op_desc, "_kernelname");
  GE_ASSERT_NOTNULL(kernel_name_ptr, "[OM2] Failed to get kernel_name from op_desc, op=%s",
                    op_desc->GetName().c_str());
  const std::string kernel_name = *kernel_name_ptr;

  uint64_t tiling_key = 0U;
  std::string aicore_sign = kernel_name;
  auto kind = KernelBinaryKind::kAicore;
  if (Om2CodegenUtils::IsAllKernel(task_type)) {
    const auto tiling_info =
        op_desc->GetExtAttr<std::shared_ptr<optiling::utils::OpRunInfo>>(ge::ATTR_NAME_OP_RUN_INFO);
    if ((tiling_info != nullptr) && (*tiling_info != nullptr)) {
      tiling_key = (*tiling_info)->GetTilingKey();
    }
    aicore_sign = kernel_name + "#" + std::to_string(tiling_key);
    kind = KernelBinaryKind::kAllKernel;
  }
  if (codegen_model.kernel_registry.func_handle_indices.find(aicore_sign) !=
      codegen_model.kernel_registry.func_handle_indices.end()) {
    return SUCCESS;
  }

  std::string magic;
  GE_CHK_STATUS(Om2CodegenUtils::GetMagic(op_desc, magic));
  const uint32_t func_handle_index =
      static_cast<uint32_t>(codegen_model.kernel_registry.func_handle_indices.size());
  codegen_model.kernel_registry.binaries.push_back(KernelBinaryRecord{kind,
                                                                      kernel_name,
                                                                      Om2CodegenUtils::GetKernelNameWithExtension(
                                                                          kernel_name),
                                                                      "",
                                                                      "",
                                                                      "",
                                                                      magic,
                                                                      tiling_key,
                                                                      func_handle_index});
  codegen_model.kernel_registry.func_handle_indices.emplace(aicore_sign, func_handle_index);
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildKernelRegistryForAicpu(Om2CodegenModel &codegen_model,
                                                           const domi::TaskDef task_def,
                                                           const std::string op_type,
                                                           const std::string &kernel_name,
                                                           const std::string &aicpu_kernel_sign) {
  const std::string &so_name = task_def.kernel().so_name();
  const std::string op_kernel_lib = "AICPUKernel";
  const uint32_t func_handle_index =
      static_cast<uint32_t>(codegen_model.kernel_registry.func_handle_indices.size());
  codegen_model.kernel_registry.binaries.push_back(KernelBinaryRecord{KernelBinaryKind::kAicpu,
                                                                      kernel_name,
                                                                      "",
                                                                      op_type,
                                                                      so_name,
                                                                      op_kernel_lib,
                                                                      "",
                                                                      0U,
                                                                      func_handle_index});
  codegen_model.kernel_registry.func_handle_indices.emplace(aicpu_kernel_sign, func_handle_index);
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildKernelRegistryForCustAicpu(Om2CodegenModel &codegen_model,
                                                               const OpDescPtr &op_desc,
                                                               const std::string &op_type,
                                                               const std::string &kernel_name,
                                                               const std::string &kernel_sign) {
  const auto cust_aicpu_bin_ptr = op_desc->TryGetExtAttr(OP_EXTATTR_CUSTAICPU_KERNEL, CustAICPUKernelPtr());
  GE_ASSERT_NOTNULL(cust_aicpu_bin_ptr);
  const size_t hash_id = std::hash<std::string>{}(std::string(
      cust_aicpu_bin_ptr->GetBinData(), cust_aicpu_bin_ptr->GetBinData() + cust_aicpu_bin_ptr->GetBinDataSize()));
  const std::string file_name = std::to_string(hash_id) + "_CustAicpuKernel";
  const uint32_t func_handle_index =
      static_cast<uint32_t>(codegen_model.kernel_registry.func_handle_indices.size());
  codegen_model.kernel_registry.binaries.push_back(KernelBinaryRecord{KernelBinaryKind::kCustAicpu,
                                                                      kernel_name,
                                                                      file_name,
                                                                      op_type,
                                                                      "",
                                                                      "",
                                                                      "",
                                                                      0U,
                                                                      func_handle_index});
  codegen_model.kernel_registry.func_handle_indices.emplace(kernel_sign, func_handle_index);
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildKernelRegistryForTFAicpu(Om2CodegenModel &codegen_model,
 	                                                              const std::string op_type,
 	                                                              const std::string &tf_aicpu_kernel_sign) {
  const uint32_t func_handle_index = static_cast<uint32_t>(codegen_model.kernel_registry.func_handle_indices.size());
  codegen_model.kernel_registry.binaries.push_back(KernelBinaryRecord{KernelBinaryKind::kAicpu,
    "TFOperateAPI",
    "",
    op_type,
    "libtf_kernels.so",
    "TFKernel",
    "",
    0U,
    func_handle_index});
  codegen_model.kernel_registry.func_handle_indices.emplace(tf_aicpu_kernel_sign, func_handle_index);
  return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildKernelRegistryForTFAicpuSession(Om2CodegenModel &codegen_model,
  const std::string op_type, const std::string &tf_aicpu_kernel_sign) {
  const uint32_t func_handle_index = static_cast<uint32_t>(codegen_model.kernel_registry.func_handle_indices.size());
  codegen_model.kernel_registry.binaries.push_back(KernelBinaryRecord{KernelBinaryKind::kAicpu,
    "TFOperateAPI",
    "",
    op_type,
    "libtf_kernels.so",
    "TFKernel",
    "",
    0U,
    func_handle_index});
  codegen_model.kernel_registry.func_handle_indices.emplace(tf_aicpu_kernel_sign, func_handle_index);
 	return SUCCESS;
}

Status Om2CodegenModelBuilder::BuildTaskSemantics(const GeModelPtr &model,
                                                  const std::vector<TaskCodeBuilderPtr> &task_builders,
                                                  Om2CodegenModel &codegen_model) {
  GE_ASSERT_NOTNULL(model);
  const auto &model_task_def = model->GetModelTaskDefPtr();
  GE_ASSERT_NOTNULL(model_task_def);

  const size_t task_size = static_cast<size_t>(model_task_def->task_size());
  uint64_t next_args_table_index = 0UL;
  uint64_t next_host_args_offset = 0UL;
  for (size_t i = 0U; i < task_size; ++i) {
    const auto &task_def = model_task_def->task(static_cast<int32_t>(i));
    const auto task_type = static_cast<ModelTaskType>(task_def.type());
    GE_ASSERT_TRUE(i < task_builders.size(), "[OM2] Task builder index %zu is out of range, size=%zu", i,
                   task_builders.size());
    const auto &task_builder = task_builders[i];
    GE_ASSERT_NOTNULL(task_builder, "[OM2] Task builder from type %d and task index %zu has not been created.",
                      task_def.type(), i);

    int64_t op_index = kInvalidOpIndex;
    OpDescPtr op_desc = nullptr;
    op_index = task_builder->ParseOpIndex(task_def);
    if (op_index != kInvalidOpIndex) {
      const auto op_desc_it = op_desc_by_index_.find(op_index);
      GE_ASSERT_TRUE(op_desc_it != op_desc_by_index_.end(), "[OM2] Failed to find op_desc by op_index %" PRId64 ".",
                     op_index);
      op_desc = op_desc_it->second;
    }

    TaskSemanticContributeContext context{
        task_type,                  task_def,                       op_index,
        op_desc,                    &codegen_model.runtime,         &codegen_model.model_io,
        &codegen_model.kernel_registry.func_handle_indices, &weight_offset_to_varname_,
        &fileconst_output_offset_to_varname_, &op_id_to_input_edges_, &op_index_to_count_map_,
        &next_args_table_index,     &next_host_args_offset,         &codegen_model.aicpu_task_count};
    GE_ASSERT_SUCCESS(task_builder->Contribute(context));
  }
  return SUCCESS;
}

Status Om2CodegenModelBuilder::AggregateArgsTable(const std::vector<TaskCodeBuilderPtr> &task_builders,
                                                  Om2CodegenModel &codegen_model) const {
  uint64_t max_end = 0UL;
  std::multimap<uint64_t, uint64_t> io_addr_offset_map;
  for (const auto &task_builder : task_builders) {
    GE_ASSERT_NOTNULL(task_builder);
    const auto *entry = task_builder->GetArgsTableEntry();
    if (entry == nullptr) {
      continue;
    }
    codegen_model.args_table.entries.push_back(*entry);
    const auto &records = task_builder->GetIoAddrRefreshRecords();
    for (const auto &record : records) {
      io_addr_offset_map.emplace(record.compile_state_io_addr_offset, record.host_offset);
    }
    max_end = std::max(max_end, entry->host_offset + entry->args_size);
  }
  codegen_model.args_table.total_host_args_len = max_end;
  GE_ASSERT_SUCCESS(BuildHostArgsOffsets(io_addr_offset_map, codegen_model));
  return SUCCESS;
}
}  // namespace ge
