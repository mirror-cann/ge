/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <unistd.h>
#include <sys/mman.h>

#include "macro_utils/dt_public_scope.h"
#include "graph/manager/host_mem_manager.h"
#include "common/aclrt_malloc_helper.h"
#include "securec.h"
#include "depends/ascendcl/src/ascendcl_stub.h"
#include "macro_utils/dt_public_unscope.h"

namespace ge {
class UtestSharedMemAllocatorTest : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UtestSharedMemAllocatorTest, malloc_zero_size) {
  SharedMemAllocator allocator;
  string var_name = "host_params";
  SharedMemInfo info;
  uint8_t tmp(0);
  info.device_address = &tmp;

  std::shared_ptr<AlignedPtr> aligned_ptr = std::make_shared<AlignedPtr>(100, 16);

  info.host_aligned_ptr = aligned_ptr;
  info.fd = 0;
  info.mem_size = 100;
  info.op_name = var_name;
  info.shm_name = var_name;
  EXPECT_EQ(allocator.Allocate(info), SUCCESS);
  EXPECT_EQ(allocator.DeAllocate(info), SUCCESS);
}

class UtestAclrtMallocHostSharedMemTest : public testing::Test {
 protected:
  void TearDown() override {
    for (const auto &n : shm_names_) {
      shm_unlink(n.c_str());
    }
    shm_names_.clear();
    AclRuntimeStub::Reset();
  }
  std::string MakeShmName() {
    std::string name = "/ge_ut_shm_" + std::to_string(getpid()) + "_" + std::to_string(counter_++);
    shm_names_.push_back(name);
    return name;
  }
  uint64_t counter_ = 0U;
  std::vector<std::string> shm_names_;
};

TEST_F(UtestAclrtMallocHostSharedMemTest, invalid_param_null_name) {
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  EXPECT_EQ(AclrtMallocHostSharedMemory(nullptr, 4096, &fd, &host_ptr, &dev_ptr), ACL_ERROR_INVALID_PARAM);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, invalid_param_null_fd) {
  char name[] = "/test";
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name, 4096, nullptr, &host_ptr, &dev_ptr), ACL_ERROR_INVALID_PARAM);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, invalid_param_null_host_ptr) {
  char name[] = "/test";
  int32_t fd = -1;
  void *dev_ptr = nullptr;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name, 4096, &fd, nullptr, &dev_ptr), ACL_ERROR_INVALID_PARAM);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, invalid_param_null_dev_ptr) {
  char name[] = "/test";
  int32_t fd = -1;
  void *host_ptr = nullptr;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name, 4096, &fd, &host_ptr, nullptr), ACL_ERROR_INVALID_PARAM);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, new_shm_success) {
  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 2UL * 1024UL * 1024UL;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  EXPECT_GE(fd, 0);
  EXPECT_NE(host_ptr, nullptr);
  EXPECT_NE(dev_ptr, nullptr);
  const uint64_t kHugePageSize = 2UL * 1024UL * 1024UL;
  for (uint64_t offset = 0; offset < size; offset += kHugePageSize) {
    uint64_t val = *reinterpret_cast<uint64_t *>(static_cast<char *>(host_ptr) + offset);
    EXPECT_EQ(val, 0ULL);
  }
  EXPECT_EQ(AclrtFreeHostSharedMemory(name.c_str(), size, fd, host_ptr), ACL_SUCCESS);
  host_ptr = nullptr;
}

TEST_F(UtestAclrtMallocHostSharedMemTest, existing_shm_same_size) {
  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 4096U;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  EXPECT_EQ(AclrtFreeHostSharedMemory(name.c_str(), size, fd, host_ptr), ACL_SUCCESS);
  host_ptr = nullptr;
  fd = -1;
  host_ptr = nullptr;
  dev_ptr = nullptr;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  EXPECT_NE(host_ptr, nullptr);
  EXPECT_EQ(AclrtFreeHostSharedMemory(name.c_str(), size, fd, host_ptr), ACL_SUCCESS);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, existing_shm_size_mismatch) {
  std::string name = MakeShmName();
  const uint64_t size1 = 4096U;
  const uint64_t size2 = 8192U;
  int32_t pre_fd = shm_open(name.c_str(), O_RDWR | O_CREAT, 0666);
  EXPECT_GE(pre_fd, 0);
  EXPECT_EQ(ftruncate(pre_fd, static_cast<off_t>(size1)), 0);
  close(pre_fd);
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size2, &fd, &host_ptr, &dev_ptr), ACL_ERROR_FAILURE);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, host_register_fail) {
  class MockAclRuntime : public AclRuntimeStub {
   public:
    aclError aclrtHostRegister(void *ptr, uint64_t sz, aclrtHostRegisterType type, void **devPtr) override {
      return ACL_ERROR_RT_INTERNAL_ERROR;
    }
  };
  AclRuntimeStub::SetInstance(std::make_shared<MockAclRuntime>());

  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 4096U;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_ERROR_RT_INTERNAL_ERROR);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, host_unregister_fail) {
  class MockAclRuntime : public AclRuntimeStub {
   public:
    aclError aclrtHostUnregister(void *ptr) override {
      return ACL_ERROR_RT_INTERNAL_ERROR;
    }
  };
  AclRuntimeStub::SetInstance(std::make_shared<MockAclRuntime>());

  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 4096U;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  EXPECT_EQ(AclrtFreeHostSharedMemory(name.c_str(), size, fd, host_ptr), ACL_ERROR_RT_INTERNAL_ERROR);
  munmap(host_ptr, size);
  close(fd);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, advise_and_touch_huge_pages) {
  const uint64_t size = 4UL * 1024UL * 1024UL;
  void *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(ptr, MAP_FAILED);
  memset_s(ptr, size, 0xFF, size);
  AdviseAndTouchHugePages(ptr, size);
  const uint64_t kHugePageSize = 2UL * 1024UL * 1024UL;
  for (uint64_t offset = 0; offset < size; offset += kHugePageSize) {
    uint64_t val = *reinterpret_cast<uint64_t *>(static_cast<char *>(ptr) + offset);
    EXPECT_EQ(val, 0ULL);
  }
  munmap(ptr, size);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, advise_and_touch_partial_page) {
  const uint64_t size = 4096U;
  void *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(ptr, MAP_FAILED);
  memset_s(ptr, size, 0xFF, size);
  AdviseAndTouchHugePages(ptr, size);
  uint64_t val = *reinterpret_cast<uint64_t *>(ptr);
  EXPECT_EQ(val, 0ULL);
  munmap(ptr, size);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, free_verify_shm_unlinked) {
  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 4096U;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  EXPECT_EQ(AclrtFreeHostSharedMemory(name.c_str(), size, fd, host_ptr), ACL_SUCCESS);
  int32_t verify_fd = shm_open(name.c_str(), O_RDWR, 0666);
  if (verify_fd >= 0) {
    struct stat st;
    fstat(verify_fd, &st);
    EXPECT_EQ(st.st_size, 0);
    close(verify_fd);
  }
}

TEST_F(UtestAclrtMallocHostSharedMemTest, free_null_name_no_shm_unlink) {
  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 4096U;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  EXPECT_EQ(AclrtFreeHostSharedMemory(nullptr, size, fd, host_ptr), ACL_SUCCESS);
  int32_t verify_fd = shm_open(name.c_str(), O_RDWR, 0666);
  EXPECT_GE(verify_fd, 0);
  if (verify_fd >= 0) {
    struct stat st;
    fstat(verify_fd, &st);
    EXPECT_EQ(static_cast<uint64_t>(st.st_size), size);
    close(verify_fd);
  }
}

TEST_F(UtestAclrtMallocHostSharedMemTest, free_empty_name_no_shm_unlink) {
  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 4096U;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  EXPECT_EQ(AclrtFreeHostSharedMemory("", size, fd, host_ptr), ACL_SUCCESS);
  int32_t verify_fd = shm_open(name.c_str(), O_RDWR, 0666);
  EXPECT_GE(verify_fd, 0);
  if (verify_fd >= 0) {
    struct stat st;
    fstat(verify_fd, &st);
    EXPECT_EQ(static_cast<uint64_t>(st.st_size), size);
    close(verify_fd);
  }
}

TEST_F(UtestAclrtMallocHostSharedMemTest, free_realloc_after_free) {
  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 4096U;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  memset_s(host_ptr, size, 0xAA, size);
  EXPECT_EQ(AclrtFreeHostSharedMemory(name.c_str(), size, fd, host_ptr), ACL_SUCCESS);
  host_ptr = nullptr;
  fd = -1;
  dev_ptr = nullptr;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  uint64_t val = *reinterpret_cast<uint64_t *>(host_ptr);
  EXPECT_EQ(val, 0ULL);
  EXPECT_EQ(AclrtFreeHostSharedMemory(name.c_str(), size, fd, host_ptr), ACL_SUCCESS);
}

TEST_F(UtestAclrtMallocHostSharedMemTest, free_fd_neg1_skip_close_and_unlink) {
  std::string name = MakeShmName();
  int32_t fd = -1;
  void *host_ptr = nullptr;
  void *dev_ptr = nullptr;
  const uint64_t size = 2UL * 1024UL * 1024UL;
  EXPECT_EQ(AclrtMallocHostSharedMemory(name.c_str(), size, &fd, &host_ptr, &dev_ptr), ACL_SUCCESS);
  EXPECT_GE(fd, 0);
  EXPECT_EQ(AclrtFreeHostSharedMemory(name.c_str(), size, -1, host_ptr), ACL_SUCCESS);
  int32_t verify_fd = shm_open(name.c_str(), O_RDWR, 0666);
  EXPECT_GE(verify_fd, 0);
  if (verify_fd >= 0) {
    struct stat st;
    fstat(verify_fd, &st);
    EXPECT_EQ(static_cast<uint64_t>(st.st_size), size);
    close(verify_fd);
  }
  close(fd);
}
}  // namespace ge
