/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "exe_graph/runtime/update_args_context.h"
#include "exe_graph/runtime/kernel_args.h"
#include "framework/runtime/args_handler.h"
#include "common/checker.h"

namespace gert {

const KernelArgs* UpdateArgsContext::GetKernelArgs(Placement placement, size_t index) const {
  auto additional_output_start = GetAdditionalOutputStartIndex();
  GE_ASSERT_TRUE(additional_output_start >= 0);

  auto *chain = GetOutput(static_cast<size_t>(additional_output_start) +
      static_cast<size_t>(EagerOpExecutionContext::AdditionalOutputIndex::kArgsHandler));
  GE_ASSERT_NOTNULL(chain);
  auto *handler = chain->GetValue<ArgsHandler *>();
  GE_ASSERT_NOTNULL(handler);

  const auto &args_deque = handler->GetKernelArgs(placement);
  GE_ASSERT_TRUE(index < args_deque.size());
  return &args_deque[index];
}

}  // namespace gert
