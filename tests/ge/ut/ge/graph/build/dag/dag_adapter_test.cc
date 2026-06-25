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
#include <ge_running_env/ge_running_env_faker.h>
#include <ge_running_env/fake_op.h>
#include <common/share_graph.h>
#include "api/gelib/gelib.h"
#include "engines/manager/engine_manager/dnnengine_manager.h"
#include <graph_utils_ex.h>

#include "graph/build/stream/dag_adapter.h"
#include "graph/build/dag/dag_types.h"
#include "framework/common/ge_inner_error_codes.h"
#include "external/graph/graph.h"
#include "external/graph/operator.h"
#include "graph/build/stream/stream_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/node_adapter.h"
#include "register/custom_pass_context_impl.h"
#include "graph/ge_local_context.h"
#include "external/ge_common/ge_common_api_types.h"
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
#include <nlohmann/json.hpp>

#include "depends/ascendcl/src/ascendcl_stub.h"

namespace ge {

// 测试辅助函数 - 模拟 dag_adapter.cc 中的私有函数
namespace {
graphStatus CallFromGEGraph(const ConstGraphPtr &ge_graph,
                            std::shared_ptr<minidag::DAGGraph> &dag) {
  bool has_profiled_node_cost = false;
  return DAGAdapter::FromGEGraph(ge_graph, dag, has_profiled_node_cost);
}

std::string FindLatestFileInDir(const std::string &dir_path, const std::string &prefix) {
  DIR *dir = opendir(dir_path.c_str());
  if (dir == nullptr) {
    return "";
  }
  
  std::vector<std::string> matching_files;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_REG) {
      std::string filename = entry->d_name;
      if (filename.find(prefix) == 0) {
        matching_files.push_back(filename);
      }
    }
  }
  closedir(dir);
  
  if (matching_files.empty()) {
    return "";
  }
  
  std::sort(matching_files.begin(), matching_files.end());
  return matching_files.back();
}

std::string FindLatestProfDir(const std::string &base_path) {
  DIR *dir = opendir(base_path.c_str());
  if (dir == nullptr) {
    return "";
  }
  
  std::vector<std::string> prof_dirs;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_DIR) {
      std::string dirname = entry->d_name;
      if (dirname.find("PROF_") == 0 && dirname != "." && dirname != "..") {
        prof_dirs.push_back(dirname);
      }
    }
  }
  closedir(dir);
  
  if (prof_dirs.empty()) {
    return "";
  }
  
  std::sort(prof_dirs.begin(), prof_dirs.end());
  return prof_dirs.back();
}

std::string BuildOpSummaryPath(const std::string &output_dir) {
  if (output_dir.empty()) {
    return "";
  }
  
  std::string prof_dir = FindLatestProfDir(output_dir);
  if (prof_dir.empty()) {
    return "";
  }
  
  std::string profiler_output_path = output_dir + "/" + prof_dir + "/mindstudio_profiler_output";
  DIR *profiler_dir = opendir(profiler_output_path.c_str());
  if (profiler_dir == nullptr) {
    return "";
  }
  closedir(profiler_dir);
  
  std::string op_summary_file = FindLatestFileInDir(profiler_output_path, "op_summary_");
  if (op_summary_file.empty()) {
    return "";
  }
  
  return profiler_output_path + "/" + op_summary_file;
}

void CreateMockProfilingStructure(const std::string &base_path, const std::string &prof_dir_name,
                                   const std::string &csv_filename) {
  mkdir(base_path.c_str(), 0755);
  std::string prof_path = base_path + "/" + prof_dir_name;
  mkdir(prof_path.c_str(), 0755);
  std::string output_path = prof_path + "/mindstudio_profiler_output";
  mkdir(output_path.c_str(), 0755);
  
  std::string csv_path = output_path + "/" + csv_filename;
  std::ofstream file(csv_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,AI_CORE,100.0,8,0\n";
  file.close();
}

void CleanupMockProfilingStructure(const std::string &base_path) {
  auto remove_dir = [](const std::string &path) {
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_type == DT_REG) {
        std::string filepath = path + "/" + entry->d_name;
        std::remove(filepath.c_str());
      }
    }
    closedir(dir);
    rmdir(path.c_str());
  };
  
  std::string prof_path = base_path + "/" + FindLatestProfDir(base_path);
  if (!prof_path.empty() && prof_path != base_path) {
    std::string output_path = prof_path + "/mindstudio_profiler_output";
    remove_dir(output_path);
    rmdir(prof_path.c_str());
  }
  rmdir(base_path.c_str());
}
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

ge::ConstGraphPtr BuildEmptyGraph() {
  return std::make_shared<ge::Graph>("empty_graph");
}

ge::ConstGraphPtr BuildMultiNodeGraph() {
  auto graph = std::make_shared<ge::Graph>("multi_node_graph");
  ge::Operator data1_op("data1", "Data");
  ge::Operator data2_op("data2", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator relu1_op("relu1", "Relu");
  ge::Operator netoutput_op("NetOutput", "NetOutput");
  auto data1 = graph->AddNodeByOp(data1_op);
  auto data2 = graph->AddNodeByOp(data2_op);
  auto add1 = graph->AddNodeByOp(add1_op);
  auto relu1 = graph->AddNodeByOp(relu1_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);
  graph->AddControlEdge(data1, add1);
  graph->AddControlEdge(data2, add1);
  graph->AddControlEdge(add1, relu1);
  graph->AddControlEdge(relu1, netoutput);
  return graph;
}

void SetStreamLabel(const ge::GNode &gnode, const std::string &stream_label) {
  auto node = NodeAdapter::GNode2Node(gnode);
  ASSERT_NE(node, nullptr);
  ASSERT_NE(node->GetOpDesc(), nullptr);
  ASSERT_TRUE(AttrUtils::SetStr(node->GetOpDesc(), ATTR_NAME_STREAM_LABEL, stream_label));
}

ge::ConstGraphPtr BuildGraphWithStreamLabel() {
  auto graph = std::make_shared<ge::Graph>("stream_label_graph");
  ge::Operator data1_op("data1", "Data");
  ge::Operator add1_op("add1", "Add");
  ge::Operator relu1_op("relu1", "Relu");
  ge::Operator pool1_op("pool1", "Pool");
  ge::Operator abs1_op("abs1", "Abs");
  ge::Operator netoutput_op("NetOutput", "NetOutput");

  auto data1 = graph->AddNodeByOp(data1_op);
  auto add1 = graph->AddNodeByOp(add1_op);
  auto relu1 = graph->AddNodeByOp(relu1_op);
  auto pool1 = graph->AddNodeByOp(pool1_op);
  auto abs1 = graph->AddNodeByOp(abs1_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);
  SetStreamLabel(add1, "serial_label");
  SetStreamLabel(relu1, "serial_label");
  SetStreamLabel(pool1, "another_label");
  SetStreamLabel(abs1, "");
  graph->AddControlEdge(data1, add1);
  graph->AddControlEdge(add1, relu1);
  graph->AddControlEdge(relu1, pool1);
  graph->AddControlEdge(pool1, abs1);
  graph->AddControlEdge(abs1, netoutput);
  return graph;
}
}  // namespace

class DAGAdapterToGEStatusTest : public testing::Test {};

// --------------------
// 场景 1：ToGEStatus 状态映射测试
// --------------------

/**
 * 场景 1-1: 所有 graphStatus 到 ge::graphStatus 的映射
 * 验证所有已定义的 graphStatus 值都能正确映射
 */
TEST_F(DAGAdapterToGEStatusTest, ToGEStatusAllMappings) {
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::SUCCESS), ge::GRAPH_SUCCESS);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::FAILED), ge::GRAPH_FAILED);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::INVALID_NODE), ge::GRAPH_FAILED);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::INVALID_EDGE), ge::GRAPH_FAILED);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::NODE_NOT_FOUND), ge::GE_GRAPH_GRAPH_NODE_NULL);
  EXPECT_EQ(DAGAdapter::ToGEStatus(minidag::graphStatus::DUPLICATE_NODE), ge::GRAPH_FAILED);
}

/**
 * 场景 1-2: 未知错误码映射
 * 验证未知错误码正确映射为 ge::GRAPH_FAILED
 */
TEST_F(DAGAdapterToGEStatusTest, ToGEStatusUnknown) {
  EXPECT_EQ(DAGAdapter::ToGEStatus(static_cast<minidag::graphStatus>(999)), ge::GRAPH_FAILED);
  EXPECT_EQ(DAGAdapter::ToGEStatus(static_cast<minidag::graphStatus>(-1)), ge::GRAPH_FAILED);
}

class DAGAdapterGEIntegrationTest : public testing::Test {};

// --------------------
// 场景 2：FromGEGraph 基本测试
// --------------------

/**
 * 场景 2-1: 节点数量正确
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NodeCount) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), 3);
}

/**
 * 场景 2-2: 控制边转换正确
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_ControlEdgeCount) {
  auto ge_graph = BuildGraphWithControlEdge();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetEdgeCount(), 2);
}

/**
 * 场景 2-3: 拓扑序正确
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_TopoOrder) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  EXPECT_EQ(nodes.size(), 3);
  EXPECT_EQ(nodes[0]->GetTopoId(), 0);
  EXPECT_EQ(nodes[1]->GetTopoId(), 1);
  EXPECT_EQ(nodes[2]->GetTopoId(), 2);
}

/**
 * 场景 2-4: 节点类型正确
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NodeTypes) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto data1 = dag->FindNode("data1");
  ASSERT_NE(data1, nullptr);
  EXPECT_EQ(data1->GetType(), "Data");

  auto add1 = dag->FindNode("add1");
  ASSERT_NE(add1, nullptr);
  EXPECT_EQ(add1->GetType(), "Add");
}

/**
 * 场景 2-5: 多节点图转换
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_MultiNodeGraph) {
  auto ge_graph = BuildMultiNodeGraph();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), 5);
  EXPECT_EQ(dag->GetEdgeCount(), 4);
}

/**
 * 场景 2-6: GE stream_label 转换为 minidag 串行标记
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_StreamLabelConvertedToSerialFlag) {
  auto ge_graph = BuildGraphWithStreamLabel();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto add1 = dag->FindNode("add1");
  auto relu1 = dag->FindNode("relu1");
  auto pool1 = dag->FindNode("pool1");
  auto abs1 = dag->FindNode("abs1");
  auto data1 = dag->FindNode("data1");
  ASSERT_NE(add1, nullptr);
  ASSERT_NE(relu1, nullptr);
  ASSERT_NE(pool1, nullptr);
  ASSERT_NE(abs1, nullptr);
  ASSERT_NE(data1, nullptr);
  EXPECT_EQ(add1->GetSerialFlag(), "serial_label");
  EXPECT_EQ(relu1->GetSerialFlag(), "serial_label");
  EXPECT_EQ(pool1->GetSerialFlag(), "another_label");
  EXPECT_TRUE(abs1->GetSerialFlag().empty());
  EXPECT_TRUE(data1->GetSerialFlag().empty());
}

// --------------------
// 场景 3：边界/异常场景
// --------------------

/**
 * 场景 3-1: 空图转换
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_EmptyGraph) {
  auto ge_graph = BuildEmptyGraph();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), 0);
  EXPECT_EQ(dag->GetEdgeCount(), 0);
}

/**
 * 场景 3-2: null 输入
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NullInput) {
  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(nullptr, dag);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 3-3: DAGGraph 重复节点报错
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGGraph_DuplicateNode) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  ASSERT_NE(node1, nullptr);
  auto node2 = dag->AddNode("node1", "Relu");
  EXPECT_EQ(node2, nullptr);
}

/**
 * 场景 3-4: DAGGraph 边添加失败
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGGraph_AddEdge_NullNode) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  minidag::graphStatus ret = dag->AddEdge(nullptr, 0, node2, 0);
  EXPECT_EQ(ret, minidag::graphStatus::INVALID_NODE);

  ret = dag->AddEdge(node1, 0, nullptr, 0);
  EXPECT_EQ(ret, minidag::graphStatus::INVALID_NODE);
}

// --------------------
// 场景 4：节点边关系测试
// --------------------

/**
 * 场景 4-1: 控制边端口验证（端口值都为 -1）
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_ControlEdgePortValues) {
  auto ge_graph = BuildGraphWithControlEdge();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto edges = dag->GetAllEdges();
  EXPECT_EQ(edges.size(), 2);
  for (const auto& edge : edges) {
    EXPECT_EQ(edge->GetSrcPort(), -1);
    EXPECT_EQ(edge->GetDstPort(), -1);
  }
}

/**
 * 场景 4-2: 节点输入输出边数量
 * 图结构: data1 -> relu1, add1 -> netoutput
 * data1: 输出=1, 输入=0
 * relu1: 输入=1, 输出=0
 * add1: 输出=1, 输入=0
 * netoutput: 输入=1, 输出=0
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NodeInputOutputCount) {
  auto ge_graph = BuildGraphWithControlEdge();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto data1 = dag->FindNode("data1");
  ASSERT_NE(data1, nullptr);
  EXPECT_EQ(data1->GetInputCount(), 0);
  EXPECT_EQ(data1->GetOutputCount(), 1);

  auto relu1 = dag->FindNode("relu1");
  ASSERT_NE(relu1, nullptr);
  EXPECT_EQ(relu1->GetInputCount(), 1);
  EXPECT_EQ(relu1->GetOutputCount(), 0);
}

/**
 * 场景 4-3: 多节点图连接验证
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_MultiNodeConnections) {
  auto ge_graph = BuildMultiNodeGraph();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto add1 = dag->FindNode("add1");
  ASSERT_NE(add1, nullptr);
  EXPECT_EQ(add1->GetInputCount(), 2);

  auto relu1 = dag->FindNode("relu1");
  ASSERT_NE(relu1, nullptr);
  auto relu_input_nodes = relu1->GetInputNodes();
  EXPECT_EQ(relu_input_nodes.size(), 1);
  EXPECT_EQ(relu_input_nodes[0]->GetName(), "add1");
}

// --------------------
// 场景 5：节点属性验证
// --------------------

/**
 * 场景 5-1: 节点 StreamId 默认值
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_NodeStreamIdDefault) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  for (const auto& node : nodes) {
    EXPECT_EQ(node->GetStreamId(), INVALID_STREAM_ID);
  }
}

/**
 * 场景 5-2: 节点 StreamId 设置验证
 */
TEST_F(DAGAdapterGEIntegrationTest, NodeStreamIdSetGet) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node = dag->AddNode("node1", "Conv");
  ASSERT_NE(node, nullptr);

  EXPECT_EQ(node->GetStreamId(), INVALID_STREAM_ID);
  node->SetStreamId(5);
  EXPECT_EQ(node->GetStreamId(), 5);
}

/**
 * 场景 5-3: 节点 Cost 属性
 */
TEST_F(DAGAdapterGEIntegrationTest, NodeCostSetGet) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node = dag->AddNode("node1", "Conv");
  ASSERT_NE(node, nullptr);

  minidag::NodeCost cost;
  cost.execution_time = 100.0;
  cost.memory_usage = 1024;
  node->SetCost(cost);

  const auto& retrieved_cost = node->GetCost();
  EXPECT_EQ(retrieved_cost.execution_time, 100.0);
  EXPECT_EQ(retrieved_cost.memory_usage, 1024);
}

/**
 * 场景 5-4: DeviceResourceInfo 默认值
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGGraph_DeviceResourceDefault) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");

  const auto& resource = dag->GetDeviceResource();
  EXPECT_EQ(resource.cube_core_num, -1);
  EXPECT_EQ(resource.vector_core_num, -1);
}

/**
 * 场景 5-5: DeviceResourceInfo 设置验证
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGGraph_DeviceResourceSetGet) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");

  minidag::DeviceResourceInfo resource;
  resource.cube_core_num = 30;
  resource.vector_core_num = 8;

  dag->SetDeviceResource(resource);

  const auto& retrieved = dag->GetDeviceResource();
  EXPECT_EQ(retrieved.cube_core_num, 30);
  EXPECT_EQ(retrieved.vector_core_num, 8);
}

// --------------------
// 场景 6：RefreshStreamIdsToGE 测试
// --------------------

/**
 * 场景 6-1: null graph 返回失败
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_NullGraph) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");
  ge::StreamPassContext context(10);
  auto ret = DAGAdapter::RefreshStreamIdsToGE(*dag, nullptr, context);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-2: 空 DAG 返回成功
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_EmptyDAG) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);
  auto empty_dag = std::make_shared<minidag::DAGGraph>("empty_dag");
  ge::StreamPassContext context(10);
  auto ret = DAGAdapter::RefreshStreamIdsToGE(*empty_dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-3: 正常流程
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_NormalFlow) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  for (auto& node : nodes) {
    node->SetStreamId(0);
  }

  ge::StreamPassContext context(0);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-4: INVALID_STREAM_ID 节点跳过
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_InvalidStreamId) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  for (auto& node : nodes) {
    node->SetStreamId(INVALID_STREAM_ID);
  }

  ge::StreamPassContext context(10);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-5: 节点不在 GE 图中时跳过
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_NodeNotInGE) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node = dag->AddNode("nonexistent_node", "Conv");
  node->SetStreamId(0);

  ge::StreamPassContext context(0);
  auto ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 6-6: stream_id 超出范围时返回失败
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_StreamIdOutOfRange) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  for (auto& node : nodes) {
    node->SetStreamId(100);
  }

  ge::StreamPassContext context(0);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_FAILED);
}

/**
 * 场景 6-7: GE 节点原始 stream_id 为 INVALID 时跳过刷新
 * 验证：DAG 节点有有效 stream_id，但 GE 节点原始 stream_id 为 INVALID_STREAM_ID 时，
 *       该节点不会被刷新，函数返回成功
 */
TEST_F(DAGAdapterGEIntegrationTest, RefreshStreamIdsToGE_OriginStreamIdInvalid) {
  auto ge_graph = BuildMultiNodeGraph();
  ASSERT_NE(ge_graph, nullptr);

  // 为部分 GE 节点设置有效 stream_id，部分保持 INVALID
  auto gnodes = ge_graph->GetDirectNode();
  for (const auto& gnode : gnodes) {
    AscendString name;
    gnode.GetName(name);
    std::string node_name(name.GetString());
    auto compute_node = NodeAdapter::GNode2Node(gnode);
    ASSERT_NE(compute_node, nullptr);
    if (node_name == "add1") {
      // add1 设置有效 stream_id
      compute_node->GetOpDesc()->SetStreamId(0);
    } else {
      compute_node->GetOpDesc()->SetStreamId(-1);
    }
  }

  // 转换为 DAG
  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  // DAG 节点全部设置有效 stream_id（模拟 CustomStreamPass）
  auto dag_nodes = dag->GetAllNodes();
  for (auto& node : dag_nodes) {
    node->SetStreamId(1);
  }

  ge::StreamPassContext context(10);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  // 验证：只有 add1 被刷新为 1，其他节点保持 INVALID
  for (const auto& gnode : gnodes) {
    AscendString name;
    gnode.GetName(name);
    std::string node_name(name.GetString());
    auto compute_node = NodeAdapter::GNode2Node(gnode);
    ASSERT_NE(compute_node, nullptr);
    int64_t stream_id = compute_node->GetOpDesc()->GetStreamId();
    if (node_name == "add1") {
      EXPECT_EQ(stream_id, 1);  // 被刷新
    } else if (node_name != "data1" && node_name != "data2" && node_name != "NetOutput") {
      // relu1 原始为 INVALID，应被跳过
      EXPECT_EQ(stream_id, INVALID_STREAM_ID);
    }
  }
}

// --------------------
// 场景 7：私有函数测试（覆盖率）
// --------------------

/**
 * 场景 7-1: ConvertNodes 重复节点测试
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertNodes_DuplicateNode) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("data1", "Data");
  bool has_profiled_node_cost = false;

  auto ret = DAGAdapter::ConvertNodes(ge_graph, *dag, has_profiled_node_cost);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 7-2: ConvertEdges DAG 缺少节点测试
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertEdges_NodeNotInDAG) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto ret = DAGAdapter::ConvertEdges(ge_graph, *dag);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 7-3: ConvertNodes 空图测试
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertNodes_EmptyGraph) {
  auto ge_graph = BuildEmptyGraph();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("empty_dag");
  bool has_profiled_node_cost = false;
  auto ret = DAGAdapter::ConvertNodes(ge_graph, *dag, has_profiled_node_cost);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_EQ(dag->GetNodeCount(), 0);
}

/**
 * 场景 7-4: ConvertEdges 空图测试
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertEdges_EmptyGraph) {
  auto ge_graph = BuildEmptyGraph();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("empty_dag");
  auto ret = DAGAdapter::ConvertEdges(ge_graph, *dag);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_EQ(dag->GetEdgeCount(), 0);
}

/**
 * 场景 7-5: ConvertControlEdgesForNode 目标节点未找到
 */
TEST_F(DAGAdapterGEIntegrationTest, ConvertControlEdgesForNode_DstNotFound) {
  auto ge_graph = BuildGraphWithControlEdge();
  ASSERT_NE(ge_graph, nullptr);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("data1", "Data");

  auto src_node = dag->FindNode("data1");
  ASSERT_NE(src_node, nullptr);

  auto gnodes = ge_graph->GetDirectNode();
  for (const auto& gnode : gnodes) {
    ge::AscendString name;
    gnode.GetName(name);
    if (std::string(name.GetString()) == "data1") {
      int64_t edge_count = 0;
      auto ret = DAGAdapter::ConvertControlEdgesForNode(gnode, src_node, *dag, edge_count);
      EXPECT_NE(ret, ge::GRAPH_SUCCESS);
      break;
    }
  }
}

/**
 * 场景 7-6: DAGGraph GetAllEdges
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGGraph_GetAllEdges) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  dag->AddEdge(node1, 0, node2, 0);

  auto edges = dag->GetAllEdges();
  EXPECT_EQ(edges.size(), 1);
  EXPECT_EQ(edges[0]->GetSrcNode()->GetName(), "node1");
  EXPECT_EQ(edges[0]->GetDstNode()->GetName(), "node2");
}

/**
 * 场景 7-7: DAGNode GetInputNodes/GetOutputNodes
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGNode_GetInputOutputNodes) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  dag->AddEdge(node1, 0, node2, 0);

  auto inputs = node2->GetInputNodes();
  EXPECT_EQ(inputs.size(), 1);
  EXPECT_EQ(inputs[0]->GetName(), "node1");

  auto outputs = node1->GetOutputNodes();
  EXPECT_EQ(outputs.size(), 1);
  EXPECT_EQ(outputs[0]->GetName(), "node2");
}

/**
 * 场景 7-8: DAGNode GetInputEdges/GetOutputEdges
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGNode_GetInputOutputEdges) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  dag->AddEdge(node1, 0, node2, 1);

  auto out_edges = node1->GetOutputEdges();
  EXPECT_EQ(out_edges.size(), 1);
  EXPECT_EQ(out_edges[0]->GetSrcPort(), 0);
  EXPECT_EQ(out_edges[0]->GetDstPort(), 1);

  auto in_edges = node2->GetInputEdges();
  EXPECT_EQ(in_edges.size(), 1);
}

/**
 * 场景 7-9: DAGEdge GetSrcNode/GetDstNode
 */
TEST_F(DAGAdapterGEIntegrationTest, DAGEdge_GetSrcDstNode) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  auto node1 = dag->AddNode("node1", "Conv");
  auto node2 = dag->AddNode("node2", "Relu");

  dag->AddEdge(node1, 0, node2, 0);

  auto edges = dag->GetAllEdges();
  EXPECT_EQ(edges.size(), 1);
  EXPECT_EQ(edges[0]->GetSrcNode()->GetName(), "node1");
  EXPECT_EQ(edges[0]->GetDstNode()->GetName(), "node2");
}

// ============================================================================
// 数据边测试（需要算子原型注册）
// ============================================================================

class DAGAdapterDataEdgeTest : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    std::map<std::string, std::string> options;
    EXPECT_EQ(ge::GELib::Initialize(options), ge::SUCCESS);
    ge::GELib::GetInstance()->OpsKernelManagerObj().ops_kernel_store_.clear();

    ge::GeRunningEnvFaker().Reset()
      .Install(ge::FakeEngine("DNN_VM_GE_LOCAL").KernelInfoStore("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeEngine("AIcoreEngine").KernelInfoStore("AIcoreEngine"))
      .Install(ge::FakeOp(ge::DATA).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeOp(ge::NETOUTPUT).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeOp("Add").InfoStoreAndBuilder("AIcoreEngine"));

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

  static ge::ConstGraphPtr ToConstGraphPtr(const ge::ComputeGraphPtr& compute_graph) {
    return ge::GraphUtilsEx::CreateGraphPtrFromComputeGraph(compute_graph);
  }
};

/**
 * 场景 8-1: 数据边转换测试
 */
TEST_F(DAGAdapterDataEdgeTest, FromGEGraph_DataEdges) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  EXPECT_GT(dag->GetEdgeCount(), 0);
}

/**
 * 场景 8-2: 数据边数量验证
 */
TEST_F(DAGAdapterDataEdgeTest, FromGEGraph_DataEdgeCount) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  size_t original_data_edges = 0;
  for (const auto& node : compute_graph->GetAllNodes()) {
    original_data_edges += node->GetOutDataNodes().size();
  }

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  EXPECT_EQ(dag->GetEdgeCount(), original_data_edges);
}

// --------------------
// 场景 9：FillDeviceResource 错误路径测试
// --------------------

/**
 * 场景 9-1: GetThreadLocalContext 获取 soc_version 失败时返回默认值
 */
TEST_F(DAGAdapterGEIntegrationTest, FillDeviceResource_SocVersionNotSet) {
  ge::GetThreadLocalContext().SetGraphOption({});

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");

  auto ret = DAGAdapter::FillDeviceResource(*dag);

  EXPECT_EQ(ret, SUCCESS);
  const auto &resource = dag->GetDeviceResource();
  EXPECT_EQ(resource.cube_core_num, -1);
  EXPECT_EQ(resource.vector_core_num, -1);
}

/**
 * 场景 9-2: 正常获取 PlatformInfo 并填充资源信息
 */
TEST_F(DAGAdapterGEIntegrationTest, FillDeviceResource_Success) {
  std::map<std::string, std::string> options;
  options[ge::SOC_VERSION] = "Ascend910B1";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");

  auto ret = DAGAdapter::FillDeviceResource(*dag);

  EXPECT_EQ(ret, SUCCESS);
  const auto &resource = dag->GetDeviceResource();
  // stub 实现固定返回 32
  EXPECT_EQ(resource.cube_core_num, 32);
  EXPECT_EQ(resource.vector_core_num, 32);
}

/**
 * 场景 9-3: FromGEGraph 在 FillDeviceResource 场景下正常返回
 */
TEST_F(DAGAdapterGEIntegrationTest, FromGEGraph_FillDeviceResourceSuccess) {
  std::map<std::string, std::string> options;
  options[ge::SOC_VERSION] = "Ascend910B1";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  const auto &resource = dag->GetDeviceResource();
  EXPECT_EQ(resource.cube_core_num, 32);
  EXPECT_EQ(resource.vector_core_num, 32);
}

// --------------------
// 场景 10：路径拼接辅助函数测试
// --------------------

class DAGAdapterPathUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    test_base_path_ = "/tmp/test_profiling_" + std::to_string(getpid());
  }
  
  void TearDown() override {
    CleanupMockProfilingStructure(test_base_path_);
  }
  
  std::string test_base_path_;
};

/**
 * 场景 10-1: FindLatestFileInDir 正常查找
 */
TEST_F(DAGAdapterPathUtilsTest, FindLatestFileInDir_Normal) {
  std::string dir_path = "/tmp/test_find_files_" + std::to_string(getpid());
  mkdir(dir_path.c_str(), 0755);
  
  std::ofstream file1(dir_path + "/op_summary_20260101.csv");
  file1 << "test1\n";
  file1.close();
  
  std::ofstream file2(dir_path + "/op_summary_20260202.csv");
  file2 << "test2\n";
  file2.close();
  
  std::ofstream file3(dir_path + "/other_file.txt");
  file3 << "test3\n";
  file3.close();
  
  std::string result = FindLatestFileInDir(dir_path, "op_summary_");
  EXPECT_EQ(result, "op_summary_20260202.csv");
  
  std::remove((dir_path + "/op_summary_20260101.csv").c_str());
  std::remove((dir_path + "/op_summary_20260202.csv").c_str());
  std::remove((dir_path + "/other_file.txt").c_str());
  rmdir(dir_path.c_str());
}

/**
 * 场景 10-2: FindLatestFileInDir 目录不存在返回空
 */
TEST_F(DAGAdapterPathUtilsTest, FindLatestFileInDir_DirNotExist) {
  std::string result = FindLatestFileInDir("/tmp/nonexistent_dir_12345", "op_summary_");
  EXPECT_TRUE(result.empty());
}

/**
 * 场景 10-3: FindLatestFileInDir 无匹配文件返回空
 */
TEST_F(DAGAdapterPathUtilsTest, FindLatestFileInDir_NoMatchingFiles) {
  std::string dir_path = "/tmp/test_find_no_files_" + std::to_string(getpid());
  mkdir(dir_path.c_str(), 0755);
  
  std::ofstream file1(dir_path + "/other_file.txt");
  file1 << "test\n";
  file1.close();
  
  std::string result = FindLatestFileInDir(dir_path, "op_summary_");
  EXPECT_TRUE(result.empty());
  
  std::remove((dir_path + "/other_file.txt").c_str());
  rmdir(dir_path.c_str());
}

/**
 * 场景 10-4: FindLatestProfDir 正常查找
 */
TEST_F(DAGAdapterPathUtilsTest, FindLatestProfDir_Normal) {
  CreateMockProfilingStructure(test_base_path_, "PROF_000001_20260101", "op_summary_1.csv");
  
  std::string result = FindLatestProfDir(test_base_path_);
  EXPECT_EQ(result, "PROF_000001_20260101");
}

/**
 * 场景 10-5: FindLatestProfDir 多个目录返回最新
 */
TEST_F(DAGAdapterPathUtilsTest, FindLatestProfDir_MultipleDirs) {
  CreateMockProfilingStructure(test_base_path_, "PROF_000001_20260101", "op_summary_1.csv");
  std::string prof_path2 = test_base_path_ + "/PROF_000002_20260202";
  mkdir(prof_path2.c_str(), 0755);
  
  std::string result = FindLatestProfDir(test_base_path_);
  EXPECT_EQ(result, "PROF_000002_20260202");
  
  rmdir(prof_path2.c_str());
}

/**
 * 场景 10-6: FindLatestProfDir 目录不存在返回空
 */
TEST_F(DAGAdapterPathUtilsTest, FindLatestProfDir_DirNotExist) {
  std::string result = FindLatestProfDir("/tmp/nonexistent_dir_12345");
  EXPECT_TRUE(result.empty());
}

/**
 * 场景 10-7: BuildOpSummaryPath 正常构建完整路径
 */
TEST_F(DAGAdapterPathUtilsTest, BuildOpSummaryPath_Normal) {
  CreateMockProfilingStructure(test_base_path_, "PROF_000001_20260101", "op_summary_20260101.csv");
  
  std::string result = BuildOpSummaryPath(test_base_path_);
  EXPECT_FALSE(result.empty());
  EXPECT_TRUE(result.find("PROF_000001_20260101") != std::string::npos);
  EXPECT_TRUE(result.find("mindstudio_profiler_output") != std::string::npos);
  EXPECT_TRUE(result.find("op_summary_") != std::string::npos);
  EXPECT_TRUE(result.find(".csv") != std::string::npos);
}

/**
 * 场景 10-8: BuildOpSummaryPath 空输入目录返回空
 */
TEST_F(DAGAdapterPathUtilsTest, BuildOpSummaryPath_EmptyInput) {
  std::string result = BuildOpSummaryPath("");
  EXPECT_TRUE(result.empty());
}

/**
 * 场景 10-9: BuildOpSummaryPath 无 PROF 目录返回空
 */
TEST_F(DAGAdapterPathUtilsTest, BuildOpSummaryPath_NoProfDir) {
  mkdir(test_base_path_.c_str(), 0755);
  
  std::string result = BuildOpSummaryPath(test_base_path_);
  EXPECT_TRUE(result.empty());
  
  rmdir(test_base_path_.c_str());
}

/**
 * 场景 10-10: BuildOpSummaryPath 无 mindstudio_profiler_output 返回空
 */
TEST_F(DAGAdapterPathUtilsTest, BuildOpSummaryPath_NoProfilerOutput) {
  mkdir(test_base_path_.c_str(), 0755);
  std::string prof_path = test_base_path_ + "/PROF_000001_20260101";
  mkdir(prof_path.c_str(), 0755);
  
  std::string result = BuildOpSummaryPath(test_base_path_);
  EXPECT_TRUE(result.empty());
  
  rmdir(prof_path.c_str());
  rmdir(test_base_path_.c_str());
}

/**
 * 场景 10-11: BuildOpSummaryPath 无 op_summary 文件返回空
 */
TEST_F(DAGAdapterPathUtilsTest, BuildOpSummaryPath_NoOpSummaryFile) {
  mkdir(test_base_path_.c_str(), 0755);
  std::string prof_path = test_base_path_ + "/PROF_000001_20260101";
  std::string output_path = prof_path + "/mindstudio_profiler_output";
  mkdir(prof_path.c_str(), 0755);
  mkdir(output_path.c_str(), 0755);

  std::ofstream other_file(output_path + "/other_file.txt");
  other_file << "test\n";
  other_file.close();

  std::string result = BuildOpSummaryPath(test_base_path_);
  EXPECT_TRUE(result.empty());

  std::remove((output_path + "/other_file.txt").c_str());
  rmdir(output_path.c_str());
  rmdir(prof_path.c_str());
  rmdir(test_base_path_.c_str());
}

// --------------------
// 场景 11：MiniDAG Profiling 开关测试
// --------------------

class DAGAdapterProfilingPriorityTest : public testing::Test {
 protected:
  void SetUp() override {
    test_base_path_ = "/tmp/test_prof_priority_" + std::to_string(getpid());
  }
  
  void TearDown() override {
    CleanupMockProfilingStructure(test_base_path_);
    unsetenv("GE_PROFILING_MODE");
    unsetenv("GE_PROFILING_OPTIONS");
    unsetenv("MINIDAG_PROFILING_PATH");
    ge::GetThreadLocalContext().SetGraphOption({});
  }
  
  void CreateProfilingDir(const std::string &base_path, const std::string &prof_dir_name,
                           const std::string &csv_filename, const std::string &op_name,
                           float exec_time, size_t cube_block) {
    mkdir(base_path.c_str(), 0755);
    std::string prof_path = base_path + "/" + prof_dir_name;
    mkdir(prof_path.c_str(), 0755);
    std::string output_path = prof_path + "/mindstudio_profiler_output";
    mkdir(output_path.c_str(), 0755);
    
    std::string csv_path = output_path + "/" + csv_filename;
    std::ofstream file(csv_path);
    file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
         << op_name << ",AI_CORE," << exec_time << "," << cube_block << ",0\n";
    file.close();
  }
  
  void RemoveProfilingDir(const std::string &base_path) {
    auto remove_dir = [](const std::string &path) {
      DIR *dir = opendir(path.c_str());
      if (dir == nullptr) return;
      struct dirent *entry;
      while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
          std::string filepath = path + "/" + entry->d_name;
          std::remove(filepath.c_str());
        }
      }
      closedir(dir);
      rmdir(path.c_str());
    };
    
    std::string prof_dir = FindLatestProfDir(base_path);
    if (!prof_dir.empty() && prof_dir != "." && prof_dir != "..") {
      std::string prof_path = base_path + "/" + prof_dir;
      std::string output_path = prof_path + "/mindstudio_profiler_output";
      remove_dir(output_path);
      rmdir(prof_path.c_str());
    }
    rmdir(base_path.c_str());
  }
  
  std::string test_base_path_;
};

/**
 * 场景 11-1: GE Options (profilingMode=1 + profilingOptions) 被忽略
 * 验证即使 profilingMode=1 且 profilingOptions 有 output，MiniDAG 也不读取 GE Options 路径
 */
TEST_F(DAGAdapterProfilingPriorityTest, GEOptionsIgnored) {
  std::string option_path = test_base_path_ + "_option";
  CreateProfilingDir(option_path, "PROF_000001_20260101", "op_summary_1.csv", "add1", 100.0f, 8);
  
  std::map<std::string, std::string> options;
  options[OPTION_EXEC_PROFILING_MODE] = "1";
  options[OPTION_EXEC_PROFILING_OPTIONS] = "{\"output\":\"" + option_path + "\"}";
  ge::GetThreadLocalContext().SetGraphOption(options);
  
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);
  
  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  
  auto add_node = dag->FindNode("add1");
  ASSERT_NE(add_node, nullptr);
  const auto& cost = add_node->GetCost();
  EXPECT_EQ(cost.execution_time, -1.0f);
  EXPECT_EQ(cost.cube_block_num, 0U);
  
  RemoveProfilingDir(option_path);
}

/**
 * 场景 11-4: GE 环境变量被忽略
 * 验证即使设置 GE_PROFILING_MODE + GE_PROFILING_OPTIONS，MiniDAG 也不读取 GE 环境变量
 */
TEST_F(DAGAdapterProfilingPriorityTest, GEEnvIgnored) {
  std::string env_path = test_base_path_ + "_env";
  CreateProfilingDir(env_path, "PROF_000001_20260101", "op_summary_1.csv", "add1", 200.0f, 16);
  
  // 不设置 GE Options（或设置无效的）
  ge::GetThreadLocalContext().SetGraphOption({});
  
  // 设置 GE 环境变量
  setenv("GE_PROFILING_MODE", "true", 1);
  setenv("GE_PROFILING_OPTIONS", ("{\"output\":\"" + env_path + "\"}").c_str(), 1);
  
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);
  
  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  
  auto add_node = dag->FindNode("add1");
  ASSERT_NE(add_node, nullptr);
  const auto& cost = add_node->GetCost();
  EXPECT_EQ(cost.execution_time, -1.0f);
  EXPECT_EQ(cost.cube_block_num, 0U);
  
  RemoveProfilingDir(env_path);
}

/**
 * 场景 11-6: MINIDAG_PROFILING_PATH 独立于 GE Options 生效
 * 验证即使 GE Options 存在，真正生效的仍只有 MINIDAG_PROFILING_PATH
 */
/**
 * 场景 11-5: 仅 MINIDAG_PROFILING_PATH 作为 profiling 开关
 * 验证当 GE Options、GE 环境变量和 MINIDAG_PROFILING_PATH 同时存在时，只有 MINIDAG_PROFILING_PATH 生效
 */
TEST_F(DAGAdapterProfilingPriorityTest, OnlyMinidagProfilingPathIsEffective) {
  std::string option_path = test_base_path_ + "_option";
  std::string env_path = test_base_path_ + "_env";
  std::string minidag_path = test_base_path_ + "_minidag.csv";
  
  // 三种数据源，数据不同
  CreateProfilingDir(option_path, "PROF_000001_20260101", "op_summary_1.csv", "add1", 100.0f, 8);
  CreateProfilingDir(env_path, "PROF_000001_20260101", "op_summary_1.csv", "add1", 200.0f, 16);
  
  std::ofstream file(minidag_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,AI_CORE,500.0,40,0\n";
  file.close();
  
  // 设置所有方式
  std::map<std::string, std::string> options;
  options[OPTION_EXEC_PROFILING_MODE] = "1";
  options[OPTION_EXEC_PROFILING_OPTIONS] = "{\"output\":\"" + option_path + "\"}";
  ge::GetThreadLocalContext().SetGraphOption(options);
  
  setenv("GE_PROFILING_MODE", "true", 1);
  setenv("GE_PROFILING_OPTIONS", ("{\"output\":\"" + env_path + "\"}").c_str(), 1);
  setenv("MINIDAG_PROFILING_PATH", minidag_path.c_str(), 1);
  
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);
  
  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  
  auto add_node = dag->FindNode("add1");
  ASSERT_NE(add_node, nullptr);
  const auto& cost = add_node->GetCost();
  // 仅 MINIDAG_PROFILING_PATH 生效，使用 minidag_path 的数据
  EXPECT_EQ(cost.execution_time, 500.0f);
  EXPECT_EQ(cost.cube_block_num, 40U);
  
  RemoveProfilingDir(option_path);
  RemoveProfilingDir(env_path);
  std::remove(minidag_path.c_str());
}

TEST_F(DAGAdapterProfilingPriorityTest, InvalidProfilingCsvUsesDefaultCost) {
  std::string invalid_csv_path = test_base_path_ + "_invalid.csv";
  std::ofstream file(invalid_csv_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,AI_CORE,invalid,8,0\n";
  file.close();
  setenv("MINIDAG_PROFILING_PATH", invalid_csv_path.c_str(), 1);

  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  auto add_node = dag->FindNode("add1");
  ASSERT_NE(add_node, nullptr);
  EXPECT_EQ(add_node->GetCost().execution_time, -1.0f);
  EXPECT_EQ(add_node->GetCost().cube_block_num, 0U);

  std::remove(invalid_csv_path.c_str());
}

TEST_F(DAGAdapterProfilingPriorityTest, BlockDimFallbackForAicNode) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);
  auto add_gnode = ge_graph->FindNodeByName(AscendString("add1"));
  ASSERT_NE(add_gnode, nullptr);
  auto add_node = NodeAdapter::GNode2Node(*add_gnode);
  ASSERT_NE(add_node, nullptr);
  ASSERT_TRUE(AttrUtils::SetInt(add_node->GetOpDesc(), TVM_ATTR_NAME_BLOCKDIM, 32));
  ASSERT_TRUE(AttrUtils::SetStr(add_node->GetOpDesc(), ATTR_NAME_CUBE_VECTOR_CORE_TYPE, "AIC"));

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  auto dag_add_node = dag->FindNode("add1");
  ASSERT_NE(dag_add_node, nullptr);
  EXPECT_EQ(dag_add_node->GetCost().cube_block_num, 32U);
  EXPECT_EQ(dag_add_node->GetCost().vec_block_num, 0U);
}

TEST_F(DAGAdapterProfilingPriorityTest, BlockDimFallbackForAivNode) {
  auto ge_graph = BuildGraphWithNodes();
  ASSERT_NE(ge_graph, nullptr);
  auto add_gnode = ge_graph->FindNodeByName(AscendString("add1"));
  ASSERT_NE(add_gnode, nullptr);
  auto add_node = NodeAdapter::GNode2Node(*add_gnode);
  ASSERT_NE(add_node, nullptr);
  ASSERT_TRUE(AttrUtils::SetInt(add_node->GetOpDesc(), TVM_ATTR_NAME_BLOCKDIM, 16));
  ASSERT_TRUE(AttrUtils::SetStr(add_node->GetOpDesc(), ATTR_NAME_CUBE_VECTOR_CORE_TYPE, "AIV"));

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  auto dag_add_node = dag->FindNode("add1");
  ASSERT_NE(dag_add_node, nullptr);
  EXPECT_EQ(dag_add_node->GetCost().cube_block_num, 0U);
  EXPECT_EQ(dag_add_node->GetCost().vec_block_num, 16U);
}

}  // namespace ge
