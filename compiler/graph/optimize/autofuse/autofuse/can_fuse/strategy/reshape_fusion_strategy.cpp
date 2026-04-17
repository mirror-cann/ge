/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "reshape_fusion_strategy.h"
#include "can_fuse/backend/backend_utils.h"
#include "fusion/autofuse_attrs.h"
#include "can_fuse/strategy/fusion_strategy_registry.h"

namespace ge {
bool ReshapeFusionStrategy::CanFuse(const NodePtr &node1, const NodePtr &node2) {
  const auto attr1 = BackendUtils::GetNodeAutoFuseAttr(node1);
  GE_ASSERT_NOTNULL(attr1);
  const auto attr2 = BackendUtils::GetNodeAutoFuseAttr(node2);
  GE_ASSERT_NOTNULL(attr2);

  uint64_t reshape_type = (1UL << static_cast<uint64_t>(loop::FuseType::kReshape));

  if (attr1->GetAllFuseType() == reshape_type && attr2->GetAllFuseType() == reshape_type) {
    GELOGI(
        "node1 %s(%s) and node2 %s(%s) can not fuse, the reason is [Both nodes only have reshape fuse type]",
        node1->GetNamePtr(), node1->GetType().c_str(), node2->GetNamePtr(), node2->GetType().c_str());
    return false;
  }

  return true;
}
REGISTER_FUSION_STRATEGY(ReshapeFusionStrategy, loop::FuseType::kReshape);
}
