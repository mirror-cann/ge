/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "external/graph/named_io_node_builder.h"

#include <exception>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "graph/detail/attributes_holder.h"
#include "graph/ge_attr_value.h"
#include "graph/ge_tensor.h"
#include "graph/op_desc.h"
#include "graph/graph.h"
#include "graph/operator_factory.h"
#include "graph/debug/ge_op_types.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/utils/op_type_utils.h"
#include "graph/utils/recover_ir_utils.h"
#include "graph/utils/tensor_adapter.h"
#include "framework/common/debug/ge_log.h"

namespace ge {

class NamedIoNodeBuilder::Impl {
 public:
  explicit Impl(Graph &graph) : graph_(graph) {}

  void Type(const char_t *type);
  void Name(const char_t *name);
  void AddInput(const char_t *name);
  void AddInput(const char_t *name, const TensorDesc &desc);
  void AddOutput(const char_t *name);
  void AddOutput(const char_t *name, const TensorDesc &desc);
  void Attr(const char_t *name, const AttrValue &value);
  std::unique_ptr<GNode> Build(AscendString &error_message);

 private:
  struct InputRecord {
    std::string name;
    TensorDesc desc;
  };

  struct OutputRecord {
    std::string name;
    TensorDesc desc;
  };

  static std::string DynamicInstanceName(const std::string &ir_name, const size_t index);

  graphStatus ValidateBuildParam(AscendString &error_message) const;
  OpDescPtr CreateOpDesc(AscendString &error_message) const;
  graphStatus SetAttrs(const OpDescPtr &op_desc, AscendString &error_message) const;
  graphStatus RecoverIrDefinition(const OpDescPtr &op_desc, AscendString &error_message) const;
  graphStatus ValidateIrInstance(const OpDescPtr &op_desc, AscendString &error_message) const;
  graphStatus ValidateIrInputInstances(const std::vector<std::pair<std::string, IrInputType>> &ir_inputs,
                                       AscendString &error_message) const;
  graphStatus ValidateIrOutputInstances(const std::vector<std::pair<std::string, IrOutputType>> &ir_outputs,
                                        AscendString &error_message) const;
  graphStatus SetError(AscendString &error_message, const std::string &message) const;
  GNode AddNode(const OpDescPtr &op_desc) const;

  Graph &graph_;
  std::string name_;
  std::string type_;
  std::vector<InputRecord> inputs_;
  std::vector<OutputRecord> outputs_;
  std::vector<std::pair<std::string, AttrValue>> attrs_;
  bool built_ = false;
};

NamedIoNodeBuilder::NamedIoNodeBuilder(Graph &graph) : impl_(new (std::nothrow) Impl(graph)) {}

NamedIoNodeBuilder::~NamedIoNodeBuilder() = default;

std::string NamedIoNodeBuilder::Impl::DynamicInstanceName(const std::string &ir_name, const size_t index) {
  return ir_name + std::to_string(index);
}

NamedIoNodeBuilder &NamedIoNodeBuilder::Type(const char_t *type) {
  if (impl_ != nullptr) {
    impl_->Type(type);
  }
  return *this;
}

void NamedIoNodeBuilder::Impl::Type(const char_t *type) {
  if (type != nullptr) {
    type_ = type;
  }
}

NamedIoNodeBuilder &NamedIoNodeBuilder::Name(const char_t *name) {
  if (impl_ != nullptr) {
    impl_->Name(name);
  }
  return *this;
}

void NamedIoNodeBuilder::Impl::Name(const char_t *name) {
  if (name != nullptr) {
    name_ = name;
  }
}

NamedIoNodeBuilder &NamedIoNodeBuilder::AddInput(const char_t *name) {
  if (impl_ != nullptr) {
    impl_->AddInput(name);
  }
  return *this;
}

void NamedIoNodeBuilder::Impl::AddInput(const char_t *name) {
  if (name != nullptr) {
    inputs_.push_back({name, TensorDesc()});
  }
}

NamedIoNodeBuilder &NamedIoNodeBuilder::AddInput(const char_t *name, const TensorDesc &desc) {
  if (impl_ != nullptr) {
    impl_->AddInput(name, desc);
  }
  return *this;
}

void NamedIoNodeBuilder::Impl::AddInput(const char_t *name, const TensorDesc &desc) {
  if (name != nullptr) {
    inputs_.push_back({name, desc});
  }
}

NamedIoNodeBuilder &NamedIoNodeBuilder::AddOutput(const char_t *name) {
  if (impl_ != nullptr) {
    impl_->AddOutput(name);
  }
  return *this;
}

void NamedIoNodeBuilder::Impl::AddOutput(const char_t *name) {
  if (name != nullptr) {
    outputs_.push_back({name, TensorDesc()});
  }
}

NamedIoNodeBuilder &NamedIoNodeBuilder::AddOutput(const char_t *name, const TensorDesc &desc) {
  if (impl_ != nullptr) {
    impl_->AddOutput(name, desc);
  }
  return *this;
}

void NamedIoNodeBuilder::Impl::AddOutput(const char_t *name, const TensorDesc &desc) {
  if (name != nullptr) {
    outputs_.push_back({name, desc});
  }
}

NamedIoNodeBuilder &NamedIoNodeBuilder::Attr(const char_t *name, const AttrValue &value) {
  if (impl_ != nullptr) {
    impl_->Attr(name, value);
  }
  return *this;
}

void NamedIoNodeBuilder::Impl::Attr(const char_t *name, const AttrValue &value) {
  if ((name != nullptr) && (value.impl != nullptr)) {
    attrs_.emplace_back(name, value);
  }
}

std::unique_ptr<GNode> NamedIoNodeBuilder::Build(AscendString &error_message) {
  // impl_ 由构造函数中 new (std::nothrow) 分配，分配小对象不会失败，此分支不可达
  if (impl_ == nullptr) {
    return nullptr;
  }
  return impl_->Build(error_message);
}

std::unique_ptr<GNode> NamedIoNodeBuilder::Impl::Build(AscendString &error_message) {
  if (ValidateBuildParam(error_message) != GRAPH_SUCCESS) {
    return nullptr;
  }

  const OpDescPtr op_desc = CreateOpDesc(error_message);
  if (op_desc == nullptr) {
    return nullptr;
  }

  if (SetAttrs(op_desc, error_message) != GRAPH_SUCCESS) {
    return nullptr;
  }

  if (RecoverIrDefinition(op_desc, error_message) != GRAPH_SUCCESS) {
    return nullptr;
  }

  if (ValidateIrInstance(op_desc, error_message) != GRAPH_SUCCESS) {
    return nullptr;
  }

  const GNode gnode = AddNode(op_desc);

  std::unique_ptr<GNode> node;
  try {
    node = std::make_unique<GNode>(gnode);
  } catch (const std::exception &) {
    // 不可达：GNode 拷贝构造不会抛异常
    return nullptr;
  }

  built_ = true;
  error_message = AscendString();
  return node;
}

graphStatus NamedIoNodeBuilder::Impl::ValidateBuildParam(AscendString &error_message) const {
  if (built_) {
    error_message = AscendString("Build() has already been called on this builder");
    GELOGE(GRAPH_FAILED, "%s", error_message.GetString());
    return GRAPH_FAILED;
  }

  if (type_.empty()) {
    error_message = AscendString("Type must be set before Build()");
    GELOGE(GRAPH_FAILED, "%s", error_message.GetString());
    return GRAPH_FAILED;
  }

  if (!OperatorFactory::IsExistOp(type_.c_str())) {
    error_message = AscendString(("Op type \"" + type_ + "\" is not registered in OperatorFactory").c_str());
    GELOGE(GRAPH_FAILED, "%s", error_message.GetString());
    return GRAPH_FAILED;
  }
  return GRAPH_SUCCESS;
}

OpDescPtr NamedIoNodeBuilder::Impl::CreateOpDesc(AscendString &error_message) const {
  OpDescBuilder builder(name_, type_);
  for (const auto &input : inputs_) {
    const auto ge_desc = TensorAdapter::TensorDesc2GeTensorDesc(input.desc);
    builder.AddInput(input.name, ge_desc);
  }
  for (const auto &output : outputs_) {
    const auto ge_desc = TensorAdapter::TensorDesc2GeTensorDesc(output.desc);
    builder.AddOutput(output.name, ge_desc);
  }
  const OpDescPtr op_desc = builder.Build();
  if (op_desc == nullptr) {
    // 不可达：已通过 ValidateBuildParam 校验的注册算子，OpDescBuilder::Build 不会返回 nullptr
    error_message = AscendString(("Failed to build OpDesc for " + type_).c_str());
  }
  return op_desc;
}

graphStatus NamedIoNodeBuilder::Impl::SetAttrs(const OpDescPtr &op_desc, AscendString &error_message) const {
  for (const auto &attr : attrs_) {
    const graphStatus ret = op_desc->AttrHolder::SetAttr(attr.first, attr.second.impl->MutableAnyValue());
    if (ret != GRAPH_SUCCESS) {
      error_message = AscendString(("Failed to set attr: " + attr.first).c_str());
      GELOGE(ret, "%s", error_message.GetString());
      return ret;
    }
  }
  return GRAPH_SUCCESS;
}

graphStatus NamedIoNodeBuilder::Impl::RecoverIrDefinition(const OpDescPtr &op_desc, AscendString &error_message) const {
  const graphStatus ret = RecoverIrUtils::RecoverOpDescIrDefinition(op_desc);
  if (ret != GRAPH_SUCCESS) {
    error_message = AscendString(("Failed to recover IR definition for op: " + type_).c_str());
    GELOGE(ret, "%s", error_message.GetString());
    return ret;
  }
  return GRAPH_SUCCESS;
}

graphStatus NamedIoNodeBuilder::Impl::ValidateIrInstance(const OpDescPtr &op_desc, AscendString &error_message) const {
  // Data 和 NetOutput 节点在 RecoverIrDefinition 中被跳过（不恢复 IR 定义），
  // 此处同步跳过校验，避免对无 IR 信息的节点做实例兼容性检查
  if ((op_desc->GetType() == ge::NETOUTPUT) || ge::OpTypeUtils::IsDataNode(op_desc->GetType())) {
    return GRAPH_SUCCESS;
  }
  if (ValidateIrInputInstances(op_desc->GetIrInputs(), error_message) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }
  return ValidateIrOutputInstances(op_desc->GetIrOutputs(), error_message);
}

graphStatus NamedIoNodeBuilder::Impl::ValidateIrInputInstances(
    const std::vector<std::pair<std::string, IrInputType>> &ir_inputs, AscendString &error_message) const {
  size_t input_index = 0U;
  for (const auto &ir_input : ir_inputs) {
    const std::string &ir_name = ir_input.first;
    const IrInputType ir_type = ir_input.second;
    if (ir_type == kIrInputRequired) {
      if (input_index >= inputs_.size()) {
        return SetError(error_message, "Missing required input \"" + ir_name + "\" for op \"" + type_ + "\"");
      }
      const auto &input = inputs_[input_index];
      if (input.name != ir_name) {
        return SetError(error_message, "Input \"" + input.name + "\" is not compatible with IR input \"" + ir_name +
                                           "\" for op \"" + type_ + "\"; check input name and order");
      }
      ++input_index;
    } else if (ir_type == kIrInputOptional) {
      if ((input_index < inputs_.size()) && (inputs_[input_index].name == ir_name)) {
        ++input_index;
      }
    } else if (ir_type == kIrInputDynamic) {
      size_t dynamic_index = 0U;
      while ((input_index < inputs_.size()) &&
             (inputs_[input_index].name == DynamicInstanceName(ir_name, dynamic_index))) {
        ++input_index;
        ++dynamic_index;
      }
    } else {
      // 不可达：IrInputType 枚举只有 kIrInputRequired/kIrInputOptional/kIrInputDynamic
    }
  }
  if (input_index < inputs_.size()) {
    return SetError(error_message, "Input \"" + inputs_[input_index].name +
                                       "\" is not defined in IR or is out of order for op \"" + type_ + "\"");
  }
  return GRAPH_SUCCESS;
}

graphStatus NamedIoNodeBuilder::Impl::ValidateIrOutputInstances(
    const std::vector<std::pair<std::string, IrOutputType>> &ir_outputs, AscendString &error_message) const {
  size_t output_index = 0U;
  for (const auto &ir_output : ir_outputs) {
    const std::string &ir_name = ir_output.first;
    const IrOutputType ir_type = ir_output.second;
    if (ir_type == kIrOutputRequired) {
      if (output_index >= outputs_.size()) {
        return SetError(error_message, "Missing required output \"" + ir_name + "\" for op \"" + type_ + "\"");
      }
      const auto &output = outputs_[output_index];
      if (output.name != ir_name) {
        return SetError(error_message, "Output \"" + output.name + "\" is not compatible with IR output \"" + ir_name +
                                           "\" for op \"" + type_ + "\"; check output name and order");
      }
      ++output_index;
    } else if (ir_type == kIrOutputDynamic) {
      size_t dynamic_index = 0U;
      while ((output_index < outputs_.size()) &&
             (outputs_[output_index].name == DynamicInstanceName(ir_name, dynamic_index))) {
        ++output_index;
        ++dynamic_index;
      }
    } else {
      // 不可达：IrOutputType 枚举只有 kIrOutputRequired/kIrOutputDynamic
    }
  }
  if (output_index < outputs_.size()) {
    return SetError(error_message, "Output \"" + outputs_[output_index].name +
                                       "\" is not defined in IR or is out of order for op \"" + type_ + "\"");
  }
  return GRAPH_SUCCESS;
}

graphStatus NamedIoNodeBuilder::Impl::SetError(AscendString &error_message, const std::string &message) const {
  error_message = AscendString(message.c_str());
  GELOGE(GRAPH_FAILED, "%s", error_message.GetString());
  return GRAPH_FAILED;
}

GNode NamedIoNodeBuilder::Impl::AddNode(const OpDescPtr &op_desc) const {
  Operator op = OpDescUtils::CreateOperatorFromOpDesc(op_desc);
  GNode gnode = graph_.AddNodeByOp(op);
  op.BreakConnect();
  return gnode;
}

}  // namespace ge
