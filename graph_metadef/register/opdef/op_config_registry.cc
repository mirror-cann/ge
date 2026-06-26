/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "base/asc/opdef/op_config_registry_impl.h"
#include "framework/common/debug/ge_log.h"
#include "base/asc/opdef/op_def_impl.h"

namespace ops {

OpConfigRegistry::OpConfigRegistry() {}

void OpConfigRegistry::RegisterOpAICoreConfig(const char *name, const char *socVersion,
                                              OpAICoreConfigFunc func = nullptr) {
  OpConfigRegistryImpl::GetInstance().AddAICoreConfig(name, socVersion, func);
}

std::map<ge::AscendString, OpAICoreConfigFunc> GetOpAllAICoreConfig(const char *name) {
  return OpConfigRegistryImpl::GetInstance().GetOpAllAICoreConfig(name);
}

}  // namespace ops
