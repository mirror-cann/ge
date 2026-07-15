/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_CUSTOM_OP_REGISTRY_CORE_H
#define CANN_GRAPH_ENGINE_CUSTOM_OP_REGISTRY_CORE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "graph/ascend_string.h"
#include "graph/ge_error_codes.h"

namespace ge {
class BaseCustomOp;
using BaseOpCreator = std::function<std::unique_ptr<BaseCustomOp>()>;

class CustomOpSoHandle;
using CustomOpSoHandlePtr = std::shared_ptr<CustomOpSoHandle>;

class CustomOpRegistry {
 public:
  ~CustomOpRegistry();

  graphStatus RegisterCreator(const AscendString &op_type, const BaseOpCreator &creator);
  void AddSoHandles(const std::vector<CustomOpSoHandlePtr> &so_handles);

  BaseCustomOp *CreateOrGetCustomOp(const AscendString &op_type);
  void RemoveCustomOps(const std::vector<AscendString> &op_types);
  bool IsAddressRefreshable(const AscendString &op_type);
  BaseCustomOp *FindCustomOp(const AscendString &op_type) const;
  bool HasCreator(const AscendString &op_type) const;
  bool HasCustomOp(const AscendString &op_type) const;
  graphStatus GetAllRegisteredOps(std::vector<AscendString> &all_registered_ops) const;
  graphStatus LoadCustomOpsPartition(const uint8_t *data, size_t len);

 private:
  mutable std::mutex mu_;
  std::vector<CustomOpSoHandlePtr> so_handles_;
  std::map<AscendString, BaseOpCreator> creators_;
  std::map<AscendString, std::shared_ptr<BaseCustomOp>> custom_ops_;
};

using CustomOpRegistryPtr = std::shared_ptr<CustomOpRegistry>;
}  // namespace ge

#endif  // CANN_GRAPH_ENGINE_CUSTOM_OP_REGISTRY_CORE_H
