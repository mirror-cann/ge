/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "engines/local_engine/ops_kernel_store/op/ge_deleted_op.h"
#include "framework/common/debug/ge_log.h"
#include "framework/common/ge_inner_error_codes.h"
#include "engines/local_engine/ops_kernel_store/op/ge_local_op_factory.h"
#include "ge_context.h"
#include "ge_local_context.h"
#include "base/err_msg.h"

namespace ge {
namespace ge_local {
namespace {
const char_t *const kOptionEnabledValue = "true";
}  // namespace

GeDeletedOp::GeDeletedOp(const Node &node, RunContext &run_context) : Op(node, run_context) {}

Status GeDeletedOp::Run() {
  // Op 类型与负责删除该类型节点的图优化选项的映射，可以继续扩展
  static const std::map<std::string, std::string> kOpTypeToOptimizationOption = {{"Size", OO_CONSTANT_FOLDING},
                                                                                 {"Shape", OO_CONSTANT_FOLDING},
                                                                                 {"ShapeN", OO_CONSTANT_FOLDING},
                                                                                 {"Rank", OO_CONSTANT_FOLDING}};

  const auto iter = kOpTypeToOptimizationOption.find(type_);
  if (iter == kOpTypeToOptimizationOption.cend()) {
    REPORT_INNER_ERR_MSG("E19999",
                         "Node %s with type %s should have been removed during graph optimization, but it still "
                         "exists and cannot be executed by the GE local engine.",
                         name_.c_str(), type_.c_str());
    GELOGE(FAILED,
           "[Check][Node] node %s with type %s should have been removed during graph optimization, but it still "
           "exists.",
           name_.c_str(), type_.c_str());
    return FAILED;
  }

  const std::string &option_key = iter->second;
  const std::string option_name = GetContext().GetReadableName(option_key);
  std::string option_value;
  if (GetThreadLocalContext().GetOption(option_key, option_value) != GRAPH_SUCCESS) {
    REPORT_INNER_ERR_MSG("E19999",
                         "Failed to get the value of optimization option %s, which is expected to remove node %s "
                         "with type %s during graph optimization.",
                         option_name.c_str(), name_.c_str(), type_.c_str());
    GELOGE(FAILED, "[Get][Option] failed to get the value of optimization option %s for node %s with type %s.",
           option_name.c_str(), name_.c_str(), type_.c_str());
    return FAILED;
  }

  if (option_value != kOptionEnabledValue) {
    // 用户关闭了该优化选项导致冗余节点残留，属于用户可修复的外部错误
    const std::string reported_value = option_value.empty() ? std::string("\"\"") : option_value;
    const std::string reason = "node " + name_ + " with type " + type_ +
                               " is removed only when this optimization option is enabled, otherwise it cannot be "
                               "executed by the GE local engine.";
    (void)REPORT_PREDEFINED_ERR_MSG(
        "E10001", std::vector<const char_t *>({"value", "parameter", "reason"}),
        std::vector<const char_t *>({reported_value.c_str(), option_name.c_str(), reason.c_str()}));
    GELOGE(FAILED, "[Check][Option] the value %s of optimization option %s is invalid. %s", reported_value.c_str(),
           option_name.c_str(), reason.c_str());
    return FAILED;
  }

  // 优化选项已开启但节点仍然残留，说明关联的图优化 pass 未按预期生效
  REPORT_INNER_ERR_MSG("E19999",
                       "Node %s with type %s should have been removed by the enabled optimization option %s, but it "
                       "still exists and cannot be executed by the GE local engine.",
                       name_.c_str(), type_.c_str(), option_name.c_str());
  GELOGE(FAILED,
         "[Check][Node] node %s with type %s should have been removed by the enabled optimization option %s, but it "
         "still exists.",
         name_.c_str(), type_.c_str(), option_name.c_str());
  return FAILED;
}

REGISTER_OP_CREATOR(TemporaryVariable, GeDeletedOp);
REGISTER_OP_CREATOR(DestroyTemporaryVariable, GeDeletedOp);
REGISTER_OP_CREATOR(GuaranteeConst, GeDeletedOp);
REGISTER_OP_CREATOR(PreventGradient, GeDeletedOp);
REGISTER_OP_CREATOR(StopGradient, GeDeletedOp);
REGISTER_OP_CREATOR(Size, GeDeletedOp);
REGISTER_OP_CREATOR(Shape, GeDeletedOp);
REGISTER_OP_CREATOR(ShapeN, GeDeletedOp);
REGISTER_OP_CREATOR(Rank, GeDeletedOp);
REGISTER_OP_CREATOR(_Retval, GeDeletedOp);
REGISTER_OP_CREATOR(ReadVariableOp, GeDeletedOp);
REGISTER_OP_CREATOR(VarHandleOp, GeDeletedOp);
REGISTER_OP_CREATOR(VarIsInitializedOp, GeDeletedOp);
REGISTER_OP_CREATOR(Snapshot, GeDeletedOp);
REGISTER_OP_CREATOR(RefIdentity, GeDeletedOp);
REGISTER_OP_CREATOR(Identity, GeDeletedOp);
REGISTER_OP_CREATOR(IdentityN, GeDeletedOp);
REGISTER_OP_CREATOR(VariableV2, GeDeletedOp);
REGISTER_OP_CREATOR(Empty, GeDeletedOp);
REGISTER_OP_CREATOR(PlaceholderWithDefault, GeDeletedOp);
REGISTER_OP_CREATOR(IsVariableInitialized, GeDeletedOp);
REGISTER_OP_CREATOR(PlaceholderV2, GeDeletedOp);
REGISTER_OP_CREATOR(Placeholder, GeDeletedOp);
REGISTER_OP_CREATOR(End, GeDeletedOp);
REGISTER_OP_CREATOR(Switch, GeDeletedOp);
REGISTER_OP_CREATOR(RefMerge, GeDeletedOp);
REGISTER_OP_CREATOR(RefSwitch, GeDeletedOp);
REGISTER_OP_CREATOR(TransShape, GeDeletedOp);
REGISTER_OP_CREATOR(GatherShapes, GeDeletedOp);
}  // namespace ge_local
}  // namespace ge
