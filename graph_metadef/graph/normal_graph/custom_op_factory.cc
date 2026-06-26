/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/custom_op_factory.h"
#include "graph/custom_op.h"
#include "graph/custom_op_load_context.h"
#include "graph/custom_op_registry.h"
#include "debug/ge_log.h"

#include <cstdint>
#include <memory>

namespace ge {

CustomOpRegistry &CustomOpFactory::GetGlobalRegistry() {
  return *GetGlobalRegistryPtr();
}

CustomOpRegistryPtr CustomOpFactory::GetGlobalRegistryPtr() {
  static CustomOpRegistryPtr registry = std::make_shared<CustomOpRegistry>();
  return registry;
}

graphStatus CustomOpFactory::RegisterCustomOpCreator(const AscendString &op_type, const BaseOpCreator &op_creator) {
  return GetGlobalRegistry().RegisterCreator(op_type, op_creator);
}

BaseCustomOp *CustomOpFactory::CreateOrGetCustomOp(const AscendString &op_type) {
  return GetGlobalRegistry().CreateOrGetCustomOp(op_type);
}

graphStatus CustomOpFactory::GetAllRegisteredOps(std::vector<AscendString> &all_registered_ops) {
  return GetGlobalRegistry().GetAllRegisteredOps(all_registered_ops);
}
bool CustomOpFactory::IsExistOp(const AscendString &op_type) {
  return GetGlobalRegistry().HasCreator(op_type);
}

bool CustomOpFactory::IsAddressRefreshable(const AscendString &op_type) {
  return GetGlobalRegistry().IsAddressRefreshable(op_type);
}

graphStatus CustomOpFactory::LoadCustomOpsPartition(const uint8_t *data, size_t len) {
  return GetGlobalRegistry().LoadCustomOpsPartition(data, len);
}

CustomOpCreatorRegister::CustomOpCreatorRegister(const AscendString &operator_type, BaseOpCreator const &op_creator) {
  if (IsOfflineCustomOpSoLoading()) {
    return;
  }
  CustomOpFactory::RegisterCustomOpCreator(operator_type, op_creator);
}
}  // namespace ge
