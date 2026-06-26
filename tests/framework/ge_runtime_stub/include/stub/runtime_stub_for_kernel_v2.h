/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_TESTS_FRAMEWORK_GE_RUNTIME_STUB_SRC_RUNTIME_STUB_FOR_KERNEL_V2_H_
#define AIR_CXX_TESTS_FRAMEWORK_GE_RUNTIME_STUB_SRC_RUNTIME_STUB_FOR_KERNEL_V2_H_

#include <set>
#include "depends/ascendcl/src/ascendcl_stub.h"

namespace gert {
class RuntimeStubForKernelV2 : public ge::AclRuntimeStub {
 public:
  aclError aclrtBinaryLoadFromFile(const char *binPath, aclrtBinaryLoadOptions *options,
                                   aclrtBinHandle *binHandle) override;

  aclError aclrtBinaryLoadFromData(const void *data, size_t length, const aclrtBinaryLoadOptions *options,
                                   aclrtBinHandle *binHandle) override;

  aclError aclrtRegisterCpuFunc(const aclrtBinHandle handle, const char *funcName, const char *kernelName,
                                aclrtFuncHandle *funcHandle) override;

  aclError aclrtBinaryGetFunction(const aclrtBinHandle binHandle, const char *kernelName,
                                  aclrtFuncHandle *funcHandle) override;

 private:
  int64_t ffts_addr_{0xfff3243};
  uint64_t stub_bin_addr_{0x1200};
  uint64_t stub_func_addr_{0x1600};
  std::set<aclrtBinHandle> bin_handle_store_;
};
}  // namespace gert
#endif  // AIR_CXX_TESTS_FRAMEWORK_GE_RUNTIME_STUB_SRC_RUNTIME_STUB_FOR_KERNEL_V2_H_
