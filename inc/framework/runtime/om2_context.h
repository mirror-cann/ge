/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_RUNTIME_V2_CORE_OM2_CONTEXT_H_
#define AIR_CXX_RUNTIME_V2_CORE_OM2_CONTEXT_H_

#include <cstdint>
#include "framework/common/ge_visibility.h"

namespace gert {

/**
 * OM2 thread-local context for managing session-scoped configurations.
 * Each thread has its own independent context instance, supporting multi-threaded concurrent execution.
 *
 * Design rationale:
 * - Thread-Local Pattern: timeout is an execution-level temporary parameter, not a persistent session config
 * - Consistent with GEThreadLocalContext pattern (GEContext also uses thread-local storage)
 * - No session hierarchy needed: timeout doesn't need cross-execution persistence
 */
class Om2ThreadLocalContext {
 public:
  Om2ThreadLocalContext() = default;
  ~Om2ThreadLocalContext() = default;

  // Stream synchronization timeout management
  void SetStreamSyncTimeout(int32_t timeout) {
    stream_sync_timeout_ = timeout;
  }
  int32_t StreamSyncTimeout() const {
    return stream_sync_timeout_;
  }

  // Reserved for future extension: event synchronization timeout
  void SetEventSyncTimeout(int32_t timeout) {
    event_sync_timeout_ = timeout;
  }
  int32_t EventSyncTimeout() const {
    return event_sync_timeout_;
  }

 private:
  int32_t stream_sync_timeout_ = -1;  // Default value consistent with GEContext
  int32_t event_sync_timeout_ = -1;   // Reserved for future use
};

/**
 * Get the thread-local OM2 context instance.
 * Each thread gets its own independent context, enabling thread-safe concurrent execution.
 *
 * @return Reference to the thread-local Om2ThreadLocalContext instance
 */
VISIBILITY_EXPORT Om2ThreadLocalContext &GetOm2ThreadLocalContext();

}  // namespace gert

#endif  // AIR_CXX_RUNTIME_V2_CORE_OM2_CONTEXT_H_
