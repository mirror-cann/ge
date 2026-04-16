/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#ifndef CANN_GRAPH_ENGINE_PASS_REGISTRY_H
#define CANN_GRAPH_ENGINE_PASS_REGISTRY_H
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "ge/fusion/pass/fusion_pass_reg.h"
namespace ge {
namespace fusion {
enum class PythonPassKind : uint32_t {
  kFusionBase = 0U,
  kPatternFusion = 1U,
  kDecompose = 2U
};

struct PythonPassCreateContext {
  // Python pass 目前仍复用 FusionPassRegistrationData 的无参 CreatePassFn。
  // 因此在 PassRegistry::CreatePass() 选中某个 Python pass 后，需要用一份
  // 创建期上下文把“当前正在创建哪个 descriptor”临时传给共享的 adapter factory。
  // 其中 descriptor_key 是核心路由信息，pass_name/kind 主要用于一致性校验。
  std::string descriptor_key;
  std::string pass_name;
  PythonPassKind kind{PythonPassKind::kFusionBase};

  bool IsValid() const {
    return (!descriptor_key.empty()) && (!pass_name.empty());
  }
};

struct PythonPassDescriptor {
  std::string descriptor_key;
  std::string pass_name;
  std::string module_name;
  std::string class_name;
  CustomPassStage stage{CustomPassStage::kBeforeInferShape};
  PythonPassKind kind{PythonPassKind::kFusionBase};
  std::vector<std::string> op_types;

  bool IsValid() const {
    return (!descriptor_key.empty()) && (!pass_name.empty());
  }

  PythonPassCreateContext ToCreateContext() const {
    return PythonPassCreateContext{descriptor_key, pass_name, kind};
  }
};

class PythonPassCreateScope {
 public:
  explicit PythonPassCreateScope(const PythonPassCreateContext &create_context);
  ~PythonPassCreateScope();

  PythonPassCreateScope(const PythonPassCreateScope &) = delete;
  PythonPassCreateScope &operator=(const PythonPassCreateScope &) = delete;

 private:
  bool has_previous_{false};
  PythonPassCreateContext previous_context_;
};

void SetCurrentPythonPassCreateContext(const PythonPassCreateContext &create_context);
bool GetCurrentPythonPassCreateContext(PythonPassCreateContext &create_context);
void ClearCurrentPythonPassCreateContext();

inline std::string CustomPassStageToString(CustomPassStage stage) {
  static const std::map<CustomPassStage, std::string> kCustomPassStageToStringMap = {
      {CustomPassStage::kBeforeInferShape, "BeforeInferShape"},
      {CustomPassStage::kAfterInferShape, "AfterInferShape"},
      {CustomPassStage::kAfterAssignLogicStream, "AfterAssignLogicStream"},
      {CustomPassStage::kAfterBuiltinFusionPass, "AfterBuiltinFusionPass"},
      {CustomPassStage::kAfterOriginGraphOptimize, "AfterOriginGraphOptimize"},
      {CustomPassStage::kCompatibleInherited, "CompatibleInherited"},
      {CustomPassStage::kInvalid, "InvalidStage"}
  };
  if (stage > CustomPassStage::kInvalid) {
    return "";
  }
  return kCustomPassStageToStringMap.find(stage)->second;
}

class PassRegistry {
 public:
  ~PassRegistry();

  static PassRegistry &GetInstance();

  void RegisterFusionPass(FusionPassRegistrationData &reg_data);

  bool RegisterPythonPass(const PythonPassDescriptor &pass_desc, const CreateFusionPassFn &create_fusion_pass_fn);

  std::vector<FusionPassRegistrationData> GetFusionPassRegDataByStage(CustomPassStage stage) const;

  bool GetPythonPassDescriptor(const std::string &descriptor_key, PythonPassDescriptor &pass_desc) const;

  bool ResolveCurrentPythonPassDescriptor(PythonPassDescriptor &pass_desc) const;

  FusionBasePass *CreatePass(const FusionPassRegistrationData &reg_data) const;

  void ClearPythonPasses();

 private:
  PassRegistry();
  bool GetPythonPassCreateContext(const std::string &pass_name, PythonPassCreateContext &create_context) const;
  std::map<std::string, FusionPassRegistrationData> name_2_fusion_pass_regs_;
  std::map<std::string, PythonPassDescriptor> descriptor_key_2_python_pass_descs_;
  std::map<std::string, PythonPassCreateContext> pass_name_2_python_pass_create_contexts_;
};
}  // namespace fuison
}  // namespace ge


#endif  // CANN_GRAPH_ENGINE_PASS_REGISTRY_H
