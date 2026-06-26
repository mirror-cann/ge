/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_MINIDAG_DAG_LOG_H_
#define GE_GRAPH_BUILD_DAG_MINIDAG_DAG_LOG_H_

#include <cinttypes>
#include <cstdint>
#include "dlog_pub.h"

#ifdef __GNUC__
#include <unistd.h>
#include <sys/syscall.h>
#endif

namespace minidag {

inline uint64_t GetTid() {
#ifdef __GNUC__
  thread_local static uint64_t tid = static_cast<uint64_t>(syscall(__NR_gettid));
  return tid;
#else
  return 0U;
#endif
}

constexpr int32_t kModuleId = static_cast<int32_t>(GE);

}  // namespace minidag

#define MINIDAG_LOG_DEBUG(fmt, ...) \
  dlog_debug(minidag::kModuleId, "%" PRIu64 " %s:" fmt, minidag::GetTid(), __FUNCTION__, ##__VA_ARGS__)

#define MINIDAG_LOG_INFO(fmt, ...) \
  dlog_info(minidag::kModuleId, "%" PRIu64 " %s:" fmt, minidag::GetTid(), __FUNCTION__, ##__VA_ARGS__)

#define MINIDAG_LOG_WARN(fmt, ...) \
  dlog_warn(minidag::kModuleId, "%" PRIu64 " %s:" fmt, minidag::GetTid(), __FUNCTION__, ##__VA_ARGS__)

#define MINIDAG_LOG_ERROR(fmt, ...) \
  dlog_error(minidag::kModuleId, "%" PRIu64 " %s:" fmt, minidag::GetTid(), __FUNCTION__, ##__VA_ARGS__)

#endif  // GE_GRAPH_BUILD_DAG_MINIDAG_DAG_LOG_H_
