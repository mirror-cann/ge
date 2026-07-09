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
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"

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
}  // namespace

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

std::string CmoAddrTaskCodeBuilder::BuildAutoArgsFormat(const TaskSemanticContributeContext &context) const {
  const GeTensorDesc &tensor_desc = context.op_desc->GetInputDesc(0U);
  const int64_t num_cnt = tensor_desc.GetShape().IsScalar() ? 1 : tensor_desc.GetShape().GetShapeSize();
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
  const uint32_t len_inner = std::min(static_cast<uint32_t>(shape_len), max_size);
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

  build_data_.cmo_op_code = cmo_addr_task.cmo_op_code();

  args_format_str_ = cmo_addr_task.args_format().empty() ? BuildAutoArgsFormat(context) : cmo_addr_task.args_format();
  GE_ASSERT_SUCCESS(ArgsFormatDesc::Parse(context.op_desc, args_format_str_, arg_descs_));

  AddrSemantic src_addr_node;
  GE_ASSERT_SUCCESS(
      Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(cmo_addr_task.src()), src_addr_node, true, 0U));

  GE_ASSERT_SUCCESS(BuildOrderedArgs(context, src_addr_node));

  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);
  GELOGI("CmoAddrTaskCodeBuilder: op[%s], cmo_op_code[%u], args_format[%s], stream_id[%u]",
         context.op_desc->GetName().c_str(), build_data_.cmo_op_code, args_format_str_.c_str(), header_.stream_id);

  build_data_.stream_id = header_.stream_id;
  build_data_.args_table_idx = static_cast<uint32_t>(entry_->table_index);
  build_data_.args_info_num = static_cast<uint32_t>(ordered_arg_values_.size());
  for (const auto &semantic : ordered_arg_values_) {
    build_data_.ordered_args.push_back(TaskCodeBuilderUtil::ConvertAddrDesc(semantic));
  }
  size_t host_offset = build_data_.align_offset;
  for (size_t i = 0; i < arg_descs_.size(); ++i) {
    if (arg_descs_[i].addr_type == AddrType::CUSTOM_VALUE) {
      OpArgDesc arg;
      arg.offset = static_cast<uint64_t>(host_offset);
      arg.custom_value = *reinterpret_cast<const uint64_t *>(arg_descs_[i].reserved);
      arg.size = (arg_descs_[i].ir_idx == static_cast<int32_t>(ArgsFormatWidth::BIT32)) ? 4U : 8U;
      build_data_.custom_value_args.push_back(std::move(arg));
    }
    host_offset += arg_sizes_[i];
  }
  return SUCCESS;
}

Status CmoAddrTaskCodeBuilder::BuildOrderedArgs(TaskSemanticContributeContext &context,
                                                const AddrSemantic &src_addr_node) {
  const uint64_t current_host_offset = *context.next_host_args_offset;
  build_data_.align_offset =
      static_cast<uint32_t>((current_host_offset + kAlignment - 1U) / kAlignment * kAlignment - current_host_offset);

  build_data_.args_size = 0U;
  arg_sizes_.clear();
  bool io_encountered = false;
  for (const auto &arg_desc : arg_descs_) {
    if (arg_desc.addr_type == AddrType::INPUT_INSTANCE && !io_encountered) {
      GELOGI("align_offset: %u, io_offset: %u", build_data_.align_offset, build_data_.args_size);
      build_data_.io_offset = build_data_.args_size;
      io_encountered = true;
    }
    if (arg_desc.addr_type == AddrType::INPUT_INSTANCE) {
      AppendOrderedArgValue(src_addr_node, current_host_offset + build_data_.align_offset + build_data_.args_size);
    }
    size_t arg_size = 0U;
    GE_ASSERT_SUCCESS(ArgsFormatDesc::GetArgSize(context.op_desc, arg_desc, arg_size));
    build_data_.args_size += static_cast<uint32_t>(arg_size);
    arg_sizes_.push_back(arg_size);
  }

  (void)entry_.emplace();
  entry_->table_index = *context.next_args_table_index;
  const size_t total_args_size = build_data_.align_offset + build_data_.args_size + kAlignment;
  entry_->args_size = total_args_size;
  entry_->host_offset = *context.next_host_args_offset;
  args_table_entry_ = &(*entry_);
  ++(*context.next_args_table_index);
  *context.next_host_args_offset += Om2ModelUtils::ArgsSizeAlign8(entry_->args_size);
  return SUCCESS;
}

Status CmoAddrTaskCodeBuilder::RenderKernelDistributeFunc(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto args_addr = ast_.Var("void *", "args_addr");
  auto args_size = ast_.Var("uint32_t", "args_size");
  auto cmo_op_code = ast_.Var("rtCmoOpCode_t", "cmo_op_code");
  auto stream = ast_.Var("aclrtStream", "stream");
  auto flag = ast_.Var("uint32_t", "flag");
  (void)items.push_back(ast_.DefineFunction(
      "KernelCmoAddrTaskDistribute", {op_name, args_addr, args_size, cmo_op_code, stream, flag}, "aclError",
      {
          ChkRt(RtSetTaskTag(op_name)),
          ChkRt(ast_.Call("rtCmoAddrTaskLaunch", {args_addr, args_size, cmo_op_code, stream, flag})),
          ast_.Return("ACL_SUCCESS"),
      }));
  return SUCCESS;
}

Status CmoAddrTaskCodeBuilder::RenderKernelLaunch(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                                                  const ExprRef &dev_addr_off, const ExprRef &host_addr_off) {
  (void)body.push_back(ChkStatus(ast_.Call(
      "KernelCmoAddrTaskDistribute",
      {
          Arg(op.Arrow("op_name")),
          Arg(dev_addr_off),
          Arg(op.Arrow("dispatch_info").Attr("cmo_addr").Attr("args_size")),
          Arg(ast_.StaticCast("rtCmoOpCode_t", op.Arrow("dispatch_info").Attr("cmo_addr").Attr("cmo_op_code"))),
          Arg(ctx.Attr("stream_list")[op.Arrow("dispatch_info").Attr("cmo_addr").Attr("stream_id")]),
          Arg(ast_.UInt(0)),
      })));
  (void)body.push_back(ChkStatus(AclrtMemcpy(
      Arg(host_addr_off), Arg(op.Arrow("dispatch_info").Attr("cmo_addr").Attr("args_size")), Arg(dev_addr_off),
      Arg(op.Arrow("dispatch_info").Attr("cmo_addr").Attr("args_size")), "ACL_MEMCPY_DEVICE_TO_HOST")));
  return SUCCESS;
}

Status CmoAddrTaskCodeBuilder::RenderArgsWriteback(std::vector<BodyItem> &body, const VarRef &op, const VarRef &ctx,
                                                   const VarRef &iow_addr, const ExprRef &args_table_idx) {
  (void)body.push_back(ChkStatus(MemcpyS(
      ast_.Call(
          "ValueToPtr",
          {ast_.Call("PtrToValue", {ctx.Attr("args_table").Attr("GetArgsInfo")(args_table_idx).Arrow("host_addr")}) +
           op.Arrow("dispatch_info").Attr("cmo_addr").Attr("align_offset") +
           op.Arrow("dispatch_info").Attr("cmo_addr").Attr("io_offset")}),
      ctx.Attr("args_table").Attr("GetArgsInfo")(args_table_idx).Arrow("size") -
          (op.Arrow("dispatch_info").Attr("cmo_addr").Attr("align_offset") +
           op.Arrow("dispatch_info").Attr("cmo_addr").Attr("io_offset")),
      iow_addr.Data(), iow_addr.Size() * ast_.Sizeof("uint64_t"))));
  RenderCustomValueWriteback(body, ctx, args_table_idx);
  return SUCCESS;
}

void CmoAddrTaskCodeBuilder::RenderCustomValueWriteback(std::vector<BodyItem> &body, const VarRef &ctx,
                                                        const ExprRef &args_table_idx) {
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto host_addr_info = ctx.Attr("args_table").Attr("GetArgsInfo")(args_table_idx).Arrow("host_addr");
  (void)body.emplace_back(ast_.For(
      ast_.VarDecl("uint32_t", "_j", ast_.UInt(0U)),
      ast_.Var("", "_j") < op.Arrow("dispatch_info").Attr("cmo_addr").Attr("num_custom_values"),
      ast_.PostInc(ast_.Var("", "_j")),
      std::initializer_list<BodyItem>{
          ast_.VarDecl("auto", "cv",
                       op.Arrow("dispatch_info").Attr("cmo_addr").Attr("custom_values")[ast_.Var("", "_j")]),
          ast_.VarDecl("auto", "host_base",
                       ast_.Call("ValueToPtr",
                                 {ast_.Call("PtrToValue", {host_addr_info}) + ast_.Var("", "cv").Attr("host_offset")})),
          ast_.If(Arg(ast_.Var("", "cv").Attr("size") == ast_.UInt(4)),
                  std::initializer_list<BodyItem>{
                      ast_.Assign(ast_.Deref(ast_.ReinterpretCast("uint32_t *", ast_.Var("", "host_base"))),
                                  ast_.StaticCast("uint32_t", ast_.Var("", "cv").Attr("value")))},
                  std::initializer_list<BodyItem>{
                      ast_.Assign(ast_.Deref(ast_.ReinterpretCast("uint64_t *", ast_.Var("", "host_base"))),
                                  ast_.Var("", "cv").Attr("value"))})}));
}

Status CmoAddrTaskCodeBuilder::RenderDispatchFunc(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  VarRef args_info = ast_.Var("ArgsInfo *", "args_info");
  VarRef iow_addr = ast_.Var("std::vector<uint64_t>", "iow_addr");
  ExprRef args_table_idx = op.Arrow("dispatch_info").Attr("cmo_addr").Attr("args_table_idx");
  (void)body.push_back(ast_.VarDecl(args_info, ctx.Attr("args_table").Attr("GetArgsInfo")(args_table_idx)));
  const auto align_offset = Arg(op.Arrow("dispatch_info").Attr("cmo_addr").Attr("align_offset"));
  ExprRef dev_addr_off = ast_.Call("GET_ADDR", {args_info.Arrow("dev_addr"), align_offset});
  ExprRef host_addr_off = ast_.Call("GET_ADDR", {args_info.Arrow("host_addr"), align_offset});
  (void)body.emplace_back(ast_.VarDecl(iow_addr));
  (void)body.emplace_back(ast_.For(
      ast_.VarDecl("uint32_t", "_i", ast_.UInt(0U)),
      ast_.Var("", "_i") < op.Arrow("dispatch_info").Attr("cmo_addr").Attr("args_info_num"),
      ast_.PostInc(ast_.Var("", "_i")),
      {iow_addr.PushBack(ast_.ReinterpretCast(
          "uint64_t", ast_.Call("ResolveOpAddr", {op.Arrow("dispatch_info")
                                                      .Attr("cmo_addr")
                                                      .Attr("args_info")[ast_.Var("", "_i")]
                                                      .Attr("addr")
                                                      .Attr("mem_src"),
                                                  op.Arrow("dispatch_info")
                                                      .Attr("cmo_addr")
                                                      .Attr("args_info")[ast_.Var("", "_i")]
                                                      .Attr("addr")
                                                      .Attr("offset"),
                                                  ctx.Attr("total_dev_mem_ptr"), ctx.Attr("session_scope_mem_ptr"),
                                                  ctx.Attr("constants")})))}));
  GE_ASSERT_SUCCESS(RenderKernelLaunch(body, op, ctx, dev_addr_off, host_addr_off));
  GE_ASSERT_SUCCESS(RenderArgsWriteback(body, op, ctx, iow_addr, args_table_idx));

  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

Status CmoAddrTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  GE_ASSERT_SUCCESS(RenderKernelDistributeFunc(items));
  GE_ASSERT_SUCCESS(RenderDispatchFunc(items));
  return SUCCESS;
}

int64_t CmoAddrTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::CmoAddrTaskDef &cmo_addr_task = task_def.cmo_addr_task();
  return static_cast<int64_t>(cmo_addr_task.op_index());
}

std::string CmoAddrTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status CmoAddrTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
  fields.push_back({"dispatch_type", ast_.StaticCast("OpDispatchType", static_cast<int64_t>(kDispatchType))});
  fields.push_back({"op_name", Arg::StringLiteral(header_.op_name)});

  // 构建 CustomValueEntry 复合字面量（复用 RenderOpArgDesc 的 CCast 模式）
  std::vector<Arg> cv_entries;
  cv_entries.reserve(build_data_.custom_value_args.size());
  for (const auto &entry : build_data_.custom_value_args) {
    cv_entries.push_back(ast_.InitList({static_cast<int64_t>(entry.offset), static_cast<int64_t>(entry.custom_value),
                                        static_cast<uint32_t>(entry.size)}));
  }
  auto cv_init =
      cv_entries.empty() ? Arg(nullptr) : ast_.CCast("const CustomValueEntry[]", ast_.InitList(cv_entries, true));

  fields.push_back(
      {"dispatch_info",
       ast_.DesignatedInit(
           {{"cmo_addr", ast_.InitList({TaskCodeBuilderUtil::RenderOpArgDesc(ast_, build_data_.ordered_args),
                                        build_data_.args_info_num, build_data_.args_size, build_data_.align_offset,
                                        build_data_.cmo_op_code, build_data_.stream_id, build_data_.args_table_idx,
                                        build_data_.io_offset,
                                        static_cast<uint32_t>(build_data_.custom_value_args.size()), cv_init})}})});
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_CMO_ADDR, CmoAddrTaskCodeBuilder);
}  // namespace ge
