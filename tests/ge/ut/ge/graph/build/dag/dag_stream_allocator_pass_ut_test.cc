/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

#include <gtest/gtest.h>

#include "graph/build/stream/dag_stream_allocator_pass.h"
#include "graph/build/stream/dag_adapter.h"
#include "framework/common/ge_inner_error_codes.h"
#include "external/graph/graph.h"
#include "external/graph/operator.h"
#include "register/custom_pass_context_impl.h"
#include "graph/ge_context.h"
#include "graph/ge_local_context.h"

namespace minidag {

namespace {
struct GraphOptionGuard {
  ~GraphOptionGuard() {
    ge::GetThreadLocalContext().SetGraphOption({});
  }
};

class ProfilingFileGuard {
 public:
  ProfilingFileGuard() = default;
  ~ProfilingFileGuard() {
    if (!path_.empty()) {
      unsetenv("MINIDAG_PROFILING_PATH");
      std::remove(path_.c_str());
    }
  }

  bool CreateAndSet(const std::string &csv_content) {
    char temp_path[] = "/tmp/test_runpass_profiled_XXXXXX.csv";
    const std::string suffix = ".csv";
    const int suffix_len = static_cast<int>(suffix.size());
    int fd = mkstemps(temp_path, suffix_len);
    if (fd == -1) {
      return false;
    }
    close(fd);

    path_ = temp_path;
    std::ofstream file(path_);
    if (!file.is_open()) {
      return false;
    }
    file << csv_content;
    file.close();
    return setenv("MINIDAG_PROFILING_PATH", path_.c_str(), 1) == 0;
  }

 private:
  std::string path_;
};

// 测试辅助：构建简单的 Graph（仅包含节点和控制边）
ge::ConstGraphPtr BuildGraphWithNodes() {
  auto graph = std::make_shared<ge::Graph>("test_graph");

  ge::Operator data1_op("data1", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator netoutput_op("NetOutput", "NetOutput");

  (void)graph->AddNodeByOp(data1_op);
  (void)graph->AddNodeByOp(add1_op);
  (void)graph->AddNodeByOp(netoutput_op);

  return graph;
}

ge::ConstGraphPtr BuildGraphWithControlEdge() {
  auto graph = std::make_shared<ge::Graph>("control_edge_graph");

  ge::Operator data1_op("data1", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator relu1_op("relu1", "Relu");
  ge::Operator netoutput_op("NetOutput", "NetOutput");

  auto data1 = graph->AddNodeByOp(data1_op);
  auto add1 = graph->AddNodeByOp(add1_op);
  auto relu1 = graph->AddNodeByOp(relu1_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);

  graph->AddControlEdge(data1, relu1);
  graph->AddControlEdge(add1, netoutput);

  return graph;
}
}  // namespace

class DagStreamAllocatorPassTest : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

// --------------------
// 场景 A：RunMiniDAGStreamPass 正常执行
// --------------------

/**
 * 场景 A1: 正常图执行返回 SUCCESS
 */
TEST(DagStreamAllocatorPassTest, RunPass_Success) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);  // current_max_stream_id = 0
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 A2: 空图（nullptr）返回 FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_NullGraph) {
  ge::StreamPassContext context(0);  // current_max_stream_id = 0
  auto ret = RunMiniDAGStreamPass(nullptr, context);
  EXPECT_NE(ret, ge::SUCCESS);
}

/**
 * 场景 A3: 多次执行均返回 SUCCESS
 */
TEST(DagStreamAllocatorPassTest, RunPass_MultipleExecution) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph1 = BuildGraphWithNodes();
  auto graph2 = BuildGraphWithControlEdge();
  ASSERT_NE(graph1, nullptr);
  ASSERT_NE(graph2, nullptr);

  ge::StreamPassContext context1(0), context2(0);

  auto ret1 = RunMiniDAGStreamPass(graph1, context1);
  auto ret2 = RunMiniDAGStreamPass(graph2, context2);

  EXPECT_EQ(ret1, ge::SUCCESS);
  EXPECT_EQ(ret2, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 A4: 仅包含 Data/NetOutput 的图返回 SUCCESS（空 DAG）
 */
TEST(DagStreamAllocatorPassTest, RunPass_EmptyDAG) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithNodes();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);  // current_max_stream_id = 0
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

// --------------------
// 场景 B：基础功能测试
// --------------------

/**
 * 场景 B1: 空图多次执行
 */
TEST(DagStreamAllocatorPassTest, RunPass_EmptyGraphMultipleTimes) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithNodes();
  ASSERT_NE(graph, nullptr);

  for (int i = 0; i < 3; ++i) {
    ge::StreamPassContext context(i);
    auto ret = RunMiniDAGStreamPass(graph, context);
    EXPECT_EQ(ret, ge::SUCCESS);
  }

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 B2: 带 Data 节点的图
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithDataNode) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("data_test_graph");
  ge::Operator data_op("data1", "Data");
  ge::Operator add_op("add1", "Add");
  (void)graph->AddNodeByOp(data_op);
  (void)graph->AddNodeByOp(add_op);
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 B3: 仅 NetOutput 节点
 */
TEST(DagStreamAllocatorPassTest, RunPass_OnlyNetOutput) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("netoutput_only_graph");
  ge::Operator netoutput_op("NetOutput", "NetOutput");
  (void)graph->AddNodeByOp(netoutput_op);
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 B4: 多节点复杂图
 */
TEST(DagStreamAllocatorPassTest, RunPass_ComplexMultiNodeGraph) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("complex_graph");

  ge::Operator data1("data1", "Data");
  ge::Operator data2("data2", "Data");
  ge::Operator add1("add1", "Add");
  ge::Operator add2("add2", "Add");
  ge::Operator mul1("mul1", "Mul");
  ge::Operator netoutput("NetOutput", "NetOutput");

  (void)graph->AddNodeByOp(data1);
  (void)graph->AddNodeByOp(data2);
  (void)graph->AddNodeByOp(add1);
  (void)graph->AddNodeByOp(add2);
  (void)graph->AddNodeByOp(mul1);
  (void)graph->AddNodeByOp(netoutput);

  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 B5: 带 Relu 节点的图
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithReluNode) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("relu_graph");

  ge::Operator data("data", "Data");
  ge::Operator relu("relu", "Relu");
  ge::Operator netoutput("NetOutput", "NetOutput");

  (void)graph->AddNodeByOp(data);
  (void)graph->AddNodeByOp(relu);
  (void)graph->AddNodeByOp(netoutput);

  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 B6: 不同 context stream id
 */
TEST(DagStreamAllocatorPassTest, RunPass_DifferentStreamId) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  for (int stream_id = 0; stream_id < 5; ++stream_id) {
    ge::StreamPassContext context(stream_id);
    auto ret = RunMiniDAGStreamPass(graph, context);
    EXPECT_EQ(ret, ge::SUCCESS);
  }

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 B7: 带 Sigmoid 节点的图
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithSigmoidNode) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("sigmoid_graph");

  ge::Operator data("data", "Data");
  ge::Operator sigmoid("sigmoid", "Sigmoid");
  ge::Operator netoutput("NetOutput", "NetOutput");

  (void)graph->AddNodeByOp(data);
  (void)graph->AddNodeByOp(sigmoid);
  (void)graph->AddNodeByOp(netoutput);

  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 B8: 仅 Data 节点
 */
TEST(DagStreamAllocatorPassTest, RunPass_OnlyDataNode) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = std::make_shared<ge::Graph>("data_only_graph");
  ge::Operator data_op("data_only", "Data");
  (void)graph->AddNodeByOp(data_op);
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

// --------------------
// 场景 C：ge.autoMultistreamParallelMode 配置测试
// --------------------

/**
 * 场景 C1: 设置 ge.autoMultistreamParallelMode="LoadBalance:8" - 解析冒号格式
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_LoadBalance) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C1.1: profiling 文件命中节点时，stream pass 成功走通入口路径
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithProfiledNodeCost_EntryPathSucceeds) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);
  GraphOptionGuard graph_option_guard;

  ProfilingFileGuard profiling_file_guard;
  ASSERT_TRUE(
      profiling_file_guard.CreateAndSet("Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
                                        "add1,AI_CORE,88.0,8,0\n"));

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);
}

/**
 * 场景 C1.2: profiling 文件未命中节点时，stream pass 成功走通入口路径
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithUnmatchedProfiling_EntryPathSucceeds) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:8";
  ge::GetThreadLocalContext().SetGraphOption(options);
  GraphOptionGuard graph_option_guard;

  ProfilingFileGuard profiling_file_guard;
  ASSERT_TRUE(
      profiling_file_guard.CreateAndSet("Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
                                        "other_node,AI_CORE,21.0,4,0\n"));

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);
}

/**
 * 场景 C2: 设置 ge.autoMultistreamParallelMode="MainStream:6" - 解析冒号格式+MainStream策略
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_MainStream) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "MainStream:6";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C2-1: 设置 ge.autoMultistreamParallelMode="WeightedLoadBalance:6" - 解析冒号格式+WeightedLoadBalance策略
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_WeightedLoadBalance) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "WeightedLoadBalance:6";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C3: 设置 ge.autoMultistreamParallelMode="LoadBalance:invalid" - 无效max_stream值，返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_InvalidMax) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:invalid";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C4: 设置 ge.autoMultistreamParallelMode="LoadBalance" - 无冒号格式，使用默认8条流
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_LegacyFormat) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C4-1: 设置 ge.autoMultistreamParallelMode="WeightedLoadBalance" - 无冒号格式，返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_WeightedLoadBalanceLegacyFormat) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "WeightedLoadBalance";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C5: 设置 ge.autoMultistreamParallelMode="LoadBalance:0" - max_val <= 0，返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_ZeroMax) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:0";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C6: 设置 ge.autoMultistreamParallelMode="LoadBalance:-5" - max_val为负数，返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_NegativeMax) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:-5";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C9: ge.autoMultistreamParallelMode 超大 max_stream 值，strtol 溢出返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_HugeMax) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:99999999999999999999";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C10: ge.autoMultistreamParallelMode="LoadBalance:1" - 合法值下边界测试
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_ValidLowerBoundary) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:1";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C11: ge.autoMultistreamParallelMode="LoadBalance:64" - 合法值上边界测试
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_ValidUpperBoundary) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:64";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::SUCCESS);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 C12: ge.autoMultistreamParallelMode="LoadBalance:65" - 超过上限64，返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_WithAutoMultistreamMode_ExceedsMax) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:65";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

// --------------------
// 场景 D：ParseStreamConfig 异常场景
// --------------------

/**
 * 场景 D1: 无冒号分隔符（非"LoadBalance"字符串），返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_MissingColonSeparator) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "MainStream";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 D2: 无冒号分隔符（随机字符串），返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_MissingColonSeparator_RandomString) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "RandomString";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 D3: algo 名称为空（冒号在最前面），返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_EmptyAlgoName) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = ":8";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 D4: 未知的 algo 名称，返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_UnknownAlgoName) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "UnknownAlgo:4";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}

/**
 * 场景 D5: max_stream 为空字符串（冒号在最后面），返回FAILED
 */
TEST(DagStreamAllocatorPassTest, RunPass_EmptyMaxStream) {
  std::map<std::string, std::string> options;
  options["ge.autoMultistreamParallelMode"] = "LoadBalance:";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto graph = BuildGraphWithControlEdge();
  ASSERT_NE(graph, nullptr);

  ge::StreamPassContext context(0);
  auto ret = RunMiniDAGStreamPass(graph, context);
  EXPECT_EQ(ret, ge::FAILED);

  ge::GetThreadLocalContext().SetGraphOption({});
}
}  // namespace minidag
