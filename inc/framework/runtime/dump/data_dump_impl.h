/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_DATA_DUMP_IMPL_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_DATA_DUMP_IMPL_H_

#include <cstdint>
#include <vector>
#include <map>
#include <string>
#include "framework/common/ge_types.h"
#include "framework/runtime/dump/model_dump_manager.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "rt_external_base.h"
#include "proto/op_mapping.pb.h"

namespace ge {
namespace dump {

class DataDumpImpl {
 public:
  DataDumpImpl();
  ~DataDumpImpl();

  Status SaveTask(const Om2TaskInfo& task_info, ModelTaskType task_type,
                  rtStream_t stream, bool is_op_debug);

  Status BuildAndLoadOpMappingInfo(const ModelDumpInfo& model_info);

  void Clear();

  // Overflow dump 相关信息
  void SetOpDebugInfo(uint32_t task_id, uint32_t stream_id, void* debug_addr);

 private:
  struct InnerTensorInfo {
    uint64_t offset;
    uint64_t device_address;
    uint64_t size;
    int32_t data_type;
    int32_t format;
    std::vector<int64_t> shape_dims;
  };

  struct InnerDumpInfo {
    uint32_t task_id;
    uint32_t stream_id;
    uint32_t context_id;
    uint32_t thread_id;
    ModelTaskType task_type;
    rtStream_t stream;
    bool is_op_debug;
    uintptr_t args_base;
    size_t args_size;
    std::string op_name;
    std::string op_type;
    bool is_raw_address;
    std::vector<InnerTensorInfo> inputs;
    std::vector<InnerTensorInfo> outputs;
    std::vector<uint64_t> workspace_addrs;
    std::vector<uint64_t> workspace_sizes;
  };

  Status ExecuteLoadDumpInfo(const toolkit::aicpu::dump::OpMappingInfo& op_mapping_info);

  Status BuildOpMappingBasicInfo(const ModelDumpInfo& model_info,
                                  toolkit::aicpu::dump::OpMappingInfo& op_mapping_info);

  Status BuildTaskList(toolkit::aicpu::dump::OpMappingInfo& op_mapping_info) const;

  void BuildTaskInputs(const InnerDumpInfo& dump_info, toolkit::aicpu::dump::Task& task) const;

  void BuildTaskOutputs(const InnerDumpInfo& dump_info, toolkit::aicpu::dump::Task& task) const;

  void BuildTaskWorkspaces(const InnerDumpInfo& dump_info, toolkit::aicpu::dump::Task& task) const;

  void BuildOpDebugTask(toolkit::aicpu::dump::OpMappingInfo& op_mapping_info) const;

  std::vector<InnerDumpInfo> task_list_;
  toolkit::aicpu::dump::OpMappingInfo op_mapping_info_;
  void* dev_mem_load_ = nullptr;
  void* step_id_dev_addr_ = nullptr;
  bool load_flag_ = false;

  // Overflow dump 相关成员
  bool is_op_debug_ = false;
  uint32_t op_debug_task_id_ = 0U;
  uint32_t op_debug_stream_id_ = 0U;
  void* op_debug_addr_ = nullptr;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_COMMON_DUMP_INTERNAL_DATA_DUMP_IMPL_H_
