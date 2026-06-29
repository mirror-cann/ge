/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "eager_args_handler.h"
#include "acl/acl_rt.h"
#include "common/checker.h"

namespace gert {

EagerArgsHandler::EagerArgsHandler(GertAllocator *allocator, int64_t stream_id)
    : allocator_(allocator), stream_id_(stream_id) {}

void EagerArgsHandler::Initialize(GertAllocator *allocator, int64_t stream_id) {
  allocator_ = allocator;
  stream_id_ = stream_id;
}

const KernelArgs* EagerArgsHandler::MallocReadOnlyDevArgs(void *host_args, size_t args_size) {
  GE_ASSERT_NOTNULL(allocator_);
  GE_ASSERT_NOTNULL(host_args);
  GE_ASSERT_TRUE(args_size > 0);

  auto *block = allocator_->Malloc(args_size);
  GE_ASSERT_NOTNULL(block);

  auto ret = aclrtMemcpy(block->GetAddr(), args_size, host_args, args_size, ACL_MEMCPY_HOST_TO_DEVICE);
  GE_ASSERT_RT_OK(ret);
  allocated_blocks_.push_back(block);

  KernelArgs args;
  args.args_data = block->GetAddr();
  args.args_size = args_size;
  args.placement = Placement::kPlacementDevice;
  device_args_.push_back(args);

  return &device_args_.back();
}

const std::deque<KernelArgs>& EagerArgsHandler::GetKernelArgs(Placement placement) const {
  if (placement == Placement::kPlacementHost) {
    static const std::deque<KernelArgs> kEmptyArgs;
    return kEmptyArgs;
  }
  return device_args_;
}

void EagerArgsHandler::Release() {
  for (auto *block : allocated_blocks_) {
    if (block != nullptr) {
      block->Free(stream_id_);
    }
  }
  allocated_blocks_.clear();
  device_args_.clear();
}

}  // namespace gert
