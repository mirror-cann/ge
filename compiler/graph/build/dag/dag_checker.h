/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_GRAPH_BUILD_DAG_MINIDAG_DAG_CHECKER_H_
#define GE_GRAPH_BUILD_DAG_MINIDAG_DAG_CHECKER_H_

#include <cstdarg>
#include <cstdio>
#include <string>
#include "graph/build/dag/dag_log.h"
#include "graph/build/dag/dag_types.h"

namespace minidag {

inline std::string BuildAssertMsg(const char *expr) {
  return "Assert " + std::string(expr) + " failed";
}

inline std::string BuildAssertMsg(const char *expr, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  va_list args_copy;
  va_copy(args_copy, args);
  const int32_t len = vsnprintf(nullptr, 0, fmt, args_copy);
  va_end(args_copy);
  std::string msg;
  if (len > 0) {
    msg.resize(static_cast<size_t>(len + 1));
    vsnprintf(&msg[0], static_cast<size_t>(len + 1), fmt, args);
    msg.resize(static_cast<size_t>(len));
  }
  va_end(args);
  return "Assert " + std::string(expr) + " failed: " + msg;
}

}  // namespace minidag

#define MINIDAG_ASSERT_SUCCESS(expr, ...)                                             \
  do {                                                                                \
    if ((expr) != minidag::graphStatus::SUCCESS) {                                    \
      MINIDAG_LOG_ERROR("%s", minidag::BuildAssertMsg(#expr, ##__VA_ARGS__).c_str()); \
      return minidag::graphStatus::FAILED;                                            \
    }                                                                                 \
  } while (false)

#define MINIDAG_ASSERT_NOTNULL(expr, ...)                                                           \
  do {                                                                                              \
    if ((expr) == nullptr) {                                                                        \
      MINIDAG_LOG_ERROR("%s", minidag::BuildAssertMsg(#expr " is nullptr", ##__VA_ARGS__).c_str()); \
      return minidag::graphStatus::FAILED;                                                          \
    }                                                                                               \
  } while (false)

#endif  // GE_GRAPH_BUILD_DAG_MINIDAG_DAG_CHECKER_H_
