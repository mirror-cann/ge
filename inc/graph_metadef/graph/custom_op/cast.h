/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef METADEF_CXX_INC_GRAPH_CUSTOM_OP_CAST_H_
#define METADEF_CXX_INC_GRAPH_CUSTOM_OP_CAST_H_

#include "graph/custom_op.h"
#include "graph/custom_op/capability.h"

namespace ge {
class CustomOpCapabilityProvider {
 public:
  virtual ~CustomOpCapabilityProvider() = default;
  virtual bool HasCapability(CustomOpCapability capability) const = 0;
};

template <typename T>
struct CustomOpCapabilityTrait;

template <>
struct CustomOpCapabilityTrait<EagerExecuteOp> {
  static constexpr CustomOpCapability kCapability = CustomOpCapability::kEagerExecute;
};

template <>
struct CustomOpCapabilityTrait<CompilableOp> {
  static constexpr CustomOpCapability kCapability = CustomOpCapability::kCompilable;
};

template <>
struct CustomOpCapabilityTrait<ShapeInferOp> {
  static constexpr CustomOpCapability kCapability = CustomOpCapability::kShapeInfer;
};

template <>
struct CustomOpCapabilityTrait<PortableOp> {
  static constexpr CustomOpCapability kCapability = CustomOpCapability::kPortable;
};

template <>
struct CustomOpCapabilityTrait<ArgsUpdater> {
  static constexpr CustomOpCapability kCapability = CustomOpCapability::kArgsUpdater;
};

template <typename T>
T *CustomOpCast(BaseCustomOp *op) {
  if (op == nullptr) {
    return nullptr;
  }
  auto *provider = dynamic_cast<CustomOpCapabilityProvider *>(op);
  if ((provider != nullptr) && (!provider->HasCapability(CustomOpCapabilityTrait<T>::kCapability))) {
    return nullptr;
  }
  return dynamic_cast<T *>(op);
}

template <typename T>
const T *CustomOpCast(const BaseCustomOp *op) {
  if (op == nullptr) {
    return nullptr;
  }
  auto *provider = dynamic_cast<const CustomOpCapabilityProvider *>(op);
  if ((provider != nullptr) && (!provider->HasCapability(CustomOpCapabilityTrait<T>::kCapability))) {
    return nullptr;
  }
  return dynamic_cast<const T *>(op);
}
}  // namespace ge

#endif  // METADEF_CXX_INC_GRAPH_CUSTOM_OP_CAST_H_
