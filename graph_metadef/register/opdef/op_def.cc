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
#include "base/asc/opdef/op_def_impl.h"
#include "framework/common/debug/ge_log.h"
#include "register/op_def.h"
#include "register/op_config_registry.h"

namespace ops {
OpDef::OpDef(const char *type) : impl_(new(std::nothrow) OpDefImpl) {
  OpDefImpl::Construct(this, type);
}

OpDef::OpDef(const OpDef &op_def) : impl_(new(std::nothrow) OpDefImpl) {
  OpDefImpl::Construct(this, op_def);
}

OpDef::~OpDef() = default;

OpDef &OpDef::operator=(const OpDef &op_def) {
  return this->impl_->Eq(this, op_def);
}

OpParamDef &OpDef::Input(const char *name) {
  return this->impl_->op_params.Input(name);
}

OpParamDef &OpDef::Output(const char *name) {
  return this->impl_->op_params.Output(name);
}

OpAttrDef &OpDef::Attr(const char *name) {
  return this->GetOrCreateAttr(name);
}

OpDef &OpDef::Comment(CommentSection section, const char *comment) {
  return this->impl_->Comment(this, section, comment);
}

ItemFindStatus OpDef::FindAttr(const char *name, OpAttrDef **attr) {
  return this->impl_->FindAttr(this, name, attr);
}

OpAttrDef &OpDef::AddAttr(OpAttrDef &attr) {
  return this->impl_->AddAttr(this, attr);
}

OpAttrDef &OpDef::GetOrCreateAttr(const char *name) {
  return this->impl_->GetOrCreateAttr(this, name);
}

std::vector<OpAttrDef> &OpDef::GetAttrs(void) {
  return this->impl_->attrs;
}

OpDef &OpDef::SetInferShape(gert::OpImplRegisterV2::InferShapeKernelFunc func) {
  return this->impl_->SetInferShape(this, func);
}

OpDef &OpDef::SetInferShapeRange(gert::OpImplRegisterV2::InferShapeRangeKernelFunc func) {
  return this->impl_->SetInferShapeRange(this, func);
}

OpDef &OpDef::SetInferDataType(gert::OpImplRegisterV2::InferDataTypeKernelFunc func) {
  return this->impl_->SetInferDataType(this, func);
}

gert::OpImplRegisterV2::InferShapeKernelFunc &OpDef::GetInferShape(void) {
  return this->impl_->infer_shape;
}
gert::OpImplRegisterV2::InferShapeRangeKernelFunc &OpDef::GetInferShapeRange(void) {
  return this->impl_->infer_shape_range;
}
gert::OpImplRegisterV2::InferDataTypeKernelFunc &OpDef::GetInferDataType(void) {
  return this->impl_->infer_data_type;
}
ge::AscendString &OpDef::GetOpType(void) {
  return this->impl_->op_type;
}
ge::AscendString &OpDef::GetCateGory(void) const {
  return this->impl_->category;
}
std::vector<ge::AscendString> &OpDef::GetBrief(void) const {
  return this->impl_->comment_map[ops::CommentSection::BRIEF];
}
std::vector<ge::AscendString> &OpDef::GetConstraints(void) const {
  return this->impl_->comment_map[ops::CommentSection::CONSTRAINTS];
}
std::vector<ge::AscendString> &OpDef::GetRestrictions(void) const {
  return this->impl_->comment_map[ops::CommentSection::RESTRICTIONS];
}
std::vector<ge::AscendString> &OpDef::GetSee(void) const {
  return this->impl_->comment_map[ops::CommentSection::SEE];
}
std::vector<ge::AscendString> &OpDef::GetThirdPartyFwkCopat(void) const {
  return this->impl_->comment_map[ops::CommentSection::THIRDPARTYFWKCOMPAT];
}
std::vector<OpParamDef> &OpDef::GetInputs(void) {
  return this->impl_->op_params.GetInputs();
}

std::vector<OpParamDef> &OpDef::GetOutputs(void) {
  return this->impl_->op_params.GetOutputs();
}

void OpDef::MergeParam(std::vector<OpParamDef> &merge, std::vector<OpParamDef> &aicore_params) const {
  this->impl_->MergeParam(merge, aicore_params);
}

void OpDef::DfsDataType(DfsParam &dfs_param, const std::vector<OpParamDef> &all_param,
                        uint32_t list_idx, uint32_t non_list_idx) const {
  this->impl_->DfsDataType(dfs_param, all_param, list_idx, non_list_idx);
}

void OpDef::DfsFormat(DfsParam &dfs_param, const std::vector<OpParamDef> &all_param,
                      uint32_t list_idx, uint32_t non_list_idx) const {
  this->impl_->DfsFormat(dfs_param, all_param, list_idx, non_list_idx);
}

void OpDef::DfsFullPermutation(DfsParam &dfs_param, const std::vector<OpParamDef> &all_param,
                               uint32_t list_idx, uint32_t non_list_idx) const {
  this->impl_->DfsFullPermutation(dfs_param, all_param, list_idx, non_list_idx);
}

bool OpDef::IsNonListTypes(const OpParamDef &def) const {
  return this->impl_->IsNonListTypes(def);
}

bool OpDef::IsNonListFormats(const OpParamDef &def) const {
  return this->impl_->IsNonListFormats(def);
}

uint32_t OpDef::GetNonListLen(std::vector<OpParamDef> &input_param, std::vector<OpParamDef> &output_param) const {
  return this->impl_->GetNonListLen(this, input_param, output_param);
}

void OpDef::UpdateDtypeImpl(const DfsParam &dfs_param, OpParamDef &param, const uint32_t &param_idx) {
  this->impl_->UpdateDtypeImpl(dfs_param, param, param_idx);
}

void OpDef::UpdateFormatImpl(const DfsParam &dfs_param, OpParamDef &param, const uint32_t &param_idx) {
  this->impl_->UpdateFormatImpl(dfs_param, param, param_idx);
}

void OpDef::UpdateInput(const DfsParam &dfs_param, std::vector<OpParamDef> &input) {
  this->impl_->UpdateInput(this, dfs_param, input);
}

void OpDef::UpdateOutput(const DfsParam &dfs_param, std::vector<OpParamDef> &output) {
  this->impl_->UpdateOutput(this, dfs_param, output);
}

void OpDef::SetPermutedParam(const DfsParam &dfs_param,
                             std::vector<OpParamDef> &input,
                             std::vector<OpParamDef> &output) {
  this->impl_->SetPermutedParam(this, dfs_param, input, output);
}

void OpDef::CheckIncompatible(const std::vector<OpParamDef>& all) const {
  this->impl_->CheckIncompatible(all);
}

void OpDef::FullPermutation(std::vector<OpParamDef> &input_param,
                            std::vector<OpParamDef> &output_param) {
  this->impl_->FullPermutation(this, input_param, output_param);
}

void OpDef::SetDefaultND(std::vector<OpParamDef> &defs) const {
  this->impl_->SetDefaultND(defs);
}

std::vector<std::vector<OpParamDef>> OpDef::GetMergeInputsOutputs(const OpAICoreConfig &aicore_config) {
  return this->impl_->GetMergeInputsOutputs(this, aicore_config);
}

std::vector<OpParamDef> OpDef::GetMergeInputs(OpAICoreConfig &aicore_config) {
  return this->impl_->GetMergeInputs(this, aicore_config);
}

std::vector<OpParamDef> OpDef::GetMergeOutputs(OpAICoreConfig &aicore_config) {
  return this->impl_->GetMergeOutputs(this, aicore_config);
}

OpAICoreDef &OpDef::AICore(void) {
  return this->impl_->op_aicore;
}

OpAICPUDef &OpDef::AICPU(void) {
  return this->impl_->op_aicpu;
}

OpHostCPUDef &OpDef::HostCPU(void) {
  return this->impl_->op_hostcpu;
}

OpMC2Def &OpDef::MC2(void) {
  return this->impl_->op_mc2;
}
 
void OpDef::FollowImpl(void) {
  this->impl_->op_params.FollowDataImpl();
  return;
}
 
void OpDef::FollowListImpl(const DfsParam &dfs_param, std::vector<OpParamDef>& input, std::vector<OpParamDef>& output) {
  this->impl_->op_params.FollowListDataImpl(dfs_param, input, output);
  return;
}

std::map<ge::AscendString, OpDef::PortFollowInfo> OpDef::GetFollowMap(void) {
  return this->impl_->op_params.GetFollowMap();
}
std::map<ge::AscendString, std::vector<std::pair<ge::AscendString, OpDef::PortStat>>> OpDef::GetFollowShapeMap(void) {
  return this->impl_->op_params.GetShapeMap();
}
std::map<ge::AscendString, std::vector<std::pair<ge::AscendString, OpDef::PortStat>>> OpDef::GetFollowTypeMap(void) {
  return this->impl_->op_params.GetDtypeMap();
}
OpParamDef OpDef::GetParamDef(const ge::AscendString& name, OpDef::PortStat stat) {
  return this->impl_->op_params.GetParamDef(name, stat);
}

OpDef &OpDef::FormatMatchMode(FormatCheckOption option) {
  return this->impl_->FormatMatchMode(this, option);
}

FormatCheckOption OpDef::GetFormatMatchMode(void) {
  return this->impl_->format_mode;
}

OpDef &OpDef::EnableFallBack(void) {
  return this->impl_->EnableFallBack(this);
}

bool OpDef::IsEnableFallBack(void) {
  return this->impl_->enable_fall_back;
}

}  // namespace ops
