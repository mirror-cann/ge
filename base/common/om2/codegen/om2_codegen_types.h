/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_TYPES_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_TYPES_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/opskernel/ops_kernel_info_types.h"
#include "common/om2/codegen/ast/ast_nodes.h"
#include "ast/ast_context.h"
#include "framework/common/taskdown_common.h"
#include "proto/task.pb.h"
#include "common/math/math_util.h"

namespace ge {
constexpr int64_t kInvalidOpIndex = -1;
constexpr int64_t kInvalidOpId = -1;
constexpr int32_t kInvalidAnchorIndex = -1;
constexpr uint64_t kSessionScopeMemoryMask = 0x100000000UL;
constexpr int32_t kWidthPerChar = 3;  // 转八进制字符串时每个字符的宽度

struct OpInputEdges {
  std::vector<int64_t> input_op_ids;
  std::vector<int32_t> input_anchor_indices;
  std::vector<std::string> output_var_names;
};

enum class Om2MemoryAppType : int32_t {
  kMemoryTypeFix,  // const and var and fix fm
  kMemoryTypeFeatureMap,
  kMemoryTypeModelIo,
  kEnd
};

struct MemInfo {
  int64_t logic_memory_base;
  int64_t memory_size;
  uint8_t *memory_base;
  uint64_t memory_type;
  std::string memory_key;
  bool is_fixed_addr_prior;
  MemInfo() : MemInfo(0, 0, nullptr, false) {}

  MemInfo(int64_t logic_memory_base_tmp, int64_t memory_size_tmp, uint8_t *const memory_base_tmp,
          bool is_fixed_addr_prior_tmp = false)
      : logic_memory_base(logic_memory_base_tmp),
        memory_size(memory_size_tmp),
        memory_base(memory_base_tmp),
        memory_type(RT_MEMORY_HBM),
        is_fixed_addr_prior(is_fixed_addr_prior_tmp) {}

  friend bool operator<(const MemInfo &left, const MemInfo &right) noexcept {
    // 加上大小是为了地址段匹配处理，不要擅自修改
    // 如logic_memory_base=0, memory_size=100, logic_memory_base=100, memory_size=300
    // logic_addr为[0, 100)匹配到的基地址就是logic_memory_base=0
    // logic_addr为[100, 400)匹配到的基地址就是logic_memory_base=100
    return (left.logic_memory_base + left.memory_size) < (right.logic_memory_base + right.memory_size);
  }

  std::string ToString() const {
    std::stringstream ss;
    ss << "memory_size:" << memory_size << ", logic_memory_base:" << logic_memory_base << ", memory_base:0x"
       << &std::hex << PtrToValue(PtrToPtr<uint8_t, void>(memory_base)) << ", memory_type:" << memory_type
       << ", memory_key:" << memory_key << ", is_fixed_addr_prior:" << is_fixed_addr_prior;
    return ss.str();
  }

  void *GetMemory(const int64_t offset, const int64_t bytes) const {
    if (bytes <= 0) {
      return nullptr;
    }
    GE_CHK_STATUS_EXEC(CheckInt64SubOverflow(offset, logic_memory_base), return nullptr,
                       "[Get][Memory] failed,Out of range, total size:%" PRId64 ", offset:%" PRId64
                       ", logic_memory_base:%" PRId64 ".",
                       memory_size, offset, logic_memory_base);
    const int64_t real_offset = offset - logic_memory_base;

    GE_CHK_STATUS_EXEC(CheckInt64AddOverflow(real_offset, bytes), return nullptr,
                       "[Get][Memory] failed,Out of range, total size:%" PRId64 ", offset:%" PRId64 ", bytes:%" PRId64
                       ".",
                       memory_size, real_offset, bytes);

    if ((real_offset + bytes) <= memory_size) {
      return ValueToPtr(PtrToValue(memory_base) + static_cast<uint64_t>(real_offset));
    }

    REPORT_INNER_ERR_MSG("E19999",
                         "Out of range, total size:%" PRId64 ", offset:%" PRId64
                         ", bytes:"
                         "%" PRId64 ".",
                         memory_size, real_offset, bytes);
    GELOGE(OUT_OF_MEMORY, "Out of range, total size:%" PRId64 ", offset:%" PRId64 ", bytes:%" PRId64 ".", memory_size,
           real_offset, bytes);
    return nullptr;
  }
};

struct RuntimeResourceSemantic {
  uint64_t total_mem_size{0U};
  uint64_t total_weight_size{0U};
  uint32_t stream_num{0U};
  uint32_t notify_num{0U};
  uint32_t event_num{0U};
  uint32_t label_num{0U};
  uint32_t kernel_bin_num{0U};
  uint64_t logic_mem_base{0U};
  uint64_t logic_weight_base{0U};
  uint64_t logic_var_base{0U};
  uint64_t var_size{0U};
  std::map<uint64_t, MemInfo> memory_infos;
  bool has_label_switch{false};
  bool has_label_goto{false};
  std::vector<std::string> stream_flag_values;
  std::vector<std::string> bind_flag_values;
};

struct ModelIoEntry {
  uint32_t index{0U};
  int64_t memory_offset{0};
  uint32_t update_host_args_index{0U};
  bool is_input{true};
  bool is_addr_refreshable{true};
  int64_t addr_offset{0};
};

struct ModelIoSemantic {
  std::vector<ModelIoEntry> entries;
  std::set<int64_t> io_offsets;
  uint32_t input_count{0U};
  uint32_t output_count{0U};
};

struct Om2TensorInfo {
  uint64_t args_offset{0U};
  uint64_t size{0U};
  int32_t data_type{0};
  int32_t format{0};
  std::vector<int64_t> shape_dims;
};

struct ConstInputEntry {
  size_t const_index{0U};
  std::string var_name;
  Om2TensorInfo tensor_info;
};

struct Om2ConstMeta {
  size_t index = 0U;
  std::string type;
  std::string file_name;
  std::string file_path;
  int64_t offset = 0;
  int64_t size = 0;
  std::string op_name;
};

using Om2ConstMetas = std::vector<Om2ConstMeta>;

struct Om2CodegenArtifact {
  std::string file_name;
  std::string data;
};

using Om2CodegenArtifacts = std::vector<Om2CodegenArtifact>;

enum class KernelBinaryKind : int32_t {
  kAicore,
  kAicpu,
  kCustAicpu,
  kAllKernel,
};

struct KernelBinaryRecord {
  KernelBinaryKind kind{KernelBinaryKind::kAicore};
  std::string kernel_name;
  std::string file_name;
  std::string op_type;
  std::string so_name;
  std::string op_kernel_lib;
  std::string magic;
  uint64_t tiling_key{0U};
  uint32_t func_handle_index{0U};
};

struct KernelRegistrySemantic {
  std::vector<KernelBinaryRecord> binaries;
  std::unordered_map<std::string, uint32_t> func_handle_indices;
};

struct LaunchConfigSemantic {
  uint8_t schedule_mode{0U};
  uint32_t local_memory_size{0U};
  std::string engine_type{"ACL_RT_ENGINE_TYPE_AIC"};
  uint32_t block_dim_offset{0U};
  bool is_block_task_prefetch{false};
  uint8_t is_data_dump{0U};
  uint16_t time_out{0U};
};

struct LaunchCallSemantic {
  uint32_t block_dim{0U};
  uint32_t stream_id{0U};
  LaunchConfigSemantic config;
  uint32_t func_handle_index{0U};
  uint32_t tf_session_func_handle_index{0U};
};

enum class AddrValueKind : int32_t {
  kInputInstance,
  kOutputInstance,
  kWorkspace,
  kConstTensor,
  kCustomValue,
  kPlaceholder,
  kLevel1DescPtr,
  kShapeInfoBuffer,
  kOptionalEmpty,
  kFftsAddr,
  kEventAddr,
  kOverflowAddr,
  kTiling,
  kEmptyAddr,
};

enum class MemoryAppType : int32_t {
  kFix,
  kFeatureMap,
  kModelIo,
};

struct AddrSemantic {
  AddrValueKind kind{AddrValueKind::kInputInstance};
  MemoryAppType memory_app{MemoryAppType::kFix};
  std::string symbol_hint;
  int64_t mem_offset{0};
  uint64_t byte_size{0U};
  uint64_t custom_value{0U};
  uint32_t event_id{0U};
  std::optional<size_t> const_index;
  int64_t compile_state_io_addr_offset{0};
  bool is_reused_from_upstream{false};
  std::optional<std::vector<int64_t>> shape_info;
  std::optional<uint64_t> level1_target_offset;
  uint64_t memory_type{RT_MEMORY_HBM};
  std::optional<Om2TensorInfo> tensor_info;
};

struct ArgsTableEntrySemantic {
  uint64_t table_index{0U};
  uint64_t args_size{0U};
  uint64_t host_offset{0U};
};

struct AicpuArgsSemantic {
  std::vector<uint8_t> args_buffer;
  uint32_t args_size{0U};
};

struct AicpuExtInfoSemantic {
  size_t total_len{0UL};
  int32_t session_info_offset{-1};
  std::vector<uint8_t> serialized_bytes;
};

struct KernelTaskSemantic {
  ModelTaskType task_type{ModelTaskType::MODEL_TASK_KERNEL};
  ccKernelType kernel_type{ccKernelType::INVALID};
  uint64_t tiling_key{0U};
  LaunchCallSemantic launch;
  std::vector<AddrSemantic> input_addrs;
  std::vector<AddrSemantic> output_addrs;
  std::vector<AddrSemantic> workspace_addrs;
  std::vector<AddrSemantic> ordered_arg_values;
  std::optional<ArgsTableEntrySemantic> args_table_entry;
  uint64_t aicpu_task_index{0U};
  std::optional<AicpuArgsSemantic> aicpu_args;
  std::optional<AicpuExtInfoSemantic> aicpu_ext_info;
};

struct TaskSemanticHeader {
  int64_t op_index{kInvalidOpIndex};
  int64_t op_desc_id{kInvalidOpId};
  std::string op_name;
  std::string op_type;
  uint32_t stream_id{0U};
};

struct ArgsTableSemantic {
  std::vector<ArgsTableEntrySemantic> entries;
  uint64_t total_host_args_len{0UL};
  std::vector<std::vector<uint64_t>> host_args_offsets;
};

struct Om2CodegenModel {
  std::string model_name;
  RuntimeResourceSemantic runtime;
  ModelIoSemantic model_io;
  KernelRegistrySemantic kernel_registry;
  ArgsTableSemantic args_table;
  std::vector<ConstInputEntry> const_inputs;
  uint32_t aicpu_task_count{0U};
};

}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TYPES_H_
