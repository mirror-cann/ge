/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "register/op_def.h"
#include "base/asc/opdef/op_def_impl.h"
#include "framework/common/debug/ge_log.h"

namespace ops {
OpAICoreConfig::OpAICoreConfig() : impl_(new(std::nothrow) OpAICoreConfigImpl) {}

OpAICoreConfig::OpAICoreConfig(const char *soc) : impl_(new(std::nothrow) OpAICoreConfigImpl) {
  this->impl_->Construct(this, soc);
}

OpAICoreConfig::OpAICoreConfig(const OpAICoreConfig &aicore_config) : impl_(new(std::nothrow) OpAICoreConfigImpl) {
  this->impl_->Construct(this, aicore_config);
}

OpAICoreConfig::~OpAICoreConfig() = default;

OpAICoreConfig &OpAICoreConfig::operator=(const OpAICoreConfig &aicore_config) {
  return this->impl_->Eq(this, aicore_config);
}

OpParamDef &OpAICoreConfig::Input(const char *name) {
  return this->impl_->op_params.Input(name);
}

OpParamDef &OpAICoreConfig::Output(const char *name) {
  return this->impl_->op_params.Output(name);
}

OpAICoreConfig &OpAICoreConfig::DynamicCompileStaticFlag(bool flag) {
  return this->impl_->DynamicCompileStaticFlag(this, flag);
}

OpAICoreConfig &OpAICoreConfig::DynamicFormatFlag(bool flag) {
  return this->impl_->DynamicFormatFlag(this, flag);
}

OpAICoreConfig &OpAICoreConfig::DynamicRankSupportFlag(bool flag) {
  return this->impl_->DynamicRankSupportFlag(this, flag);
}

OpAICoreConfig &OpAICoreConfig::DynamicShapeSupportFlag(bool flag) {
  return this->impl_->DynamicShapeSupportFlag(this, flag);
}

OpAICoreConfig &OpAICoreConfig::NeedCheckSupportFlag(bool flag) {
  return this->impl_->NeedCheckSupportFlag(this, flag);
}

OpAICoreConfig &OpAICoreConfig::PrecisionReduceFlag(bool flag) {
  return this->impl_->PrecisionReduceFlag(this, flag);
}

OpAICoreConfig &OpAICoreConfig::ExtendCfgInfo(const char *key, const char *value) {
  return this->impl_->ExtendCfgInfo(this, key, value);
}

std::vector<OpParamDef> &OpAICoreConfig::GetInputs(void) const {
  return this->impl_->op_params.GetInputs();
}
std::vector<OpParamDef> &OpAICoreConfig::GetOutputs(void) const {
  return this->impl_->op_params.GetOutputs();
}
void OpAICoreConfig::AddCfgItem(const char *key, const char *value) {
  this->impl_->AddCfgItem(this, key, value);
}

std::vector<ge::AscendString> &OpAICoreConfig::GetCfgKeys(void) {
  return this->impl_->cfg_keys;
}

std::map<ge::AscendString, ge::AscendString> &OpAICoreConfig::GetCfgInfo(void) {
  return this->impl_->cfg_info;
}

ge::AscendString &OpAICoreConfig::GetConfigValue(const char *key) {
  return this->impl_->cfg_info[key];
}

OpAICoreDef::OpAICoreDef() : impl_(new(std::nothrow) OpAICoreDefImpl) {}

OpAICoreDef::OpAICoreDef(const OpAICoreDef &aicore_def) : impl_(new(std::nothrow) OpAICoreDefImpl) {
  this->impl_->Construct(this, aicore_def);
}

OpAICoreDef::~OpAICoreDef() = default;

OpAICoreDef &OpAICoreDef::operator=(const OpAICoreDef &aicore_def) {
  return this->impl_->Eq(this, aicore_def);
}

ge::graphStatus TilingParsePlaceHolder(gert::TilingParseContext* context)
{
  (void)context;
  return ge::GRAPH_SUCCESS;
}

OpAICoreDef &OpAICoreDef::SetTiling(gert::OpImplRegisterV2::TilingKernelFunc func) {
  this->impl_->tiling_func = func;
  this->impl_->tiling_parse = TilingParsePlaceHolder;
  return *this;
}

OpAICoreDef &OpAICoreDef::SetCheckSupport(optiling::OP_CHECK_FUNC func) {
  return this->impl_->SetCheckSupport(this, func);
}

OpAICoreDef &OpAICoreDef::SetOpSelectFormat(optiling::OP_CHECK_FUNC func) {
  return this->impl_->SetOpSelectFormat(this, func);
}

OpAICoreDef &OpAICoreDef::SetOpSupportInfo(optiling::OP_CHECK_FUNC func) {
  return this->impl_->SetOpSupportInfo(this, func);
}

OpAICoreDef &OpAICoreDef::SetOpSpecInfo(optiling::OP_CHECK_FUNC func) {
  return this->impl_->SetOpSpecInfo(this, func);
}

OpAICoreDef &OpAICoreDef::SetParamGeneralize(optiling::PARAM_GENERALIZE_FUNC func) {
  return this->impl_->SetParamGeneralize(this, func);
}

OpAICoreDef &OpAICoreDef::AddConfig(const char *soc) {
  return this->impl_->AddConfig(this, soc);
}

OpAICoreDef &OpAICoreDef::LaunchWithZeroEleOutputTensors(bool launchFlag) {
  return this->impl_->LaunchWithZeroEleOutputTensors(this, launchFlag);
}

OpAICoreDef &OpAICoreDef::AddConfig(const char *soc, OpAICoreConfig &aicore_config) {
  return this->impl_->AddConfig(this, soc, aicore_config);
}

bool OpAICoreDef::GetZeroEleOutputLaunchFlag(void) {
  return this->impl_->zero_ele_output_launch_flag;
}

std::map<ge::AscendString, OpAICoreConfig> &OpAICoreDef::GetAICoreConfigs(void) {
  return this->impl_->aicore_configs;
}

gert::OpImplRegisterV2::TilingKernelFunc &OpAICoreDef::GetTiling(void) {
  return this->impl_->tiling_func;
}

optiling::OP_CHECK_FUNC &OpAICoreDef::GetCheckSupport(void) {
  return this->impl_->op_chk_support;
}
optiling::OP_CHECK_FUNC &OpAICoreDef::GetOpSelectFormat(void) {
  return this->impl_->op_sel_format;
}
optiling::OP_CHECK_FUNC &OpAICoreDef::GetOpSupportInfo(void) {
  return this->impl_->op_get_support;
}
optiling::OP_CHECK_FUNC &OpAICoreDef::GetOpSpecInfo(void) {
  return this->impl_->op_get_spec;
}
optiling::PARAM_GENERALIZE_FUNC &OpAICoreDef::GetParamGeneralize(void) {
  return this->impl_->op_generlize_func;
}
void OpAICoreDef::Log(const char *op_type, const char *info) const {
  GELOGD("%s, op_type:%s.", info, op_type);
}
}  // namespace ops
