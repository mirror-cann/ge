/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/om2/codegen/om2_model_utils.h"
#include <cinttypes>
#include <limits>
#include "common/checker.h"
#include "common/om2/codegen/ast/ast_nodes.h"
#include "common/om2/codegen/task_code_builder/task_code_builder.h"
#include "common/ge_common/debug/ge_log.h"
#include "common/math/math_util.h"
#include "graph/utils/tensor_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "framework/common/types.h"
#include "runtime/mem.h"

namespace ge {
constexpr uint64_t kSessionScopeMemoryMask = 0x100000000UL;
constexpr int32_t kSessionNoReuse = 1;
constexpr uint32_t kAlign8B = 8U;

uint64_t Om2ModelUtils::GetWorkspaceMemTypeByPriority(const bool is_p2p_memory, const bool is_l1_memory,
                                                      const bool is_ub_memory,
                                                      const bool session_scope_memory) {
  if (is_p2p_memory) {
    return RT_MEMORY_P2P_DDR;
  }
  if (is_l1_memory) {
    return RT_MEMORY_L1;
  }
  if (is_ub_memory) {
    return kRtMemoryUB;
  }
  if (session_scope_memory) {
    return kSessionScopeMemoryMask | RT_MEMORY_HBM;
  }
  return RT_MEMORY_HBM;
}

bool Om2ModelUtils::ValidateMemRange(const ConstOpDescPtr &op_desc, const uint64_t total_size, const int64_t offset,
                                     const int64_t size) {
  if (CheckInt64AddOverflow(offset, size) != SUCCESS) {
    GELOGE(PARAM_INVALID, "[OM2] Int64 %" PRId64 " and %" PRId64 " addition can result in overflow!", offset, size);
    return false;
  }
  const int64_t mem_range = offset + size;
  if (total_size < static_cast<uint64_t>(mem_range)) {
    REPORT_INNER_ERR_MSG("E19999", "Node:%s(%s) memory out of range, offset:%" PRId64 ", size:%" PRId64
                       ", exceed total size:%" PRIu64 ".", op_desc->GetName().c_str(), op_desc->GetType().c_str(),
                       offset, size, total_size);
    GELOGE(OUT_OF_MEMORY, "[OM2][Check][Param]Node:%s(%s) memory out of range, offset:%" PRId64 ", size:%" PRId64
           ", exceed total size:%" PRIu64 ".", op_desc->GetName().c_str(), op_desc->GetType().c_str(), offset, size,
           total_size);
    return false;
  }
  return true;
}

Status Om2ModelUtils::GetValidatedTensorMemType(const GeTensorDescPtr &tensor_desc,
                                                const std::vector<int64_t> &mem_types, size_t index,
                                                uint64_t &memory_type) {
  int64_t tensor_mem_type = -1;
  const bool tensor_has_mem_type = AttrUtils::GetInt(tensor_desc, ATTR_NAME_TENSOR_MEM_TYPE, tensor_mem_type);
  memory_type = RT_MEMORY_DEFAULT;
  if (tensor_has_mem_type) {
    memory_type = static_cast<uint64_t>(tensor_mem_type);
  } else if (mem_types.size() > index) {
    memory_type = static_cast<uint64_t>(mem_types[index]);
  }

  if (memory_type != RT_MEMORY_HBM && memory_type != RT_MEMORY_DEFAULT) {
    REPORT_INNER_ERR_MSG("E19999", "Workspace mem type[%" PRIu64 "] is invalid, must be RT_MEMORY_HBM.", memory_type);
    GELOGE(FAILED, "[OM2] Workspace mem type[%" PRIu64 "] is invalid, must be RT_MEMORY_HBM.", memory_type);
    return FAILED;
  }
  return SUCCESS;
}

Status Om2ModelUtils::ResolveInputVarName(std::unordered_map<int64_t, OpInputEdges> &op_id_to_input_edges,
                                          const ConstOpDescPtr &op_desc, const int64_t op_index,
                                          const size_t input_idx, const size_t non_const_idx,
                                          std::string &input_ptr_name, bool &is_reused_from_upstream) {
  GE_ASSERT_NOTNULL(op_desc);
  const int64_t current_op_id = op_desc->GetId();
  GE_ASSERT_TRUE(op_id_to_input_edges.find(current_op_id) != op_id_to_input_edges.end(),
                 "[OM2] Current op_id %" PRId64 " not found in op_id_to_input_edges", current_op_id);
  const OpInputEdges &current_edges = op_id_to_input_edges.at(current_op_id);
  GE_ASSERT_TRUE(input_idx < current_edges.input_op_ids.size(),
                 "[OM2] Input index %zu out of range for op_id %" PRId64, input_idx, current_op_id);
  const int64_t src_op_id = current_edges.input_op_ids[input_idx];
  const int32_t src_anchor_idx = current_edges.input_anchor_indices[input_idx];
  if (src_op_id == kInvalidOpId) {
    GELOGE(FAILED, "[OM2] Input %zu of op %s(%" PRId64 ") is unconnected optional input, not supported",
           input_idx, op_desc->GetName().c_str(), current_op_id);
    return FAILED;
  }
  GE_ASSERT_TRUE(op_id_to_input_edges.find(src_op_id) != op_id_to_input_edges.end(),
                 "[OM2] Source op_id %" PRId64 " not found in op_id_to_input_edges", src_op_id);
  OpInputEdges &src_edges = op_id_to_input_edges.at(src_op_id);
  GE_ASSERT_TRUE(src_anchor_idx < static_cast<int32_t>(src_edges.output_var_names.size()),
                 "[OM2] Source anchor index %d out of range for src_op_id %" PRId64, src_anchor_idx, src_op_id);
  if (!src_edges.output_var_names[src_anchor_idx].empty()) {
    input_ptr_name = src_edges.output_var_names[src_anchor_idx];
    is_reused_from_upstream = true;
  } else {
    input_ptr_name = "op" + std::to_string(op_index) + "_input" + std::to_string(non_const_idx);
    src_edges.output_var_names[src_anchor_idx] = input_ptr_name;
    is_reused_from_upstream = false;
  }
  return SUCCESS;
}

bool Om2ModelUtils::GetFileConstInputVarName(
    const int64_t input_offset,
    const std::unordered_map<int64_t, std::string> &fileconst_output_offset_to_varname,
    std::string &input_ptr_name) {
  const auto iter = fileconst_output_offset_to_varname.find(input_offset);
  if (iter == fileconst_output_offset_to_varname.end()) {
    return false;
  }
  input_ptr_name = iter->second;
  return true;
}

Status Om2ModelUtils::ConstructAddrSemanticForInputConst(
                      const TaskSemanticContributeContext &context, AddrSemantic &input_addr,
                      const GeTensorDescPtr &tensor_desc, size_t index) {
  int64_t data_offset = 0;
  GE_CHK_STATUS(TensorUtils::GetDataOffset(*tensor_desc, data_offset));
  int64_t weight_size = 0;
  GE_CHK_STATUS(TensorUtils::GetTensorSizeInBytes(*tensor_desc, weight_size));
  GE_IF_BOOL_EXEC(!ValidateMemRange(context.op_desc, context.runtime->total_weight_size, data_offset, weight_size),
                  return FAILED);
  const auto var_name_it = context.weight_offset_to_varname->find(data_offset);
  GE_ASSERT_TRUE(var_name_it != context.weight_offset_to_varname->end(),
                  "[OM2] Const input offset %" PRId64 " not found, op %s, index %zu", data_offset,
                  context.op_desc->GetName().c_str(), index);
  input_addr.kind = AddrValueKind::kConstTensor;
  input_addr.symbol_hint = var_name_it->second;
  input_addr.mem_offset = data_offset;
  input_addr.is_reused_from_upstream = true;
  return SUCCESS;
}

Status Om2ModelUtils::ConstructAddrSemanticForCommon(
                      const TaskSemanticContributeContext &context, AddrSemantic &input_addr,
                      const GeTensorDescPtr &tensor_desc, size_t &input_offset_index,
                      const std::vector<int64_t> &input_offsets, size_t index) {
  uint64_t memory_type = RT_MEMORY_DEFAULT;
  std::vector<int64_t> v_memory_type;
  (void)AttrUtils::GetListInt(context.op_desc, ATTR_NAME_INPUT_MEM_TYPE_LIST, v_memory_type);
  GE_ASSERT_SUCCESS(GetValidatedTensorMemType(tensor_desc, v_memory_type, index, memory_type));
  GE_ASSERT_TRUE(input_offset_index < input_offsets.size(), "[OM2] Input offset index %zu out of range, op=%s",
                  input_offset_index, context.op_desc->GetName().c_str());
  const int64_t input_offset = input_offsets[input_offset_index];
  GE_IF_BOOL_EXEC(!ValidateMemRange(context.op_desc, context.runtime->total_mem_size, input_offset, 0),
                  return FAILED);
  input_addr.mem_offset = input_offset;
  if (context.model_io->io_offsets.find(input_offset) != context.model_io->io_offsets.end()) {
    input_addr.memory_app = MemoryAppType::kModelIo;
    input_addr.compile_state_io_addr_offset = input_offset;
  }

  GE_ASSERT_SUCCESS(ResolveInputVarName(*context.op_id_to_input_edges, context.op_desc, context.op_index, index,
                                        input_offset_index, input_addr.symbol_hint,
                                        input_addr.is_reused_from_upstream));
  return SUCCESS;
}

Status Om2ModelUtils::ResolveInputAddrs(const TaskSemanticContributeContext &context,
                                        std::vector<AddrSemantic> &input_addrs) {
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.model_io);
  GE_ASSERT_NOTNULL(context.weight_offset_to_varname);
  GE_ASSERT_NOTNULL(context.fileconst_output_offset_to_varname);
  GE_ASSERT_NOTNULL(context.op_id_to_input_edges);
  size_t input_offset_index = 0U;
  for (size_t i = 0U; i < context.op_desc->GetAllInputsSize(); ++i) {
    const GeTensorDescPtr tensor_desc = context.op_desc->MutableInputDesc(static_cast<uint32_t>(i));
    if (tensor_desc == nullptr) {
      GELOGD("[OM2] Op: %s, Index: %zu, has no input", context.op_desc->GetName().c_str(), i);
      continue;
    }
    int64_t tensor_size = 0;
    GE_CHK_STATUS_EXEC(TensorUtils::GetSize(*tensor_desc, tensor_size), return FAILED);
    const vector_bit_t &v_is_input_const = context.op_desc->GetIsInputConst();
    const auto &input_offsets = context.op_desc->GetInputOffset();

    AddrSemantic input_addr;
    input_addr.kind = AddrValueKind::kInputInstance;
    input_addr.memory_app = MemoryAppType::kFix;
    input_addr.byte_size = static_cast<uint64_t>(tensor_size);
    if ((i < v_is_input_const.size()) && v_is_input_const[i]) {
      GE_ASSERT_SUCCESS(ConstructAddrSemanticForInputConst(context, input_addr, tensor_desc, i));
      input_addrs.push_back(std::move(input_addr));
      ++input_offset_index;
      continue;
    }

    if (input_offset_index < input_offsets.size()) {
      const int64_t input_offset = input_offsets[input_offset_index];
      if (GetFileConstInputVarName(input_offset, *context.fileconst_output_offset_to_varname,
                                   input_addr.symbol_hint)) {
        input_addr.kind = AddrValueKind::kConstTensor;
        input_addr.mem_offset = input_offset;
        input_addr.is_reused_from_upstream = true;
        input_addrs.push_back(std::move(input_addr));
        ++input_offset_index;
        continue;
      }
    }
    GE_ASSERT_SUCCESS(ConstructAddrSemanticForCommon(context, input_addr, tensor_desc, input_offset_index,
                                                     input_offsets, i));
    input_addrs.push_back(std::move(input_addr));
    ++input_offset_index;
  }
  return SUCCESS;
}

Status Om2ModelUtils::ConstructOutputAddrForCommon(
                      const TaskSemanticContributeContext &context, AddrSemantic &output_addr,
                      const GeTensorDescPtr &tensor_desc, std::vector<int64_t> &v_memory_type,
                      const std::vector<int64_t> &v_output_offset, size_t index) {
  uint64_t memory_type = RT_MEMORY_DEFAULT;
  GE_ASSERT_SUCCESS(GetValidatedTensorMemType(tensor_desc, v_memory_type, index, memory_type));
  GE_ASSERT_TRUE(index < v_output_offset.size(), "[OM2] Output offset index %zu out of range, op=%s", index,
                  context.op_desc->GetName().c_str());
  GE_IF_BOOL_EXEC(!ValidateMemRange(context.op_desc, context.runtime->total_mem_size, v_output_offset[index], 0),
                  return FAILED);
  output_addr.mem_offset = v_output_offset[index];
  if (context.model_io->io_offsets.find(v_output_offset[index]) != context.model_io->io_offsets.end()) {
    output_addr.memory_app = MemoryAppType::kModelIo;
    output_addr.compile_state_io_addr_offset = v_output_offset[index];
  }
  return SUCCESS;
}

Status Om2ModelUtils::ResolveOutputAddrs(const TaskSemanticContributeContext &context,
                                         const bool has_optional_addr,
                                         std::vector<AddrSemantic> &output_addrs) {
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.model_io);
  GE_ASSERT_NOTNULL(context.op_id_to_input_edges);
  const size_t outputs_size = context.op_desc->GetOutputsSize();
  const auto v_output_offset = context.op_desc->GetOutputOffset();
  std::vector<int64_t> v_memory_type;
  (void)AttrUtils::GetListInt(context.op_desc, ATTR_NAME_OUTPUT_MEM_TYPE_LIST, v_memory_type);
  const int64_t current_op_id = context.op_desc->GetId();
  GE_ASSERT_TRUE(context.op_id_to_input_edges->find(current_op_id) != context.op_id_to_input_edges->end(),
                 "[OM2] Current op_id %" PRId64 " not found in op_id_to_input_edges", current_op_id);
  OpInputEdges &current_edges = context.op_id_to_input_edges->at(current_op_id);
  GE_ASSERT_TRUE(current_edges.output_var_names.size() == outputs_size,
                 "[OM2] output_var_names size %zu != outputs_size %zu for op_id %" PRId64,
                 current_edges.output_var_names.size(), outputs_size, current_op_id);

  for (size_t i = 0U; i < outputs_size; ++i) {
    const GeTensorDescPtr tensor_desc = context.op_desc->MutableOutputDesc(static_cast<uint32_t>(i));
    if (tensor_desc == nullptr) {
      GELOGW("[OM2] Op: %s, Index: %zu, Tensor Desc is null", context.op_desc->GetName().c_str(), i);
      continue;
    }
    AddrSemantic output_addr;
    output_addr.kind = AddrValueKind::kOutputInstance;
    output_addr.memory_app = MemoryAppType::kFix;
    output_addr.symbol_hint = "op" + std::to_string(context.op_index) + "_output" + std::to_string(i);
    int64_t tensor_size = 0;
    GE_CHK_STATUS_EXEC(TensorUtils::GetSize(*tensor_desc, tensor_size), return FAILED);
    output_addr.byte_size = static_cast<uint64_t>(tensor_size);
    if (TensorUtils::IsMemorySizeCalcTypeAlwaysEmpty(*tensor_desc)) {
      if (has_optional_addr) {
        output_addr.kind = AddrValueKind::kOptionalEmpty;
        current_edges.output_var_names[i] = output_addr.symbol_hint;
        output_addrs.push_back(std::move(output_addr));
      }
      continue;
    }
    GE_ASSERT_SUCCESS(ConstructOutputAddrForCommon(context, output_addr, tensor_desc, v_memory_type, v_output_offset,
                                                   i));
    current_edges.output_var_names[i] = output_addr.symbol_hint;
    output_addrs.push_back(std::move(output_addr));
  }
  return SUCCESS;
}

Status Om2ModelUtils::ResolveWorkspaceAddrs(const TaskSemanticContributeContext &context,
                                            std::vector<AddrSemantic> &workspace_addrs) {
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.runtime);
  const auto &v_workspace_offset = context.op_desc->GetWorkspace();
  const auto &v_workspace_bytes = context.op_desc->GetWorkspaceBytes();
  GE_ASSERT_TRUE(v_workspace_offset.size() == v_workspace_bytes.size(),
                 "[OM2] workspace offset size %zu != bytes size %zu", v_workspace_offset.size(), v_workspace_bytes.size());

  vector_bit_t workspace_reuse_flag;
  const bool has_workspace_reuse = AttrUtils::GetListBool(context.op_desc, "workspace_reuse_flag", workspace_reuse_flag);
  std::vector<int64_t> v_memory_type;
  std::vector<int64_t> workspace_memory_type;
  const bool has_mem_type_attr = AttrUtils::GetListInt(context.op_desc, TVM_ATTR_NAME_WORKSPACE_TYPE, v_memory_type);
  const bool has_mem_type_workspace =
      AttrUtils::GetListInt(context.op_desc, ATTR_NAME_WORKSPACE_TYPE_LIST, workspace_memory_type);
  std::vector<int32_t> workspace_no_reuse_scope;
  const bool has_workspace_no_reuse_scope =
      AttrUtils::GetListInt(context.op_desc, ATTR_NAME_WORKSPACE_MEMORY_NO_REUSE_SCOPE, workspace_no_reuse_scope);

  for (size_t i = 0U; i < v_workspace_bytes.size(); ++i) {
    const bool aicpu_work_space =
        has_workspace_reuse && (i < workspace_reuse_flag.size()) && (!workspace_reuse_flag[i]);
    if (aicpu_work_space) {
      GELOGE(FAILED, "[OM2] Aicpu task not support append workspace addrs for now.");
      return FAILED;
    }
    const bool session_scope_memory =
        has_workspace_no_reuse_scope && (i < workspace_no_reuse_scope.size()) &&
        (workspace_no_reuse_scope[i] == kSessionNoReuse);
    const bool is_p2p_memory =
        has_mem_type_workspace && (static_cast<uint64_t>(workspace_memory_type[i]) == RT_MEMORY_P2P_DDR);
    const bool is_l1_memory = has_mem_type_attr && (static_cast<uint64_t>(v_memory_type[i]) == RT_MEMORY_L1);
    const bool is_ub_memory = has_mem_type_attr && (static_cast<uint64_t>(v_memory_type[i]) == kRtMemoryUB);
    const uint64_t memory_type =
        GetWorkspaceMemTypeByPriority(is_p2p_memory, is_l1_memory, is_ub_memory, session_scope_memory);
    if (memory_type != RT_MEMORY_HBM) {
      REPORT_INNER_ERR_MSG("E19999", "Workspace mem type[%" PRIu64 "] is invalid, must be RT_MEMORY_HBM.", memory_type);
      GELOGE(ge::FAILED, "[OM2] Workspace mem type[%" PRIu64 "] is invalid, must be RT_MEMORY_HBM.", memory_type);
      return FAILED;
    }
    GE_IF_BOOL_EXEC(!ValidateMemRange(context.op_desc, context.runtime->total_mem_size, v_workspace_offset[i], 0),
                    return FAILED);
    AddrSemantic workspace_addr;
    workspace_addr.kind = AddrValueKind::kWorkspace;
    workspace_addr.memory_app = MemoryAppType::kFix;
    workspace_addr.symbol_hint = "op" + std::to_string(context.op_index) + "_ws" + std::to_string(i);
    workspace_addr.mem_offset = v_workspace_offset[i];
    workspace_addr.byte_size = static_cast<uint64_t>(v_workspace_bytes[i]);
    workspace_addrs.push_back(std::move(workspace_addr));
  }
  return SUCCESS;
}

Status Om2ModelUtils::GetRtInputAddress(const TaskSemanticContributeContext &context, const int64_t logical_offset,
                                        AddrSemantic &addr_node, uint32_t index) {
  const GeTensorDescPtr tensor_desc = context.op_desc->MutableInputDesc(static_cast<uint32_t>(index));
  GE_ASSERT_TRUE(tensor_desc != nullptr, "[OM2] Op: %s, Index: %u, has no input",
                  context.op_desc->GetName().c_str(), index);
  int64_t tensor_size = 0;
  GE_CHK_STATUS_EXEC(TensorUtils::GetSize(*tensor_desc, tensor_size), return FAILED);

  addr_node.kind = AddrValueKind::kInputInstance;
  addr_node.memory_app = MemoryAppType::kFix;
  addr_node.byte_size = static_cast<uint64_t>(tensor_size);
  addr_node.mem_offset = logical_offset;
  if (context.model_io->io_offsets.find(logical_offset) != context.model_io->io_offsets.end()) {
    addr_node.memory_app = MemoryAppType::kModelIo;
    addr_node.compile_state_io_addr_offset = logical_offset;
  }

  GE_ASSERT_SUCCESS(ResolveInputVarName(*context.op_id_to_input_edges, context.op_desc, context.op_index, index,
                                        index, addr_node.symbol_hint,
                                        addr_node.is_reused_from_upstream));
  return SUCCESS;
}

Status Om2ModelUtils::GetRtAddress(const TaskSemanticContributeContext &context, const uintptr_t logic_addr,
                                    uint64_t &mem_type, AddrSemantic &addr_node, bool isInput, uint32_t index) {
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.model_io);
  GE_ASSERT_NOTNULL(context.op_id_to_input_edges);
  if (logic_addr == std::numeric_limits<uintptr_t>::max()) {
    GELOGI("Got placeholder logic addr.");
    return SUCCESS;
  }
  if ((context.runtime->logic_mem_base <= logic_addr) && (logic_addr < (context.runtime->logic_mem_base +
      context.runtime->total_mem_size))) {
    mem_type = kFmMemType;
    const int64_t logical_offset = static_cast<int64_t>(logic_addr - context.runtime->logic_mem_base);
    GELOGI("logical_offset %" PRId64 ", index %u", logical_offset, index);
    if (isInput) {
      GE_ASSERT_SUCCESS(GetRtInputAddress(context, logical_offset, addr_node, index));
    } else {
      const GeTensorDescPtr tensor_desc = context.op_desc->MutableOutputDesc(static_cast<uint32_t>(index));
      GE_ASSERT_TRUE(tensor_desc != nullptr, "[OM2] Op: %s, Index: %u, Tensor Desc is null",
                     context.op_desc->GetName().c_str(), index);
      addr_node.kind = AddrValueKind::kOutputInstance;
      addr_node.memory_app = MemoryAppType::kFix;
      addr_node.symbol_hint = "op" + std::to_string(context.op_index) + "_output" + std::to_string(index);
      int64_t tensor_size = 0;
      GE_CHK_STATUS_EXEC(TensorUtils::GetSize(*tensor_desc, tensor_size), return FAILED);
      addr_node.byte_size = static_cast<uint64_t>(tensor_size);
      addr_node.mem_offset = logical_offset;
      if (context.model_io->io_offsets.find(logical_offset) != context.model_io->io_offsets.end()) {
        addr_node.memory_app = MemoryAppType::kModelIo;
        addr_node.compile_state_io_addr_offset = logical_offset;
      }
      const int64_t current_op_id = context.op_desc->GetId();
      GE_ASSERT_TRUE(context.op_id_to_input_edges->find(current_op_id) != context.op_id_to_input_edges->end(),
                     "[OM2] Current op_id %" PRId64 " not found in op_id_to_input_edges", current_op_id);
      OpInputEdges &current_edges = context.op_id_to_input_edges->at(current_op_id);
      GE_ASSERT_TRUE(index < current_edges.output_var_names.size(),
                     "[OM2] Output index %u out of range for op_id %" PRId64, index, current_op_id);
      current_edges.output_var_names[index] = addr_node.symbol_hint;
    }
    return SUCCESS;
  } else {
    REPORT_INNER_ERR_MSG("E19999", "Unsupport scenario in GetRtAddress");
    GELOGE(ge::FAILED, "Unsupport scenario in GetRtAddress");
    return FAILED;
  }
  return SUCCESS;
}

uint32_t Om2ModelUtils::ArgsSizeAlign8(uint32_t args_size) {
  return (args_size + kAlign8B - 1) / kAlign8B * kAlign8B;
}
}  // namespace ge
