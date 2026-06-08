/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include "graph/build/dag/dag_graph.h"
#include "graph/build/dag/dag_stream_allocator.h"

namespace minidag {
namespace test {

class DagStreamAllocatorTest : public testing::Test {
 protected:
  void SetUp() override {
    graph_ = std::make_shared<DAGGraph>("test");
  }
  std::shared_ptr<DAGGraph> graph_;
};

TEST_F(DagStreamAllocatorTest, ByPathCover_LinearGraph_ReturnsOneStream) {
  auto n1 = graph_->AddNode("n1", "Op1");
  auto n2 = graph_->AddNode("n2", "Op2");
  auto n3 = graph_->AddNode("n3", "Op3");
  graph_->AddEdge(n1, 0, n2, 0);
  graph_->AddEdge(n2, 0, n3, 0);

  StreamAllocConfig config{-1, 0, 0};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_EQ(config.required_streams, 1);
}

TEST_F(DagStreamAllocatorTest, ByPathCover_MultiBranchGraph_ReturnsCorrectStreamCount) {
  auto n1 = graph_->AddNode("n1", "Op1");
  auto n2 = graph_->AddNode("n2", "Op2");
  auto n3 = graph_->AddNode("n3", "Op3");
  auto n4 = graph_->AddNode("n4", "Op4");
  graph_->AddEdge(n1, 0, n2, 0);
  graph_->AddEdge(n1, 0, n3, 0);
  graph_->AddEdge(n2, 0, n4, 0);
  graph_->AddEdge(n3, 0, n4, 0);

  StreamAllocConfig config{-1, 0, 0};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_GE(config.required_streams, 1);
  EXPECT_LE(config.required_streams, 4);
}

TEST_F(DagStreamAllocatorTest, ByPathCover_EmptyGraph_ReturnsZero) {
  StreamAllocConfig config{-1, 0, 0};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_EQ(config.required_streams, 0);
}

TEST_F(DagStreamAllocatorTest, ByPathCover_WithStreamLimit_RespectsLimit) {
  auto n1 = graph_->AddNode("n1", "Op1");
  auto n2 = graph_->AddNode("n2", "Op2");
  auto n3 = graph_->AddNode("n3", "Op3");
  auto n4 = graph_->AddNode("n4", "Op4");
  graph_->AddEdge(n1, 0, n2, 0);
  graph_->AddEdge(n2, 0, n3, 0);
  graph_->AddEdge(n3, 0, n4, 0);

  StreamAllocConfig config{2, 0, 0};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_LE(config.required_streams, 3);
}

TEST_F(DagStreamAllocatorTest, ByPathCover_DataNetOutputNodes_AssignedByAlgorithm) {
  auto data = graph_->AddNode("data", "Data");
  auto op = graph_->AddNode("op", "Op");
  auto netoutput = graph_->AddNode("netoutput", "NetOutput");
  graph_->AddEdge(data, 0, op, 0);
  graph_->AddEdge(op, 0, netoutput, 0);

  StreamAllocConfig config{-1, 0, 0};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_GE(data->GetStreamId(), 0);
  EXPECT_GE(netoutput->GetStreamId(), 0);
  EXPECT_EQ(config.required_streams, 1);
}

TEST_F(DagStreamAllocatorTest, ByPathCover_SerialLabelNodes_AssignedToSameAppendedStream) {
  auto n1 = graph_->AddNode("n1", "Op1");
  auto n2 = graph_->AddNode("n2", "Op2");
  auto n3 = graph_->AddNode("n3", "Op3");
  auto n4 = graph_->AddNode("n4", "Op4");
  n2->SetSerialFlag("label_a");
  n3->SetSerialFlag("label_a");
  graph_->AddEdge(n1, 0, n2, 0);
  graph_->AddEdge(n2, 0, n3, 0);
  graph_->AddEdge(n3, 0, n4, 0);

  StreamAllocConfig config{-1, 0, 0};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_EQ(n2->GetStreamId(), n3->GetStreamId());
  EXPECT_NE(n1->GetStreamId(), INVALID_STREAM_ID);
  EXPECT_NE(n4->GetStreamId(), INVALID_STREAM_ID);
  EXPECT_EQ(n2->GetStreamId(), config.required_streams - 1);
  EXPECT_EQ(config.required_streams, 2);
}

TEST_F(DagStreamAllocatorTest, ByPathCover_AllSerialNodes_ReturnsSerialLabelCount) {
  auto n1 = graph_->AddNode("n1", "Op1");
  auto n2 = graph_->AddNode("n2", "Op2");
  auto n3 = graph_->AddNode("n3", "Op3");
  n1->SetSerialFlag("label_a");
  n2->SetSerialFlag("label_b");
  n3->SetSerialFlag("label_a");
  graph_->AddEdge(n1, 0, n2, 0);
  graph_->AddEdge(n2, 0, n3, 0);

  StreamAllocConfig config{-1, 0, 5};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_EQ(config.required_streams, 2);
  EXPECT_EQ(n1->GetStreamId(), 5);
  EXPECT_EQ(n3->GetStreamId(), 5);
  EXPECT_EQ(n2->GetStreamId(), 6);
}

TEST_F(DagStreamAllocatorTest, ByPathCover_StreamLimitDoesNotLimitSerialStreams) {
  auto n1 = graph_->AddNode("n1", "Op1");
  auto n2 = graph_->AddNode("n2", "Op2");
  auto n3 = graph_->AddNode("n3", "Op3");
  n2->SetSerialFlag("label_a");
  n3->SetSerialFlag("label_b");
  graph_->AddEdge(n1, 0, n2, 0);
  graph_->AddEdge(n1, 0, n3, 0);

  StreamAllocConfig config{0, 0, 0};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_EQ(n1->GetStreamId(), 0);
  EXPECT_EQ(n2->GetStreamId(), 1);
  EXPECT_EQ(n3->GetStreamId(), 2);
  EXPECT_EQ(config.required_streams, 3);
}

TEST_F(DagStreamAllocatorTest, ByPathCover_EdgeToNodeOutsideGraph_AbortWithoutAssignStream) {
  auto n1 = graph_->AddNode("n1", "Op1");
  auto external_node = std::make_shared<DAGNode>("external", "Op2");
  ASSERT_EQ(graph_->AddEdge(n1, 0, external_node, 0), graphStatus::SUCCESS);

  StreamAllocConfig config{-1, 0, 0};
  DagStreamAllocator::ByPathCover(*graph_, config);

  EXPECT_EQ(config.required_streams, 0);
  EXPECT_EQ(n1->GetStreamId(), INVALID_STREAM_ID);
}

}  // namespace test
}  // namespace minidag
