/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "exe_graph/runtime/annotated_args_context.h"

#include <limits>
#include <new>
#include <securec.h>

#include "annotated_kernel_args_impl.h"
#include "graph/utils/args_format_desc_utils.h"

namespace gert {
namespace {
void AppendSlot(const uint64_t value, internal::AnnotatedKernelArgsImpl &impl) {
  const auto old_size = impl.args.size();
  impl.args.resize(old_size + internal::kKernelArgSlotSize);
  static_assert(sizeof(value) == internal::kKernelArgSlotSize, "kernel arg slot must be 8 bytes");
  auto ret = memcpy_s(impl.args.data() + old_size, internal::kKernelArgSlotSize, &value, sizeof(value));
  if (ret != EOK) {
    GELOGE(ge::FAILED, "Append kernel arg slot failed, memcpy_s failed, ret=%d", ret);
    impl.status = ge::GRAPH_FAILED;
  }
}

bool IsValidIrIndex(const uint32_t index) {
  return index <= static_cast<uint32_t>(std::numeric_limits<int32_t>::max());
}

ge::graphStatus AppendAddrArg(const ge::AddrType addr_type, const uint32_t index, const void *const addr,
                              internal::AnnotatedKernelArgsImpl *const impl) {
  if (impl == nullptr) {
    return ge::GRAPH_FAILED;
  }
  if (!IsValidIrIndex(index)) {
    impl->status = ge::GRAPH_FAILED;
    return impl->status;
  }
  ge::ArgsFormatDescUtils::Append(impl->arg_descs, addr_type, static_cast<int32_t>(index));
  AppendSlot(reinterpret_cast<uint64_t>(addr), *impl);
  return impl->status;
}
}  // namespace

AnnotatedKernelArgs::AnnotatedKernelArgs() : impl_(new (std::nothrow) internal::AnnotatedKernelArgsImpl()) {}

AnnotatedKernelArgs::AnnotatedKernelArgs(const AnnotatedKernelArgs &other)
    : impl_(new (std::nothrow) internal::AnnotatedKernelArgsImpl()) {
  const auto *other_impl = static_cast<const internal::AnnotatedKernelArgsImpl *>(other.impl_);
  auto *impl = static_cast<internal::AnnotatedKernelArgsImpl *>(impl_);
  if ((impl != nullptr) && (other_impl != nullptr)) {
    *impl = *other_impl;
  } else if (impl != nullptr) {
    impl->status = ge::GRAPH_FAILED;
  }
}

AnnotatedKernelArgs::AnnotatedKernelArgs(AnnotatedKernelArgs &&other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}

AnnotatedKernelArgs &AnnotatedKernelArgs::operator=(const AnnotatedKernelArgs &other) {
  if (this == &other) {
    return *this;
  }
  if (impl_ == nullptr) {
    impl_ = new (std::nothrow) internal::AnnotatedKernelArgsImpl();
  }
  const auto *other_impl = static_cast<const internal::AnnotatedKernelArgsImpl *>(other.impl_);
  auto *impl = static_cast<internal::AnnotatedKernelArgsImpl *>(impl_);
  if ((impl != nullptr) && (other_impl != nullptr)) {
    *impl = *other_impl;
  } else if (impl != nullptr) {
    impl->status = ge::GRAPH_FAILED;
  }
  return *this;
}

AnnotatedKernelArgs &AnnotatedKernelArgs::operator=(AnnotatedKernelArgs &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  delete static_cast<internal::AnnotatedKernelArgsImpl *>(impl_);
  impl_ = other.impl_;
  other.impl_ = nullptr;
  return *this;
}

AnnotatedKernelArgs::~AnnotatedKernelArgs() {
  delete static_cast<internal::AnnotatedKernelArgsImpl *>(impl_);
  impl_ = nullptr;
}

ge::graphStatus AnnotatedKernelArgs::AppendArg(const InputAddr &addr) {
  return AppendAddrArg(ge::AddrType::INPUT, addr.index, addr.addr,
                       static_cast<internal::AnnotatedKernelArgsImpl *>(impl_));
}

ge::graphStatus AnnotatedKernelArgs::AppendArg(const OutputAddr &addr) {
  return AppendAddrArg(ge::AddrType::OUTPUT, addr.index, addr.addr,
                       static_cast<internal::AnnotatedKernelArgsImpl *>(impl_));
}

ge::graphStatus AnnotatedKernelArgs::AppendArg(const WorkspaceAddr &addr) {
  return AppendAddrArg(ge::AddrType::WORKSPACE, addr.index, addr.addr,
                       static_cast<internal::AnnotatedKernelArgsImpl *>(impl_));
}

ge::graphStatus AnnotatedKernelArgs::AppendArg(const uint64_t value) {
  auto *impl = static_cast<internal::AnnotatedKernelArgsImpl *>(impl_);
  if (impl == nullptr) {
    return ge::GRAPH_FAILED;
  }
  impl->status = ge::ArgsFormatDescUtils::InsertCustomValue(impl->arg_descs, -1, value);
  if (impl->status != ge::GRAPH_SUCCESS) {
    return impl->status;
  }
  AppendSlot(value, *impl);
  return impl->status;
}

ge::graphStatus AnnotatedKernelArgs::ExtractArgsData(std::vector<uint8_t> &args_data,
                                                     std::vector<ge::ArgDesc> &arg_descs) const {
  return internal::ExtractArgsData(static_cast<const internal::AnnotatedKernelArgsImpl *>(impl_), args_data, arg_descs);
}

}  // namespace gert
