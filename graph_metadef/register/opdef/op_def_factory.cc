/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include <algorithm>
#include <map>
#include <set>
#include "register/op_def.h"
#include "base/asc/opdef/op_def_impl.h"
#include "register/op_def_factory.h"
#include "base/asc/opdef/op_def_factory_impl.h"

namespace ops {

int OpDefFactory::OpDefRegister(const char *name, OpDefCreator creator) {
  return OpDefFactoryImpl::GetInstance().OpDefRegister(name, creator);
}

int __attribute__((weak)) OpDefFactory::OpDefRegisterV2(const char *name, OpDefFuncPtr ptr) {
  return OpDefFactoryImpl::GetInstance().OpDefRegisterV2(name, ptr);
}

OpDef OpDefFactory::OpDefCreate(const char *name) {
  return OpDefFactoryImpl::GetInstance().OpDefCreate(name);
}

std::vector<ge::AscendString> &OpDefFactory::GetAllOp(void) {
  return OpDefFactoryImpl::GetInstance().GetAllOp();
}

void OpDefFactory::OpTilingSinkRegister(const char *opType) {
  OpDefFactoryImpl::GetInstance().OpTilingSinkRegister(opType);
}

bool OpDefFactory::OpIsTilingSink(const char *opType) {
  return OpDefFactoryImpl::GetInstance().OpIsTilingSink(opType);
}
}  // namespace ops
