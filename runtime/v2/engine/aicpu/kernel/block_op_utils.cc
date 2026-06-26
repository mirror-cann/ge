/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "block_op_utils.h"
#include "base/err_msg.h"
#include "acl/acl_rt.h"

namespace gert {
ge::Status CheckDeviceSupportBlockingAicpuOpProcess(bool &is_support) {
  // 默认认为支持该能力
  is_support = true;
  return ge::SUCCESS;
}

ge::Status DistributeWaitTaskForAicpuBlockingOp(rtStream_t stream, const AicpuArgsHandler *arg_handler,
                                                const char *op_name) {
  const auto rt_event = arg_handler->GetRtEvent();
  GE_ASSERT_NOTNULL(rt_event);
  GE_ASSERT_RT_OK(rtSetTaskTag(op_name));
  uint32_t async_timeout = arg_handler->GetAsyncTimeout();
  GELOGI("Async timeout:0x%x.", async_timeout);
  if (async_timeout != 0xFFFFFFFF) {
    GE_ASSERT_RT_OK(aclrtStreamWaitEventWithTimeout(stream, rt_event, static_cast<int32_t>(async_timeout)));
  } else {
    GE_ASSERT_RT_OK(aclrtStreamWaitEvent(stream, rt_event));
  }

  GE_ASSERT_RT_OK(rtSetTaskTag(op_name));
  GE_ASSERT_RT_OK(aclrtResetEvent(rt_event, stream));

  return ge::SUCCESS;
}
}  // namespace gert
