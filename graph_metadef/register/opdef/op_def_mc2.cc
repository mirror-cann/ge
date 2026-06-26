/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <algorithm>
#include "base/asc/opdef/op_def_impl.h"

namespace ops {
OpMC2Def::OpMC2Def() : impl_(new (std::nothrow) OpMC2DefImpl) {}

OpMC2Def::OpMC2Def(const OpMC2Def &mc2_def) : impl_(new (std::nothrow) OpMC2DefImpl) {
  this->impl_->Construct(this, mc2_def);
}

OpMC2Def::~OpMC2Def() = default;

OpMC2Def &OpMC2Def::operator=(const OpMC2Def &mc2_def) {
  return this->impl_->Eq(this, mc2_def);
}

OpMC2Def &OpMC2Def::HcclGroup(const char *value) {
  return this->impl_->HcclGroup(this, value);
}

OpMC2Def &OpMC2Def::HcclGroup(std::vector<const char *> value) {
  return this->impl_->HcclGroup(this, value);
}

std::vector<ge::AscendString> &OpMC2Def::GetHcclGroups(void) const {
  return this->impl_->group_list;
}

void OpMC2Def::HcclServerType(enum HcclServerType type, const char *soc) {
  this->impl_->HcclServerTypeImpl(this, type, soc);
}

/**
 * @brief get hccl server type by soc version
 * @param soc_version "" means checking if any hccl server type has been set
 * @return hccl server type corresponding to soc version.
           For scenarios where soc version is empty, return MAX if not set, AICPU if set.
 */
enum HcclServerType OpMC2Def::GetHcclServerType(const ge::AscendString &soc_version) const {
  return this->impl_->GetHcclServerType(this, soc_version);
}

}  // namespace ops
