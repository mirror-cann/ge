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
#include "common/om2/codegen/task_code_builder/task_code_builder_util.h"

#include <cinttypes>

#include "common/om2/codegen/task_code_builder_factory.h"
#include "common/om2/codegen/om2_model_utils.h"

namespace {
constexpr uint64_t kAlignment = 64U;
}

namespace ge {
void MemcpyAddrAsyncTaskCodeBuilder::AppendOrderedArgValue(const AddrSemantic &semantic, uint64_t current_host_offset) {
  if (semantic.memory_app == MemoryAppType::kModelIo) {
    io_addr_refresh_records_.push_back(
        IoAddrRefreshRecord{static_cast<uint64_t>(semantic.compile_state_io_addr_offset), current_host_offset});
    GELOGI("[OM2]append addr offset map: compile offset[%lu], args info offset[%lu]",
           static_cast<uint64_t>(semantic.compile_state_io_addr_offset), current_host_offset);
  }
  ordered_arg_values_.push_back(semantic);
}

void MemcpyAddrAsyncTaskCodeBuilder::PopulateBuildData() {
  build_data_.stream_id = header_.stream_id;
  build_data_.args_table_idx = static_cast<uint32_t>(entry_.table_index);
  for (const auto &semantic : ordered_arg_values_) {
    build_data_.ordered_args.push_back(TaskCodeBuilderUtil::ConvertAddrDesc(semantic));
  }
  size_t host_offset = build_data_.align_offset;
  for (size_t i = 0; i < arg_descs_.size(); ++i) {
    if (arg_descs_[i].addr_type == AddrType::CUSTOM_VALUE) {
      OpArgDesc arg;
      arg.offset = static_cast<uint64_t>(host_offset);
      arg.custom_value = *(PtrToPtr<uint8_t, const uint64_t>(arg_descs_[i].reserved));
      arg.size = (arg_descs_[i].ir_idx == static_cast<int32_t>(ArgsFormatWidth::BIT32)) ? 4U : 8U;
      build_data_.custom_value_args.push_back(std::move(arg));
    }
    host_offset += arg_sizes_[i];
  }
}

Status MemcpyAddrAsyncTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.op_index_to_count_map);
  const domi::MemcpyAsyncDef &memcpy_async = context.task_def.memcpy_async();

  ResolveInternalIndex(context);

  args_format_str_ =
      memcpy_async.args_format().empty() ? "{}{}{i_instance0*}{o_instance0*}" : memcpy_async.args_format();
  GE_ASSERT_SUCCESS(ArgsFormatDesc::Parse(context.op_desc, args_format_str_, arg_descs_));

  AddrSemantic src_addr_node;
  AddrSemantic dst_addr_node;
  GELOGI("[OM2][MemcpyAddrAsync] op=%s, src_logic_addr=0x%" PRIx64 ", dst_logic_addr=0x%" PRIx64
         ", internal_index=%u, op_index=%u",
         context.op_desc->GetName().c_str(), static_cast<uint64_t>(memcpy_async.src()),
         static_cast<uint64_t>(memcpy_async.dst()), internal_index_, memcpy_async.op_index());
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(memcpy_async.src()), src_addr_node,
                                                true, internal_index_));
  GELOGI("[OM2][MemcpyAddrAsync] src resolved: kind=%d, memory_app=%d, symbol_hint=%s, mem_offset=%" PRId64
         ", is_reused=%d",
         static_cast<int32_t>(src_addr_node.kind), static_cast<int32_t>(src_addr_node.memory_app),
         src_addr_node.symbol_hint.c_str(), src_addr_node.mem_offset, src_addr_node.is_reused_from_upstream);
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(memcpy_async.dst()), dst_addr_node,
                                                false, internal_index_));
  GELOGI("[OM2][MemcpyAddrAsync] dst resolved: kind=%d, memory_app=%d, symbol_hint=%s, mem_offset=%" PRId64
         ", is_reused=%d",
         static_cast<int32_t>(dst_addr_node.kind), static_cast<int32_t>(dst_addr_node.memory_app),
         dst_addr_node.symbol_hint.c_str(), dst_addr_node.mem_offset, dst_addr_node.is_reused_from_upstream);

  GE_ASSERT_SUCCESS(CalcArgSizes(context));

  build_data_.dst_max = memcpy_async.dst_max();
  build_data_.count = memcpy_async.count();
  build_data_.kind = memcpy_async.kind();

  GE_ASSERT_SUCCESS(BuildOrderedArgs(context, src_addr_node, dst_addr_node));

  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num, "[OM2][Check][Param] stream list size:%u, cur:%u!",
                 context.runtime->stream_num, header_.stream_id);
  GELOGI("Memcpy Addr Async Task Codegen: op[%s], dst max[%" PRIu64 "], count[%" PRIu64 "], kind[%u], stream id[%u].",
         context.op_desc->GetName().c_str(), build_data_.dst_max, build_data_.count, build_data_.kind,
         header_.stream_id);
  GELOGI("op_index %u, op_id %" PRId64, memcpy_async.op_index(), context.op_desc->GetId());

  PopulateBuildData();
  return SUCCESS;
}

void MemcpyAddrAsyncTaskCodeBuilder::ResolveInternalIndex(TaskSemanticContributeContext &context) {
  auto it = context.op_index_to_count_map->find(static_cast<uint32_t>(header_.op_index));
  if (it == context.op_index_to_count_map->end()) {
    internal_index_ = 0U;
    (*context.op_index_to_count_map)[static_cast<uint32_t>(header_.op_index)] = 1U;
  } else {
    internal_index_ = it->second;
    ++(*context.op_index_to_count_map)[static_cast<uint32_t>(header_.op_index)];
  }
}

Status MemcpyAddrAsyncTaskCodeBuilder::CalcArgSizes(const TaskSemanticContributeContext &context) {
  build_data_.args_size = 0U;
  arg_sizes_.clear();
  for (const auto &arg_desc : arg_descs_) {
    size_t arg_size = 0U;
    GE_ASSERT_SUCCESS(ArgsFormatDesc::GetArgSize(context.op_desc, arg_desc, arg_size));
    if (build_data_.args_size > std::numeric_limits<uint32_t>::max() - static_cast<uint32_t>(arg_size)) {
      GELOGE(FAILED, "Args size overflow: current=%u, adding=%zu", build_data_.args_size, arg_size);
      return FAILED;
    }
    build_data_.args_size += static_cast<uint32_t>(arg_size);
    arg_sizes_.push_back(arg_size);
  }
  return SUCCESS;
}

Status MemcpyAddrAsyncTaskCodeBuilder::BuildOrderedArgs(TaskSemanticContributeContext &context,
                                                        const AddrSemantic &src_addr_node,
                                                        const AddrSemantic &dst_addr_node) {
  const uint64_t current_host_offset = *context.next_host_args_offset;
  build_data_.align_offset =
      static_cast<uint32_t>((current_host_offset + kAlignment - 1U) / kAlignment * kAlignment - current_host_offset);

  bool io_encountered = false;
  size_t io_offset = 0;
  for (const auto &arg_desc : arg_descs_) {
    if ((arg_desc.addr_type == AddrType::INPUT_INSTANCE || arg_desc.addr_type == AddrType::OUTPUT_INSTANCE) &&
        !io_encountered) {
      GELOGI("align_offset: %u, io_offset: %zu", build_data_.align_offset, io_offset);
      build_data_.aligned_io_offset = build_data_.align_offset + static_cast<uint32_t>(io_offset);
      io_encountered = true;
    }
    if (arg_desc.addr_type == AddrType::INPUT_INSTANCE) {
      AppendOrderedArgValue(src_addr_node, current_host_offset + build_data_.align_offset + io_offset);
    } else if (arg_desc.addr_type == AddrType::OUTPUT_INSTANCE) {
      AppendOrderedArgValue(dst_addr_node, current_host_offset + build_data_.align_offset + io_offset);
    }
    GE_ASSERT_SUCCESS(ArgsFormatDesc::GetArgSize(context.op_desc, arg_desc, io_offset));
  }

  entry_.table_index = *context.next_args_table_index;
  entry_.args_size = build_data_.align_offset + io_offset;
  entry_.host_offset = *context.next_host_args_offset;
  args_table_entry_ = &entry_;
  ++(*context.next_args_table_index);
  *context.next_host_args_offset += Om2ModelUtils::ArgsSizeAlign8(static_cast<uint64_t>(entry_.args_size));
  return SUCCESS;
}

Status MemcpyAddrAsyncTaskCodeBuilder::RenderKernelDistributeFunc(std::vector<DeclNode *> &items) {
  auto op_name = ast_.Var("const char_t *const", "op_name");
  auto src_addr = ast_.Var("void *", "src_addr");
  auto dst_max = ast_.Var("uint64_t", "dst_max");
  auto count = ast_.Var("uint64_t", "count");
  auto kind = ast_.Var("const rtMemcpyKind_t", "kind");
  auto stream = ast_.Var("aclrtStream &", "stream");
  auto qos_cfg = ast_.Var("const uint32_t", "qos_cfg");
  (void)items.push_back(ast_.DefineFunction(
      "KernelMemcpyAddrAsyncDistribute", {op_name, src_addr, dst_max, count, kind, stream, qos_cfg}, "aclError",
      {
          ChkRt(RtSetTaskTag(op_name)),
          ChkRt(ast_.Call("rtMemcpyAsyncPtr", {src_addr, dst_max, count, kind, stream, qos_cfg})),
          ast_.Return("ACL_SUCCESS"),
      }));
  return SUCCESS;
}

Status MemcpyAddrAsyncTaskCodeBuilder::RenderDispatchFunc(std::vector<DeclNode *> &items) {
  std::vector<BodyItem> body;
  auto op = ast_.Var("const TaskDispatchInfo *", "op");
  auto ctx = ast_.Var("const DispatchOpContext &", "ctx");
  const auto memcpy_addr = op.Arrow("dispatch_info").Attr("memcpy_addr");

  // -- 解析地址参数 --
  auto args_table_idx = memcpy_addr.Attr("args_table_idx");
  auto args_info = ast_.Var("ArgsInfo *", "args_info");
  (void)body.push_back(ast_.VarDecl(args_info, ctx.Attr("args_table").Attr("GetArgsInfo")(args_table_idx)));
  const auto align_offset = Arg(memcpy_addr.Attr("align_offset"));
  auto dev_addr_off = ast_.Call("GET_ADDR", {args_info.Arrow("dev_addr"), align_offset});
  auto host_addr_off = ast_.Call("GET_ADDR", {args_info.Arrow("host_addr"), align_offset});

  // -- 构建 IO 地址向量 --
  auto iow_addr = ast_.Var("std::vector<uint64_t>", "iow_addr");
  (void)body.emplace_back(ast_.VarDecl(iow_addr));
  auto resolve_loop = RenderIoAddrResolveLoop(ctx, memcpy_addr);
  body.insert(body.end(), resolve_loop.begin(), resolve_loop.end());

  // -- 执行 memcpy + D2H 回传 --
  (void)body.push_back(ChkStatus(
      ast_.Call("KernelMemcpyAddrAsyncDistribute", {
                                                       Arg(op.Arrow("op_name")),
                                                       Arg(dev_addr_off),
                                                       Arg(memcpy_addr.Attr("dst_max")),
                                                       Arg(memcpy_addr.Attr("count")),
                                                       Arg(ast_.StaticCast("rtMemcpyKind_t", memcpy_addr.Attr("kind"))),
                                                       Arg(ctx.Attr("stream_list")[memcpy_addr.Attr("stream_id")]),
                                                       Arg(ast_.UInt(0)),
                                                   })));
  (void)body.push_back(ChkStatus(AclrtMemcpy(Arg(host_addr_off), Arg(memcpy_addr.Attr("args_size")), Arg(dev_addr_off),
                                             Arg(memcpy_addr.Attr("args_size")), "ACL_MEMCPY_DEVICE_TO_HOST")));

  // -- IO 地址写回 host args table --
  (void)body.push_back(ChkStatus(MemcpyS(
      ast_.Call(
          "ValueToPtr",
          {ast_.Call("PtrToValue", {ctx.Attr("args_table").Attr("GetArgsInfo")(args_table_idx).Arrow("host_addr")}) +
           memcpy_addr.Attr("aligned_io_offset")}),
      ctx.Attr("args_table").Attr("GetArgsInfo")(args_table_idx).Arrow("size") - memcpy_addr.Attr("aligned_io_offset"),
      iow_addr.Data(), iow_addr.Size() * ast_.Sizeof("uint64_t"))));

  // -- CustomValue 写回 --
  GE_ASSERT_SUCCESS(RenderCustomValueWriteback(body, op, ctx, args_table_idx));

  GE_ASSERT_SUCCESS(TaskCodeBuilderUtil::RenderDispatchFunc(ast_, kDispatchFuncName, body, items));
  return SUCCESS;
}

std::vector<BodyItem> MemcpyAddrAsyncTaskCodeBuilder::RenderIoAddrResolveLoop(const VarRef &ctx,
                                                                              const ExprRef &memcpy_addr) {
  auto iow_addr = ast_.Var("std::vector<uint64_t>", "iow_addr");
  return {
      ast_.For(
          ast_.VarDecl("uint32_t", "_i", ast_.UInt(0U)), ast_.Var("", "_i") < memcpy_addr.Attr("args_info_num"),
          ast_.PostInc(ast_.Var("", "_i")),
          std::initializer_list<BodyItem>{iow_addr.PushBack(ast_.ReinterpretCast(
              "uint64_t",
              ast_.Call("ResolveOpAddr",
                        {memcpy_addr.Attr("args_info")[ast_.Var("", "_i")].Attr("addr").Attr("mem_src"),
                         memcpy_addr.Attr("args_info")[ast_.Var("", "_i")].Attr("addr").Attr("offset"),
                         ctx.Attr("total_dev_mem_ptr"), ctx.Attr("session_scope_mem_ptr"), ctx.Attr("constants")})))}),
  };
}

Status MemcpyAddrAsyncTaskCodeBuilder::RenderDistHelper(std::vector<DeclNode *> &items) {
  GE_ASSERT_SUCCESS(RenderKernelDistributeFunc(items));
  GE_ASSERT_SUCCESS(RenderDispatchFunc(items));
  return SUCCESS;
}

int64_t MemcpyAddrAsyncTaskCodeBuilder::ParseOpIndex(const domi::TaskDef &task_def) {
  const domi::MemcpyAsyncDef &memcpy_async = task_def.memcpy_async();
  return static_cast<int64_t>(memcpy_async.op_index());
}

std::string MemcpyAddrAsyncTaskCodeBuilder::GetFuncName() const {
  return kDispatchFuncName;
}

Status MemcpyAddrAsyncTaskCodeBuilder::RenderOpDefTableFields(std::vector<std::pair<std::string, Arg>> &fields) {
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
           {{"memcpy_addr",
             ast_.InitList({TaskCodeBuilderUtil::RenderOpArgDesc(ast_, build_data_.ordered_args),
                            static_cast<uint32_t>(build_data_.ordered_args.size()),
                            static_cast<int64_t>(build_data_.dst_max), static_cast<int64_t>(build_data_.count),
                            build_data_.kind, build_data_.align_offset, build_data_.args_size, build_data_.stream_id,
                            build_data_.args_table_idx, build_data_.aligned_io_offset,
                            static_cast<uint32_t>(build_data_.custom_value_args.size()), cv_init})}})});
  return SUCCESS;
}

Status MemcpyAddrAsyncTaskCodeBuilder::RenderCustomValueWriteback(std::vector<BodyItem> &body, const VarRef &op,
                                                                  const VarRef &ctx, const ExprRef &args_table_idx) {
  // 运行时循环：遍历 op->dispatch_info.memcpy_addr.custom_values 写回 CUSTOM_VALUE
  // 等价于 runtime/v1: for (iter : format_) { if (CUSTOM_VALUE) write; GetArgSize(host_addr); }
  const auto memcpy_addr = op.Arrow("dispatch_info").Attr("memcpy_addr");
  auto host_addr_info = ctx.Attr("args_table").Attr("GetArgsInfo")(args_table_idx).Arrow("host_addr");
  (void)body.emplace_back(ast_.For(
      ast_.VarDecl("uint32_t", "_j", ast_.UInt(0U)), ast_.Var("", "_j") < memcpy_addr.Attr("num_custom_values"),
      ast_.PostInc(ast_.Var("", "_j")),
      std::initializer_list<BodyItem>{
          ast_.VarDecl("auto", "cv", memcpy_addr.Attr("custom_values")[ast_.Var("", "_j")]),
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
  return SUCCESS;
}

REGISTER_TASK_CODE_BUILDER(MODEL_TASK_MEMCPY_ADDR_ASYNC, MemcpyAddrAsyncTaskCodeBuilder);
}  // namespace ge
