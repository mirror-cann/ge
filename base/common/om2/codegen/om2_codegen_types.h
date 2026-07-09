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
#include "graph/op_desc.h"
#include "proto/task.pb.h"
#include "common/math/ge_math_util.h"

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

struct OpDispatchType {
  enum Value : uint32_t {
    DISPATCH_AICORE = 0,
    DISPATCH_AICPU = 1,
    DISPATCH_EVENT_RECORD = 2,
    DISPATCH_EVENT_WAIT = 3,
    DISPATCH_FUSION_START = 4,
    DISPATCH_FUSION_END = 5,
    DISPATCH_END_GRAPH = 6,
    DISPATCH_LABEL_SET = 7,
    DISPATCH_NOTIFY_RECORD = 8,
    DISPATCH_NOTIFY_WAIT = 9,
    DISPATCH_STREAM_ACTIVE = 10,
    DISPATCH_STREAM_SWITCH = 11,
    DISPATCH_LABEL_GOTO_EX = 12,
    DISPATCH_LABEL_SWITCH_BY_INDEX = 13,
    DISPATCH_BARRIER = 14,
    DISPATCH_CMO = 15,
    DISPATCH_MEMCPY_ASYNC = 16,
    DISPATCH_MEMCPY_ADDR_ASYNC = 17,
    DISPATCH_CMO_ADDR = 18,
    DISPATCH_DSA = 19,
    DISPATCH_KERNEL_EX = 20,
  };

  static std::string ToString() {
    static const char *kNames[] = {"DISPATCH_AICORE",        "DISPATCH_AICPU",
                                   "DISPATCH_EVENT_RECORD",  "DISPATCH_EVENT_WAIT",
                                   "DISPATCH_FUSION_START",  "DISPATCH_FUSION_END",
                                   "DISPATCH_END_GRAPH",     "DISPATCH_LABEL_SET",
                                   "DISPATCH_NOTIFY_RECORD", "DISPATCH_NOTIFY_WAIT",
                                   "DISPATCH_STREAM_ACTIVE", "DISPATCH_STREAM_SWITCH",
                                   "DISPATCH_LABEL_GOTO_EX", "DISPATCH_LABEL_SWITCH_BY_INDEX",
                                   "DISPATCH_BARRIER",       "DISPATCH_CMO",
                                   "DISPATCH_MEMCPY_ASYNC",  "DISPATCH_MEMCPY_ADDR_ASYNC",
                                   "DISPATCH_CMO_ADDR",      "DISPATCH_DSA",
                                   "DISPATCH_KERNEL_EX"};
    std::string code = "enum OpDispatchType : uint32_t {\n";
    for (uint32_t i = 0U; i <= static_cast<uint32_t>(DISPATCH_KERNEL_EX); i++) {
      code += "    " + std::string(kNames[i]) + " = " + std::to_string(i) + ",\n";
    }
    code += "    DISPATCH_TYPE_COUNT\n};\n";
    return code;
  }
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

// shape 维度数的上限（与 OpArgInfo.tensor.shape 数组大小一致）
constexpr size_t kShapeMaxDims = 8U;

// OpArgType 枚举 - 与 generated code 中的 OpArgType 保持一致
enum OpArgType : int32_t {
  OP_ARG_INPUT = 0,
  OP_ARG_OUTPUT = 1,
  OP_ARG_WORKSPACE = 2,
  OP_ARG_CONST_TENSOR = 3,
  OP_ARG_LEVEL1_DESC = 4,
  OP_ARG_SHAPE_INFO = 5,
  OP_ARG_CUSTOM_VALUE = 6,
  OP_ARG_PLACEHOLDER = 7,
  OP_ARG_OPTIONAL_EMPTY = 8,
  OP_ARG_FFTS_ADDR = 9,
  OP_ARG_EVENT_ADDR = 10,
  OP_ARG_OVERFLOW_ADDR = 11,
  OP_ARG_TILING = 12,
  OP_ARG_RAW_ADDR = 13,
};

// 算子参数构建数据 —— TaskCodeBuilder 子类填充后，由 BuildOpArgList() 转换为 OpArgInfo 数组 AST
struct OpArgDesc {
  int32_t type{0};                       // OpArgType 枚举值（INPUT/OUTPUT/WORKSPACE/TILING/...）
  uint32_t mem_src{0U};                  // 内存来源（0=设备内存，0xFFFFFFFF=session，≥1=常量数组索引）
  uint64_t offset{0U};                   // 内存偏移量
  uint64_t size{0U};                     // 数据大小（字节），INPUT/OUTPUT/WORKSPACE 使用
  int32_t data_type{0};                  // 数据类型（INPUT/OUTPUT）
  int32_t format{0};                     // 数据格式（INPUT/OUTPUT）
  std::vector<int64_t> shape_dims;       // shape 维度数组（INPUT/OUTPUT）
  bool has_tensor_info{false};           // 是否有 tensor 元数据（无则不生成 data 字段）
  uint64_t custom_value{0U};             // 自定义值（LEVEL1_DESC/SHAPE_INFO/CUSTOM_VALUE/EVENT_ADDR）
  std::vector<uint8_t> raw_data;         // Tiling 原始二进制数据
  std::vector<int64_t> shape_info_data;  // Shape 信息数据
  uint64_t args_offset{UINT64_MAX};      // args table 内字节偏移
};

struct IoAddrRefreshRecord {
  uint64_t compile_state_io_addr_offset{0U};
  uint64_t host_offset{0U};
};

struct CustomValueEntry {
  uint64_t host_offset;  // 编译期确定的 offset（含 align_offset）
  uint64_t value;        // 编译期确定的写入值
  uint32_t size;         // 4（uint32_t）或 8（uint64_t），编译期确定
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

}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TYPES_H_
