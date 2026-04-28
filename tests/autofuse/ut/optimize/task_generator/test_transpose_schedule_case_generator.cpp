/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "gtest/gtest.h"
#include "ascendc_ir.h"
#include "ascir_ops_utils.h"
#include "ascir_utils.h"
#include "asc_graph_utils.h"
#include "task_generator/transpose_schedule_case_generator.h"
#include "asc_graph_builder.h"
#include "ascgraph_info_complete.h"

namespace schedule {
using namespace optimize;
using namespace ge;
using namespace ge::ops;
using ge::testing::AscGraphBuilder;
using ge::testing::Sym;

class TransposeScheduleCaseGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dlog_setlevel(ASCGEN_MODULE_NAME, DLOG_ERROR, 0);
  }
  void TearDown() override {
    dlog_setlevel(ASCGEN_MODULE_NAME, DLOG_ERROR, 0);
  }

  static void SetupTransposeSchedAxis(ge::AscGraph &graph) {
    std::vector<int64_t> loop_axes;
    for (auto node : graph.GetAllNodes()) {
      if (!node->attr.sched.axis.empty()) {
        loop_axes = node->attr.sched.axis;
        break;
      }
    }
    for (auto node : graph.GetAllNodes()) {
      if (ge::ops::IsOps<ge::ascir_op::Transpose>(node) && node->attr.sched.axis.empty()) {
        node->attr.sched.axis = loop_axes;
      }
    }
  }
};

// 两个Load各接一个Transpose，汇聚到Add → 只生成1个消除模板，无打分函数
TEST_F(TransposeScheduleCaseGeneratorTest, MultiTranspose_TwoBranchAdd) {
  auto s0 = Sym("s0"), s1 = Sym("s1"), s2 = Sym("s2");
  auto graph = AscGraphBuilder("multi_transpose_two_branch")
                   .Loops({s0, s1, s2})
                   .Data("data0", 0)
                   .Load("load0", "data0")
                   .Transpose("transpose0", "load0", {1, 0, 2})
                   .Data("data1", 1)
                   .Load("load1", "data1")
                   .Transpose("transpose1", "load1", {2, 1, 0})
                   .Add("add0", "transpose0", "transpose1")
                   .Store("store0", "add0")
                   .Output("out0", "store0", 0)
                   .Build();
  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  SetupTransposeSchedAxis(graph);

  optimize::TransposeFusionCaseGenerator generator;
  std::vector<ge::AscGraph> graphs;
  std::vector<std::string> score_functions;
  EXPECT_EQ(generator.Generate(graph, graphs, score_functions), ge::SUCCESS);
  ASSERT_EQ(graphs.size(), 1UL);
  EXPECT_TRUE(score_functions.empty());
}

// 两个Load各接一个Transpose（中间夹Abs），汇聚到Add，验证task结构
TEST_F(TransposeScheduleCaseGeneratorTest, MultiTranspose_TwoBranchAdd_TaskStructure) {
  auto s0 = Sym("s0"), s1 = Sym("s1"), s2 = Sym("s2");
  auto graph = AscGraphBuilder("multi_transpose_task")
                   .Loops({s0, s1, s2})
                   .Data("data0", 0)
                   .Load("load0", "data0")
                   .Transpose("transpose0", "load0", {1, 0, 2})
                   .Abs("abs0", "transpose0")
                   .Data("data1", 1)
                   .Load("load1", "data1")
                   .Transpose("transpose1", "load1", {2, 1, 0})
                   .Add("add0", "abs0", "transpose1")
                   .Store("store0", "add0")
                   .Output("out0", "store0", 0)
                   .Build();
  optimize::AscGraphInfoComplete::CompleteApiInfo(graph);
  SetupTransposeSchedAxis(graph);

  optimize::TransposeFusionCaseGenerator generator;
  std::vector<ScheduleTask> tasks;
  EXPECT_EQ(generator.GeneratorTask(graph, tasks, {}), ge::SUCCESS);
  ASSERT_EQ(tasks.size(), 1UL);
}

}  // namespace schedule
