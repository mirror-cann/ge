/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "stub/runtime_stub_for_kernel_v2.h"

namespace gert {
aclError RuntimeStubForKernelV2::aclrtBinaryLoadFromFile(const char *binPath, aclrtBinaryLoadOptions *options,
                                                         aclrtBinHandle *binHandle) {
  *binHandle = reinterpret_cast<void *>(static_cast<uintptr_t>(stub_bin_addr_));
  bin_handle_store_.emplace(*binHandle);
  stub_bin_addr_++;
  return ACL_SUCCESS;
}

aclError RuntimeStubForKernelV2::aclrtBinaryLoadFromData(const void *data, size_t length,
                                                         const aclrtBinaryLoadOptions *options,
                                                         aclrtBinHandle *binHandle) {
  *binHandle = reinterpret_cast<void *>(static_cast<uintptr_t>(stub_bin_addr_));
  bin_handle_store_.emplace(*binHandle);
  stub_bin_addr_++;
  return ACL_SUCCESS;
}

aclError RuntimeStubForKernelV2::aclrtRegisterCpuFunc(const aclrtBinHandle handle, const char *funcName,
                                                      const char *kernelName, aclrtFuncHandle *funcHandle) {
  if (bin_handle_store_.find(handle) != bin_handle_store_.end()) {
    *funcHandle = reinterpret_cast<void *>(static_cast<uintptr_t>(stub_func_addr_));
    stub_func_addr_++;
    return ACL_SUCCESS;
  }
  return -1;
}

aclError RuntimeStubForKernelV2::aclrtBinaryGetFunction(const aclrtBinHandle binHandle, const char *kernelName,
                                                        aclrtFuncHandle *funcHandle) {
  if (bin_handle_store_.find(binHandle) != bin_handle_store_.end()) {
    *funcHandle = reinterpret_cast<void *>(static_cast<uintptr_t>(stub_func_addr_));
    stub_func_addr_++;
    return ACL_SUCCESS;
  }
  return -1;
}
}  // namespace gert
