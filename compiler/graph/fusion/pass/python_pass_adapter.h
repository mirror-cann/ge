/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_PYTHON_PASS_ADAPTER_H
#define CANN_GRAPH_ENGINE_PYTHON_PASS_ADAPTER_H

#include <memory>
#include <vector>

#include "ge/fusion/pass/fusion_base_pass.h"
#include "ge/fusion/pass/pattern_fusion_pass.h"
#include "pass_registry.h"

namespace ge {
namespace fusion {
using PythonFusionBasePassHolderCreateFn = void *(*)(const PythonPassDescriptor *pass_desc);
using PythonFusionBasePassHolderDestroyFn = void (*)(void *holder);
using PythonFusionBasePassRunFn = Status (*)(void *holder, GraphPtr &graph, CustomPassContext &pass_context);

using PythonFusionPassPatternsFn = Status (*)(void *holder, std::vector<PatternUniqPtr> &patterns);
using PythonFusionPassMeetRequirementsFn = bool (*)(void *holder, const std::unique_ptr<MatchResult> &match_result);
using PythonFusionPassReplacementFn =
    Status (*)(void *holder, const std::unique_ptr<MatchResult> &match_result, GraphUniqPtr &replacement_graph);

struct PythonFusionPassCallbacks {
  PythonFusionBasePassHolderCreateFn create{nullptr};
  PythonFusionBasePassHolderDestroyFn destroy{nullptr};
  PythonFusionBasePassRunFn run{nullptr};
  PythonFusionPassPatternsFn patterns{nullptr};
  PythonFusionPassMeetRequirementsFn meet_requirements{nullptr};
  PythonFusionPassReplacementFn replacement{nullptr};

  bool IsValid(PythonPassKind kind) const {
    if ((create == nullptr) || (destroy == nullptr)) {
      return false;
    }
    switch (kind) {
      case PythonPassKind::kFusionBase:
        return run != nullptr;
      case PythonPassKind::kPatternFusion:
        return (patterns != nullptr) && (replacement != nullptr);
      default:
        return false;
    }
  }
};

class PythonFusionPassRuntimeRegistry {
 public:
  static PythonFusionPassRuntimeRegistry &GetInstance();

  bool Register(const PythonPassDescriptor &pass_desc, const PythonFusionPassCallbacks &callbacks);
  bool Unregister(const std::string &descriptor_key);
  bool Acquire(const PythonPassDescriptor &pass_desc, PythonFusionPassCallbacks &callbacks);
  void Release(const PythonPassDescriptor &pass_desc);
  void Clear();

 private:
  PythonFusionPassRuntimeRegistry() = default;
  ~PythonFusionPassRuntimeRegistry() = default;
};

class PythonPassHolder {
 public:
  explicit PythonPassHolder(const PythonPassDescriptor &pass_desc);
  ~PythonPassHolder();

  PythonPassHolder(const PythonPassHolder &) = delete;
  PythonPassHolder &operator=(const PythonPassHolder &) = delete;

  bool IsValid() const;
  void *GetHolder() const;
  const PythonFusionPassCallbacks &GetCallbacks() const;
  const PythonPassDescriptor &GetPassDescriptor() const;

 private:
  PythonPassDescriptor pass_desc_;
  PythonFusionPassCallbacks callbacks_;
  void *holder_{nullptr};
  bool valid_{false};
};

class PythonFusionBasePassAdapter : public FusionBasePass {
 public:
  explicit PythonFusionBasePassAdapter(const PythonPassDescriptor &pass_desc);
  ~PythonFusionBasePassAdapter() override;

  Status Run(GraphPtr &graph, CustomPassContext &pass_context) override;
  bool IsValid() const;

 private:
  std::unique_ptr<PythonPassHolder> holder_;
};

class PythonPatternFusionPassAdapter : public PatternFusionPass {
 public:
  explicit PythonPatternFusionPassAdapter(const PythonPassDescriptor &pass_desc);
  ~PythonPatternFusionPassAdapter() override;

  bool IsValid() const;

 protected:
  std::vector<PatternUniqPtr> Patterns() override;
  bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override;
  GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override;

 private:
  std::unique_ptr<PythonPassHolder> holder_;
};

FusionBasePass *CreatePythonPassAdapter();
bool RegisterPythonPass(const PythonPassDescriptor &pass_desc,
                        const PythonFusionPassCallbacks &callbacks);

void ClearPythonPassRuntimeRegistry();
}  // namespace fusion
}  // namespace ge

#endif  // CANN_GRAPH_ENGINE_PYTHON_PASS_ADAPTER_H
