/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "sink_op_args_handler.h"
#include "custom_task_info.h"

namespace ge {

SinkOpArgsHandler::SinkOpArgsHandler(CustomTaskInfo *task_info) : task_info_(task_info) {}

const gert::KernelArgs* SinkOpArgsHandler::MallocReadOnlyDevArgs(void *host_args, size_t args_size) {
  if (task_info_ == nullptr) {
    return nullptr;
  }
  return task_info_->MallocReadOnlyDevArgsImpl(host_args, args_size);
}

const std::deque<gert::KernelArgs>& SinkOpArgsHandler::GetKernelArgs(gert::Placement placement) const {
  if (task_info_ == nullptr) {
    static const std::deque<gert::KernelArgs> empty_deque;
    return empty_deque;
  }
  return task_info_->GetKernelArgsDeque(placement);
}

}  // namespace ge
