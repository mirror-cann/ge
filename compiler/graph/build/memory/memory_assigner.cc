/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "framework/memory/memory_assigner.h"
#include <memory>
#include "framework/common/debug/ge_log.h"
#include "common/util/mem_utils.h"
#include "checker.h"

#define RETURN_ON_FAIL(expr, ...)  \
  do {                             \
    if ((expr) != SUCCESS) {       \
      GELOGE(FAILED, __VA_ARGS__); \
      return FAILED;               \
    }                              \
  } while (0)

namespace ge {
Status MemoryAssigner::AssignMemory(std::map<uint64_t, size_t> &mem_offsets, size_t &zero_copy_mem_size,
                                    const bool has_assigned_var_mem) {
  graph_mem_assigner_ = MakeShared<GraphMemoryAssigner>(compute_graph_);
  GE_CHECK_NOTNULL(graph_mem_assigner_);

  RETURN_ON_FAIL(graph_mem_assigner_->AssignMemory(has_assigned_var_mem), "[Assign][Memory] failed, graph:%s",
                 compute_graph_->GetName().c_str());
  RETURN_ON_FAIL(graph_mem_assigner_->ReAssignMemory(mem_offsets), "[ReAssign][Memory] failed, graph:%s",
                 compute_graph_->GetName().c_str());
  RETURN_ON_FAIL(graph_mem_assigner_->AssignZeroCopyMemory(mem_offsets, zero_copy_mem_size),
                 "[Assign][ZeroCopyMemory] failed, graph:%s", compute_graph_->GetName().c_str());

  const auto graph_mem_splitter = graph_mem_assigner_->GetGraphMemSplitter();
  if (graph_mem_splitter != nullptr) {
    sub_mem_offsets_ = std::move(graph_mem_splitter->GetSubMemOffsets());
  }

  RETURN_ON_FAIL(graph_mem_assigner_->AssignMemory2HasRefAttrNode(),
                 "[Assign][Memory] to node which has ref attr failed! graph:%s", compute_graph_->GetName().c_str());
  RETURN_ON_FAIL(graph_mem_assigner_->AssignReferenceMemory(), "[Assign][ReferenceMemory] failed! graph:%s",
                 compute_graph_->GetName().c_str());
  RETURN_ON_FAIL(graph_mem_assigner_->ReAssignContinuousMemory(), "[Assign][ReAssignContinuousMemory] failed, graph:%s",
                 compute_graph_->GetName().c_str());
  RETURN_ON_FAIL(graph_mem_assigner_->AssignVarAttr2Nodes(), "[Variable][Memory] assigner failed, graph:%s",
                 compute_graph_->GetName().c_str());
  RETURN_ON_FAIL(graph_mem_assigner_->SetInputOffset(), "[Set][InputOffset] Fail! graph:%s",
                 compute_graph_->GetName().c_str());

  GE_ASSERT_SUCCESS(graph_mem_assigner_->SetAtomicCleanOffset());

  RETURN_ON_FAIL(graph_mem_assigner_->UpdateParentNodeOffset(), "[Update][ParentNodeOffset] Fail! graph:%s",
                 compute_graph_->GetName().c_str());
  RETURN_ON_FAIL(graph_mem_assigner_->CheckOffset(), "[Check][Offset] Fail! graph:%s",
                 compute_graph_->GetName().c_str());

  graph_mem_assigner_->MarkDistanceAttr();
  return SUCCESS;
}
}  // namespace ge
