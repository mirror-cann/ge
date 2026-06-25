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
#include <fstream>
#include "api/gelib/gelib.h"
#include "graph/build/stream/dag_adapter.h"
#include "graph/build/dag/dag_profiling_parser.h"
#include "graph/build/dag/dag_stream_allocator.h"
#include "graph/build/dag/dag_types.h"
#include "engines/manager/engine_manager/dnnengine_manager.h"
#include "register/custom_pass_context_impl.h"
#include <graph_utils_ex.h>
#include <register/register_custom_pass.h>
#include "graph/utils/node_adapter.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/attr_utils.h"
#include "graph/ge_local_context.h"
#include "external/ge_common/ge_common_api_types.h"

namespace ge {
namespace {
graphStatus CallFromGEGraph(const ConstGraphPtr &ge_graph,
                            std::shared_ptr<minidag::DAGGraph> &dag) {
  bool has_profiled_node_cost = false;
  return DAGAdapter::FromGEGraph(ge_graph, dag, has_profiled_node_cost);
}

void SetStreamLabel(const ge::GNode &gnode, const std::string &stream_label) {
  auto node = NodeAdapter::GNode2Node(gnode);
  ASSERT_NE(node, nullptr);
  ASSERT_NE(node->GetOpDesc(), nullptr);
  ASSERT_TRUE(AttrUtils::SetStr(node->GetOpDesc(), ATTR_NAME_STREAM_LABEL, stream_label));
}

ge::ConstGraphPtr BuildStreamLabelControlGraph() {
  auto graph = std::make_shared<ge::Graph>("stream_label_control_graph");
  ge::Operator data_op("data1", "Data");
  ge::Operator add_op("add1", "Add");
  ge::Operator relu_op("relu1", "Relu");
  ge::Operator pool_op("pool1", "Pool");
  ge::Operator abs_op("abs1", "Abs");
  ge::Operator netoutput_op("NetOutput", "NetOutput");

  auto data = graph->AddNodeByOp(data_op);
  auto add = graph->AddNodeByOp(add_op);
  auto relu = graph->AddNodeByOp(relu_op);
  auto pool = graph->AddNodeByOp(pool_op);
  auto abs = graph->AddNodeByOp(abs_op);
  auto netoutput = graph->AddNodeByOp(netoutput_op);
  SetStreamLabel(relu, "serial_label");
  SetStreamLabel(pool, "serial_label");
  graph->AddControlEdge(data, add);
  graph->AddControlEdge(add, relu);
  graph->AddControlEdge(relu, pool);
  graph->AddControlEdge(pool, abs);
  graph->AddControlEdge(abs, netoutput);
  return graph;
}
}  // namespace

class DAGAdapterIntegrationTest : public testing::Test {
 protected:
  void SetUp() override {
    unsetenv("GE_PROFILING_MODE");
    unsetenv("GE_PROFILING_OPTIONS");
    unsetenv("MINIDAG_PROFILING_PATH");
    ge::GetThreadLocalContext().SetGraphOption({{ge::SOC_VERSION, "Ascend910B1"}});
  }

  void TearDown() override {
    unsetenv("GE_PROFILING_MODE");
    unsetenv("GE_PROFILING_OPTIONS");
    unsetenv("MINIDAG_PROFILING_PATH");
    ge::GetThreadLocalContext().SetGraphOption({{ge::SOC_VERSION, "Ascend910B1"}});
  }

  static void SetUpTestSuite() {
    std::map<std::string, std::string> options;
    options[ge::SOC_VERSION] = "Ascend910B1";
    EXPECT_EQ(ge::GELib::Initialize(options), ge::SUCCESS);
    ge::GELib::GetInstance()->OpsKernelManagerObj().ops_kernel_store_.clear();

    ge::GetThreadLocalContext().SetGraphOption(options);

    ge::GeRunningEnvFaker().Reset()
      .Install(ge::FakeEngine("DNN_VM_GE_LOCAL").KernelInfoStore("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeEngine("AIcoreEngine").KernelInfoStore("AIcoreEngine"))
      .Install(ge::FakeOp(ge::DATA).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeOp(ge::NETOUTPUT).InfoStoreAndBuilder("DNN_VM_GE_LOCAL_OP_STORE"))
      .Install(ge::FakeOp("Add").InfoStoreAndBuilder("AIcoreEngine"))
      .Install(ge::FakeOp("Conv").InfoStoreAndBuilder("AIcoreEngine"))
      .Install(ge::FakeOp("Relu").InfoStoreAndBuilder("AIcoreEngine"))
      .Install(ge::FakeOp("Pool").InfoStoreAndBuilder("AIcoreEngine"))
      .Install(ge::FakeOp("Abs").InfoStoreAndBuilder("AIcoreEngine"));

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

  static int64_t GetGNodeStreamId(const ge::GNode& gnode) {
    ge::NodePtr node = ge::NodeAdapter::GNode2Node(gnode);
    if (node == nullptr || node->GetOpDesc() == nullptr) {
      return ge::INVALID_STREAM_ID;
    }
    return node->GetOpDesc()->GetStreamId();
  }
};

// --------------------
// 场景 1：端到端流程测试
// --------------------

/**
 * 场景 1-1: 完整端到端流程
 */
TEST_F(DAGAdapterIntegrationTest, EndToEnd_FullPipeline) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  size_t original_node_count = compute_graph->GetAllNodes().size();

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), original_node_count);

  minidag::StreamAllocConfig config{-1, 0};
  minidag::DagStreamAllocator::ByPathCover(*dag, config);

  ge::StreamPassContext context(config.required_streams);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  EXPECT_EQ(compute_graph->GetAllNodes().size(), original_node_count);
}

/**
 * 场景 1-2: 多节点图端到端流程
 */
TEST_F(DAGAdapterIntegrationTest, EndToEnd_MultiNodeGraph) {
  auto compute_graph = gert::ShareGraph::BuildStaticAbsReluExpAddNodeGraph();
  ASSERT_NE(compute_graph, nullptr);

  size_t original_node_count = compute_graph->GetAllNodes().size();

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_EQ(dag->GetNodeCount(), original_node_count);

  minidag::StreamAllocConfig config{-1, 0};
  minidag::DagStreamAllocator::ByPathCover(*dag, config);

  ge::StreamPassContext context(config.required_streams);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);

  EXPECT_EQ(compute_graph->GetAllNodes().size(), original_node_count);
}

/**
 * 用例描述：测试DAG Adapter端到端流程中stream_label节点按串行标记分配到同一条追加stream
 * 预置条件：
 *   1. Fake AIcoreEngine及Add/Relu/Pool/Abs算子信息
 *   2. 构造带stream_label的控制边图
 * 测试步骤：
 *   1. 将GE图转换为MiniDAG图
 *   2. 执行MiniDAG流分配
 *   3. 将MiniDAG上的stream_id刷新回GE图
 * 预期结果：
 *   1. 相同stream_label的Relu和Pool节点分配到同一条stream
 *   2. label节点使用的stream为residual graph之后追加的串行stream
 */
TEST_F(DAGAdapterIntegrationTest, EndToEnd_StreamLabelAssignedToSameSerialStream) {
  auto ge_graph = BuildStreamLabelControlGraph();
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto relu_dag_node = dag->FindNode("relu1");
  auto pool_dag_node = dag->FindNode("pool1");
  ASSERT_NE(relu_dag_node, nullptr);
  ASSERT_NE(pool_dag_node, nullptr);
  ASSERT_EQ(relu_dag_node->GetSerialFlag(), "serial_label");
  ASSERT_EQ(pool_dag_node->GetSerialFlag(), "serial_label");

  minidag::StreamAllocConfig config{-1, 0, 0};
  minidag::DagStreamAllocator::ByPathCover(*dag, config);
  ASSERT_EQ(config.required_streams, 2);
  ASSERT_EQ(relu_dag_node->GetStreamId(), pool_dag_node->GetStreamId());
  ASSERT_EQ(relu_dag_node->GetStreamId(), config.required_streams - 1);

  ge::StreamPassContext context(config.required_streams - 1);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);

  auto relu_gnode = ge_graph->FindNodeByName(AscendString("relu1"));
  auto pool_gnode = ge_graph->FindNodeByName(AscendString("pool1"));
  auto add_gnode = ge_graph->FindNodeByName(AscendString("add1"));
  ASSERT_NE(relu_gnode, nullptr);
  ASSERT_NE(pool_gnode, nullptr);
  ASSERT_NE(add_gnode, nullptr);
  EXPECT_EQ(GetGNodeStreamId(*relu_gnode), GetGNodeStreamId(*pool_gnode));
  EXPECT_EQ(GetGNodeStreamId(*relu_gnode), 1);
  EXPECT_EQ(GetGNodeStreamId(*add_gnode), 0);
}

// --------------------
// 场景 2：RefreshStreamIdsToGE 测试
// --------------------

/**
 * 场景 2-1: stream_id 正确设置到 GE GNode
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_StreamIdSet) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  int64_t stream_idx = 0;
  for (auto& node : nodes) {
    node->SetStreamId(stream_idx++);
  }

  ge::StreamPassContext context(static_cast<int64_t>(nodes.size()));
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 2-2: null 输入返回失败
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_NullInput) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");
  ge::StreamPassContext context(10);
  auto ret = DAGAdapter::RefreshStreamIdsToGE(*dag, nullptr, context);
  EXPECT_NE(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 2-3: 无效 stream_id 被跳过
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_SkipInvalidStreamId) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  if (nodes.size() >= 2) {
    nodes[0]->SetStreamId(INVALID_STREAM_ID);
    nodes[1]->SetStreamId(0);
  }

  ge::StreamPassContext context(10);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 2-4: DAG 节点不在 GE 图中的场景
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_NodeNotFoundInGE) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  dag->AddNode("extra_node_not_in_ge", "CustomOp");

  ge::StreamPassContext context(10);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

/**
 * 场景 2-5: 多种 stream_id 值测试
 */
TEST_F(DAGAdapterIntegrationTest, RefreshStreamIdsToGE_VariousStreamIds) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto nodes = dag->GetAllNodes();
  if (nodes.size() >= 4) {
    nodes[0]->SetStreamId(-1);
    nodes[1]->SetStreamId(0);
    nodes[2]->SetStreamId(1);
    nodes[3]->SetStreamId(100);
  }

  ge::StreamPassContext context(101);
  ret = DAGAdapter::RefreshStreamIdsToGE(*dag, ge_graph, context);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}


// --------------------
// 场景 3：DeviceResourceInfo 测试
// --------------------

/**
 * 场景 3-1: DeviceResourceInfo 默认值验证
 */
TEST_F(DAGAdapterIntegrationTest, DeviceResourceInfo_DefaultValues) {
  minidag::DeviceResourceInfo resource;
  EXPECT_EQ(resource.cube_core_num, -1);
  EXPECT_EQ(resource.vector_core_num, -1);

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");
  const auto& default_resource = dag->GetDeviceResource();
  EXPECT_EQ(default_resource.cube_core_num, -1);
  EXPECT_EQ(default_resource.vector_core_num, -1);
}

/**
 * 场景 3-2: DeviceResourceInfo 设置与获取验证
 */
TEST_F(DAGAdapterIntegrationTest, DeviceResourceInfo_SetAndGet) {
  minidag::DeviceResourceInfo resource;
  resource.cube_core_num = 30;
  resource.vector_core_num = 15;

  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->SetDeviceResource(resource);

  const auto& retrieved = dag->GetDeviceResource();
  EXPECT_EQ(retrieved.cube_core_num, 30);
  EXPECT_EQ(retrieved.vector_core_num, 15);
}

/**
 * 场景 3-3: FromGEGraph 后 DeviceResource 被正确填充
 */
TEST_F(DAGAdapterIntegrationTest, FromGEGraph_DeviceResourcePopulated) {
  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);

  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  const auto& resource = dag->GetDeviceResource();
  EXPECT_GT(resource.cube_core_num, 0);
  EXPECT_GT(resource.vector_core_num, 0);
}

/**
 * 场景 3-4: FillDeviceResource 直接调用成功路径
 */
TEST_F(DAGAdapterIntegrationTest, FillDeviceResource_Success) {
  auto dag = std::make_shared<minidag::DAGGraph>("test_dag");
  dag->AddNode("node1", "Conv");

  auto ret = DAGAdapter::FillDeviceResource(*dag);
  EXPECT_EQ(ret, ge::SUCCESS);

  const auto& resource = dag->GetDeviceResource();
  // stub 实现固定返回 32
  EXPECT_GT(resource.cube_core_num, 0);
  EXPECT_GT(resource.vector_core_num, 0);
}

/**
 * 场景 3-5: DeviceResourceInfo 各字段独立验证
 */
TEST_F(DAGAdapterIntegrationTest, DeviceResourceInfo_FieldVerification) {
  minidag::DeviceResourceInfo resource;
  resource.cube_core_num = 32;
  resource.vector_core_num = 64;

  auto dag = std::make_shared<minidag::DAGGraph>("field_test_dag");
  dag->SetDeviceResource(resource);

  const auto& r1 = dag->GetDeviceResource();
  EXPECT_EQ(r1.cube_core_num, 32);
  EXPECT_EQ(r1.vector_core_num, 64);

  // 覆盖多次设置场景
  resource.cube_core_num = 48;
  dag->SetDeviceResource(resource);
  const auto& r2 = dag->GetDeviceResource();
  EXPECT_EQ(r2.cube_core_num, 48);
}

/**
 * 场景 3-6: FillDeviceResource 后设备资源值合理性
 */
TEST_F(DAGAdapterIntegrationTest, FillDeviceResource_ValuesReasonable) {
  auto compute_graph = gert::ShareGraph::BuildStaticAbsReluExpAddNodeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);

  const auto& resource = dag->GetDeviceResource();
  EXPECT_GT(resource.cube_core_num, 0);
  EXPECT_GE(resource.vector_core_num, 0);
}


// --------------------
// 场景 4：Profiling 集成测试
// --------------------

/**
 * 场景 4-1: Profiling 数据集成验证
 * 用例描述：测试环境变量 MINIDAG_PROFILING_PATH 设置后，DAGAdapter 正确解析 profiling 数据并设置节点开销
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 *   2. 创建 profiling CSV 文件
 * 测试步骤：
 *   1. 创建包含 add1 节点 profiling 数据的 CSV 文件
 *   2. 设置 MINIDAG_PROFILING_PATH 环境变量
 *   3. 将 GE 图转换为 MiniDAG 图
 *   4. 验证 add1 节点的开销来自 profiling 数据
 * 预期结果：
 *   1. add1 节点的 execution_time、cube_block_num 正确设置
 */
TEST_F(DAGAdapterIntegrationTest, FromGEGraph_WithProfilingData_UsesProfilingCost) {
  const char* profiling_path = "/tmp/test_minidag_profiling_integration.csv";
  std::ofstream file(profiling_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,AI_CORE,100.0,8,0\n";
  file.close();

  setenv("MINIDAG_PROFILING_PATH", profiling_path, 1);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto add_node = dag->FindNode("add1");
  ASSERT_NE(add_node, nullptr);
  const auto& cost = add_node->GetCost();
  EXPECT_EQ(cost.execution_time, 100.0f);
  EXPECT_EQ(cost.cube_block_num, 8U);

  unsetenv("MINIDAG_PROFILING_PATH");
  std::remove(profiling_path);
}

/**
 * 用例描述：测试 profiling 命中节点时，FromGEGraph 输出 has_profiled_node_cost 为 true
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 *   2. 创建 add1 节点命中的 profiling CSV 文件
 * 测试步骤：
 *   1. 设置 MINIDAG_PROFILING_PATH 环境变量
 *   2. 调用带 has_profiled_node_cost 出参的新 FromGEGraph 签名
 *   3. 校验转换成功且命中标志为 true
 * 预期结果：
 *   1. 返回 GRAPH_SUCCESS
 *   2. has_profiled_node_cost 为 true
 */
TEST_F(DAGAdapterIntegrationTest, FromGEGraph_WithProfilingData_SetsProfiledCostFlag) {
  const char *profiling_path = "/tmp/test_minidag_profiled_flag.csv";
  struct ProfilingPathGuard {
    explicit ProfilingPathGuard(const char *path) : path(path) {}
    ~ProfilingPathGuard() {
      unsetenv("MINIDAG_PROFILING_PATH");
      std::remove(path);
    }
    const char *path;
  } guard(profiling_path);
  std::ofstream file(profiling_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,AI_CORE,123.0,8,0\n";
  file.close();

  setenv("MINIDAG_PROFILING_PATH", profiling_path, 1);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  bool has_profiled_node_cost = false;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag, has_profiled_node_cost);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_TRUE(has_profiled_node_cost);
}

/**
 * 用例描述：测试 profiling 文件存在但节点未命中时，FromGEGraph 输出 has_profiled_node_cost 为 false
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 *   2. 创建节点名不匹配的 profiling CSV 文件
 * 测试步骤：
 *   1. 设置 MINIDAG_PROFILING_PATH 环境变量
 *   2. 调用带 has_profiled_node_cost 出参的新 FromGEGraph 签名
 *   3. 校验转换成功且命中标志为 false
 * 预期结果：
 *   1. 返回 GRAPH_SUCCESS
 *   2. has_profiled_node_cost 为 false
 */
TEST_F(DAGAdapterIntegrationTest, FromGEGraph_WithUnmatchedProfiling_KeepsProfiledCostFlagFalse) {
  const char *profiling_path = "/tmp/test_minidag_profiled_unmatched.csv";
  struct ProfilingPathGuard {
    explicit ProfilingPathGuard(const char *path) : path(path) {}
    ~ProfilingPathGuard() {
      unsetenv("MINIDAG_PROFILING_PATH");
      std::remove(path);
    }
    const char *path;
  } guard(profiling_path);
  std::ofstream file(profiling_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "other_node,AI_CORE,55.0,4,0\n";
  file.close();

  setenv("MINIDAG_PROFILING_PATH", profiling_path, 1);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  bool has_profiled_node_cost = true;
  auto ret = DAGAdapter::FromGEGraph(ge_graph, dag, has_profiled_node_cost);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);
  EXPECT_FALSE(has_profiled_node_cost);
}

/**
 * 场景 4-3: MINIDAG 环境变量独立于 profilingOptions 生效
 * 用例描述：测试环境变量 MINIDAG_PROFILING_PATH 设置后，即使 profilingOptions 存在，也只读取 MINIDAG 路径
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 *   2. 创建两个不同的 profiling 文件
 * 测试步骤：
 *   1. 设置 profilingOptions 的 output 指向目录 A
 *   2. 设置环境变量 MINIDAG_PROFILING_PATH 指向文件 B
 *   3. 将 GE 图转换为 MiniDAG 图
 *   4. 验证节点开销来自环境变量指定的文件 B
 * 预期结果：
 *   1. 仅 MINIDAG_PROFILING_PATH 生效，使用文件 B 的数据
 */
/**
 * 场景 4-2: 环境变量指定目录时不生效
 * 用例描述：测试环境变量 MINIDAG_PROFILING_PATH 设置为目录时，MiniDAG profiling 回填不生效
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 *   2. 创建模拟的 profiling 目录结构
 * 测试步骤：
 *   1. 创建 PROF_* 目录结构
 *   2. 设置环境变量为目录路径（不是具体 csv 文件）
 *   3. 将 GE 图转换为 MiniDAG 图
 *   4. 验证节点开销保持默认值
 * 预期结果：
 *   1. 目录路径不生效，节点开销保持默认值
 */
TEST_F(DAGAdapterIntegrationTest, FromGEGraph_EnvDirectoryIgnored) {
  std::string base_path = "/tmp/test_prof_env_dir_" + std::to_string(getpid());
  std::string prof_dir = "PROF_000001_20260101";
  std::string output_dir = base_path + "/" + prof_dir + "/mindstudio_profiler_output";
  
  mkdir(base_path.c_str(), 0755);
  mkdir((base_path + "/" + prof_dir).c_str(), 0755);
  mkdir(output_dir.c_str(), 0755);
  
  std::string csv_path = output_dir + "/op_summary_20260101.csv";
  std::ofstream file(csv_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,AI_CORE,150.0,12,0\n";
  file.close();
  
  // 设置环境变量为目录（不是 csv 文件）
  setenv("MINIDAG_PROFILING_PATH", base_path.c_str(), 1);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
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

  // 清理
  unsetenv("MINIDAG_PROFILING_PATH");
  std::remove(csv_path.c_str());
  rmdir(output_dir.c_str());
  rmdir((base_path + "/" + prof_dir).c_str());
  rmdir(base_path.c_str());
}

// --------------------
// 场景 5：MiniDAG Profiling 开关集成测试
// --------------------

/**
 * 场景 5-1: GE Options (profilingMode=1 + profilingOptions) 被忽略
 * 用例描述：测试 profilingMode=1 且 profilingOptions 有 output 时，完整流程仍忽略 GE Options 路径
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 *   2. 创建 profiling 目录结构
 * 测试步骤：
 *   1. 设置 profilingMode=1 和 profilingOptions
 *   2. 构建 GE 图并转换为 MiniDAG
 *   3. 验证节点开销保持默认值
 * 预期结果：
 *   1. 节点开销保持默认值，不使用 GE Options 指定的 profiling 数据
 */
TEST_F(DAGAdapterIntegrationTest, Profiling_GEOptionsIgnored) {
  std::string base_path = "/tmp/test_st_ge_options_" + std::to_string(getpid());
  std::string prof_dir = "PROF_000001_20260101";
  std::string output_dir = base_path + "/" + prof_dir + "/mindstudio_profiler_output";
  
  mkdir(base_path.c_str(), 0755);
  mkdir((base_path + "/" + prof_dir).c_str(), 0755);
  mkdir(output_dir.c_str(), 0755);
  
  std::string csv_path = output_dir + "/op_summary_20260101.csv";
  std::ofstream file(csv_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,AI_CORE,250.0,20,0\n";
  file.close();
  
  std::map<std::string, std::string> options;
  options[ge::SOC_VERSION] = "Ascend910B1";
  options[OPTION_EXEC_PROFILING_MODE] = "1";
  options[OPTION_EXEC_PROFILING_OPTIONS] = "{\"output\":\"" + base_path + "\"}";
  ge::GetThreadLocalContext().SetGraphOption(options);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
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

  // 清理
  std::remove(csv_path.c_str());
  rmdir(output_dir.c_str());
  rmdir((base_path + "/" + prof_dir).c_str());
  rmdir(base_path.c_str());
  options.erase(OPTION_EXEC_PROFILING_MODE);
  options.erase(OPTION_EXEC_PROFILING_OPTIONS);
  ge::GetThreadLocalContext().SetGraphOption(options);
}

/**
 * 场景 5-2: GE 环境变量被忽略端到端测试
 * 用例描述：测试即使设置 GE_PROFILING_MODE + GE_PROFILING_OPTIONS，MiniDAG 仍忽略 GE 环境变量
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 *   2. 创建 profiling 目录结构
 * 测试步骤：
 *   1. 不设置 GE Options 或设置无效的
 *   2. 设置 GE_PROFILING_MODE=true 和 GE_PROFILING_OPTIONS
 *   3. 构建 GE 图并转换为 MiniDAG
 *   4. 验证节点开销保持默认值
 * 预期结果：
 *   1. 节点开销保持默认值，不使用 GE 环境变量指定的 profiling 数据
 */
TEST_F(DAGAdapterIntegrationTest, Profiling_GEEnvIgnored) {
  std::string base_path = "/tmp/test_st_ge_env_" + std::to_string(getpid());
  std::string prof_dir = "PROF_000001_20260101";
  std::string output_dir = base_path + "/" + prof_dir + "/mindstudio_profiler_output";
  
  mkdir(base_path.c_str(), 0755);
  mkdir((base_path + "/" + prof_dir).c_str(), 0755);
  mkdir(output_dir.c_str(), 0755);
  
  std::string csv_path = output_dir + "/op_summary_20260101.csv";
  std::ofstream file(csv_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,AI_CORE,300.0,25,0\n";
  file.close();
  
  // 不设置 GE profiling options，让系统回退到环境变量
  std::map<std::string, std::string> options;
  options[ge::SOC_VERSION] = "Ascend910B1";
  ge::GetThreadLocalContext().SetGraphOption(options);

  setenv("GE_PROFILING_MODE", "true", 1);
  setenv("GE_PROFILING_OPTIONS", ("{\"output\":\"" + base_path + "\"}").c_str(), 1);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
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

  // 清理
  unsetenv("GE_PROFILING_MODE");
  unsetenv("GE_PROFILING_OPTIONS");
  std::remove(csv_path.c_str());
  rmdir(output_dir.c_str());
  rmdir((base_path + "/" + prof_dir).c_str());
  rmdir(base_path.c_str());
}

/**
 * 场景 5-3: MINIDAG_PROFILING_PATH 独立于 GE Options 生效端到端测试
 * 用例描述：测试即使 GE Options 存在，真正生效的仍只有 MINIDAG_PROFILING_PATH
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 *   2. 创建两个不同的 profiling 数据源
 * 测试步骤：
 *   1. 设置 GE Options 指向数据源 A
 *   2. 设置 MINIDAG_PROFILING_PATH 指向数据源 B
 *   3. 构建 GE 图并转换为 MiniDAG
 *   4. 验证节点开销来自 MINIDAG_PROFILING_PATH
 * 预期结果：
 *   1. 仅 MINIDAG_PROFILING_PATH 生效，使用数据源 B 的数据
 */
TEST_F(DAGAdapterIntegrationTest, Profiling_MinidagWorksIndependentlyOfGEOptions) {
  // GE Options 指向的数据（不会被使用）
  std::string option_path = "/tmp/test_st_option_" + std::to_string(getpid());
  std::string prof_dir1 = "PROF_000001_20260101";
  std::string output_dir1 = option_path + "/" + prof_dir1 + "/mindstudio_profiler_output";
  
  mkdir(option_path.c_str(), 0755);
  mkdir((option_path + "/" + prof_dir1).c_str(), 0755);
  mkdir(output_dir1.c_str(), 0755);
  
  std::string csv_path1 = output_dir1 + "/op_summary_1.csv";
  std::ofstream file1(csv_path1);
  file1 << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
        << "add1,AI_CORE,100.0,8,0\n";  // 这个数据不会被使用
  file1.close();
  
  // MINIDAG_PROFILING_PATH 指向的数据（会被使用）
  std::string minidag_path = "/tmp/test_st_minidag_override.csv";
  std::ofstream file2(minidag_path);
  file2 << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
        << "add1,AI_CORE,350.0,28,0\n";  // 这个数据会被使用
  file2.close();
  
  std::map<std::string, std::string> options;
  options[ge::SOC_VERSION] = "Ascend910B1";
  options[OPTION_EXEC_PROFILING_MODE] = "1";
  options[OPTION_EXEC_PROFILING_OPTIONS] = "{\"output\":\"" + option_path + "\"}";
  ge::GetThreadLocalContext().SetGraphOption(options);
  
  setenv("MINIDAG_PROFILING_PATH", minidag_path.c_str(), 1);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto add_node = dag->FindNode("add1");
  ASSERT_NE(add_node, nullptr);
  const auto& cost = add_node->GetCost();
  // MINIDAG_PROFILING_PATH 覆盖，使用 minidag_path 的数据
  EXPECT_EQ(cost.execution_time, 350.0f);
  EXPECT_EQ(cost.cube_block_num, 28U);

  // 清理
  unsetenv("MINIDAG_PROFILING_PATH");
  std::remove(minidag_path.c_str());
  std::remove(csv_path1.c_str());
  rmdir(output_dir1.c_str());
  rmdir((option_path + "/" + prof_dir1).c_str());
  rmdir(option_path.c_str());
  options.erase(OPTION_EXEC_PROFILING_MODE);
  options.erase(OPTION_EXEC_PROFILING_OPTIONS);
  ge::GetThreadLocalContext().SetGraphOption(options);
}

/**
 * 用例描述：测试 profiling CSV 中 MIX_AIV 任务类型在集成路径下被正确解析
 * 预置条件：
 *   1. Fake AIcoreEngine 及 Add 算子信息
 * 测试步骤：
 *   1. 创建一条 MIX_AIV profiling 记录
 *   2. 设置 MINIDAG_PROFILING_PATH 指向该 CSV
 *   3. 将 GE 图转换为 MiniDAG 图
 * 预期结果：
 *   1. 转换成功
 *   2. add1 节点的 cube/vec block 数按 MIX_AIV 规则解析
 */
TEST_F(DAGAdapterIntegrationTest, FromGEGraph_WithMixAivProfiling_UsesParsedCoreCounts) {
  const char *profiling_path = "/tmp/test_minidag_profiling_mix_aiv.csv";
  std::ofstream file(profiling_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num,Mix Block Num\n"
       << "add1,MIX_AIV,180.0,10,6\n";
  file.close();

  setenv("MINIDAG_PROFILING_PATH", profiling_path, 1);

  auto compute_graph = gert::ShareGraph::BuildTwoAddNodeKnownShapeGraph();
  ASSERT_NE(compute_graph, nullptr);
  auto ge_graph = ToConstGraphPtr(compute_graph);
  ASSERT_NE(ge_graph, nullptr);

  std::shared_ptr<minidag::DAGGraph> dag;
  auto ret = CallFromGEGraph(ge_graph, dag);
  ASSERT_EQ(ret, ge::GRAPH_SUCCESS);
  ASSERT_NE(dag, nullptr);

  auto add_node = dag->FindNode("add1");
  ASSERT_NE(add_node, nullptr);
  const auto &cost = add_node->GetCost();
  EXPECT_EQ(cost.execution_time, 180.0f);
  EXPECT_EQ(cost.cube_block_num, 6U);
  EXPECT_EQ(cost.vec_block_num, 10U);

  unsetenv("MINIDAG_PROFILING_PATH");
  std::remove(profiling_path);
}

/**
 * 用例描述：测试 ProfilingParser 在 profiling 文件不存在时返回失败
 * 预置条件：
 *   1. 无
 * 测试步骤：
 *   1. 调用 ProfilingParser::Parse 解析一个不存在的 CSV 路径
 * 预期结果：
 *   1. 返回 FAILED
 *   2. profiles 保持为空
 */
TEST_F(DAGAdapterIntegrationTest, Parse_FileNotExist_ReturnsFailed) {
  std::unordered_map<std::string, minidag::ProfilingData> profiles;
  const auto ret = minidag::ProfilingParser::Parse("/tmp/nonexistent_minidag_st.csv", profiles);
  EXPECT_EQ(ret, minidag::graphStatus::FAILED);
  EXPECT_TRUE(profiles.empty());
}

/**
 * 用例描述：测试 ProfilingParser 在 profiling 文件为空时返回失败
 * 预置条件：
 *   1. 创建一个空的 CSV 文件
 * 测试步骤：
 *   1. 调用 ProfilingParser::Parse 解析该空文件
 * 预期结果：
 *   1. 返回 FAILED
 *   2. profiles 保持为空
 */
TEST_F(DAGAdapterIntegrationTest, Parse_EmptyFile_ReturnsFailed) {
  const char *profiling_path = "/tmp/test_minidag_empty_st.csv";
  std::ofstream file(profiling_path);
  file.close();

  std::unordered_map<std::string, minidag::ProfilingData> profiles;
  const auto ret = minidag::ProfilingParser::Parse(profiling_path, profiles);
  EXPECT_EQ(ret, minidag::graphStatus::FAILED);
  EXPECT_TRUE(profiles.empty());

  std::remove(profiling_path);
}

/**
 * 用例描述：测试 ProfilingParser 在缺少必需列时返回失败
 * 预置条件：
 *   1. 创建缺少 Mix Block Num 列的 CSV 文件
 * 测试步骤：
 *   1. 调用 ProfilingParser::Parse 解析该 CSV
 * 预期结果：
 *   1. 返回 FAILED
 *   2. profiles 保持为空
 */
TEST_F(DAGAdapterIntegrationTest, Parse_MissingRequiredColumn_ReturnsFailed) {
  const char *profiling_path = "/tmp/test_minidag_missing_column_st.csv";
  std::ofstream file(profiling_path);
  file << "Op Name,Task Type,Task Duration(us),Block Num\n"
       << "add1,AI_CORE,100.0,4\n";
  file.close();

  std::unordered_map<std::string, minidag::ProfilingData> profiles;
  const auto ret = minidag::ProfilingParser::Parse(profiling_path, profiles);
  EXPECT_EQ(ret, minidag::graphStatus::FAILED);
  EXPECT_TRUE(profiles.empty());

  std::remove(profiling_path);
}

}  // namespace ge
