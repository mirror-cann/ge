/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/custom_op_pull_registry.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#ifdef __GNUC__
#ifdef CUSTOM_OP_PULL_REGISTRY_HIDDEN_EXPORT
#define CUSTOM_OP_PULL_REGISTRY_EXPORT __attribute__((visibility("hidden")))
#else
#define CUSTOM_OP_PULL_REGISTRY_EXPORT __attribute__((visibility("default")))
#endif
#else
#define CUSTOM_OP_PULL_REGISTRY_EXPORT
#endif

namespace ge {
namespace {
struct LocalCreator {
  std::string op_type;
  CustomOpCreateFunc creator;
};

std::mutex &GetCustomOpLocalCreatorMutex() {
  static std::mutex mu;
  return mu;
}

std::vector<LocalCreator> &GetCustomOpLocalCreators() {
  static std::vector<LocalCreator> creators;
  return creators;
}
}  // namespace

void RegisterCustomOpLocalCreator(const char *const op_type, const CustomOpCreateFunc creator) {
  if ((op_type == nullptr) || (op_type[0] == '\0') || (creator == nullptr)) {
    return;
  }
  const std::lock_guard<std::mutex> lock(GetCustomOpLocalCreatorMutex());
  GetCustomOpLocalCreators().push_back({op_type, creator});
}
}  // namespace ge

extern "C" CUSTOM_OP_PULL_REGISTRY_EXPORT uint32_t GetRegisteredCustomOpCreatorAbiVersion() {
  return ge::kCustomOpCreatorPullAbiVersion;
}

extern "C" CUSTOM_OP_PULL_REGISTRY_EXPORT size_t GetRegisteredCustomOpCreatorNum() {
  const std::lock_guard<std::mutex> lock(ge::GetCustomOpLocalCreatorMutex());
  return ge::GetCustomOpLocalCreators().size();
}

extern "C" CUSTOM_OP_PULL_REGISTRY_EXPORT int32_t GetRegisteredCustomOpCreators(ge::CustomOpTypeToCreator *creators,
                                                                                const size_t creator_num,
                                                                                const size_t creator_struct_size) {
  const std::lock_guard<std::mutex> lock(ge::GetCustomOpLocalCreatorMutex());
  const auto &local_creators = ge::GetCustomOpLocalCreators();
  if ((creator_num < local_creators.size()) || ((creator_num > 0U) && (creators == nullptr)) ||
      (creator_struct_size < sizeof(ge::CustomOpTypeToCreator))) {
    return -1;
  }
  for (size_t i = 0U; i < local_creators.size(); ++i) {
    auto *creator_addr = reinterpret_cast<uint8_t *>(creators) + (i * creator_struct_size);
    auto *creator = reinterpret_cast<ge::CustomOpTypeToCreator *>(creator_addr);
    *creator = {sizeof(ge::CustomOpTypeToCreator), local_creators[i].op_type.c_str(), local_creators[i].creator};
  }
  return 0;
}

#undef CUSTOM_OP_PULL_REGISTRY_EXPORT
