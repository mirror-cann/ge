/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/aclrt_malloc_helper.h"

#include <climits>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

#include <unordered_map>

#include "acl/acl_rt.h"
#include "runtime/rt_external_mem.h"
#include "ge_common/debug/ge_log.h"

namespace {

using ge::FAILED;

// Memory type dispatch table entry.
struct MemTypeInfo {
  aclrtMemMallocPolicy policy;
  aclError (*handler)(void **, size_t, uint16_t, aclrtMemMallocPolicy);
};

// Validate input parameters for shared memory allocation.
// Returns true if the function should return early (either with error or success).
bool ShouldEarlyExit(const char *name, uint64_t size, const int32_t *fd, void **host_ptr, void **dev_ptr,
                     aclError *err) {
  if ((name == nullptr) || (fd == nullptr) || (host_ptr == nullptr) || (dev_ptr == nullptr)) {
    GELOGE(FAILED, "[Call][AclrtMallocHostSharedMemory] invalid param, name: %p, fd: %p, host_ptr: %p, dev_ptr: %p",
           static_cast<const void *>(name), static_cast<const void *>(fd), static_cast<const void *>(host_ptr),
           static_cast<const void *>(dev_ptr));
    *err = ACL_ERROR_INVALID_PARAM;
    return true;
  }
  if (size > static_cast<uint64_t>(INT64_MAX)) {
    GELOGE(FAILED, "[Call][AclrtMallocHostSharedMemory] size %lu exceeds off_t max value", size);
    *err = ACL_ERROR_INVALID_PARAM;
    return true;
  }
  return false;
}

// Open or create shared memory file, set its size, and mmap it.
aclError PrepareShmFd(const char *name, uint64_t size, int32_t *fd, void **host_ptr) {
  *fd = shm_open(name, static_cast<uint16_t>(O_RDWR) | static_cast<uint16_t>(O_CREAT),
                 static_cast<uint16_t>(S_IRUSR) | static_cast<uint16_t>(S_IWUSR));
  if (*fd < 0) {
    GELOGE(FAILED, "[Call][shm_open] failed, name: %s, errno: %d", name, errno);
    return ACL_ERROR_FAILURE;
  }
  struct stat st;
  if (fstat(*fd, &st) != 0) {
    GELOGE(FAILED, "[Call][fstat] failed, name: %s, errno: %d", name, errno);
    return ACL_ERROR_FAILURE;
  }
  // Newly created file: set size via ftruncate.
  if (st.st_size == 0) {
    if (ftruncate(*fd, static_cast<off_t>(size)) != 0) {
      GELOGE(FAILED, "[Call][ftruncate] failed, name: %s, size: %lu, errno: %d", name, size, errno);
      return ACL_ERROR_FAILURE;
    }
  } else if (static_cast<uint64_t>(st.st_size) != size) {
    // Existing file: verify size matches.
    GELOGE(FAILED, "[Call][AclrtMallocHostSharedMemory] size mismatch, name: %s, existing: %ld, requested: %lu", name,
           st.st_size, size);
    return ACL_ERROR_FAILURE;
  } else {
    // do nothing
  }
  *host_ptr =
      mmap(nullptr, size, static_cast<uint16_t>(PROT_READ) | static_cast<uint16_t>(PROT_WRITE), MAP_SHARED, *fd, 0);
  if (*host_ptr == MAP_FAILED) {
    GELOGE(FAILED, "[Call][mmap] failed, name: %s, size: %lu, errno: %d", name, size, errno);
    return ACL_ERROR_FAILURE;
  }
  // Touch pages only for newly created memory to force immediate physical allocation.
  if (st.st_size == 0) {
    ge::AdviseAndTouchHugePages(*host_ptr, size);
  }
  return ACL_SUCCESS;
}

// Handler for standard device memory (HBM/DDR/P2P).
aclError HandleDevice(void **ptr, size_t size, uint16_t module_id, aclrtMemMallocPolicy policy) {
  aclrtMallocAttribute attr;
  attr.attr = ACL_RT_MEM_ATTR_MODULE_ID;
  attr.value.moduleId = module_id;
  aclrtMallocConfig cfg;
  cfg.attrs = &attr;
  cfg.numAttrs = 1UL;
  const aclError ret = aclrtMallocWithCfg(ptr, size, policy, &cfg);
  if (ret != ACL_SUCCESS) {
    *ptr = nullptr;
    GELOGE(FAILED, "[Call][aclrtMallocWithCfg] failed, size: %zu, policy: %d, ret: %d", size,
           static_cast<int32_t>(policy), ret);
  }
  return ret;
}

// Handler for Task Scheduler memory.
aclError HandleTs(void **ptr, size_t size, uint16_t module_id, aclrtMemMallocPolicy policy) {
  return ge::AclrtMallocForTaskScheduler(ptr, size, policy, module_id);
}

// Handler for Host memory.
aclError HandleHost(void **ptr, size_t size, uint16_t module_id, aclrtMemMallocPolicy) {
  return ge::AclrtMallocHost(ptr, size, module_id);
}

}  // namespace

namespace ge {

void AdviseAndTouchHugePages(void *ptr, uint64_t size) {
  if ((ptr == nullptr) || (size == 0U)) {
    return;
  }
  // Advise kernel to use transparent huge pages for this range.
  if (madvise(ptr, static_cast<size_t>(size), MADV_HUGEPAGE) != 0) {
    GELOGW("[madvise][MADV_HUGEPAGE] failed, size: %lu, errno: %d", size, errno);
  }
  // Touch every page's first 8 bytes to force immediate physical page allocation.
  constexpr uint64_t kPageSize = 4096U;
  for (uint64_t offset = 0; offset < size; offset += kPageSize) {
    *reinterpret_cast<uint64_t *>(static_cast<uint8_t *>(ptr) + offset) = 0ULL;
  }
}

aclError AclrtMallocHost(void **ptr, size_t size, uint16_t module_id) {
  *ptr = nullptr;
  aclrtMallocAttribute attr;
  attr.attr = ACL_RT_MEM_ATTR_MODULE_ID;
  attr.value.moduleId = module_id;
  aclrtMallocConfig cfg;
  cfg.attrs = &attr;
  cfg.numAttrs = 1UL;
  const aclError ret = aclrtMallocHostWithCfg(ptr, size, &cfg);
  if (ret != ACL_SUCCESS) {
    *ptr = nullptr;
    GELOGE(FAILED, "[Call][aclrtMallocHostWithCfg] failed, size: %zu, ret: %d", size, ret);
  }
  return ret;
}

aclError AclrtMallocForTaskScheduler(void **ptr, size_t size, aclrtMemMallocPolicy policy, uint16_t module_id) {
  *ptr = nullptr;
  aclrtMallocAttribute attr;
  attr.attr = ACL_RT_MEM_ATTR_MODULE_ID;
  attr.value.moduleId = module_id;
  aclrtMallocConfig cfg;
  cfg.attrs = &attr;
  cfg.numAttrs = 1UL;
  const aclError ret = aclrtMallocForTaskScheduler(ptr, size, policy, &cfg);
  if (ret != ACL_SUCCESS) {
    *ptr = nullptr;
    GELOGE(FAILED, "[Call][aclrtMallocForTaskScheduler] failed, size: %zu, ret: %d", size, ret);
  }
  return ret;
}

aclError AclrtMallocHostSharedMemory(const char *name, uint64_t size, int32_t *fd, void **host_ptr, void **dev_ptr) {
  aclError early_err = ACL_SUCCESS;
  if (ShouldEarlyExit(name, size, fd, host_ptr, dev_ptr, &early_err)) {
    return early_err;
  }
  const aclError ret = PrepareShmFd(name, size, fd, host_ptr);
  if (ret != ACL_SUCCESS) {
    if (*fd >= 0) {
      (void)close(*fd);
      (void)shm_unlink(name);
    }
    return ret;
  }
  const aclError register_ret = aclrtHostRegister(*host_ptr, size, ACL_HOST_REGISTER_MAPPED, dev_ptr);
  if (register_ret != ACL_SUCCESS) {
    GELOGE(FAILED, "[Call][aclrtHostRegister] failed, name: %s, size: %lu, ret: %d", name, size, register_ret);
    (void)munmap(*host_ptr, size);
    (void)close(*fd);
    if (name != nullptr) {
      (void)shm_unlink(name);
    }
    return register_ret;
  }
  return ACL_SUCCESS;
}

aclError AclrtFreeHostSharedMemory(const char *name, uint64_t size, int32_t fd, void *host_ptr) {
  // Unregister from device mapping before releasing memory.
  const aclError ret = aclrtHostUnregister(host_ptr);
  if (ret != ACL_SUCCESS) {
    GELOGE(FAILED, "[Call][aclrtHostUnregister] failed, name: %s, ret: %d", (name != nullptr) ? name : "", ret);
    return ret;
  }
  (void)munmap(host_ptr, size);
  if (fd >= 0) {
    (void)close(fd);
    if (name != nullptr) {
      (void)shm_unlink(name);
    }
  }
  return ACL_SUCCESS;
}

aclError AclrtMalloc(void **ptr, size_t size, rtMemType_t mem_type, uint16_t module_id) {
  *ptr = nullptr;

  // Map RT memory types to ACL allocation policy and handler.
  static const std::unordered_map<rtMemType_t, MemTypeInfo> kMemTypeMap = {
      // Task Scheduler memory: use dedicated API with high bandwidth policy.
      {RT_MEMORY_TS, {ACL_MEM_MALLOC_HUGE_FIRST, &HandleTs}},
      // Host memory: use host allocation API.
      {RT_MEMORY_HOST, {ACL_MEM_TYPE_HIGH_BAND_WIDTH, &HandleHost}},
      // HBM and default device memory: high bandwidth.
      {RT_MEMORY_HBM, {ACL_MEM_TYPE_HIGH_BAND_WIDTH, &HandleDevice}},
      {RT_MEMORY_DEFAULT, {ACL_MEM_TYPE_HIGH_BAND_WIDTH, &HandleDevice}},
      {RT_MEMORY_RDMA_HBM, {ACL_MEM_TYPE_HIGH_BAND_WIDTH, &HandleDevice}},
      {RT_MEMORY_SPM, {ACL_MEM_TYPE_HIGH_BAND_WIDTH, &HandleDevice}},
      // P2P HBM memory: P2P policy with huge page first.
      {RT_MEMORY_P2P_HBM, {ACL_MEM_MALLOC_HUGE_FIRST_P2P, &HandleDevice}},
      // DDR memory: low bandwidth.
      {RT_MEMORY_DDR, {ACL_MEM_TYPE_LOW_BAND_WIDTH, &HandleDevice}},
      {RT_MEMORY_DDR_NC, {ACL_MEM_TYPE_LOW_BAND_WIDTH, &HandleDevice}},
      // P2P DDR memory: P2P policy with huge page first.
      {RT_MEMORY_P2P_DDR, {ACL_MEM_MALLOC_HUGE_FIRST_P2P, &HandleDevice}},
  };

  auto it = kMemTypeMap.find(mem_type);
  if (it != kMemTypeMap.end()) {
    return it->second.handler(ptr, size, module_id, it->second.policy);
  }
  GELOGW("[Call][AclrtMalloc] unknown mem_type: %u, fallback to default", mem_type);
  return HandleDevice(ptr, size, module_id, ACL_MEM_TYPE_HIGH_BAND_WIDTH);
}

}  // namespace ge
