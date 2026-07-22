/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>

#include "macro_utils/dt_public_scope.h"
#include "engines/local_engine/ops_kernel_store/op/no_op.h"
#include "engines/local_engine/ops_kernel_store/ge_local_ops_kernel_builder.h"
#include "engines/local_engine/ops_kernel_store/op/ge_local_op_factory.h"
#include "macro_utils/dt_public_unscope.h"

using namespace std;
using namespace testing;

namespace ge {
namespace ge_local {

class UtestNoOp : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UtestNoOp, Normal) {
  RunContext runContext;
  OpDescPtr opdesc = std::make_shared<OpDesc>("test", "test");
  ComputeGraphPtr graph;
  const NodePtr node = std::make_shared<Node>(opdesc, graph);
  std::shared_ptr<NoOp> op = std::make_shared<NoOp>(*node, runContext);
  EXPECT_EQ(op->Run(), SUCCESS);
}

TEST_F(UtestNoOp, FactoryCreateAndRun_DataFlowOps) {
  RunContext runContext;
  const std::vector<std::string> types = {"Data",     "RefData",      "QueueData",       "AippData",
                                          "Variable", "Constant",     "Const",           "ControlTrigger",
                                          "Merge",    "FileConstant", "ConstPlaceHolder"};
  for (const auto &type : types) {
    OpDescPtr opdesc = std::make_shared<OpDesc>("test", type);
    ComputeGraphPtr graph;
    const NodePtr node = std::make_shared<Node>(opdesc, graph);
    auto op = OpFactory::Instance().CreateOp(*node, runContext);
    EXPECT_NE(op, nullptr) << "Failed to create op for type: " << type;
    EXPECT_EQ(op->Run(), SUCCESS) << "Run failed for type: " << type;
  }
}

TEST_F(UtestNoOp, FactoryCreateAndRun_FunctionalOps) {
  RunContext runContext;
  const std::vector<std::string> types = {"If",
                                          "_If",
                                          "StatelessIf",
                                          "Case",
                                          "StatelessCase",
                                          "While",
                                          "_While",
                                          "StatelessWhile",
                                          "For",
                                          "PartitionedCall",
                                          "StatefulPartitionedCall",
                                          "OpTiling",
                                          "ConditionCalc",
                                          "UnfedData"};
  for (const auto &type : types) {
    OpDescPtr opdesc = std::make_shared<OpDesc>("test", type);
    ComputeGraphPtr graph;
    const NodePtr node = std::make_shared<Node>(opdesc, graph);
    auto op = OpFactory::Instance().CreateOp(*node, runContext);
    EXPECT_NE(op, nullptr) << "Failed to create op for type: " << type;
    EXPECT_EQ(op->Run(), SUCCESS) << "Run failed for type: " << type;
  }
}

TEST_F(UtestNoOp, FactoryCreateAndRun_StackOps) {
  RunContext runContext;
  const std::vector<std::string> types = {"Stack", "StackPush", "StackPop", "StackClose"};
  for (const auto &type : types) {
    OpDescPtr opdesc = std::make_shared<OpDesc>("test", type);
    ComputeGraphPtr graph;
    const NodePtr node = std::make_shared<Node>(opdesc, graph);
    auto op = OpFactory::Instance().CreateOp(*node, runContext);
    EXPECT_NE(op, nullptr) << "Failed to create op for type: " << type;
    EXPECT_EQ(op->Run(), SUCCESS) << "Run failed for type: " << type;
  }
}

TEST_F(UtestNoOp, FactoryCreateAndRun_ShapeOps) {
  RunContext runContext;
  const std::vector<std::string> types = {"Bitcast", "PhonyConcat", "PhonySplit", "Flatten"};
  for (const auto &type : types) {
    OpDescPtr opdesc = std::make_shared<OpDesc>("test", type);
    ComputeGraphPtr graph;
    const NodePtr node = std::make_shared<Node>(opdesc, graph);
    auto op = OpFactory::Instance().CreateOp(*node, runContext);
    EXPECT_NE(op, nullptr) << "Failed to create op for type: " << type;
    EXPECT_EQ(op->Run(), SUCCESS) << "Run failed for type: " << type;
  }
}

TEST_F(UtestNoOp, FactoryCreateAndRun_NoOpType) {
  RunContext runContext;
  OpDescPtr opdesc = std::make_shared<OpDesc>("test", "NoOp");
  ComputeGraphPtr graph;
  const NodePtr node = std::make_shared<Node>(opdesc, graph);
  auto op = OpFactory::Instance().CreateOp(*node, runContext);
  EXPECT_NE(op, nullptr);
  EXPECT_EQ(op->Run(), SUCCESS);
}

TEST_F(UtestNoOp, FactoryCreate_UnsupportedType) {
  RunContext runContext;
  OpDescPtr opdesc = std::make_shared<OpDesc>("test", "UnsupportedOpType_xyz");
  ComputeGraphPtr graph;
  const NodePtr node = std::make_shared<Node>(opdesc, graph);
  auto op = OpFactory::Instance().CreateOp(*node, runContext);
  EXPECT_EQ(op, nullptr);
}

TEST_F(UtestNoOp, FactoryGetAllOps_ContainsNoOpTypes) {
  const auto &all_ops = OpFactory::Instance().GetAllOps();
  const std::vector<std::string> expected_types = {"Data", "Const", "Merge",   "If",          "While",
                                                   "For",  "Stack", "Bitcast", "PhonyConcat", "Flatten"};
  for (const auto &type : expected_types) {
    bool found = false;
    for (const auto &registered_type : all_ops) {
      if (registered_type == type) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Type " << type << " not found in registered ops";
  }
}

}  // namespace ge_local
}  // namespace ge
