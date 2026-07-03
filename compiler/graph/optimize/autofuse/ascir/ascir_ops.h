/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

// Generated from asc-ir definition files, any modification made to this file may be overwritten after compile.
// If you want to add self-defined asc-ir, please create a seperated header file.
#ifndef ASCIR_OPS_ASCIR_OPS_H_
#define ASCIR_OPS_ASCIR_OPS_H_

#include "ascendc_ir/utils/cg_calc_tmp_buff_common_funcs.h"

#include "ascendc_ir/ascendc_ir_core/ascendc_ir.h"

#include "ascendc_ir/ascend_reg_ops.h"

#include "utils/cg_utils.h"

#include "graph/type/tensor_type_impl.h"

#include "graph/type/sym_dtype.h"

#include <variant>
#include <type_traits>
#include <tuple>

// Defined at ascir_builtin_ops_v1.cpp:46
namespace af {
namespace ascir_op {
struct Data : public Operator {
  static constexpr const char *Type = "Data";
  af::AscNodeAttr &attr;
  using AscDataIrAttrDef = af::AscDataIrAttrDef;
  AscDataIrAttrDef &ir_attr;
  af::AscOpOutput y;
  inline Data(const char *name, af::AscGraph &graph)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscDataIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscDataIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
    graph.AddNode(*this);
  }
  inline Data(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscDataIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscDataIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 0U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW(
          "Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, "
          "DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64, DT_BF16}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32,
                                                             DT_UINT8, DT_INT16,   DT_UINT16, DT_UINT32,
                                                             DT_INT64, DT_UINT64,  DT_BF16};
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 0U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    GELOGW(
        "Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, "
        "DT_UINT32, DT_INT64, DT_UINT64, DT_BF16}, cannot infer.");
    return FAILED;
  };

  inline Data &operator=(const Data &) = delete;
  inline Data(Data &&) = delete;
  inline Data(const Data &other)
      : af::Operator(other), attr(other.attr), ir_attr(dynamic_cast<AscDataIrAttrDef &>(*(attr.ir_attr))), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:57
namespace af {
namespace ascir_op {
struct VectorFunc : public Operator {
  using af::Operator::DynamicOutputRegister;
  static constexpr const char *Type = "VectorFunc";
  af::AscNodeAttr &attr;
  struct AscVectorFuncIrAttrDef : public af::AscIrAttrDefBase {
    ~AscVectorFuncIrAttrDef() override = default;
    graphStatus GetSub_graph_name(std::string &sub_graph_name) const {
      auto attr_value = attr_store_.GetAnyValue("sub_graph_name");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(sub_graph_name);
    }
    graphStatus SetSub_graph_name(std::string sub_graph_name) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("sub_graph_name");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(sub_graph_name);
    }
  };
  AscVectorFuncIrAttrDef &ir_attr;
  af::AscOpDynamicInput<0> x;
  std::vector<af::AscOpOutput> y;
  inline VectorFunc(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscVectorFuncIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscVectorFuncIrAttrDef &>(*(attr.ir_attr))),
        x(this) {
    this->DynamicInputRegister("x", 0U, true);
    this->DynamicOutputRegister("y", 0U, true);
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16,   DT_INT8,       DT_INT32,  DT_UINT8,    DT_INT16,
                                 DT_UINT16, DT_UINT32,    DT_INT64,      DT_UINT64, DT_DOUBLE,   DT_BOOL,
                                 DT_STRING, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8,  DT_QINT16,   DT_QINT32,
                                 DT_QUINT8, DT_QUINT16,   DT_RESOURCE,   DT_BF16,   DT_COMPLEX32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16,   DT_INT8,       DT_INT32,  DT_UINT8,    DT_INT16,
                                 DT_UINT16, DT_UINT32,    DT_INT64,      DT_UINT64, DT_DOUBLE,   DT_BOOL,
                                 DT_STRING, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8,  DT_QINT16,   DT_QINT32,
                                 DT_QUINT8, DT_QUINT16,   DT_RESOURCE,   DT_BF16,   DT_COMPLEX32};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16,   DT_INT8,       DT_INT32,  DT_UINT8,    DT_INT16,
                                 DT_UINT16, DT_UINT32,    DT_INT64,      DT_UINT64, DT_DOUBLE,   DT_BOOL,
                                 DT_STRING, DT_COMPLEX64, DT_COMPLEX128, DT_QINT8,  DT_QINT16,   DT_QINT32,
                                 DT_QUINT8, DT_QUINT16,   DT_RESOURCE,   DT_BF16,   DT_COMPLEX32};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline VectorFunc &operator=(const VectorFunc &) = delete;
  inline VectorFunc(VectorFunc &&) = delete;
  inline VectorFunc(const VectorFunc &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscVectorFuncIrAttrDef &>(*(attr.ir_attr))),
        x(this) {
    InstanceOutputy(other.y.size());
  }
  void InstanceOutputy(uint32_t num) {
    this->DynamicOutputRegister("y", num);
    for (size_t i = 0U; i < num; i++) {
      this->y.push_back(af::AscOpOutput(this, i));
    }
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:64
namespace af {
namespace ascir_op {
struct Scalar : public Operator {
  static constexpr const char *Type = "Scalar";
  af::AscNodeAttr &attr;
  struct AscScalarIrAttrDef : public af::AscIrAttrDefBase {
    ~AscScalarIrAttrDef() override = default;
    graphStatus GetValue(std::string &value) const {
      auto attr_value = attr_store_.GetAnyValue("value");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(value);
    }
    graphStatus SetValue(std::string value) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("value");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(value);
    }
    graphStatus GetIndex(int64_t &index) const {
      auto attr_value = attr_store_.GetAnyValue("index");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(index);
    }
    graphStatus SetIndex(int64_t index) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("index");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(index);
    }
  };
  AscScalarIrAttrDef &ir_attr;
  af::AscOpOutput y;
  inline Scalar(const char *name, af::AscGraph &graph)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscScalarIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscScalarIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
    graph.AddNode(*this);
  }
  inline Scalar(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscScalarIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscScalarIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 0U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW(
          "Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, "
          "DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64, DT_BF16}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32,
                                                             DT_UINT8, DT_INT16,   DT_UINT16, DT_UINT32,
                                                             DT_INT64, DT_UINT64,  DT_BF16};
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 0U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    GELOGW(
        "Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, "
        "DT_UINT32, DT_INT64, DT_UINT64, DT_BF16}, cannot infer.");
    return FAILED;
  };

  inline Scalar &operator=(const Scalar &) = delete;
  inline Scalar(Scalar &&) = delete;
  inline Scalar(const Scalar &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscScalarIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:76
namespace af {
namespace ascir_op {
struct ScalarData : public Operator {
  static constexpr const char *Type = "ScalarData";
  af::AscNodeAttr &attr;
  struct AscScalarDataIrAttrDef : public af::AscIrAttrDefBase {
    ~AscScalarDataIrAttrDef() override = default;
    graphStatus GetIndex(int64_t &index) const {
      auto attr_value = attr_store_.GetAnyValue("index");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(index);
    }
    graphStatus SetIndex(int64_t index) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("index");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(index);
    }
  };
  AscScalarDataIrAttrDef &ir_attr;
  af::AscOpOutput y;
  inline ScalarData(const char *name, af::AscGraph &graph)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscScalarDataIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscScalarDataIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
    graph.AddNode(*this);
  }
  inline ScalarData(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscScalarDataIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscScalarDataIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 0U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW(
          "Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, "
          "DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64, DT_BF16}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32,
                                                             DT_UINT8, DT_INT16,   DT_UINT16, DT_UINT32,
                                                             DT_INT64, DT_UINT64,  DT_BF16};
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 0U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    GELOGW(
        "Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, "
        "DT_UINT32, DT_INT64, DT_UINT64, DT_BF16}, cannot infer.");
    return FAILED;
  };

  inline ScalarData &operator=(const ScalarData &) = delete;
  inline ScalarData(ScalarData &&) = delete;
  inline ScalarData(const ScalarData &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscScalarDataIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:87
namespace af {
namespace ascir_op {
struct IndexExpr : public Operator {
  static constexpr const char *Type = "IndexExpr";
  af::AscNodeAttr &attr;
  struct AscIndexExprIrAttrDef : public af::AscIrAttrDefBase {
    ~AscIndexExprIrAttrDef() override = default;
    graphStatus GetExpr(int64_t &expr) const {
      auto attr_value = attr_store_.GetAnyValue("expr");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(expr);
    }
    graphStatus SetExpr(int64_t expr) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("expr");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(expr);
    }
  };
  AscIndexExprIrAttrDef &ir_attr;
  af::AscOpOutput y;
  inline IndexExpr(const char *name, af::AscGraph &graph)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscIndexExprIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscIndexExprIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
    graph.AddNode(*this);
  }
  inline IndexExpr(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscIndexExprIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscIndexExprIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 0U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW(
          "Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, "
          "DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                                             DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_UINT64};
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 0U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    GELOGW(
        "Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, "
        "DT_UINT32, DT_INT64, DT_UINT64}, cannot infer.");
    return FAILED;
  };

  inline IndexExpr &operator=(const IndexExpr &) = delete;
  inline IndexExpr(IndexExpr &&) = delete;
  inline IndexExpr(const IndexExpr &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscIndexExprIrAttrDef &>(*(attr.ir_attr))),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:98
namespace af {
namespace ascir_op {
struct Output : public Operator {
  static constexpr const char *Type = "Output";
  af::AscNodeAttr &attr;
  struct AscOutputIrAttrDef : public af::AscIrAttrDefBase {
    ~AscOutputIrAttrDef() override = default;
    graphStatus GetIndex(int64_t &index) const {
      auto attr_value = attr_store_.GetAnyValue("index");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(index);
    }
    graphStatus SetIndex(int64_t index) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("index");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(index);
    }
  };
  AscOutputIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Output(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscOutputIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscOutputIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Output &operator=(const Output &) = delete;
  inline Output(Output &&) = delete;
  inline Output(const Output &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscOutputIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:108
namespace af {
namespace ascir_op {
struct Workspace : public Operator {
  static constexpr const char *Type = "Workspace";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Workspace(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Workspace &operator=(const Workspace &) = delete;
  inline Workspace(Workspace &&) = delete;
  inline Workspace(const Workspace &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:117
namespace af {
namespace ascir_op {
struct Load : public Operator {
  static constexpr const char *Type = "Load";
  af::AscNodeAttr &attr;
  struct AscLoadIrAttrDef : public af::AscIrAttrDefBase {
    ~AscLoadIrAttrDef() override = default;
    graphStatus GetOffset(Expression &offset) const {
      auto attr_value = attr_store_.GetAnyValue("offset");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset);
    }
    graphStatus SetOffset(Expression offset) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset);
    }
  };
  AscLoadIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Load(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscLoadIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscLoadIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(0);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Load &operator=(const Load &) = delete;
  inline Load(Load &&) = delete;
  inline Load(const Load &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscLoadIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:127
namespace af {
namespace ascir_op {
struct Store : public Operator {
  static constexpr const char *Type = "Store";
  af::AscNodeAttr &attr;
  struct AscStoreIrAttrDef : public af::AscIrAttrDefBase {
    ~AscStoreIrAttrDef() override = default;
    graphStatus GetOffset(Expression &offset) const {
      auto attr_value = attr_store_.GetAnyValue("offset");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset);
    }
    graphStatus SetOffset(Expression offset) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset);
    }
  };
  AscStoreIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Store(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscStoreIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscStoreIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(1);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Store &operator=(const Store &) = delete;
  inline Store(Store &&) = delete;
  inline Store(const Store &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscStoreIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:138
namespace af {
namespace ascir_op {
struct Broadcast : public Operator {
  static constexpr const char *Type = "Broadcast";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Broadcast(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(4);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_UINT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Broadcast &operator=(const Broadcast &) = delete;
  inline Broadcast(Broadcast &&) = delete;
  inline Broadcast(const Broadcast &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:147
namespace af {
namespace ascir_op {
struct RemovePad : public Operator {
  static constexpr const char *Type = "RemovePad";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline RemovePad(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline RemovePad &operator=(const RemovePad &) = delete;
  inline RemovePad(RemovePad &&) = delete;
  inline RemovePad(const RemovePad &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:156
namespace af {
namespace ascir_op {
struct Pad : public Operator {
  static constexpr const char *Type = "Pad";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Pad(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Pad &operator=(const Pad &) = delete;
  inline Pad(Pad &&) = delete;
  inline Pad(const Pad &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:165
namespace af {
namespace ascir_op {
struct Nop : public Operator {
  static constexpr const char *Type = "Nop";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Nop(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_UINT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Nop &operator=(const Nop &) = delete;
  inline Nop(Nop &&) = delete;
  inline Nop(const Nop &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:189
namespace af {
namespace ascir_op {
struct Cast : public Operator {
  static constexpr const char *Type = "Cast";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Cast(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    std::map<ge::DataType, std::set<ge::DataType>> results;
    if (npu_arch == "2201") {
      results = {{DT_FLOAT, {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_INT64, DT_BF16}},
                 {DT_FLOAT16, {DT_FLOAT, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64, DT_INT4}},
                 {DT_INT8, {DT_FLOAT16, DT_UINT8}},
                 {DT_INT32, {DT_FLOAT, DT_FLOAT16, DT_INT16, DT_UINT32, DT_INT64}},
                 {DT_UINT8, {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT4}},
                 {DT_INT16, {DT_FLOAT, DT_FLOAT16, DT_UINT16}},
                 {DT_UINT16, {DT_INT16}},
                 {DT_UINT32, {DT_INT32}},
                 {DT_INT64, {DT_INT32}},
                 {DT_UINT64, {DT_INT64}},
                 {DT_BF16, {DT_FLOAT, DT_INT32}},
                 {DT_INT4, {DT_FLOAT16}}};
    } else if (npu_arch == "3510") {
      results = {{DT_FLOAT, {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT64, DT_BOOL, DT_BF16}},
                 {DT_FLOAT16, {DT_FLOAT, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64}},
                 {DT_INT8, {DT_FLOAT, DT_FLOAT16, DT_UINT8, DT_INT16}},
                 {DT_INT32, {DT_FLOAT, DT_FLOAT16, DT_INT16, DT_UINT32, DT_INT64}},
                 {DT_UINT8, {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT64}},
                 {DT_INT16, {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_UINT8, DT_UINT16}},
                 {DT_UINT16, {DT_INT16}},
                 {DT_UINT32, {DT_INT32}},
                 {DT_INT64, {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_UINT8, DT_UINT64}},
                 {DT_UINT64, {DT_INT64}},
                 {DT_BF16, {DT_FLOAT, DT_INT32}}};
    } else if (npu_arch == "5102") {
      results = {{DT_FLOAT, {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT64, DT_BOOL, DT_BF16}},
                 {DT_FLOAT16, {DT_FLOAT, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64}},
                 {DT_INT8, {DT_FLOAT, DT_FLOAT16, DT_UINT8, DT_INT16}},
                 {DT_INT32, {DT_FLOAT, DT_FLOAT16, DT_INT16, DT_UINT32, DT_INT64}},
                 {DT_UINT8, {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT64}},
                 {DT_INT16, {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_UINT8, DT_UINT16}},
                 {DT_UINT16, {DT_INT16}},
                 {DT_UINT32, {DT_INT32}},
                 {DT_INT64, {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_UINT8, DT_UINT64}},
                 {DT_UINT64, {DT_INT64}},
                 {DT_BF16, {DT_FLOAT, DT_INT32}}};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }

    auto iter = results.find(input_dtypes[0]);
    GE_WARN_ASSERT(iter != results.end());
    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GE_WARN_ASSERT(iter->second.size() == 1U);
      expect_output_dtypes.push_back(*(iter->second.begin()));
      return ge::SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(iter->second.find(expect_output_dtypes[0]) != iter->second.end());

    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 输入输出存在关联, 无法进行推导
    (void)input_dtypes;
    (void)expect_output_dtypes;
    GELOGW("Node type %s is not supported to infernocheck for dtype.", Type);
    return af::FAILED;
  };

  inline Cast &operator=(const Cast &) = delete;
  inline Cast(Cast &&) = delete;
  inline Cast(const Cast &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:197
namespace af {
namespace ascir_op {
struct Abs : public Operator {
  static constexpr const char *Type = "Abs";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Abs(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Abs &operator=(const Abs &) = delete;
  inline Abs(Abs &&) = delete;
  inline Abs(const Abs &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:205
namespace af {
namespace ascir_op {
struct Exp : public Operator {
  static constexpr const char *Type = "Exp";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Exp(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Exp &operator=(const Exp &) = delete;
  inline Exp(Exp &&) = delete;
  inline Exp(const Exp &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:213
namespace af {
namespace ascir_op {
struct Ln : public Operator {
  static constexpr const char *Type = "Ln";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Ln(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Ln &operator=(const Ln &) = delete;
  inline Ln(Ln &&) = delete;
  inline Ln(const Ln &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:221
namespace af {
namespace ascir_op {
struct ArgMax : public Operator {
  static constexpr const char *Type = "ArgMax";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline ArgMax(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_INT32};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_INT64);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_U = {DT_INT64};
    GE_WARN_ASSERT(support_dtypes_of_sym_U.find(expect_output_dtypes[0]) != support_dtypes_of_sym_U.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(DT_INT64);
    return SUCCESS;
  };

  inline ArgMax &operator=(const ArgMax &) = delete;
  inline ArgMax(ArgMax &&) = delete;
  inline ArgMax(const ArgMax &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:230
namespace af {
namespace ascir_op {
struct ArgMaxMultiRPhase1 : public Operator {
  static constexpr const char *Type = "ArgMaxMultiRPhase1";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput value;
  af::AscOpOutput index;
  inline ArgMaxMultiRPhase1(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), value(this, 0), index(this, 1) {
    this->InputRegister("x");
    this->OutputRegister("value");
    this->OutputRegister("index");
    value.TryInitTensorAttr();
    index.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 2U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_INT32};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      expect_output_dtypes.push_back(DT_INT64);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    static std::set<ge::DataType> support_dtypes_of_sym_U = {DT_INT64};
    GE_WARN_ASSERT(support_dtypes_of_sym_U.find(expect_output_dtypes[1]) != support_dtypes_of_sym_U.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    expect_output_dtypes.push_back(DT_INT64);
    return SUCCESS;
  };

  inline ArgMaxMultiRPhase1 &operator=(const ArgMaxMultiRPhase1 &) = delete;
  inline ArgMaxMultiRPhase1(ArgMaxMultiRPhase1 &&) = delete;
  inline ArgMaxMultiRPhase1(const ArgMaxMultiRPhase1 &other)
      : af::Operator(other), attr(other.attr), x(this), value(this, 0), index(this, 1) {
    value.TryInitTensorAttr();
    index.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:240
namespace af {
namespace ascir_op {
struct ArgMaxMultiRPhase2 : public Operator {
  static constexpr const char *Type = "ArgMaxMultiRPhase2";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> value;
  af::AscOpInput<1> index;
  af::AscOpOutput y;
  inline ArgMaxMultiRPhase2(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), value(this), index(this), y(this, 0) {
    this->InputRegister("value");
    this->InputRegister("index");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_INT32};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());
    std::set<ge::DataType> support_dtypes_of_sym_U;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_U = {DT_INT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_U.find(input_dtypes[1]) != support_dtypes_of_sym_U.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[1]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[1] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[1]);
    return SUCCESS;
  };

  inline ArgMaxMultiRPhase2 &operator=(const ArgMaxMultiRPhase2 &) = delete;
  inline ArgMaxMultiRPhase2(ArgMaxMultiRPhase2 &&) = delete;
  inline ArgMaxMultiRPhase2(const ArgMaxMultiRPhase2 &other)
      : af::Operator(other), attr(other.attr), value(this), index(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:249
namespace af {
namespace ascir_op {
struct Sqrt : public Operator {
  static constexpr const char *Type = "Sqrt";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Sqrt(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Sqrt &operator=(const Sqrt &) = delete;
  inline Sqrt(Sqrt &&) = delete;
  inline Sqrt(const Sqrt &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:257
namespace af {
namespace ascir_op {
struct Rsqrt : public Operator {
  static constexpr const char *Type = "Rsqrt";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Rsqrt(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Rsqrt &operator=(const Rsqrt &) = delete;
  inline Rsqrt(Rsqrt &&) = delete;
  inline Rsqrt(const Rsqrt &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:265
namespace af {
namespace ascir_op {
struct Reciprocal : public Operator {
  static constexpr const char *Type = "Reciprocal";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Reciprocal(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Reciprocal &operator=(const Reciprocal &) = delete;
  inline Reciprocal(Reciprocal &&) = delete;
  inline Reciprocal(const Reciprocal &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:273
namespace af {
namespace ascir_op {
struct Erf : public Operator {
  static constexpr const char *Type = "Erf";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Erf(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Erf &operator=(const Erf &) = delete;
  inline Erf(Erf &&) = delete;
  inline Erf(const Erf &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:282
namespace af {
namespace ascir_op {
struct Sign : public Operator {
  static constexpr const char *Type = "Sign";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Sign(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_UINT8, DT_INT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_UINT8, DT_INT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Sign &operator=(const Sign &) = delete;
  inline Sign(Sign &&) = delete;
  inline Sign(const Sign &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:290
namespace af {
namespace ascir_op {
struct Tanh : public Operator {
  static constexpr const char *Type = "Tanh";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Tanh(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Tanh &operator=(const Tanh &) = delete;
  inline Tanh(Tanh &&) = delete;
  inline Tanh(const Tanh &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:298
namespace af {
namespace ascir_op {
struct Isnan : public Operator {
  static constexpr const char *Type = "Isnan";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Isnan(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline Isnan &operator=(const Isnan &) = delete;
  inline Isnan(Isnan &&) = delete;
  inline Isnan(const Isnan &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:306
namespace af {
namespace ascir_op {
struct IsFinite : public Operator {
  static constexpr const char *Type = "IsFinite";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline IsFinite(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline IsFinite &operator=(const IsFinite &) = delete;
  inline IsFinite(IsFinite &&) = delete;
  inline IsFinite(const IsFinite &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:314
namespace af {
namespace ascir_op {
struct IsInf : public Operator {
  static constexpr const char *Type = "IsInf";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline IsInf(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline IsInf &operator=(const IsInf &) = delete;
  inline IsInf(IsInf &&) = delete;
  inline IsInf(const IsInf &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:322
namespace af {
namespace ascir_op {
struct Relu : public Operator {
  static constexpr const char *Type = "Relu";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Relu(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_UINT8, DT_INT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_UINT8, DT_INT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Relu &operator=(const Relu &) = delete;
  inline Relu(Relu &&) = delete;
  inline Relu(const Relu &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:330
namespace af {
namespace ascir_op {
struct Neg : public Operator {
  static constexpr const char *Type = "Neg";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Neg(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Neg &operator=(const Neg &) = delete;
  inline Neg(Neg &&) = delete;
  inline Neg(const Neg &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:342
namespace af {
namespace ascir_op {
struct LogicalNot : public Operator {
  static constexpr const char *Type = "LogicalNot";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline LogicalNot(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    std::map<ge::DataType, std::set<ge::DataType>> results;
    if (npu_arch == "2201") {
      results = {{DT_FLOAT, {DT_FLOAT}},
                 {DT_FLOAT16, {DT_FLOAT16}},
                 {DT_INT32, {DT_INT32}},
                 {DT_UINT8, {DT_UINT8}},
                 {DT_INT16, {DT_INT16}}};
    } else if (npu_arch == "3510") {
      results = {{DT_FLOAT, {DT_UINT8}}, {DT_FLOAT16, {DT_UINT8}}, {DT_INT8, {DT_UINT8}},   {DT_INT32, {DT_UINT8}},
                 {DT_UINT8, {DT_UINT8}}, {DT_INT16, {DT_UINT8}},   {DT_UINT16, {DT_UINT8}}, {DT_UINT32, {DT_UINT8}},
                 {DT_INT64, {DT_UINT8}}, {DT_UINT64, {DT_UINT8}},  {DT_BF16, {DT_UINT8}}};
    } else if (npu_arch == "5102") {
      results = {{DT_FLOAT, {DT_UINT8}}, {DT_FLOAT16, {DT_UINT8}}, {DT_INT8, {DT_UINT8}},   {DT_INT32, {DT_UINT8}},
                 {DT_UINT8, {DT_UINT8}}, {DT_INT16, {DT_UINT8}},   {DT_UINT16, {DT_UINT8}}, {DT_UINT32, {DT_UINT8}},
                 {DT_INT64, {DT_UINT8}}, {DT_UINT64, {DT_UINT8}},  {DT_BF16, {DT_UINT8}}};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }

    auto iter = results.find(input_dtypes[0]);
    GE_WARN_ASSERT(iter != results.end());
    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GE_WARN_ASSERT(iter->second.size() == 1U);
      expect_output_dtypes.push_back(*(iter->second.begin()));
      return ge::SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(iter->second.find(expect_output_dtypes[0]) != iter->second.end());

    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 输入输出存在关联, 无法进行推导
    (void)input_dtypes;
    (void)expect_output_dtypes;
    GELOGW("Node type %s is not supported to infernocheck for dtype.", Type);
    return af::FAILED;
  };

  inline LogicalNot &operator=(const LogicalNot &) = delete;
  inline LogicalNot(LogicalNot &&) = delete;
  inline LogicalNot(const LogicalNot &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:350
namespace af {
namespace ascir_op {
struct Max : public Operator {
  static constexpr const char *Type = "Max";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Max(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Max &operator=(const Max &) = delete;
  inline Max(Max &&) = delete;
  inline Max(const Max &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:358
namespace af {
namespace ascir_op {
struct Sum : public Operator {
  static constexpr const char *Type = "Sum";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Sum(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_INT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_INT16, DT_INT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Sum &operator=(const Sum &) = delete;
  inline Sum(Sum &&) = delete;
  inline Sum(const Sum &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:366
namespace af {
namespace ascir_op {
struct Min : public Operator {
  static constexpr const char *Type = "Min";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Min(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_INT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Min &operator=(const Min &) = delete;
  inline Min(Min &&) = delete;
  inline Min(const Min &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:374
namespace af {
namespace ascir_op {
struct Mean : public Operator {
  static constexpr const char *Type = "Mean";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Mean(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Mean &operator=(const Mean &) = delete;
  inline Mean(Mean &&) = delete;
  inline Mean(const Mean &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:382
namespace af {
namespace ascir_op {
struct Prod : public Operator {
  static constexpr const char *Type = "Prod";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Prod(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Prod &operator=(const Prod &) = delete;
  inline Prod(Prod &&) = delete;
  inline Prod(const Prod &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:390
namespace af {
namespace ascir_op {
struct Sigmoid : public Operator {
  static constexpr const char *Type = "Sigmoid";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Sigmoid(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Sigmoid &operator=(const Sigmoid &) = delete;
  inline Sigmoid(Sigmoid &&) = delete;
  inline Sigmoid(const Sigmoid &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:398
namespace af {
namespace ascir_op {
struct Any : public Operator {
  static constexpr const char *Type = "Any";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Any(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Any &operator=(const Any &) = delete;
  inline Any(Any &&) = delete;
  inline Any(const Any &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:406
namespace af {
namespace ascir_op {
struct All : public Operator {
  static constexpr const char *Type = "All";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline All(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(5);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline All &operator=(const All &) = delete;
  inline All(All &&) = delete;
  inline All(const All &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:414
namespace af {
namespace ascir_op {
struct Add : public Operator {
  static constexpr const char *Type = "Add";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Add(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Add &operator=(const Add &) = delete;
  inline Add(Add &&) = delete;
  inline Add(const Add &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:423
namespace af {
namespace ascir_op {
struct Sub : public Operator {
  static constexpr const char *Type = "Sub";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Sub(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Sub &operator=(const Sub &) = delete;
  inline Sub(Sub &&) = delete;
  inline Sub(const Sub &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:432
namespace af {
namespace ascir_op {
struct Div : public Operator {
  static constexpr const char *Type = "Div";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Div(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Div &operator=(const Div &) = delete;
  inline Div(Div &&) = delete;
  inline Div(const Div &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:441
namespace af {
namespace ascir_op {
struct Mul : public Operator {
  static constexpr const char *Type = "Mul";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Mul(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8,
                                 DT_INT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8,
                                 DT_INT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Mul &operator=(const Mul &) = delete;
  inline Mul(Mul &&) = delete;
  inline Mul(const Mul &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:450
namespace af {
namespace ascir_op {
struct Minimum : public Operator {
  static constexpr const char *Type = "Minimum";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Minimum(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Minimum &operator=(const Minimum &) = delete;
  inline Minimum(Minimum &&) = delete;
  inline Minimum(const Minimum &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:459
namespace af {
namespace ascir_op {
struct Maximum : public Operator {
  static constexpr const char *Type = "Maximum";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Maximum(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Maximum &operator=(const Maximum &) = delete;
  inline Maximum(Maximum &&) = delete;
  inline Maximum(const Maximum &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:473
namespace af {
namespace ascir_op {
struct TrueDiv : public Operator {
  static constexpr const char *Type = "TrueDiv";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline TrueDiv(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::map<ge::DataType, std::set<ge::DataType>> results;
    if (npu_arch == "2201") {
      results = {{DT_FLOAT, {DT_FLOAT}}, {DT_FLOAT16, {DT_FLOAT16}}, {DT_INT32, {DT_FLOAT}}};
    } else if (npu_arch == "3510") {
      results = {{DT_FLOAT, {DT_FLOAT}}, {DT_FLOAT16, {DT_FLOAT16}}, {DT_BF16, {DT_BF16}}};
    } else if (npu_arch == "5102") {
      results = {{DT_FLOAT, {DT_FLOAT}}, {DT_FLOAT16, {DT_FLOAT16}}, {DT_BF16, {DT_BF16}}};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }

    auto iter = results.find(input_dtypes[0]);
    GE_WARN_ASSERT(iter != results.end());
    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GE_WARN_ASSERT(iter->second.size() == 1U);
      expect_output_dtypes.push_back(*(iter->second.begin()));
      return ge::SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(iter->second.find(expect_output_dtypes[0]) != iter->second.end());

    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 输入输出存在关联, 无法进行推导
    (void)input_dtypes;
    (void)expect_output_dtypes;
    GELOGW("Node type %s is not supported to infernocheck for dtype.", Type);
    return af::FAILED;
  };

  inline TrueDiv &operator=(const TrueDiv &) = delete;
  inline TrueDiv(TrueDiv &&) = delete;
  inline TrueDiv(const TrueDiv &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:482
namespace af {
namespace ascir_op {
struct Remainder : public Operator {
  static constexpr const char *Type = "Remainder";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Remainder(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_INT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Remainder &operator=(const Remainder &) = delete;
  inline Remainder(Remainder &&) = delete;
  inline Remainder(const Remainder &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:491
namespace af {
namespace ascir_op {
struct LogicalOr : public Operator {
  static constexpr const char *Type = "LogicalOr";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline LogicalOr(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_UINT8, DT_INT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline LogicalOr &operator=(const LogicalOr &) = delete;
  inline LogicalOr(LogicalOr &&) = delete;
  inline LogicalOr(const LogicalOr &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:502
namespace af {
namespace ascir_op {
struct LogicalAnd : public Operator {
  static constexpr const char *Type = "LogicalAnd";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline LogicalAnd(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_UINT8, DT_INT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline LogicalAnd &operator=(const LogicalAnd &) = delete;
  inline LogicalAnd(LogicalAnd &&) = delete;
  inline LogicalAnd(const LogicalAnd &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:512
namespace af {
namespace ascir_op {
struct Pow : public Operator {
  static constexpr const char *Type = "Pow";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Pow(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Pow &operator=(const Pow &) = delete;
  inline Pow(Pow &&) = delete;
  inline Pow(const Pow &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:521
namespace af {
namespace ascir_op {
struct ClipByValue : public Operator {
  static constexpr const char *Type = "ClipByValue";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> x3;
  af::AscOpOutput y;
  inline ClipByValue(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), x3(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("x3");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    GE_WARN_ASSERT(input_dtypes[1] == input_dtypes[2]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    GE_WARN_ASSERT(input_dtypes[1] == input_dtypes[2]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline ClipByValue &operator=(const ClipByValue &) = delete;
  inline ClipByValue(ClipByValue &&) = delete;
  inline ClipByValue(const ClipByValue &other)
      : af::Operator(other), attr(other.attr), x1(this), x2(this), x3(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:532
namespace af {
namespace ascir_op {
struct Ge : public Operator {
  static constexpr const char *Type = "Ge";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Ge(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline Ge &operator=(const Ge &) = delete;
  inline Ge(Ge &&) = delete;
  inline Ge(const Ge &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:542
namespace af {
namespace ascir_op {
struct Eq : public Operator {
  static constexpr const char *Type = "Eq";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Eq(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline Eq &operator=(const Eq &) = delete;
  inline Eq(Eq &&) = delete;
  inline Eq(const Eq &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:552
namespace af {
namespace ascir_op {
struct Ne : public Operator {
  static constexpr const char *Type = "Ne";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Ne(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline Ne &operator=(const Ne &) = delete;
  inline Ne(Ne &&) = delete;
  inline Ne(const Ne &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:562
namespace af {
namespace ascir_op {
struct Gt : public Operator {
  static constexpr const char *Type = "Gt";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Gt(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline Gt &operator=(const Gt &) = delete;
  inline Gt(Gt &&) = delete;
  inline Gt(const Gt &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:572
namespace af {
namespace ascir_op {
struct Le : public Operator {
  static constexpr const char *Type = "Le";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Le(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline Le &operator=(const Le &) = delete;
  inline Le(Le &&) = delete;
  inline Le(const Le &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:582
namespace af {
namespace ascir_op {
struct Lt : public Operator {
  static constexpr const char *Type = "Lt";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Lt(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline Lt &operator=(const Lt &) = delete;
  inline Lt(Lt &&) = delete;
  inline Lt(const Lt &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:592
namespace af {
namespace ascir_op {
struct Concat : public Operator {
  static constexpr const char *Type = "Concat";
  af::AscNodeAttr &attr;
  af::AscOpDynamicInput<0> x;
  af::AscOpOutput y;
  inline Concat(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->DynamicInputRegister("x", 0U, true);
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(7);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32,  DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_UINT64, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Concat &operator=(const Concat &) = delete;
  inline Concat(Concat &&) = delete;
  inline Concat(const Concat &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:601
namespace af {
namespace ascir_op {
struct Select : public Operator {
  static constexpr const char *Type = "Select";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> x3;
  af::AscOpOutput y;
  inline Select(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), x3(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("x3");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    GE_WARN_ASSERT(input_dtypes[1] == input_dtypes[2]);
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_INT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_INT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_INT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[1]) != support_dtypes_of_sym_T2.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[1]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[1] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[1] == input_dtypes[2]);

    expect_output_dtypes.push_back(input_dtypes[1]);
    return SUCCESS;
  };

  inline Select &operator=(const Select &) = delete;
  inline Select(Select &&) = delete;
  inline Select(const Select &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), x3(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:612
namespace af {
namespace ascir_op {
struct Where : public Operator {
  static constexpr const char *Type = "Where";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> x3;
  af::AscOpOutput y;
  inline Where(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), x3(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("x3");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    GE_WARN_ASSERT(input_dtypes[1] == input_dtypes[2]);
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_INT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[1]) != support_dtypes_of_sym_T2.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[1]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[1] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[1] == input_dtypes[2]);

    expect_output_dtypes.push_back(input_dtypes[1]);
    return SUCCESS;
  };

  inline Where &operator=(const Where &) = delete;
  inline Where(Where &&) = delete;
  inline Where(const Where &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), x3(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:623
namespace af {
namespace ascir_op {
struct MaskedFill : public Operator {
  static constexpr const char *Type = "MaskedFill";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpInput<1> mask;
  af::AscOpInput<2> value;
  af::AscOpOutput y;
  inline MaskedFill(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), mask(this), value(this), y(this, 0) {
    this->InputRegister("x");
    this->InputRegister("mask");
    this->InputRegister("value");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_UINT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[1]) != support_dtypes_of_sym_T1.end());
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[2]);
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[0]) != support_dtypes_of_sym_T2.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[2]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline MaskedFill &operator=(const MaskedFill &) = delete;
  inline MaskedFill(MaskedFill &&) = delete;
  inline MaskedFill(const MaskedFill &other)
      : af::Operator(other), attr(other.attr), x(this), mask(this), value(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:635
namespace af {
namespace ascir_op {
struct Ub2ub : public Operator {
  static constexpr const char *Type = "Ub2ub";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Ub2ub(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_UINT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Ub2ub &operator=(const Ub2ub &) = delete;
  inline Ub2ub(Ub2ub &&) = delete;
  inline Ub2ub(const Ub2ub &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:641
namespace af {
namespace ascir_op {
struct LeakyRelu : public Operator {
  static constexpr const char *Type = "LeakyRelu";
  af::AscNodeAttr &attr;
  struct AscLeakyReluIrAttrDef : public af::AscIrAttrDefBase {
    ~AscLeakyReluIrAttrDef() override = default;
    graphStatus GetNegative_slope(float &negative_slope) const {
      auto attr_value = attr_store_.GetAnyValue("negative_slope");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(negative_slope);
    }
    graphStatus SetNegative_slope(float negative_slope) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("negative_slope");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(negative_slope);
    }
  };
  AscLeakyReluIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline LeakyRelu(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscLeakyReluIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscLeakyReluIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline LeakyRelu &operator=(const LeakyRelu &) = delete;
  inline LeakyRelu(LeakyRelu &&) = delete;
  inline LeakyRelu(const LeakyRelu &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscLeakyReluIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:651
namespace af {
namespace ascir_op {
struct BitwiseAnd : public Operator {
  static constexpr const char *Type = "BitwiseAnd";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline BitwiseAnd(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_INT32, DT_UINT8, DT_INT16, DT_UINT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline BitwiseAnd &operator=(const BitwiseAnd &) = delete;
  inline BitwiseAnd(BitwiseAnd &&) = delete;
  inline BitwiseAnd(const BitwiseAnd &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:660
namespace af {
namespace ascir_op {
struct Gather : public Operator {
  static constexpr const char *Type = "Gather";
  af::AscNodeAttr &attr;
  struct AscGatherIrAttrDef : public af::AscIrAttrDefBase {
    ~AscGatherIrAttrDef() override = default;
    graphStatus GetAxis(int64_t &axis) const {
      auto attr_value = attr_store_.GetAnyValue("axis");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(axis);
    }
    graphStatus SetAxis(int64_t axis) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("axis");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(axis);
    }
    graphStatus GetNegative_index_support(bool &negative_index_support) const {
      auto attr_value = attr_store_.GetAnyValue("negative_index_support");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(negative_index_support);
    }
    graphStatus SetNegative_index_support(bool negative_index_support) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("negative_index_support");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(negative_index_support);
    }
  };
  AscGatherIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Gather(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscGatherIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscGatherIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(8);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_INT32, DT_INT64};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_INT32, DT_INT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_INT32, DT_INT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[1]) != support_dtypes_of_sym_T2.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Gather &operator=(const Gather &) = delete;
  inline Gather(Gather &&) = delete;
  inline Gather(const Gather &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscGatherIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:673
namespace af {
namespace ascir_op {
struct Transpose : public Operator {
  static constexpr const char *Type = "Transpose";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Transpose(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(6);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT32, DT_INT16, DT_UINT16, DT_UINT32};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Transpose &operator=(const Transpose &) = delete;
  inline Transpose(Transpose &&) = delete;
  inline Transpose(const Transpose &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:682
namespace af {
namespace ascir_op {
struct FlashSoftmax : public Operator {
  static constexpr const char *Type = "FlashSoftmax";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> x3;
  af::AscOpOutput y1;
  af::AscOpOutput y2;
  af::AscOpOutput y3;
  inline FlashSoftmax(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create(*this)),
        x1(this),
        x2(this),
        x3(this),
        y1(this, 0),
        y2(this, 1),
        y3(this, 2) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("x3");
    this->OutputRegister("y1");
    this->OutputRegister("y2");
    this->OutputRegister("y3");
    y1.TryInitTensorAttr();
    y2.TryInitTensorAttr();
    y3.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 3U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    return SUCCESS;
  };

  inline FlashSoftmax &operator=(const FlashSoftmax &) = delete;
  inline FlashSoftmax(FlashSoftmax &&) = delete;
  inline FlashSoftmax(const FlashSoftmax &other)
      : af::Operator(other), attr(other.attr), x1(this), x2(this), x3(this), y1(this, 0), y2(this, 1), y3(this, 2) {
    y1.TryInitTensorAttr();
    y2.TryInitTensorAttr();
    y3.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:691
namespace af {
namespace ascir_op {
struct FloorDiv : public Operator {
  static constexpr const char *Type = "FloorDiv";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline FloorDiv(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8,   DT_INT32, DT_UINT8,
                                 DT_INT16, DT_UINT16,  DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline FloorDiv &operator=(const FloorDiv &) = delete;
  inline FloorDiv(FloorDiv &&) = delete;
  inline FloorDiv(const FloorDiv &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:700
namespace af {
namespace ascir_op {
struct Gelu : public Operator {
  static constexpr const char *Type = "Gelu";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Gelu(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Gelu &operator=(const Gelu &) = delete;
  inline Gelu(Gelu &&) = delete;
  inline Gelu(const Gelu &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:708
namespace af {
namespace ascir_op {
struct Axpy : public Operator {
  static constexpr const char *Type = "Axpy";
  af::AscNodeAttr &attr;
  struct AscAxpyIrAttrDef : public af::AscIrAttrDefBase {
    ~AscAxpyIrAttrDef() override = default;
    graphStatus GetAlpha(float &alpha) const {
      auto attr_value = attr_store_.GetAnyValue("alpha");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(alpha);
    }
    graphStatus SetAlpha(float alpha) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("alpha");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(alpha);
    }
  };
  AscAxpyIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Axpy(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscAxpyIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscAxpyIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Axpy &operator=(const Axpy &) = delete;
  inline Axpy(Axpy &&) = delete;
  inline Axpy(const Axpy &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscAxpyIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:717
namespace af {
namespace ascir_op {
struct MatMul : public Operator {
  static constexpr const char *Type = "MatMul";
  af::AscNodeAttr &attr;
  struct AscMatMulIrAttrDef : public af::AscIrAttrDefBase {
    ~AscMatMulIrAttrDef() override = default;
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetTranspose_x1(int64_t &transpose_x1) const {
      auto attr_value = attr_store_.GetAnyValue("transpose_x1");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(transpose_x1);
    }
    graphStatus SetTranspose_x1(int64_t transpose_x1) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("transpose_x1");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(transpose_x1);
    }
    graphStatus GetTranspose_x2(int64_t &transpose_x2) const {
      auto attr_value = attr_store_.GetAnyValue("transpose_x2");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(transpose_x2);
    }
    graphStatus SetTranspose_x2(int64_t transpose_x2) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("transpose_x2");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(transpose_x2);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetEnable_hf32(int64_t &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(int64_t enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
  };
  AscMatMulIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline MatMul(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscMatMulIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscMatMulIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16}, cannot infer.");
    return FAILED;
  };

  inline MatMul &operator=(const MatMul &) = delete;
  inline MatMul(MatMul &&) = delete;
  inline MatMul(const MatMul &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscMatMulIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:732
namespace af {
namespace ascir_op {
struct MatMulBias : public Operator {
  static constexpr const char *Type = "MatMulBias";
  af::AscNodeAttr &attr;
  struct AscMatMulBiasIrAttrDef : public af::AscIrAttrDefBase {
    ~AscMatMulBiasIrAttrDef() override = default;
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetTranspose_x1(int64_t &transpose_x1) const {
      auto attr_value = attr_store_.GetAnyValue("transpose_x1");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(transpose_x1);
    }
    graphStatus SetTranspose_x1(int64_t transpose_x1) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("transpose_x1");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(transpose_x1);
    }
    graphStatus GetTranspose_x2(int64_t &transpose_x2) const {
      auto attr_value = attr_store_.GetAnyValue("transpose_x2");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(transpose_x2);
    }
    graphStatus SetTranspose_x2(int64_t transpose_x2) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("transpose_x2");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(transpose_x2);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetEnable_hf32(int64_t &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(int64_t enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
  };
  AscMatMulBiasIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> bias;
  af::AscOpOutput y;
  inline MatMulBias(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscMatMulBiasIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscMatMulBiasIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        bias(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("bias");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[2]) != support_dtypes_of_sym_T2.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[2]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[2] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[2]);
    return SUCCESS;
  };

  inline MatMulBias &operator=(const MatMulBias &) = delete;
  inline MatMulBias(MatMulBias &&) = delete;
  inline MatMulBias(const MatMulBias &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscMatMulBiasIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        bias(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:748
namespace af {
namespace ascir_op {
struct MatMulOffset : public Operator {
  static constexpr const char *Type = "MatMulOffset";
  af::AscNodeAttr &attr;
  struct AscMatMulOffsetIrAttrDef : public af::AscIrAttrDefBase {
    ~AscMatMulOffsetIrAttrDef() override = default;
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetTranspose_x1(int64_t &transpose_x1) const {
      auto attr_value = attr_store_.GetAnyValue("transpose_x1");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(transpose_x1);
    }
    graphStatus SetTranspose_x1(int64_t transpose_x1) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("transpose_x1");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(transpose_x1);
    }
    graphStatus GetTranspose_x2(int64_t &transpose_x2) const {
      auto attr_value = attr_store_.GetAnyValue("transpose_x2");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(transpose_x2);
    }
    graphStatus SetTranspose_x2(int64_t transpose_x2) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("transpose_x2");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(transpose_x2);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetEnable_hf32(int64_t &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(int64_t enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
  };
  AscMatMulOffsetIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> offset_w;
  af::AscOpOutput y;
  inline MatMulOffset(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscMatMulOffsetIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscMatMulOffsetIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        offset_w(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("offset_w");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T3;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T3.find(input_dtypes[2]) != support_dtypes_of_sym_T3.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16}, cannot infer.");
    return FAILED;
  };

  inline MatMulOffset &operator=(const MatMulOffset &) = delete;
  inline MatMulOffset(MatMulOffset &&) = delete;
  inline MatMulOffset(const MatMulOffset &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscMatMulOffsetIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        offset_w(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:765
namespace af {
namespace ascir_op {
struct MatMulOffsetBias : public Operator {
  static constexpr const char *Type = "MatMulOffsetBias";
  af::AscNodeAttr &attr;
  struct AscMatMulOffsetBiasIrAttrDef : public af::AscIrAttrDefBase {
    ~AscMatMulOffsetBiasIrAttrDef() override = default;
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetTranspose_x1(int64_t &transpose_x1) const {
      auto attr_value = attr_store_.GetAnyValue("transpose_x1");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(transpose_x1);
    }
    graphStatus SetTranspose_x1(int64_t transpose_x1) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("transpose_x1");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(transpose_x1);
    }
    graphStatus GetTranspose_x2(int64_t &transpose_x2) const {
      auto attr_value = attr_store_.GetAnyValue("transpose_x2");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(transpose_x2);
    }
    graphStatus SetTranspose_x2(int64_t transpose_x2) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("transpose_x2");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(transpose_x2);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetEnable_hf32(int64_t &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(int64_t enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
  };
  AscMatMulOffsetBiasIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> bias;
  af::AscOpInput<3> offset_w;
  af::AscOpOutput y;
  inline MatMulOffsetBias(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscMatMulOffsetBiasIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscMatMulOffsetBiasIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        bias(this),
        offset_w(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("bias");
    this->InputRegister("offset_w");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 4U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[2]) != support_dtypes_of_sym_T2.end());
    std::set<ge::DataType> support_dtypes_of_sym_T3;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T3.find(input_dtypes[3]) != support_dtypes_of_sym_T3.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[2]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[2] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 4U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[2]);
    return SUCCESS;
  };

  inline MatMulOffsetBias &operator=(const MatMulOffsetBias &) = delete;
  inline MatMulOffsetBias(MatMulOffsetBias &&) = delete;
  inline MatMulOffsetBias(const MatMulOffsetBias &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscMatMulOffsetBiasIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        bias(this),
        offset_w(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:783
namespace af {
namespace ascir_op {
struct BatchMatMul : public Operator {
  static constexpr const char *Type = "BatchMatMul";
  af::AscNodeAttr &attr;
  struct AscBatchMatMulIrAttrDef : public af::AscIrAttrDefBase {
    ~AscBatchMatMulIrAttrDef() override = default;
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetEnable_hf32(int64_t &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(int64_t enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
    graphStatus GetAdj_x1(int64_t &adj_x1) const {
      auto attr_value = attr_store_.GetAnyValue("adj_x1");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(adj_x1);
    }
    graphStatus SetAdj_x1(int64_t adj_x1) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("adj_x1");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(adj_x1);
    }
    graphStatus GetAdj_x2(int64_t &adj_x2) const {
      auto attr_value = attr_store_.GetAnyValue("adj_x2");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(adj_x2);
    }
    graphStatus SetAdj_x2(int64_t adj_x2) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("adj_x2");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(adj_x2);
    }
  };
  AscBatchMatMulIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline BatchMatMul(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscBatchMatMulIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscBatchMatMulIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16}, cannot infer.");
    return FAILED;
  };

  inline BatchMatMul &operator=(const BatchMatMul &) = delete;
  inline BatchMatMul(BatchMatMul &&) = delete;
  inline BatchMatMul(const BatchMatMul &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscBatchMatMulIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:798
namespace af {
namespace ascir_op {
struct BatchMatMulBias : public Operator {
  static constexpr const char *Type = "BatchMatMulBias";
  af::AscNodeAttr &attr;
  struct AscBatchMatMulBiasIrAttrDef : public af::AscIrAttrDefBase {
    ~AscBatchMatMulBiasIrAttrDef() override = default;
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetEnable_hf32(int64_t &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(int64_t enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
    graphStatus GetAdj_x1(int64_t &adj_x1) const {
      auto attr_value = attr_store_.GetAnyValue("adj_x1");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(adj_x1);
    }
    graphStatus SetAdj_x1(int64_t adj_x1) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("adj_x1");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(adj_x1);
    }
    graphStatus GetAdj_x2(int64_t &adj_x2) const {
      auto attr_value = attr_store_.GetAnyValue("adj_x2");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(adj_x2);
    }
    graphStatus SetAdj_x2(int64_t adj_x2) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("adj_x2");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(adj_x2);
    }
  };
  AscBatchMatMulBiasIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> bias;
  af::AscOpOutput y;
  inline BatchMatMulBias(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscBatchMatMulBiasIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscBatchMatMulBiasIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        bias(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("bias");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[2]) != support_dtypes_of_sym_T2.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[2]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[2] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[2]);
    return SUCCESS;
  };

  inline BatchMatMulBias &operator=(const BatchMatMulBias &) = delete;
  inline BatchMatMulBias(BatchMatMulBias &&) = delete;
  inline BatchMatMulBias(const BatchMatMulBias &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscBatchMatMulBiasIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        bias(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:814
namespace af {
namespace ascir_op {
struct BatchMatMulOffset : public Operator {
  static constexpr const char *Type = "BatchMatMulOffset";
  af::AscNodeAttr &attr;
  struct AscBatchMatMulOffsetIrAttrDef : public af::AscIrAttrDefBase {
    ~AscBatchMatMulOffsetIrAttrDef() override = default;
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetEnable_hf32(int64_t &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(int64_t enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
    graphStatus GetAdj_x1(int64_t &adj_x1) const {
      auto attr_value = attr_store_.GetAnyValue("adj_x1");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(adj_x1);
    }
    graphStatus SetAdj_x1(int64_t adj_x1) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("adj_x1");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(adj_x1);
    }
    graphStatus GetAdj_x2(int64_t &adj_x2) const {
      auto attr_value = attr_store_.GetAnyValue("adj_x2");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(adj_x2);
    }
    graphStatus SetAdj_x2(int64_t adj_x2) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("adj_x2");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(adj_x2);
    }
  };
  AscBatchMatMulOffsetIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> offset_w;
  af::AscOpOutput y;
  inline BatchMatMulOffset(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscBatchMatMulOffsetIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscBatchMatMulOffsetIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        offset_w(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("offset_w");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T3;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T3.find(input_dtypes[2]) != support_dtypes_of_sym_T3.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16}, cannot infer.");
    return FAILED;
  };

  inline BatchMatMulOffset &operator=(const BatchMatMulOffset &) = delete;
  inline BatchMatMulOffset(BatchMatMulOffset &&) = delete;
  inline BatchMatMulOffset(const BatchMatMulOffset &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscBatchMatMulOffsetIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        offset_w(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:831
namespace af {
namespace ascir_op {
struct BatchMatMulOffsetBias : public Operator {
  static constexpr const char *Type = "BatchMatMulOffsetBias";
  af::AscNodeAttr &attr;
  struct AscBatchMatMulOffsetBiasIrAttrDef : public af::AscIrAttrDefBase {
    ~AscBatchMatMulOffsetBiasIrAttrDef() override = default;
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetEnable_hf32(int64_t &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(int64_t enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
    graphStatus GetAdj_x1(int64_t &adj_x1) const {
      auto attr_value = attr_store_.GetAnyValue("adj_x1");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(adj_x1);
    }
    graphStatus SetAdj_x1(int64_t adj_x1) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("adj_x1");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(adj_x1);
    }
    graphStatus GetAdj_x2(int64_t &adj_x2) const {
      auto attr_value = attr_store_.GetAnyValue("adj_x2");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(adj_x2);
    }
    graphStatus SetAdj_x2(int64_t adj_x2) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("adj_x2");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(adj_x2);
    }
  };
  AscBatchMatMulOffsetBiasIrAttrDef &ir_attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> bias;
  af::AscOpInput<3> offset_w;
  af::AscOpOutput y;
  inline BatchMatMulOffsetBias(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscBatchMatMulOffsetBiasIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscBatchMatMulOffsetBiasIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        bias(this),
        offset_w(this),
        y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("bias");
    this->InputRegister("offset_w");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 4U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[2]) != support_dtypes_of_sym_T2.end());
    std::set<ge::DataType> support_dtypes_of_sym_T3;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T3 = {DT_INT8, DT_INT4};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T3.find(input_dtypes[3]) != support_dtypes_of_sym_T3.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[2]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[2] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 4U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[2]);
    return SUCCESS;
  };

  inline BatchMatMulOffsetBias &operator=(const BatchMatMulOffsetBias &) = delete;
  inline BatchMatMulOffsetBias(BatchMatMulOffsetBias &&) = delete;
  inline BatchMatMulOffsetBias(const BatchMatMulOffsetBias &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscBatchMatMulOffsetBiasIrAttrDef &>(*(attr.ir_attr))),
        x1(this),
        x2(this),
        bias(this),
        offset_w(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:849
namespace af {
namespace ascir_op {
struct Conv2D : public Operator {
  static constexpr const char *Type = "Conv2D";
  af::AscNodeAttr &attr;
  struct AscConv2DIrAttrDef : public af::AscIrAttrDefBase {
    ~AscConv2DIrAttrDef() override = default;
    graphStatus GetStrides(std::vector<int64_t> &strides) const {
      auto attr_value = attr_store_.GetAnyValue("strides");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(strides);
    }
    graphStatus SetStrides(std::vector<int64_t> strides) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("strides");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(strides);
    }
    graphStatus GetPads(std::vector<int64_t> &pads) const {
      auto attr_value = attr_store_.GetAnyValue("pads");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(pads);
    }
    graphStatus SetPads(std::vector<int64_t> pads) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("pads");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(pads);
    }
    graphStatus GetDilations(std::vector<int64_t> &dilations) const {
      auto attr_value = attr_store_.GetAnyValue("dilations");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(dilations);
    }
    graphStatus SetDilations(std::vector<int64_t> dilations) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("dilations");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(dilations);
    }
    graphStatus GetGroups(int64_t &groups) const {
      auto attr_value = attr_store_.GetAnyValue("groups");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(groups);
    }
    graphStatus SetGroups(int64_t groups) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("groups");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(groups);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetPad_mode(std::string &pad_mode) const {
      auto attr_value = attr_store_.GetAnyValue("pad_mode");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(pad_mode);
    }
    graphStatus SetPad_mode(std::string pad_mode) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("pad_mode");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(pad_mode);
    }
    graphStatus GetData_format(std::string &data_format) const {
      auto attr_value = attr_store_.GetAnyValue("data_format");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(data_format);
    }
    graphStatus SetData_format(std::string data_format) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("data_format");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(data_format);
    }
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetEnable_hf32(bool &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(bool enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
  };
  AscConv2DIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpInput<1> filter;
  af::AscOpOutput y;
  inline Conv2D(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscConv2DIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscConv2DIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        filter(this),
        y(this, 0) {
    this->InputRegister("x");
    this->InputRegister("filter");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8}, cannot infer.");
    return FAILED;
  };

  inline Conv2D &operator=(const Conv2D &) = delete;
  inline Conv2D(Conv2D &&) = delete;
  inline Conv2D(const Conv2D &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscConv2DIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        filter(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:868
namespace af {
namespace ascir_op {
struct Conv2DBias : public Operator {
  static constexpr const char *Type = "Conv2DBias";
  af::AscNodeAttr &attr;
  struct AscConv2DBiasIrAttrDef : public af::AscIrAttrDefBase {
    ~AscConv2DBiasIrAttrDef() override = default;
    graphStatus GetStrides(std::vector<int64_t> &strides) const {
      auto attr_value = attr_store_.GetAnyValue("strides");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(strides);
    }
    graphStatus SetStrides(std::vector<int64_t> strides) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("strides");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(strides);
    }
    graphStatus GetPads(std::vector<int64_t> &pads) const {
      auto attr_value = attr_store_.GetAnyValue("pads");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(pads);
    }
    graphStatus SetPads(std::vector<int64_t> pads) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("pads");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(pads);
    }
    graphStatus GetDilations(std::vector<int64_t> &dilations) const {
      auto attr_value = attr_store_.GetAnyValue("dilations");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(dilations);
    }
    graphStatus SetDilations(std::vector<int64_t> dilations) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("dilations");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(dilations);
    }
    graphStatus GetGroups(int64_t &groups) const {
      auto attr_value = attr_store_.GetAnyValue("groups");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(groups);
    }
    graphStatus SetGroups(int64_t groups) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("groups");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(groups);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetPad_mode(std::string &pad_mode) const {
      auto attr_value = attr_store_.GetAnyValue("pad_mode");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(pad_mode);
    }
    graphStatus SetPad_mode(std::string pad_mode) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("pad_mode");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(pad_mode);
    }
    graphStatus GetData_format(std::string &data_format) const {
      auto attr_value = attr_store_.GetAnyValue("data_format");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(data_format);
    }
    graphStatus SetData_format(std::string data_format) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("data_format");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(data_format);
    }
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetEnable_hf32(bool &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(bool enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
  };
  AscConv2DBiasIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpInput<1> filter;
  af::AscOpInput<2> bias;
  af::AscOpOutput y;
  inline Conv2DBias(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscConv2DBiasIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscConv2DBiasIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        filter(this),
        bias(this),
        y(this, 0) {
    this->InputRegister("x");
    this->InputRegister("filter");
    this->InputRegister("bias");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[2]) != support_dtypes_of_sym_T2.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[2]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[2] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[2]);
    return SUCCESS;
  };

  inline Conv2DBias &operator=(const Conv2DBias &) = delete;
  inline Conv2DBias(Conv2DBias &&) = delete;
  inline Conv2DBias(const Conv2DBias &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscConv2DBiasIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        filter(this),
        bias(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:888
namespace af {
namespace ascir_op {
struct Conv2DOffset : public Operator {
  static constexpr const char *Type = "Conv2DOffset";
  af::AscNodeAttr &attr;
  struct AscConv2DOffsetIrAttrDef : public af::AscIrAttrDefBase {
    ~AscConv2DOffsetIrAttrDef() override = default;
    graphStatus GetStrides(std::vector<int64_t> &strides) const {
      auto attr_value = attr_store_.GetAnyValue("strides");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(strides);
    }
    graphStatus SetStrides(std::vector<int64_t> strides) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("strides");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(strides);
    }
    graphStatus GetPads(std::vector<int64_t> &pads) const {
      auto attr_value = attr_store_.GetAnyValue("pads");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(pads);
    }
    graphStatus SetPads(std::vector<int64_t> pads) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("pads");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(pads);
    }
    graphStatus GetDilations(std::vector<int64_t> &dilations) const {
      auto attr_value = attr_store_.GetAnyValue("dilations");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(dilations);
    }
    graphStatus SetDilations(std::vector<int64_t> dilations) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("dilations");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(dilations);
    }
    graphStatus GetGroups(int64_t &groups) const {
      auto attr_value = attr_store_.GetAnyValue("groups");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(groups);
    }
    graphStatus SetGroups(int64_t groups) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("groups");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(groups);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetPad_mode(std::string &pad_mode) const {
      auto attr_value = attr_store_.GetAnyValue("pad_mode");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(pad_mode);
    }
    graphStatus SetPad_mode(std::string pad_mode) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("pad_mode");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(pad_mode);
    }
    graphStatus GetData_format(std::string &data_format) const {
      auto attr_value = attr_store_.GetAnyValue("data_format");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(data_format);
    }
    graphStatus SetData_format(std::string data_format) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("data_format");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(data_format);
    }
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetEnable_hf32(bool &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(bool enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
  };
  AscConv2DOffsetIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpInput<1> filter;
  af::AscOpInput<2> offset_w;
  af::AscOpOutput y;
  inline Conv2DOffset(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscConv2DOffsetIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscConv2DOffsetIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        filter(this),
        offset_w(this),
        y(this, 0) {
    this->InputRegister("x");
    this->InputRegister("filter");
    this->InputRegister("offset_w");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T3;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T3 = {DT_INT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T3 = {DT_INT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T3 = {DT_INT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T3.find(input_dtypes[2]) != support_dtypes_of_sym_T3.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8}, cannot infer.");
      return FAILED;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    GELOGW("Output ir_index [0] has multi result {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8}, cannot infer.");
    return FAILED;
  };

  inline Conv2DOffset &operator=(const Conv2DOffset &) = delete;
  inline Conv2DOffset(Conv2DOffset &&) = delete;
  inline Conv2DOffset(const Conv2DOffset &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscConv2DOffsetIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        filter(this),
        offset_w(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:909
namespace af {
namespace ascir_op {
struct Conv2DOffsetBias : public Operator {
  static constexpr const char *Type = "Conv2DOffsetBias";
  af::AscNodeAttr &attr;
  struct AscConv2DOffsetBiasIrAttrDef : public af::AscIrAttrDefBase {
    ~AscConv2DOffsetBiasIrAttrDef() override = default;
    graphStatus GetStrides(std::vector<int64_t> &strides) const {
      auto attr_value = attr_store_.GetAnyValue("strides");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(strides);
    }
    graphStatus SetStrides(std::vector<int64_t> strides) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("strides");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(strides);
    }
    graphStatus GetPads(std::vector<int64_t> &pads) const {
      auto attr_value = attr_store_.GetAnyValue("pads");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(pads);
    }
    graphStatus SetPads(std::vector<int64_t> pads) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("pads");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(pads);
    }
    graphStatus GetDilations(std::vector<int64_t> &dilations) const {
      auto attr_value = attr_store_.GetAnyValue("dilations");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(dilations);
    }
    graphStatus SetDilations(std::vector<int64_t> dilations) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("dilations");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(dilations);
    }
    graphStatus GetGroups(int64_t &groups) const {
      auto attr_value = attr_store_.GetAnyValue("groups");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(groups);
    }
    graphStatus SetGroups(int64_t groups) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("groups");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(groups);
    }
    graphStatus GetHas_relu(int64_t &has_relu) const {
      auto attr_value = attr_store_.GetAnyValue("has_relu");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(has_relu);
    }
    graphStatus SetHas_relu(int64_t has_relu) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("has_relu");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(has_relu);
    }
    graphStatus GetPad_mode(std::string &pad_mode) const {
      auto attr_value = attr_store_.GetAnyValue("pad_mode");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(pad_mode);
    }
    graphStatus SetPad_mode(std::string pad_mode) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("pad_mode");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(pad_mode);
    }
    graphStatus GetData_format(std::string &data_format) const {
      auto attr_value = attr_store_.GetAnyValue("data_format");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(data_format);
    }
    graphStatus SetData_format(std::string data_format) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("data_format");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(data_format);
    }
    graphStatus GetOffset_x(int64_t &offset_x) const {
      auto attr_value = attr_store_.GetAnyValue("offset_x");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset_x);
    }
    graphStatus SetOffset_x(int64_t offset_x) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset_x");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset_x);
    }
    graphStatus GetEnable_hf32(bool &enable_hf32) const {
      auto attr_value = attr_store_.GetAnyValue("enable_hf32");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(enable_hf32);
    }
    graphStatus SetEnable_hf32(bool enable_hf32) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("enable_hf32");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(enable_hf32);
    }
  };
  AscConv2DOffsetBiasIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpInput<1> filter;
  af::AscOpInput<2> bias;
  af::AscOpInput<3> offset_w;
  af::AscOpOutput y;
  inline Conv2DOffsetBias(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscConv2DOffsetBiasIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscConv2DOffsetBiasIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        filter(this),
        bias(this),
        offset_w(this),
        y(this, 0) {
    this->InputRegister("x");
    this->InputRegister("filter");
    this->InputRegister("bias");
    this->InputRegister("offset_w");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(9);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 4U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());
    std::set<ge::DataType> support_dtypes_of_sym_T2;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T2 = {DT_FLOAT, DT_FLOAT16, DT_BF16, DT_HIFLOAT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(input_dtypes[2]) != support_dtypes_of_sym_T2.end());
    std::set<ge::DataType> support_dtypes_of_sym_T3;
    if (npu_arch == "2201") {
      support_dtypes_of_sym_T3 = {DT_INT8};
    } else if (npu_arch == "3510") {
      support_dtypes_of_sym_T3 = {DT_INT8};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T3 = {DT_INT8};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T3.find(input_dtypes[3]) != support_dtypes_of_sym_T3.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[2]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[2] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 4U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[2]);
    return SUCCESS;
  };

  inline Conv2DOffsetBias &operator=(const Conv2DOffsetBias &) = delete;
  inline Conv2DOffsetBias(Conv2DOffsetBias &&) = delete;
  inline Conv2DOffsetBias(const Conv2DOffsetBias &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscConv2DOffsetBiasIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        filter(this),
        bias(this),
        offset_w(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v1.cpp:931
namespace af {
namespace ascir_op {
struct Split : public Operator {
  using af::Operator::DynamicOutputRegister;
  static constexpr const char *Type = "Split";
  af::AscNodeAttr &attr;
  struct AscSplitIrAttrDef : public af::AscIrAttrDefBase {
    ~AscSplitIrAttrDef() override = default;
    graphStatus GetIndex(int64_t &index) const {
      auto attr_value = attr_store_.GetAnyValue("index");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(index);
    }
    graphStatus SetIndex(int64_t index) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("index");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(index);
    }
    graphStatus GetGid(int64_t &gid) const {
      auto attr_value = attr_store_.GetAnyValue("gid");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(gid);
    }
    graphStatus SetGid(int64_t gid) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("gid");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(gid);
    }
  };
  AscSplitIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  std::vector<af::AscOpOutput> y;
  inline Split(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscSplitIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscSplitIrAttrDef &>(*(attr.ir_attr))),
        x(this) {
    this->InputRegister("x");
    this->DynamicOutputRegister("y", 0U, true);
    this->attr.api.compute_type = static_cast<af::ComputeType>(11);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,   DT_INT32,  DT_UINT8,   DT_INT16,     DT_UINT16,
                                 DT_UINT32, DT_INT64,   DT_UINT64, DT_DOUBLE, DT_BOOL,    DT_COMPLEX64, DT_COMPLEX128,
                                 DT_QINT8,  DT_QINT16,  DT_QINT32, DT_QUINT8, DT_QUINT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,   DT_INT32,  DT_UINT8,   DT_INT16,     DT_UINT16,
                                 DT_UINT32, DT_INT64,   DT_UINT64, DT_DOUBLE, DT_BOOL,    DT_COMPLEX64, DT_COMPLEX128,
                                 DT_QINT8,  DT_QINT16,  DT_QINT32, DT_QUINT8, DT_QUINT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Split &operator=(const Split &) = delete;
  inline Split(Split &&) = delete;
  inline Split(const Split &other)
      : af::Operator(other), attr(other.attr), ir_attr(dynamic_cast<AscSplitIrAttrDef &>(*(attr.ir_attr))), x(this) {
    InstanceOutputy(other.y.size());
  }
  void InstanceOutputy(uint32_t num) {
    this->DynamicOutputRegister("y", num);
    for (size_t i = 0U; i < num; i++) {
      this->y.push_back(af::AscOpOutput(this, i));
    }
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:55
namespace af {
namespace ascir_op {
struct Square : public Operator {
  static constexpr const char *Type = "Square";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Square(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT32, DT_UINT8,  DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT32, DT_UINT8,  DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Square &operator=(const Square &) = delete;
  inline Square(Square &&) = delete;
  inline Square(const Square &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:68
namespace af {
namespace ascir_op {
struct RoundToInt : public Operator {
  static constexpr const char *Type = "RoundToInt";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline RoundToInt(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    std::map<ge::DataType, std::set<ge::DataType>> results;
    if (npu_arch == "3510") {
      results = {{DT_FLOAT, {DT_INT32, DT_INT16, DT_INT64}},
                 {DT_FLOAT16, {DT_INT8, DT_INT32, DT_UINT8, DT_INT16}},
                 {DT_BF16, {DT_INT32}}};
    } else if (npu_arch == "5102") {
      results = {{DT_FLOAT, {DT_INT32, DT_INT16, DT_INT64}},
                 {DT_FLOAT16, {DT_INT8, DT_INT32, DT_UINT8, DT_INT16}},
                 {DT_BF16, {DT_INT32}}};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }

    auto iter = results.find(input_dtypes[0]);
    GE_WARN_ASSERT(iter != results.end());
    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GE_WARN_ASSERT(iter->second.size() == 1U);
      expect_output_dtypes.push_back(*(iter->second.begin()));
      return ge::SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(iter->second.find(expect_output_dtypes[0]) != iter->second.end());

    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 输入输出存在关联, 无法进行推导
    (void)input_dtypes;
    (void)expect_output_dtypes;
    GELOGW("Node type %s is not supported to infernocheck for dtype.", Type);
    return af::FAILED;
  };

  inline RoundToInt &operator=(const RoundToInt &) = delete;
  inline RoundToInt(RoundToInt &&) = delete;
  inline RoundToInt(const RoundToInt &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:80
namespace af {
namespace ascir_op {
struct TruncToInt : public Operator {
  static constexpr const char *Type = "TruncToInt";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline TruncToInt(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    std::map<ge::DataType, std::set<ge::DataType>> results;
    if (npu_arch == "3510") {
      results = {{DT_FLOAT, {DT_INT32, DT_INT16, DT_INT64}},
                 {DT_FLOAT16, {DT_INT8, DT_INT32, DT_UINT8, DT_INT16}},
                 {DT_BF16, {DT_INT32}}};
    } else if (npu_arch == "5102") {
      results = {{DT_FLOAT, {DT_INT32, DT_INT16, DT_INT64}},
                 {DT_FLOAT16, {DT_INT8, DT_INT32, DT_UINT8, DT_INT16}},
                 {DT_BF16, {DT_INT32}}};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }

    auto iter = results.find(input_dtypes[0]);
    GE_WARN_ASSERT(iter != results.end());
    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      GE_WARN_ASSERT(iter->second.size() == 1U);
      expect_output_dtypes.push_back(*(iter->second.begin()));
      return ge::SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(iter->second.find(expect_output_dtypes[0]) != iter->second.end());

    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 输入输出存在关联, 无法进行推导
    (void)input_dtypes;
    (void)expect_output_dtypes;
    GELOGW("Node type %s is not supported to infernocheck for dtype.", Type);
    return af::FAILED;
  };

  inline TruncToInt &operator=(const TruncToInt &) = delete;
  inline TruncToInt(TruncToInt &&) = delete;
  inline TruncToInt(const TruncToInt &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:88
namespace af {
namespace ascir_op {
struct Xor : public Operator {
  static constexpr const char *Type = "Xor";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Xor(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_UINT8, DT_INT16, DT_UINT16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_UINT8, DT_INT16, DT_UINT16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Xor &operator=(const Xor &) = delete;
  inline Xor(Xor &&) = delete;
  inline Xor(const Xor &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:97
namespace af {
namespace ascir_op {
struct Trunc : public Operator {
  static constexpr const char *Type = "Trunc";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Trunc(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Trunc &operator=(const Trunc &) = delete;
  inline Trunc(Trunc &&) = delete;
  inline Trunc(const Trunc &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:105
namespace af {
namespace ascir_op {
struct Tan : public Operator {
  static constexpr const char *Type = "Tan";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Tan(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Tan &operator=(const Tan &) = delete;
  inline Tan(Tan &&) = delete;
  inline Tan(const Tan &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:113
namespace af {
namespace ascir_op {
struct TruncDiv : public Operator {
  static constexpr const char *Type = "TruncDiv";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline TruncDiv(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline TruncDiv &operator=(const TruncDiv &) = delete;
  inline TruncDiv(TruncDiv &&) = delete;
  inline TruncDiv(const TruncDiv &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:122
namespace af {
namespace ascir_op {
struct Sinh : public Operator {
  static constexpr const char *Type = "Sinh";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Sinh(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Sinh &operator=(const Sinh &) = delete;
  inline Sinh(Sinh &&) = delete;
  inline Sinh(const Sinh &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:139
namespace af {
namespace ascir_op {
struct ModifiedBesselI0 : public Operator {
  static constexpr const char *Type = "ModifiedBesselI0";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline ModifiedBesselI0(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline ModifiedBesselI0 &operator=(const ModifiedBesselI0 &) = delete;
  inline ModifiedBesselI0(ModifiedBesselI0 &&) = delete;
  inline ModifiedBesselI0(const ModifiedBesselI0 &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:147
namespace af {
namespace ascir_op {
struct ModifiedBesselI1 : public Operator {
  static constexpr const char *Type = "ModifiedBesselI1";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline ModifiedBesselI1(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline ModifiedBesselI1 &operator=(const ModifiedBesselI1 &) = delete;
  inline ModifiedBesselI1(ModifiedBesselI1 &&) = delete;
  inline ModifiedBesselI1(const ModifiedBesselI1 &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:155
namespace af {
namespace ascir_op {
struct ModifiedBesselK0 : public Operator {
  static constexpr const char *Type = "ModifiedBesselK0";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline ModifiedBesselK0(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline ModifiedBesselK0 &operator=(const ModifiedBesselK0 &) = delete;
  inline ModifiedBesselK0(ModifiedBesselK0 &&) = delete;
  inline ModifiedBesselK0(const ModifiedBesselK0 &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:163
namespace af {
namespace ascir_op {
struct ModifiedBesselK1 : public Operator {
  static constexpr const char *Type = "ModifiedBesselK1";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline ModifiedBesselK1(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline ModifiedBesselK1 &operator=(const ModifiedBesselK1 &) = delete;
  inline ModifiedBesselK1(ModifiedBesselK1 &&) = delete;
  inline ModifiedBesselK1(const ModifiedBesselK1 &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:171
namespace af {
namespace ascir_op {
struct LaguerrePolynomialL : public Operator {
  static constexpr const char *Type = "LaguerrePolynomialL";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpInput<1> n;
  af::AscOpOutput y;
  inline LaguerrePolynomialL(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), n(this), y(this, 0) {
    this->InputRegister("x");
    this->InputRegister("n");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());
    std::set<ge::DataType> support_dtypes_of_sym_U;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_U = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_U = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_U.find(input_dtypes[1]) != support_dtypes_of_sym_U.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline LaguerrePolynomialL &operator=(const LaguerrePolynomialL &) = delete;
  inline LaguerrePolynomialL(LaguerrePolynomialL &&) = delete;
  inline LaguerrePolynomialL(const LaguerrePolynomialL &other)
      : af::Operator(other), attr(other.attr), x(this), n(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:182
namespace af {
namespace ascir_op {
struct LegendrePolynomialP : public Operator {
  static constexpr const char *Type = "LegendrePolynomialP";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpInput<1> n;
  af::AscOpOutput y;
  inline LegendrePolynomialP(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), n(this), y(this, 0) {
    this->InputRegister("x");
    this->InputRegister("n");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());
    std::set<ge::DataType> support_dtypes_of_sym_U;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_U = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_U = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_U.find(input_dtypes[1]) != support_dtypes_of_sym_U.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline LegendrePolynomialP &operator=(const LegendrePolynomialP &) = delete;
  inline LegendrePolynomialP(LegendrePolynomialP &&) = delete;
  inline LegendrePolynomialP(const LegendrePolynomialP &other)
      : af::Operator(other), attr(other.attr), x(this), n(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:193
namespace af {
namespace ascir_op {
struct AiryAi : public Operator {
  static constexpr const char *Type = "AiryAi";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline AiryAi(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline AiryAi &operator=(const AiryAi &) = delete;
  inline AiryAi(AiryAi &&) = delete;
  inline AiryAi(const AiryAi &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:201
namespace af {
namespace ascir_op {
struct Erfinv : public Operator {
  static constexpr const char *Type = "Erfinv";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Erfinv(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Erfinv &operator=(const Erfinv &) = delete;
  inline Erfinv(Erfinv &&) = delete;
  inline Erfinv(const Erfinv &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:256
namespace af {
namespace ascir_op {
struct Nddma : public Operator {
  static constexpr const char *Type = "Nddma";
  af::AscNodeAttr &attr;
  struct AscNddmaIrAttrDef : public af::AscIrAttrDefBase {
    ~AscNddmaIrAttrDef() override = default;
    graphStatus GetOffset(Expression &offset) const {
      auto attr_value = attr_store_.GetAnyValue("offset");
      GE_WARN_ASSERT(attr_value != nullptr);
      return attr_value->GetValue(offset);
    }
    graphStatus SetOffset(Expression offset) {
      auto attr_value = attr_store_.GetOrCreateAnyValue("offset");
      GE_ASSERT_NOTNULL(attr_value);
      return attr_value->SetValue(offset);
    }
  };
  AscNddmaIrAttrDef &ir_attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Nddma(const char *name)
      : af::Operator(name, Type),
        attr(*af::AscNodeAttr::Create<AscNddmaIrAttrDef>(*this)),
        ir_attr(dynamic_cast<AscNddmaIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(0);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                 DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Nddma &operator=(const Nddma &) = delete;
  inline Nddma(Nddma &&) = delete;
  inline Nddma(const Nddma &other)
      : af::Operator(other),
        attr(other.attr),
        ir_attr(dynamic_cast<AscNddmaIrAttrDef &>(*(attr.ir_attr))),
        x(this),
        y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:287
namespace af {
namespace ascir_op {
struct Round : public Operator {
  static constexpr const char *Type = "Round";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Round(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Round &operator=(const Round &) = delete;
  inline Round(Round &&) = delete;
  inline Round(const Round &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:330
namespace af {
namespace ascir_op {
struct Exp2 : public Operator {
  static constexpr const char *Type = "Exp2";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Exp2(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Exp2 &operator=(const Exp2 &) = delete;
  inline Exp2(Exp2 &&) = delete;
  inline Exp2(const Exp2 &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:338
namespace af {
namespace ascir_op {
struct Floor : public Operator {
  static constexpr const char *Type = "Floor";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Floor(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Floor &operator=(const Floor &) = delete;
  inline Floor(Floor &&) = delete;
  inline Floor(const Floor &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:346
namespace af {
namespace ascir_op {
struct Fma : public Operator {
  static constexpr const char *Type = "Fma";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpInput<2> x3;
  af::AscOpOutput y;
  inline Fma(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), x3(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->InputRegister("x3");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    GE_WARN_ASSERT(input_dtypes[1] == input_dtypes[2]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_UINT8, DT_INT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_UINT8, DT_INT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 3U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    GE_WARN_ASSERT(input_dtypes[1] == input_dtypes[2]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Fma &operator=(const Fma &) = delete;
  inline Fma(Fma &&) = delete;
  inline Fma(const Fma &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), x3(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:360
namespace af {
namespace ascir_op {
struct Expm : public Operator {
  static constexpr const char *Type = "Expm";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Expm(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Expm &operator=(const Expm &) = delete;
  inline Expm(Expm &&) = delete;
  inline Expm(const Expm &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:368
namespace af {
namespace ascir_op {
struct Log2 : public Operator {
  static constexpr const char *Type = "Log2";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Log2(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Log2 &operator=(const Log2 &) = delete;
  inline Log2(Log2 &&) = delete;
  inline Log2(const Log2 &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:380
namespace af {
namespace ascir_op {
struct LShift : public Operator {
  static constexpr const char *Type = "LShift";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline LShift(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    std::map<std::vector<ge::DataType>, std::set<ge::DataType>> results;
    if (npu_arch == "3510") {
      results = {{{DT_INT8, DT_INT8}, {}},   {{DT_INT32, DT_INT32}, {}},  {{DT_UINT8, DT_INT8}, {}},
                 {{DT_INT16, DT_INT16}, {}}, {{DT_UINT16, DT_INT16}, {}}, {{DT_UINT32, DT_INT32}, {}},
                 {{DT_INT64, DT_INT64}, {}}, {{DT_UINT64, DT_INT64}, {}}};
    } else if (npu_arch == "5102") {
      results = {{{DT_INT8, DT_INT8}, {}},   {{DT_INT32, DT_INT32}, {}},  {{DT_UINT8, DT_INT8}, {}},
                 {{DT_INT16, DT_INT16}, {}}, {{DT_UINT16, DT_INT16}, {}}, {{DT_UINT32, DT_INT32}, {}},
                 {{DT_INT64, DT_INT64}, {}}, {{DT_UINT64, DT_INT64}, {}}};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }

    auto iter = results.find(std::vector<ge::DataType>{input_dtypes[0], input_dtypes[1]});
    GE_WARN_ASSERT(iter != results.end());
    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return ge::SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);

    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 输入输出存在关联, 无法进行推导
    (void)input_dtypes;
    (void)expect_output_dtypes;
    GELOGW("Node type %s is not supported to infernocheck for dtype.", Type);
    return af::FAILED;
  };

  inline LShift &operator=(const LShift &) = delete;
  inline LShift(LShift &&) = delete;
  inline LShift(const LShift &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:389
namespace af {
namespace ascir_op {
struct Mod : public Operator {
  static constexpr const char *Type = "Mod";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Mod(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_UINT8, DT_INT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_INT8, DT_UINT8, DT_INT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Mod &operator=(const Mod &) = delete;
  inline Mod(Mod &&) = delete;
  inline Mod(const Mod &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:649
namespace af {
namespace ascir_op {
struct BitwiseNot : public Operator {
  static constexpr const char *Type = "BitwiseNot";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline BitwiseNot(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline BitwiseNot &operator=(const BitwiseNot &) = delete;
  inline BitwiseNot(BitwiseNot &&) = delete;
  inline BitwiseNot(const BitwiseNot &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:658
namespace af {
namespace ascir_op {
struct BitwiseOr : public Operator {
  static constexpr const char *Type = "BitwiseOr";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline BitwiseOr(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline BitwiseOr &operator=(const BitwiseOr &) = delete;
  inline BitwiseOr(BitwiseOr &&) = delete;
  inline BitwiseOr(const BitwiseOr &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:668
namespace af {
namespace ascir_op {
struct BitwiseXor : public Operator {
  static constexpr const char *Type = "BitwiseXor";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline BitwiseXor(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_INT8, DT_INT32, DT_UINT8, DT_INT16, DT_UINT16, DT_UINT32, DT_INT64, DT_UINT64};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline BitwiseXor &operator=(const BitwiseXor &) = delete;
  inline BitwiseXor(BitwiseXor &&) = delete;
  inline BitwiseXor(const BitwiseXor &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:678
namespace af {
namespace ascir_op {
struct Ceil : public Operator {
  static constexpr const char *Type = "Ceil";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Ceil(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Ceil &operator=(const Ceil &) = delete;
  inline Ceil(Ceil &&) = delete;
  inline Ceil(const Ceil &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:686
namespace af {
namespace ascir_op {
struct Cos : public Operator {
  static constexpr const char *Type = "Cos";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Cos(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Cos &operator=(const Cos &) = delete;
  inline Cos(Cos &&) = delete;
  inline Cos(const Cos &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:694
namespace af {
namespace ascir_op {
struct Acos : public Operator {
  static constexpr const char *Type = "Acos";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Acos(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Acos &operator=(const Acos &) = delete;
  inline Acos(Acos &&) = delete;
  inline Acos(const Acos &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:702
namespace af {
namespace ascir_op {
struct Cosh : public Operator {
  static constexpr const char *Type = "Cosh";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Cosh(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Cosh &operator=(const Cosh &) = delete;
  inline Cosh(Cosh &&) = delete;
  inline Cosh(const Cosh &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:710
namespace af {
namespace ascir_op {
struct Digamma : public Operator {
  static constexpr const char *Type = "Digamma";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Digamma(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Digamma &operator=(const Digamma &) = delete;
  inline Digamma(Digamma &&) = delete;
  inline Digamma(const Digamma &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:718
namespace af {
namespace ascir_op {
struct Erfc : public Operator {
  static constexpr const char *Type = "Erfc";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Erfc(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Erfc &operator=(const Erfc &) = delete;
  inline Erfc(Erfc &&) = delete;
  inline Erfc(const Erfc &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:726
namespace af {
namespace ascir_op {
struct Erfcx : public Operator {
  static constexpr const char *Type = "Erfcx";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Erfcx(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Erfcx &operator=(const Erfcx &) = delete;
  inline Erfcx(Erfcx &&) = delete;
  inline Erfcx(const Erfcx &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:734
namespace af {
namespace ascir_op {
struct Atan2 : public Operator {
  static constexpr const char *Type = "Atan2";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Atan2(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Atan2 &operator=(const Atan2 &) = delete;
  inline Atan2(Atan2 &&) = delete;
  inline Atan2(const Atan2 &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:743
namespace af {
namespace ascir_op {
struct CopySign : public Operator {
  static constexpr const char *Type = "CopySign";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline CopySign(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline CopySign &operator=(const CopySign &) = delete;
  inline CopySign(CopySign &&) = delete;
  inline CopySign(const CopySign &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:752
namespace af {
namespace ascir_op {
struct Ceil2Int : public Operator {
  static constexpr const char *Type = "Ceil2Int";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Ceil2Int(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_INT32);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_INT32};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(DT_INT32);
    return SUCCESS;
  };

  inline Ceil2Int &operator=(const Ceil2Int &) = delete;
  inline Ceil2Int(Ceil2Int &&) = delete;
  inline Ceil2Int(const Ceil2Int &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:781
namespace af {
namespace ascir_op {
struct FloorToInt : public Operator {
  static constexpr const char *Type = "FloorToInt";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline FloorToInt(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_INT32);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_INT32};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(DT_INT32);
    return SUCCESS;
  };

  inline FloorToInt &operator=(const FloorToInt &) = delete;
  inline FloorToInt(FloorToInt &&) = delete;
  inline FloorToInt(const FloorToInt &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:789
namespace af {
namespace ascir_op {
struct Fmod : public Operator {
  static constexpr const char *Type = "Fmod";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Fmod(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Fmod &operator=(const Fmod &) = delete;
  inline Fmod(Fmod &&) = delete;
  inline Fmod(const Fmod &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:798
namespace af {
namespace ascir_op {
struct Hypot : public Operator {
  static constexpr const char *Type = "Hypot";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline Hypot(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Hypot &operator=(const Hypot &) = delete;
  inline Hypot(Hypot &&) = delete;
  inline Hypot(const Hypot &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:807
namespace af {
namespace ascir_op {
struct Lgamma : public Operator {
  static constexpr const char *Type = "Lgamma";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Lgamma(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Lgamma &operator=(const Lgamma &) = delete;
  inline Lgamma(Lgamma &&) = delete;
  inline Lgamma(const Lgamma &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:815
namespace af {
namespace ascir_op {
struct Log10 : public Operator {
  static constexpr const char *Type = "Log10";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Log10(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Log10 &operator=(const Log10 &) = delete;
  inline Log10(Log10 &&) = delete;
  inline Log10(const Log10 &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:823
namespace af {
namespace ascir_op {
struct LogicalXor : public Operator {
  static constexpr const char *Type = "LogicalXor";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline LogicalXor(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);
    std::set<ge::DataType> support_dtypes_of_sym_T1;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T1 = {DT_FLOAT,  DT_FLOAT16, DT_INT8,  DT_INT32,  DT_UINT8, DT_INT16,
                                  DT_UINT16, DT_UINT32,  DT_INT64, DT_UINT64, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T1.find(input_dtypes[0]) != support_dtypes_of_sym_T1.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(DT_UINT8);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    static std::set<ge::DataType> support_dtypes_of_sym_T2 = {DT_UINT8};
    GE_WARN_ASSERT(support_dtypes_of_sym_T2.find(expect_output_dtypes[0]) != support_dtypes_of_sym_T2.end());
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致
    GE_WARN_ASSERT(input_dtypes[0] == input_dtypes[1]);

    expect_output_dtypes.push_back(DT_UINT8);
    return SUCCESS;
  };

  inline LogicalXor &operator=(const LogicalXor &) = delete;
  inline LogicalXor(LogicalXor &&) = delete;
  inline LogicalXor(const LogicalXor &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:834
namespace af {
namespace ascir_op {
struct Log1p : public Operator {
  static constexpr const char *Type = "Log1p";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Log1p(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Log1p &operator=(const Log1p &) = delete;
  inline Log1p(Log1p &&) = delete;
  inline Log1p(const Log1p &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:982
namespace af {
namespace ascir_op {
struct Sin : public Operator {
  static constexpr const char *Type = "Sin";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Sin(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Sin &operator=(const Sin &) = delete;
  inline Sin(Sin &&) = delete;
  inline Sin(const Sin &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:990
namespace af {
namespace ascir_op {
struct Acosh : public Operator {
  static constexpr const char *Type = "Acosh";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Acosh(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Acosh &operator=(const Acosh &) = delete;
  inline Acosh(Acosh &&) = delete;
  inline Acosh(const Acosh &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:998
namespace af {
namespace ascir_op {
struct Asin : public Operator {
  static constexpr const char *Type = "Asin";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Asin(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Asin &operator=(const Asin &) = delete;
  inline Asin(Asin &&) = delete;
  inline Asin(const Asin &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:1006
namespace af {
namespace ascir_op {
struct Asinh : public Operator {
  static constexpr const char *Type = "Asinh";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Asinh(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Asinh &operator=(const Asinh &) = delete;
  inline Asinh(Asinh &&) = delete;
  inline Asinh(const Asinh &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:1014
namespace af {
namespace ascir_op {
struct Atan : public Operator {
  static constexpr const char *Type = "Atan";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Atan(const char *name) : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Atan &operator=(const Atan &) = delete;
  inline Atan(Atan &&) = delete;
  inline Atan(const Atan &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:1022
namespace af {
namespace ascir_op {
struct Atanh : public Operator {
  static constexpr const char *Type = "Atanh";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x;
  af::AscOpOutput y;
  inline Atanh(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x(this), y(this, 0) {
    this->InputRegister("x");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    // 校验同sym的输入的dtype是否在注册范围内并且一致
    std::set<ge::DataType> support_dtypes_of_sym_T;
    if (npu_arch == "3510") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else if (npu_arch == "5102") {
      support_dtypes_of_sym_T = {DT_FLOAT, DT_FLOAT16, DT_BF16};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }
    GE_WARN_ASSERT(support_dtypes_of_sym_T.find(input_dtypes[0]) != support_dtypes_of_sym_T.end());

    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);
    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 1U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty());

    // 校验同sym的输入的dtype是否一致

    expect_output_dtypes.push_back(input_dtypes[0]);
    return SUCCESS;
  };

  inline Atanh &operator=(const Atanh &) = delete;
  inline Atanh(Atanh &&) = delete;
  inline Atanh(const Atanh &other) : af::Operator(other), attr(other.attr), x(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

// Defined at ascir_builtin_ops_v2.cpp:1034
namespace af {
namespace ascir_op {
struct RShift : public Operator {
  static constexpr const char *Type = "RShift";
  af::AscNodeAttr &attr;
  af::AscOpInput<0> x1;
  af::AscOpInput<1> x2;
  af::AscOpOutput y;
  inline RShift(const char *name)
      : af::Operator(name, Type), attr(*af::AscNodeAttr::Create(*this)), x1(this), x2(this), y(this, 0) {
    this->InputRegister("x1");
    this->InputRegister("x2");
    this->OutputRegister("y");
    y.TryInitTensorAttr();
    this->attr.api.compute_type = static_cast<af::ComputeType>(3);
  }

  inline static Status InferDataType(const std::vector<DataType> &input_dtypes,
                                     std::vector<DataType> &expect_output_dtypes,
                                     [[maybe_unused]] const std::string &npu_arch) {
    // 校验入参容器的元素个数是否合法
    GE_ASSERT_EQ(input_dtypes.size(), 2U);
    GE_ASSERT_TRUE(expect_output_dtypes.empty() || expect_output_dtypes.size() == 1U);

    std::map<std::vector<ge::DataType>, std::set<ge::DataType>> results;
    if (npu_arch == "3510") {
      results = {{{DT_INT8, DT_INT8}, {}},   {{DT_INT32, DT_INT32}, {}},  {{DT_UINT8, DT_INT8}, {}},
                 {{DT_INT16, DT_INT16}, {}}, {{DT_UINT16, DT_INT16}, {}}, {{DT_UINT32, DT_INT32}, {}},
                 {{DT_INT64, DT_INT64}, {}}, {{DT_UINT64, DT_INT64}, {}}};
    } else if (npu_arch == "5102") {
      results = {{{DT_INT8, DT_INT8}, {}},   {{DT_INT32, DT_INT32}, {}},  {{DT_UINT8, DT_INT8}, {}},
                 {{DT_INT16, DT_INT16}, {}}, {{DT_UINT16, DT_INT16}, {}}, {{DT_UINT32, DT_INT32}, {}},
                 {{DT_INT64, DT_INT64}, {}}, {{DT_UINT64, DT_INT64}, {}}};
    } else {
      GELOGE(ge::FAILED, "Unknown npu arch: %s", npu_arch.c_str());
      return af::FAILED;
    }

    auto iter = results.find(std::vector<ge::DataType>{input_dtypes[0], input_dtypes[1]});
    GE_WARN_ASSERT(iter != results.end());
    // 输出外部不指定的时候，生成推导的代码
    if (expect_output_dtypes.empty()) {
      expect_output_dtypes.push_back(input_dtypes[0]);
      return ge::SUCCESS;
    }
    // 输出外部指定，生成校验的代码
    GE_WARN_ASSERT(input_dtypes[0] == expect_output_dtypes[0]);

    return SUCCESS;
  };
  inline static Status InferDataTypeWithNoCheck(const std::vector<DataType> &input_dtypes,
                                                std::vector<DataType> &expect_output_dtypes,
                                                [[maybe_unused]] const std::string &npu_arch = "") {
    // 输入输出存在关联, 无法进行推导
    (void)input_dtypes;
    (void)expect_output_dtypes;
    GELOGW("Node type %s is not supported to infernocheck for dtype.", Type);
    return af::FAILED;
  };

  inline RShift &operator=(const RShift &) = delete;
  inline RShift(RShift &&) = delete;
  inline RShift(const RShift &other) : af::Operator(other), attr(other.attr), x1(this), x2(this), y(this, 0) {
    y.TryInitTensorAttr();
  }
};
}  // namespace ascir_op
}  // namespace af

namespace af {
namespace ascir {
namespace cg {
inline af::AscOpOutput Data(const char *name, af::AscGraph &graph, const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Data>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Data(const char *name, af::AscGraph &graph, ge::DataType dt,
                            const std::vector<af::AxisId> &axis_ids, const std::vector<af::Expression> &repeats,
                            const std::vector<af::Expression> &strides, const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Data>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  *op.y.axis = axis_ids;
  *op.y.repeats = repeats;
  *op.y.strides = strides;

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ContiguousData(const char *name, af::AscGraph &graph, ge::DataType dt,
                                      const std::vector<af::Axis> &axes, const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Data>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  op.y.SetContiguousView(axes);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Data(const char *name, af::AscGraph &graph) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Data>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Data(const char *name, af::AscGraph &graph, ge::DataType dt,
                            const std::vector<af::AxisId> &axis_ids, const std::vector<af::Expression> &repeats,
                            const std::vector<af::Expression> &strides) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Data>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  *op.y.axis = axis_ids;
  *op.y.repeats = repeats;
  *op.y.strides = strides;

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ContiguousData(const char *name, af::AscGraph &graph, ge::DataType dt,
                                      const std::vector<af::Axis> &axes) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Data>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  op.y.SetContiguousView(axes);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Scalar(const char *name, af::AscGraph &graph, const std::string &value, const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Scalar>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetValue(value);
  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Scalar(const char *name, af::AscGraph &graph, ge::DataType dt,
                              const std::vector<af::AxisId> &axis_ids, const std::vector<af::Expression> &repeats,
                              const std::vector<af::Expression> &strides, const std::string &value,
                              const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Scalar>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetValue(value);
  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  *op.y.axis = axis_ids;
  *op.y.repeats = repeats;
  *op.y.strides = strides;

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ContiguousScalar(const char *name, af::AscGraph &graph, ge::DataType dt,
                                        const std::vector<af::Axis> &axes, const std::string &value,
                                        const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Scalar>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetValue(value);
  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  op.y.SetContiguousView(axes);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Scalar(const char *name, af::AscGraph &graph) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Scalar>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Scalar(const char *name, af::AscGraph &graph, ge::DataType dt,
                              const std::vector<af::AxisId> &axis_ids, const std::vector<af::Expression> &repeats,
                              const std::vector<af::Expression> &strides) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Scalar>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  *op.y.axis = axis_ids;
  *op.y.repeats = repeats;
  *op.y.strides = strides;

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ContiguousScalar(const char *name, af::AscGraph &graph, ge::DataType dt,
                                        const std::vector<af::Axis> &axes) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Scalar>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  op.y.SetContiguousView(axes);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ScalarData(const char *name, af::AscGraph &graph, const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ScalarData>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ScalarData(const char *name, af::AscGraph &graph, ge::DataType dt,
                                  const std::vector<af::AxisId> &axis_ids, const std::vector<af::Expression> &repeats,
                                  const std::vector<af::Expression> &strides, const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ScalarData>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  *op.y.axis = axis_ids;
  *op.y.repeats = repeats;
  *op.y.strides = strides;

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ContiguousScalarData(const char *name, af::AscGraph &graph, ge::DataType dt,
                                            const std::vector<af::Axis> &axes, const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ScalarData>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  op.y.SetContiguousView(axes);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ScalarData(const char *name, af::AscGraph &graph) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ScalarData>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ScalarData(const char *name, af::AscGraph &graph, ge::DataType dt,
                                  const std::vector<af::AxisId> &axis_ids, const std::vector<af::Expression> &repeats,
                                  const std::vector<af::Expression> &strides) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ScalarData>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  *op.y.axis = axis_ids;
  *op.y.repeats = repeats;
  *op.y.strides = strides;

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ContiguousScalarData(const char *name, af::AscGraph &graph, ge::DataType dt,
                                            const std::vector<af::Axis> &axes) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ScalarData>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  op.y.SetContiguousView(axes);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput IndexExpr(const char *name, af::AscGraph &graph, const int64_t &expr) {
  const auto &op_ptr = std::make_shared<af::ascir_op::IndexExpr>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetExpr(expr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput IndexExpr(const char *name, af::AscGraph &graph, ge::DataType dt,
                                 const std::vector<af::AxisId> &axis_ids, const std::vector<af::Expression> &repeats,
                                 const std::vector<af::Expression> &strides, const int64_t &expr) {
  const auto &op_ptr = std::make_shared<af::ascir_op::IndexExpr>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetExpr(expr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  *op.y.axis = axis_ids;
  *op.y.repeats = repeats;
  *op.y.strides = strides;

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ContiguousIndexExpr(const char *name, af::AscGraph &graph, ge::DataType dt,
                                           const std::vector<af::Axis> &axes, const int64_t &expr) {
  const auto &op_ptr = std::make_shared<af::ascir_op::IndexExpr>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.ir_attr.SetExpr(expr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  op.y.SetContiguousView(axes);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput IndexExpr(const char *name, af::AscGraph &graph) {
  const auto &op_ptr = std::make_shared<af::ascir_op::IndexExpr>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput IndexExpr(const char *name, af::AscGraph &graph, ge::DataType dt,
                                 const std::vector<af::AxisId> &axis_ids, const std::vector<af::Expression> &repeats,
                                 const std::vector<af::Expression> &strides) {
  const auto &op_ptr = std::make_shared<af::ascir_op::IndexExpr>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  *op.y.axis = axis_ids;
  *op.y.repeats = repeats;
  *op.y.strides = strides;

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ContiguousIndexExpr(const char *name, af::AscGraph &graph, ge::DataType dt,
                                           const std::vector<af::Axis> &axes) {
  const auto &op_ptr = std::make_shared<af::ascir_op::IndexExpr>(name, graph);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.dtype = dt;
  op.y.SetContiguousView(axes);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Output(const char *name, const af::AscOpOutput &x_in, const int64_t &index) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Output>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.ir_attr.SetIndex(index);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Output(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Output>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Workspace(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Workspace>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Load(const char *name, const af::AscOpOutput &x_in, const Expression &offset) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Load>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.ir_attr.SetOffset(offset);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Load(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Load>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Store(const char *name, const af::AscOpOutput &x_in, const Expression &offset) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Store>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.ir_attr.SetOffset(offset);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline void Store(const char *name, const af::AscOpOutput &ub_in, af::AscOpOutput &gm_output,
                  const Expression &offset) {
  auto store_out = Store(name, ub_in, offset);
  auto &gm_producer = const_cast<af::Operator &>(gm_output.GetOwnerOp());
  auto &store_op = const_cast<af::Operator &>(store_out.GetOwnerOp());
  gm_producer.SetInput(0U, store_op, 0U);
  AddEdgeForNode(store_op, 0U, gm_producer, 0U);
  auto *gm_producer_attr = CodeGenUtils::GetOwnerOpAscAttr(gm_producer);
  gm_producer_attr->sched.exec_order = CodeGenUtils::GenNextExecId(store_op);
}
inline af::AscOpOutput Store(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Store>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline void Store(const char *name, const af::AscOpOutput &ub_in, af::AscOpOutput &gm_output) {
  auto store_out = Store(name, ub_in);
  auto &gm_producer = const_cast<af::Operator &>(gm_output.GetOwnerOp());
  auto &store_op = const_cast<af::Operator &>(store_out.GetOwnerOp());
  gm_producer.SetInput(0U, store_op, 0U);
  AddEdgeForNode(store_op, 0U, gm_producer, 0U);
  auto *gm_producer_attr = CodeGenUtils::GetOwnerOpAscAttr(gm_producer);
  gm_producer_attr->sched.exec_order = CodeGenUtils::GenNextExecId(store_op);
}
inline af::AscOpOutput Broadcast(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Broadcast>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput RemovePad(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::RemovePad>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Pad(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Pad>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Nop(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Nop>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Cast(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Cast>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Abs(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Abs>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Exp(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Exp>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Ln(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Ln>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ArgMax(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ArgMax>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline std::tuple<af::AscOpOutput, af::AscOpOutput> ArgMaxMultiRPhase1(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ArgMaxMultiRPhase1>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.value.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  op.index.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.value));
  THROW(PadOutputViewToSched(op.index));
  *op.value.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.value.axis, op.attr.sched.loop_axis);
  *op.index.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.index.axis, op.attr.sched.loop_axis);

  return std::make_tuple(op.value, op.index);
}
inline af::AscOpOutput ArgMaxMultiRPhase2(const char *name, const af::AscOpOutput &value_in,
                                          const af::AscOpOutput &index_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ArgMaxMultiRPhase2>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.value = value_in;
  op.index = index_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Sqrt(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Sqrt>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Rsqrt(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Rsqrt>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Reciprocal(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Reciprocal>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Erf(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Erf>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Sign(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Sign>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Tanh(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Tanh>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Isnan(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Isnan>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput IsFinite(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::IsFinite>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput IsInf(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::IsInf>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Relu(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Relu>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Neg(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Neg>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LogicalNot(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LogicalNot>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Max(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Max>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Sum(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Sum>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Min(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Min>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Mean(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Mean>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Prod(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Prod>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Sigmoid(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Sigmoid>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Any(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Any>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput All(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::All>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Add(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Add>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Sub(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Sub>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Div(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Div>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Mul(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Mul>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Minimum(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Minimum>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Maximum(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Maximum>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput TrueDiv(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::TrueDiv>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Remainder(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Remainder>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LogicalOr(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LogicalOr>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LogicalAnd(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LogicalAnd>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Pow(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Pow>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ClipByValue(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                   const af::AscOpOutput &x3_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ClipByValue>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.x3 = x3_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Ge(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Ge>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Eq(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Eq>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Ne(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Ne>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Gt(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Gt>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Le(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Le>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Lt(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Lt>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Concat(const char *name) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Concat>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Select(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                              const af::AscOpOutput &x3_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Select>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.x3 = x3_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Where(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                             const af::AscOpOutput &x3_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Where>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.x3 = x3_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MaskedFill(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &mask_in,
                                  const af::AscOpOutput &value_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MaskedFill>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.mask = mask_in;
  op.value = value_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Ub2ub(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Ub2ub>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LeakyRelu(const char *name, const af::AscOpOutput &x_in, const float &negative_slope) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LeakyRelu>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.ir_attr.SetNegative_slope(negative_slope);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LeakyRelu(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LeakyRelu>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BitwiseAnd(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BitwiseAnd>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Gather(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                              const int64_t &axis, const bool &negative_index_support) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Gather>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.ir_attr.SetAxis(axis);
  op.ir_attr.SetNegative_index_support(negative_index_support);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Gather(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Gather>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Transpose(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Transpose>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline std::tuple<af::AscOpOutput, af::AscOpOutput, af::AscOpOutput> FlashSoftmax(const char *name,
                                                                                  const af::AscOpOutput &x1_in,
                                                                                  const af::AscOpOutput &x2_in,
                                                                                  const af::AscOpOutput &x3_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::FlashSoftmax>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.x3 = x3_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y1.dtype = static_cast<ge::DataType>(x1_in.dtype);
  op.y2.dtype = static_cast<ge::DataType>(x1_in.dtype);
  op.y3.dtype = static_cast<ge::DataType>(x1_in.dtype);

  op.y1.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  op.y2.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  op.y3.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y1));
  THROW(PadOutputViewToSched(op.y2));
  THROW(PadOutputViewToSched(op.y3));
  *op.y1.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y1.axis, op.attr.sched.loop_axis);
  *op.y2.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y2.axis, op.attr.sched.loop_axis);
  *op.y3.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y3.axis, op.attr.sched.loop_axis);

  return std::make_tuple(op.y1, op.y2, op.y3);
}
inline af::AscOpOutput FloorDiv(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::FloorDiv>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Gelu(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Gelu>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Axpy(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                            const float &alpha) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Axpy>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.ir_attr.SetAlpha(alpha);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Axpy(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Axpy>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MatMul(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                              const int64_t &offset_x, const int64_t &transpose_x1, const int64_t &transpose_x2,
                              const int64_t &has_relu, const int64_t &enable_hf32) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MatMul>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetTranspose_x1(transpose_x1);
  op.ir_attr.SetTranspose_x2(transpose_x2);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetEnable_hf32(enable_hf32);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MatMul(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MatMul>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MatMulBias(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                  const af::AscOpOutput &bias_in, const int64_t &offset_x, const int64_t &transpose_x1,
                                  const int64_t &transpose_x2, const int64_t &has_relu, const int64_t &enable_hf32) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MatMulBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.bias = bias_in;

  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetTranspose_x1(transpose_x1);
  op.ir_attr.SetTranspose_x2(transpose_x2);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetEnable_hf32(enable_hf32);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MatMulBias(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                  const af::AscOpOutput &bias_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MatMulBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.bias = bias_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MatMulOffset(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                    const af::AscOpOutput &offset_w_in, const int64_t &offset_x,
                                    const int64_t &transpose_x1, const int64_t &transpose_x2, const int64_t &has_relu,
                                    const int64_t &enable_hf32) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MatMulOffset>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.offset_w = offset_w_in;

  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetTranspose_x1(transpose_x1);
  op.ir_attr.SetTranspose_x2(transpose_x2);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetEnable_hf32(enable_hf32);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MatMulOffset(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                    const af::AscOpOutput &offset_w_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MatMulOffset>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.offset_w = offset_w_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MatMulOffsetBias(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                        const af::AscOpOutput &bias_in, const af::AscOpOutput &offset_w_in,
                                        const int64_t &offset_x, const int64_t &transpose_x1,
                                        const int64_t &transpose_x2, const int64_t &has_relu,
                                        const int64_t &enable_hf32) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MatMulOffsetBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.bias = bias_in;
  op.offset_w = offset_w_in;

  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetTranspose_x1(transpose_x1);
  op.ir_attr.SetTranspose_x2(transpose_x2);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetEnable_hf32(enable_hf32);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput MatMulOffsetBias(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                        const af::AscOpOutput &bias_in, const af::AscOpOutput &offset_w_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::MatMulOffsetBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.bias = bias_in;
  op.offset_w = offset_w_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BatchMatMul(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                   const int64_t &offset_x, const int64_t &has_relu, const int64_t &enable_hf32,
                                   const int64_t &adj_x1, const int64_t &adj_x2) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BatchMatMul>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetEnable_hf32(enable_hf32);
  op.ir_attr.SetAdj_x1(adj_x1);
  op.ir_attr.SetAdj_x2(adj_x2);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BatchMatMul(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BatchMatMul>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BatchMatMulBias(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                       const af::AscOpOutput &bias_in, const int64_t &offset_x, const int64_t &has_relu,
                                       const int64_t &enable_hf32, const int64_t &adj_x1, const int64_t &adj_x2) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BatchMatMulBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.bias = bias_in;

  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetEnable_hf32(enable_hf32);
  op.ir_attr.SetAdj_x1(adj_x1);
  op.ir_attr.SetAdj_x2(adj_x2);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BatchMatMulBias(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                       const af::AscOpOutput &bias_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BatchMatMulBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.bias = bias_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BatchMatMulOffset(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                         const af::AscOpOutput &offset_w_in, const int64_t &offset_x,
                                         const int64_t &has_relu, const int64_t &enable_hf32, const int64_t &adj_x1,
                                         const int64_t &adj_x2) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BatchMatMulOffset>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.offset_w = offset_w_in;

  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetEnable_hf32(enable_hf32);
  op.ir_attr.SetAdj_x1(adj_x1);
  op.ir_attr.SetAdj_x2(adj_x2);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BatchMatMulOffset(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                                         const af::AscOpOutput &offset_w_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BatchMatMulOffset>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.offset_w = offset_w_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BatchMatMulOffsetBias(const char *name, const af::AscOpOutput &x1_in,
                                             const af::AscOpOutput &x2_in, const af::AscOpOutput &bias_in,
                                             const af::AscOpOutput &offset_w_in, const int64_t &offset_x,
                                             const int64_t &has_relu, const int64_t &enable_hf32, const int64_t &adj_x1,
                                             const int64_t &adj_x2) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BatchMatMulOffsetBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.bias = bias_in;
  op.offset_w = offset_w_in;

  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetEnable_hf32(enable_hf32);
  op.ir_attr.SetAdj_x1(adj_x1);
  op.ir_attr.SetAdj_x2(adj_x2);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BatchMatMulOffsetBias(const char *name, const af::AscOpOutput &x1_in,
                                             const af::AscOpOutput &x2_in, const af::AscOpOutput &bias_in,
                                             const af::AscOpOutput &offset_w_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BatchMatMulOffsetBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.bias = bias_in;
  op.offset_w = offset_w_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Conv2D(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &filter_in,
                              const std::vector<int64_t> &strides, const std::vector<int64_t> &pads,
                              const std::vector<int64_t> &dilations, const int64_t &groups, const int64_t &has_relu,
                              const std::string &pad_mode, const std::string &data_format, const int64_t &offset_x,
                              const bool &enable_hf32) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Conv2D>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.filter = filter_in;

  op.ir_attr.SetStrides(strides);
  op.ir_attr.SetPads(pads);
  op.ir_attr.SetDilations(dilations);
  op.ir_attr.SetGroups(groups);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetPad_mode(pad_mode);
  op.ir_attr.SetData_format(data_format);
  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetEnable_hf32(enable_hf32);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Conv2D(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &filter_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Conv2D>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.filter = filter_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Conv2DBias(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &filter_in,
                                  const af::AscOpOutput &bias_in, const std::vector<int64_t> &strides,
                                  const std::vector<int64_t> &pads, const std::vector<int64_t> &dilations,
                                  const int64_t &groups, const int64_t &has_relu, const std::string &pad_mode,
                                  const std::string &data_format, const int64_t &offset_x, const bool &enable_hf32) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Conv2DBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.filter = filter_in;
  op.bias = bias_in;

  op.ir_attr.SetStrides(strides);
  op.ir_attr.SetPads(pads);
  op.ir_attr.SetDilations(dilations);
  op.ir_attr.SetGroups(groups);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetPad_mode(pad_mode);
  op.ir_attr.SetData_format(data_format);
  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetEnable_hf32(enable_hf32);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Conv2DBias(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &filter_in,
                                  const af::AscOpOutput &bias_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Conv2DBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.filter = filter_in;
  op.bias = bias_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Conv2DOffset(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &filter_in,
                                    const af::AscOpOutput &offset_w_in, const std::vector<int64_t> &strides,
                                    const std::vector<int64_t> &pads, const std::vector<int64_t> &dilations,
                                    const int64_t &groups, const int64_t &has_relu, const std::string &pad_mode,
                                    const std::string &data_format, const int64_t &offset_x, const bool &enable_hf32) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Conv2DOffset>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.filter = filter_in;
  op.offset_w = offset_w_in;

  op.ir_attr.SetStrides(strides);
  op.ir_attr.SetPads(pads);
  op.ir_attr.SetDilations(dilations);
  op.ir_attr.SetGroups(groups);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetPad_mode(pad_mode);
  op.ir_attr.SetData_format(data_format);
  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetEnable_hf32(enable_hf32);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Conv2DOffset(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &filter_in,
                                    const af::AscOpOutput &offset_w_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Conv2DOffset>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.filter = filter_in;
  op.offset_w = offset_w_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Conv2DOffsetBias(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &filter_in,
                                        const af::AscOpOutput &bias_in, const af::AscOpOutput &offset_w_in,
                                        const std::vector<int64_t> &strides, const std::vector<int64_t> &pads,
                                        const std::vector<int64_t> &dilations, const int64_t &groups,
                                        const int64_t &has_relu, const std::string &pad_mode,
                                        const std::string &data_format, const int64_t &offset_x,
                                        const bool &enable_hf32) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Conv2DOffsetBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.filter = filter_in;
  op.bias = bias_in;
  op.offset_w = offset_w_in;

  op.ir_attr.SetStrides(strides);
  op.ir_attr.SetPads(pads);
  op.ir_attr.SetDilations(dilations);
  op.ir_attr.SetGroups(groups);
  op.ir_attr.SetHas_relu(has_relu);
  op.ir_attr.SetPad_mode(pad_mode);
  op.ir_attr.SetData_format(data_format);
  op.ir_attr.SetOffset_x(offset_x);
  op.ir_attr.SetEnable_hf32(enable_hf32);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Conv2DOffsetBias(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &filter_in,
                                        const af::AscOpOutput &bias_in, const af::AscOpOutput &offset_w_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Conv2DOffsetBias>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.filter = filter_in;
  op.bias = bias_in;
  op.offset_w = offset_w_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Square(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Square>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput RoundToInt(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::RoundToInt>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput TruncToInt(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::TruncToInt>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Xor(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Xor>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Trunc(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Trunc>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Tan(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Tan>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput TruncDiv(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::TruncDiv>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Sinh(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Sinh>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ModifiedBesselI0(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ModifiedBesselI0>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ModifiedBesselI1(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ModifiedBesselI1>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ModifiedBesselK0(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ModifiedBesselK0>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput ModifiedBesselK1(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::ModifiedBesselK1>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LaguerrePolynomialL(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &n_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LaguerrePolynomialL>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.n = n_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LegendrePolynomialP(const char *name, const af::AscOpOutput &x_in, const af::AscOpOutput &n_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LegendrePolynomialP>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;
  op.n = n_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput AiryAi(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::AiryAi>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Erfinv(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Erfinv>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Nddma(const char *name, const af::AscOpOutput &x_in, const Expression &offset) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Nddma>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.ir_attr.SetOffset(offset);

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Nddma(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Nddma>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Round(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Round>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Exp2(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Exp2>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Floor(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Floor>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Fma(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in,
                           const af::AscOpOutput &x3_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Fma>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;
  op.x3 = x3_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Expm(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Expm>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Log2(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Log2>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LShift(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LShift>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Mod(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Mod>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BitwiseNot(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BitwiseNot>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BitwiseOr(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BitwiseOr>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput BitwiseXor(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::BitwiseXor>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Ceil(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Ceil>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Cos(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Cos>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Acos(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Acos>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Cosh(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Cosh>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Digamma(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Digamma>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Erfc(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Erfc>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Erfcx(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Erfcx>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Atan2(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Atan2>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput CopySign(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::CopySign>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Ceil2Int(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Ceil2Int>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput FloorToInt(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::FloorToInt>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Fmod(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Fmod>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Hypot(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Hypot>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Lgamma(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Lgamma>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Log10(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Log10>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput LogicalXor(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::LogicalXor>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Log1p(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Log1p>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Sin(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Sin>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Acosh(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Acosh>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Asin(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Asin>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Asinh(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Asinh>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Atan(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Atan>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput Atanh(const char *name, const af::AscOpOutput &x_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::Atanh>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x = x_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
inline af::AscOpOutput RShift(const char *name, const af::AscOpOutput &x1_in, const af::AscOpOutput &x2_in) {
  const auto &op_ptr = std::make_shared<af::ascir_op::RShift>(name);
  auto &op = *op_ptr;
  const auto &desc = OpDescUtils::GetOpDescFromOperator(op);
  desc->SetExtAttr(RELATED_OP, op_ptr);

  op.x1 = x1_in;
  op.x2 = x2_in;

  op.attr.sched.exec_order = CodeGenUtils::GenNextExecId(op);
  SET_SCHED_AXIS_IF_IN_CONTEXT(op);

  op.y.mem->tensor_id = CodeGenUtils::GenNextTensorId(op);
  THROW(PadOutputViewToSched(op.y));
  *op.y.vectorized_axis = AxisUtils::GetDefaultVectorizedAxis(*op.y.axis, op.attr.sched.loop_axis);

  return op.y;
}
}  // namespace cg
inline af::Status CommonInferDtype(const std::string &type, const std::vector<DataType> &input_dtypes,
                                   std::vector<DataType> &expect_output_dtypes, const std::string &npu_arch) {
  using func = af::Status (*)(const std::vector<DataType> &input_dtypes, std::vector<DataType> &expect_output_dtypes,
                              const std::string &npu_arch);
  static const std::unordered_map<std::string, func> func_table = {
      {"Data", ::af::ascir_op::Data::InferDataType},
      {"VectorFunc", ::af::ascir_op::VectorFunc::InferDataType},
      {"Scalar", ::af::ascir_op::Scalar::InferDataType},
      {"ScalarData", ::af::ascir_op::ScalarData::InferDataType},
      {"IndexExpr", ::af::ascir_op::IndexExpr::InferDataType},
      {"Output", ::af::ascir_op::Output::InferDataType},
      {"Workspace", ::af::ascir_op::Workspace::InferDataType},
      {"Load", ::af::ascir_op::Load::InferDataType},
      {"Store", ::af::ascir_op::Store::InferDataType},
      {"Broadcast", ::af::ascir_op::Broadcast::InferDataType},
      {"RemovePad", ::af::ascir_op::RemovePad::InferDataType},
      {"Pad", ::af::ascir_op::Pad::InferDataType},
      {"Nop", ::af::ascir_op::Nop::InferDataType},
      {"Cast", ::af::ascir_op::Cast::InferDataType},
      {"Abs", ::af::ascir_op::Abs::InferDataType},
      {"Exp", ::af::ascir_op::Exp::InferDataType},
      {"Ln", ::af::ascir_op::Ln::InferDataType},
      {"ArgMax", ::af::ascir_op::ArgMax::InferDataType},
      {"ArgMaxMultiRPhase1", ::af::ascir_op::ArgMaxMultiRPhase1::InferDataType},
      {"ArgMaxMultiRPhase2", ::af::ascir_op::ArgMaxMultiRPhase2::InferDataType},
      {"Sqrt", ::af::ascir_op::Sqrt::InferDataType},
      {"Rsqrt", ::af::ascir_op::Rsqrt::InferDataType},
      {"Reciprocal", ::af::ascir_op::Reciprocal::InferDataType},
      {"Erf", ::af::ascir_op::Erf::InferDataType},
      {"Sign", ::af::ascir_op::Sign::InferDataType},
      {"Tanh", ::af::ascir_op::Tanh::InferDataType},
      {"Isnan", ::af::ascir_op::Isnan::InferDataType},
      {"IsFinite", ::af::ascir_op::IsFinite::InferDataType},
      {"IsInf", ::af::ascir_op::IsInf::InferDataType},
      {"Relu", ::af::ascir_op::Relu::InferDataType},
      {"Neg", ::af::ascir_op::Neg::InferDataType},
      {"LogicalNot", ::af::ascir_op::LogicalNot::InferDataType},
      {"Max", ::af::ascir_op::Max::InferDataType},
      {"Sum", ::af::ascir_op::Sum::InferDataType},
      {"Min", ::af::ascir_op::Min::InferDataType},
      {"Mean", ::af::ascir_op::Mean::InferDataType},
      {"Prod", ::af::ascir_op::Prod::InferDataType},
      {"Sigmoid", ::af::ascir_op::Sigmoid::InferDataType},
      {"Any", ::af::ascir_op::Any::InferDataType},
      {"All", ::af::ascir_op::All::InferDataType},
      {"Add", ::af::ascir_op::Add::InferDataType},
      {"Sub", ::af::ascir_op::Sub::InferDataType},
      {"Div", ::af::ascir_op::Div::InferDataType},
      {"Mul", ::af::ascir_op::Mul::InferDataType},
      {"Minimum", ::af::ascir_op::Minimum::InferDataType},
      {"Maximum", ::af::ascir_op::Maximum::InferDataType},
      {"TrueDiv", ::af::ascir_op::TrueDiv::InferDataType},
      {"Remainder", ::af::ascir_op::Remainder::InferDataType},
      {"LogicalOr", ::af::ascir_op::LogicalOr::InferDataType},
      {"LogicalAnd", ::af::ascir_op::LogicalAnd::InferDataType},
      {"Pow", ::af::ascir_op::Pow::InferDataType},
      {"ClipByValue", ::af::ascir_op::ClipByValue::InferDataType},
      {"Ge", ::af::ascir_op::Ge::InferDataType},
      {"Eq", ::af::ascir_op::Eq::InferDataType},
      {"Ne", ::af::ascir_op::Ne::InferDataType},
      {"Gt", ::af::ascir_op::Gt::InferDataType},
      {"Le", ::af::ascir_op::Le::InferDataType},
      {"Lt", ::af::ascir_op::Lt::InferDataType},
      {"Concat", ::af::ascir_op::Concat::InferDataType},
      {"Select", ::af::ascir_op::Select::InferDataType},
      {"Where", ::af::ascir_op::Where::InferDataType},
      {"MaskedFill", ::af::ascir_op::MaskedFill::InferDataType},
      {"Ub2ub", ::af::ascir_op::Ub2ub::InferDataType},
      {"LeakyRelu", ::af::ascir_op::LeakyRelu::InferDataType},
      {"BitwiseAnd", ::af::ascir_op::BitwiseAnd::InferDataType},
      {"Gather", ::af::ascir_op::Gather::InferDataType},
      {"Transpose", ::af::ascir_op::Transpose::InferDataType},
      {"FlashSoftmax", ::af::ascir_op::FlashSoftmax::InferDataType},
      {"FloorDiv", ::af::ascir_op::FloorDiv::InferDataType},
      {"Gelu", ::af::ascir_op::Gelu::InferDataType},
      {"Axpy", ::af::ascir_op::Axpy::InferDataType},
      {"MatMul", ::af::ascir_op::MatMul::InferDataType},
      {"MatMulBias", ::af::ascir_op::MatMulBias::InferDataType},
      {"MatMulOffset", ::af::ascir_op::MatMulOffset::InferDataType},
      {"MatMulOffsetBias", ::af::ascir_op::MatMulOffsetBias::InferDataType},
      {"BatchMatMul", ::af::ascir_op::BatchMatMul::InferDataType},
      {"BatchMatMulBias", ::af::ascir_op::BatchMatMulBias::InferDataType},
      {"BatchMatMulOffset", ::af::ascir_op::BatchMatMulOffset::InferDataType},
      {"BatchMatMulOffsetBias", ::af::ascir_op::BatchMatMulOffsetBias::InferDataType},
      {"Conv2D", ::af::ascir_op::Conv2D::InferDataType},
      {"Conv2DBias", ::af::ascir_op::Conv2DBias::InferDataType},
      {"Conv2DOffset", ::af::ascir_op::Conv2DOffset::InferDataType},
      {"Conv2DOffsetBias", ::af::ascir_op::Conv2DOffsetBias::InferDataType},
      {"Split", ::af::ascir_op::Split::InferDataType},
      {"Square", ::af::ascir_op::Square::InferDataType},
      {"RoundToInt", ::af::ascir_op::RoundToInt::InferDataType},
      {"TruncToInt", ::af::ascir_op::TruncToInt::InferDataType},
      {"Xor", ::af::ascir_op::Xor::InferDataType},
      {"Trunc", ::af::ascir_op::Trunc::InferDataType},
      {"Tan", ::af::ascir_op::Tan::InferDataType},
      {"TruncDiv", ::af::ascir_op::TruncDiv::InferDataType},
      {"Sinh", ::af::ascir_op::Sinh::InferDataType},
      {"ModifiedBesselI0", ::af::ascir_op::ModifiedBesselI0::InferDataType},
      {"ModifiedBesselI1", ::af::ascir_op::ModifiedBesselI1::InferDataType},
      {"ModifiedBesselK0", ::af::ascir_op::ModifiedBesselK0::InferDataType},
      {"ModifiedBesselK1", ::af::ascir_op::ModifiedBesselK1::InferDataType},
      {"LaguerrePolynomialL", ::af::ascir_op::LaguerrePolynomialL::InferDataType},
      {"LegendrePolynomialP", ::af::ascir_op::LegendrePolynomialP::InferDataType},
      {"AiryAi", ::af::ascir_op::AiryAi::InferDataType},
      {"Erfinv", ::af::ascir_op::Erfinv::InferDataType},
      {"Nddma", ::af::ascir_op::Nddma::InferDataType},
      {"Round", ::af::ascir_op::Round::InferDataType},
      {"Exp2", ::af::ascir_op::Exp2::InferDataType},
      {"Floor", ::af::ascir_op::Floor::InferDataType},
      {"Fma", ::af::ascir_op::Fma::InferDataType},
      {"Expm", ::af::ascir_op::Expm::InferDataType},
      {"Log2", ::af::ascir_op::Log2::InferDataType},
      {"LShift", ::af::ascir_op::LShift::InferDataType},
      {"Mod", ::af::ascir_op::Mod::InferDataType},
      {"BitwiseNot", ::af::ascir_op::BitwiseNot::InferDataType},
      {"BitwiseOr", ::af::ascir_op::BitwiseOr::InferDataType},
      {"BitwiseXor", ::af::ascir_op::BitwiseXor::InferDataType},
      {"Ceil", ::af::ascir_op::Ceil::InferDataType},
      {"Cos", ::af::ascir_op::Cos::InferDataType},
      {"Acos", ::af::ascir_op::Acos::InferDataType},
      {"Cosh", ::af::ascir_op::Cosh::InferDataType},
      {"Digamma", ::af::ascir_op::Digamma::InferDataType},
      {"Erfc", ::af::ascir_op::Erfc::InferDataType},
      {"Erfcx", ::af::ascir_op::Erfcx::InferDataType},
      {"Atan2", ::af::ascir_op::Atan2::InferDataType},
      {"CopySign", ::af::ascir_op::CopySign::InferDataType},
      {"Ceil2Int", ::af::ascir_op::Ceil2Int::InferDataType},
      {"FloorToInt", ::af::ascir_op::FloorToInt::InferDataType},
      {"Fmod", ::af::ascir_op::Fmod::InferDataType},
      {"Hypot", ::af::ascir_op::Hypot::InferDataType},
      {"Lgamma", ::af::ascir_op::Lgamma::InferDataType},
      {"Log10", ::af::ascir_op::Log10::InferDataType},
      {"LogicalXor", ::af::ascir_op::LogicalXor::InferDataType},
      {"Log1p", ::af::ascir_op::Log1p::InferDataType},
      {"Sin", ::af::ascir_op::Sin::InferDataType},
      {"Acosh", ::af::ascir_op::Acosh::InferDataType},
      {"Asin", ::af::ascir_op::Asin::InferDataType},
      {"Asinh", ::af::ascir_op::Asinh::InferDataType},
      {"Atan", ::af::ascir_op::Atan::InferDataType},
      {"Atanh", ::af::ascir_op::Atanh::InferDataType},
      {"RShift", ::af::ascir_op::RShift::InferDataType},
  };
  const auto &iter = func_table.find(type);
  if (iter != func_table.end()) {
    return iter->second(input_dtypes, expect_output_dtypes, npu_arch);
  }
  GELOGW("Node type %s is not supported to infer for now!", type.c_str());
  return af::FAILED;
}
inline af::Status CommonInferDtypeWithNoCheck(const std::string &type, const std::vector<DataType> &input_dtypes,
                                              std::vector<DataType> &expect_output_dtypes,
                                              const std::string &npu_arch) {
  using func = af::Status (*)(const std::vector<DataType> &input_dtypes, std::vector<DataType> &expect_output_dtypes,
                              const std::string &npu_arch);
  static const std::unordered_map<std::string, func> func_table = {
      {"Data", ::af::ascir_op::Data::InferDataTypeWithNoCheck},
      {"VectorFunc", ::af::ascir_op::VectorFunc::InferDataTypeWithNoCheck},
      {"Scalar", ::af::ascir_op::Scalar::InferDataTypeWithNoCheck},
      {"ScalarData", ::af::ascir_op::ScalarData::InferDataTypeWithNoCheck},
      {"IndexExpr", ::af::ascir_op::IndexExpr::InferDataTypeWithNoCheck},
      {"Output", ::af::ascir_op::Output::InferDataTypeWithNoCheck},
      {"Workspace", ::af::ascir_op::Workspace::InferDataTypeWithNoCheck},
      {"Load", ::af::ascir_op::Load::InferDataTypeWithNoCheck},
      {"Store", ::af::ascir_op::Store::InferDataTypeWithNoCheck},
      {"Broadcast", ::af::ascir_op::Broadcast::InferDataTypeWithNoCheck},
      {"RemovePad", ::af::ascir_op::RemovePad::InferDataTypeWithNoCheck},
      {"Pad", ::af::ascir_op::Pad::InferDataTypeWithNoCheck},
      {"Nop", ::af::ascir_op::Nop::InferDataTypeWithNoCheck},
      {"Cast", ::af::ascir_op::Cast::InferDataTypeWithNoCheck},
      {"Abs", ::af::ascir_op::Abs::InferDataTypeWithNoCheck},
      {"Exp", ::af::ascir_op::Exp::InferDataTypeWithNoCheck},
      {"Ln", ::af::ascir_op::Ln::InferDataTypeWithNoCheck},
      {"ArgMax", ::af::ascir_op::ArgMax::InferDataTypeWithNoCheck},
      {"ArgMaxMultiRPhase1", ::af::ascir_op::ArgMaxMultiRPhase1::InferDataTypeWithNoCheck},
      {"ArgMaxMultiRPhase2", ::af::ascir_op::ArgMaxMultiRPhase2::InferDataTypeWithNoCheck},
      {"Sqrt", ::af::ascir_op::Sqrt::InferDataTypeWithNoCheck},
      {"Rsqrt", ::af::ascir_op::Rsqrt::InferDataTypeWithNoCheck},
      {"Reciprocal", ::af::ascir_op::Reciprocal::InferDataTypeWithNoCheck},
      {"Erf", ::af::ascir_op::Erf::InferDataTypeWithNoCheck},
      {"Sign", ::af::ascir_op::Sign::InferDataTypeWithNoCheck},
      {"Tanh", ::af::ascir_op::Tanh::InferDataTypeWithNoCheck},
      {"Isnan", ::af::ascir_op::Isnan::InferDataTypeWithNoCheck},
      {"IsFinite", ::af::ascir_op::IsFinite::InferDataTypeWithNoCheck},
      {"IsInf", ::af::ascir_op::IsInf::InferDataTypeWithNoCheck},
      {"Relu", ::af::ascir_op::Relu::InferDataTypeWithNoCheck},
      {"Neg", ::af::ascir_op::Neg::InferDataTypeWithNoCheck},
      {"LogicalNot", ::af::ascir_op::LogicalNot::InferDataTypeWithNoCheck},
      {"Max", ::af::ascir_op::Max::InferDataTypeWithNoCheck},
      {"Sum", ::af::ascir_op::Sum::InferDataTypeWithNoCheck},
      {"Min", ::af::ascir_op::Min::InferDataTypeWithNoCheck},
      {"Mean", ::af::ascir_op::Mean::InferDataTypeWithNoCheck},
      {"Prod", ::af::ascir_op::Prod::InferDataTypeWithNoCheck},
      {"Sigmoid", ::af::ascir_op::Sigmoid::InferDataTypeWithNoCheck},
      {"Any", ::af::ascir_op::Any::InferDataTypeWithNoCheck},
      {"All", ::af::ascir_op::All::InferDataTypeWithNoCheck},
      {"Add", ::af::ascir_op::Add::InferDataTypeWithNoCheck},
      {"Sub", ::af::ascir_op::Sub::InferDataTypeWithNoCheck},
      {"Div", ::af::ascir_op::Div::InferDataTypeWithNoCheck},
      {"Mul", ::af::ascir_op::Mul::InferDataTypeWithNoCheck},
      {"Minimum", ::af::ascir_op::Minimum::InferDataTypeWithNoCheck},
      {"Maximum", ::af::ascir_op::Maximum::InferDataTypeWithNoCheck},
      {"TrueDiv", ::af::ascir_op::TrueDiv::InferDataTypeWithNoCheck},
      {"Remainder", ::af::ascir_op::Remainder::InferDataTypeWithNoCheck},
      {"LogicalOr", ::af::ascir_op::LogicalOr::InferDataTypeWithNoCheck},
      {"LogicalAnd", ::af::ascir_op::LogicalAnd::InferDataTypeWithNoCheck},
      {"Pow", ::af::ascir_op::Pow::InferDataTypeWithNoCheck},
      {"ClipByValue", ::af::ascir_op::ClipByValue::InferDataTypeWithNoCheck},
      {"Ge", ::af::ascir_op::Ge::InferDataTypeWithNoCheck},
      {"Eq", ::af::ascir_op::Eq::InferDataTypeWithNoCheck},
      {"Ne", ::af::ascir_op::Ne::InferDataTypeWithNoCheck},
      {"Gt", ::af::ascir_op::Gt::InferDataTypeWithNoCheck},
      {"Le", ::af::ascir_op::Le::InferDataTypeWithNoCheck},
      {"Lt", ::af::ascir_op::Lt::InferDataTypeWithNoCheck},
      {"Concat", ::af::ascir_op::Concat::InferDataTypeWithNoCheck},
      {"Select", ::af::ascir_op::Select::InferDataTypeWithNoCheck},
      {"Where", ::af::ascir_op::Where::InferDataTypeWithNoCheck},
      {"MaskedFill", ::af::ascir_op::MaskedFill::InferDataTypeWithNoCheck},
      {"Ub2ub", ::af::ascir_op::Ub2ub::InferDataTypeWithNoCheck},
      {"LeakyRelu", ::af::ascir_op::LeakyRelu::InferDataTypeWithNoCheck},
      {"BitwiseAnd", ::af::ascir_op::BitwiseAnd::InferDataTypeWithNoCheck},
      {"Gather", ::af::ascir_op::Gather::InferDataTypeWithNoCheck},
      {"Transpose", ::af::ascir_op::Transpose::InferDataTypeWithNoCheck},
      {"FlashSoftmax", ::af::ascir_op::FlashSoftmax::InferDataTypeWithNoCheck},
      {"FloorDiv", ::af::ascir_op::FloorDiv::InferDataTypeWithNoCheck},
      {"Gelu", ::af::ascir_op::Gelu::InferDataTypeWithNoCheck},
      {"Axpy", ::af::ascir_op::Axpy::InferDataTypeWithNoCheck},
      {"MatMul", ::af::ascir_op::MatMul::InferDataTypeWithNoCheck},
      {"MatMulBias", ::af::ascir_op::MatMulBias::InferDataTypeWithNoCheck},
      {"MatMulOffset", ::af::ascir_op::MatMulOffset::InferDataTypeWithNoCheck},
      {"MatMulOffsetBias", ::af::ascir_op::MatMulOffsetBias::InferDataTypeWithNoCheck},
      {"BatchMatMul", ::af::ascir_op::BatchMatMul::InferDataTypeWithNoCheck},
      {"BatchMatMulBias", ::af::ascir_op::BatchMatMulBias::InferDataTypeWithNoCheck},
      {"BatchMatMulOffset", ::af::ascir_op::BatchMatMulOffset::InferDataTypeWithNoCheck},
      {"BatchMatMulOffsetBias", ::af::ascir_op::BatchMatMulOffsetBias::InferDataTypeWithNoCheck},
      {"Conv2D", ::af::ascir_op::Conv2D::InferDataTypeWithNoCheck},
      {"Conv2DBias", ::af::ascir_op::Conv2DBias::InferDataTypeWithNoCheck},
      {"Conv2DOffset", ::af::ascir_op::Conv2DOffset::InferDataTypeWithNoCheck},
      {"Conv2DOffsetBias", ::af::ascir_op::Conv2DOffsetBias::InferDataTypeWithNoCheck},
      {"Split", ::af::ascir_op::Split::InferDataTypeWithNoCheck},
      {"Square", ::af::ascir_op::Square::InferDataTypeWithNoCheck},
      {"RoundToInt", ::af::ascir_op::RoundToInt::InferDataTypeWithNoCheck},
      {"TruncToInt", ::af::ascir_op::TruncToInt::InferDataTypeWithNoCheck},
      {"Xor", ::af::ascir_op::Xor::InferDataTypeWithNoCheck},
      {"Trunc", ::af::ascir_op::Trunc::InferDataTypeWithNoCheck},
      {"Tan", ::af::ascir_op::Tan::InferDataTypeWithNoCheck},
      {"TruncDiv", ::af::ascir_op::TruncDiv::InferDataTypeWithNoCheck},
      {"Sinh", ::af::ascir_op::Sinh::InferDataTypeWithNoCheck},
      {"ModifiedBesselI0", ::af::ascir_op::ModifiedBesselI0::InferDataTypeWithNoCheck},
      {"ModifiedBesselI1", ::af::ascir_op::ModifiedBesselI1::InferDataTypeWithNoCheck},
      {"ModifiedBesselK0", ::af::ascir_op::ModifiedBesselK0::InferDataTypeWithNoCheck},
      {"ModifiedBesselK1", ::af::ascir_op::ModifiedBesselK1::InferDataTypeWithNoCheck},
      {"LaguerrePolynomialL", ::af::ascir_op::LaguerrePolynomialL::InferDataTypeWithNoCheck},
      {"LegendrePolynomialP", ::af::ascir_op::LegendrePolynomialP::InferDataTypeWithNoCheck},
      {"AiryAi", ::af::ascir_op::AiryAi::InferDataTypeWithNoCheck},
      {"Erfinv", ::af::ascir_op::Erfinv::InferDataTypeWithNoCheck},
      {"Nddma", ::af::ascir_op::Nddma::InferDataTypeWithNoCheck},
      {"Round", ::af::ascir_op::Round::InferDataTypeWithNoCheck},
      {"Exp2", ::af::ascir_op::Exp2::InferDataTypeWithNoCheck},
      {"Floor", ::af::ascir_op::Floor::InferDataTypeWithNoCheck},
      {"Fma", ::af::ascir_op::Fma::InferDataTypeWithNoCheck},
      {"Expm", ::af::ascir_op::Expm::InferDataTypeWithNoCheck},
      {"Log2", ::af::ascir_op::Log2::InferDataTypeWithNoCheck},
      {"LShift", ::af::ascir_op::LShift::InferDataTypeWithNoCheck},
      {"Mod", ::af::ascir_op::Mod::InferDataTypeWithNoCheck},
      {"BitwiseNot", ::af::ascir_op::BitwiseNot::InferDataTypeWithNoCheck},
      {"BitwiseOr", ::af::ascir_op::BitwiseOr::InferDataTypeWithNoCheck},
      {"BitwiseXor", ::af::ascir_op::BitwiseXor::InferDataTypeWithNoCheck},
      {"Ceil", ::af::ascir_op::Ceil::InferDataTypeWithNoCheck},
      {"Cos", ::af::ascir_op::Cos::InferDataTypeWithNoCheck},
      {"Acos", ::af::ascir_op::Acos::InferDataTypeWithNoCheck},
      {"Cosh", ::af::ascir_op::Cosh::InferDataTypeWithNoCheck},
      {"Digamma", ::af::ascir_op::Digamma::InferDataTypeWithNoCheck},
      {"Erfc", ::af::ascir_op::Erfc::InferDataTypeWithNoCheck},
      {"Erfcx", ::af::ascir_op::Erfcx::InferDataTypeWithNoCheck},
      {"Atan2", ::af::ascir_op::Atan2::InferDataTypeWithNoCheck},
      {"CopySign", ::af::ascir_op::CopySign::InferDataTypeWithNoCheck},
      {"Ceil2Int", ::af::ascir_op::Ceil2Int::InferDataTypeWithNoCheck},
      {"FloorToInt", ::af::ascir_op::FloorToInt::InferDataTypeWithNoCheck},
      {"Fmod", ::af::ascir_op::Fmod::InferDataTypeWithNoCheck},
      {"Hypot", ::af::ascir_op::Hypot::InferDataTypeWithNoCheck},
      {"Lgamma", ::af::ascir_op::Lgamma::InferDataTypeWithNoCheck},
      {"Log10", ::af::ascir_op::Log10::InferDataTypeWithNoCheck},
      {"LogicalXor", ::af::ascir_op::LogicalXor::InferDataTypeWithNoCheck},
      {"Log1p", ::af::ascir_op::Log1p::InferDataTypeWithNoCheck},
      {"Sin", ::af::ascir_op::Sin::InferDataTypeWithNoCheck},
      {"Acosh", ::af::ascir_op::Acosh::InferDataTypeWithNoCheck},
      {"Asin", ::af::ascir_op::Asin::InferDataTypeWithNoCheck},
      {"Asinh", ::af::ascir_op::Asinh::InferDataTypeWithNoCheck},
      {"Atan", ::af::ascir_op::Atan::InferDataTypeWithNoCheck},
      {"Atanh", ::af::ascir_op::Atanh::InferDataTypeWithNoCheck},
      {"RShift", ::af::ascir_op::RShift::InferDataTypeWithNoCheck},
  };
  const auto &iter = func_table.find(type);
  if (iter != func_table.end()) {
    return iter->second(input_dtypes, expect_output_dtypes, npu_arch);
  }
  GELOGW("Node type %s is not supported to infer for now!", type.c_str());
  return af::FAILED;
}
}  // namespace ascir
}  // namespace af

#endif  // ASCIR_OPS_ASCIR_OPS_H_
