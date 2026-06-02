/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/load/model_manager/task_info/args_io_addrs_updater.h"
#include "graph/load/model_manager/memory_app_type_classifier.h"
#include <gtest/gtest.h>

namespace ge {

class UtestArgsIoAddrsUpdater : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(UtestArgsIoAddrsUpdater, Init_Success) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL},
    {1, 0x20000000ULL, 2048ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 1, 0ULL, 0ULL},
    {2, 0x0ULL, 0ULL, MemAllocation::ABSOLUTE, 0, 0ULL, 0ULL}
  };
  
  std::vector<uint64_t> logical_addrs = {
    0x10000000ULL + 0x1000ULL,
    0x20000000ULL + 0x2000ULL,
  };
  
  std::vector<uint64_t> mem_types = {
    static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap),
    static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)
  };
  
  ArgsIoAddrsUpdater::OpInfo op_info{"test_op", "TestOpType"};
  
  ArgsIoAddrsUpdater updater;
  Status ret = updater.Init(logical_mem_allocations, logical_addrs, mem_types, op_info);
  EXPECT_EQ(ret, SUCCESS);
  
  std::vector<MemAllocationAndOffset> mem_allocs;
  updater.GetArgsMemAllocationAndOffset(mem_allocs);
  EXPECT_EQ(mem_allocs.size(), 2UL);
  
  EXPECT_EQ(mem_allocs[0].id, 0UL);
  EXPECT_EQ(mem_allocs[0].offset, 0x1000ULL);
  
  EXPECT_EQ(mem_allocs[1].id, 1UL);
  EXPECT_EQ(mem_allocs[1].offset, 0x2000ULL);
}

TEST_F(UtestArgsIoAddrsUpdater, Init_EmptyAddrs_Success) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL}
  };
  
  std::vector<uint64_t> logical_addrs = {};
  std::vector<uint64_t> mem_types = {};
  
  ArgsIoAddrsUpdater::OpInfo op_info{"empty_op", "EmptyOp"};
  
  ArgsIoAddrsUpdater updater;
  Status ret = updater.Init(logical_mem_allocations, logical_addrs, mem_types, op_info);
  EXPECT_EQ(ret, SUCCESS);
  
  std::vector<MemAllocationAndOffset> mem_allocs;
  updater.GetArgsMemAllocationAndOffset(mem_allocs);
  EXPECT_EQ(mem_allocs.size(), 0UL);
}

TEST_F(UtestArgsIoAddrsUpdater, SetArgIoAddrs_Success) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL},
    {1, 0x20000000ULL, 2048ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 1, 0ULL, 0ULL}
  };
  
  std::vector<uint64_t> logical_addrs = {
    0x10000000ULL + 0x1000ULL,
    0x20000000ULL + 0x2000ULL,
  };
  
  std::vector<uint64_t> mem_types = {0ULL, 0ULL};
  
  ArgsIoAddrsUpdater::OpInfo op_info{"test_op", "TestOp"};
  
  ArgsIoAddrsUpdater updater;
  updater.Init(logical_mem_allocations, logical_addrs, mem_types, op_info);
  
  std::vector<uint64_t> active_mem_base_addr = {
    0x30000000ULL,
    0x40000000ULL,
  };
  
  uint64_t host_args_buffer[2] = {0ULL, 0ULL};
  
  Status ret = updater.SetArgIoAddrs(active_mem_base_addr, host_args_buffer, sizeof(host_args_buffer));
  EXPECT_EQ(ret, SUCCESS);
  
  EXPECT_EQ(host_args_buffer[0], 0x30000000ULL + 0x1000ULL);
  EXPECT_EQ(host_args_buffer[1], 0x40000000ULL + 0x2000ULL);
}

TEST_F(UtestArgsIoAddrsUpdater, GenArgsRefreshInfos_Success) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL},
    {1, 0x20000000ULL, 2048ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 1, 0ULL, 0ULL},
    {2, 0x0ULL, 0ULL, MemAllocation::ABSOLUTE, 0, 0ULL, 0ULL}
  };
  
  std::vector<uint64_t> logical_addrs = {
    0x10000000ULL + 0x1000ULL,
    0x20000000ULL + 0x2000ULL,
  };
  
  std::vector<uint64_t> mem_types = {
    static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap),
    static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)
  };
  
  ArgsIoAddrsUpdater::OpInfo op_info{"test_op", "TestOp"};
  
  ArgsIoAddrsUpdater updater;
  updater.Init(logical_mem_allocations, logical_addrs, mem_types, op_info);
  
  std::vector<TaskArgsRefreshInfo> infos;
  uint64_t host_args_base_offset = 0ULL;
  updater.GenArgsRefreshInfos(infos, host_args_base_offset, ArgsPlacement::kArgsPlacementHbm);
  
  EXPECT_EQ(infos.size(), 2UL);
  
  EXPECT_EQ(infos[0].id, 0UL);
  EXPECT_EQ(infos[0].offset, 0x1000ULL);
  EXPECT_EQ(infos[0].io_index, 0ULL);
  EXPECT_EQ(infos[0].args_offset, 0ULL);
  EXPECT_EQ(infos[0].placement, ArgsPlacement::kArgsPlacementHbm);
  
  EXPECT_EQ(infos[1].id, 1UL);
  EXPECT_EQ(infos[1].offset, 0x2000ULL);
  EXPECT_EQ(infos[1].io_index, 1ULL);
  EXPECT_EQ(infos[1].args_offset, sizeof(uint64_t));
}

TEST_F(UtestArgsIoAddrsUpdater, Init_WithRefreshableFlags_Success) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL},
    {1, 0x20000000ULL, 2048ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 1, 0ULL, 0ULL},
    {2, 0x0ULL, 0ULL, MemAllocation::ABSOLUTE, 0, 0ULL, 0ULL}
  };
  
  std::vector<uint64_t> logical_addrs = {
    0x10000000ULL + 0x1000ULL,
    0x20000000ULL + 0x2000ULL,
  };
  
  std::vector<uint8_t> refreshable_flags = {1U, 1U};
  
  ArgsIoAddrsUpdater::OpInfo op_info{"refreshable_op", "RefreshableOp"};
  
  ArgsIoAddrsUpdater updater;
  Status ret = updater.Init(logical_mem_allocations, logical_addrs, refreshable_flags, op_info);
  EXPECT_EQ(ret, SUCCESS);
  
  std::vector<MemAllocationAndOffset> mem_allocs;
  updater.GetArgsMemAllocationAndOffset(mem_allocs);
  EXPECT_EQ(mem_allocs.size(), 2UL);
  
  EXPECT_EQ(mem_allocs[0].id, 0UL);
  EXPECT_EQ(mem_allocs[1].id, 1UL);
}

TEST_F(UtestArgsIoAddrsUpdater, Init_MismatchSize_Failed) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL}
  };
  
  std::vector<uint64_t> logical_addrs = {0x10000000ULL};
  std::vector<uint64_t> mem_types = {0ULL, 0ULL};  // mismatch size
  
  ArgsIoAddrsUpdater::OpInfo op_info{"mismatch_op", "MismatchOp"};
  
  ArgsIoAddrsUpdater updater;
  Status ret = updater.Init(logical_mem_allocations, logical_addrs, mem_types, op_info);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(UtestArgsIoAddrsUpdater, Init_EmptyMemAllocations_Failed) {
  std::vector<MemAllocation> logical_mem_allocations = {};
  
  std::vector<uint64_t> logical_addrs = {0x10000000ULL};
  std::vector<uint64_t> mem_types = {0ULL};
  
  ArgsIoAddrsUpdater::OpInfo op_info{"empty_alloc_op", "EmptyAllocOp"};
  
  ArgsIoAddrsUpdater updater;
  Status ret = updater.Init(logical_mem_allocations, logical_addrs, mem_types, op_info);
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(UtestArgsIoAddrsUpdater, SetArgIoAddrs_IdOutOfBounds_Fails) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL}
  };
  
  std::vector<uint64_t> logical_addrs = {
    0x10000000ULL + 0x1000ULL,
  };
  
  std::vector<uint64_t> mem_types = {
    static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)
  };
  
  ArgsIoAddrsUpdater::OpInfo op_info{"test_op", "TestOp"};
  
  ArgsIoAddrsUpdater updater;
  updater.Init(logical_mem_allocations, logical_addrs, mem_types, op_info);
  
  std::vector<uint64_t> active_mem_base_addr = {};
  uint64_t host_args_buffer[1] = {0ULL};
  
  Status ret = updater.SetArgIoAddrs(active_mem_base_addr, host_args_buffer, sizeof(host_args_buffer));
  EXPECT_NE(ret, SUCCESS);
}

TEST_F(UtestArgsIoAddrsUpdater, Init_RefreshableFlagZero_MapsToAbsolute) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL},
    {1, 0x0ULL, 0ULL, MemAllocation::ABSOLUTE, 0, 0ULL, 0ULL}
  };
  
  std::vector<uint64_t> logical_addrs = {
    0x10000000ULL + 0x1000ULL,
  };
  
  std::vector<uint8_t> refreshable_flags = {0U};
  
  ArgsIoAddrsUpdater::OpInfo op_info{"non_refreshable_op", "NonRefreshableOp"};
  
  ArgsIoAddrsUpdater updater;
  Status ret = updater.Init(logical_mem_allocations, logical_addrs, refreshable_flags, op_info);
  EXPECT_EQ(ret, SUCCESS);
  
  std::vector<MemAllocationAndOffset> mem_allocs;
  updater.GetArgsMemAllocationAndOffset(mem_allocs);
  EXPECT_EQ(mem_allocs.size(), 1UL);
  EXPECT_EQ(mem_allocs[0].id, 1UL);
}

TEST_F(UtestArgsIoAddrsUpdater, GenArgsRefreshInfos_EmptyAddrs_NoInfos) {
  std::vector<MemAllocation> logical_mem_allocations = {
    {0, 0x10000000ULL, 1024ULL * 1024ULL, MemAllocation::FIXED_FEATURE_MAP, 0, 0ULL, 0ULL},
    {1, 0x0ULL, 0ULL, MemAllocation::ABSOLUTE, 0, 0ULL, 0ULL}
  };
  
  ArgsIoAddrsUpdater::OpInfo op_info{"empty_addrs_op", "EmptyAddrsOp"};
  
  ArgsIoAddrsUpdater updater;
  std::vector<uint64_t> empty_addrs;
  std::vector<uint8_t> empty_flags;
  updater.Init(logical_mem_allocations, empty_addrs, empty_flags, op_info);
  
  std::vector<TaskArgsRefreshInfo> infos;
  updater.GenArgsRefreshInfos(infos, 0ULL, ArgsPlacement::kArgsPlacementHbm);
  EXPECT_EQ(infos.size(), 0UL);
}

}  // namespace ge