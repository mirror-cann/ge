/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "exe_graph/runtime/annotated_args_context.h"

#include <limits>
#include <utility>
#include <vector>

#include "annotated_args_handler.h"
#include "common/checker.h"
#include "exe_graph/runtime/gert_mem_allocator.h"
#include "exe_graph/runtime/gert_mem_block.h"
#include "graph/utils/args_format_desc_utils.h"

namespace gert {
namespace {
constexpr uint32_t kInvalidStreamId = std::numeric_limits<uint32_t>::max();
}  // namespace

WorkspaceAddr AnnotatedArgsContext::MallocWorkSpace(size_t size) {
  GE_ASSERT_TRUE(size > 0U, "Offline launch workspace size is zero.");
  const auto additional_input_start = GetAdditionalInputStartIndex();
  GE_ASSERT_TRUE(additional_input_start >= 0);
  const int64_t allocator_index =
      additional_input_start + static_cast<int64_t>(AdditionalInputIndex::kWorkspaceAllocator);
  GE_ASSERT_TRUE(allocator_index >= 0);
  auto *allocator = GetInputValue<GertAllocator *>(allocator_index);
  GE_ASSERT_NOTNULL(allocator);

  const auto additional_output_start = GetAdditionalOutputStartIndex();
  GE_ASSERT_TRUE(additional_output_start >= 0);
  const int64_t workspace_output_index =
      additional_output_start + static_cast<int64_t>(AdditionalOutputIndex::kWorkSpace);
  auto *memory_vec = GetOutputPointer<std::vector<GertMemBlock *>>(workspace_output_index);
  GE_ASSERT_NOTNULL(memory_vec);
  GE_ASSERT_TRUE(memory_vec->size() <= static_cast<size_t>(std::numeric_limits<uint32_t>::max()),
                 "Offline launch workspace index overflow.");
  const auto workspace_index = static_cast<uint32_t>(memory_vec->size());
  auto *mem_block = allocator->Malloc(size);
  GE_ASSERT_NOTNULL(mem_block);
  (void)memory_vec->emplace_back(mem_block);
  return WorkspaceAddr{workspace_index, mem_block->GetAddr()};
}

uint32_t AnnotatedArgsContext::GetStreamId() const {
  const auto additional_input_start = GetAdditionalInputStartIndex();
  if (additional_input_start < 0) {
    GELOGE(ge::GRAPH_FAILED, "Annotated args context has no compute node info.");
    return kInvalidStreamId;
  }
  const int64_t allocator_index =
      additional_input_start + static_cast<int64_t>(AdditionalInputIndex::kWorkspaceAllocator);
  const auto *allocator = GetInputValue<GertAllocator *>(allocator_index);
  if (allocator == nullptr) {
    GELOGE(ge::GRAPH_FAILED, "Annotated args context workspace allocator is null.");
    return kInvalidStreamId;
  }
  const int64_t stream_id = allocator->GetStreamId();
  if ((stream_id < 0) || (stream_id >= static_cast<int64_t>(kInvalidStreamId))) {
    GELOGE(ge::GRAPH_FAILED, "Annotated args context stream id %ld is invalid.", stream_id);
    return kInvalidStreamId;
  }
  return static_cast<uint32_t>(stream_id);
}

ge::graphStatus AnnotatedArgsContext::AddLaunch(const AnnotatedKernelLaunchInfo &launch_info,
                                                AnnotatedKernelArgs &&args) {
  GE_ASSERT_TRUE((launch_info.kernel_name != nullptr) && (launch_info.kernel_name[0] != '\0'),
                 "Offline launch kernel name is empty.");
  GE_ASSERT_TRUE((launch_info.kernel_bin != nullptr) && (launch_info.kernel_bin_size > 0U),
                 "Offline launch kernel bin is empty.");
  GE_ASSERT_TRUE(launch_info.block_dim > 0U, "Offline launch block dim is zero.");
  GE_ASSERT_TRUE(launch_info.stream_id != kInvalidStreamId, "Offline launch stream id is invalid.");

  std::vector<uint8_t> args_data;
  std::vector<ge::ArgDesc> arg_descs;
  GE_ASSERT_SUCCESS(args.ExtractArgsData(args_data, arg_descs));

  const auto additional_output_start = GetAdditionalOutputStartIndex();
  GE_ASSERT_TRUE(additional_output_start >= 0);
  const int64_t handler_index = additional_output_start + static_cast<int64_t>(AdditionalOutputIndex::kArgsHandler);
  auto *handler_chain = GetOutput(handler_index);
  GE_ASSERT_NOTNULL(handler_chain);
  auto *args_handler = handler_chain->GetValue<ArgsHandler *>();
  GE_ASSERT_NOTNULL(args_handler);
  auto *annotated_args_handler = dynamic_cast<AnnotatedArgsHandler *>(args_handler);
  GE_ASSERT_NOTNULL(annotated_args_handler);
  annotated_args_handler->AddLaunch(launch_info.kernel_name, launch_info.kernel_bin, launch_info.kernel_bin_size,
                                    launch_info.block_dim, launch_info.stream_id, std::move(args_data),
                                    std::move(arg_descs));
  return ge::GRAPH_SUCCESS;
}

}  // namespace gert
