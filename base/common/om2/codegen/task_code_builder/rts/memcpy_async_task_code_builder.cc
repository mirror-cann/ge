/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "memcpy_async_task_code_builder.h"

#include <cinttypes>

#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"

namespace ge {
constexpr uint64_t ioAddrArgSize = 16;

Status MemcpyAsyncTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.op_index_to_count_map);
  const domi::MemcpyAsyncDef &memcpy_async = context.task_def.memcpy_async();

  ResolveInternalIndex(context);

  uint64_t logical_src_mem_type = 0U;
  uint64_t logical_dst_mem_type = 0U;
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(memcpy_async.src()),
                                                logical_src_mem_type, input_addr_node_, true, internal_index_));
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(memcpy_async.dst()),
                                                logical_dst_mem_type, output_addr_node_, false, internal_index_));

  CheckIoRefresh(context);

  dst_max_ = memcpy_async.dst_max();
  count_ = memcpy_async.count();
  kind_ = memcpy_async.kind();

  if (io_refresh_) {
    SetupIoAddrRefresh(context);
    kind_ = RT_MEMCPY_ADDR_DEVICE_TO_DEVICE;
  }
  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] stream list size:%u, cur:%u!", context.runtime->stream_num, header_.stream_id);
  GELOGI("Memcpy Async Task Codegen: op[%s], dst max[%" PRIu64 "], count[%" PRIu64
         "], kind[%u], stream id[%u].",
         context.op_desc->GetName().c_str(), dst_max_, count_, kind_, header_.stream_id);
  GELOGI("op_index %u, op_id %" PRId64, memcpy_async.op_index(), context.op_desc->GetId());
  return SUCCESS;
}

void MemcpyAsyncTaskCodeBuilder::ResolveInternalIndex(TaskSemanticContributeContext &context) {
  auto it = context.op_index_to_count_map->find(header_.op_index);
  if (it == context.op_index_to_count_map->end()) {
    internal_index_ = 0;
    (*context.op_index_to_count_map)[header_.op_index] = 1;
  } else {
    internal_index_ = it->second;
    ++(*context.op_index_to_count_map)[header_.op_index];
  }
}

void MemcpyAsyncTaskCodeBuilder::CheckIoRefresh(TaskSemanticContributeContext &context) {
  for (auto &item : context.model_io->entries) {
    if (item.memory_offset == input_addr_node_.mem_offset || item.memory_offset == output_addr_node_.mem_offset) {
      io_refresh_ = true;
      GELOGI("memcpy async is linked to io. input offset[%ld], output offset[%ld]", input_addr_node_.mem_offset,
             output_addr_node_.mem_offset);
      break;
    }
  }
}

void MemcpyAsyncTaskCodeBuilder::SetupIoAddrRefresh(TaskSemanticContributeContext &context) {
  const uint64_t addr_offset = *context.next_host_args_offset;
  if (input_addr_node_.memory_app == MemoryAppType::kModelIo) {
    io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(input_addr_node_.compile_state_io_addr_offset), addr_offset});
    GELOGI("[OM2]append input addr offset map: compile offset[%lu], args info offset[%lu]",
           static_cast<uint64_t>(input_addr_node_.compile_state_io_addr_offset), addr_offset);
  }
  if (output_addr_node_.memory_app == MemoryAppType::kModelIo) {
    io_addr_refresh_records_.push_back(
          IoAddrRefreshRecord{static_cast<uint64_t>(output_addr_node_.compile_state_io_addr_offset),
                              addr_offset + sizeof(uint64_t)});
    GELOGI("[OM2]append output addr offset map: compile offset[%lu], args info offset[%lu]",
           static_cast<uint64_t>(output_addr_node_.compile_state_io_addr_offset), addr_offset + sizeof(uint64_t));
  }

  entry_.emplace();
  entry_->table_index = *context.next_args_table_index;
  entry_->args_size = ioAddrArgSize;
  entry_->host_offset = *context.next_host_args_offset;
  args_table_entry_ = &(*entry_);
  ++(*context.next_args_table_index);
  *context.next_host_args_offset += Om2ModelUtils::ArgsSizeAlign8(entry_->args_size);
}

Status MemcpyAsyncTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(ast_.Comment("============================= " + header_.op_name + " ==============================="));
  if (!input_addr_node_.is_reused_from_upstream) {
    GE_ASSERT_TRUE(input_addr_node_.tensor_info.has_value(),
                   "[OM2] MemcpyAsync input tensor info is required for %s.",
                   input_addr_node_.symbol_hint.c_str());
    const auto &input_tensor_info = *input_addr_node_.tensor_info;
    const std::string input_shape_var_name = input_addr_node_.symbol_hint + "_shape";
    items.push_back(ast_.VarDecl("std::vector<int64_t>", input_shape_var_name,
                                 ast_.InitList(ConvertToArgs(input_tensor_info.shape_dims))));
    items.push_back(ast_.VarDecl("Om2Tensor", input_addr_node_.symbol_hint, ast_.Call("BuildOm2Tensor", {
        GetAddr(total_dev_mem_ptr_, input_addr_node_.mem_offset),
        ast_.ULong(input_tensor_info.size),
        input_tensor_info.data_type,
        input_tensor_info.format,
        ast_.Var("std::vector<int64_t>", input_shape_var_name)})));
  }
  if (!output_addr_node_.is_reused_from_upstream) {
    GE_ASSERT_TRUE(output_addr_node_.tensor_info.has_value(),
                   "[OM2] MemcpyAsync output tensor info is required for %s.",
                   output_addr_node_.symbol_hint.c_str());
    const auto &output_tensor_info = *output_addr_node_.tensor_info;
    const std::string output_shape_var_name = output_addr_node_.symbol_hint + "_shape";
    items.push_back(ast_.VarDecl("std::vector<int64_t>", output_shape_var_name,
                                 ast_.InitList(ConvertToArgs(output_tensor_info.shape_dims))));
    items.push_back(ast_.VarDecl("Om2Tensor", output_addr_node_.symbol_hint, ast_.Call("BuildOm2Tensor", {
        GetAddr(total_dev_mem_ptr_, output_addr_node_.mem_offset),
        ast_.ULong(output_tensor_info.size),
        output_tensor_info.data_type,
        output_tensor_info.format,
        ast_.Var("std::vector<int64_t>", output_shape_var_name)})));
  }
  if (io_refresh_) {
    std::vector<Arg> args_vars;
    args_vars.emplace_back(ast_.Var("auto", input_addr_node_.symbol_hint));
    args_vars.emplace_back(ast_.Var("auto", output_addr_node_.symbol_hint));
    const std::string ioaddr_var_name = "op" + std::to_string(header_.op_index) + "_iow_addr" +
                                        std::to_string(internal_index_);
    auto ioaddr_var = ast_.Var("std::vector<uint64_t>", ioaddr_var_name);
    items.emplace_back(ast_.VarDecl(ioaddr_var, FlattenHostArgs(args_vars)));
    items.push_back(ChkStatus(MemcpyS(
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("host_addr"),
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("size"),
      ioaddr_var.Data(), ioaddr_var.Size() * ast_.Sizeof("uint64_t")))),
    items.push_back(ChkStatus(ast_.Call("KernelMemcpyAsyncDistribute", {
      ast_.Str(header_.op_name),
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("dev_addr") + ast_.Sizeof("uint64_t"),
      ast_.UInt(dst_max_),
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("dev_addr"),
      ast_.UInt(count_),
      ast_.StaticCast("rtMemcpyKind_t", static_cast<int64_t>(kind_)),
      stream_list_[static_cast<int>(header_.stream_id)],
      0})));
    return SUCCESS;
  }
  items.push_back(ChkStatus(ast_.Call("KernelMemcpyAsyncDistribute", {
      ast_.Str(header_.op_name),
      ast_.Call("ValueToPtr", {ast_.Var("auto", output_addr_node_.symbol_hint).Attr("device_address")}),
      ast_.UInt(dst_max_),
      ast_.Call("ValueToPtr", {ast_.Var("auto", input_addr_node_.symbol_hint).Attr("device_address")}),
      ast_.UInt(count_),
      ast_.StaticCast("rtMemcpyKind_t", static_cast<int64_t>(kind_)),
      stream_list_[static_cast<int>(header_.stream_id)],
      0})));
  return SUCCESS;
}

Status MemcpyAsyncTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto dst = ast_.Var("void *", "dst");
  auto dest_max = ast_.Var("uint64_t", "destMax");
  auto src = ast_.Var("void *", "src");
  auto count = ast_.Var("uint64_t", "count");
  auto kind = ast_.Var("const rtMemcpyKind_t", "kind");
  auto stream = ast_.Var("aclrtStream &", "stream");
  auto qos_cfg = ast_.Var("const uint32_t", "qosCfg");
  auto inputs = ast_.Var("std::array<uintptr_t, 7U>", "inputs");
  items.push_back(ast_.DefineFunction("KernelMemcpyAsyncDistribute",
      {op_name, dst, dest_max, src, count, kind, stream, qos_cfg}, "aclError", {
          ChkRt(RtSetTaskTag(op_name)),
          ast_.VarDecl(inputs, ast_.InitList({
              ast_.ReinterpretCast("uintptr_t", dst),
              ast_.StaticCast("uintptr_t", dest_max),
              ast_.ReinterpretCast("uintptr_t", src),
              ast_.StaticCast("uintptr_t", count),
              ast_.StaticCast("uintptr_t", kind),
              ast_.ReinterpretCast("uintptr_t", stream),
              ast_.StaticCast("uintptr_t", qos_cfg),
          })),
          ChkRt(RtGeneralCtrl(inputs[0].Addr(), ast_.StaticCast("uint32_t", 7), 0)),
          ast_.Return("ACL_SUCCESS"),
      }));
  return SUCCESS;
}

int64_t MemcpyAsyncTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::MemcpyAsyncDef &memcpy_async = task_def.memcpy_async();
  return static_cast<int64_t>(memcpy_async.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_MEMCPY_ASYNC, MemcpyAsyncTaskCodeBuilder);
}  // namespace ge
