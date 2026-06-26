/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "pass_registry.h"
#include "framework/common/debug/ge_log.h"
#include "graph/option/optimization_option_info.h"
#include "optimization_option_registry.h"

namespace ge {
namespace fusion {
namespace {
thread_local bool g_has_current_python_pass_create_context = false;
thread_local PythonPassCreateContext g_current_python_pass_create_context = {};

void RegisterPassOption(const std::string &pass_name) {
  const uint64_t level_bits = ge::OoInfoUtils::GenOptLevelBits({OoLevel::kO3});
  GELOGD("Fusion name [%s] registered with compile level %lu.", pass_name.c_str(), level_bits);
  ge::OoInfo opt{pass_name, ge::OoHierarchy::kH1, level_bits};
  ge::OptionRegistry::GetInstance().Register(opt);
  ge::PassOptionRegistry::GetInstance().Register(pass_name, {{ge::OoHierarchy::kH1, opt.name}});
}
}  // namespace

PythonPassCreateScope::PythonPassCreateScope(const PythonPassCreateContext &create_context) {
  has_previous_ = GetCurrentPythonPassCreateContext(previous_context_);
  SetCurrentPythonPassCreateContext(create_context);
}

PythonPassCreateScope::~PythonPassCreateScope() {
  if (has_previous_) {
    SetCurrentPythonPassCreateContext(previous_context_);
    return;
  }
  ClearCurrentPythonPassCreateContext();
}

void SetCurrentPythonPassCreateContext(const PythonPassCreateContext &create_context) {
  g_current_python_pass_create_context = create_context;
  g_has_current_python_pass_create_context = create_context.IsValid();
}

bool GetCurrentPythonPassCreateContext(PythonPassCreateContext &create_context) {
  if (!g_has_current_python_pass_create_context) {
    return false;
  }
  create_context = g_current_python_pass_create_context;
  return true;
}

void ClearCurrentPythonPassCreateContext() {
  g_current_python_pass_create_context = {};
  g_has_current_python_pass_create_context = false;
}

PassRegistry::PassRegistry() = default;
PassRegistry::~PassRegistry() = default;

PassRegistry &PassRegistry::GetInstance() {
  static PassRegistry instance;
  return instance;
}

void PassRegistry::RegisterFusionPass(FusionPassRegistrationData &reg_data) {
  auto pass_name = reg_data.GetPassName().GetString();
  auto iter = name_2_fusion_pass_regs_.find(pass_name);
  if (iter != name_2_fusion_pass_regs_.cend()) {
    GELOGI("Fusion Pass has already registered, detail:[%s]", iter->second.ToString().GetString());
  } else {
    name_2_fusion_pass_regs_.emplace(pass_name, reg_data);
    RegisterPassOption(pass_name);
    GELOGI("Fusion Pass registered success, detail:[%s]", reg_data.ToString().GetString());
  }
}

bool PassRegistry::RegisterPythonPass(const PythonPassDescriptor &pass_desc,
                                      const CreateFusionPassFn &create_fusion_pass_fn) {
  if ((!pass_desc.IsValid()) || (create_fusion_pass_fn == nullptr)) {
    GELOGW("Register python pass failed because descriptor or creator is invalid.");
    return false;
  }

  if (descriptor_key_2_python_pass_descs_.find(pass_desc.descriptor_key) !=
      descriptor_key_2_python_pass_descs_.cend()) {
    GELOGI("Python pass descriptor key [%s] has already registered.", pass_desc.descriptor_key.c_str());
    return false;
  }
  if (name_2_fusion_pass_regs_.find(pass_desc.pass_name) != name_2_fusion_pass_regs_.cend()) {
    GELOGI("Python pass [%s] has already registered.", pass_desc.pass_name.c_str());
    return false;
  }

  FusionPassRegistrationData reg_data(pass_desc.pass_name.c_str());
  reg_data.Stage(pass_desc.stage).CreatePassFn(create_fusion_pass_fn);
  RegisterFusionPass(reg_data);
  descriptor_key_2_python_pass_descs_.emplace(pass_desc.descriptor_key, pass_desc);
  pass_name_2_python_pass_create_contexts_.emplace(pass_desc.pass_name, pass_desc.ToCreateContext());
  GELOGI("Python pass descriptor registered success, pass name[%s], descriptor key[%s].", pass_desc.pass_name.c_str(),
         pass_desc.descriptor_key.c_str());
  return true;
}

std::vector<FusionPassRegistrationData> PassRegistry::GetFusionPassRegDataByStage(CustomPassStage stage) const {
  std::vector<FusionPassRegistrationData> fusion_passes;
  for (const auto &name_2_fusion_reg : name_2_fusion_pass_regs_) {
    if (name_2_fusion_reg.second.GetStage() == stage) {
      fusion_passes.emplace_back(name_2_fusion_reg.second);
      GELOGI("Got Fusion pass:%s", name_2_fusion_reg.second.ToString().GetString());
    }
  }
  return fusion_passes;
}

bool PassRegistry::GetPythonPassDescriptor(const std::string &descriptor_key, PythonPassDescriptor &pass_desc) const {
  auto iter = descriptor_key_2_python_pass_descs_.find(descriptor_key);
  if (iter == descriptor_key_2_python_pass_descs_.cend()) {
    return false;
  }
  pass_desc = iter->second;
  return true;
}

bool PassRegistry::ResolveCurrentPythonPassDescriptor(PythonPassDescriptor &pass_desc) const {
  PythonPassCreateContext create_context;
  if (!GetCurrentPythonPassCreateContext(create_context)) {
    return false;
  }
  if (!GetPythonPassDescriptor(create_context.descriptor_key, pass_desc)) {
    return false;
  }
  return (pass_desc.pass_name == create_context.pass_name) && (pass_desc.kind == create_context.kind);
}

FusionBasePass *PassRegistry::CreatePass(const FusionPassRegistrationData &reg_data) const {
  auto create_fn = reg_data.GetCreatePassFn();
  if (create_fn == nullptr) {
    return nullptr;
  }

  PythonPassCreateContext create_context;
  if (!GetPythonPassCreateContext(reg_data.GetPassName().GetString(), create_context)) {
    return create_fn();
  }

  PythonPassCreateScope scope(create_context);
  return create_fn();
}

void PassRegistry::ClearPythonPasses() {
  for (const auto &item : descriptor_key_2_python_pass_descs_) {
    (void)name_2_fusion_pass_regs_.erase(item.second.pass_name);
    (void)pass_name_2_python_pass_create_contexts_.erase(item.second.pass_name);
  }
  descriptor_key_2_python_pass_descs_.clear();
}

bool PassRegistry::GetPythonPassCreateContext(const std::string &pass_name,
                                              PythonPassCreateContext &create_context) const {
  const auto iter = pass_name_2_python_pass_create_contexts_.find(pass_name);
  if (iter == pass_name_2_python_pass_create_contexts_.cend()) {
    return false;
  }
  create_context = iter->second;
  return true;
}
}  // namespace fusion
}  // namespace ge
