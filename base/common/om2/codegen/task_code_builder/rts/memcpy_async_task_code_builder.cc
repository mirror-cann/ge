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
Status MemcpyAsyncTaskCodeBuilder::Contribute(TaskSemanticContributeContext &context) {
  FillTaskSemanticHeader(context, header_);
  GE_ASSERT_NOTNULL(context.runtime);
  GE_ASSERT_NOTNULL(context.op_desc);
  GE_ASSERT_NOTNULL(context.op_index_to_count_map);
  const domi::MemcpyAsyncDef &memcpy_async = context.task_def.memcpy_async();
  uint32_t internal_index = 0U;
  const uint32_t op_index = memcpy_async.op_index();
  auto it = context.op_index_to_count_map->find(op_index);
  if (it == context.op_index_to_count_map->end()) {
    internal_index = 0;
    (*context.op_index_to_count_map)[op_index] = 1;
  } else {
    internal_index = it->second;
    ++(*context.op_index_to_count_map)[op_index];
  }

  uint64_t logical_src_mem_type = 0U;
  uint64_t logical_dst_mem_type = 0U;
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(memcpy_async.src()),
                                                logical_src_mem_type, input_addr_node_, true, internal_index));
  GE_ASSERT_SUCCESS(Om2ModelUtils::GetRtAddress(context, static_cast<uintptr_t>(memcpy_async.dst()),
                                                logical_dst_mem_type, output_addr_node_, false, internal_index));
  for (auto &item : context.model_io->entries) {
    GE_ASSERT_TRUE(item.memory_offset != input_addr_node_.mem_offset &&
                   item.memory_offset != output_addr_node_.mem_offset,
                   "memcpy async should not link with io! input offset[%ld], output offset[%ld], memory_offset[%ld]",
                   input_addr_node_.mem_offset, output_addr_node_.mem_offset, item.memory_offset);
  }
  dst_max_ = memcpy_async.dst_max();
  count_ = memcpy_async.count();
  kind_ = memcpy_async.kind();
  GE_ASSERT_TRUE(header_.stream_id < context.runtime->stream_num,
                 "[OM2][Check][Param] stream list size:%u, cur:%u!", context.runtime->stream_num, header_.stream_id);
  GELOGI("Memcpy Async Task Codegen: op[%s], dst max[%" PRIu64 "], count[%" PRIu64
         "], kind[%u], stream id[%u].",
         context.op_desc->GetName().c_str(), dst_max_, count_, kind_, header_.stream_id);
  GELOGI("op_index %u, op_id %" PRId64, memcpy_async.op_index(), context.op_desc->GetId());
  return SUCCESS;
}

Status MemcpyAsyncTaskCodeBuilder::RenderDistribution(std::vector<BodyItem> &items) {
  items.push_back(ast_.Comment("============================= " + header_.op_name + " ==============================="));
  if (!input_addr_node_.is_reused_from_upstream) {
    items.push_back(ast_.VarDecl("auto", input_addr_node_.symbol_hint, GetAddr(total_dev_mem_ptr_,
                                                                               input_addr_node_.mem_offset)));
  }
  if (!output_addr_node_.is_reused_from_upstream) {
    items.push_back(ast_.VarDecl("auto", output_addr_node_.symbol_hint, GetAddr(total_dev_mem_ptr_,
                                                                                output_addr_node_.mem_offset)));
  }
  items.push_back(ChkStatus(ast_.Call("KernelMemcpyAsyncDistribute", {
      ast_.Str(header_.op_name),
      output_addr_node_.symbol_hint,
      ast_.UInt(dst_max_),
      input_addr_node_.symbol_hint,
      ast_.UInt(count_),
      ast_.StaticCast("rtMemcpyKind_t", static_cast<int64_t>(kind_)),
      stream_list_[static_cast<int>(header_.stream_id)],
      0,
  })));
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
