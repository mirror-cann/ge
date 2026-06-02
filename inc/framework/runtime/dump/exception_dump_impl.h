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
#define GE_FRAMEWORK_RUNTIME_DUMP_EXCEPTION_DUMP_IMPL_H_

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include "framework/common/ge_types.h"
#include "framework/runtime/dump/model_dump_manager.h"

namespace Adx {
struct TensorInfoV2;
}  // namespace Adx

namespace ge {
class AdumpOpInfoBuilder;
namespace dump {

class ExceptionDumpImpl {
 public:
  explicit ExceptionDumpImpl(uint32_t device_id = 0U);
  ~ExceptionDumpImpl();

  Status SaveOpInfo(const Om2TaskInfo& task_info);

  Status ReportL0ExceptionDumpInfo(const Om2TaskInfo& task_info) const;

  bool GetOpDescInfo(const OpDescInfoId& op_id, OpDescInfo& op_info) const;

  void Clear();

  void SetDeviceId(uint32_t device_id) { device_id_ = device_id; }

 private:
  Status ReportL1ExceptionDumpInfo(const Om2TaskInfo& task_info, const OpDescInfo& op_info) const;

  void FillAdumpOpInfoBuilder(const OpDescInfo& op_info,
                              std::vector<Adx::TensorInfoV2>& input_infos,
                              std::vector<Adx::TensorInfoV2>& output_infos,
                              std::vector<Adx::TensorInfoV2>& workspace_infos,
                              AdumpOpInfoBuilder& builder) const;

  Status SubmitToAdump(const char* op_name, const Om2TaskInfo& task_info, const OpDescInfo& op_info,
                       std::vector<Adx::TensorInfoV2>& input_infos,
                       std::vector<Adx::TensorInfoV2>& output_infos,
                       std::vector<Adx::TensorInfoV2>& workspace_infos,
                       const AdumpOpInfoBuilder& builder) const;

  uint32_t device_id_{0U};
  std::vector<OpDescInfo> op_info_list_;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_COMMON_DUMP_INTERNAL_EXCEPTION_DUMP_IMPL_H_
