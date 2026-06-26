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
#include <ge_running_env/ge_running_env_faker.h>
#include <ge_running_env/fake_op.h>
#include "api/gelib/gelib.h"
#include "external/graph/named_io_node_builder.h"
#include "graph/op_desc.h"
#include "graph/utils/node_adapter.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/operator_reg.h"

namespace ge {

// 注册 ST 测试算子
REG_OP(STTestAddForIRBuilder)
    .INPUT(x1, TensorType({DT_FLOAT}))
    .INPUT(x2, TensorType({DT_FLOAT}))
    .OUTPUT(y, TensorType({DT_FLOAT}))
    .ATTR(axis, Int, 0)
    .OP_END_FACTORY_REG(STTestAddForIRBuilder)

        REG_OP(STTestConvForIRBuilder)
    .INPUT(x, TensorType({DT_FLOAT}))
    .INPUT(w, TensorType({DT_FLOAT}))
    .OPTIONAL_INPUT(bias, TensorType({DT_FLOAT}))
    .OUTPUT(y, TensorType({DT_FLOAT}))
    .REQUIRED_ATTR(strides, ListInt)
    .ATTR(pad_mode, Int, 0)
    .OP_END_FACTORY_REG(STTestConvForIRBuilder)

        REG_OP(STTestDynamicForIRBuilder)
    .DYNAMIC_INPUT(x, TensorType({DT_FLOAT}))
    .DYNAMIC_OUTPUT(y, TensorType({DT_FLOAT}))
    .ATTR(N, Int, 1)
    .OP_END_FACTORY_REG(STTestDynamicForIRBuilder)

        class NamedIoNodeBuilderSTTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    std::map<std::string, std::string> options;
    EXPECT_EQ(ge::GELib::Initialize(options), ge::SUCCESS);
    ge::GELib::GetInstance()->OpsKernelManagerObj().ops_kernel_store_.clear();

    ge::GeRunningEnvFaker()
        .Reset()
        .Install(ge::FakeEngine("DNN_VM_GE_LOCAL").KernelInfoStore("DNN_VM_GE_LOCAL_OP_STORE"))
        .Install(ge::FakeEngine("AIcoreEngine").KernelInfoStore("AIcoreEngine"))
        .Install(ge::FakeOp(ge::DATA).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
        .Install(ge::FakeOp(ge::NETOUTPUT).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
        .Install(ge::FakeOp("Add").InfoStoreAndBuilder("AIcoreEngine"))
        .Install(ge::FakeOp("STTestAddForIRBuilder").InfoStoreAndBuilder("AIcoreEngine"))
        .Install(ge::FakeOp("STTestConvForIRBuilder").InfoStoreAndBuilder("AIcoreEngine"))
        .Install(ge::FakeOp("STTestDynamicForIRBuilder").InfoStoreAndBuilder("AIcoreEngine"));

    ge::EngineConfPtr conf1 = std::make_shared<ge::EngineConf>();
    conf1->id = "AIcoreEngine";
    conf1->scheduler_id = "scheduler";

    ge::EngineConfPtr conf2 = std::make_shared<ge::EngineConf>();
    conf2->id = "DNN_VM_GE_LOCAL";
    conf2->scheduler_id = "scheduler";
    conf2->skip_assign_stream = true;
    conf2->attach = true;

    ge::SchedulerConf scheduler_conf;
    scheduler_conf.cal_engines[conf1->id] = conf1;
    scheduler_conf.cal_engines[conf2->id] = conf2;

    auto instance_ptr = ge::GELib::GetInstance();
    instance_ptr->DNNEngineManagerObj().schedulers_["scheduler"] = scheduler_conf;
  }

  static void TearDownTestSuite() {
    ge::GeRunningEnvFaker().Reset();
    ge::GELib::GetInstance()->Finalize();
  }
};

// ST: 基本构建 - 验证 IR 恢复在完整运行环境中正常工作
TEST_F(NamedIoNodeBuilderSTTest, BuildBasicNodeInRunningEnv) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestAddForIRBuilder")
                  .Name("add_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  const auto node_ptr = NodeAdapter::GNode2Node(*node);
  ASSERT_NE(node_ptr, nullptr);
  const auto op_desc = node_ptr->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  EXPECT_EQ(op_desc->GetType(), "STTestAddForIRBuilder");
  EXPECT_EQ(op_desc->GetName(), "add_node");
  EXPECT_EQ(op_desc->GetInputsSize(), 2U);
  EXPECT_EQ(op_desc->GetOutputsSize(), 1U);
}

// ST: 带属性的构建 - 验证用户属性不被覆盖
TEST_F(NamedIoNodeBuilderSTTest, BuildWithUserAttrPreserved) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  AttrValue axis_val;
  axis_val.SetAttrValue(static_cast<int64_t>(42));

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestAddForIRBuilder")
                  .Name("add_with_attr")
                  .AddInput("x1")
                  .AddInput("x2")
                  .AddOutput("y")
                  .Attr("axis", axis_val)
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  int64_t result = -1;
  AscendString axis_name("axis");
  EXPECT_EQ(node->GetAttr(axis_name, result), GRAPH_SUCCESS);
  EXPECT_EQ(result, 42);
}

// ST: 带可选输入的算子构建
TEST_F(NamedIoNodeBuilderSTTest, BuildWithOptionalInput) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestConvForIRBuilder")
                  .Name("conv_node")
                  .AddInput("x")
                  .AddInput("w")
                  .AddInput("bias")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  const auto node_ptr = NodeAdapter::GNode2Node(*node);
  ASSERT_NE(node_ptr, nullptr);
  const auto op_desc = node_ptr->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  EXPECT_EQ(op_desc->GetAllInputsSize(), 3U);
}

// ST: 带可选输入的算子构建 - 只连接必选输入
TEST_F(NamedIoNodeBuilderSTTest, BuildWithOptionalInputSkipped) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestConvForIRBuilder")
                  .Name("conv_no_bias")
                  .AddInput("x")
                  .AddInput("w")
                  .AddOutput("y")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  const auto node_ptr = NodeAdapter::GNode2Node(*node);
  ASSERT_NE(node_ptr, nullptr);
  const auto op_desc = node_ptr->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  EXPECT_EQ(op_desc->GetInputsSize(), 2U);
  EXPECT_EQ(op_desc->GetAllInputsSize(), 2U);
}

// ST: 动态输入/输出
TEST_F(NamedIoNodeBuilderSTTest, BuildDynamicInputOutput) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestDynamicForIRBuilder")
                  .Name("dynamic_node")
                  .AddInput("x0")
                  .AddInput("x1")
                  .AddOutput("y0")
                  .AddOutput("y1")
                  .Build(error_msg);

  ASSERT_NE(node, nullptr) << "Build failed: " << (error_msg.GetString() != nullptr ? error_msg.GetString() : "");

  const auto node_ptr = NodeAdapter::GNode2Node(*node);
  ASSERT_NE(node_ptr, nullptr);
  const auto op_desc = node_ptr->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  EXPECT_EQ(op_desc->GetInputsSize(), 2U);
  EXPECT_EQ(op_desc->GetOutputsSize(), 2U);
}

// ST: 带描述的输入/输出
TEST_F(NamedIoNodeBuilderSTTest, BuildWithTensorDesc) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  TensorDesc x_desc(Shape({3, 4}), FORMAT_ND, DT_FLOAT);
  TensorDesc y_desc(Shape({3, 4}), FORMAT_ND, DT_FLOAT);

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestAddForIRBuilder")
                  .Name("add_with_desc")
                  .AddInput("x1", x_desc)
                  .AddInput("x2", x_desc)
                  .AddOutput("y", y_desc)
                  .Build(error_msg);

  ASSERT_NE(node, nullptr);

  const auto node_ptr = NodeAdapter::GNode2Node(*node);
  ASSERT_NE(node_ptr, nullptr);
  const auto op_desc = node_ptr->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  EXPECT_EQ(op_desc->GetInputDesc(0U).GetDataType(), DT_FLOAT);
  EXPECT_EQ(op_desc->GetOutputDesc(0U).GetDataType(), DT_FLOAT);
}

// ST: 异常 - 未注册算子
TEST_F(NamedIoNodeBuilderSTTest, BuildWithUnregisteredType_ReturnsNullptr) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  auto node =
      NamedIoNodeBuilder(graph).Type("NonExistentOp").Name("bad_node").AddInput("x").AddOutput("y").Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
}

// ST: 异常 - 缺少必选输入
TEST_F(NamedIoNodeBuilderSTTest, BuildWithMissingRequiredInput_ReturnsNullptr) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestAddForIRBuilder")
                  .Name("missing_input_node")
                  .AddInput("x1")
                  .AddOutput("y")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
}

// ST: 异常 - 输入名称不匹配
TEST_F(NamedIoNodeBuilderSTTest, BuildWithIncompatibleInputName_ReturnsNullptr) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestAddForIRBuilder")
                  .Name("bad_input_node")
                  .AddInput("x1")
                  .AddInput("not_in_ir")
                  .AddOutput("y")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
}

// ST: 异常 - 缺少必选输出
TEST_F(NamedIoNodeBuilderSTTest, BuildWithMissingRequiredOutput_ReturnsNullptr) {
  Graph graph("st_test_graph");
  AscendString error_msg;

  auto node = NamedIoNodeBuilder(graph)
                  .Type("STTestAddForIRBuilder")
                  .Name("missing_output_node")
                  .AddInput("x1")
                  .AddInput("x2")
                  .Build(error_msg);

  EXPECT_EQ(node, nullptr);
  EXPECT_NE(error_msg.GetString(), nullptr);
}

// ST: 多节点构建 - 在同一个 graph 上构建多个节点
TEST_F(NamedIoNodeBuilderSTTest, BuildMultipleNodesOnSameGraph) {
  Graph graph("st_multi_node_graph");
  AscendString error_msg;

  auto node1 = NamedIoNodeBuilder(graph)
                   .Type("STTestAddForIRBuilder")
                   .Name("add1")
                   .AddInput("x1")
                   .AddInput("x2")
                   .AddOutput("y")
                   .Build(error_msg);
  ASSERT_NE(node1, nullptr);

  auto node2 = NamedIoNodeBuilder(graph)
                   .Type("STTestAddForIRBuilder")
                   .Name("add2")
                   .AddInput("x1")
                   .AddInput("x2")
                   .AddOutput("y")
                   .Build(error_msg);
  ASSERT_NE(node2, nullptr);

  const auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);
  EXPECT_EQ(compute_graph->GetDirectNodesSize(), 2U);
}

}  // namespace ge
