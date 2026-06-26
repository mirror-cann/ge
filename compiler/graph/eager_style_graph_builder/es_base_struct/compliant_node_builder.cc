/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "compliant_node_builder.h"
#include "graph/graph.h"
#include "common/checker.h"
namespace ge {
namespace es {

using IrInputDef = CompliantNodeBuilder::IrInputDef;
using IrOutputDef = CompliantNodeBuilder::IrOutputDef;
using IrAttrDef = CompliantNodeBuilder::IrAttrDef;
using IrInputDefV2 = CompliantNodeBuilder::IrInputDefV2;
using IrOutputDefV2 = CompliantNodeBuilder::IrOutputDefV2;
using IrAttrDefV2 = CompliantNodeBuilder::IrAttrDefV2;
using AnyTypeOperator = CompliantNodeBuilder::AnyTypeOperator;

namespace {
std::string ToString(const char_t *value) {
  return (value == nullptr) ? std::string() : std::string(value);
}
}  // namespace

class CompliantNodeBuilder::IrInputDefV2::Impl {
 public:
  std::string name;
  IrInputType ir_input_type = CompliantNodeBuilder::kEsIrInputRequired;
  std::string symbol_id;
};

CompliantNodeBuilder::IrInputDefV2::IrInputDefV2() : impl_(new (std::nothrow) Impl()) {}

CompliantNodeBuilder::IrInputDefV2::IrInputDefV2(const char_t *name, IrInputType ir_input_type, const char_t *symbol_id)
    : impl_(new (std::nothrow) Impl{ToString(name), ir_input_type, ToString(symbol_id)}) {}

CompliantNodeBuilder::IrInputDefV2::~IrInputDefV2() {
  delete impl_;
}

CompliantNodeBuilder::IrInputDefV2::IrInputDefV2(const IrInputDefV2 &other)
    : impl_((other.impl_ == nullptr) ? nullptr : new (std::nothrow) Impl(*other.impl_)) {}

CompliantNodeBuilder::IrInputDefV2 &CompliantNodeBuilder::IrInputDefV2::operator=(const IrInputDefV2 &other) {
  if (this != &other) {
    auto *new_impl = (other.impl_ == nullptr) ? nullptr : new (std::nothrow) Impl(*other.impl_);
    delete impl_;
    impl_ = new_impl;
  }
  return *this;
}

CompliantNodeBuilder::IrInputDefV2::IrInputDefV2(IrInputDefV2 &&other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}

CompliantNodeBuilder::IrInputDefV2 &CompliantNodeBuilder::IrInputDefV2::operator=(IrInputDefV2 &&other) noexcept {
  if (this != &other) {
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

CompliantNodeBuilder::IrInputDefV2 &CompliantNodeBuilder::IrInputDefV2::Name(const char_t *name) {
  if (impl_ != nullptr) {
    impl_->name = ToString(name);
  }
  return *this;
}

CompliantNodeBuilder::IrInputDefV2 &CompliantNodeBuilder::IrInputDefV2::InputType(IrInputType ir_input_type) {
  if (impl_ != nullptr) {
    impl_->ir_input_type = ir_input_type;
  }
  return *this;
}

CompliantNodeBuilder::IrInputDefV2 &CompliantNodeBuilder::IrInputDefV2::SymbolId(const char_t *symbol_id) {
  if (impl_ != nullptr) {
    impl_->symbol_id = ToString(symbol_id);
  }
  return *this;
}

const char_t *CompliantNodeBuilder::IrInputDefV2::GetName() const {
  return (impl_ == nullptr) ? "" : impl_->name.c_str();
}

CompliantNodeBuilder::IrInputType CompliantNodeBuilder::IrInputDefV2::GetInputType() const {
  return (impl_ == nullptr) ? CompliantNodeBuilder::kEsIrInputRequired : impl_->ir_input_type;
}

const char_t *CompliantNodeBuilder::IrInputDefV2::GetSymbolId() const {
  return (impl_ == nullptr) ? "" : impl_->symbol_id.c_str();
}

class CompliantNodeBuilder::IrOutputDefV2::Impl {
 public:
  std::string name;
  IrOutputType ir_output_type = CompliantNodeBuilder::kEsIrOutputRequired;
  std::string symbol_id;
};

CompliantNodeBuilder::IrOutputDefV2::IrOutputDefV2() : impl_(new (std::nothrow) Impl()) {}

CompliantNodeBuilder::IrOutputDefV2::IrOutputDefV2(const char_t *name, IrOutputType ir_output_type,
                                                   const char_t *symbol_id)
    : impl_(new (std::nothrow) Impl{ToString(name), ir_output_type, ToString(symbol_id)}) {}

CompliantNodeBuilder::IrOutputDefV2::~IrOutputDefV2() {
  delete impl_;
}

CompliantNodeBuilder::IrOutputDefV2::IrOutputDefV2(const IrOutputDefV2 &other)
    : impl_((other.impl_ == nullptr) ? nullptr : new (std::nothrow) Impl(*other.impl_)) {}

CompliantNodeBuilder::IrOutputDefV2 &CompliantNodeBuilder::IrOutputDefV2::operator=(const IrOutputDefV2 &other) {
  if (this != &other) {
    auto *new_impl = (other.impl_ == nullptr) ? nullptr : new (std::nothrow) Impl(*other.impl_);
    delete impl_;
    impl_ = new_impl;
  }
  return *this;
}

CompliantNodeBuilder::IrOutputDefV2::IrOutputDefV2(IrOutputDefV2 &&other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}

CompliantNodeBuilder::IrOutputDefV2 &CompliantNodeBuilder::IrOutputDefV2::operator=(IrOutputDefV2 &&other) noexcept {
  if (this != &other) {
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

CompliantNodeBuilder::IrOutputDefV2 &CompliantNodeBuilder::IrOutputDefV2::Name(const char_t *name) {
  if (impl_ != nullptr) {
    impl_->name = ToString(name);
  }
  return *this;
}

CompliantNodeBuilder::IrOutputDefV2 &CompliantNodeBuilder::IrOutputDefV2::OutputType(IrOutputType ir_output_type) {
  if (impl_ != nullptr) {
    impl_->ir_output_type = ir_output_type;
  }
  return *this;
}

CompliantNodeBuilder::IrOutputDefV2 &CompliantNodeBuilder::IrOutputDefV2::SymbolId(const char_t *symbol_id) {
  if (impl_ != nullptr) {
    impl_->symbol_id = ToString(symbol_id);
  }
  return *this;
}

const char_t *CompliantNodeBuilder::IrOutputDefV2::GetName() const {
  return (impl_ == nullptr) ? "" : impl_->name.c_str();
}

CompliantNodeBuilder::IrOutputType CompliantNodeBuilder::IrOutputDefV2::GetOutputType() const {
  return (impl_ == nullptr) ? CompliantNodeBuilder::kEsIrOutputRequired : impl_->ir_output_type;
}

const char_t *CompliantNodeBuilder::IrOutputDefV2::GetSymbolId() const {
  return (impl_ == nullptr) ? "" : impl_->symbol_id.c_str();
}

class CompliantNodeBuilder::IrAttrDefV2::Impl {
 public:
  std::string attr_name;
  IrAttrType ir_attr_type = CompliantNodeBuilder::kEsAttrOptional;
  std::string attr_data_type;
  AttrValue attr_default_value;
};

CompliantNodeBuilder::IrAttrDefV2::IrAttrDefV2() : impl_(new (std::nothrow) Impl()) {}

CompliantNodeBuilder::IrAttrDefV2::IrAttrDefV2(const char_t *attr_name, IrAttrType ir_attr_type,
                                               const char_t *attr_data_type, const AttrValue &attr_default_value)
    : impl_(new (std::nothrow) Impl{ToString(attr_name), ir_attr_type, ToString(attr_data_type), attr_default_value}) {}

CompliantNodeBuilder::IrAttrDefV2::~IrAttrDefV2() {
  delete impl_;
}

CompliantNodeBuilder::IrAttrDefV2::IrAttrDefV2(const IrAttrDefV2 &other)
    : impl_((other.impl_ == nullptr) ? nullptr : new (std::nothrow) Impl(*other.impl_)) {}

CompliantNodeBuilder::IrAttrDefV2 &CompliantNodeBuilder::IrAttrDefV2::operator=(const IrAttrDefV2 &other) {
  if (this != &other) {
    auto *new_impl = (other.impl_ == nullptr) ? nullptr : new (std::nothrow) Impl(*other.impl_);
    delete impl_;
    impl_ = new_impl;
  }
  return *this;
}

CompliantNodeBuilder::IrAttrDefV2::IrAttrDefV2(IrAttrDefV2 &&other) noexcept : impl_(other.impl_) {
  other.impl_ = nullptr;
}

CompliantNodeBuilder::IrAttrDefV2 &CompliantNodeBuilder::IrAttrDefV2::operator=(IrAttrDefV2 &&other) noexcept {
  if (this != &other) {
    delete impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
  }
  return *this;
}

CompliantNodeBuilder::IrAttrDefV2 &CompliantNodeBuilder::IrAttrDefV2::AttrName(const char_t *attr_name) {
  if (impl_ != nullptr) {
    impl_->attr_name = ToString(attr_name);
  }
  return *this;
}

CompliantNodeBuilder::IrAttrDefV2 &CompliantNodeBuilder::IrAttrDefV2::AttrType(IrAttrType ir_attr_type) {
  if (impl_ != nullptr) {
    impl_->ir_attr_type = ir_attr_type;
  }
  return *this;
}

CompliantNodeBuilder::IrAttrDefV2 &CompliantNodeBuilder::IrAttrDefV2::AttrDataType(const char_t *attr_data_type) {
  if (impl_ != nullptr) {
    impl_->attr_data_type = ToString(attr_data_type);
  }
  return *this;
}

CompliantNodeBuilder::IrAttrDefV2 &CompliantNodeBuilder::IrAttrDefV2::DefaultValue(
    const AttrValue &attr_default_value) {
  if (impl_ != nullptr) {
    impl_->attr_default_value = attr_default_value;
  }
  return *this;
}

const char_t *CompliantNodeBuilder::IrAttrDefV2::GetAttrName() const {
  return (impl_ == nullptr) ? "" : impl_->attr_name.c_str();
}

CompliantNodeBuilder::IrAttrType CompliantNodeBuilder::IrAttrDefV2::GetAttrType() const {
  return (impl_ == nullptr) ? CompliantNodeBuilder::kEsAttrOptional : impl_->ir_attr_type;
}

const char_t *CompliantNodeBuilder::IrAttrDefV2::GetAttrDataType() const {
  return (impl_ == nullptr) ? "" : impl_->attr_data_type.c_str();
}

const AttrValue &CompliantNodeBuilder::IrAttrDefV2::GetDefaultValue() const {
  static const AttrValue kEmptyAttrValue;
  return (impl_ == nullptr) ? kEmptyAttrValue : impl_->attr_default_value;
}

class CompliantNodeBuilder::CompliantNodeBuilderImpl {
 public:
  explicit CompliantNodeBuilderImpl(ge::Graph *graph) : owner_graph_(graph) {}
  void OpType(const char_t *type) {
    type_ = type;
  }
  void IrDefInputs(std::vector<IrInputDef> input_ir_def) {
    ir_def_inputs_ = std::move(input_ir_def);
  }
  void IrDefOutputs(std::vector<IrOutputDef> output_ir_def) {
    ir_def_outputs_ = std::move(output_ir_def);
  }
  void IrDefAttrs(std::vector<IrAttrDef> attr_ir_def) {
    ir_def_attrs_ = std::move(attr_ir_def);
  }
  void IrDefInputsV2(const IrInputDefV2 *input_ir_def, size_t input_ir_def_num) {
    ir_def_inputs_.clear();
    if (input_ir_def == nullptr) {
      return;
    }
    ir_def_inputs_.reserve(input_ir_def_num);
    for (size_t i = 0U; i < input_ir_def_num; ++i) {
      ir_def_inputs_.push_back(
          {input_ir_def[i].GetName(), input_ir_def[i].GetInputType(), input_ir_def[i].GetSymbolId()});
    }
  }
  void IrDefOutputsV2(const IrOutputDefV2 *output_ir_def, size_t output_ir_def_num) {
    ir_def_outputs_.clear();
    if (output_ir_def == nullptr) {
      return;
    }
    ir_def_outputs_.reserve(output_ir_def_num);
    for (size_t i = 0U; i < output_ir_def_num; ++i) {
      ir_def_outputs_.push_back(
          {output_ir_def[i].GetName(), output_ir_def[i].GetOutputType(), output_ir_def[i].GetSymbolId()});
    }
  }
  void IrDefAttrsV2(const IrAttrDefV2 *attr_ir_def, size_t attr_ir_def_num) {
    ir_def_attrs_.clear();
    if (attr_ir_def == nullptr) {
      return;
    }
    ir_def_attrs_.reserve(attr_ir_def_num);
    for (size_t i = 0U; i < attr_ir_def_num; ++i) {
      ir_def_attrs_.push_back({attr_ir_def[i].GetAttrName(), attr_ir_def[i].GetAttrType(),
                               attr_ir_def[i].GetAttrDataType(), attr_ir_def[i].GetDefaultValue()});
    }
  }
  void Name(const char_t *name) {
    name_ = name;
  }
  GNode Build() const {
    GE_ASSERT_NOTNULL(owner_graph_);
    AnyTypeOperator op{name_.c_str(), type_.c_str()};
    RegisterInputs(op);
    RegisterOutputs(op);
    GE_ASSERT_SUCCESS(UpdateOutputDescs(op));
    GE_ASSERT_SUCCESS(RegisterAttrs(op));

    op.BreakConnect();
    return owner_graph_->AddNodeByOp(op);
  }

  void RegisterInputs(AnyTypeOperator &op) const {
    for (const auto &input : ir_def_inputs_) {
      if (input.ir_input_type == CompliantNodeBuilder::kEsIrInputRequired) {
        op.InputRegister(input.name.c_str(), input.symbol_id.c_str());
      } else if (input.ir_input_type == CompliantNodeBuilder::kEsIrInputOptional) {
        op.OptionalInputRegister(input.name.c_str(), input.symbol_id.c_str());
      } else {
        op.DynamicInputRegister(input.name.c_str(), 0, input.symbol_id.c_str(), true);
        auto iter = dynamic_input_ir_names_to_inst_num_.find(input.name);
        if (iter != dynamic_input_ir_names_to_inst_num_.end()) {
          op.DynamicInputRegister(input.name.c_str(), iter->second, true);
        }
      }
    }
  }

  void RegisterOutputs(AnyTypeOperator &op) const {
    for (const auto &output : ir_def_outputs_) {
      if (output.ir_output_type == CompliantNodeBuilder::kEsIrOutputRequired) {
        op.OutputRegister(output.name.c_str(), output.symbol_id.c_str());
      } else {
        op.DynamicOutputRegister(output.name.c_str(), 0, output.symbol_id.c_str(), true);
        auto iter = dynamic_output_ir_names_to_inst_num_.find(output.name);
        if (iter != dynamic_output_ir_names_to_inst_num_.end()) {
          op.DynamicOutputRegister(output.name.c_str(), iter->second, true);
        }
      }
    }
  }
  Status UpdateOutputDescs(AnyTypeOperator &op) const {
    for (const auto &output_name_and_td : output_names_to_td_) {
      GE_ASSERT_GRAPH_SUCCESS(op.UpdateOutputDesc(output_name_and_td.first.c_str(), output_name_and_td.second));
    }
    return SUCCESS;
  }

  Status RegisterAttrs(AnyTypeOperator &op) const {
    for (const auto &attr : ir_def_attrs_) {
      if (attr.ir_attr_type == CompliantNodeBuilder::kEsAttrRequired) {
        op.RequiredAttrWithTypeRegister(attr.attr_name.c_str(), attr.attr_data_type.c_str());
        (void)op.SetAttr(attr.attr_name.c_str(), attr.attr_default_value);
      } else {
        op.AttrRegister(attr.attr_name.c_str(), attr.attr_default_value);
      }
    }
    return SUCCESS;
  }

  void InstanceDynamicInputNum(const char_t *ir_name, int32_t num) {
    dynamic_input_ir_names_to_inst_num_[ir_name] = num;
  }
  void InstanceDynamicOutputNum(const char_t *ir_name, int32_t num) {
    dynamic_output_ir_names_to_inst_num_[ir_name] = num;
  }
  void InstanceOutputShape(const char_t *name, const vector<int64_t> &shape) {
    InstanceOutputOriginShape(name, shape);
    InstanceOutputStorageShape(name, shape);
  }
  void InstanceOutputOriginShape(const char_t *name, const vector<int64_t> &shape) {
    auto &td = output_names_to_td_[name];
    td.SetOriginShape(Shape{shape});
  }
  void InstanceOutputStorageShape(const char_t *name, const vector<int64_t> &shape) {
    auto &td = output_names_to_td_[name];
    td.SetShape(Shape{shape});
  }
  void InstanceOutputDataType(const char_t *name, const ge::DataType data_type) {
    auto &td = output_names_to_td_[name];
    td.SetDataType(data_type);
  }
  void InstanceOutputFormat(const char_t *name, ge::Format format) {
    InstanceOutputOriginFormat(name, format);
    InstanceOutputStorageFormat(name, format);
  }
  void InstanceOutputOriginFormat(const char_t *name, ge::Format format) {
    auto &td = output_names_to_td_[name];
    td.SetOriginFormat(format);
  }
  void InstanceOutputStorageFormat(const char_t *name, ge::Format format) {
    auto &td = output_names_to_td_[name];
    td.SetFormat(format);
  }

 private:
  ge::Graph *owner_graph_{nullptr};

  // IR定义相关成员
  std::string type_;
  std::vector<IrInputDef> ir_def_inputs_;
  std::vector<IrOutputDef> ir_def_outputs_;
  std::vector<IrAttrDef> ir_def_attrs_;

  // 实例信息相关成员
  std::string name_;
  std::unordered_map<std::string, int32_t> dynamic_input_ir_names_to_inst_num_;
  std::unordered_map<std::string, int32_t> dynamic_output_ir_names_to_inst_num_;
  std::unordered_map<std::string, TensorDesc> output_names_to_td_;
};

CompliantNodeBuilder::CompliantNodeBuilder(ge::Graph *graph) {
  // new失败说明业务无法正常进行，主动抛出异常
  impl_ = std::make_unique<CompliantNodeBuilderImpl>(graph);
}
CompliantNodeBuilder::~CompliantNodeBuilder() = default;
CompliantNodeBuilder::CompliantNodeBuilder(CompliantNodeBuilder &&) noexcept = default;
CompliantNodeBuilder &CompliantNodeBuilder::operator=(CompliantNodeBuilder &&) noexcept = default;

CompliantNodeBuilder &CompliantNodeBuilder::OpType(const char_t *type) {
  impl_->OpType(type);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::IrDefInputs(std::vector<IrInputDef> input_ir_def) {
  impl_->IrDefInputs(input_ir_def);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::IrDefOutputs(std::vector<IrOutputDef> output_ir_def) {
  impl_->IrDefOutputs(output_ir_def);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::IrDefAttrs(std::vector<IrAttrDef> attr_ir_def) {
  impl_->IrDefAttrs(attr_ir_def);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::IrDefInputsV2(const IrInputDefV2 *input_ir_def, size_t input_ir_def_num) {
  impl_->IrDefInputsV2(input_ir_def, input_ir_def_num);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::IrDefOutputsV2(const IrOutputDefV2 *output_ir_def,
                                                           size_t output_ir_def_num) {
  impl_->IrDefOutputsV2(output_ir_def, output_ir_def_num);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::IrDefAttrsV2(const IrAttrDefV2 *attr_ir_def, size_t attr_ir_def_num) {
  impl_->IrDefAttrsV2(attr_ir_def, attr_ir_def_num);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::Name(const char_t *name) {
  impl_->Name(name);
  return *this;
}
GNode CompliantNodeBuilder::Build() const {
  return impl_->Build();
}

CompliantNodeBuilder &CompliantNodeBuilder::InstanceDynamicInputNum(const char_t *ir_name, int32_t num) {
  impl_->InstanceDynamicInputNum(ir_name, num);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::InstanceDynamicOutputNum(const char_t *ir_name, int32_t num) {
  impl_->InstanceDynamicOutputNum(ir_name, num);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::InstanceOutputShape(const char_t *name, const vector<int64_t> &shape) {
  impl_->InstanceOutputShape(name, shape);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::InstanceOutputOriginShape(const char_t *name,
                                                                      const vector<int64_t> &shape) {
  impl_->InstanceOutputOriginShape(name, shape);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::InstanceOutputStorageShape(const char_t *name,
                                                                       const vector<int64_t> &shape) {
  impl_->InstanceOutputStorageShape(name, shape);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::InstanceOutputDataType(const char_t *name, ge::DataType data_type) {
  impl_->InstanceOutputDataType(name, data_type);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::InstanceOutputFormat(const char_t *name, ge::Format format) {
  impl_->InstanceOutputFormat(name, format);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::InstanceOutputOriginFormat(const char_t *name, ge::Format format) {
  impl_->InstanceOutputOriginFormat(name, format);
  return *this;
}
CompliantNodeBuilder &CompliantNodeBuilder::InstanceOutputStorageFormat(const char_t *name, ge::Format format) {
  impl_->InstanceOutputStorageFormat(name, format);
  return *this;
}

ge::graphStatus AddEdgeAndUpdatePeerDesc(Graph &graph, GNode &src_node, int32_t src_port_index, GNode &dst_node,
                                         int32_t dst_port_index) {
  GE_ASSERT_GRAPH_SUCCESS(graph.AddDataEdge(src_node, src_port_index, dst_node, dst_port_index), "Add edge failed");
  TensorDesc dst_tensor_desc;
  if (dst_node.GetInputDesc(dst_port_index, dst_tensor_desc) == GRAPH_FAILED) {
    GE_ASSERT_GRAPH_SUCCESS(dst_node.UpdateInputDesc(dst_port_index, dst_tensor_desc),
                            "Update InputDesc of index %s using default TensorDesc failed", dst_port_index);
  }
  return ge::GRAPH_SUCCESS;
}

}  // namespace es
}  // namespace ge
