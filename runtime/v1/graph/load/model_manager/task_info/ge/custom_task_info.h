/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_EAGER_TASK_INFO_H
#define CANN_GRAPH_ENGINE_EAGER_TASK_INFO_H

#include "graph/args_format_desc.h"
#include "graph/op_desc.h"
#include "graph/def_types.h"
#include "graph/load/model_manager/task_info/task_info.h"
#include "graph/load/model_manager/task_info/args_format/args_format_utils.h"
#include "graph/load/model_manager/task_info/args_io_addrs_updater.h"
#include "graph/ge_context.h"
#include "graph/utils/attr_utils.h"
#include "framework/omg/parser/parser_types.h"
#include "framework/common/framework_types_internal.h"
#include "common/dump/dump_op.h"
#include "graph/load/model_manager/sink_only_allocator.h"
#include "register/op_tiling_registry.h"
#include "graph/custom_op.h"
#include "graph/custom_op/args_refresh.h"
#include "exe_graph/runtime/eager_op_execution_context.h"
#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>
#include "framework/runtime/args_handler.h"

namespace ge {
struct CustomArgsFormatInfo {
  std::map<size_t, std::pair<size_t, size_t>> ir_input_2_range;
  std::map<size_t, std::pair<size_t, size_t>> ir_output_2_range;
  std::vector<ArgDesc> arg_descs;
};
class SinkOpArgsHandler;
class CustomTaskInfo : public TaskInfo {
  friend class SinkOpArgsHandler;

 public:
  CustomTaskInfo() = default;
  ~CustomTaskInfo() override {
    davinci_model_ = nullptr;
  }

  Status ParseTaskRunParam(const domi::TaskDef &task_def, DavinciModel *const davinci_model,
                           TaskRunParam &task_run_param) override;

  Status Init(const domi::TaskDef &task_def, DavinciModel *const davinci_model, const PisToArgs &args = {},
              const PisToPersistentWorkspace &persistent_workspace = {},
              const IowAddrs &iow_addrs = {{}, {}, {}}) override;

  Status Distribute() override;

  Status Release() override;

  uint32_t GetTaskID() const override {
    return task_id_;
  }

  uint32_t GetStreamId() const override {
    return stream_id_;
  }

  void PostProcess(const domi::TaskDef &task_def) override;

  int64_t ParseOpIndex(const domi::TaskDef &task_def) const override;

  const std::vector<ArgsAllocationResult> &GetArgsAllocationResults() const override {
    return args_allocation_results_;
  }

  bool NeedReserveArgsTable() const override {
    return args_refresh_strategy_ == ArgsRefreshStrategy::kUpdateCallback;
  }

  Status UpdateHostArgs(void *base_addr, size_t mem_size) override;

  Status GetTaskArgsRefreshInfos(std::vector<TaskArgsRefreshInfo> &infos) override;

  ArgsRefreshStrategy GetArgsRefreshStrategy() const {
    return args_refresh_strategy_;
  }

 private:
  Status InsertDumpOp(const std::string &dump_mode);
  Status UpdateCustomDumpAddrs(const std::vector<uint64_t> &input_addrs_value,
                               const std::vector<uint64_t> &output_addrs_value);
  void SetCustomDumpInfo(const DumpProperties &dump_properties, DumpOp &dump_op) const;

  void UpdateIoAndWorkspaceAddrs(const IowAddrs &iow_addrs);

  const std::deque<gert::KernelArgs> &GetKernelArgsDeque(gert::Placement placement) const;

  Status ParseAnnotatedArgsTaskRunParam(const domi::KernelDef &kernel_def, const domi::KernelContext &context,
                                        TaskRunParam &task_run_param);

  Status ConstructCustomKernelContextInputsOutputs(const ge::OpDescPtr &op_desc,
                                                   std::vector<std::unique_ptr<uint8_t[]>> &inputs,
                                                   std::vector<std::unique_ptr<uint8_t[]>> &outputs) const;
  Status DistributeAnnotatedArgsFromTaskDef();

  Status InitArgsIoAddrsUpdater();

  Status AssembleIoByArgsFormat();
  size_t GetArgsSizeByFormat() const;
  void AppendIoAddr(const uint64_t addr, const uint64_t addr_type);
  Status AppendInputOutputAddr(size_t ir_idx, bool is_input);
  Status AppendWorkspaceAddr(int32_t ir_idx);

  const gert::KernelArgs *MallocReadOnlyDevArgsImpl(void *host_args, size_t args_size);

  DavinciModel *davinci_model_{};
  uint32_t task_id_{0U};
  uint32_t stream_id_{0U};
  OpDescPtr op_desc_;

  ArgsPlacement args_placement_{ArgsPlacement::kArgsPlacementHbm};
  std::vector<uint64_t> input_data_addrs_;
  std::vector<uint64_t> output_data_addrs_;
  std::vector<uint64_t> workspace_addrs_;
  std::vector<uint64_t> input_mem_types_;
  std::vector<uint64_t> output_mem_types_;
  std::vector<uint64_t> workspace_mem_types_;
  gert::KernelContextHolder eager_context_holder_{};
  std::shared_ptr<gert::memory::SinkOnlyAllocator> sink_only_allocator_;
  DumpOp input_custom_dump_;
  DumpOp output_custom_dump_;

  ArgsIoAddrsUpdater args_io_addrs_updater_;
  ArgsUpdater *args_update_op_ = nullptr;
  ArgsRefreshStrategy args_refresh_strategy_ = ArgsRefreshStrategy::kNone;
  bool is_args_refreshable_ = false;
  size_t input_count_ = 0;
  size_t output_count_ = 0;
  std::unique_ptr<gert::ArgsHandler> args_handler_;
  std::deque<gert::KernelArgs> kernel_args_host_deque_;
  std::deque<gert::KernelArgs> kernel_args_device_deque_;
  std::vector<ArgsAllocationResult> args_allocation_results_;
  std::vector<gert::GertMemBlock *> offline_workspace_blocks_;
  std::vector<void *> ws_vec_;
  std::vector<std::unique_ptr<uint8_t[]>> inputs_holder_;
  std::vector<std::unique_ptr<uint8_t[]>> outputs_holder_;
  std::string kernel_name_;
  uint32_t block_dim_ = 1U;

  CustomArgsFormatInfo args_format_holder_;
  void *args_ = nullptr;
  std::vector<uint64_t> io_addrs_;
  std::vector<uint64_t> io_addr_mem_types_;
  // io_addr_offset_ is 0 because custom op args buffer contains only io addrs (no tiling/header prefix)
  size_t io_addr_offset_ = 0UL;
};
}  // namespace ge
#endif  // CANN_GRAPH_ENGINE_EAGER_TASK_INFO_H
