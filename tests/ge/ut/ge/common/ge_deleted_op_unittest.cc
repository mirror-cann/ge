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
#include "engines/local_engine/ops_kernel_store/op/ge_deleted_op.h"
#include "engines/local_engine/ops_kernel_store/ge_local_ops_kernel_builder.h"
#include "engines/local_engine/ops_kernel_store/op/ge_local_op_factory.h"
#include "macro_utils/dt_public_unscope.h"
#include "ge_local_context.h"
#include "dlog_pub.h"
#include "optimization_option_registry.h"

using namespace std;
using namespace testing;

namespace ge {
namespace ge_local {

class UtestGeDeletedOp : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UtestGeDeletedOp, Normal) {
  RunContext runContext;
  OpDescPtr opdesc = std::make_shared<OpDesc>("test", "test");
  ComputeGraphPtr graph;
  const NodePtr node = std::make_shared<Node>(opdesc, graph);
  std::shared_ptr<GeDeletedOp> op = std::make_shared<GeDeletedOp>(*node, runContext);
  EXPECT_EQ(op->Run(), FAILED);
}

TEST_F(UtestGeDeletedOp, Shape) {
  RunContext runContext;
  OpDescPtr opdesc = std::make_shared<OpDesc>("test", "Shape");
  GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
  ComputeGraphPtr graph;
  const NodePtr node = std::make_shared<Node>(opdesc, graph);
  std::shared_ptr<GeDeletedOp> op = std::make_shared<GeDeletedOp>(*node, runContext);
  EXPECT_EQ(op->Run(), FAILED);
}

TEST_F(UtestGeDeletedOp, Shape_ConsantFoling_Off) {
  RunContext runContext;
  OpDescPtr opdesc = std::make_shared<OpDesc>("test", "Shape");
  GetThreadLocalContext().GetOo().Initialize({{ge::OO_LEVEL, "O1"}, {ge::OO_CONSTANT_FOLDING, "false"}},
                                             OptionRegistry::GetInstance().GetRegisteredOptTable());

  ComputeGraphPtr graph;
  const NodePtr node = std::make_shared<Node>(opdesc, graph);
  std::shared_ptr<GeDeletedOp> op = std::make_shared<GeDeletedOp>(*node, runContext);
  EXPECT_EQ(op->Run(), FAILED);
  GetThreadLocalContext().SetGlobalOption({});
}

TEST_F(UtestGeDeletedOp, Shape_ConsantFoling_On) {
  RunContext runContext;
  OpDescPtr opdesc = std::make_shared<OpDesc>("test", "Shape");
  GetThreadLocalContext().GetOo().Initialize({{ge::OO_LEVEL, "O3"}},
                                             OptionRegistry::GetInstance().GetRegisteredOptTable());
  ComputeGraphPtr graph;
  const NodePtr node = std::make_shared<Node>(opdesc, graph);
  std::shared_ptr<GeDeletedOp> op = std::make_shared<GeDeletedOp>(*node, runContext);
  EXPECT_EQ(op->Run(), FAILED);
}

TEST_F(UtestGeDeletedOp, FactoryCreateAndRun_NonOptMapTypes) {
  RunContext runContext;
  const std::vector<std::string> types = {"TemporaryVariable",
                                          "DestroyTemporaryVariable",
                                          "GuaranteeConst",
                                          "PreventGradient",
                                          "StopGradient",
                                          "_Retval",
                                          "ReadVariableOp",
                                          "VarHandleOp",
                                          "VarIsInitializedOp",
                                          "Snapshot",
                                          "RefIdentity",
                                          "Identity",
                                          "IdentityN",
                                          "VariableV2",
                                          "Empty",
                                          "PlaceholderWithDefault",
                                          "IsVariableInitialized",
                                          "PlaceholderV2",
                                          "Placeholder",
                                          "End",
                                          "Switch",
                                          "RefMerge",
                                          "RefSwitch",
                                          "TransShape",
                                          "GatherShapes"};
  for (const auto &type : types) {
    OpDescPtr opdesc = std::make_shared<OpDesc>("test", type);
    ComputeGraphPtr graph;
    const NodePtr node = std::make_shared<Node>(opdesc, graph);
    auto op = OpFactory::Instance().CreateOp(*node, runContext);
    EXPECT_NE(op, nullptr) << "Failed to create op for type: " << type;
    EXPECT_EQ(op->Run(), FAILED) << "Run should fail for type: " << type;
  }
}

TEST_F(UtestGeDeletedOp, FactoryCreateAndRun_OptMapTypes) {
  RunContext runContext;
  GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
  const std::vector<std::string> types = {"Size", "Shape", "ShapeN", "Rank"};
  for (const auto &type : types) {
    OpDescPtr opdesc = std::make_shared<OpDesc>("test", type);
    ComputeGraphPtr graph;
    const NodePtr node = std::make_shared<Node>(opdesc, graph);
    auto op = OpFactory::Instance().CreateOp(*node, runContext);
    EXPECT_NE(op, nullptr) << "Failed to create op for type: " << type;
    EXPECT_EQ(op->Run(), FAILED) << "Run should fail for type: " << type;
  }
}

TEST_F(UtestGeDeletedOp, FactoryCreateAndRun_OptMapTypes_OptionOn) {
  RunContext runContext;
  GetThreadLocalContext().GetOo().Initialize({{ge::OO_LEVEL, "O3"}},
                                             OptionRegistry::GetInstance().GetRegisteredOptTable());
  const std::vector<std::string> types = {"Size", "ShapeN", "Rank"};
  for (const auto &type : types) {
    OpDescPtr opdesc = std::make_shared<OpDesc>("test", type);
    ComputeGraphPtr graph;
    const NodePtr node = std::make_shared<Node>(opdesc, graph);
    auto op = OpFactory::Instance().CreateOp(*node, runContext);
    EXPECT_NE(op, nullptr) << "Failed to create op for type: " << type;
    EXPECT_EQ(op->Run(), FAILED) << "Run should fail for type: " << type;
  }
}

TEST_F(UtestGeDeletedOp, FactoryCreateAndRun_OptMapTypes_OptionOff) {
  RunContext runContext;
  GetThreadLocalContext().GetOo().Initialize({{ge::OO_LEVEL, "O1"}, {ge::OO_CONSTANT_FOLDING, "false"}},
                                             OptionRegistry::GetInstance().GetRegisteredOptTable());
  const std::vector<std::string> types = {"Size", "ShapeN", "Rank"};
  for (const auto &type : types) {
    OpDescPtr opdesc = std::make_shared<OpDesc>("test", type);
    ComputeGraphPtr graph;
    const NodePtr node = std::make_shared<Node>(opdesc, graph);
    auto op = OpFactory::Instance().CreateOp(*node, runContext);
    EXPECT_NE(op, nullptr) << "Failed to create op for type: " << type;
    EXPECT_EQ(op->Run(), FAILED) << "Run should fail for type: " << type;
  }
  GetThreadLocalContext().SetGlobalOption({});
}

TEST_F(UtestGeDeletedOp, FactoryGetAllOps_ContainsDeletedTypes) {
  const auto &all_ops = OpFactory::Instance().GetAllOps();
  const std::vector<std::string> expected_types = {
      "TemporaryVariable", "Identity", "Switch", "Placeholder", "End", "Size", "Shape", "ShapeN", "Rank"};
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
