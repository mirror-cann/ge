/**
* Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_ACLRT_MALLOC_HELPER_H
#define GE_ACLRT_MALLOC_HELPER_H

#include <cstdint>

#include "acl/acl_rt.h"

namespace ge {

using rtMemType_t = uint32_t;

void AdviseAndTouchHugePages(void *ptr, uint64_t size);

aclError AclrtMallocHost(void **ptr, size_t size, uint16_t module_id);

aclError AclrtMallocForTaskScheduler(void **ptr, size_t size, aclrtMemMallocPolicy policy, uint16_t module_id);

aclError AclrtMallocHostSharedMemory(const char *name, uint64_t size, int32_t *fd, void **host_ptr,
                                      void **dev_ptr);

aclError AclrtFreeHostSharedMemory(const char *name, uint64_t size, int32_t fd, void *host_ptr);

aclError AclrtMalloc(void **ptr, size_t size, rtMemType_t mem_type, uint16_t module_id);

}  // namespace ge

#endif  // GE_ACLRT_MALLOC_HELPER_H