/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_CUSTOM_OP_PULL_REGISTRY_H
#define CANN_GRAPH_ENGINE_CUSTOM_OP_PULL_REGISTRY_H

#include <cstddef>
#include <cstdint>

namespace ge {
class BaseCustomOp;
using CustomOpCreateFunc = BaseCustomOp *(*)();

constexpr uint32_t kCustomOpCreatorPullAbiVersion = 1U;

struct CustomOpTypeToCreator {
  uint32_t struct_size;
  const char *op_type;
  CustomOpCreateFunc creator;
};

void RegisterCustomOpLocalCreator(const char *const op_type, const CustomOpCreateFunc creator);
}  // namespace ge

extern "C" uint32_t GetRegisteredCustomOpCreatorAbiVersion();

extern "C" size_t GetRegisteredCustomOpCreatorNum();

extern "C" int32_t GetRegisteredCustomOpCreators(
    ge::CustomOpTypeToCreator *creators, const size_t creator_num, const size_t creator_struct_size);

#endif  // CANN_GRAPH_ENGINE_CUSTOM_OP_PULL_REGISTRY_H
