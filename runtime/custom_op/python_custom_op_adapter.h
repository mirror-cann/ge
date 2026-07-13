/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_ADAPTER_H_
#define CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "graph/custom_op/cast.h"
#include "runtime/custom_op/python_custom_op_bridge_types.h"

namespace ge {
namespace custom_op {
class PythonCustomOpRuntimeRegistry {
 public:
  static PythonCustomOpRuntimeRegistry &GetInstance();

  static bool Register(const PythonCustomOpDescriptor &desc, const PythonCustomOpCallbacks &callbacks);
  static bool Unregister(const std::string &descriptor_key);
  static bool Acquire(const PythonCustomOpDescriptor &desc, PythonCustomOpCallbacks &callbacks);
  static void Release(const PythonCustomOpDescriptor &desc);
  static void Clear();

 private:
  PythonCustomOpRuntimeRegistry() = default;
  ~PythonCustomOpRuntimeRegistry() = default;
};

class PythonCustomOpHolder {
 public:
  explicit PythonCustomOpHolder(const PythonCustomOpDescriptor &desc);
  ~PythonCustomOpHolder();

  PythonCustomOpHolder(const PythonCustomOpHolder &) = delete;
  PythonCustomOpHolder &operator=(const PythonCustomOpHolder &) = delete;

  bool IsValid() const;
  void *GetHolder() const;
  const PythonCustomOpCallbacks &GetCallbacks() const;
  const PythonCustomOpDescriptor &GetDescriptor() const;

 private:
  PythonCustomOpDescriptor desc_;
  PythonCustomOpCallbacks callbacks_;
  void *holder_{nullptr};
  bool valid_{false};
};

class PythonCustomOpAdapter final : public EagerExecuteOp,
                                    public CompilableOp,
                                    public ShapeInferOp,
                                    public PortableOp,
                                    public ArgsUpdater,
                                    public CustomOpCapabilityProvider {
 public:
  explicit PythonCustomOpAdapter(PythonCustomOpDescriptor desc);
  ~PythonCustomOpAdapter() override;

  bool IsValid() const;
  bool HasCapability(CustomOpCapability capability) const override;

  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override;
  graphStatus Compile(gert::OpCompileContext *ctx) override;
  graphStatus InferShape(gert::InferShapeContext *ctx) override;
  graphStatus InferDataType(gert::InferDataTypeContext *ctx) override;
  graphStatus Serialize(std::vector<uint8_t> &buffer) override;
  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override;
  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override;

 private:
  graphStatus ReportUnsupported(CustomOpCapability capability, const char *method_name) const;

  PythonCustomOpDescriptor desc_;
  std::unique_ptr<PythonCustomOpHolder> holder_;
};

void ClearPythonCustomOpRuntimeRegistry();
}  // namespace custom_op
}  // namespace ge

#endif  // CANN_GRAPH_ENGINE_RUNTIME_CUSTOM_OP_PYTHON_CUSTOM_OP_ADAPTER_H_
