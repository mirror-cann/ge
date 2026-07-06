/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "reduce_fusion_strategy.h"
#include "can_fuse/backend/backend_utils.h"
#include "fusion/autofuse_attrs.h"
#include "can_fuse/strategy/fusion_strategy_registry.h"
#include "utils/not_fuse_reason_code.h"
#include "utils/auto_fuse_config.h"
#include "utils/autofuse_utils.h"
#include "can_fuse/backend/asc_graph_axis_mapping.h"

namespace ge {
// 检查并初始化节点的is_reduce_all_load状态
void CheckAndInitReduceAllLoadState(const NodePtr &node, AutoFuseAttrs *attr, const std::string &node_desc) {
  (void)node_desc;
  // 只在状态为未初始化且节点为Reduce类型时才检查
  if (attr->HasFuseType(loop::FuseType::kReduction) &&
      attr->GetReduceAllLoadState() == REDUCE_ALL_LOAD_INIT) {
    int32_t state;
    // 调用BackendUtils::IfNormLikeReduce检查是否为norm-like reduce
    if (BackendUtils::IfNormLikeReduce(node)) {
      // 所有load都满足norm-like条件
      state = REDUCE_ALL_LOAD_ALL;
    } else {
      // 不是所有load都满足norm-like条件
      state = REDUCE_ALL_LOAD_NOT_ALL;
    }
    attr->SetReduceAllLoadState(state);
  }
}

bool HasReduceOriginalInfo(const AutoFuseAttrs *attr) {
  return (!attr->GetReduceOriginalAxis().empty()) || (!attr->GetReduceOriginalRepeats().empty());
}

bool IsReduceOriginalInfoCompatible(const NodePtr &node1, const AutoFuseAttrs *attr1, const NodePtr &node2,
                                    const AutoFuseAttrs *attr2) {
  if (!HasReduceOriginalInfo(attr1) || !HasReduceOriginalInfo(attr2)) {
    return true;
  }

  const auto &axis1 = attr1->GetReduceOriginalAxis();
  const auto &axis2 = attr2->GetReduceOriginalAxis();
  const auto &repeats1 = attr1->GetReduceOriginalRepeats();
  const auto &repeats2 = attr2->GetReduceOriginalRepeats();
  if ((axis1.size() != repeats1.size()) || (axis2.size() != repeats2.size()) || (axis1 != axis2) ||
      (repeats1 != repeats2)) {
    GELOGI(
        "node1 %s(%s) and node2 %s(%s) cannot fuse, reduce original axis or repeats conflict. node1 axis:%s, "
        "repeats:%s; node2 axis:%s, repeats:%s.",
        node1->GetNamePtr(), node1->GetType().c_str(), node2->GetNamePtr(), node2->GetType().c_str(),
        AutofuseUtils::VectorToStr(axis1).c_str(), AutofuseUtils::VectorToStr(repeats1).c_str(),
        AutofuseUtils::VectorToStr(axis2).c_str(), AutofuseUtils::VectorToStr(repeats2).c_str());
    return false;
  }
  return true;
}

bool ReduceFusionStrategy::CanFuse(const NodePtr &node1, const NodePtr &node2) {
  const auto attr1 = BackendUtils::GetNodeAutoFuseAttr(node1);
  GE_ASSERT_NOTNULL(attr1);
  const auto attr2 = BackendUtils::GetNodeAutoFuseAttr(node2);
  GE_ASSERT_NOTNULL(attr2);

  // 构建节点描述信息用于日志
  std::string node1_desc = std::string("node1 ") + node1->GetNamePtr() + "(" + node1->GetType().c_str() + ")";
  std::string node2_desc = std::string("node2 ") + node2->GetNamePtr() + "(" + node2->GetType().c_str() + ")";

  if (!IsReduceOriginalInfoCompatible(node1, attr1, node2, attr2)) {
    return false;
  }

  // 检查并初始化node1的is_reduce_all_load状态
  CheckAndInitReduceAllLoadState(node1, attr1, node1_desc);

  // 检查并初始化node2的is_reduce_all_load状态（注意：这里检查的是node2，不是node1）
  CheckAndInitReduceAllLoadState(node2, attr2, node2_desc);

  // 检查两个节点的norm-like reduce状态是否允许融合
  if (attr1->GetReduceAllLoadState() != REDUCE_ALL_LOAD_NOT_ALL && attr2->GetReduceAllLoadState() != REDUCE_ALL_LOAD_NOT_ALL) {
    return true;
  }

  // reduce不支持水平融合
  if (BackendUtils::IsHorizontal(node1, node2)) {
    GELOGI(
        "node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s][node1 and node2 have horizontal link, and one "
        "node is Reduce]", node1->GetNamePtr(), node1->GetType().c_str(), node2->GetNamePtr(),
        node2->GetType().c_str(), ge::NotFuseReasonCode(ge::NotFuseReason::kReduceCanNotFuseHorizontal));
    return false;
  }

  if (attr1->HasFuseType(loop::FuseType::kReduction)) {
    // 不支持reduce后融合 非 elementwise
    if (!BackendUtils::IsOnlyPointwise(node2)) {
      GELOGI(
          "node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s][Reduce can only backward fuse with "
          "elementwise nodes]",
          node1->GetNamePtr(), node1->GetType().c_str(), node2->GetNamePtr(), node2->GetType().c_str(),
          ge::NotFuseReasonCode(ge::NotFuseReason::kReduceCanOnlyBackwardFuse3Elementwise));
      return false;
    }
    // reduce最多只能向后融合3个elementwise（非AscBackend数量，而是AscBackend内部的elementwise数量）
    const auto config = autofuse::AutoFuseConfig::Config().GetFusionStrategySolver();
    if (attr1->GetReduceFusedElementwiseNodeNum() + BackendUtils::GetComputeNodeNumInAscgraph(node2) >
        config.max_reduce_can_fuse_elementwise_nums) { // reduce已后融合的elementwise数量+将要融合的elementwise数量不大于3
      GELOGI(
          "node1 %s(%s) and node2 %s(%s) cannot fuse, the reason is [%s][Reduce can only backward fuse with at most"
          " 3 elementwise nodes]", node1->GetNamePtr(), node1->GetType().c_str(), node2->GetNamePtr(),
          node2->GetType().c_str(), ge::NotFuseReasonCode(ge::NotFuseReason::kReduceCanOnlyBackwardFuse3Elementwise));
      return false;
    }
  }
  return true;
}
REGISTER_FUSION_STRATEGY(ReduceFusionStrategy, loop::FuseType::kReduction);
}