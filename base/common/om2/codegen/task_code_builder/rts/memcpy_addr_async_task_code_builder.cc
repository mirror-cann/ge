/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "memcpy_addr_async_task_code_builder.h"

#include <cinttypes>

#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"

namespace {
constexpr uint64_t kAlignment = 64U;
}

namespace ge {
void MemcpyAddrAsyncTaskCodeBuilder::AppendOrderedArgValue(const AddrSemantic &semantic,
                                                           uint64_t current_host_offset) {
  if (semantic.memory_app == MemoryAppType::kModelIo) {
    io_addr_refresh_records_.push_back(
        IoAddrRefreshRecord{static_cast<uint64_t>(semantic.compile_state_io_addr_offset), current_host_offset});
    GELOGI("[OM2]append addr offset map: compile offset[%lu], args info offset[%lu]",
           static_cast<uint64_t>(semantic.compile_state_io_addr_offset), current_host_offset);
  }
  ordered_arg_values_.push_back(semantic);
}

Status MemcpyAddrAsyncTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.op_index_to_count_map);
  const domi::MemcpyAsyncDef &memcpy_async = context.task_def.memcpy_async();

  ResolveInternalIndex(context);

  args_format_str_ = memcpy_async.args_format().empty() ?
                      "{}{}{i_instance0*}{o_instance0*}" :
                      memcpy_async.args_format();
  GE_ASSERT_SUCCESS(ArgsFormatDesc::Parse(context.op_desc, args_format_str_, arg_descs_));

  uint64_t logical_src_mem_type = 0U;
  uint64_t logical_dst_mem_type = 0U;
  AddrSemantic src_addr_node;
  AddrSemantic dst_addr_node;
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(memcpy_async.src()),
                                                logical_src_mem_type, src_addr_node, true, internal_index_));
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(memcpy_async.dst()),
                                                logical_dst_mem_type, dst_addr_node, false, internal_index_));

  GE_ASSERT_SUCCESS(CalcArgSizes(context));

  dst_max_ = memcpy_async.dst_max();
  count_ = memcpy_async.count();
  kind_ = memcpy_async.kind();

  GE_ASSERT_SUCCESS(BuildOrderedArgs(context, src_addr_node, dst_addr_node));

  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] stream list size:%u, cur:%u!", context.runtime->stream_num, header_.stream_id);
  GELOGI("Memcpy Addr Async Task Codegen: op[%s], dst max[%" PRIu64 "], count[%" PRIu64
         "], kind[%u], stream id[%u].",
         context.op_desc->GetName().c_str(), dst_max_, count_, kind_, header_.stream_id);
  GELOGI("op_index %u, op_id %" PRId64, memcpy_async.op_index(), context.op_desc->GetId());
  return SUCCESS;
}

void MemcpyAddrAsyncTaskCodeBuilder::ResolveInternalIndex(TaskSemanticContributeContext &context) {
  auto it = context.op_index_to_count_map->find(header_.op_index);
  if (it == context.op_index_to_count_map->end()) {
    internal_index_ = 0;
    (*context.op_index_to_count_map)[header_.op_index] = 1;
  } else {
    internal_index_ = it->second;
    ++(*context.op_index_to_count_map)[header_.op_index];
  }
}

Status MemcpyAddrAsyncTaskCodeBuilder::CalcArgSizes(const TaskSemanticContributeContext &context) {
  args_size_ = 0U;
  arg_sizes_.clear();
  for (const auto &arg_desc : arg_descs_) {
    size_t arg_size = 0U;
    GE_ASSERT_SUCCESS(ArgsFormatDesc::GetArgSize(context.op_desc, arg_desc, arg_size));
    args_size_ += arg_size;
    arg_sizes_.push_back(arg_size);
  }
  return SUCCESS;
}

Status MemcpyAddrAsyncTaskCodeBuilder::BuildOrderedArgs(TaskSemanticContributeContext &context,
                                                        const AddrSemantic &src_addr_node,
                                                        const AddrSemantic &dst_addr_node) {
  const uint64_t current_host_offset = *context.next_host_args_offset;
  align_offset_ = (current_host_offset + kAlignment - 1) / kAlignment * kAlignment - current_host_offset;

  // 首个IO地址的偏移量将被记录，其与align_offset之和作为后续地址刷新的总偏移
  // + ------------ + --------- + ------- + --- +
  // | align_offset | io_offset | src|dst | ... |
  // + ------------ + --------- + ------- + --- +
  // |<-  aligned_io_offset_  ->|
  bool io_encountered = false;
  size_t io_offset = 0;
  for (const auto &arg_desc : arg_descs_) {
    if ((arg_desc.addr_type == AddrType::INPUT_INSTANCE || arg_desc.addr_type == AddrType::OUTPUT_INSTANCE) &&
        !io_encountered) {
      GELOGI("align_offset: %zu, io_offset: %zu", align_offset_, io_offset);
      aligned_io_offset_ = align_offset_ + io_offset;
      io_encountered = true;
    }
    if (arg_desc.addr_type == AddrType::INPUT_INSTANCE) {
      AppendOrderedArgValue(src_addr_node, current_host_offset + align_offset_ + io_offset);
    } else if (arg_desc.addr_type == AddrType::OUTPUT_INSTANCE) {
      AppendOrderedArgValue(dst_addr_node, current_host_offset + align_offset_ + io_offset);
    }
    GE_ASSERT_SUCCESS(ArgsFormatDesc::GetArgSize(context.op_desc, arg_desc, io_offset));
  }

  entry_.table_index = *context.next_args_table_index;
  entry_.args_size = align_offset_ + io_offset;
  entry_.host_offset = *context.next_host_args_offset;
  args_table_entry_ = &entry_;
  ++(*context.next_args_table_index);
  *context.next_host_args_offset += Om2ModelUtils::ArgsSizeAlign8(entry_.args_size);
  return SUCCESS;
}

Status MemcpyAddrAsyncTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(ast_.Comment("============================= " + header_.op_name + " ==============================="));

  std::vector<Arg> args_vars;
  CollectIoAddrVars(items, args_vars);

  const std::string ioaddr_var_name = "op" + std::to_string(header_.op_index) +
                                      "_iow_addr" + std::to_string(internal_index_);
  auto ioaddr_var = ast_.Var("std::vector<uint64_t>", ioaddr_var_name);
  items.emplace_back(ast_.VarDecl(ioaddr_var, FlattenHostArgs(args_vars)));
  items.push_back(ChkStatus(MemcpyS(
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_.table_index)).Arrow("host_addr") + aligned_io_offset_,
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_.table_index)).Arrow("size") - aligned_io_offset_,
      ioaddr_var.Data(), ioaddr_var.Size() * ast_.Sizeof("uint64_t"))));
  items.push_back(ChkStatus(ast_.Call("KernelMemcpyAddrAsyncDistribute", {
      ast_.Str(header_.op_name),
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_.table_index)).Arrow("dev_addr") + align_offset_,
      ast_.UInt(dst_max_),
      ast_.UInt(count_),
      ast_.StaticCast("rtMemcpyKind_t", static_cast<int64_t>(kind_)),
      stream_list_[static_cast<int>(header_.stream_id)],
      0})));
  items.push_back(ChkStatus(AclrtMemcpy(
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_.table_index)).Arrow("host_addr") + align_offset_,
      args_size_,
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_.table_index)).Arrow("dev_addr") + align_offset_,
      args_size_,
      "ACL_MEMCPY_DEVICE_TO_HOST")));
  // 与 TaskInfo::Distribute() 对称：回拷后遍历 arg_descs_ 写入 CUSTOM_VALUE
  RenderCustomValueWriteback(items);
  return SUCCESS;
}

void MemcpyAddrAsyncTaskCodeBuilder::CollectIoAddrVars(std::vector<BodyItem> &items,
                                                       std::vector<Arg> &args_vars) {
  for (const auto &semantic : ordered_arg_values_) {
    switch (semantic.kind) {
      case AddrValueKind::kInputInstance:
      case AddrValueKind::kOutputInstance:
        if (!semantic.is_reused_from_upstream) {
          if (!semantic.tensor_info.has_value()) {
            GELOGE(FAILED, "[OM2] MemcpyAddrAsync tensor info is required for %s.",
                   semantic.symbol_hint.c_str());
            return;
          }
          const auto &tensor_info = *semantic.tensor_info;
          const std::string shape_var_name = semantic.symbol_hint + "_shape";
          items.push_back(
              ast_.VarDecl("std::vector<int64_t>", shape_var_name, ast_.InitList(ConvertToArgs(tensor_info.shape_dims))));
          items.push_back(ast_.VarDecl("Om2Tensor", semantic.symbol_hint, ast_.Call("BuildOm2Tensor", {
              GetAddr(total_dev_mem_ptr_, semantic.mem_offset),
              ast_.ULong(tensor_info.size),
              tensor_info.data_type,
              tensor_info.format,
              ast_.Var("std::vector<int64_t>", shape_var_name)})));
        }
        args_vars.emplace_back(ast_.Var("auto", semantic.symbol_hint));
        break;
      default:
        break;
    }
  }
}

void MemcpyAddrAsyncTaskCodeBuilder::RenderCustomValueWriteback(std::vector<BodyItem> &items) {
  size_t host_offset = align_offset_;
  for (size_t i = 0; i < arg_descs_.size(); ++i) {
    if (arg_descs_[i].addr_type == AddrType::CUSTOM_VALUE) {
      const uint64_t value = *reinterpret_cast<const uint64_t *>(arg_descs_[i].reserved);
      auto host_base = args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_.table_index)).Arrow("host_addr");
      if (arg_descs_[i].ir_idx == static_cast<int32_t>(ArgsFormatWidth::BIT32)) {
        auto target_ptr = ast_.ReinterpretCast("uint32_t *", host_base + host_offset);
        items.push_back(ast_.Assign(ast_.Deref(target_ptr), ast_.StaticCast("uint32_t", value)));
      } else {
        auto target_ptr = ast_.ReinterpretCast("uint64_t *", host_base + host_offset);
        items.push_back(ast_.Assign(ast_.Deref(target_ptr), ast_.UInt(value)));
      }
    }
    host_offset += arg_sizes_[i];
  }
}

Status MemcpyAddrAsyncTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto src_addr = ast_.Var("void *", "src_addr");
  auto dst_max = ast_.Var("uint64_t", "dst_max");
  auto count = ast_.Var("uint64_t", "count");
  auto kind = ast_.Var("const rtMemcpyKind_t", "kind");
  auto stream = ast_.Var("aclrtStream &", "stream");
  auto qos_cfg = ast_.Var("const uint32_t", "qos_cfg");
  items.push_back(ast_.DefineFunction("KernelMemcpyAddrAsyncDistribute",
      {op_name, src_addr, dst_max, count, kind, stream, qos_cfg}, "aclError", {
          ChkRt(RtSetTaskTag(op_name)),
          ChkRt(ast_.Call("rtMemcpyAsyncPtr", {
              src_addr,
              dst_max,
              count,
              kind,
              stream,
              qos_cfg})),
          ast_.Return("ACL_SUCCESS"),
      }));
  return SUCCESS;
}

int64_t MemcpyAddrAsyncTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::MemcpyAsyncDef &memcpy_async = task_def.memcpy_async();
  return static_cast<int64_t>(memcpy_async.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_MEMCPY_ADDR_ASYNC, MemcpyAddrAsyncTaskCodeBuilder);
}  // namespace ge
