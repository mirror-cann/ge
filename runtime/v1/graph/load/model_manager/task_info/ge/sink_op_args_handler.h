/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_RUNTIME_V1_GRAPH_LOAD_MODEL_MANAGER_TASK_INFO_GE_SINK_OP_ARGS_HANDLER_H_
#define GE_RUNTIME_V1_GRAPH_LOAD_MODEL_MANAGER_TASK_INFO_GE_SINK_OP_ARGS_HANDLER_H_

#include "framework/runtime/args_handler.h"

namespace ge {
class CustomTaskInfo;

class SinkOpArgsHandler : public gert::ArgsHandler {
 public:
  explicit SinkOpArgsHandler(CustomTaskInfo *task_info);
  ~SinkOpArgsHandler() override = default;

  const gert::KernelArgs *MallocReadOnlyDevArgs(void *host_args, size_t args_size) override;
  const std::deque<gert::KernelArgs> &GetKernelArgs(gert::Placement placement) const override;

 private:
  CustomTaskInfo *task_info_;
};

}  // namespace ge

#endif  // GE_RUNTIME_V1_GRAPH_LOAD_MODEL_MANAGER_TASK_INFO_GE_SINK_OP_ARGS_HANDLER_H_
