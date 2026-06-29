/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include "securec.h"
#include "engine/custom/kernel/eager_args_handler.h"
#include "graph_metadef/depends/faker/allocator_faker.h"

namespace gert {
namespace {

class TrackingAllocatorFaker : public AllocatorFaker {
 public:
  class TrackingGertMemBlockFaker : public GertMemBlockFaker {
   public:
    explicit TrackingGertMemBlockFaker(void *addr, size_t *free_count)
        : GertMemBlockFaker(addr), free_count_(free_count) {}
    void Free(int64_t stream_id) override {
      if (free_count_ != nullptr) {
        ++(*free_count_);
      }
      GertMemBlockFaker::Free(stream_id);
    }
   private:
    size_t *free_count_;
  };

  TrackingAllocatorFaker() = default;
  ~TrackingAllocatorFaker() override {
    for (auto *block : blocks_) {
      delete block;
    }
  }
  size_t GetFreeCount() const { return free_count_; }

  GertMemBlock *Malloc(size_t size) override {
    void *addr = malloc(size);
    if (addr == nullptr) {
      return nullptr;
    }
    auto *block = new TrackingGertMemBlockFaker(addr, &free_count_);
    blocks_.push_back(block);
    return block;
  }
 private:
  size_t free_count_ = 0;
  std::vector<GertMemBlock *> blocks_;
};

class EagerArgsHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    allocator_ = std::make_unique<AllocatorFaker>();
    stream_id_ = 123;
  }

  void TearDown() override {
    handler_.reset();
    allocator_.reset();
  }

  std::unique_ptr<AllocatorFaker> allocator_;
  std::unique_ptr<EagerArgsHandler> handler_;
  int64_t stream_id_;
};

TEST_F(EagerArgsHandlerTest, MallocReadOnlyDevArgs_Success) {
  handler_ = std::make_unique<EagerArgsHandler>(allocator_.get(), stream_id_);
  uint8_t host_data[1024] = {};
  for (size_t i = 0; i < sizeof(host_data); ++i) {
    host_data[i] = static_cast<uint8_t>(i & 0xFF);
  }

  const KernelArgs *result = handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));

  ASSERT_NE(result, nullptr);
  EXPECT_NE(result->args_data, nullptr);
  EXPECT_EQ(result->args_size, sizeof(host_data));
  EXPECT_EQ(result->placement, ge::kPlacementDevice);
  EXPECT_EQ(memcmp(result->args_data, host_data, sizeof(host_data)), 0);
}

TEST_F(EagerArgsHandlerTest, MallocReadOnlyDevArgs_MultipleCallsReturnIndependentArgs) {
  handler_ = std::make_unique<EagerArgsHandler>(allocator_.get(), stream_id_);
  uint8_t host_data1[512] = {};
  uint8_t host_data2[256] = {};
  memset_s(host_data1, sizeof(host_data1), 0xAA, sizeof(host_data1));
  memset_s(host_data2, sizeof(host_data2), 0xBB, sizeof(host_data2));

  const KernelArgs *result1 = handler_->MallocReadOnlyDevArgs(host_data1, sizeof(host_data1));
  const KernelArgs *result2 = handler_->MallocReadOnlyDevArgs(host_data2, sizeof(host_data2));

  ASSERT_NE(result1, nullptr);
  ASSERT_NE(result2, nullptr);
  EXPECT_NE(result1->args_data, result2->args_data);
  EXPECT_EQ(result1->args_size, sizeof(host_data1));
  EXPECT_EQ(result2->args_size, sizeof(host_data2));
  EXPECT_EQ(memcmp(result1->args_data, host_data1, sizeof(host_data1)), 0);
  EXPECT_EQ(memcmp(result2->args_data, host_data2, sizeof(host_data2)), 0);
}

TEST_F(EagerArgsHandlerTest, Release_ClearsAllAllocatedBlocks) {
  auto tracking_allocator = std::make_unique<TrackingAllocatorFaker>();
  handler_ = std::make_unique<EagerArgsHandler>(tracking_allocator.get(), stream_id_);
  uint8_t host_data[512] = {};

  handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));
  handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));

  const auto &args_before = handler_->GetKernelArgs(ge::kPlacementDevice);
  EXPECT_EQ(args_before.size(), 2U);
  EXPECT_EQ(tracking_allocator->GetFreeCount(), 0U);

  handler_->Release();

  const auto &args_after = handler_->GetKernelArgs(ge::kPlacementDevice);
  EXPECT_EQ(args_after.size(), 0U);
  EXPECT_EQ(tracking_allocator->GetFreeCount(), 2U);
}

TEST_F(EagerArgsHandlerTest, GetKernelArgs_DevicePlacement_ReturnsDeviceArgs) {
  handler_ = std::make_unique<EagerArgsHandler>(allocator_.get(), stream_id_);
  uint8_t host_data[256] = {};

  handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));

  const auto &args = handler_->GetKernelArgs(ge::kPlacementDevice);
  ASSERT_EQ(args.size(), 1U);
  EXPECT_EQ(args[0].placement, ge::kPlacementDevice);
  EXPECT_EQ(args[0].args_size, sizeof(host_data));
}

TEST_F(EagerArgsHandlerTest, GetKernelArgs_HostPlacement_ReturnsEmpty) {
  handler_ = std::make_unique<EagerArgsHandler>(allocator_.get(), stream_id_);
  uint8_t host_data[256] = {};

  handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));

  const auto &args = handler_->GetKernelArgs(ge::kPlacementHost);
  EXPECT_EQ(args.size(), 0U);
}

TEST_F(EagerArgsHandlerTest, Destructor_CallsReleaseWithoutCrash) {
  auto handler = std::make_unique<EagerArgsHandler>(allocator_.get(), stream_id_);
  uint8_t host_data[256] = {};
  handler->MallocReadOnlyDevArgs(host_data, sizeof(host_data));
  handler->MallocReadOnlyDevArgs(host_data, sizeof(host_data));
  handler.reset();
}

TEST_F(EagerArgsHandlerTest, Release_CanBeCalledMultipleTimes) {
  handler_ = std::make_unique<EagerArgsHandler>(allocator_.get(), stream_id_);
  uint8_t host_data[256] = {};
  handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));

  handler_->Release();
  handler_->Release();

  const auto &args = handler_->GetKernelArgs(ge::kPlacementDevice);
  EXPECT_EQ(args.size(), 0U);
}

TEST_F(EagerArgsHandlerTest, MallocReadOnlyDevArgs_AfterRelease) {
  handler_ = std::make_unique<EagerArgsHandler>(allocator_.get(), stream_id_);
  uint8_t host_data[256] = {};
  memset_s(host_data, sizeof(host_data), 0xCC, sizeof(host_data));

  handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));
  handler_->Release();

  const KernelArgs *result = handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->args_size, sizeof(host_data));
  EXPECT_EQ(result->placement, ge::kPlacementDevice);

  const auto &args = handler_->GetKernelArgs(ge::kPlacementDevice);
  EXPECT_EQ(args.size(), 1U);
}

TEST_F(EagerArgsHandlerTest, MallocReadOnlyDevArgs_WithoutInitialize_ReturnsNullptr) {
  handler_ = std::make_unique<EagerArgsHandler>();
  uint8_t host_data[256] = {};

  const KernelArgs *result = handler_->MallocReadOnlyDevArgs(host_data, sizeof(host_data));
  EXPECT_EQ(result, nullptr);
}

}  // namespace
}  // namespace gert
