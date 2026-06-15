/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_HELPER_CUSTOM_OP_REGISTRY_BUILDER_H_
#define BASE_COMMON_HELPER_CUSTOM_OP_REGISTRY_BUILDER_H_

#include <vector>

#include "external/ge_common/ge_common_api_types.h"
#include "graph/custom_op_registry.h"

namespace ge {
class CustomOpRegistryBuilder {
 public:
  using DlsymFunc = void *(*)(void *handle, const char *symbol);

  static Status AddCreatorsFromSoHandles(const std::vector<CustomOpSoHandlePtr> &so_handles,
                                         const CustomOpRegistryPtr &registry);

 private:
  static Status AddCreatorsFromSoHandles(const std::vector<CustomOpSoHandlePtr> &so_handles,
                                         const CustomOpRegistryPtr &registry, const DlsymFunc dlsym_func);
};
}  // namespace ge

#endif  // BASE_COMMON_HELPER_CUSTOM_OP_REGISTRY_BUILDER_H_
