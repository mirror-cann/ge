/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_OM2_MODEL_UTILS_H
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_OM2_MODEL_UTILS_H

#include "common/om2/codegen/om2_codegen_types.h"
#include "graph/op_desc.h"
#include "graph/utils/attr_utils.h"

namespace ge {
struct TaskSemanticContributeContext;
constexpr uint64_t kMemoryVarLogicBase = 34359738368U; // 32UL * 1024UL * 1024UL * 1024UL;
constexpr uint64_t kMemoryHostFeatureMapLogicBase = 68719476736U; // 64UL * 1024UL * 1024UL * 1024UL;
constexpr uint64_t kMemoryVarAddressSize = kMemoryHostFeatureMapLogicBase - kMemoryVarLogicBase;

class Om2ModelUtils {
 public:
  static Status ResolveInputAddrs(const TaskSemanticContributeContext &context,
                                  std::vector<AddrSemantic> &input_addrs);

  static Status ResolveOutputAddrs(const TaskSemanticContributeContext &context, const bool has_optional_addr,
                                   std::vector<AddrSemantic> &output_addrs);

  static Status ResolveWorkspaceAddrs(const TaskSemanticContributeContext &context,
                                      std::vector<AddrSemantic> &workspace_addrs);

  static Status BuildInputTensorInfo(const GeTensorDescPtr &tensor_desc, Om2TensorInfo &tensor_info);

  static Status BuildOutputTensorInfo(const GeTensorDescPtr &tensor_desc, Om2TensorInfo &tensor_info);

  static Status GetRtAddress(const TaskSemanticContributeContext &context, const uintptr_t logic_addr,
                                    AddrSemantic &addr_node, bool isInput, uint32_t index);

  static uint32_t ArgsSizeAlign8(uint32_t args_size);
  static uint64_t ArgsSizeAlign8(uint64_t args_size);

 private:
  struct WorkspaceMemAttrs {
    vector_bit_t reuse_flag;
    bool has_reuse_flag{false};
    std::vector<int64_t> v_memory_type;
    bool has_mem_type_attr{false};
    std::vector<int64_t> workspace_memory_type;
    bool has_mem_type_workspace{false};
    std::vector<int32_t> no_reuse_scope;
    bool has_no_reuse_scope{false};
  };
  static uint64_t GetWorkspaceMemTypeByPriority(const bool is_p2p_memory, const bool is_l1_memory,
                                                const bool is_ub_memory, const bool session_scope_memory);
  static bool ValidateMemRange(const ConstOpDescPtr &op_desc, const uint64_t total_size, const int64_t offset,
                               const int64_t size);
  static Status GetValidatedTensorMemType(const GeTensorDescPtr &tensor_desc, const std::vector<int64_t> &mem_types,
                                          size_t index, uint64_t &memory_type);
  static Status ResolveInputVarName(std::unordered_map<int64_t, OpInputEdges> &op_id_to_input_edges,
                                    const ConstOpDescPtr &op_desc, const int64_t op_index, const size_t input_idx,
                                    const size_t non_const_idx, std::string &input_ptr_name,
                                    bool &is_reused_from_upstream);
  static bool GetFileConstInputVarName(const int64_t input_offset,
                                       const std::unordered_map<int64_t, std::string> &fileconst_output_offset_to_varname,
                                       std::string &input_ptr_name);
  static Status ConstructAddrSemanticForInputConst(
                const TaskSemanticContributeContext &context, AddrSemantic &input_addr,
                const GeTensorDescPtr &tensor_desc, size_t index);
  static Status ConstructAddrSemanticForCommon(
                const TaskSemanticContributeContext &context, AddrSemantic &input_addr,
                const GeTensorDescPtr &tensor_desc, size_t &input_offset_index,
                const std::vector<int64_t> &input_offsets, size_t index);
  static Status ConstructOutputAddrForCommon(
                const TaskSemanticContributeContext &context, AddrSemantic &output_addr,
                const GeTensorDescPtr &tensor_desc, std::vector<int64_t> &v_memory_type,
                const std::vector<int64_t> &v_output_offset, size_t index);
  static Status GetRtInputAddress(const TaskSemanticContributeContext &context,
                                  const int64_t logical_offset, AddrSemantic &addr_node, uint32_t index);
  static Status GetRtOutputAddress(const TaskSemanticContributeContext &context,
                                   const int64_t logical_offset, AddrSemantic &addr_node, uint32_t index);
  static Status GetRtFmAddress(const TaskSemanticContributeContext &context,
                               const int64_t logical_offset,
                               AddrSemantic &addr_node, bool is_input, uint32_t index);
  static Status GetRtWeightAddress(const TaskSemanticContributeContext &context,
                                   const int64_t logical_offset,
                                   AddrSemantic &addr_node, uint32_t index);
  static Status GetRtVarAddress(const TaskSemanticContributeContext &context,
                                const uintptr_t logic_addr,
                                AddrSemantic &addr_node);
  static Status GetRtUnknownAddress(const TaskSemanticContributeContext &context,
                                    const uintptr_t logic_addr);
  static Status GetRtEmptyAddress(const TaskSemanticContributeContext &context,
                                  AddrSemantic &addr_node, bool is_input, uint32_t index);
  static Status CollectWorkspaceMemAttrs(const ConstOpDescPtr &op_desc, WorkspaceMemAttrs &attrs);
  static Status ConstructWorkspaceAddr(const TaskSemanticContributeContext &context,
                                       const WorkspaceMemAttrs &attrs,
                                       const std::vector<int64_t> &v_workspace_offset,
                                       const std::vector<int64_t> &v_workspace_bytes,
                                       size_t index, AddrSemantic &workspace_addr);
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_OM2_MODEL_UTILS_H
