/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_IMPL_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_IMPL_H_

#include <cstdint>
#include "framework/runtime/dump/model_dump_manager.h"
#include "runtime/base.h"

namespace ge {
namespace dump {

class ProfilingImpl {
 public:
  Status ReportModelLoadBegin(const ModelDumpInfo &model_info) const;
  Status ReportModelLoadEnd(const ModelDumpInfo &model_info) const;
  Status SaveTaskInfo(const Om2TaskInfo &task_info, const ModelDumpInfo &model_info) const;

 private:
  Status BuildTaskDescInfo(const Om2TaskInfo &task_info, const ModelDumpInfo &model_info,
                           TaskDescInfo &task_desc_info, uint32_t &prof_task_type) const;
  Status ReportTaskDescInfo(const TaskDescInfo &task_desc_info, uint32_t prof_task_type, uint32_t tid) const;
  Status ReportTensorInfo(const TaskDescInfo &task_desc_info, uint32_t tid) const;
  Status ReportContextIdInfo(const TaskDescInfo &task_desc_info, uint32_t tid) const;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_FRAMEWORK_RUNTIME_DUMP_PROFILING_IMPL_H_
