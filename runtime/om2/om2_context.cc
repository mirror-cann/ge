/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "runtime/om2_context.h"

namespace gert {

/**
 * Thread-local storage implementation for OM2 context.
 * Uses static thread_local to ensure each thread has its own independent context instance.
 * This design enables thread-safe concurrent execution without locks or synchronization.
 */
Om2ThreadLocalContext &GetOm2ThreadLocalContext() {
  static thread_local Om2ThreadLocalContext thread_context;
  return thread_context;
}

}  // namespace gert
