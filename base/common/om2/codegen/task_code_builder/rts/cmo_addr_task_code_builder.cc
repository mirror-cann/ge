/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "cmo_addr_task_code_builder.h"

#include <cinttypes>

#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"
#include "common/ge_common/debug/ge_log.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/tensor_utils.h"

namespace {
constexpr uint64_t kAlignment = 64U;
constexpr uint32_t kMaxPrefetchLen = 120U * 1024U * 1024U;
constexpr char_t const *kAttrMaxSize = "max_size";
constexpr char_t const *kAttrAddrOffset = "offset";
}

namespace ge {
void CmoAddrTaskCodeBuilder::AppendOrderedArgValue(const AddrSemantic &semantic, uint64_t current_host_offset) {
  if (semantic.memory_app == MemoryAppType::kModelIo) {
    io_addr_refresh_records_.push_back(
        IoAddrRefreshRecord{static_cast<uint64_t>(semantic.compile_state_io_addr_offset), current_host_offset});
    GELOGI("[OM2]append addr offset map: compile offset[%lu], args info offset[%lu]",
           static_cast<uint64_t>(semantic.compile_state_io_addr_offset), current_host_offset);
  }
  ordered_arg_values_.push_back(semantic);
}

std::string CmoAddrTaskCodeBuilder::BuildAutoArgsFormat(const TaskSemanticContributeContext &context) {
  const GeTensorDesc &tensor_desc = context.op_desc->GetInputDesc(0U);
  int64_t num_cnt = tensor_desc.GetShape().IsScalar() ? 1 : tensor_desc.GetShape().GetShapeSize();
  int64_t shape_len = GetSizeInBytes(num_cnt, tensor_desc.GetDataType());
  GE_ASSERT_TRUE(shape_len > 0);
  int64_t offset{0};
  (void)AttrUtils::GetInt(context.op_desc, kAttrAddrOffset, offset);
  GE_ASSERT_TRUE((offset >= 0) && (offset < shape_len),
                 "[OM2] CmoAddr offset %" PRId64 " out of range [0, %" PRId64 ").", offset, shape_len);
  shape_len -= offset;
  uint32_t max_size{0U};
  (void)AttrUtils::GetInt(context.op_desc, kAttrMaxSize, max_size);
  if (max_size == 0U) {
    max_size = kMaxPrefetchLen;
  }
  uint32_t len_inner = std::min(static_cast<uint32_t>(shape_len), max_size);
  std::string format_str = "{}{.32b}{#.32b" + std::to_string(len_inner) + "}{i_instance0*}{}";
  GELOGI("CmoAddrTaskCodeBuilder: auto format, shape_len[%" PRId64 "], offset[%" PRId64
         "], max_size[%u], len_inner[%u]",
         shape_len, offset, max_size, len_inner);
  return format_str;
}

Status CmoAddrTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);

  const domi::CmoAddrTaskDef &cmo_addr_task = context.task_def.cmo_addr_task();

  cmo_op_code_ = cmo_addr_task.cmo_op_code();

  args_format_str_ = cmo_addr_task.args_format().empty()
                         ? BuildAutoArgsFormat(context)
                         : cmo_addr_task.args_format();
  GE_ASSERT_SUCCESS(ArgsFormatDesc::Parse(context.op_desc, args_format_str_, arg_descs_));

  uint64_t logical_src_mem_type = 0U;
  AddrSemantic src_addr_node;
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(cmo_addr_task.src()),
                                                logical_src_mem_type, src_addr_node, true, 0U));

  GE_ASSERT_SUCCESS(BuildOrderedArgs(context, src_addr_node));

  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);
  GELOGI("CmoAddrTaskCodeBuilder: op[%s], cmo_op_code[%u], args_format[%s], stream_id[%u]",
         context.op_desc->GetName().c_str(), cmo_op_code_, args_format_str_.c_str(), header_.stream_id);

  return SUCCESS;
}

Status CmoAddrTaskCodeBuilder::BuildOrderedArgs(TaskSemanticContributeContext &context,
                                                const AddrSemantic &src_addr_node) {
  const uint64_t current_host_offset = *context.next_host_args_offset;
  align_offset_ = (current_host_offset + kAlignment - 1) / kAlignment * kAlignment - current_host_offset;

  args_size_ = 0U;
  arg_sizes_.clear();
  bool io_encountered = false;
  for (const auto &arg_desc : arg_descs_) {
    if (arg_desc.addr_type == AddrType::INPUT_INSTANCE && !io_encountered) {
      GELOGI("align_offset: %zu, io_offset: %zu", align_offset_, args_size_);
      io_offset_ = args_size_;
      io_encountered = true;
    }
    if (arg_desc.addr_type == AddrType::INPUT_INSTANCE) {
      AppendOrderedArgValue(src_addr_node, current_host_offset + align_offset_ + args_size_);
    }
    size_t arg_size = 0U;
    GE_ASSERT_SUCCESS(ArgsFormatDesc::GetArgSize(context.op_desc, arg_desc, arg_size));
    args_size_ += arg_size;
    arg_sizes_.push_back(arg_size);
  }

  entry_.emplace();
  entry_->table_index = *context.next_args_table_index;
  total_args_size_ = align_offset_ + args_size_ + kAlignment;
  entry_->args_size = total_args_size_;
  entry_->host_offset = *context.next_host_args_offset;
  args_table_entry_ = &(*entry_);
  ++(*context.next_args_table_index);
  *context.next_host_args_offset += Om2ModelUtils::ArgsSizeAlign8(entry_->args_size);
  return SUCCESS;
}

Status CmoAddrTaskCodeBuilder::CollectIoAddrVars(std::vector<BodyItem> &items, std::vector<Arg> &args_vars) {
  for (const auto &semantic : ordered_arg_values_) {
    if (semantic.kind == AddrValueKind::kInputInstance || semantic.kind == AddrValueKind::kOutputInstance) {
      if (!semantic.is_reused_from_upstream) {
        if (!semantic.tensor_info.has_value()) {
          GELOGE(FAILED, "[OM2] CmoAddrAsync tensor info is required for %s.", semantic.symbol_hint.c_str());
          return FAILED;
        }
        const auto &tensor_info = *semantic.tensor_info;
        const std::string shape_var_name = semantic.symbol_hint + "_shape";
        items.push_back(
            ast_.VarDecl("std::vector<int64_t>", shape_var_name, ast_.InitList(ConvertToArgs(tensor_info.shape_dims))));
        auto &base_ptr = (semantic.memory_type == (kSessionScopeMemoryMask | RT_MEMORY_HBM))
                             ? session_scope_mem_ptr_ : total_dev_mem_ptr_;
        items.push_back(ast_.VarDecl(
            "Om2Tensor", semantic.symbol_hint,
            ast_.Call("BuildOm2Tensor",
                      {GetAddr(base_ptr, semantic.mem_offset), ast_.ULong(tensor_info.size),
                       tensor_info.data_type, tensor_info.format, ast_.Var("std::vector<int64_t>", shape_var_name)})));
      }
      args_vars.emplace_back(ast_.Var("auto", semantic.symbol_hint));
    } else if (semantic.kind == AddrValueKind::kConstTensor) {
      args_vars.emplace_back(ast_.Var("auto", semantic.symbol_hint));
    } else {
      GELOGE(FAILED, "[OM2] CmoAddrAsync unsupported addr kind %d.", static_cast<int32_t>(semantic.kind));
      return FAILED;
    }
  }
  return SUCCESS;
}

void CmoAddrTaskCodeBuilder::RenderCustomValueWriteback(std::vector<BodyItem> &items) {
  size_t host_offset = align_offset_;
  for (size_t i = 0; i < arg_descs_.size(); ++i) {
    if (arg_descs_[i].addr_type == AddrType::CUSTOM_VALUE) {
      const uint64_t value = *reinterpret_cast<const uint64_t *>(arg_descs_[i].reserved);
      auto host_base = args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("host_addr");
      auto target_ptr = ast_.ReinterpretCast(
          arg_descs_[i].ir_idx == static_cast<int32_t>(ArgsFormatWidth::BIT32) ? "uint32_t *" : "uint64_t *",
          ast_.Call("ValueToPtr", {ast_.Call("PtrToValue", {host_base}) + host_offset}));
      if (arg_descs_[i].ir_idx == static_cast<int32_t>(ArgsFormatWidth::BIT32)) {
        items.push_back(ast_.Assign(ast_.Deref(target_ptr), ast_.StaticCast("uint32_t", value)));
      } else {
        items.push_back(ast_.Assign(ast_.Deref(target_ptr), ast_.UInt(value)));
      }
    }
    host_offset += arg_sizes_[i];
  }
}

Status CmoAddrTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(
      ast_.Comment("============================= " + header_.op_name + " (CMO_ADDR) ==============================="));

  std::vector<Arg> args_vars;
  GE_ASSERT_SUCCESS(CollectIoAddrVars(items, args_vars));

  const std::string ioaddr_var_name =
      "op" + std::to_string(header_.op_index) + "_iow_addr0";
  auto ioaddr_var = ast_.Var("std::vector<uint64_t>", ioaddr_var_name);
  items.emplace_back(ast_.VarDecl(ioaddr_var, FlattenHostArgs(args_vars)));

  auto dev_addr_expr = ast_.Call("ValueToPtr", {ast_.Call("PtrToValue", {
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("dev_addr")}) + align_offset_});
  auto host_addr_expr = ast_.Call("ValueToPtr", {ast_.Call("PtrToValue", {
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("host_addr")}) + align_offset_});

  items.push_back(ChkStatus(ast_.Call(
      "KernelCmoAddrTaskDistribute",
      {ast_.Str(header_.op_name),
       dev_addr_expr,
       ast_.UInt(static_cast<uint32_t>(args_size_)), ast_.StaticCast("rtCmoOpCode_t", cmo_op_code_),
       stream_list_[static_cast<int>(header_.stream_id)], ast_.UInt(0)})));

  items.push_back(ChkStatus(AclrtMemcpy(
      host_addr_expr,
      args_size_,
      dev_addr_expr,
      args_size_, "ACL_MEMCPY_DEVICE_TO_HOST")));

  items.push_back(ChkStatus(MemcpyS(
      ast_.Call("ValueToPtr", {ast_.Call("PtrToValue", {
          args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("host_addr")}) + align_offset_ + io_offset_}),
      args_table_.Attr("GetArgsInfo")(static_cast<int64_t>(entry_->table_index)).Arrow("size") - (align_offset_ + io_offset_),
      ioaddr_var.Data(), ioaddr_var.Size() * ast_.Sizeof("uint64_t"))));

  RenderCustomValueWriteback(items);

  return SUCCESS;
}

Status CmoAddrTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto args_addr = ast_.Var("void *", "args_addr");
  auto args_size = ast_.Var("uint32_t", "args_size");
  auto cmo_op_code = ast_.Var("rtCmoOpCode_t", "cmo_op_code");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto flag = ast_.Var("uint32_t", "flag");

  items.push_back(ast_.DefineFunction(
      "KernelCmoAddrTaskDistribute", {op_name, args_addr, args_size, cmo_op_code, stream, flag}, "aclError",
      {
          ChkRt(RtSetTaskTag(op_name)),
          ChkRt(ast_.Call("rtCmoAddrTaskLaunch", {args_addr, args_size, cmo_op_code, stream, flag})),
          ast_.Return("ACL_SUCCESS"),
      }));

  return SUCCESS;
}

int64_t CmoAddrTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::CmoAddrTaskDef &cmo_addr_task = task_def.cmo_addr_task();
  return static_cast<int64_t>(cmo_addr_task.op_index());
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_CMO_ADDR, CmoAddrTaskCodeBuilder);
}  // namespace ge