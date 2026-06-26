/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "external/graph/named_io_node_builder.h"
#include "graph/op_desc.h"
#include "graph/normal_graph/op_desc_impl.h"
#include "graph/utils/node_adapter.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/operator_reg.h"

namespace ge {

// ===== 注册测试算子 =====

// 简单双输入算子
REG_OP(TestAddForIRBuilder)
    .INPUT(x1, TensorType({DT_FLOAT}))
    .INPUT(x2, TensorType({DT_FLOAT}))
    .OUTPUT(y, TensorType({DT_FLOAT}))
    .ATTR(axis, Int, 0)
    .OP_END_FACTORY_REG(TestAddForIRBuilder)

    // 带可选输入的算子
    REG_OP(TestConvForIRBuilder)
    .INPUT(x, TensorType({DT_FLOAT}))
    .INPUT(w, TensorType({DT_FLOAT}))
    .OPTIONAL_INPUT(bias, TensorType({DT_FLOAT}))
    .OUTPUT(y, TensorType({DT_FLOAT}))
    .REQUIRED_ATTR(strides, ListInt)
    .ATTR(pad_mode, Int, 0)
    .OP_END_FACTORY_REG(TestConvForIRBuilder)

    // 带动态输入/输出的算子
    REG_OP(TestDynamicForIRBuilder)
    .DYNAMIC_INPUT(x, TensorType({DT_FLOAT}))
    .DYNAMIC_OUTPUT(y, TensorType({DT_FLOAT}))
    .ATTR(N, Int, 1)
    .OP_END_FACTORY_REG(TestDynamicForIRBuilder)

    // Data 类型算子（RecoverIrDefinition 和 ValidateIrInstance 均跳过 Data/NetOutput）
    REG_OP(Data)
    .OUTPUT(y, TensorType({DT_FLOAT}))
    .ATTR(index, Int, 0)
    .OP_END_FACTORY_REG(Data)

        namespace {
  class OpDescChecker {
   public:
    explicit OpDescChecker(const GNode &node) {
      const auto node_ptr = NodeAdapter::GNode2Node(node);
      EXPECT_NE(node_ptr, nullptr);
      if (node_ptr != nullptr) {
        op_desc_ = node_ptr->GetOpDesc();
      }
      EXPECT_NE(op_desc_, nullptr);
    }

    OpDescChecker &CheckName(const std::string &name) {
      if (op_desc_ != nullptr) {
        EXPECT_EQ(op_desc_->GetName(), name);
      }
      return *this;
    }

    OpDescChecker &CheckType(const std::string &type) {
      if (op_desc_ != nullptr) {
        EXPECT_EQ(op_desc_->GetType(), type);
      }
      return *this;
    }

    OpDescChecker &CheckInputSize(const size_t valid_size, const size_t all_size) {
      if (op_desc_ != nullptr) {
        EXPECT_EQ(op_desc_->GetInputsSize(), valid_size);
        EXPECT_EQ(op_desc_->GetAllInputsSize(), all_size);
      }
      return *this;
    }

    OpDescChecker &CheckOutputSize(const size_t size) {
      if (op_desc_ != nullptr) {
        EXPECT_EQ(op_desc_->GetOutputsSize(), size);
        EXPECT_EQ(op_desc_->GetAllOutputsDescSize(), size);
      }
      return *this;
    }

    OpDescChecker &CheckInputNames(const std::vector<std::string> &names) {
      if (op_desc_ != nullptr) {
        std::vector<std::string> input_names;
        for (const auto &name : op_desc_->GetAllInputNames()) {
          input_names.emplace_back(name);
        }
        std::vector<std::string> sorted_names = names;
        std::sort(input_names.begin(), input_names.end());
        std::sort(sorted_names.begin(), sorted_names.end());
        EXPECT_EQ(input_names, sorted_names);
        EXPECT_EQ(op_desc_->GetRegisterInputName(), names);
      }
      return *this;
    }

    OpDescChecker &CheckIrInputs(const std::vector<std::pair<std::string, IrInputType>> &inputs) {
      if (op_desc_ != nullptr) {
        EXPECT_EQ(op_desc_->GetIrInputs(), inputs);
      }
      return *this;
    }

    OpDescChecker &CheckIrOutputs(const std::vector<std::pair<std::string, IrOutputType>> &outputs) {
      if (op_desc_ != nullptr) {
        EXPECT_EQ(op_desc_->GetIrOutputs(), outputs);
      }
      return *this;
    }

    OpDescChecker &CheckIrAttrs(const std::vector<std::string> &attrs) {
      if (op_desc_ != nullptr) {
        const auto &ir_attr_names = op_desc_->GetIrAttrNames();
        for (const auto &attr : attrs) {
          EXPECT_NE(std::find(ir_attr_names.begin(), ir_attr_names.end(), attr), ir_attr_names.end());
        }
      }
      return *this;
    }

    OpDescChecker &CheckAttr(const std::vector<std::string> &attrs) {
      if (op_desc_ != nullptr) {
        for (const auto &attr : attrs) {
          EXPECT_TRUE(op_desc_->HasAttr(attr));
        }
      }
      return *this;
    }

    OpDescChecker &CheckInputRawIrInfo(const std::map<size_t, std::pair<size_t, size_t>> &ir_ranges_golden) {
      if (op_desc_ != nullptr) {
        std::map<size_t, std::pair<size_t, size_t>> ir_ranges;
        EXPECT_EQ(OpDescUtils::GetIrInputRawDescRange(op_desc_, ir_ranges), GRAPH_SUCCESS);
        EXPECT_EQ(ir_ranges, ir_ranges_golden);
      }
      return *this;
    }

    OpDescChecker &CheckInputInstanceIrInfo(const std::map<size_t, std::pair<size_t, size_t>> &ir_ranges_golden) {
      if (op_desc_ != nullptr) {
        std::map<size_t, std::pair<size_t, size_t>> ir_ranges;
        EXPECT_EQ(OpDescUtils::GetIrInputInstanceDescRange(op_desc_, ir_ranges), GRAPH_SUCCESS);
        EXPECT_EQ(ir_ranges, ir_ranges_golden);
      }
      return *this;
    }

    OpDescChecker &CheckOutputIrInfo(const std::map<size_t, std::pair<size_t, size_t>> &ir_ranges_golden) {
      if (op_desc_ != nullptr) {
        std::map<size_t, std::pair<size_t, size_t>> ir_ranges;
        EXPECT_EQ(OpDescUtils::GetIrOutputDescRange(op_desc_, ir_ranges), GRAPH_SUCCESS);
        EXPECT_EQ(ir_ranges, ir_ranges_golden);
      }
      return *this;
    }

    OpDescChecker &CheckInputDesc(const size_t index, const DataType data_type, const Format format,
                                  const std::vector<int64_t> &shape) {
      if (op_desc_ != nullptr) {
        CheckTensorDesc(op_desc_->GetInputDesc(static_cast<uint32_t>(index)), data_type, format, shape);
      }
      return *this;
    }

    OpDescChecker &CheckOutputDesc(const size_t index, const DataType data_type, const Format format,
                                   const std::vector<int64_t> &shape) {
      if (op_desc_ != nullptr) {
        CheckTensorDesc(op_desc_->GetOutputDesc(static_cast<uint32_t>(index)), data_type, format, shape);
      }
      return *this;
    }

   private:
    void CheckTensorDesc(const GeTensorDesc &desc, const DataType data_type, const Format format,
                         const std::vector<int64_t> &shape) const {
      EXPECT_EQ(desc.GetDataType(), data_type);
      EXPECT_EQ(desc.GetFormat(), format);
      const auto desc_shape = desc.GetShape();
      EXPECT_EQ(desc_shape.GetDimNum(), shape.size());
      for (size_t i = 0U; i < shape.size(); ++i) {
        EXPECT_EQ(desc_shape.GetDim(i), shape[i]);
      }
    }

    OpDescPtr op_desc_;
  };

  void ExpectGraphNotPolluted(const Graph &graph) {
    const auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
    if (compute_graph != nullptr) {
      EXPECT_EQ(compute_graph->GetDirectNodesSize(), 0U);
    }
  }
}  // namespace

// ===== 测试类 =====

class NamedIoNodeBuilderTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// ===== 功能测试 =====

// 基本构建：Type + Name + Input + Output，验证 IR 恢复成功
TEST_F(NamedIoNodeBuilderTest, BuildBasic_RecoverIRSuccess) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  OpDescChecker(*node)
      .CheckName("add_node")
      .CheckType("TestAddForIRBuilder")
      .CheckInputSize(2U, 2U)
      .CheckOutputSize(1U)
      .CheckInputNames({"x1", "x2"})
      .CheckIrInputs({{"x1", kIrInputRequired}, {"x2", kIrInputRequired}})
      .CheckIrOutputs({{"y", kIrOutputRequired}})
      .CheckInputRawIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}})
      .CheckInputInstanceIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}})
      .CheckOutputIrInfo({{0U, {0U, 1U}}})
      .CheckIrAttrs({"axis"})
      .CheckAttr({"axis"});
}

// 构建成功时清空错误信息，避免调用方读到历史失败文案
TEST_F(NamedIoNodeBuilderTest, BuildBasic_SuccessClearsErrorMessage) {
  Graph graph("test_graph");
  AscendString error_msg("stale error");

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);
  EXPECT_EQ(error_msg.GetLength(), 0U);
  if (error_msg.GetString() != nullptr) {
    EXPECT_STREQ(error_msg.GetString(), "");
  }
}

// 验证 IR inputs 被正确恢复
TEST_F(NamedIoNodeBuilderTest, BuildBasic_IrInputsRecovered) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  OpDescChecker(*node)
      .CheckIrInputs({{"x1", kIrInputRequired}, {"x2", kIrInputRequired}})
      .CheckInputRawIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}})
      .CheckInputInstanceIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}});
}

// 验证 IR outputs 被正确恢复
TEST_F(NamedIoNodeBuilderTest, BuildBasic_IrOutputsRecovered) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  OpDescChecker(*node).CheckIrOutputs({{"y", kIrOutputRequired}}).CheckOutputIrInfo({{0U, {0U, 1U}}});
}

// 验证 IR attr names 被正确恢复
TEST_F(NamedIoNodeBuilderTest, BuildBasic_IrAttrNamesRecovered) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  OpDescChecker(*node).CheckIrAttrs({"axis"}).CheckAttr({"axis"});
}

// 验证默认属性被补全
TEST_F(NamedIoNodeBuilderTest, BuildBasic_DefaultAttrRecovered) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  OpDescChecker(*node).CheckAttr({"axis"}).CheckIrAttrs({"axis"});

  // axis 默认值为 0
  int64_t axis_val = -1;
  AscendString axis_name("axis");
  EXPECT_EQ(node->GetAttr(axis_name, axis_val), GRAPH_SUCCESS);
  EXPECT_EQ(axis_val, 0);
}

// 用户属性设置，验证不被 Recover 覆盖
TEST_F(NamedIoNodeBuilderTest, BuildWithAttr_UserAttrPreserved) {
  Graph graph("test_graph");
  AscendString error_msg;

  AttrValue axis_val;
  axis_val.SetAttrValue(static_cast<int64_t>(42));

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Attr("axis", axis_val)
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  OpDescChecker(*node).CheckAttr({"axis"}).CheckIrAttrs({"axis"});

  // 用户设置的值应保留
  int64_t result = -1;
  AscendString axis_name("axis");
  EXPECT_EQ(node->GetAttr(axis_name, result), GRAPH_SUCCESS);
  EXPECT_EQ(result, 42);
}

// 带可选输入的算子：只连接必选输入
TEST_F(NamedIoNodeBuilderTest, BuildWithOptionalInput_OnlyRequiredConnected) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestConvForIRBuilder")
                  .Name("conv_node")
                  .AddInput("x")
                  .AddInput("w")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  OpDescChecker(*node)
      .CheckName("conv_node")
      .CheckType("TestConvForIRBuilder")
      .CheckInputSize(2U, 2U)
      .CheckOutputSize(1U)
      .CheckInputNames({"x", "w"})
      .CheckIrInputs({{"x", kIrInputRequired}, {"w", kIrInputRequired}, {"bias", kIrInputOptional}})
      .CheckIrOutputs({{"y", kIrOutputRequired}})
      .CheckInputRawIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}, {2U, {2U, 0U}}})
      .CheckInputInstanceIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}, {2U, {2U, 0U}}})
      .CheckOutputIrInfo({{0U, {0U, 1U}}})
      .CheckIrAttrs({"strides", "pad_mode"})
      .CheckAttr({"pad_mode"});
}

// 带可选输入的算子：用户显式传入无效 TensorDesc 时，Builder 不改写该描述
TEST_F(NamedIoNodeBuilderTest, BuildWithOptionalInput_InvalidTensorDescPreserved) {
  Graph graph("test_graph");
  AscendString error_msg;

  TensorDesc invalid_bias_desc(Shape({1}), FORMAT_RESERVED, DT_UNDEFINED);

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestConvForIRBuilder")
                  .Name("conv_node")
                  .AddInput("x")
                  .AddInput("w")
                  .AddInput("bias", invalid_bias_desc)
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  OpDescChecker(*node)
      .CheckName("conv_node")
      .CheckType("TestConvForIRBuilder")
      .CheckInputSize(2U, 3U)
      .CheckOutputSize(1U)
      .CheckInputNames({"x", "w", "bias"})
      .CheckIrInputs({{"x", kIrInputRequired}, {"w", kIrInputRequired}, {"bias", kIrInputOptional}})
      .CheckInputRawIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}, {2U, {2U, 0U}}})
      .CheckInputInstanceIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}, {2U, {2U, 0U}}})
      .CheckOutputIrInfo({{0U, {0U, 1U}}})
      .CheckInputDesc(2U, DT_UNDEFINED, FORMAT_RESERVED, {1});
}

// 带可选输入的算子：用户用实例名添加可选输入，不需要区分 optional 类型
TEST_F(NamedIoNodeBuilderTest, BuildWithOptionalInput_DefaultTensorDescConnected) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestConvForIRBuilder")
                  .Name("conv_node")
                  .AddInput("x")
                  .AddInput("w")
                  .AddInput("bias")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  OpDescChecker(*node)
      .CheckInputSize(3U, 3U)
      .CheckOutputSize(1U)
      .CheckInputNames({"x", "w", "bias"})
      .CheckIrInputs({{"x", kIrInputRequired}, {"w", kIrInputRequired}, {"bias", kIrInputOptional}})
      .CheckInputRawIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}, {2U, {2U, 1U}}})
      .CheckInputInstanceIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}, {2U, {2U, 1U}}});
}

// 动态输入/输出
TEST_F(NamedIoNodeBuilderTest, BuildDynamicInputOutput) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestDynamicForIRBuilder")
                  .Name("dynamic_node")
                  .AddInput("x0")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y0")
                  .AddOutput("y1")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  OpDescChecker(*node)
      .CheckName("dynamic_node")
      .CheckType("TestDynamicForIRBuilder")
      .CheckInputSize(3U, 3U)
      .CheckOutputSize(2U)
      .CheckInputNames({"x0", "x1", "x2"})
      .CheckIrInputs({{"x", kIrInputDynamic}})
      .CheckIrOutputs({{"y", kIrOutputDynamic}})
      .CheckInputRawIrInfo({{0U, {0U, 3U}}})
      .CheckInputInstanceIrInfo({{0U, {0U, 3U}}})
      .CheckOutputIrInfo({{0U, {0U, 2U}}});
}

// 带描述的输入/输出
TEST_F(NamedIoNodeBuilderTest, BuildWithTensorDesc) {
  Graph graph("test_graph");
  AscendString error_msg;

  TensorDesc x_desc(Shape({3, 4}), FORMAT_ND, DT_FLOAT);
  TensorDesc y_desc(Shape({3, 4}), FORMAT_ND, DT_FLOAT);

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1", x_desc)
                  .AddInput("x2", x_desc)
                  .AddOutput("y", y_desc)
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  OpDescChecker(*node)
      .CheckInputSize(2U, 2U)
      .CheckOutputSize(1U)
      .CheckInputDesc(0U, DT_FLOAT, FORMAT_ND, {3, 4})
      .CheckInputDesc(1U, DT_FLOAT, FORMAT_ND, {3, 4})
      .CheckOutputDesc(0U, DT_FLOAT, FORMAT_ND, {3, 4});
}

// ===== 异常测试 =====

// Type 未设置
TEST_F(NamedIoNodeBuilderTest, BuildWithoutType_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph).Name("no_type_node").AddInput("x").AddOutput("y").Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("Type"), std::string::npos);
}

// 未注册的算子类型：Build 返回 nullptr
TEST_F(NamedIoNodeBuilderTest, BuildWithUnregisteredType_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node =
      NamedIoNodeBuilder(graph).Type("NonExistentOp").Name("bad_node").AddInput("x").AddOutput("y").Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not registered"), std::string::npos);
}

// 输入名称不在 IR 定义中：Build 返回 nullptr，且不向 Graph 添加节点
TEST_F(NamedIoNodeBuilderTest, BuildWithUnknownInputName_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("bad_input_node")
                  .AddInput("x1")
                  .AddInput("not_in_ir")
                  .AddOutput("y")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not compatible"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 缺少必选输入实例：Build 返回 nullptr，且不向 Graph 添加节点
TEST_F(NamedIoNodeBuilderTest, BuildWithMissingRequiredInput_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("missing_input_node")
                  .AddInput("x1")
                  .AddOutput("y")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("Missing required input"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 输入实例顺序需与 IR 定义一致
TEST_F(NamedIoNodeBuilderTest, BuildWithInputOutOfOrder_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestConvForIRBuilder")
                  .Name("out_of_order_node")
                  .AddInput("w")
                  .AddInput("x")
                  .AddOutput("y")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not compatible"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 可选输入最多添加一个实例，重复添加会破坏 IR 实例序列
TEST_F(NamedIoNodeBuilderTest, BuildWithOptionalInputRepeated_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestConvForIRBuilder")
                  .Name("repeated_optional_node")
                  .AddInput("x")
                  .AddInput("w")
                  .AddInput("bias")
                  .AddInput("bias")
                  .AddOutput("y")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not defined"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 动态输入实例名需从 ir_name0 开始连续
TEST_F(NamedIoNodeBuilderTest, BuildWithDynamicInputNotStartFromZero_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestDynamicForIRBuilder")
                  .Name("bad_dynamic_input_node")
                  .AddInput("x1")
                  .AddOutput("y0")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not defined"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 动态输入实例名需连续，不能跳过中间序号
TEST_F(NamedIoNodeBuilderTest, BuildWithDynamicInputIndexGap_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestDynamicForIRBuilder")
                  .Name("bad_dynamic_input_node")
                  .AddInput("x0")
                  .AddInput("x2")
                  .AddOutput("y0")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not defined"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 动态输出实例名同样需从 ir_name0 开始连续
TEST_F(NamedIoNodeBuilderTest, BuildWithDynamicOutputNotStartFromZero_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestDynamicForIRBuilder")
                  .Name("bad_dynamic_output_node")
                  .AddInput("x0")
                  .AddOutput("y1")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not defined"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 输出名称不在 IR 定义中：Build 返回 nullptr，且不向 Graph 添加节点
TEST_F(NamedIoNodeBuilderTest, BuildWithUnknownOutputName_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("bad_output_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("not_in_ir")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not compatible"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// Build 后再次调用 Build 应返回 nullptr
TEST_F(NamedIoNodeBuilderTest, BuildCalledTwice_ReturnsNullptrOnSecondCall) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto builder = std::make_unique<NamedIoNodeBuilder>(graph);
  auto node1 = builder->Type("TestAddForIRBuilder")
                   .Name("add_node")
                   .AddInput("x1")
                   .AddInput("x2")
                   .AddOutput("y")
                   .Build(error_msg);

  ASSERT_NE(node1, nullptr);

  // 第二次调用 Build 应失败
  auto node2 = builder->Build(error_msg);
  EXPECT_EQ(node2, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("already been called"), std::string::npos);
}

// ===== 与 torchair 状态一致性测试 =====

// 验证 Build 后的 OpDesc 状态与 torchair 反序列化一致
TEST_F(NamedIoNodeBuilderTest, OpDescStateMatchesTorchairPattern) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  OpDescChecker(*node)
      .CheckInputSize(2U, 2U)
      .CheckOutputSize(1U)
      .CheckInputNames({"x1", "x2"})
      .CheckIrInputs({{"x1", kIrInputRequired}, {"x2", kIrInputRequired}})
      .CheckIrOutputs({{"y", kIrOutputRequired}})
      .CheckInputRawIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}})
      .CheckInputInstanceIrInfo({{0U, {0U, 1U}}, {1U, {1U, 1U}}})
      .CheckOutputIrInfo({{0U, {0U, 1U}}})
      .CheckIrAttrs({"axis"});
}

// ===== Data/NetOutput 跳过校验测试 =====

// Data 类型节点：RecoverIrDefinition 跳过 IR 恢复，ValidateIrInstance 同步跳过校验，Build 成功
TEST_F(NamedIoNodeBuilderTest, BuildDataNode_SkipsIrValidation) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph).Type("Data").Name("data_node").AddOutput("y").Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  OpDescChecker(*node)
      .CheckName("data_node")
      .CheckType("Data")
      .CheckOutputSize(1U)
      .CheckOutputDesc(0U, DT_FLOAT, FORMAT_ND, {});

  // Data 节点被 RecoverIrDefinition 跳过，ir_inputs/ir_outputs 不恢复
  const auto node_ptr = NodeAdapter::GNode2Node(*node);
  ASSERT_NE(node_ptr, nullptr);
  const auto op_desc = node_ptr->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  EXPECT_TRUE(op_desc->GetIrInputs().empty());
  EXPECT_TRUE(op_desc->GetIrOutputs().empty());
}

// 缺少必选输出实例：Build 返回 nullptr
TEST_F(NamedIoNodeBuilderTest, BuildWithMissingRequiredOutput_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("missing_output_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("Missing required output"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 输出名称与 IR 不匹配：Build 返回 nullptr（覆盖 ValidateIrOutputInstances 中 name 不匹配分支）
TEST_F(NamedIoNodeBuilderTest, BuildWithIncompatibleOutputName_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("bad_output_name_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("wrong_name")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not compatible"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// 动态输出有额外未定义输出：覆盖 output_index < outputs_.size() 分支
TEST_F(NamedIoNodeBuilderTest, BuildWithExtraOutputNotInIr_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestDynamicForIRBuilder")
                  .Name("extra_output_node")
                  .AddInput("x0")
                  .AddOutput("y0")
                  .AddOutput("y1")
                  .AddOutput("extra_output")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("not defined"), std::string::npos);
  ExpectGraphNotPolluted(graph);
}

// NetOutput 类型节点：RecoverIrDefinition 和 ValidateIrInstance 均跳过，Build 成功
TEST_F(NamedIoNodeBuilderTest, BuildNetOutputNode_SkipsIrValidation) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph).Type("NetOutput").Name("netoutput_node").Build(error_msg);

  // NetOutput 需要注册，若未注册则跳过
  if (node == nullptr) {
    GTEST_SKIP() << "NetOutput not registered in this test environment";
  }

  const auto node_ptr = NodeAdapter::GNode2Node(*node);
  ASSERT_NE(node_ptr, nullptr);
  const auto op_desc = node_ptr->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  EXPECT_TRUE(op_desc->GetIrInputs().empty());
  EXPECT_TRUE(op_desc->GetIrOutputs().empty());
}

// ===== nullptr guard 分支测试 =====

// Type(nullptr)：不应设置 type_，Build 时应报 "Type must be set"
TEST_F(NamedIoNodeBuilderTest, BuildWithTypeNullptr_ReturnsNullptr) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph).Type(nullptr).Name("node").AddInput("x1").AddOutput("y").Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("Type"), std::string::npos);
}

// Name(nullptr)：不应设置 name_，但 Build 仍可成功（name_ 为空合法）
TEST_F(NamedIoNodeBuilderTest, BuildWithNameNullptr_StillSucceeds) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name(nullptr)
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);
}

// AddInput(nullptr)：不应添加输入，导致缺少必选输入
TEST_F(NamedIoNodeBuilderTest, BuildWithAddInputNullptr_MissingInput) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("node")
                  .AddInput(nullptr)
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  // nullptr input 被跳过，inputs_ 只有 x1, x2，构建应成功
  ASSERT_NE(node, nullptr);
}

// AddInput(nullptr, desc)：不应添加输入
TEST_F(NamedIoNodeBuilderTest, BuildWithAddInputDescNullptr_Skipped) {
  Graph graph("test_graph");
  AscendString error_msg;

  TensorDesc desc(Shape({3, 4}), FORMAT_ND, DT_FLOAT);

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("node")
                  .AddInput(nullptr, desc)
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);
}

// AddOutput(nullptr)：不应添加输出
TEST_F(NamedIoNodeBuilderTest, BuildWithAddOutputNullptr_MissingOutput) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput(nullptr)
                  .Build(error_msg);

  // nullptr output 被跳过，outputs_ 为空，缺少必选输出 y
  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
  EXPECT_NE(std::string(error_msg.GetString()).find("Missing required output"), std::string::npos);
}

// AddOutput(nullptr, desc)：不应添加输出
TEST_F(NamedIoNodeBuilderTest, BuildWithAddOutputDescNullptr_Skipped) {
  Graph graph("test_graph");
  AscendString error_msg;

  TensorDesc desc(Shape({3, 4}), FORMAT_ND, DT_FLOAT);

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput(nullptr, desc)
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
}

// Attr(nullptr, value)：不应添加属性
TEST_F(NamedIoNodeBuilderTest, BuildWithAttrNameNullptr_Skipped) {
  Graph graph("test_graph");
  AscendString error_msg;

  AttrValue val;
  val.SetAttrValue(static_cast<int64_t>(42));

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Attr(nullptr, val)
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);
  // Attr 未被设置，axis 应保留默认值 0
  int64_t axis_val = -1;
  AscendString axis_name("axis");
  EXPECT_EQ(node->GetAttr(axis_name, axis_val), GRAPH_SUCCESS);
  EXPECT_EQ(axis_val, 0);
}

// Attr(name, empty_value)：value 默认构造后 impl 不为 nullptr，但 MutableAnyValue 为空
// 验证不会崩溃
TEST_F(NamedIoNodeBuilderTest, BuildWithAttrDefaultValue_NoCrash) {
  Graph graph("test_graph");
  AscendString error_msg;

  AttrValue default_val;  // impl 非 nullptr（ComGraphMakeShared），但未设置任何值

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Attr("axis", default_val)
                  .Build(error_msg);

  ASSERT_EQ(node, nullptr);
}

// 带描述的 AddOutput(name, desc) 路径
TEST_F(NamedIoNodeBuilderTest, BuildWithOutputTensorDesc) {
  Graph graph("test_graph");
  AscendString error_msg;

  TensorDesc y_desc(Shape({3, 4}), FORMAT_ND, DT_FLOAT);

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y", y_desc)
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  OpDescChecker(*node).CheckOutputSize(1U).CheckOutputDesc(0U, DT_FLOAT, FORMAT_ND, {3, 4});
}

// 动态输入只添加一个实例
TEST_F(NamedIoNodeBuilderTest, BuildDynamicInputSingle) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestDynamicForIRBuilder")
                  .Name("dynamic_node")
                  .AddInput("x0")
                  .AddOutput("y0")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  OpDescChecker(*node)
      .CheckInputSize(1U, 1U)
      .CheckOutputSize(1U)
      .CheckIrInputs({{"x", kIrInputDynamic}})
      .CheckIrOutputs({{"y", kIrOutputDynamic}});
}

// 无名称构建（name_ 为空）
TEST_F(NamedIoNodeBuilderTest, BuildWithoutName_Succeeds) {
  Graph graph("test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("TestAddForIRBuilder")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);
}

}  // namespace ge
