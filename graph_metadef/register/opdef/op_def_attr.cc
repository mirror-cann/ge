/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <vector>
#include <string>
#include <sstream>
#include "register/op_def.h"
#include "base/asc/opdef/op_def_impl.h"
#include "framework/common/debug/ge_log.h"

namespace ops {
OpAttrDef::OpAttrDef(const char *name) : impl_(new(std::nothrow) OpAttrDefImpl) {
  OpAttrDefImpl::Construct(this, name);
}

OpAttrDef::OpAttrDef(const OpAttrDef &attr_def) : impl_(new(std::nothrow) OpAttrDefImpl) {
  OpAttrDefImpl::Construct(this, attr_def);
}

OpAttrDef::~OpAttrDef() = default;

OpAttrDef &OpAttrDef::operator=(const OpAttrDef &attr_def) {
  return this->impl_->Eq(this, attr_def);
}

bool OpAttrDef::operator==(const OpAttrDef &attr_def) const {
  return this->impl_->DoubleEq(this, attr_def);
}

OpAttrDef &OpAttrDef::AttrType(Option attr_type) {
  return this->impl_->AttrType(this, attr_type);
}

OpAttrDef &OpAttrDef::Bool(void) {
  return this->impl_->Bool(this);
}

OpAttrDef &OpAttrDef::Bool(bool value) {
  return this->impl_->Bool(this, value);
}

OpAttrDef &OpAttrDef::Float(void) {
  return this->impl_->Float(this);
}

OpAttrDef &OpAttrDef::Float(float value) {
  return this->impl_->Float(this, value);
}

OpAttrDef &OpAttrDef::Int(void) {
  return this->impl_->Int(this);
}

OpAttrDef &OpAttrDef::Int(int64_t value) {
  return this->impl_->Int(this, value);
}

OpAttrDef &OpAttrDef::String(void) {
  return this->impl_->String(this);
}

OpAttrDef &OpAttrDef::String(const char *value) {
  return this->impl_->String(this, value);
}

OpAttrDef &OpAttrDef::ListBool(void) {
  return this->impl_->ListBool(this);
}

OpAttrDef &OpAttrDef::ListBool(std::vector<bool> value) {
  return this->impl_->ListBool(this, value);
}

OpAttrDef &OpAttrDef::ListFloat(void) {
  return this->impl_->ListFloat(this);
}

OpAttrDef &OpAttrDef::ListFloat(std::vector<float> value) {
  return this->impl_->ListFloat(this, value);
}

OpAttrDef &OpAttrDef::ListInt(void) {
  return this->impl_->ListInt(this);
}

OpAttrDef &OpAttrDef::ListInt(std::vector<int64_t> value) {
  return this->impl_->ListInt(this, value);
}

OpAttrDef &OpAttrDef::ListListInt(void) {
  return this->impl_->ListListInt(this);
}

OpAttrDef &OpAttrDef::ListListInt(std::vector<std::vector<int64_t>> value) {
  return this->impl_->ListListInt(this, value);
}

OpAttrDef &OpAttrDef::Version(uint32_t version) {
  return this->impl_->Version(this, version);
}

OpAttrDef &OpAttrDef::Comment(const char *comment) {
  return this->impl_->Comment(this, comment);
}
 
ge::AscendString &OpAttrDef::GetComment(void) const {
  return this->impl_->comment;
}

uint32_t OpAttrDef::GetVersion(void) {
  return this->impl_->version;
}

ge::AscendString &OpAttrDef::GetName(void) const {
  return this->impl_->name;
}

bool OpAttrDef::IsRequired(void) {
  return this->impl_->required;
}

ge::AscendString &OpAttrDef::GetCfgDataType(void) const {
  return this->impl_->GetCfgDataType(this);
}

ge::AscendString &OpAttrDef::GetProtoDataType(void) const {
  return this->impl_->GetProtoDataType(this);
}

ge::AscendString &OpAttrDef::GetAttrDefaultVal(const char *brac) {
  return this->impl_->GetAttrDefaultVal(this, brac);
}
}  // namespace ops
