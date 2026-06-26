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
#include "register/op_def.h"
#include "base/asc/opdef/op_def_impl.h"
#include "framework/common/debug/ge_log.h"

namespace ops {
OpParamDef::OpParamDef(const char *name) : impl_(new (std::nothrow) OpParamDefImpl) {
  OpParamDefImpl::Construct(this, name);
}

OpParamDef::OpParamDef(const OpParamDef &def) : impl_(new (std::nothrow) OpParamDefImpl) {
  OpParamDefImpl::Construct(this, def);
}

OpParamDef &OpParamDef::operator=(const OpParamDef &def) {
  return this->impl_->Eq(this, def);
}

void OpParamDef::MergeParam(const OpParamDef &def) {
  this->impl_->MergeParam(this, def);
}

OpParamDef::~OpParamDef() = default;

bool OpParamDef::operator==(const OpParamDef &def) const {
  return this->impl_->DoubleEq(this, def);
}

OpParamDef &OpParamDef::ParamType(Option param_type) {
  return this->impl_->ParamType(this, param_type);
}

bool OpParamDef::IsDtype(void) const {
  return this->impl_->types_status == NON_LIST;
}

bool OpParamDef::IsDtypeList(void) const {
  return this->impl_->types_status == LIST;
}

bool OpParamDef::IsFormat(void) const {
  return this->impl_->formats_status == NON_LIST;
}

bool OpParamDef::IsFormatList(void) const {
  return this->impl_->formats_status == LIST;
}

bool OpParamDef::IsScalarOrScalarList(void) const {
  return this->impl_->IsScalarOrScalarList(this);
}

bool OpParamDef::IsScalarTypeSet(void) const {
  return this->impl_->scalar_type != ge::DT_UNDEFINED;
}

bool OpParamDef::IsScalarNameSet(void) const {
  return std::strcmp(this->impl_->scalar_name.GetString(), "") != 0;
}

bool OpParamDef::IsValueDepend(void) const {
  return std::strcmp(this->impl_->value_depend.GetString(), "") != 0;
}

OpParamDef &OpParamDef::DataType(std::vector<ge::DataType> types) {
  return this->impl_->DataType(this, types);
}

OpParamDef &OpParamDef::DataTypeList(std::vector<ge::DataType> types) {
  return this->impl_->DataTypeList(this, types);
}

OpParamDef &OpParamDef::Format(std::vector<ge::Format> formats) {
  return this->impl_->Format(this, formats);
}

OpParamDef &OpParamDef::FormatList(std::vector<ge::Format> formats) {
  return this->impl_->FormatList(this, formats);
}

OpParamDef &OpParamDef::DataTypeForBinQuery(std::vector<ge::DataType> types) {
  return this->impl_->DataTypeForBinQuery(this, types);
}

OpParamDef &OpParamDef::FormatForBinQuery(std::vector<ge::Format> formats) {
  return this->impl_->FormatForBinQuery(this, formats);
}

OpParamDef &OpParamDef::UnknownShapeFormat(std::vector<ge::Format> formats) {
  return this->impl_->UnknownShapeFormat(this, formats);
}

OpParamDef &OpParamDef::ValueDepend(Option value_depend) {
  return this->impl_->ValueDepend(this, value_depend);
}

OpParamDef &OpParamDef::ValueDepend(Option value_depend, DependScope scope) {
  return this->impl_->ValueDepend(this, value_depend, scope);
}

OpParamDef &OpParamDef::IgnoreContiguous(void) {
  return this->impl_->IgnoreContiguous(this);
}

OpParamDef &OpParamDef::AutoContiguous() {
  return this->impl_->AutoContiguous(this);
}

OpParamDef &OpParamDef::Scalar() {
  return this->impl_->Scalar(this);
}

OpParamDef &OpParamDef::ScalarList() {
  return this->impl_->ScalarList(this);
}

OpParamDef &OpParamDef::To(const ge::DataType type) {
  return this->impl_->To(this, type);
}

OpParamDef &OpParamDef::To(const char *name) {
  return this->impl_->To(this, name);
}

OpParamDef &OpParamDef::Version(uint32_t version) {
  return this->impl_->Version(this, version);
}

OpParamDef &OpParamDef::InitValue(uint64_t value) {
  return this->impl_->InitValue(this, value);
}

OpParamDef &OpParamDef::InitValue(const ScalarVar &value) {
  return this->impl_->InitValue(this, value);
}

OpParamDef &OpParamDef::InitValue(const std::vector<ScalarVar> &value) {
  return this->impl_->InitValue(this, value);
}

OpParamDef &OpParamDef::OutputShapeDependOnCompute() {
  return this->impl_->OutputShapeDependOnCompute(this);
}

OpParamDef &OpParamDef::Follow(const char *paramName) {
  return this->impl_->Follow(this, paramName);
}

OpParamDef &OpParamDef::Follow(const char *paramName, FollowType ftype) {
  return this->impl_->Follow(this, paramName, ftype);
}

OpParamDef &OpParamDef::Comment(const char *comment) {
  return this->impl_->Comment(this, comment);
}

bool OpParamDef::IsOutputShapeDependOnCompute(void) const {
  return this->impl_->is_output_shape_depend_on_compute;
}

ge::AscendString &OpParamDef::GetParamName(void) const {
  return this->impl_->name;
}
Option OpParamDef::GetParamType(void) {
  return this->impl_->param_type;
}
std::vector<ge::DataType> &OpParamDef::GetDataTypes(void) {
  return this->impl_->GetDataTypes(this);
}

std::vector<ge::DataType> &OpParamDef::GetOriginDataTypes(void) {
  return this->impl_->GetOriginDataTypes(this);
}

std::vector<ge::DataType> &OpParamDef::GetDataTypesList(void) {
  return this->impl_->types_list;
}
std::vector<ge::DataType> &OpParamDef::GetDataTypesForBin(void) const {
  return this->impl_->types_for_bin;
}
bool OpParamDef::IsSetDtypeForBin(void) const {
  return this->impl_->set_type_for_bin;
}
std::vector<ge::Format> &OpParamDef::GetFormats(void) {
  return this->impl_->formats;
}
std::vector<ge::Format> &OpParamDef::GetFormatsList(void) {
  return this->impl_->formats_list;
}
std::vector<ge::Format> &OpParamDef::GetFormatsForBin(void) const {
  return this->impl_->formats_for_bin;
}
bool OpParamDef::IsSetFormatForBin(void) const {
  return this->impl_->set_format_for_bin;
}
std::vector<ge::Format> &OpParamDef::GetUnknownShapeFormats(void) {
  return this->impl_->unknown_shape_formats;
}
ge::AscendString &OpParamDef::GetValueDepend(void) const {
  return this->impl_->value_depend;
}
DependScope &OpParamDef::GetDependScope(void) const {
  return this->impl_->depend_scope;
}
ge::AscendString &OpParamDef::GetFollowName(void) const {
  return this->impl_->follow_port_name;
}
FollowType &OpParamDef::GetFollowType(void) const {
  return this->impl_->follow_type;
}
ge::AscendString &OpParamDef::GetComment(void) const {
  return this->impl_->comment;
}
bool OpParamDef::GetIgnoreContiguous(void) {
  return this->impl_->ignore_contiguous;
}
bool OpParamDef::GetAutoContiguous(void) {
  return this->impl_->auto_contiguous;
}
bool OpParamDef::IsScalar(void) const {
  return this->impl_->is_scalar;
}
bool OpParamDef::IsScalarList(void) const {
  return this->impl_->is_scalar_list;
}
ge::AscendString &OpParamDef::GetScalarName(void) const {
  return this->impl_->scalar_name;
}
ge::DataType OpParamDef::GetScalarType(void) const {
  return this->impl_->scalar_type;
}

uint32_t OpParamDef::GetVersion(void) {
  return this->impl_->version;
}

InitValueType &OpParamDef::GetInitValueType(void) {
  return this->impl_->init_value_type;
}

InitValueNum &OpParamDef::GetInitValue(void) {
  return this->impl_->init_value;
}

std::vector<ScalarVar> &OpParamDef::GetInitValueList(void) {
  return this->impl_->init_value_list;
}

}  // namespace ops
