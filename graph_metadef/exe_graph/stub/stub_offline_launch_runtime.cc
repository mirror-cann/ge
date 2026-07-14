/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "exe_graph/runtime/eager_op_execution_context.h"
#include "exe_graph/runtime/annotated_args_context.h"
#include "runtime/annotated_args_handler.h"

#include <limits>
#include <new>
#include <securec.h>
#include <utility>
#include <vector>

#include "graph/utils/args_format_desc_utils.h"

namespace gert {
namespace {
constexpr size_t kKernelArgSlotSize = sizeof(uint64_t);
constexpr int32_t kCustomValueWidthBit64 = -1;

struct StubAnnotatedKernelArgsImpl {
  std::vector<uint8_t> args;
  std::vector<ge::ArgDesc> arg_descs;
  ge::graphStatus status = ge::GRAPH_SUCCESS;
};

void AppendSlot(const uint64_t value, StubAnnotatedKernelArgsImpl &impl) {
  const auto old_size = impl.args.size();
  impl.args.resize(old_size + kKernelArgSlotSize);
  if (memcpy_s(impl.args.data() + old_size, kKernelArgSlotSize, &value, sizeof(value)) != EOK) {
    impl.status = ge::GRAPH_FAILED;
  }
}

bool IsValidIrIndex(const uint32_t index) {
  return index <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
}

ge::graphStatus AppendAddrArg(const ge::AddrType addr_type, const uint32_t index, const void *const addr,
                              StubAnnotatedKernelArgsImpl *const impl) {
  if (impl == nullptr) {
    return ge::GRAPH_FAILED;
  }
  if (!IsValidIrIndex(index)) {
    impl->status = ge::GRAPH_FAILED;
    return impl->status;
  }
  impl->arg_descs.push_back({addr_type, static_cast<int32_t>(index), false, {0}});
  AppendSlot(reinterpret_cast<uint64_t>(addr), *impl);
  return impl->status;
}

}  // namespace

AnnotatedKernelArgs::AnnotatedKernelArgs(const AnnotatedKernelArgs &other)
    : impl_(new (std::nothrow) StubAnnotatedKernelArgsImpl()) {
  const auto *other_impl = static_cast<const StubAnnotatedKernelArgsImpl *>(other.impl_);
  auto *impl = static_cast<StubAnnotatedKernelArgsImpl *>(impl_);
  if ((impl != nullptr) && (other_impl != nullptr)) {
    *impl = *other_impl;
  } else if (impl != nullptr) {
    impl->status = ge::GRAPH_FAILED;
  }
}

AnnotatedKernelArgs::AnnotatedKernelArgs() : impl_(new (std::nothrow) StubAnnotatedKernelArgsImpl()) {}

AnnotatedKernelArgs::AnnotatedKernelArgs(AnnotatedKernelArgs &&other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}

AnnotatedKernelArgs &AnnotatedKernelArgs::operator=(const AnnotatedKernelArgs &other) {
  if (this == &other) {
    return *this;
  }
  if (impl_ == nullptr) {
    impl_ = new (std::nothrow) StubAnnotatedKernelArgsImpl();
  }
  const auto *other_impl = static_cast<const StubAnnotatedKernelArgsImpl *>(other.impl_);
  auto *impl = static_cast<StubAnnotatedKernelArgsImpl *>(impl_);
  if ((impl != nullptr) && (other_impl != nullptr)) {
    *impl = *other_impl;
  } else if (impl != nullptr) {
    impl->status = ge::GRAPH_FAILED;
  }
  return *this;
}

AnnotatedKernelArgs &AnnotatedKernelArgs::operator=(AnnotatedKernelArgs &&other) noexcept {
  if (this != &other) {
    delete static_cast<StubAnnotatedKernelArgsImpl *>(impl_);
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

AnnotatedKernelArgs::~AnnotatedKernelArgs() {
  delete static_cast<StubAnnotatedKernelArgsImpl *>(impl_);
  impl_ = nullptr;
}

ge::graphStatus AnnotatedKernelArgs::AppendArg(const InputAddr &addr) {
  return AppendAddrArg(ge::AddrType::INPUT, addr.index, addr.addr, static_cast<StubAnnotatedKernelArgsImpl *>(impl_));
}

ge::graphStatus AnnotatedKernelArgs::AppendArg(const OutputAddr &addr) {
  return AppendAddrArg(ge::AddrType::OUTPUT, addr.index, addr.addr, static_cast<StubAnnotatedKernelArgsImpl *>(impl_));
}

ge::graphStatus AnnotatedKernelArgs::AppendArg(const WorkspaceAddr &addr) {
  return AppendAddrArg(ge::AddrType::WORKSPACE, addr.index, addr.addr,
                       static_cast<StubAnnotatedKernelArgsImpl *>(impl_));
}

ge::graphStatus AnnotatedKernelArgs::AppendArg(const uint64_t value) {
  auto *impl = static_cast<StubAnnotatedKernelArgsImpl *>(impl_);
  if (impl == nullptr) {
    return ge::GRAPH_FAILED;
  }
  ge::ArgDesc arg = {ge::AddrType::CUSTOM_VALUE, kCustomValueWidthBit64, false, {0}};
  if (memcpy_s(arg.reserved, sizeof(arg.reserved), &value, sizeof(value)) != EOK) {
    impl->status = ge::GRAPH_FAILED;
    return impl->status;
  }
  impl->arg_descs.emplace_back(arg);
  AppendSlot(value, *impl);
  return impl->status;
}

ge::graphStatus AnnotatedKernelArgs::ExtractArgsData(std::vector<uint8_t> &args_data,
                                                     std::vector<ge::ArgDesc> &arg_descs) const {
  const auto *const impl = static_cast<const StubAnnotatedKernelArgsImpl *>(impl_);
  if ((impl == nullptr) || (impl->status != ge::GRAPH_SUCCESS) || impl->args.empty() ||
      (impl->arg_descs.size() > (std::numeric_limits<size_t>::max() / kKernelArgSlotSize)) ||
      (impl->args.size() != (impl->arg_descs.size() * kKernelArgSlotSize))) {
    return ge::GRAPH_FAILED;
  }
  args_data = impl->args;
  arg_descs = impl->arg_descs;
  return ge::GRAPH_SUCCESS;
}

rtStream EagerOpExecutionContext::GetStream() const {
  return nullptr;
}

Tensor *EagerOpExecutionContext::MallocOutputTensor(const size_t index, const StorageShape &shape,
                                                    const StorageFormat &format, const ge::DataType dtype) {
  (void)index;
  (void)shape;
  (void)format;
  (void)dtype;
  return nullptr;
}

Tensor *EagerOpExecutionContext::MakeOutputRefInput(const size_t output_index, const size_t input_index) const {
  (void)output_index;
  (void)input_index;
  return nullptr;
}

void *EagerOpExecutionContext::MallocWorkSpace(const size_t size) {
  (void)size;
  return nullptr;
}

const KernelArgs *EagerOpExecutionContext::MallocReadOnlyDevArgs(void *const host_args, const size_t args_size) const {
  (void)host_args;
  (void)args_size;
  return nullptr;
}

WorkspaceAddr AnnotatedArgsContext::MallocWorkSpace(const size_t size) {
  (void)size;
  return WorkspaceAddr{0U, nullptr};
}

uint32_t AnnotatedArgsContext::GetStreamId() const {
  return 0U;
}

ge::graphStatus AnnotatedArgsContext::AddLaunch(const AnnotatedKernelLaunchInfo &launch_info,
                                                AnnotatedKernelArgs &&args) {
  std::vector<uint8_t> args_data;
  std::vector<ge::ArgDesc> arg_descs;
  if ((launch_info.kernel_name == nullptr) || (launch_info.kernel_name[0] == '\0') ||
      (launch_info.kernel_bin == nullptr) || (launch_info.kernel_bin_size == 0U) || (launch_info.block_dim == 0U) ||
      (args.ExtractArgsData(args_data, arg_descs) != ge::GRAPH_SUCCESS)) {
    return ge::GRAPH_FAILED;
  }
  const auto additional_output_start = GetAdditionalOutputStartIndex();
  if (additional_output_start < 0) {
    return ge::GRAPH_SUCCESS;
  }
  const int64_t handler_index = additional_output_start + static_cast<int64_t>(AdditionalOutputIndex::kArgsHandler);
  auto *handler_chain = GetOutput(handler_index);
  if (handler_chain == nullptr) {
    return ge::GRAPH_SUCCESS;
  }
  auto *args_handler = handler_chain->GetValue<ArgsHandler *>();
  if (args_handler != nullptr) {
    auto *annotated_args_handler = dynamic_cast<AnnotatedArgsHandler *>(args_handler);
    if (annotated_args_handler == nullptr) {
      return ge::GRAPH_FAILED;
    }
    annotated_args_handler->AddLaunch(launch_info.kernel_name, launch_info.kernel_bin, launch_info.kernel_bin_size,
                                      launch_info.block_dim, launch_info.stream_id, std::move(args_data),
                                      std::move(arg_descs));
  }
  return ge::GRAPH_SUCCESS;
}
}  // namespace gert
