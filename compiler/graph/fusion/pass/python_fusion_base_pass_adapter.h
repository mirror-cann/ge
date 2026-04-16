/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_PYTHON_FUSION_BASE_PASS_ADAPTER_H
#define CANN_GRAPH_ENGINE_PYTHON_FUSION_BASE_PASS_ADAPTER_H

#include <memory>

#include "ge/fusion/pass/fusion_base_pass.h"
#include "pass_registry.h"

namespace ge {
namespace fusion {
using PythonFusionBasePassHolderCreateFn = void *(*)(const PythonPassDescriptor *pass_desc);
using PythonFusionBasePassHolderDestroyFn = void (*)(void *holder);
using PythonFusionBasePassRunFn = Status (*)(void *holder, GraphPtr &graph, CustomPassContext &pass_context);

struct PythonFusionBasePassCallbacks {
  PythonFusionBasePassHolderCreateFn create{nullptr};
  PythonFusionBasePassHolderDestroyFn destroy{nullptr};
  PythonFusionBasePassRunFn run{nullptr};

  bool IsValid() const {
    return (create != nullptr) && (destroy != nullptr) && (run != nullptr);
  }
};

class PythonFusionBasePassRuntimeRegistry {
 public:
  static PythonFusionBasePassRuntimeRegistry &GetInstance();

  bool Register(const PythonPassDescriptor &pass_desc, const PythonFusionBasePassCallbacks &callbacks);
  bool Unregister(const std::string &descriptor_key);
  bool Acquire(const PythonPassDescriptor &pass_desc, PythonFusionBasePassCallbacks &callbacks);
  void Release(const PythonPassDescriptor &pass_desc);
  void Clear();

 private:
  PythonFusionBasePassRuntimeRegistry() = default;
  ~PythonFusionBasePassRuntimeRegistry() = default;
};

class PythonFusionBasePassAdapter : public FusionBasePass {
 public:
  explicit PythonFusionBasePassAdapter(const PythonPassDescriptor &pass_desc);
  ~PythonFusionBasePassAdapter() override;

  Status Run(GraphPtr &graph, CustomPassContext &pass_context) override;
  bool IsValid() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

FusionBasePass *CreatePythonFusionBasePassAdapter();
bool RegisterPythonFusionBasePass(const PythonPassDescriptor &pass_desc,
                                  const PythonFusionBasePassCallbacks &callbacks);
void ClearPythonFusionBasePassRuntimeRegistry();
}  // namespace fusion
}  // namespace ge

#endif  // CANN_GRAPH_ENGINE_PYTHON_FUSION_BASE_PASS_ADAPTER_H
