/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_EXCEPTION_DUMP_IMPL_H_
#define GE_COMMON_DUMP_INTERNAL_EXCEPTION_DUMP_IMPL_H_

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include "framework/common/ge_types.h"
#include "framework/runtime/dump/model_dump_manager.h"

namespace ge {
namespace dump {

class ExceptionDumpImpl {
 public:
  explicit ExceptionDumpImpl(uint32_t device_id = 0U);
  ~ExceptionDumpImpl();

  Status SaveOpInfo(const Om2TaskInfo& task_info);

  Status GetOpDescInfo(uint32_t task_id, uint32_t stream_id,
                       OpDescInfo& op_info) const;

  void Clear();

  void SetDeviceId(uint32_t device_id) { device_id_ = device_id; }

 private:
  Status ReportL1ExceptionDumpInfo(const Om2TaskInfo& task_info, const OpDescInfo& op_info);

  uint32_t device_id_{0U};
  std::map<uint64_t, OpDescInfo> op_info_map_;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_COMMON_DUMP_INTERNAL_EXCEPTION_DUMP_IMPL_H_
