/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef METADEF_CXX_INC_EXE_GRAPH_RUNTIME_UPDATE_ARGS_CONTEXT_H_
#define METADEF_CXX_INC_EXE_GRAPH_RUNTIME_UPDATE_ARGS_CONTEXT_H_

#include "exe_graph/runtime/eager_op_execution_context.h"
#include "exe_graph/runtime/kernel_args.h"

namespace gert {
class UpdateArgsContext : public EagerOpExecutionContext {
 public:
  /**
   * 获取指定位置和索引的 kernel args。
   *
   * @param placement kPlacementHost 获取 host args，kPlacementDevice 获取 device args。
   * @param index args 索引，与 MallocReadOnlyDevArgs 的调用顺序对应（首次 malloc
   *               对应 index 0，后续依次递增）。
   * @return 只读 KernelArgs 指针，失败返回 nullptr。
   */
  const KernelArgs *GetKernelArgs(Placement placement, size_t index) const;
};

static_assert(std::is_standard_layout<UpdateArgsContext>::value, "UpdateArgsContext must be standard layout");
static_assert(sizeof(UpdateArgsContext) == sizeof(EagerOpExecutionContext),
              "POD constraint violated: UpdateArgsContext must not add member variables");
}  // namespace gert

#endif  // METADEF_CXX_INC_EXE_GRAPH_RUNTIME_UPDATE_ARGS_CONTEXT_H_
