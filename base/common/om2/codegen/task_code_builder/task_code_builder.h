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

#include <variant>
#include <utility>
#include <sstream>
#include <iomanip>

#include "common/om2/codegen/ast/ast_build_context.h"
#include "common/om2/codegen/ast/ast_nodes.h"
#include "common/om2/codegen/om2_model_class_generator_base.h"
#include "common/om2/codegen/om2_codegen_types.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "graph/op_desc.h"
#include "proto/task.pb.h"

namespace ge {

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


struct OpArgBuildData {
  int32_t type{0};
  uint32_t mem_src{0U};              // 内存来源（0=设备内存，0xFFFFFFFF=session，≥1=常量数组索引）
  uint64_t offset{0U};
  uint64_t size{0U};
  int32_t data_type{0};
  int32_t format{0};
  std::vector<int64_t> shape_dims;
  uint64_t custom_value{0U};
  std::vector<uint8_t> raw_data;
  std::vector<int64_t> shape_info_data;
  uint64_t args_offset{UINT64_MAX};  // args table 内字节偏移
};

// AICORE 专属（原公共字段全部下沉）
struct AicoreTaskData {
  uint32_t kernel_type{0U};  // 0=AICORE
  uint32_t args_idx{0U};
  uint32_t func_idx{0U};
  uint32_t block_dim{0U};
  uint8_t schedule_mode{0U};
  uint32_t engine_type{0U};
  uint32_t block_dim_offset{0U};
  bool is_block_task_prefetch{false};
  uint16_t time_out{0U};
  uint32_t local_memory_size{0U};
  uint32_t task_type{0U};
  uint32_t need_assert_or_printf{0U};
  std::vector<AddrSemantic> ordered_arg_values;  // 供 BuildL0ArgSlotEntries 使用
  uint32_t num_io_addrs{0U};  // ordered_args 中地址条目数（blob 条目紧随其后）
};

// OpDefBuildData 只持有一个 variant，零公共字段
struct OpDefBuildData {
  std::vector<OpArgBuildData> ordered_args;  // 参数列表（多个 task 类型需要地址解析）
  std::variant<
    std::monostate,
    AicoreTaskData            // KERNEL (AICORE)
  > task_specific;
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

class TaskCodeBuilder : public Om2ModelClassGeneratorBase {
 public:
  using Om2ModelClassGeneratorBase::Om2ModelClassGeneratorBase;
  ~TaskCodeBuilder() override = default;

  virtual int64_t ParseOpIndex(const domi::TaskDef &task_def) {
    (void)task_def;
    return kInvalidOpIndex;
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

  // SupportsTableDriven: 判断当前 task 是否支持表驱动模式
  virtual bool SupportsTableDriven() const {
    return false;  // 默认不支持
  }

  virtual Status RenderDispatchOpCaseBody(std::vector<BodyItem> &items) {
    (void)items;
    return SUCCESS;
  }

  // 生成完整的 OpDef 表项（公共顶层字段 + task_data union）
  // 默认实现生成公共顶层字段（NOP 类型用此即可），各 builder 重写后调用基类再加 task_data
  virtual Status RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields,
                                        uint32_t dispatch_type) {
    const auto &header = GetHeader();
    const auto &opdef_data = GetOpDefBuildData();
    fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(dispatch_type))});
    fields.push_back({"op_name", Arg::StringLiteral(header.op_name)});
    fields.push_back({"argsInfo", BuildOpArgList(opdef_data.ordered_args)});
    return SUCCESS;
  }

  virtual ModelTaskType GetTaskType() const {
    return task_type_;
  }

  const TaskSemanticHeader &GetHeader() const {
    return header_;
  }

  const ArgsTableEntrySemantic *GetArgsTableEntry() const {
    return args_table_entry_;
  }

  const std::vector<IoAddrRefreshRecord> &GetIoAddrRefreshRecords() const {
    return io_addr_refresh_records_;
  }

  virtual OpDefBuildData GetOpDefBuildData() const {
    return OpDefBuildData{};
  }

 protected:
  static void FillTaskSemanticHeader(const TaskSemanticContributeContext &context, TaskSemanticHeader &header) {
    header.op_index = context.op_index;
    header.stream_id = context.task_def.stream_id();
    if (context.op_desc != nullptr) {
      header.op_desc_id = context.op_desc->GetId();
      header.op_name = context.op_desc->GetName();
      header.op_type = context.op_desc->GetType();
    }
  }

  static std::vector<Arg> ConvertToArgs(const std::vector<int64_t> &values) {
    std::vector<Arg> args;
    args.reserve(values.size());
    for (const int64_t value : values) {
      (void)args.emplace_back(value);
    }
    return args;
  }

  // 将 OpArgBuildData 列表转换为 OpArgInfo 数组的 AST 表达式（表驱动优化）
  Arg BuildOpArgList(const std::vector<OpArgBuildData> &args) const {
    if (args.empty()) {
      return Arg(nullptr);
    }

    using DataBuilder = Arg (TaskCodeBuilder::*)(const OpArgBuildData &) const;
    static const std::unordered_map<int32_t, DataBuilder> kDataBuilders = {
      {OP_ARG_INPUT,        &TaskCodeBuilder::BuildTensorDataField},
      {OP_ARG_OUTPUT,       &TaskCodeBuilder::BuildTensorDataField},
      {OP_ARG_WORKSPACE,    &TaskCodeBuilder::BuildWorkspaceDataField},
      {OP_ARG_CONST_TENSOR, &TaskCodeBuilder::BuildTensorDataField},
      {OP_ARG_LEVEL1_DESC,  &TaskCodeBuilder::BuildCustomValueDataField},
      {OP_ARG_SHAPE_INFO,   &TaskCodeBuilder::BuildCustomValueDataField},
      {OP_ARG_CUSTOM_VALUE, &TaskCodeBuilder::BuildCustomValueDataField},
      {OP_ARG_EVENT_ADDR,   &TaskCodeBuilder::BuildCustomValueDataField},
      {OP_ARG_TILING,       &TaskCodeBuilder::BuildTilingDataField},
    };

    std::vector<Arg> arg_entries;
    arg_entries.reserve(args.size());
    for (const auto &a : args) {
      std::vector<std::pair<std::string, Arg>> fields;
      fields.push_back({"type", a.type});

      const bool needs_addr = (a.type == OP_ARG_INPUT || a.type == OP_ARG_OUTPUT ||
                               a.type == OP_ARG_WORKSPACE || a.type == OP_ARG_CONST_TENSOR);
      if (needs_addr) {
        fields.push_back({"addr", BuildAddrField(a)});
      }

      const auto it = kDataBuilders.find(a.type);
      if (it != kDataBuilders.end()) {
        fields.push_back({"data", (this->*(it->second))(a)});
      }

      arg_entries.push_back(ast_.InitListWithDesignators(fields, true));
    }
    return ast_.CCast("const OpArgInfo[]", ast_.InitList(arg_entries, true));
  }

 private:
  // addr 子结构：{mem_src, offset}
  Arg BuildAddrField(const OpArgBuildData &a) const {
    return ast_.InitListWithDesignators(std::vector<std::pair<std::string, Arg>>{
        {"mem_src", a.mem_src},
        {"offset", static_cast<int64_t>(a.offset)},
    }, true);
  }

  // data.tensor（INPUT/OUTPUT/CONST_TENSOR）：{size, data_type, format, shape, num_shape_dims, args_offset}
  Arg BuildTensorDataField(const OpArgBuildData &a) const {
    std::vector<int64_t> padded_shape(kShapeMaxDims, 0);
    for (size_t i = 0; i < a.shape_dims.size() && i < kShapeMaxDims; ++i) {
      padded_shape[i] = a.shape_dims[i];
    }
    return ast_.InitListWithDesignators(
        std::vector<std::pair<std::string, Arg>>{
            {"tensor", ast_.InitListWithDesignators(
                std::vector<std::pair<std::string, Arg>>{
                    {"size", static_cast<int64_t>(a.size)},
                    {"data_type", a.data_type},
                    {"format", a.format},
                    {"shape", ast_.InitList(std::vector<Arg>(padded_shape.begin(), padded_shape.end()))},
                    {"num_shape_dims", static_cast<int64_t>(a.shape_dims.size())},
                    {"args_offset", static_cast<int64_t>(a.args_offset)},
                }, true)},
        }, true);
  }

  // data.tensor（WORKSPACE）：{size}
  Arg BuildWorkspaceDataField(const OpArgBuildData &a) const {
    return ast_.InitListWithDesignators(
        std::vector<std::pair<std::string, Arg>>{
            {"tensor", ast_.InitListWithDesignators(
                std::vector<std::pair<std::string, Arg>>{
                    {"size", static_cast<int64_t>(a.size)},
                }, true)},
        }, true);
  }

  // data.custom_value（LEVEL1_DESC / SHAPE_INFO / CUSTOM_VALUE / EVENT_ADDR）：{custom_value}
  Arg BuildCustomValueDataField(const OpArgBuildData &a) const {
    return ast_.InitListWithDesignators(
        std::vector<std::pair<std::string, Arg>>{
            {"custom_value", static_cast<int64_t>(a.custom_value)},
        }, true);
  }

  // data.tiling（TILING）：{raw_data, raw_data_len}
  Arg BuildTilingDataField(const OpArgBuildData &a) const {
    Arg raw_data_arg(nullptr);
    uint32_t raw_data_len = 0U;
    if (!a.raw_data.empty()) {
      std::ostringstream oss;
      for (const auto byte : a.raw_data) {
        oss << "\\" << std::oct << std::setw(kWidthPerChar) << std::setfill('0') << static_cast<int>(byte);
      }
      raw_data_arg = ast_.ReinterpretCast("const uint8_t *", Arg::StringLiteral(oss.str()));
      raw_data_len = static_cast<uint32_t>(a.raw_data.size());
    }
    return ast_.InitListWithDesignators(
        std::vector<std::pair<std::string, Arg>>{
            {"tiling", ast_.InitListWithDesignators(
                std::vector<std::pair<std::string, Arg>>{
                    {"raw_data", raw_data_arg},
                    {"raw_data_len", static_cast<int64_t>(raw_data_len)},
                }, true)},
        }, true);
  }

 protected:
  TaskSemanticHeader header_;
  const ArgsTableEntrySemantic *args_table_entry_{nullptr};
  std::vector<IoAddrRefreshRecord> io_addr_refresh_records_;
  ModelTaskType task_type_{ModelTaskType::MODEL_TASK_NOP};
};
}  // namespace ge

#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_TASK_CODE_BUILDER_TASK_CODE_BUILDER_H_
