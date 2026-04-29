/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "register/op_check_register.h"
#include "framework/common/debug/ge_log.h"
#include "base/asc/opcheck/op_check_register_impl.h"

namespace optiling {

void OpCheckFuncRegistry::RegisterOpCapability(const ge::AscendString &check_type, const ge::AscendString &op_type,
                                               OP_CHECK_FUNC func) {
  OpCheckFuncRegistryImpl::GetInstance().RegisterOpCapability(check_type, op_type, func);
}

OP_CHECK_FUNC OpCheckFuncRegistry::GetOpCapability(const ge::AscendString &check_type,
                                                   const ge::AscendString &op_type) {
  return OpCheckFuncRegistryImpl::GetInstance().GetOpCapability(check_type, op_type);
}

void OpCheckFuncRegistry::RegisterGenSimplifiedKeyFunc(const ge::AscendString &op_type, GEN_SIMPLIFIEDKEY_FUNC func) {
  OpCheckFuncRegistryImpl::GetInstance().RegisterGenSimplifiedKeyFunc(op_type, func);
}

GEN_SIMPLIFIEDKEY_FUNC OpCheckFuncRegistry::GetGenSimplifiedKeyFun(const ge::AscendString &op_type) {
  return OpCheckFuncRegistryImpl::GetInstance().GetGenSimplifiedKeyFun(op_type);
}

PARAM_GENERALIZE_FUNC OpCheckFuncRegistry::GetParamGeneralize(const ge::AscendString &op_type) {
  return OpCheckFuncRegistryImpl::GetInstance().GetParamGeneralize(op_type);
}

void OpCheckFuncRegistry::RegisterParamGeneralize(const ge::AscendString &op_type, PARAM_GENERALIZE_FUNC func) {
  OpCheckFuncRegistryImpl::GetInstance().RegisterParamGeneralize(op_type, func);
}

void OpCheckFuncRegistry::RegisterReplay(const ge::AscendString &op_type, const ge::AscendString &soc_version,
                                         REPLAY_FUNC func) {
  OpCheckFuncRegistryImpl::GetInstance().RegisterReplay(op_type, soc_version, func);
}

REPLAY_FUNC OpCheckFuncRegistry::GetReplay(const ge::AscendString &op_type, const ge::AscendString &soc_version) {
  return OpCheckFuncRegistryImpl::GetInstance().GetReplay(op_type, soc_version);
}

OpCheckFuncHelper::OpCheckFuncHelper(const ge::AscendString &check_type, const ge::AscendString &op_type,
                                     OP_CHECK_FUNC func) {
  OpCheckFuncRegistry::RegisterOpCapability(check_type, op_type, func);
}

OpCheckFuncHelper::OpCheckFuncHelper(const ge::AscendString &op_type, PARAM_GENERALIZE_FUNC func) {
  OpCheckFuncRegistry::RegisterParamGeneralize(op_type, func);
}

ReplayFuncHelper::ReplayFuncHelper(const ge::AscendString &op_type, const ge::AscendString &soc_version,
                                   REPLAY_FUNC func) {
  OpCheckFuncRegistry::RegisterReplay(op_type, soc_version, func);
}
}  // end of namespace optiling
