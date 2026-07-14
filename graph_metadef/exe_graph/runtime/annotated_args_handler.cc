/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "annotated_args_handler.h"

#include <utility>

#include "graph/utils/args_format_desc_utils.h"

namespace gert {
AnnotatedArgsHandler::LaunchRecord::LaunchRecord(const char *const kernel_name, const void *const kernel_bin,
                                                 const size_t kernel_bin_size, const uint32_t block_dim,
                                                 const uint32_t stream_id, std::vector<uint8_t> &&args_data,
                                                 std::vector<ge::ArgDesc> &&arg_descs)
    : kernel_name_(kernel_name),
      kernel_bin_(static_cast<const uint8_t *>(kernel_bin), static_cast<const uint8_t *>(kernel_bin) + kernel_bin_size),
      args_data_(std::move(args_data)),
      arg_descs_(std::move(arg_descs)),
      block_dim_(block_dim),
      stream_id_(stream_id) {}

AnnotatedArgsHandler::LaunchRecord::LaunchRecord(LaunchRecord &&other) noexcept = default;

AnnotatedArgsHandler::LaunchRecord &AnnotatedArgsHandler::LaunchRecord::operator=(LaunchRecord &&other) = default;

AnnotatedArgsHandler::LaunchRecord::~LaunchRecord() = default;

const char *AnnotatedArgsHandler::LaunchRecord::GetKernelName() const {
  return kernel_name_.c_str();
}

const uint8_t *AnnotatedArgsHandler::LaunchRecord::GetKernelBinData() const {
  if (kernel_bin_.empty()) {
    return nullptr;
  }
  return kernel_bin_.data();
}

size_t AnnotatedArgsHandler::LaunchRecord::GetKernelBinSize() const {
  return kernel_bin_.size();
}

const uint8_t *AnnotatedArgsHandler::LaunchRecord::GetArgsData() const {
  if (args_data_.empty()) {
    return nullptr;
  }
  return args_data_.data();
}

size_t AnnotatedArgsHandler::LaunchRecord::GetArgsSize() const {
  return args_data_.size();
}

const ge::ArgDesc *AnnotatedArgsHandler::LaunchRecord::GetArgDescs() const {
  if (arg_descs_.empty()) {
    return nullptr;
  }
  return arg_descs_.data();
}

size_t AnnotatedArgsHandler::LaunchRecord::GetArgDescCount() const {
  return arg_descs_.size();
}

uint32_t AnnotatedArgsHandler::LaunchRecord::GetBlockDim() const {
  return block_dim_;
}

uint32_t AnnotatedArgsHandler::LaunchRecord::GetStreamId() const {
  return stream_id_;
}

AnnotatedArgsHandler::~AnnotatedArgsHandler() = default;

const KernelArgs *AnnotatedArgsHandler::MallocReadOnlyDevArgs(void *const host_args, const size_t args_size) {
  (void)host_args;
  (void)args_size;
  return nullptr;
}

const std::deque<KernelArgs> &AnnotatedArgsHandler::GetKernelArgs(const Placement placement) const {
  (void)placement;
  static const std::deque<KernelArgs> kEmptyArgs;
  return kEmptyArgs;
}

size_t AnnotatedArgsHandler::GetLaunchCount() const {
  return launch_records_.size();
}

const AnnotatedArgsHandler::LaunchRecord *AnnotatedArgsHandler::GetLaunch(const size_t index) const {
  if (index >= launch_records_.size()) {
    return nullptr;
  }
  return &launch_records_[index];
}

void AnnotatedArgsHandler::AddLaunch(const char *const kernel_name, const void *const kernel_bin,
                                     const size_t kernel_bin_size, const uint32_t block_dim, const uint32_t stream_id,
                                     std::vector<uint8_t> &&args_data, std::vector<ge::ArgDesc> &&arg_descs) {
  LaunchRecord launch_record(kernel_name, kernel_bin, kernel_bin_size, block_dim, stream_id, std::move(args_data),
                             std::move(arg_descs));
  (void)launch_records_.emplace_back(std::move(launch_record));
}
}  // namespace gert
