/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_OVERFLOW_DUMP_IMPL_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_OVERFLOW_DUMP_IMPL_H_

#include <cstdint>
#include "framework/common/ge_types.h"
#include "runtime/rt.h"

namespace ge {
namespace dump {

class OverflowDumpImpl {
 public:
  OverflowDumpImpl();
  ~OverflowDumpImpl();

  Status RegisterForModel(rtModel_t rt_model_handle);
  void UnregisterForModel(rtModel_t rt_model_handle);

  bool IsOpDebugEnabled() const;
  uint32_t GetOpDebugTaskId() const;
  uint32_t GetOpDebugStreamId() const;
  void* GetOpDebugAddr() const;  // 返回 p2p 地址

  void Clear();

 private:
  Status MallocMemForOpdebug();

  bool is_op_debug_enabled_ = false;
  uint32_t op_debug_task_id_ = 0U;
  uint32_t op_debug_stream_id_ = 0U;
  void* op_debug_addr_ = nullptr;
  void* p2p_debug_addr_ = nullptr;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_COMMON_DUMP_INTERNAL_OVERFLOW_DUMP_IMPL_H_
