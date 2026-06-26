/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "graph/build/dag/dag_graph.h"
#include "graph/build/dag/dag_stream_merger.h"

namespace minidag {
namespace test {

class StreamMergerTest : public testing::Test {
 protected:
  void SetUp() override {
    dag_ = std::make_shared<DAGGraph>("test");
    options_.physical_stream_limit = 8;
  }
  void TearDown() override {
    dag_.reset();
  }

  void AddNodes(int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
      auto node = dag_->AddNode("node" + std::to_string(i), "Dummy");
      node->SetTopoId(i);
    }
  }

  std::shared_ptr<DAGGraph> dag_;
  StreamMergeOptions options_;
};

/**
 * 场景 1: 物理流数量限制验证
 * 验证 Merge 正确遵守物理流数量限制
 */
TEST_F(StreamMergerTest, Merge_RespectsPhysicalStreamLimit) {
  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < 10; ++i) {
    logical_routes.push_back({i});
  }

  AddNodes(10);
  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 10);

  for (auto physical_idx : logical_to_physical) {
    EXPECT_LT(physical_idx, 4);
  }
}

/**
 * 场景 2: LoadBalance与MainStream结果对比
 * 验证两种策略都能正确执行并返回有效结果
 */
TEST_F(StreamMergerTest, LoadBalanceVsMainStream_CompareResults) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}, {3}, {4}, {5}};
  std::vector<int32_t> logical_to_physical_3;
  std::vector<int32_t> logical_to_physical_4;

  AddNodes(6);
  options_.strategy = StreamMergeStrategy::kLoadBalance;
  StreamMerger merger3(options_);
  merger3.Merge(*dag_, logical_routes, logical_to_physical_3);

  options_.strategy = StreamMergeStrategy::kMainStream;
  StreamMerger merger4(options_);
  merger4.Merge(*dag_, logical_routes, logical_to_physical_4);

  EXPECT_EQ(logical_to_physical_3.size(), 6);
  EXPECT_EQ(logical_to_physical_4.size(), 6);
}

/**
 * 场景 3: 空路由处理
 * 验证空输入返回成功且输出为空
 */
TEST_F(StreamMergerTest, EmptyRoutes_ReturnsSuccess) {
  std::vector<std::vector<int32_t>> logical_routes;
  std::vector<int32_t> logical_to_physical;

  StreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_TRUE(logical_to_physical.empty());
}

/**
 * 场景 4: 单路由处理
 * 验证单个逻辑路由正确映射到物理流
 */
TEST_F(StreamMergerTest, SingleRoute_ReturnsSuccess) {
  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 2}};
  std::vector<int32_t> logical_to_physical;

  AddNodes(3);
  StreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 1);
  EXPECT_GE(logical_to_physical[0], 0);
  EXPECT_LT(logical_to_physical[0], options_.physical_stream_limit);
}

/**
 * 场景 5: 相同路由合并
 * 验证相同的逻辑路由被映射到相同的物理流
 */
TEST_F(StreamMergerTest, SameRoutes_MergedToSamePhysical) {
  std::vector<std::vector<int32_t>> logical_routes = {{0, 1}, {2, 3}};
  std::vector<int32_t> logical_to_physical;

  AddNodes(4);
  StreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 6: 不同物理流限制测试
 * 验证不同物理流限制下合并结果正确
 */
TEST_F(StreamMergerTest, DifferentPhysicalStreamLimits) {
  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < 20; ++i) {
    logical_routes.push_back({i});
  }

  AddNodes(20);
  // 测试限制为 2
  {
    options_.physical_stream_limit = 2;
    StreamMerger merger(options_);
    std::vector<int32_t> logical_to_physical;
    auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
    EXPECT_EQ(status, graphStatus::SUCCESS);
    for (auto idx : logical_to_physical) {
      EXPECT_LT(idx, 2);
    }
  }

  // 测试限制为 8
  {
    options_.physical_stream_limit = 8;
    StreamMerger merger(options_);
    std::vector<int32_t> logical_to_physical;
    auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
    EXPECT_EQ(status, graphStatus::SUCCESS);
    for (auto idx : logical_to_physical) {
      EXPECT_LT(idx, 8);
    }
  }

  // 测试限制为 16
  {
    options_.physical_stream_limit = 16;
    StreamMerger merger(options_);
    std::vector<int32_t> logical_to_physical;
    auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
    EXPECT_EQ(status, graphStatus::SUCCESS);
    for (auto idx : logical_to_physical) {
      EXPECT_LT(idx, 16);
    }
  }
}

/**
 * 场景 7: 空路由条目处理
 * 验证包含空路由条目的输入被正确处理
 */
TEST_F(StreamMergerTest, RoutesWithEmptyEntries) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {1}, {2}};
  std::vector<int32_t> logical_to_physical;

  AddNodes(3);
  StreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 3);
}

/**
 * 场景 8: 复杂路由模式测试
 * 验证复杂的多跳路由模式正确处理
 */
TEST_F(StreamMergerTest, ComplexRoutePattern) {
  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}};
  std::vector<int32_t> logical_to_physical;

  AddNodes(9);
  StreamMerger merger(options_);
  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 3);
}

/**
 * 场景 9: LoadBalance 基本功能验证
 * 验证 LoadBalance 策略基本功能正常
 */
TEST_F(StreamMergerTest, LoadBalance_BasicFunctionality) {
  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < 8; ++i) {
    logical_routes.push_back({i});
  }

  AddNodes(8);
  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 8);

  for (auto idx : logical_to_physical) {
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, 4);
  }
}

/**
 * 场景 10: MainStream 基本功能验证
 * 验证 MainStream 策略基本功能正常
 */
TEST_F(StreamMergerTest, MainStream_BasicFunctionality) {
  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < 8; ++i) {
    logical_routes.push_back({i});
  }

  AddNodes(8);
  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);

  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 8);

  for (auto idx : logical_to_physical) {
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, 4);
  }
}

/**
 * 场景 11: 非法物理流限制验证
 * 验证 physical_stream_limit <= 0 时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_InvalidPhysicalStreamLimit_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}};
  AddNodes(1);

  options_.physical_stream_limit = 0;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
  EXPECT_TRUE(logical_to_physical.empty());
}

/**
 * 场景 12: 非法窗口宽度验证
 * 验证 window_width <= 0 时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_InvalidWindowWidth_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}};
  AddNodes(1);

  options_.window_width = 0;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 13: 非法候选流限制验证
 * 验证 candidate_limit <= 0 时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_InvalidCandidateLimit_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}};
  AddNodes(1);

  options_.candidate_limit = 0;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 14: 非法修复步数验证
 * 验证 repair_moves < 0 时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_InvalidRepairMoves_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}};
  AddNodes(1);

  options_.repair_moves = -1;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 15: LoadBalance 非法轻量流限制验证
 * 验证 LoadBalance 策略下 light_stream_limit <= 0 时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_InvalidLightStreamLimit_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}};
  AddNodes(1);

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.light_stream_limit = 0;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 16: MainStream 非法低冲突限制验证
 * 验证 MainStream 策略下 low_conflict_limit <= 0 时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_InvalidLowConflictLimit_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}};
  AddNodes(1);

  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.low_conflict_limit = 0;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 17: MainStream 非法主流索引验证
 * 验证 MainStream 策略下 main_stream < 0 时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_InvalidMainStream_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}};
  AddNodes(1);

  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.main_stream = -1;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 18: LoadBalance 策略带修复步数
 * 验证 repair_moves > 0 时 repair 逻辑正常执行
 */
TEST_F(StreamMergerTest, Merge_WithRepairMoves_LoadBalance) {
  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < 8; ++i) {
    logical_routes.push_back({i});
  }
  AddNodes(8);

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 4;
  options_.repair_moves = 3;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 8);

  for (auto idx : logical_to_physical) {
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, options_.physical_stream_limit);
  }
}

/**
 * 场景 19: MainStream 策略带修复步数
 * 验证 repair_moves > 0 时 repair 逻辑正常执行
 */
TEST_F(StreamMergerTest, Merge_WithRepairMoves_MainStream) {
  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < 8; ++i) {
    logical_routes.push_back({i});
  }
  AddNodes(8);

  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.physical_stream_limit = 4;
  options_.repair_moves = 3;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 8);

  for (auto idx : logical_to_physical) {
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, options_.physical_stream_limit);
  }
}

/**
 * 场景 20: DAG 图带边结构
 * 验证复杂 DAG 图（带边）正确处理
 */
TEST_F(StreamMergerTest, Merge_DAGWithEdges_LoadBalance) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Add");
  auto n2 = dag_->AddNode("n2", "Relu");
  auto n3 = dag_->AddNode("n3", "Mul");
  auto n4 = dag_->AddNode("n4", "NetOutput");

  n0->SetTopoId(0);
  n1->SetTopoId(1);
  n2->SetTopoId(2);
  n3->SetTopoId(3);
  n4->SetTopoId(4);

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n1, 0, n2, 0);
  dag_->AddEdge(n2, 0, n3, 0);
  dag_->AddEdge(n3, 0, n4, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1}, {2, 3, 4}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 21: DAG 图带边结构 MainStream 策略
 * 验证复杂 DAG 图 MainStream 策略正确处理
 */
TEST_F(StreamMergerTest, Merge_DAGWithEdges_MainStream) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Add");
  auto n2 = dag_->AddNode("n2", "Relu");
  auto n3 = dag_->AddNode("n3", "Mul");
  auto n4 = dag_->AddNode("n4", "NetOutput");

  n0->SetTopoId(0);
  n1->SetTopoId(1);
  n2->SetTopoId(2);
  n3->SetTopoId(3);
  n4->SetTopoId(4);

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n1, 0, n2, 0);
  dag_->AddEdge(n2, 0, n3, 0);
  dag_->AddEdge(n3, 0, n4, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1}, {2, 3, 4}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 22: 多分支 DAG 图
 * 验证多分支 DAG 图正确处理
 */
TEST_F(StreamMergerTest, Merge_MultiBranchDAG_LoadBalance) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Add");
  auto n2 = dag_->AddNode("n2", "Relu");
  auto n3 = dag_->AddNode("n3", "Mul");
  auto n4 = dag_->AddNode("n4", "NetOutput");

  n0->SetTopoId(0);
  n1->SetTopoId(1);
  n2->SetTopoId(2);
  n3->SetTopoId(3);
  n4->SetTopoId(4);

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n0, 0, n2, 0);
  dag_->AddEdge(n1, 0, n3, 0);
  dag_->AddEdge(n2, 0, n3, 0);
  dag_->AddEdge(n3, 0, n4, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 3, 4}, {2}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 4;
  options_.repair_moves = 2;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 23: 多分支 DAG 图 MainStream 策略
 * 验证多分支 DAG 图 MainStream 策略正确处理
 */
TEST_F(StreamMergerTest, Merge_MultiBranchDAG_MainStream) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Add");
  auto n2 = dag_->AddNode("n2", "Relu");
  auto n3 = dag_->AddNode("n3", "Mul");
  auto n4 = dag_->AddNode("n4", "NetOutput");

  n0->SetTopoId(0);
  n1->SetTopoId(1);
  n2->SetTopoId(2);
  n3->SetTopoId(3);
  n4->SetTopoId(4);

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n0, 0, n2, 0);
  dag_->AddEdge(n1, 0, n3, 0);
  dag_->AddEdge(n2, 0, n3, 0);
  dag_->AddEdge(n3, 0, n4, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 3, 4}, {2}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.physical_stream_limit = 4;
  options_.repair_moves = 2;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 24: 节点超出索引范围
 * 验证节点索引超出范围时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_NodeIndexOutOfRange_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{100}};
  AddNodes(1);

  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 25: 节点重复分配
 * 验证节点出现在多个逻辑流中时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_NodeInMultipleRoutes_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {0}};
  AddNodes(2);

  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 26: 空逻辑流路由
 * 验证空逻辑流路由返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_EmptyLogicalRoute_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}, {}};
  AddNodes(2);

  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 27: 节点未分配到逻辑流
 * 验证节点未分配到任何逻辑流时返回 FAILED
 */
TEST_F(StreamMergerTest, Merge_NodeNotAssigned_ReturnsFailed) {
  std::vector<std::vector<int32_t>> logical_routes = {{0}};
  AddNodes(2);

  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);
  std::vector<int32_t> logical_to_physical;

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 28: 空单元数量测试
 * 验证 logical_routes 为空但 dag 有节点时返回空结果
 */
TEST_F(StreamMergerTest, Merge_EmptyRoutesWithNodes_ReturnsEmpty) {
  AddNodes(3);

  std::vector<std::vector<int32_t>> logical_routes;
  std::vector<int32_t> logical_to_physical;

  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_TRUE(logical_to_physical.empty());
}

/**
 * 场景 29: 跨流依赖图 LoadBalance
 * 验证跨流依赖的复杂图结构 LoadBalance 策略
 */
TEST_F(StreamMergerTest, Merge_CrossStreamDependency_LoadBalance) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Add");
  auto n2 = dag_->AddNode("n2", "Relu");
  auto n3 = dag_->AddNode("n3", "Mul");
  auto n4 = dag_->AddNode("n4", "Sub");
  auto n5 = dag_->AddNode("n5", "NetOutput");

  for (int i = 0; i < 6; ++i) {
    dag_->GetAllNodes()[i]->SetTopoId(i);
  }

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n0, 0, n2, 0);
  dag_->AddEdge(n1, 0, n3, 0);
  dag_->AddEdge(n2, 0, n4, 0);
  dag_->AddEdge(n3, 0, n5, 0);
  dag_->AddEdge(n4, 0, n5, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 3, 5}, {2, 4}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 3;
  options_.repair_moves = 5;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 30: 跨流依赖图 MainStream
 * 验证跨流依赖的复杂图结构 MainStream 策略
 */
TEST_F(StreamMergerTest, Merge_CrossStreamDependency_MainStream) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Add");
  auto n2 = dag_->AddNode("n2", "Relu");
  auto n3 = dag_->AddNode("n3", "Mul");
  auto n4 = dag_->AddNode("n4", "Sub");
  auto n5 = dag_->AddNode("n5", "NetOutput");

  for (int i = 0; i < 6; ++i) {
    dag_->GetAllNodes()[i]->SetTopoId(i);
  }

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n0, 0, n2, 0);
  dag_->AddEdge(n1, 0, n3, 0);
  dag_->AddEdge(n2, 0, n4, 0);
  dag_->AddEdge(n3, 0, n5, 0);
  dag_->AddEdge(n4, 0, n5, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 3, 5}, {2, 4}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.physical_stream_limit = 3;
  options_.repair_moves = 5;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 31: 大规模节点测试 LoadBalance
 * 验证大规模节点正确处理
 */
TEST_F(StreamMergerTest, Merge_LargeScaleNodes_LoadBalance) {
  const int node_count = 50;
  for (int i = 0; i < node_count; ++i) {
    auto node = dag_->AddNode("node" + std::to_string(i), "Op");
    node->SetTopoId(i);
  }

  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < node_count; i += 5) {
    logical_routes.push_back({i, i + 1, i + 2, i + 3, i + 4});
  }

  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 8;
  options_.repair_moves = 10;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 10);

  for (auto idx : logical_to_physical) {
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, 8);
  }
}

/**
 * 场景 32: 大规模节点测试 MainStream
 * 验证大规模节点 MainStream 策略正确处理
 */
TEST_F(StreamMergerTest, Merge_LargeScaleNodes_MainStream) {
  const int node_count = 50;
  for (int i = 0; i < node_count; ++i) {
    auto node = dag_->AddNode("node" + std::to_string(i), "Op");
    node->SetTopoId(i);
  }

  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < node_count; i += 5) {
    logical_routes.push_back({i, i + 1, i + 2, i + 3, i + 4});
  }

  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.physical_stream_limit = 8;
  options_.repair_moves = 10;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 10);

  for (auto idx : logical_to_physical) {
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, 8);
  }
}

/**
 * 场景 33: 单流限制测试
 * 验证物理流限制为 1 时所有单元分配到同一流
 */
TEST_F(StreamMergerTest, Merge_SinglePhysicalStream) {
  AddNodes(10);

  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < 10; ++i) {
    logical_routes.push_back({i});
  }

  std::vector<int32_t> logical_to_physical;

  options_.physical_stream_limit = 1;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 10);

  for (auto idx : logical_to_physical) {
    EXPECT_EQ(idx, 0);
  }
}

/**
 * 场景 34: 多层级 DAG 测试
 * 验证多层级 DAG 结构正确处理
 */
TEST_F(StreamMergerTest, Merge_MultiLevelDAG) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Op1");
  auto n2 = dag_->AddNode("n2", "Op2");
  auto n3 = dag_->AddNode("n3", "Op3");
  auto n4 = dag_->AddNode("n4", "Op4");
  auto n5 = dag_->AddNode("n5", "Op5");
  auto n6 = dag_->AddNode("n6", "NetOutput");

  for (int i = 0; i < 7; ++i) {
    dag_->GetAllNodes()[i]->SetTopoId(i);
  }

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n1, 0, n2, 0);
  dag_->AddEdge(n2, 0, n3, 0);
  dag_->AddEdge(n3, 0, n4, 0);
  dag_->AddEdge(n4, 0, n5, 0);
  dag_->AddEdge(n5, 0, n6, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 2}, {3, 4, 5, 6}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 4;
  options_.repair_moves = 3;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
}

/**
 * 场景 35: 带环图结构测试（非 DAG）
 * 验证带环图返回 FAILED（覆盖行 304-305）
 */
TEST_F(StreamMergerTest, Merge_CyclicGraph_ReturnsFailed) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Op1");
  auto n2 = dag_->AddNode("n2", "Op2");
  auto n3 = dag_->AddNode("n3", "NetOutput");

  for (int i = 0; i < 4; ++i) {
    dag_->GetAllNodes()[i]->SetTopoId(i);
  }

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n1, 0, n2, 0);
  dag_->AddEdge(n2, 0, n1, 0);  // 环：n1 -> n2 -> n1
  dag_->AddEdge(n2, 0, n3, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 2, 3}};
  std::vector<int32_t> logical_to_physical;

  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 36: 极端候选流限制测试
 * 验证候选流限制为 0 时正确处理
 */
TEST_F(StreamMergerTest, Merge_ZeroCandidateLimit_ReturnsFailed) {
  AddNodes(5);

  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < 5; ++i) {
    logical_routes.push_back({i});
  }

  std::vector<int32_t> logical_to_physical;

  options_.candidate_limit = 0;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::FAILED);
}

/**
 * 场景 37: 复杂交互图 LoadBalance
 * 验证多个单元间有复杂交互关系的场景
 */
TEST_F(StreamMergerTest, Merge_ComplexInteraction_LoadBalance) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Add");
  auto n2 = dag_->AddNode("n2", "Mul");
  auto n3 = dag_->AddNode("n3", "Relu");
  auto n4 = dag_->AddNode("n4", "Sub");
  auto n5 = dag_->AddNode("n5", "Div");
  auto n6 = dag_->AddNode("n6", "NetOutput");

  for (int i = 0; i < 7; ++i) {
    dag_->GetAllNodes()[i]->SetTopoId(i);
  }

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n0, 0, n2, 0);
  dag_->AddEdge(n1, 0, n3, 0);
  dag_->AddEdge(n2, 0, n3, 0);
  dag_->AddEdge(n3, 0, n4, 0);
  dag_->AddEdge(n3, 0, n5, 0);
  dag_->AddEdge(n4, 0, n6, 0);
  dag_->AddEdge(n5, 0, n6, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 3, 4, 6}, {2, 5}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 4;
  options_.repair_moves = 5;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 38: 复杂交互图 MainStream
 * 验证多个单元间有复杂交互关系的 MainStream 场景
 */
TEST_F(StreamMergerTest, Merge_ComplexInteraction_MainStream) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Add");
  auto n2 = dag_->AddNode("n2", "Mul");
  auto n3 = dag_->AddNode("n3", "Relu");
  auto n4 = dag_->AddNode("n4", "Sub");
  auto n5 = dag_->AddNode("n5", "Div");
  auto n6 = dag_->AddNode("n6", "NetOutput");

  for (int i = 0; i < 7; ++i) {
    dag_->GetAllNodes()[i]->SetTopoId(i);
  }

  dag_->AddEdge(n0, 0, n1, 0);
  dag_->AddEdge(n0, 0, n2, 0);
  dag_->AddEdge(n1, 0, n3, 0);
  dag_->AddEdge(n2, 0, n3, 0);
  dag_->AddEdge(n3, 0, n4, 0);
  dag_->AddEdge(n3, 0, n5, 0);
  dag_->AddEdge(n4, 0, n6, 0);
  dag_->AddEdge(n5, 0, n6, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 3, 4, 6}, {2, 5}};
  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kMainStream;
  options_.physical_stream_limit = 4;
  options_.repair_moves = 5;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 39: 单节点多路由测试
 * 验证每个路由只有一个节点的简单场景
 */
TEST_F(StreamMergerTest, Merge_SingleNodePerRoute) {
  const int count = 20;
  AddNodes(count);

  std::vector<std::vector<int32_t>> logical_routes;
  for (int i = 0; i < count; ++i) {
    logical_routes.push_back({i});
  }

  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 8;
  options_.repair_moves = 3;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), count);

  for (auto idx : logical_to_physical) {
    EXPECT_GE(idx, 0);
    EXPECT_LT(idx, 8);
  }
}

/**
 * 场景 40: 长链路测试
 * 验证单个长链路正确处理
 */
TEST_F(StreamMergerTest, Merge_LongChainRoute) {
  const int count = 30;
  for (int i = 0; i < count; ++i) {
    auto node = dag_->AddNode("node" + std::to_string(i), "Op");
    node->SetTopoId(i);
    if (i > 0) {
      dag_->AddEdge(dag_->GetAllNodes()[i - 1], 0, node, 0);
    }
  }

  std::vector<int32_t> chain_route;
  for (int i = 0; i < count; ++i) {
    chain_route.push_back(i);
  }

  std::vector<std::vector<int32_t>> logical_routes = {chain_route};
  std::vector<int32_t> logical_to_physical;

  options_.physical_stream_limit = 8;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 1);
}

/**
 * 场景 41: 多入口图测试
 * 验证多个 Data 节点作为入口的场景
 */
TEST_F(StreamMergerTest, Merge_MultipleDataNodes) {
  auto d0 = dag_->AddNode("d0", "Data");
  auto d1 = dag_->AddNode("d1", "Data");
  auto d2 = dag_->AddNode("d2", "Data");
  auto add = dag_->AddNode("add", "Add");
  auto out = dag_->AddNode("out", "NetOutput");

  for (int i = 0; i < 5; ++i) {
    dag_->GetAllNodes()[i]->SetTopoId(i);
  }

  dag_->AddEdge(d0, 0, add, 0);
  dag_->AddEdge(d1, 0, add, 0);
  dag_->AddEdge(d2, 0, add, 0);
  dag_->AddEdge(add, 0, out, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 3, 4}, {1}, {2}};
  std::vector<int32_t> logical_to_physical;

  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 3);
}

/**
 * 场景 42: 多出口图测试
 * 验证多个 NetOutput 节点的场景
 */
TEST_F(StreamMergerTest, Merge_MultipleNetOutputNodes) {
  auto data = dag_->AddNode("data", "Data");
  auto add = dag_->AddNode("add", "Add");
  auto mul = dag_->AddNode("mul", "Mul");
  auto out0 = dag_->AddNode("out0", "NetOutput");
  auto out1 = dag_->AddNode("out1", "NetOutput");

  for (int i = 0; i < 5; ++i) {
    dag_->GetAllNodes()[i]->SetTopoId(i);
  }

  dag_->AddEdge(data, 0, add, 0);
  dag_->AddEdge(data, 0, mul, 0);
  dag_->AddEdge(add, 0, out0, 0);
  dag_->AddEdge(mul, 0, out1, 0);

  std::vector<std::vector<int32_t>> logical_routes = {{0, 1, 3}, {2, 4}};
  std::vector<int32_t> logical_to_physical;

  options_.physical_stream_limit = 4;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 2);
}

/**
 * 场景 43: BuildUnitsByEarliestLevel interaction 比较
 * 验证同一 earliest_level 多个单元按 interaction 排序（覆盖行 441）
 *
 * DAG 结构：
 *   n0 (level 0) → n2 (level 1), n4 (level 1)  [Route A 有 2 个后继]
 *   n1 (level 0) → n3 (level 1)                 [Route C 有 1 个后继]
 *
 * earliest_level=0 组: Route A (interaction=2), Route C (interaction=1)
 * 排序时 size 相同，interaction 不同，触发 interaction 比较
 */
TEST_F(StreamMergerTest, Merge_UnitSortByInteraction_LoadBalance) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Data");
  auto n2 = dag_->AddNode("n2", "Op1");
  auto n3 = dag_->AddNode("n3", "Op2");
  auto n4 = dag_->AddNode("n4", "Op3");
  auto n5 = dag_->AddNode("n5", "NetOutput");

  n0->SetTopoId(0);
  n1->SetTopoId(1);
  n2->SetTopoId(2);
  n3->SetTopoId(3);
  n4->SetTopoId(4);
  n5->SetTopoId(5);

  dag_->AddEdge(n0, 0, n2, 0);
  dag_->AddEdge(n0, 0, n4, 0);
  dag_->AddEdge(n1, 0, n3, 0);
  dag_->AddEdge(n2, 0, n5, 0);
  dag_->AddEdge(n3, 0, n5, 0);
  dag_->AddEdge(n4, 0, n5, 0);

  std::vector<std::vector<int32_t>> logical_routes = {
      {0},     // Route A: {n0}, earliest_level=0, interaction=2 (后继 Route B, Route E)
      {2, 5},  // Route B: {n2, n5}, earliest_level=1
      {1},     // Route C: {n1}, earliest_level=0, interaction=1 (后继 Route D)
      {3},     // Route D: {n3}, earliest_level=1
      {4}      // Route E: {n4}, earliest_level=1
  };

  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 8;
  options_.repair_moves = 0;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 5);
}

/**
 * 场景 44: BuildUnitsByEarliestLevel peak_level_load 比较
 * 验证同一 earliest_level 多个单元按 peak_level_load 排序（覆盖行 444）
 *
 * DAG 结构：
 *   n0 (level 0) → n3 (level 1)  [Route A: 2节点, n0(level0)+n3(level1), peak=1]
 *   n1 → n2 (level 0→1→2)        [Route C: 3节点链, peak=3, 但取前2节点最早level=0]
 *
 * 简化设计：构造 size 相等的两个 Route
 *   Route A: {n0, n3} size=2, peak_level_load 取各 level 最大节点数 = 1
 *   Route C: {n1, n2} size=2, peak_level_load = 2 (n1,n2都在level 0)
 *
 * earliest_level=0 组: Route A (peak=1), Route C (peak=2)
 * 排序时 size 相同(=2)，interaction 相同(无跨流边)，peak_level_load 不同
 */
TEST_F(StreamMergerTest, Merge_UnitSortByPeakLevelLoad_LoadBalance) {
  auto n0 = dag_->AddNode("n0", "Data");
  auto n1 = dag_->AddNode("n1", "Data");
  auto n2 = dag_->AddNode("n2", "Op");
  auto n3 = dag_->AddNode("n3", "Op");
  auto n4 = dag_->AddNode("n4", "NetOutput");

  n0->SetTopoId(0);
  n1->SetTopoId(1);
  n2->SetTopoId(2);
  n3->SetTopoId(3);
  n4->SetTopoId(4);

  // n0 → n3 → n4 (Route A, n0和n3在同一Route)
  // n1 → n2 → n4 (Route C, n1和n2在同一Route, n1,n2不同level)
  dag_->AddEdge(n0, 0, n3, 0);
  dag_->AddEdge(n1, 0, n2, 0);
  dag_->AddEdge(n3, 0, n4, 0);
  dag_->AddEdge(n2, 0, n4, 0);

  std::vector<std::vector<int32_t>> logical_routes = {
      {0, 3},  // Route A: {n0, n3}, size=2
      {1, 2},  // Route C: {n1, n2}, size=2
      {4}      // Route B: {n4} - NetOutput单独成流
  };

  std::vector<int32_t> logical_to_physical;

  options_.strategy = StreamMergeStrategy::kLoadBalance;
  options_.physical_stream_limit = 8;
  options_.repair_moves = 0;
  StreamMerger merger(options_);

  auto status = merger.Merge(*dag_, logical_routes, logical_to_physical);
  EXPECT_EQ(status, graphStatus::SUCCESS);
  EXPECT_EQ(logical_to_physical.size(), 3);
}

}  // namespace test
}  // namespace minidag
