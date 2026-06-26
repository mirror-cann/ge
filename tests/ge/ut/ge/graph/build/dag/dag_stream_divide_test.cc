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
#include <set>
#include <vector>
#include "graph/build/dag/dag_stream_divide.h"

namespace {

using minidag::HopcroftKarp;

class HopcroftKarpTest : public testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(HopcroftKarpTest, MinStream_SimpleGraph_ReturnsCorrectCover) {
  // 5 nodes, 2 edges: 1->2, 2->3
  HopcroftKarp hk(5);
  hk.AddEdge(1, 2 + 5, 1);  // u -> v + node_num
  hk.AddEdge(2, 3 + 5, 1);

  int cover = hk.MinStreamCover();
  std::vector<std::vector<int>> routes = hk.GetRoutes();

  // Minimum path cover = node_num - max_matching = 5 - 2 = 3
  EXPECT_EQ(cover, 3);
  EXPECT_EQ(routes.size(), 3);
}

TEST_F(HopcroftKarpTest, GetRoutes_ReturnsValidPaths) {
  HopcroftKarp hk(3);
  hk.AddEdge(1, 2 + 3, 1);

  hk.MinStreamCover();
  std::vector<std::vector<int>> routes = hk.GetRoutes();

  // Verify paths cover all nodes
  std::set<int> covered_nodes;
  for (const auto &route : routes) {
    for (int node_id : route) {
      covered_nodes.insert(node_id);
    }
  }
  EXPECT_EQ(covered_nodes.size(), 3);
}

TEST_F(HopcroftKarpTest, SingleNode_ReturnsOnePath) {
  HopcroftKarp hk(1);

  int cover = hk.MinStreamCover();
  std::vector<std::vector<int>> routes = hk.GetRoutes();

  EXPECT_EQ(cover, 1);
  EXPECT_EQ(routes.size(), 1);
  EXPECT_EQ(routes[0].size(), 1);
}

TEST_F(HopcroftKarpTest, EmptyGraph_NoCrash) {
  HopcroftKarp hk(0);

  int cover = hk.MinStreamCover();
  std::vector<std::vector<int>> routes = hk.GetRoutes();

  EXPECT_EQ(cover, 0);
  EXPECT_TRUE(routes.empty());
}

}  // namespace
