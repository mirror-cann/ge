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
#include <memory>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "macro_utils/dt_public_scope.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "common/context/local_context.h"
#include "common/ge_common/ge_types.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "common/share_graph.h"
#include "graph/anchor.h"
#include "graph/attr_value.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/utils/op_desc_utils_ex.h"
#include "omg/omg_inner_types.h"
#include "../passes/graph_builder_utils.h"
#include "graph/build/model_builder.h"
#include "graph/build/input_h2d_overlap_plan.h"
#include "graph/build/input_h2d_overlap_test_utils.h"
#include "graph/build/stream/graph_stream_allocator.h"
#include "memory/memory_assigner.h"
#include "graph/build/label_allocator.h"
#include "graph/utils/tensor_adapter.h"
#include "graph/passes/pass_manager.h"
#include "api/aclgrph/option_utils.h"
#include "common/context/local_context.h"
#include "common/omg_util/omg_util.h"
#include "formats/utils/formats_trans_utils.h"
#include "proto/task.pb.h"
#include "../passes/graph_builder_utils.h"
#include "register/custom_pass_helper.h"
#include "faker/ge_model_builder.h"
#include "faker/aicore_taskdef_faker.h"
#include "graph/ops_stub.h"
#include "ge_attr_value.h"
#include "graph/manager/graph_context.h"
#include "graph/optimize/graph_optimize.h"
#include "generator/ge_generator.h"
#include "graph/utils/tensor_utils.h"
#include "graph/attr_value.h"
#include "graph/debug/ge_attr_define.h"
#include "external/ge_common/ge_common_api_types.h"
#include "graph/utils/graph_utils.h"
#include "engines/manager/opskernel_manager/ops_kernel_builder_manager.h"
#include "common/opskernel/ops_kernel_info_store.h"
#include "engines/local_engine/ops_kernel_store/ge_local_ops_kernel_info_store.h"
#include "api/gelib/gelib.h"
#include "ge/ut/ge/test_tools_task_info.h"
#include "macro_utils/dt_public_unscope.h"

using namespace std;
using namespace testing;
using namespace ge;
using namespace ge::input_h2d_overlap;
using domi::GetContext;
using namespace domi;

namespace ge {
const char *const kKernelLibName = "DNN_VM_GE_LOCAL";
const char *const kEnvName = "ASCEND_OPP_PATH";

std::string BuildInputH2DOverlapBoundaryConfigForTest(const std::vector<std::pair<size_t, size_t>> &ranges) {
  if (ranges.empty()) {
    return "";
  }
  std::string config = std::to_string(ranges[0U].first);
  for (const auto &range : ranges) {
    config += "," + std::to_string(range.second);
  }
  return config;
}

class UtestModelBuilderTest : public testing::Test {
 public:
  ge::OpDescPtr CreateOpWithWsSize(const string &name, int64_t wsByte, const string &type = "some") {
    ge::OpDescPtr op_def = std::make_shared<ge::OpDesc>(name, type);
    auto desc_temp_ptr = std::make_shared<ge::GeTensorDesc>();
    auto desc_temp = *desc_temp_ptr;

    TensorUtils::SetSize(desc_temp, 1024);
    op_def->AddInputDesc(desc_temp);
    op_def->AddOutputDesc(desc_temp);

    std::vector<int64_t> workspace_bytes;
    workspace_bytes.push_back(wsByte);
    op_def->SetWorkspaceBytes(workspace_bytes);
    return op_def;
  }
  ge::OpDescPtr CreateRefOpWithWsSize(const string &name, int64_t wsByte, const string &type = "some") {
    ge::OpDescPtr op_def = std::make_shared<ge::OpDesc>(name, type);
    auto desc_temp_ptr = std::make_shared<ge::GeTensorDesc>();
    auto desc_temp = *desc_temp_ptr;

    TensorUtils::SetSize(desc_temp, 1024);
    op_def->AddInputDesc(desc_temp);

    auto desc_output_ptr = std::make_shared<ge::GeTensorDesc>();
    auto desc_output = *desc_output_ptr;
    TensorUtils::SetSize(desc_output, 6500);
    ge::TensorUtils::SetReuseInput(desc_output, true);
    ge::TensorUtils::SetReuseInputIndex(desc_output, 0);
    op_def->AddOutputDesc(desc_output);

    std::vector<int64_t> workspace_bytes;
    workspace_bytes.push_back(wsByte);
    op_def->SetWorkspaceBytes(workspace_bytes);
    return op_def;
  }

  ge::OpDescPtr CreateOpNetOutput(const string &name, const uint32_t num = 1) {
    ge::OpDescPtr op_def = std::make_shared<ge::OpDesc>(name, NETOUTPUT);
    auto desc_temp_ptr = std::make_shared<ge::GeTensorDesc>();
    auto desc_temp = *desc_temp_ptr;
    for (uint32_t i = 0; i < num; ++i) {
      op_def->AddInputDesc(desc_temp);
    }
    return op_def;
  }

  ge::OpDescPtr CreateConstOpWithValue(std::vector<uint8_t> val) {
    auto desc_temp_ptr = std::make_shared<ge::GeTensorDesc>();
    auto desc_temp = *desc_temp_ptr;
    auto ge_tensor = std::make_shared<ge::GeTensor>(desc_temp, val);
    return OpDescUtils::CreateConstOp(ge_tensor);
  }

  void MakeGraphWithConst(ge::ComputeGraphPtr &graph) {
    ge::OpDescPtr op_def_output = CreateOpNetOutput("output", 3);
    ge::NodePtr node_output = graph->AddNode(op_def_output);

    std::vector<uint8_t> val1(256, 0);
    ge::OpDescPtr op_def_const_a = CreateConstOpWithValue(val1);
    ge::OpDescPtr op_def_const_b = CreateConstOpWithValue(val1);
    std::vector<uint8_t> val2(256, 1);
    ge::OpDescPtr op_def_const_c = CreateConstOpWithValue(val2);
    ge::NodePtr node_const_a = graph->AddNode(op_def_const_a);
    ge::NodePtr node_const_b = graph->AddNode(op_def_const_b);
    ge::NodePtr node_const_c = graph->AddNode(op_def_const_c);

    ge::GraphUtils::AddEdge(node_const_a->GetOutDataAnchor(0), node_output->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_const_b->GetOutDataAnchor(0), node_output->GetInDataAnchor(1));
    ge::GraphUtils::AddEdge(node_const_c->GetOutDataAnchor(0), node_output->GetInDataAnchor(2));
    graph->TopologicalSorting();
  }

  void MakeGraph(ge::ComputeGraphPtr &graph) {
    ge::OpDescPtr op_def_a = CreateOpWithWsSize("A", 6000);
    op_def_a->SetStreamId(0);
    ge::OpDescPtr op_def_b = CreateOpWithWsSize("B", 120000);
    op_def_b->SetStreamId(0);
    ge::OpDescPtr op_def_c = CreateOpWithWsSize("C", 16000);
    op_def_c->SetStreamId(1);
    ge::OpDescPtr op_def_d = CreateOpWithWsSize("D", 24000);
    op_def_d->SetStreamId(2);
    ge::OpDescPtr op_def_e = CreateOpWithWsSize("E", 24000);
    op_def_e->SetStreamId(3);
    ge::OpDescPtr op_def_f = CreateOpWithWsSize("F", 30000);
    op_def_f->SetStreamId(2);
    ge::OpDescPtr op_def_g = CreateOpWithWsSize("G", 32000);
    op_def_g->SetStreamId(3);
    ge::OpDescPtr op_def_h = CreateOpWithWsSize("H", 48000);
    op_def_h->SetStreamId(2);
    ge::OpDescPtr op_def_i = CreateOpWithWsSize("I", 60000);
    op_def_i->SetStreamId(2);
    ge::OpDescPtr op_def_j = CreateOpWithWsSize("J", 256000, NETOUTPUT);
    op_def_j->SetStreamId(3);

    // add node
    ge::NodePtr node_a = graph->AddNode(op_def_a);
    ge::NodePtr node_b = graph->AddNode(op_def_b);
    ge::NodePtr node_c = graph->AddNode(op_def_c);
    ge::NodePtr node_d = graph->AddNode(op_def_d);
    ge::NodePtr node_e = graph->AddNode(op_def_e);
    ge::NodePtr node_f = graph->AddNode(op_def_f);
    ge::NodePtr node_g = graph->AddNode(op_def_g);
    ge::NodePtr node_h = graph->AddNode(op_def_h);
    ge::NodePtr node_i = graph->AddNode(op_def_i);
    ge::NodePtr node_j = graph->AddNode(op_def_j);

    // add edge
    ge::GraphUtils::AddEdge(node_a->GetOutDataAnchor(0), node_b->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_a->GetOutDataAnchor(0), node_c->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_b->GetOutDataAnchor(0), node_d->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_b->GetOutDataAnchor(0), node_e->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_c->GetOutDataAnchor(0), node_g->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_d->GetOutDataAnchor(0), node_f->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_e->GetOutDataAnchor(0), node_g->GetInDataAnchor(1));
    ge::GraphUtils::AddEdge(node_f->GetOutDataAnchor(0), node_h->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_g->GetOutDataAnchor(0), node_j->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_h->GetOutDataAnchor(0), node_i->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_i->GetOutDataAnchor(0), node_j->GetInDataAnchor(1));

    GetContext().out_nodes_map["H"] = {0};
    GetContext().out_nodes_map["I"] = {0};
    GetContext().out_nodes_map["J"] = {0};
    graph->TopologicalSorting();
  }

  void MakeSessionScopeReuseGraph(ge::ComputeGraphPtr graph) {
    ge::OpDescPtr op_def_a = CreateOpWithWsSize("A", 512);
    ge::OpDescPtr op_def_b = CreateOpWithWsSize("B", 0);
    ge::OpDescPtr op_def_c = CreateOpWithWsSize("C", 512);
    ge::OpDescPtr op_def_d = CreateOpWithWsSize("D", 512);
    ge::OpDescPtr op_def_e = CreateOpWithWsSize("E", 1024);
    ge::OpDescPtr op_def_f = CreateOpWithWsSize("F", 512);
    ge::OpDescPtr op_def_g = CreateOpWithWsSize("G", 0);

    std::vector<int64_t> workspace_bytes;
    workspace_bytes.push_back(1024);
    workspace_bytes.push_back(512);
    op_def_c->SetWorkspaceBytes(workspace_bytes);
    vector<int32_t> workspace_no_reuse_scope = {0, 1};
    (void)ge::AttrUtils::SetListInt(op_def_c, ATTR_NAME_WORKSPACE_MEMORY_NO_REUSE_SCOPE, workspace_no_reuse_scope);

    vector<int32_t> workspace_no_reuse_scope_e = {1};
    (void)ge::AttrUtils::SetListInt(op_def_e, ATTR_NAME_WORKSPACE_MEMORY_NO_REUSE_SCOPE, workspace_no_reuse_scope_e);

    ge::NodePtr node_a = graph->AddNode(op_def_a);
    ge::NodePtr node_b = graph->AddNode(op_def_b);
    ge::NodePtr node_c = graph->AddNode(op_def_c);
    ge::NodePtr node_d = graph->AddNode(op_def_d);
    ge::NodePtr node_e = graph->AddNode(op_def_e);
    ge::NodePtr node_f = graph->AddNode(op_def_f);
    ge::NodePtr node_g = graph->AddNode(op_def_g);

    ge::GraphUtils::AddEdge(node_a->GetOutDataAnchor(0), node_b->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_b->GetOutDataAnchor(0), node_c->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_c->GetOutDataAnchor(0), node_d->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_d->GetOutDataAnchor(0), node_e->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_e->GetOutDataAnchor(0), node_f->GetInDataAnchor(0));
    ge::GraphUtils::AddEdge(node_f->GetOutDataAnchor(0), node_g->GetInDataAnchor(0));
    graph->TopologicalSorting();
  }

 protected:
  void SetUp() {
    SetLocalOmgContext(domi::GetContext());
  }

  void TearDown() {
    GetContext().out_nodes_map.clear();
  }

  class FakeOpsKernelInfoStore : public OpsKernelInfoStore {
   public:
    FakeOpsKernelInfoStore() {
      supported_ = true;
    };
    bool supported_;

   private:
    Status Initialize(const std::map<std::string, std::string> &options) override {
      return SUCCESS;
    };
    Status Finalize() override {
      return SUCCESS;
    };
    bool CheckSupported(const OpDescPtr &op_desc, std::string &reason) const override {
      return supported_;
    };
    void GetAllOpsKernelInfo(std::map<std::string, ge::OpInfo> &infos) const override {};
  };

  class FakeOpsKernelBuilder : public OpsKernelBuilder {
   public:
    FakeOpsKernelBuilder() = default;

   private:
    Status Initialize(const map<std::string, std::string> &options) override {
      return SUCCESS;
    };
    Status Finalize() override {
      return SUCCESS;
    };
    Status CalcOpRunningParam(Node &node) override {
      return SUCCESS;
    };
    Status GenerateTask(const Node &node, RunContext &context, std::vector<domi::TaskDef> &tasks) override {
      domi::TaskDef task_def;
      tasks.push_back(task_def);
      return SUCCESS;
    };
  };
  void InitGeLib() {
    map<string, string> options;
    Status ret = ge::GELib::Initialize(options);
    EXPECT_EQ(ret, SUCCESS);
    auto instance_ptr = ge::GELib::GetInstance();
    EXPECT_NE(instance_ptr, nullptr);

    //  SchedulerConf conf;
    SchedulerConf scheduler_conf;
    scheduler_conf.name = kKernelLibName;
    scheduler_conf.cal_engines[kKernelLibName] = std::make_shared<EngineConf>();
    scheduler_conf.cal_engines[kKernelLibName]->name = kKernelLibName;
    scheduler_conf.cal_engines[kKernelLibName]->scheduler_id = kKernelLibName;
    map<string, SchedulerConf> scheduler_confs;
    scheduler_confs["scheduler"] = scheduler_conf;
    instance_ptr->DNNEngineManagerObj().schedulers_[kKernelLibName] = scheduler_conf;

    OpsKernelInfoStorePtr ops_kernel_info_store_ptr = std::make_shared<FakeOpsKernelInfoStore>();
    OpsKernelManager::GetInstance().ops_kernel_store_.emplace(kKernelLibName, ops_kernel_info_store_ptr);
    OpsKernelBuilderPtr fake_builder = std::make_shared<FakeOpsKernelBuilder>();
    OpsKernelBuilderRegistry::GetInstance().kernel_builders_[kKernelLibName] = fake_builder;
    OpInfo op_info;
    op_info.engine = kKernelLibName;
    op_info.opKernelLib = kKernelLibName;
    OpsKernelManager &ops_kernel_manager = instance_ptr->OpsKernelManagerObj();
    ops_kernel_manager.ops_kernel_info_[DATA].emplace_back(op_info);
    ops_kernel_manager.ops_kernel_info_[ADD].emplace_back(op_info);
    ops_kernel_manager.ops_kernel_info_[NETOUTPUT].emplace_back(op_info);
  }

  void FinalizeGeLib() {
    auto instance_ptr = ge::GELib::GetInstance();
    if (instance_ptr != nullptr) {
      instance_ptr->Finalize();
    }
  }
};

namespace {
GeTensorDesc MakeInputH2DOverlapTensorDesc(const int64_t size) {
  GeTensorDesc tensor_desc(GeShape({1}), FORMAT_ND, DT_FLOAT);
  TensorUtils::SetSize(tensor_desc, size);
  return tensor_desc;
}

NodePtr AddInputH2DOverlapDataNode(const ComputeGraphPtr &graph, const string &name, const int64_t input_index,
                                   const int64_t output_offset, const int64_t size) {
  auto op_desc = std::make_shared<OpDesc>(name, DATA);
  const auto tensor_desc = MakeInputH2DOverlapTensorDesc(size);
  (void)op_desc->AddOutputDesc(tensor_desc);
  op_desc->SetOutputOffset({output_offset});
  (void)AttrUtils::SetInt(op_desc, ATTR_NAME_INDEX, input_index);
  return graph->AddNode(op_desc);
}

NodePtr AddInputH2DOverlapOpNode(const ComputeGraphPtr &graph, const string &name, const string &type,
                                 const uint32_t input_num, const uint32_t output_num, const int64_t stream_id,
                                 const int64_t size = 16) {
  auto op_desc = std::make_shared<OpDesc>(name, type);
  const auto tensor_desc = MakeInputH2DOverlapTensorDesc(size);
  for (uint32_t i = 0U; i < input_num; ++i) {
    (void)op_desc->AddInputDesc(tensor_desc);
  }
  for (uint32_t i = 0U; i < output_num; ++i) {
    (void)op_desc->AddOutputDesc(tensor_desc);
  }
  op_desc->SetStreamId(stream_id);
  return graph->AddNode(op_desc);
}

}  // namespace

TEST_F(UtestModelBuilderTest, AnalyzeInputs_SimpleDataConsumers) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_simple");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 1, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  EXPECT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  ASSERT_EQ(candidate_plan.inputs.size(), 2U);
  EXPECT_EQ(candidate_plan.inputs[0].input_index, 0U);
  EXPECT_EQ(candidate_plan.inputs[0].data_node_name, "data0");
  EXPECT_EQ(candidate_plan.inputs[0].size, 64);
  ASSERT_EQ(candidate_plan.inputs[0].wait_points.size(), 1U);
  EXPECT_EQ(candidate_plan.inputs[0].wait_points[0].consumer_node_name, "relu0");
  EXPECT_EQ(candidate_plan.inputs[0].wait_points[0].logical_stream_id, 0);

  EXPECT_EQ(candidate_plan.inputs[1].input_index, 1U);
  EXPECT_EQ(candidate_plan.inputs[1].data_node_name, "data1");
  EXPECT_EQ(candidate_plan.inputs[1].size, 128);
  ASSERT_EQ(candidate_plan.inputs[1].wait_points.size(), 1U);
  EXPECT_EQ(candidate_plan.inputs[1].wait_points[0].consumer_node_name, "relu1");
  EXPECT_EQ(candidate_plan.inputs[1].wait_points[0].logical_stream_id, 1);
  EXPECT_EQ(candidate_plan.WaitPointCount(), 2U);
}

TEST_F(UtestModelBuilderTest, AnalyzeInputs_KeepEarliestConsumer) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_multi_stream");
  auto data = AddInputH2DOverlapDataNode(graph, "data", 0, 512, 32);
  auto relu_early = AddInputH2DOverlapOpNode(graph, "relu_early", "Relu", 1U, 1U, 2, 32);
  auto relu_later = AddInputH2DOverlapOpNode(graph, "relu_later", "Relu", 1U, 1U, 2, 32);
  auto relu_other = AddInputH2DOverlapOpNode(graph, "relu_other", "Relu", 1U, 1U, 3, 32);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data->GetOutDataAnchor(0), relu_early->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu_early->GetOutDataAnchor(0), relu_later->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data->GetOutDataAnchor(0), relu_other->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu_later->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu_other->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  EXPECT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  ASSERT_EQ(candidate_plan.inputs.size(), 1U);
  ASSERT_EQ(candidate_plan.inputs[0].wait_points.size(), 1U);
  EXPECT_EQ(candidate_plan.inputs[0].wait_points[0].consumer_node_name, "relu_other");
  EXPECT_EQ(candidate_plan.inputs[0].wait_points[0].logical_stream_id, 3);
}

TEST_F(UtestModelBuilderTest, AnalyzeInputs_SkipIneligibleDataNodes) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_skip");
  auto data_without_offset = std::make_shared<OpDesc>("data_without_offset", DATA);
  (void)data_without_offset->AddOutputDesc(MakeInputH2DOverlapTensorDesc(16));
  (void)AttrUtils::SetInt(data_without_offset, ATTR_NAME_INDEX, 0);
  auto data_without_offset_node = graph->AddNode(data_without_offset);
  auto data_to_output = AddInputH2DOverlapDataNode(graph, "data_to_output", 1, 1024, 16);
  auto relu = AddInputH2DOverlapOpNode(graph, "relu", "Relu", 1U, 1U, 0, 16);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data_without_offset_node->GetOutDataAnchor(0), relu->GetInDataAnchor(0)),
            GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data_to_output->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  EXPECT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  EXPECT_TRUE(candidate_plan.empty());
  EXPECT_EQ(candidate_plan.WaitPointCount(), 0U);
}

TEST_F(UtestModelBuilderTest, AnalyzeInputs_SkipNoTaskAndGeLocalNodes) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_skip_no_task");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 64);
  auto no_task = AddInputH2DOverlapOpNode(graph, "no_task", "Identity", 1U, 1U, 0, 64);
  auto ge_local = AddInputH2DOverlapOpNode(graph, "ge_local", "Identity", 1U, 1U, 1, 64);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 2, 64);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 3, 64);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  (void)AttrUtils::SetBool(no_task->GetOpDesc(), ATTR_NAME_NOTASK, true);
  ge_local->GetOpDesc()->SetOpKernelLibName(kEngineNameGeLocal);
  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), no_task->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(no_task->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), ge_local->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(ge_local->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  EXPECT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  ASSERT_EQ(candidate_plan.inputs.size(), 2U);
  ASSERT_EQ(candidate_plan.inputs[0].wait_points.size(), 1U);
  EXPECT_EQ(candidate_plan.inputs[0].wait_points[0].consumer_node_name, "relu0");
  EXPECT_EQ(candidate_plan.inputs[0].wait_points[0].logical_stream_id, 2);
  ASSERT_EQ(candidate_plan.inputs[1].wait_points.size(), 1U);
  EXPECT_EQ(candidate_plan.inputs[1].wait_points[0].consumer_node_name, "relu1");
  EXPECT_EQ(candidate_plan.inputs[1].wait_points[0].logical_stream_id, 3);
}

TEST_F(UtestModelBuilderTest, SelectInputs_AdmitsInputsAsRuntimeResolved) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_admit_runtime_resolved");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 1, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  ASSERT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  InputH2DOverlapCopyGroupPlan group_plan;
  EXPECT_EQ(SelectInputs(candidate_plan, group_plan), SUCCESS);
  ASSERT_EQ(group_plan.groups.size(), 2U);
  EXPECT_EQ(group_plan.InputCount(), 2U);
  EXPECT_EQ(group_plan.WaitPointCount(), 2U);
  ASSERT_EQ(group_plan.groups[0].inputs.size(), 1U);
  EXPECT_EQ(group_plan.groups[0].inputs[0].input_index, 0U);
  EXPECT_EQ(group_plan.groups[0].inputs[0].data_node_name, "data0");
  EXPECT_EQ(group_plan.groups[0].inputs[0].size, 64U);
  ASSERT_EQ(group_plan.groups[0].wait_points.size(), 1U);
  EXPECT_EQ(group_plan.groups[0].wait_points[0].consumer_node_name, "relu0");
  ASSERT_EQ(group_plan.groups[1].inputs.size(), 1U);
  EXPECT_EQ(group_plan.groups[1].inputs[0].input_index, 1U);
  EXPECT_EQ(group_plan.groups[1].inputs[0].data_node_name, "data1");
  EXPECT_EQ(group_plan.groups[1].inputs[0].size, 128U);
  ASSERT_EQ(group_plan.groups[1].wait_points.size(), 1U);
  EXPECT_EQ(group_plan.groups[1].wait_points[0].consumer_node_name, "relu1");
}

TEST_F(UtestModelBuilderTest, SelectInputs_BuildsOneGroupPerInput) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_copy_group");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 1, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  ASSERT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  InputH2DOverlapCopyGroupPlan group_plan;
  EXPECT_EQ(SelectInputs(candidate_plan, group_plan), SUCCESS);
  ASSERT_EQ(group_plan.groups.size(), 2U);
  EXPECT_EQ(group_plan.InputCount(), 2U);
  EXPECT_EQ(group_plan.WaitPointCount(), 2U);

  ASSERT_EQ(group_plan.groups[0].inputs.size(), 1U);
  EXPECT_EQ(group_plan.groups[0].inputs[0].input_index, 0U);
  ASSERT_EQ(group_plan.groups[0].wait_points.size(), 1U);
  EXPECT_EQ(group_plan.groups[0].wait_points[0].consumer_node_name, "relu0");

  ASSERT_EQ(group_plan.groups[1].inputs.size(), 1U);
  EXPECT_EQ(group_plan.groups[1].inputs[0].input_index, 1U);
  ASSERT_EQ(group_plan.groups[1].wait_points.size(), 1U);
  EXPECT_EQ(group_plan.groups[1].wait_points[0].consumer_node_name, "relu1");
}

TEST_F(UtestModelBuilderTest, SelectInputs_OrdersGroupsByEarliestConsumer) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_copy_group_order");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 128);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 0, 128);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 1, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  ASSERT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  InputH2DOverlapCopyGroupPlan group_plan;
  ASSERT_EQ(SelectInputs(candidate_plan, group_plan), SUCCESS);
  ASSERT_EQ(group_plan.groups.size(), 2U);
  ASSERT_EQ(group_plan.groups[0].inputs.size(), 1U);
  ASSERT_EQ(group_plan.groups[0].wait_points.size(), 1U);
  EXPECT_EQ(group_plan.groups[0].inputs[0].input_index, 1U);
  EXPECT_EQ(group_plan.groups[0].wait_points[0].consumer_node_name, "relu1");
  ASSERT_EQ(group_plan.groups[1].inputs.size(), 1U);
  ASSERT_EQ(group_plan.groups[1].wait_points.size(), 1U);
  EXPECT_EQ(group_plan.groups[1].inputs[0].input_index, 0U);
  EXPECT_EQ(group_plan.groups[1].wait_points[0].consumer_node_name, "add");
}

TEST_F(UtestModelBuilderTest, SelectInputs_KeepsSeparateBaseGroupsOnSameStream) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_copy_group_merge");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  ASSERT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  InputH2DOverlapCopyGroupPlan group_plan;
  ASSERT_EQ(SelectInputs(candidate_plan, group_plan), SUCCESS);

  ASSERT_EQ(group_plan.groups.size(), 2U);
  ASSERT_EQ(group_plan.groups[0].inputs.size(), 1U);
  EXPECT_EQ(group_plan.groups[0].inputs[0].input_index, 0U);
  ASSERT_EQ(group_plan.groups[0].wait_points.size(), 1U);
  EXPECT_EQ(group_plan.groups[0].wait_points[0].consumer_node_name, "relu0");
  ASSERT_EQ(group_plan.groups[1].inputs.size(), 1U);
  EXPECT_EQ(group_plan.groups[1].inputs[0].input_index, 1U);
  ASSERT_EQ(group_plan.groups[1].wait_points.size(), 1U);
  EXPECT_EQ(group_plan.groups[1].wait_points[0].consumer_node_name, "relu1");
  EXPECT_EQ(group_plan.WaitPointCount(), 2U);
}

TEST_F(UtestModelBuilderTest, SelectInputs_RejectInputWithoutWaitPoint) {
  InputH2DOverlapCandidatePlan candidate_plan;
  InputH2DOverlapCandidateInput input;
  input.input_index = 0U;
  input.size = 16;
  candidate_plan.inputs.emplace_back(input);

  InputH2DOverlapCopyGroupPlan group_plan;
  EXPECT_NE(SelectInputs(candidate_plan, group_plan), SUCCESS);
}

TEST_F(UtestModelBuilderTest, BuildPendingPlan_BuildsWaitRequests) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_pending_plan");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  ASSERT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  InputH2DOverlapCopyGroupPlan group_plan;
  ASSERT_EQ(SelectInputs(candidate_plan, group_plan), SUCCESS);

  InputH2DOverlapPendingPlan pending_plan;
  ASSERT_EQ(BuildPendingPlan(group_plan, pending_plan), SUCCESS);
  EXPECT_EQ(pending_plan.version, 1U);
  EXPECT_EQ(pending_plan.copy_stream_id, UINT32_MAX);
  ASSERT_EQ(pending_plan.copy_group_plan.groups.size(), 2U);
  ASSERT_EQ(pending_plan.copy_group_plan.groups[0].inputs.size(), 1U);
  ASSERT_EQ(pending_plan.copy_group_plan.groups[1].inputs.size(), 1U);
  EXPECT_EQ(pending_plan.InputCount(), 2U);
  ASSERT_EQ(pending_plan.WaitRequestCount(), 2U);

  EXPECT_EQ(pending_plan.wait_requests[0].copy_group_index, 0U);
  EXPECT_EQ(pending_plan.wait_requests[0].input_index, 0U);
  EXPECT_EQ(pending_plan.wait_requests[0].consumer_node_name, "add");
  EXPECT_EQ(pending_plan.wait_requests[0].consumer_node, add);
  EXPECT_EQ(pending_plan.wait_requests[1].copy_group_index, 1U);
  EXPECT_EQ(pending_plan.wait_requests[1].input_index, 1U);
  EXPECT_EQ(pending_plan.wait_requests[1].consumer_node_name, "add");
  EXPECT_EQ(pending_plan.wait_requests[1].consumer_node, add);
}

TEST_F(UtestModelBuilderTest, BuildPendingPlan_RejectInvalidGroup) {
  InputH2DOverlapCopyGroupPlan group_plan;
  group_plan.groups.emplace_back();

  InputH2DOverlapPendingPlan pending_plan;
  EXPECT_NE(BuildPendingPlan(group_plan, pending_plan), SUCCESS);
}

TEST_F(UtestModelBuilderTest, BuildPendingPlan_RejectNullConsumerNode) {
  InputH2DOverlapCopyGroupPlan group_plan;
  InputH2DOverlapCopyGroup group;
  InputH2DOverlapCopyInput input;
  input.input_index = 0U;
  input.size = 16U;
  group.inputs.emplace_back(input);
  group.wait_points.emplace_back();
  group_plan.groups.emplace_back(group);

  InputH2DOverlapPendingPlan pending_plan;
  EXPECT_NE(BuildPendingPlan(group_plan, pending_plan), SUCCESS);
}

TEST_F(UtestModelBuilderTest, AddCopyStream_AppendsOneStream) {
  InputH2DOverlapPendingPlan pending_plan;
  InputH2DOverlapCopyGroup group;
  group.inputs.emplace_back();
  group.wait_points.emplace_back();
  pending_plan.copy_group_plan.groups.emplace_back(group);
  int64_t stream_num = 3;

  ASSERT_EQ(AddCopyStream(pending_plan, stream_num), SUCCESS);
  EXPECT_EQ(pending_plan.copy_stream_id, 3U);
  EXPECT_EQ(stream_num, 4);
}

TEST_F(UtestModelBuilderTest, AddCopyStream_RejectInvalidStreamNum) {
  InputH2DOverlapPendingPlan pending_plan;
  InputH2DOverlapCopyGroup group;
  group.inputs.emplace_back();
  group.wait_points.emplace_back();
  pending_plan.copy_group_plan.groups.emplace_back(group);
  int64_t stream_num = 0;

  EXPECT_NE(AddCopyStream(pending_plan, stream_num), SUCCESS);
  EXPECT_EQ(pending_plan.copy_stream_id, UINT32_MAX);
  EXPECT_EQ(stream_num, 0);
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_StoresPendingPlanWithoutChangingGraph) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_plan");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto no_task = AddInputH2DOverlapOpNode(graph, "no_task", "Identity", 1U, 1U, 0, 64);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  (void)AttrUtils::SetBool(no_task->GetOpDesc(), ATTR_NAME_NOTASK, true);
  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), no_task->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(no_task->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const std::string manual_config = "0,1,2";
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

  ASSERT_EQ(ret, SUCCESS);
  EXPECT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 2U);
  EXPECT_EQ(graph->FindNode("input_h2d_model_builder_plan_Recv_0"), nullptr);
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_MultiStreamSkipsManualPlan) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_multi_stream_skip");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 1, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;
  builder.stream_allocator_.stream_num_ = 2;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  graph_options["ge.compile.h2dOverlappedWithCompute"] = "0,1,2";
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

  ASSERT_EQ(ret, SUCCESS);
  EXPECT_TRUE(builder.stream_allocator_.GetStandaloneWaitEvents().empty());
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_SubMemOffsetsUsesRuntimeResolvedInputs) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_sub_mem");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 0, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 1024, 128);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 1, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 2048U;
  builder.zero_copy_mem_size_ = 0U;
  builder.sub_mem_offsets_ = {
      {RT_MEMORY_HBM, 0, 512, 0},
      {RT_MEMORY_HBM, 512, 512, 0},
      {RT_MEMORY_HBM, 1024, 512, 0},
      {RT_MEMORY_HBM, 1536, 512, 0},
  };

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

  ASSERT_EQ(ret, SUCCESS);
  ASSERT_EQ(builder.stream_allocator_.AssignStandaloneWaitEvents(), SUCCESS);
  builder.stream_num_ = 2;
  builder.event_num_ = 2;
  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);

  domi::ModelTaskDef model_task_def;
  auto wait_task0 = model_task_def.add_task();
  wait_task0->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
  wait_task0->set_stream_id(0U);
  wait_task0->set_event_id(0U);
  auto wait_task1 = model_task_def.add_task();
  wait_task1->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
  wait_task1->set_stream_id(1U);
  wait_task1->set_event_id(1U);

  ge::Model model;
  const size_t task_size = model_task_def.ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  ASSERT_TRUE(model_task_def.SerializeToArray(serial_buff.GetData(), static_cast<int32_t>(serial_buff.GetSize())));
  ASSERT_TRUE(AttrUtils::SetZeroCopyBytes(model, MODEL_ATTR_TASKS, std::move(serial_buff)));
  ASSERT_EQ(builder.SaveInputH2DOverlapPlan(model), SUCCESS);

  InputH2DOverlapFinalPlan saved_plan;
  ASSERT_TRUE(input_h2d_overlap_test::GetPlanAttr(model, saved_plan));
  ASSERT_EQ(saved_plan.groups.size(), 2U);
  ASSERT_EQ(saved_plan.groups[0].inputs.size(), 1U);
  EXPECT_EQ(saved_plan.groups[0].inputs[0].input_index, 0U);
  ASSERT_EQ(saved_plan.groups[1].inputs.size(), 1U);
  EXPECT_EQ(saved_plan.groups[1].inputs[0].input_index, 1U);
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigAllowsSingleCopyGroup) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_default_single_group");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 1U, 1U, 0, 64);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 256U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

  ASSERT_EQ(ret, SUCCESS);
  EXPECT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 1U);
  EXPECT_EQ(graph->FindNode("input_h2d_model_builder_default_single_group_Recv_0"), nullptr);
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigAllowsSmallPlannedBytes) {
  {
    auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_small_bytes_config");
    auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 16);
    auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 16);
    auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 16);
    auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 1, 16);
    auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

    ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

    Graph2SubGraphInfoList subgraphs;
    std::map<std::string, int32_t> stream_max_parallel_num;
    ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
    builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
    builder.zero_copy_mem_size_ = 0U;

    const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
    auto graph_options = old_graph_options;
    const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
    graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
    ge::GetThreadLocalContext().SetGraphOption(graph_options);
    const auto ret = builder.PrepareInputH2DOverlap();
    ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

    ASSERT_EQ(ret, SUCCESS);
    EXPECT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 2U);
  }
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigAllowsSmallNonFirstGroupBytes) {
  {
    auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_small_non_first_bytes_config");
    auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 128);
    auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 16);
    auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 128);
    auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 1, 16);
    auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

    ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

    Graph2SubGraphInfoList subgraphs;
    std::map<std::string, int32_t> stream_max_parallel_num;
    ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
    builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
    builder.zero_copy_mem_size_ = 0U;

    const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
    auto graph_options = old_graph_options;
    const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
    graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
    ge::GetThreadLocalContext().SetGraphOption(graph_options);
    const auto ret = builder.PrepareInputH2DOverlap();
    ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

    ASSERT_EQ(ret, SUCCESS);
    EXPECT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 2U);
  }
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigAllowsLargeFirstGroupRatio) {
  {
    auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_large_first_group_config");
    auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 512);
    auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 768, 128);
    auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 512);
    auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 1, 128);
    auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

    ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

    Graph2SubGraphInfoList subgraphs;
    std::map<std::string, int32_t> stream_max_parallel_num;
    ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
    builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 1024U;
    builder.zero_copy_mem_size_ = 0U;

    const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
    auto graph_options = old_graph_options;
    const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
    graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
    ge::GetThreadLocalContext().SetGraphOption(graph_options);
    const auto ret = builder.PrepareInputH2DOverlap();
    ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

    ASSERT_EQ(ret, SUCCESS);
    EXPECT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 2U);
  }
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigAllowsEarlyFirstConsumerTopoIndex) {
  {
    auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_early_first_consumer_config");
    auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 128);
    auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 128);
    auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
    auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 1, 128);
    auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

    ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

    Graph2SubGraphInfoList subgraphs;
    std::map<std::string, int32_t> stream_max_parallel_num;
    ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
    builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
    builder.zero_copy_mem_size_ = 0U;

    const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
    auto graph_options = old_graph_options;
    const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
    graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
    ge::GetThreadLocalContext().SetGraphOption(graph_options);
    const auto ret = builder.PrepareInputH2DOverlap();
    ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

    ASSERT_EQ(ret, SUCCESS);
    EXPECT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 2U);
  }
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_DefaultDpCountsLegacyItemCost) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_default_legacy_item");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 16);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 16);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 16);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 0, 16);
  auto relu2 = AddInputH2DOverlapOpNode(graph, "relu2", "Relu", 1U, 1U, 0, 16);
  auto relu3 = AddInputH2DOverlapOpNode(graph, "relu3", "Relu", 1U, 1U, 0, 16);
  auto relu4 = AddInputH2DOverlapOpNode(graph, "relu4", "Relu", 1U, 1U, 0, 16);
  auto relu5 = AddInputH2DOverlapOpNode(graph, "relu5", "Relu", 1U, 1U, 0, 16);
  auto relu6 = AddInputH2DOverlapOpNode(graph, "relu6", "Relu", 1U, 1U, 0, 16);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 16);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), relu2->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu2->GetOutDataAnchor(0), relu3->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu3->GetOutDataAnchor(0), relu4->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu4->GetOutDataAnchor(0), relu5->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu5->GetOutDataAnchor(0), relu6->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu6->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  graph_options["ge.compile.h2dOverlappedWithCompute"] = "1";
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

  ASSERT_EQ(ret, SUCCESS);
  const auto &wait_events = builder.stream_allocator_.GetStandaloneWaitEvents();
  ASSERT_EQ(wait_events.size(), 1U);
  ASSERT_NE(wait_events[0].consumer_node, nullptr);
  EXPECT_EQ(wait_events[0].consumer_node->GetName(), "add");
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigAllowsEventWaitOverflowWithinHardwareLimit) {
  {
    auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_event_limit_config");
    constexpr uint32_t kInputCount = 513U;
    constexpr uint32_t kInputSize = 4096U;
    std::vector<NodePtr> relu_nodes;
    relu_nodes.reserve(kInputCount);
    for (uint32_t i = 0U; i < kInputCount; ++i) {
      auto data = AddInputH2DOverlapDataNode(graph, "data" + std::to_string(i), i, i * kInputSize, kInputSize);
      const auto relu = AddInputH2DOverlapOpNode(graph, "relu" + std::to_string(i), "Relu", 1U, 1U, 0, kInputSize);
      ASSERT_EQ(GraphUtils::AddEdge(data->GetOutDataAnchor(0), relu->GetInDataAnchor(0)), GRAPH_SUCCESS);
      relu_nodes.emplace_back(relu);
    }
    auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, kInputCount, 0U, 0);
    for (uint32_t i = 0U; i < kInputCount; ++i) {
      ASSERT_EQ(
          GraphUtils::AddEdge(relu_nodes[i]->GetOutDataAnchor(0), netoutput->GetInDataAnchor(static_cast<int32_t>(i))),
          GRAPH_SUCCESS);
    }
    ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

    Graph2SubGraphInfoList subgraphs;
    std::map<std::string, int32_t> stream_max_parallel_num;
    ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
    builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = static_cast<size_t>(kInputCount) * kInputSize;
    builder.zero_copy_mem_size_ = 0U;

    const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
    auto graph_options = old_graph_options;
    std::vector<std::pair<size_t, size_t>> ranges;
    ranges.reserve(kInputCount);
    for (uint32_t i = 0U; i < kInputCount; ++i) {
      ranges.emplace_back(i, i + 1U);
    }
    const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest(ranges);
    graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
    ge::GetThreadLocalContext().SetGraphOption(graph_options);
    const auto ret = builder.PrepareInputH2DOverlap();
    ge::GetThreadLocalContext().SetGraphOption(old_graph_options);

    ASSERT_EQ(ret, SUCCESS);
    EXPECT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), kInputCount);
  }
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigBoundaryKeepsWholeBaseGroup) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_manual_boundary_group");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const std::string manual_config = "0,2";
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, SUCCESS);
  ASSERT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 1U);

  ASSERT_EQ(builder.stream_allocator_.AssignStandaloneWaitEvents(), SUCCESS);
  builder.stream_num_ = 2;
  builder.event_num_ = 1;
  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);

  domi::ModelTaskDef model_task_def;
  auto wait_task = model_task_def.add_task();
  wait_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
  wait_task->set_stream_id(0U);
  wait_task->set_event_id(0U);

  ge::Model model;
  const size_t task_size = model_task_def.ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  ASSERT_TRUE(model_task_def.SerializeToArray(serial_buff.GetData(), static_cast<int32_t>(serial_buff.GetSize())));
  ASSERT_TRUE(AttrUtils::SetZeroCopyBytes(model, MODEL_ATTR_TASKS, std::move(serial_buff)));
  ASSERT_EQ(builder.SaveInputH2DOverlapPlan(model), SUCCESS);

  InputH2DOverlapFinalPlan saved_plan;
  ASSERT_TRUE(input_h2d_overlap_test::GetPlanAttr(model, saved_plan));
  ASSERT_EQ(saved_plan.groups.size(), 1U);
  ASSERT_EQ(saved_plan.groups[0].inputs.size(), 2U);
  EXPECT_EQ(saved_plan.groups[0].inputs[0].input_index, 0U);
  EXPECT_EQ(saved_plan.groups[0].inputs[0].size, 64U);
  EXPECT_EQ(saved_plan.groups[0].inputs[1].input_index, 1U);
  EXPECT_EQ(saved_plan.groups[0].inputs[1].size, 128U);
  ASSERT_EQ(saved_plan.groups[0].wait_points.size(), 1U);
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigBoundaryAddsImplicitTailGroup) {
  for (const auto &manual_config : std::vector<std::string>{"1,", ",1"}) {
    SCOPED_TRACE(manual_config);
    auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_manual_boundary_tail");
    auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
    auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 64);
    auto data2 = AddInputH2DOverlapDataNode(graph, "data2", 2, 384, 64);
    auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
    auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 0, 64);
    auto relu2 = AddInputH2DOverlapOpNode(graph, "relu2", "Relu", 1U, 1U, 0, 64);
    auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 3U, 0U, 0);

    ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(data2->GetOutDataAnchor(0), relu2->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu2->GetOutDataAnchor(0), netoutput->GetInDataAnchor(2)), GRAPH_SUCCESS);
    ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

    Graph2SubGraphInfoList subgraphs;
    std::map<std::string, int32_t> stream_max_parallel_num;
    ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
    builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
    builder.zero_copy_mem_size_ = 0U;

    const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
    auto graph_options = old_graph_options;
    graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
    ge::GetThreadLocalContext().SetGraphOption(graph_options);
    const auto build_ret = builder.PrepareInputH2DOverlap();
    ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
    ASSERT_EQ(build_ret, SUCCESS);

    const auto &wait_events = builder.stream_allocator_.GetStandaloneWaitEvents();
    ASSERT_EQ(wait_events.size(), 1U);
    ASSERT_NE(wait_events[0].consumer_node, nullptr);
    EXPECT_EQ(wait_events[0].consumer_node->GetName(), "relu1");
  }
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigRejectsInvalidManualString) {
  for (const auto &manual_config : std::vector<std::string>{"0-a", "44", ",", "44,,68", ",44,"}) {
    SCOPED_TRACE(manual_config);
    auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_unsupported_config");
    auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
    auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
    auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 1, 64);
    auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

    ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
    ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
    ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

    Graph2SubGraphInfoList subgraphs;
    std::map<std::string, int32_t> stream_max_parallel_num;
    ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
    builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
    builder.zero_copy_mem_size_ = 0U;

    const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
    auto graph_options = old_graph_options;
    graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
    ge::GetThreadLocalContext().SetGraphOption(graph_options);
    const auto build_ret = builder.PrepareInputH2DOverlap();
    ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
    ASSERT_EQ(build_ret, PARAM_INVALID);
    EXPECT_TRUE(builder.stream_allocator_.GetStandaloneWaitEvents().empty());
  }
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_ConfigBoundaryKeepsWholeInputWaitSet) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_manual_boundary_wait_set");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto relu1 = AddInputH2DOverlapOpNode(graph, "relu1", "Relu", 1U, 1U, 1, 64);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 2U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu1->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu1->GetOutDataAnchor(0), netoutput->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const std::string manual_config = "0,1";
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, SUCCESS);
  ASSERT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 1U);

  ASSERT_EQ(builder.stream_allocator_.AssignStandaloneWaitEvents(), SUCCESS);
  builder.stream_num_ = 2;
  builder.event_num_ = 1;
  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);

  domi::ModelTaskDef model_task_def;
  auto wait_task = model_task_def.add_task();
  wait_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
  wait_task->set_stream_id(0U);
  wait_task->set_event_id(0U);

  ge::Model model;
  const size_t task_size = model_task_def.ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  ASSERT_TRUE(model_task_def.SerializeToArray(serial_buff.GetData(), static_cast<int32_t>(serial_buff.GetSize())));
  ASSERT_TRUE(AttrUtils::SetZeroCopyBytes(model, MODEL_ATTR_TASKS, std::move(serial_buff)));
  ASSERT_EQ(builder.SaveInputH2DOverlapPlan(model), SUCCESS);

  InputH2DOverlapFinalPlan saved_plan;
  ASSERT_TRUE(input_h2d_overlap_test::GetPlanAttr(model, saved_plan));
  ASSERT_EQ(saved_plan.groups.size(), 1U);
  ASSERT_EQ(saved_plan.groups[0].inputs.size(), 1U);
  EXPECT_EQ(saved_plan.groups[0].inputs[0].input_index, 0U);
  ASSERT_EQ(saved_plan.groups[0].wait_points.size(), 1U);
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_RegistersWaits) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_register");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto no_task = AddInputH2DOverlapOpNode(graph, "no_task", "Identity", 1U, 1U, 0, 64);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  (void)AttrUtils::SetBool(no_task->GetOpDesc(), ATTR_NAME_NOTASK, true);
  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), no_task->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(no_task->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, SUCCESS);

  const auto &wait_events = builder.stream_allocator_.GetStandaloneWaitEvents();
  ASSERT_EQ(wait_events.size(), 2U);
  std::set<std::string> consumer_names;
  for (const auto &wait_event : wait_events) {
    ASSERT_NE(wait_event.consumer_node, nullptr);
    consumer_names.emplace(wait_event.consumer_node->GetName());
    EXPECT_EQ(wait_event.event_id, UINT32_MAX);
    EXPECT_FALSE(wait_event.generated);
  }
  EXPECT_EQ(consumer_names, std::set<std::string>({"add", "relu0"}));
  EXPECT_EQ(graph->FindNode("input_h2d_model_builder_register_Recv_0"), nullptr);
}

TEST_F(UtestModelBuilderTest, PrepareInputH2DOverlap_DynamicShapeParentGraphFails) {
  auto root_graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_dynamic_root");
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_dynamic_sub");
  graph->SetParentGraph(root_graph);
  root_graph->AddSubgraph(graph);
  (void)AttrUtils::SetBool(root_graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);

  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;
  builder.stream_num_ = 3;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, UNSUPPORTED);

  EXPECT_TRUE(builder.stream_allocator_.GetStandaloneWaitEvents().empty());
  EXPECT_EQ(builder.stream_num_, 3);
}

TEST_F(UtestModelBuilderTest, AddInputH2DOverlapCopyStream_StoresCopyStream) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_copy_stream");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto no_task = AddInputH2DOverlapOpNode(graph, "no_task", "Identity", 1U, 1U, 0, 64);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  (void)AttrUtils::SetBool(no_task->GetOpDesc(), ATTR_NAME_NOTASK, true);
  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), no_task->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(no_task->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;
  builder.stream_num_ = 3;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, SUCCESS);

  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);
  EXPECT_EQ(builder.stream_num_, 4);
  EXPECT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 2U);
  EXPECT_EQ(graph->FindNode("input_h2d_model_builder_copy_stream_Recv_0"), nullptr);
}

TEST_F(UtestModelBuilderTest, InputH2DOverlapBuilderHelpers_EmptyPlanNoop) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_empty_plan");
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.stream_num_ = 2;
  builder.event_num_ = 3;

  ge::Model model;
  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);
  ASSERT_EQ(builder.SaveInputH2DOverlapPlan(model), SUCCESS);

  EXPECT_TRUE(builder.stream_allocator_.GetStandaloneWaitEvents().empty());
  EXPECT_EQ(builder.stream_num_, 2);
  EXPECT_EQ(builder.event_num_, 3);
  ge::Buffer task_def_bytes;
  EXPECT_FALSE(AttrUtils::GetZeroCopyBytes(model, MODEL_ATTR_TASKS, task_def_bytes));
}

TEST_F(UtestModelBuilderTest, SaveInputH2DOverlapPlan_SerializesModelBuilderPlan) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_finalize");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 1U, 1U, 0, 64);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 256U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, SUCCESS);

  ASSERT_EQ(builder.stream_allocator_.AssignStandaloneWaitEvents(), SUCCESS);
  builder.stream_num_ = 2;
  builder.event_num_ = 1;
  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);

  domi::ModelTaskDef model_task_def;
  auto wait_task = model_task_def.add_task();
  wait_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
  wait_task->set_stream_id(0U);
  wait_task->set_event_id(0U);

  ge::Model model;
  const size_t task_size = model_task_def.ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  google::protobuf::io::ArrayOutputStream array_stream(serial_buff.GetData(),
                                                       static_cast<int32_t>(serial_buff.GetSize()));
  google::protobuf::io::CodedOutputStream output_stream(&array_stream);
  output_stream.SetSerializationDeterministic(true);
  ASSERT_TRUE(model_task_def.SerializeToCodedStream(&output_stream));
  ASSERT_TRUE(AttrUtils::SetZeroCopyBytes(model, MODEL_ATTR_TASKS, std::move(serial_buff)));

  ASSERT_EQ(builder.SaveInputH2DOverlapPlan(model), SUCCESS);

  InputH2DOverlapFinalPlan saved_plan;
  ASSERT_TRUE(input_h2d_overlap_test::GetPlanAttr(model, saved_plan));
  EXPECT_EQ(saved_plan.copy_stream_id, 2U);
  ASSERT_EQ(saved_plan.groups.size(), 1U);
  const auto &group = saved_plan.groups[0];
  ASSERT_EQ(group.inputs.size(), 1U);
  EXPECT_EQ(group.inputs[0].input_index, 0U);
  EXPECT_EQ(group.inputs[0].size, 64U);
  ASSERT_EQ(group.wait_points.size(), 1U);
  EXPECT_EQ(group.wait_points[0].stream_id, 0U);
  EXPECT_EQ(group.wait_points[0].event_id, 0U);
  EXPECT_EQ(group.wait_points[0].wait_task_id, 0U);
}

TEST_F(UtestModelBuilderTest, InputH2DOverlapOptionOnInsertsWaitAndWritesFeaturePlan) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_feature_plan");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto no_task = AddInputH2DOverlapOpNode(graph, "no_task", "Identity", 1U, 1U, 0, 64);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  (void)AttrUtils::SetBool(no_task->GetOpDesc(), ATTR_NAME_NOTASK, true);
  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), no_task->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(no_task->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;
  builder.stream_allocator_.stream_num_ = 1;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, SUCCESS);
  ASSERT_EQ(builder.stream_allocator_.InsertSyncNodesByLogicStream(builder.stream_num_, builder.event_num_,
                                                                   builder.notify_num_),
            SUCCESS);

  const auto &wait_events = builder.stream_allocator_.GetStandaloneWaitEvents();
  ASSERT_EQ(wait_events.size(), 2U);
  std::set<std::string> consumer_names;
  for (const auto &wait_event : wait_events) {
    ASSERT_NE(wait_event.consumer_node, nullptr);
    consumer_names.emplace(wait_event.consumer_node->GetName());
    EXPECT_NE(wait_event.event_id, UINT32_MAX);
    EXPECT_TRUE(wait_event.generated);
    const std::string recv_name = graph->GetName() + "_Recv_" + std::to_string(wait_event.event_id);
    EXPECT_NE(graph->FindNode(recv_name), nullptr);
  }
  EXPECT_EQ(consumer_names, std::set<std::string>({"add", "relu0"}));

  const auto copy_stream_id = static_cast<uint32_t>(builder.stream_num_);
  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);
  EXPECT_GT(builder.stream_num_, static_cast<int64_t>(copy_stream_id));

  domi::ModelTaskDef model_task_def;
  for (const auto &wait_event : wait_events) {
    auto wait_task = model_task_def.add_task();
    wait_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
    wait_task->set_stream_id(static_cast<uint32_t>(wait_event.consumer_node->GetOpDesc()->GetStreamId()));
    wait_task->set_event_id(wait_event.event_id);
  }

  const size_t task_size = model_task_def.ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  google::protobuf::io::ArrayOutputStream array_stream(serial_buff.GetData(),
                                                       static_cast<int32_t>(serial_buff.GetSize()));
  google::protobuf::io::CodedOutputStream output_stream(&array_stream);
  output_stream.SetSerializationDeterministic(true);
  ASSERT_TRUE(model_task_def.SerializeToCodedStream(&output_stream));
  ge::Model model;
  ASSERT_TRUE(AttrUtils::SetZeroCopyBytes(model, MODEL_ATTR_TASKS, std::move(serial_buff)));

  ASSERT_EQ(builder.SaveInputH2DOverlapPlan(model), SUCCESS);

  InputH2DOverlapFinalPlan saved_plan;
  ASSERT_TRUE(input_h2d_overlap_test::GetPlanAttr(model, saved_plan));
  EXPECT_EQ(saved_plan.copy_stream_id, copy_stream_id);
  EXPECT_EQ(saved_plan.groups.size(), 2U);
}

TEST_F(UtestModelBuilderTest, SaveInputH2DOverlapPlan_DefaultMissingWaitTaskFails) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_default_missing_wait");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto no_task = AddInputH2DOverlapOpNode(graph, "no_task", "Identity", 1U, 1U, 0, 64);
  auto relu0 = AddInputH2DOverlapOpNode(graph, "relu0", "Relu", 1U, 1U, 0, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  (void)AttrUtils::SetBool(no_task->GetOpDesc(), ATTR_NAME_NOTASK, true);
  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), no_task->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(no_task->GetOutDataAnchor(0), relu0->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(relu0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 512U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}, {1U, 2U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, SUCCESS);
  ASSERT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 2U);

  ASSERT_EQ(builder.stream_allocator_.AssignStandaloneWaitEvents(), SUCCESS);
  builder.stream_num_ = 2;
  builder.event_num_ = 2;
  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);
  EXPECT_EQ(builder.stream_num_, 3);

  domi::ModelTaskDef model_task_def;
  model_task_def.set_stream_num(2U);
  model_task_def.set_event_num(0U);
  auto task = model_task_def.add_task();
  task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_KERNEL));
  task->set_stream_id(0U);

  ge::Model model;
  const size_t task_size = model_task_def.ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  ASSERT_TRUE(model_task_def.SerializeToArray(serial_buff.GetData(), static_cast<int32_t>(serial_buff.GetSize())));
  ASSERT_TRUE(AttrUtils::SetZeroCopyBytes(model, MODEL_ATTR_TASKS, std::move(serial_buff)));

  EXPECT_NE(builder.SaveInputH2DOverlapPlan(model), SUCCESS);
  InputH2DOverlapFinalPlan saved_plan;
  EXPECT_FALSE(input_h2d_overlap_test::GetPlanAttr(model, saved_plan));
}

TEST_F(UtestModelBuilderTest, SaveInputH2DOverlapPlan_ConfigMissingWaitTaskFails) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_model_builder_config_missing_wait");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 1U, 1U, 0, 64);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int32_t> stream_max_parallel_num;
  ModelBuilder builder(0U, graph, subgraphs, stream_max_parallel_num, false);
  builder.mem_type_to_mem_offset_[RT_MEMORY_HBM] = 256U;
  builder.zero_copy_mem_size_ = 0U;

  const auto old_graph_options = ge::GetThreadLocalContext().GetAllGraphOptions();
  auto graph_options = old_graph_options;
  const auto manual_config = BuildInputH2DOverlapBoundaryConfigForTest({{0U, 1U}});
  graph_options["ge.compile.h2dOverlappedWithCompute"] = manual_config;
  ge::GetThreadLocalContext().SetGraphOption(graph_options);
  const auto build_ret = builder.PrepareInputH2DOverlap();
  ge::GetThreadLocalContext().SetGraphOption(old_graph_options);
  ASSERT_EQ(build_ret, SUCCESS);
  ASSERT_EQ(builder.stream_allocator_.GetStandaloneWaitEvents().size(), 1U);

  ASSERT_EQ(builder.stream_allocator_.AssignStandaloneWaitEvents(), SUCCESS);
  builder.stream_num_ = 2;
  builder.event_num_ = 1;
  ASSERT_EQ(builder.AddInputH2DOverlapCopyStream(), SUCCESS);

  domi::ModelTaskDef model_task_def;
  auto task = model_task_def.add_task();
  task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_KERNEL));
  task->set_stream_id(0U);

  ge::Model model;
  const size_t task_size = model_task_def.ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  ASSERT_TRUE(model_task_def.SerializeToArray(serial_buff.GetData(), static_cast<int32_t>(serial_buff.GetSize())));
  ASSERT_TRUE(AttrUtils::SetZeroCopyBytes(model, MODEL_ATTR_TASKS, std::move(serial_buff)));

  EXPECT_NE(builder.SaveInputH2DOverlapPlan(model), SUCCESS);
}

TEST_F(UtestModelBuilderTest, RegisterWaits_RecordsStandaloneWaitEvents) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_pending_register");
  auto data0 = AddInputH2DOverlapDataNode(graph, "data0", 0, 128, 64);
  auto data1 = AddInputH2DOverlapDataNode(graph, "data1", 1, 256, 128);
  auto add = AddInputH2DOverlapOpNode(graph, "add", "Add", 2U, 1U, 0, 128);
  auto netoutput = AddInputH2DOverlapOpNode(graph, "netoutput", NETOUTPUT, 1U, 0U, 0);

  ASSERT_EQ(GraphUtils::AddEdge(data0->GetOutDataAnchor(0), add->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(data1->GetOutDataAnchor(0), add->GetInDataAnchor(1)), GRAPH_SUCCESS);
  ASSERT_EQ(GraphUtils::AddEdge(add->GetOutDataAnchor(0), netoutput->GetInDataAnchor(0)), GRAPH_SUCCESS);
  ASSERT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);

  InputH2DOverlapCandidatePlan candidate_plan;
  ASSERT_EQ(AnalyzeInputs(graph, candidate_plan), SUCCESS);
  InputH2DOverlapCopyGroupPlan group_plan;
  ASSERT_EQ(SelectInputs(candidate_plan, group_plan), SUCCESS);
  InputH2DOverlapPendingPlan pending_plan;
  ASSERT_EQ(BuildPendingPlan(group_plan, pending_plan), SUCCESS);

  Graph2SubGraphInfoList subgraphs;
  StreamAllocator allocator(graph, subgraphs);
  ASSERT_EQ(RegisterWaits(pending_plan, allocator), SUCCESS);

  const auto &wait_events = allocator.GetStandaloneWaitEvents();
  ASSERT_EQ(wait_events.size(), 2U);
  ASSERT_NE(wait_events[0].consumer_node, nullptr);
  EXPECT_EQ(wait_events[0].consumer_node->GetName(), "add");
  EXPECT_EQ(wait_events[0].event_id, UINT32_MAX);
  EXPECT_FALSE(wait_events[0].generated);
  ASSERT_NE(wait_events[1].consumer_node, nullptr);
  EXPECT_EQ(wait_events[1].consumer_node->GetName(), "add");
  EXPECT_EQ(wait_events[1].event_id, UINT32_MAX);
  EXPECT_FALSE(wait_events[1].generated);
  EXPECT_EQ(graph->FindNode("input_h2d_pending_register_Recv_0"), nullptr);
}

TEST_F(UtestModelBuilderTest, RegisterWaits_RejectPlanWithoutWaitRequest) {
  InputH2DOverlapPendingPlan pending_plan;
  InputH2DOverlapCopyGroup group;
  InputH2DOverlapCopyInput input;
  input.input_index = 0U;
  input.size = 16U;
  group.inputs.emplace_back(input);
  pending_plan.copy_group_plan.groups.emplace_back(group);

  Graph2SubGraphInfoList subgraphs;
  auto graph = std::make_shared<ComputeGraph>("input_h2d_pending_no_wait");
  StreamAllocator allocator(graph, subgraphs);
  EXPECT_NE(RegisterWaits(pending_plan, allocator), SUCCESS);
}

TEST_F(UtestModelBuilderTest, RegisterWaits_RejectNullConsumerNode) {
  InputH2DOverlapPendingPlan pending_plan;
  pending_plan.wait_requests.emplace_back();

  Graph2SubGraphInfoList subgraphs;
  auto graph = std::make_shared<ComputeGraph>("input_h2d_pending_null_consumer");
  StreamAllocator allocator(graph, subgraphs);
  EXPECT_NE(RegisterWaits(pending_plan, allocator), SUCCESS);
}

TEST_F(UtestModelBuilderTest, BuildFinalPlan_RejectUnassignedEvent) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_resolve_unassigned_wait");
  auto relu = AddInputH2DOverlapOpNode(graph, "relu", "Relu", 1U, 1U, 0, 64);

  InputH2DOverlapPendingPlan pending_plan;
  InputH2DOverlapWaitRequest wait_request;
  wait_request.copy_group_index = 0U;
  wait_request.input_index = 0U;
  wait_request.consumer_node_name = "relu";
  wait_request.consumer_node = relu;
  wait_request.logical_stream_id = 0;
  wait_request.consumer_topo_index = 0;
  pending_plan.wait_requests.emplace_back(wait_request);

  Graph2SubGraphInfoList subgraphs;
  StreamAllocator allocator(graph, subgraphs);
  ASSERT_EQ(RegisterWaits(pending_plan, allocator), SUCCESS);

  domi::ModelTaskDef model_task_def;
  InputH2DOverlapFinalPlan final_plan;
  EXPECT_NE(BuildFinalPlan(pending_plan, allocator, 1U, 2U, 1U, model_task_def, final_plan), SUCCESS);
  EXPECT_TRUE(final_plan.empty());
}

TEST_F(UtestModelBuilderTest, BuildFinalPlan_BuildsWaitTaskIds) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_final_plan");
  auto relu = AddInputH2DOverlapOpNode(graph, "relu", "Relu", 1U, 1U, 0, 64);

  InputH2DOverlapPendingPlan pending_plan;
  InputH2DOverlapCopyGroup group;
  InputH2DOverlapCopyInput input;
  input.input_index = 0U;
  input.size = 64U;
  group.inputs.emplace_back(input);
  pending_plan.copy_group_plan.groups.emplace_back(group);
  InputH2DOverlapWaitRequest wait_request;
  wait_request.copy_group_index = 0U;
  wait_request.input_index = 0U;
  wait_request.consumer_node_name = "relu";
  wait_request.consumer_node = relu;
  pending_plan.wait_requests.emplace_back(wait_request);

  Graph2SubGraphInfoList subgraphs;
  StreamAllocator allocator(graph, subgraphs);
  allocator.stream_num_ = 1;
  ASSERT_EQ(RegisterWaits(pending_plan, allocator), SUCCESS);
  int64_t stream_num = 0;
  int64_t event_num = 0;
  int64_t notify_num = 0;
  ASSERT_EQ(allocator.InsertSyncNodesByLogicStream(stream_num, event_num, notify_num), SUCCESS);

  domi::ModelTaskDef model_task_def;
  auto task0 = model_task_def.add_task();
  task0->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_KERNEL));
  task0->set_stream_id(0U);
  auto wait_task = model_task_def.add_task();
  wait_task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
  wait_task->set_stream_id(0U);
  wait_task->set_event_id(0U);

  InputH2DOverlapFinalPlan final_plan;
  ASSERT_EQ(BuildFinalPlan(pending_plan, allocator, 1U, 2U, 1U, model_task_def, final_plan), SUCCESS);
  ASSERT_EQ(final_plan.groups.size(), 1U);
  ASSERT_EQ(final_plan.groups[0].inputs.size(), 1U);
  EXPECT_EQ(final_plan.groups[0].inputs[0].input_index, 0U);
  ASSERT_EQ(final_plan.groups[0].wait_points.size(), 1U);
  EXPECT_EQ(final_plan.groups[0].wait_points[0].stream_id, 0U);
  EXPECT_EQ(final_plan.groups[0].wait_points[0].event_id, 0U);
  EXPECT_EQ(final_plan.groups[0].wait_points[0].wait_task_id, 1U);
  EXPECT_EQ(final_plan.copy_stream_id, 1U);
}

TEST_F(UtestModelBuilderTest, BuildFinalPlan_RejectMissingWaitTask) {
  auto graph = std::make_shared<ComputeGraph>("input_h2d_final_plan_missing_wait");
  auto relu = AddInputH2DOverlapOpNode(graph, "relu", "Relu", 1U, 1U, 0, 64);

  InputH2DOverlapPendingPlan pending_plan;
  pending_plan.copy_group_plan.groups.emplace_back();
  InputH2DOverlapWaitRequest wait_request;
  wait_request.copy_group_index = 0U;
  wait_request.consumer_node_name = "relu";
  wait_request.consumer_node = relu;
  pending_plan.wait_requests.emplace_back(wait_request);

  Graph2SubGraphInfoList subgraphs;
  StreamAllocator allocator(graph, subgraphs);
  allocator.stream_num_ = 1;
  ASSERT_EQ(RegisterWaits(pending_plan, allocator), SUCCESS);
  int64_t stream_num = 0;
  int64_t event_num = 0;
  int64_t notify_num = 0;
  ASSERT_EQ(allocator.InsertSyncNodesByLogicStream(stream_num, event_num, notify_num), SUCCESS);

  domi::ModelTaskDef model_task_def;

  InputH2DOverlapFinalPlan final_plan;
  EXPECT_NE(BuildFinalPlan(pending_plan, allocator, 1U, 2U, 1U, model_task_def, final_plan), SUCCESS);
}

TEST_F(UtestModelBuilderTest, SerializePlan_WritesPlanAttr) {
  InputH2DOverlapFinalPlan final_plan;
  final_plan.version = 1U;
  final_plan.copy_stream_id = 3U;
  InputH2DOverlapFinalCopyGroup group;
  InputH2DOverlapCopyInput input;
  input.input_index = 0U;
  input.size = 64U;
  group.inputs.emplace_back(input);
  InputH2DOverlapCopyInput refreshable_input;
  refreshable_input.input_index = 1U;
  refreshable_input.size = 128U;
  group.inputs.emplace_back(refreshable_input);
  InputH2DOverlapFinalWaitPoint wait_point;
  wait_point.stream_id = 1U;
  wait_point.event_id = 2U;
  wait_point.wait_task_id = 4U;
  group.wait_points.emplace_back(wait_point);
  final_plan.groups.emplace_back(group);

  NamedAttrs plan_attr;
  ASSERT_EQ(SerializePlan(final_plan, plan_attr), SUCCESS);
  InputH2DOverlapFinalPlan decoded_plan;
  ASSERT_TRUE(input_h2d_overlap_test::DecodePlan(plan_attr, decoded_plan));
  EXPECT_EQ(decoded_plan.version, 1U);
  EXPECT_EQ(decoded_plan.copy_stream_id, 3U);
  ASSERT_EQ(decoded_plan.groups.size(), 1U);
  const auto &group_def = decoded_plan.groups[0];
  ASSERT_EQ(group_def.inputs.size(), 2U);
  EXPECT_EQ(group_def.inputs[0].input_index, 0U);
  EXPECT_EQ(group_def.inputs[0].size, 64U);
  EXPECT_EQ(group_def.inputs[1].input_index, 1U);
  EXPECT_EQ(group_def.inputs[1].size, 128U);
  ASSERT_EQ(group_def.wait_points.size(), 1U);
  EXPECT_EQ(group_def.wait_points[0].stream_id, 1U);
  EXPECT_EQ(group_def.wait_points[0].event_id, 2U);
  EXPECT_EQ(group_def.wait_points[0].wait_task_id, 4U);
}

TEST_F(UtestModelBuilderTest, SerializePlan_RejectInvalidGroup) {
  InputH2DOverlapFinalPlan final_plan;
  final_plan.copy_stream_id = 1U;
  InputH2DOverlapFinalCopyGroup group;
  InputH2DOverlapCopyInput input;
  input.input_index = 0U;
  input.size = 64U;
  group.inputs.emplace_back(input);
  final_plan.groups.emplace_back(group);

  NamedAttrs plan_attr;
  EXPECT_NE(SerializePlan(final_plan, plan_attr), SUCCESS);
  std::vector<NamedAttrs> group_attrs;
  EXPECT_FALSE(AttrUtils::GetListNamedAttrs(plan_attr, input_h2d_overlap_test::kPlanAttrGroups, group_attrs));
}

// when check GetMemoryRanges return fail, Assign return fail
TEST_F(UtestModelBuilderTest, SetInputIsConst) {
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("");
  MakeGraph(graph);
  graph->TopologicalSorting();
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);
  EXPECT_EQ(builder.PreBuildModel(), SUCCESS);
}

TEST_F(UtestModelBuilderTest, test_sava_atomic_workspace) {
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("");
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  auto atomic_op_desc = std::make_shared<OpDesc>("Atomic", "Atomic");
  auto op_desc = std::make_shared<OpDesc>("Sum", "Sum");

  map<string, map<int64_t, int64_t>> workspace_info;
  map<int64_t, int64_t> workspace_info_pair;
  workspace_info_pair.insert(std::make_pair(1, 1));
  workspace_info.insert(std::make_pair("1", workspace_info_pair));
  op_desc->SetExtAttr(EXT_ATTR_ATOMIC_WORKSPACE_INFO, workspace_info);
  EXPECT_EQ(builder.SavaAtomicWorkspace(op_desc), SUCCESS);
}

TEST_F(UtestModelBuilderTest, test_save_atomic_bin) {
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("");
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  auto atomic_op_desc = std::make_shared<OpDesc>("Atomic", "Atomic");
  auto kernel_buffer = static_cast<Buffer>(Buffer(10));
  AttrUtils::SetStr(atomic_op_desc, ATTR_NAME_TBE_KERNEL_NAME, "Atomic");
  AttrUtils::SetBytes(atomic_op_desc, ATTR_NAME_TBE_KERNEL_BUFFER, kernel_buffer);

  ge::NodePtr atomic_node = graph->AddNode(atomic_op_desc);
  auto op_desc = std::make_shared<OpDesc>("Sum", "Sum");
  op_desc->SetExtAttr("atomic_clean_node_ptr", atomic_node);
  EXPECT_EQ(builder.SaveAtomicTBEKernel(op_desc), SUCCESS);
}

TEST_F(UtestModelBuilderTest, build_model_for_get_task) {
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("");
  MakeSessionScopeReuseGraph(graph);
  std::map<std::string, std::string> option;
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  MemoryAssigner mem_assigner(graph);
  EXPECT_EQ(mem_assigner.AssignMemory(builder.mem_type_to_mem_offset_, builder.zero_copy_mem_size_), SUCCESS);

  ge::Model model;
  EXPECT_EQ(builder.BuildModelDef(model), SUCCESS);
  int64_t session_scope_mem_offset = 0;
  ge::AttrUtils::GetInt(&model, ATTR_MODEL_SESSION_SCOPE_MEMORY_SIZE, session_scope_mem_offset);
  EXPECT_EQ(session_scope_mem_offset, 1536);
}

TEST_F(UtestModelBuilderTest, test_model_save) {
  DEF_GRAPH(g1) {
    CHAIN(NODE("_arg_0", DATA)->NODE("PartitionedCall_0", PARTITIONEDCALL)->NODE("Node_Output", NETOUTPUT));
    CHAIN(NODE("_arg_1", DATA)->NODE("relu", RELU)->NODE("conv2d", CONV2D)->NODE("PartitionedCall_0"));
    CHAIN(NODE("_arg_2", DATA)->NODE("framework", FRAMEWORKOP)->NODE("PartitionedCall_0"));
  };
  const ComputeGraphPtr graph = ToComputeGraph(g1);
  const auto call_node = graph->FindNode("PartitionedCall_0");
  EXPECT_NE(call_node, nullptr);
  const auto call_desc = call_node->GetOpDesc();
  const auto relu_node = graph->FindNode("relu");
  EXPECT_NE(relu_node, nullptr);
  const auto relu_desc = relu_node->GetOpDesc();
  const auto conv_node = graph->FindNode("conv2d");
  EXPECT_NE(conv_node, nullptr);
  const auto conv_desc = conv_node->GetOpDesc();
  const auto cust_node = graph->FindNode("framework");
  EXPECT_NE(cust_node, nullptr);
  const auto cust_desc = cust_node->GetOpDesc();

  DEF_GRAPH(g2) {
    CHAIN(NODE("sgt/_arg_0", DATA)->NODE("sgt/conv2d", CONV2D)->NODE("sgt/Node_Output", NETOUTPUT));
    CHAIN(NODE("sgt/_arg_1", DATA)->NODE("sgt/conv2d"));
  };
  ComputeGraphPtr sgt_graph = ToComputeGraph(g2);
  const auto ffts_node = sgt_graph->FindNode("sgt/conv2d");
  EXPECT_NE(ffts_node, nullptr);
  const auto ffts_desc = ffts_node->GetOpDesc();

  sgt_graph->SetParentNode(call_node);
  sgt_graph->SetParentGraph(call_node->GetOwnerComputeGraph());
  call_node->GetOpDesc()->AddSubgraphName("f");
  call_node->GetOpDesc()->SetSubgraphInstanceName(0, sgt_graph->GetName());
  graph->AddSubgraph(sgt_graph);
  graph->SetGraphUnknownFlag(true);
  EXPECT_TRUE(AttrUtils::SetBool(call_desc, ATTR_NAME_FFTS_PLUS_SUB_GRAPH, true));

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  size_t tbe_kernel_len = 0U;
  size_t cpu_kernel_len = 0U;
  const Buffer kernel_buffer(20U, 0x11U);
  (void)AttrUtils::SetStr(relu_desc, ATTR_NAME_CUBE_VECTOR_CORE_TYPE, std::string("MIX_AIC"));
  std::vector<std::string> name_prefix = {"_mix_aic", "_mix_aiv"};
  AttrUtils::SetListStr(relu_desc, ATTR_NAME_KERNEL_NAMES_PREFIX, name_prefix);
  AttrUtils::SetStr(relu_desc, std::string("_mix_aic") + ATTR_NAME_TBE_KERNEL_NAME, "relu_aic");
  AttrUtils::SetBytes(relu_desc, std::string("_mix_aic") + ATTR_NAME_TBE_KERNEL_BUFFER, kernel_buffer);
  tbe_kernel_len += (sizeof(KernelStoreItemHead) + std::string("relu_aic").length() + kernel_buffer.size());
  AttrUtils::SetStr(relu_desc, std::string("_mix_aiv") + ATTR_NAME_TBE_KERNEL_NAME, "relu_aiv");
  AttrUtils::SetBytes(relu_desc, std::string("_mix_aiv") + ATTR_NAME_TBE_KERNEL_BUFFER, kernel_buffer);
  tbe_kernel_len += (sizeof(KernelStoreItemHead) + std::string("relu_aiv").length() + kernel_buffer.size());

  AttrUtils::SetStr(conv_desc, ATTR_NAME_TBE_KERNEL_NAME, "conv2d");
  AttrUtils::SetBytes(conv_desc, ATTR_NAME_TBE_KERNEL_BUFFER, kernel_buffer);
  tbe_kernel_len += (sizeof(KernelStoreItemHead) + std::string("conv2d").length() + kernel_buffer.size());

  AttrUtils::SetStr(ffts_desc, ATTR_NAME_TBE_KERNEL_NAME, "sgt/conv2d");
  AttrUtils::SetBytes(ffts_desc, ATTR_NAME_TBE_KERNEL_BUFFER, kernel_buffer);
  tbe_kernel_len += (sizeof(KernelStoreItemHead) + std::string("sgt/conv2d").length() + kernel_buffer.size());

  std::vector<char> data(20U, 0x16U);
  const auto cust_kernel = MakeShared<OpKernelBin>(cust_desc->GetName(), std::move(data));
  cust_desc->SetExtAttr(OP_EXTATTR_CUSTAICPU_KERNEL, cust_kernel);
  cpu_kernel_len += (sizeof(KernelStoreItemHead) + cust_desc->GetName().length() + 20U);

  // add atomic
  auto atomic_op_desc = std::make_shared<OpDesc>("Atomic", "Atomic");
  auto atomic_buffer = static_cast<Buffer>(Buffer(10));
  AttrUtils::SetStr(atomic_op_desc, ATTR_NAME_TBE_KERNEL_NAME, "Atomic");
  AttrUtils::SetBytes(atomic_op_desc, ATTR_NAME_TBE_KERNEL_BUFFER, atomic_buffer);
  ge::NodePtr atomic_node = graph->AddNode(atomic_op_desc);
  ffts_desc->SetExtAttr("memset_node_ptr", atomic_node);
  tbe_kernel_len += (sizeof(KernelStoreItemHead) + std::string("Atomic").length() + atomic_buffer.size());

  ge::Model ge_model;
  ge::GeModel ge_gemodel;
  std::shared_ptr<domi::ModelTaskDef> model_task_def = MakeShared<domi::ModelTaskDef>();
  InitKernelTaskDef(graph, *model_task_def, "relu");
  size_t task_size = model_task_def->ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  google::protobuf::io::ArrayOutputStream array_stream(serial_buff.GetData(),
                                                       static_cast<int32_t>(serial_buff.GetSize()));
  google::protobuf::io::CodedOutputStream output_stream(&array_stream);
  output_stream.SetSerializationDeterministic(true);
  EXPECT_NE(model_task_def->SerializeToCodedStream(&output_stream), FAILED);

  EXPECT_TRUE(AttrUtils::SetZeroCopyBytes(ge_model, MODEL_ATTR_TASKS, std::move(serial_buff)));
  EXPECT_EQ(builder.SaveDataToModel(ge_model, ge_gemodel), SUCCESS);
  EXPECT_NE(conv_desc->TryGetExtAttr(OP_EXTATTR_NAME_TBE_KERNEL, TBEKernelPtr()), nullptr);
  EXPECT_NE(ffts_desc->TryGetExtAttr(OP_EXTATTR_NAME_TBE_KERNEL, TBEKernelPtr()), nullptr);
  EXPECT_NE(relu_desc->TryGetExtAttr(std::string("_mix_aic") + OP_EXTATTR_NAME_TBE_KERNEL, TBEKernelPtr()), nullptr);
  EXPECT_NE(relu_desc->TryGetExtAttr(std::string("_mix_aiv") + OP_EXTATTR_NAME_TBE_KERNEL, TBEKernelPtr()), nullptr);

  EXPECT_FALSE(builder.tbe_kernel_store_.kernels_.empty());
  EXPECT_FALSE(ge_gemodel.tbe_kernel_store_.kernels_.empty());

  // aic_relu + aiv_relu + conv2d + sgt/conv2d + atocic
  EXPECT_EQ(ge_gemodel.tbe_kernel_store_.buffer_.size(), tbe_kernel_len);

  EXPECT_FALSE(builder.cust_aicpu_kernel_store_.kernels_.empty());
  EXPECT_FALSE(ge_gemodel.cust_aicpu_kernel_store_.kernels_.empty());
  EXPECT_EQ(ge_gemodel.cust_aicpu_kernel_store_.buffer_.size(), cpu_kernel_len);  // framework

  std::set<std::string> aicpu_name_set;
  EXPECT_EQ(builder.SaveCustAiCpuKernel(cust_desc, aicpu_name_set), SUCCESS);
  EXPECT_NE(builder.SaveCustAiCpuKernel(cust_desc, aicpu_name_set), SUCCESS);
}

namespace {
void SetDummyModelTaskDef(ge::Model &model) {
  auto model_task_def = ge::MakeShared<domi::ModelTaskDef>();
  ASSERT_NE(model_task_def, nullptr);
  auto *const task_def = model_task_def->add_task();
  ASSERT_NE(task_def, nullptr);
  task_def->set_type(0U);
  const size_t task_size = model_task_def->ByteSizeLong();
  ge::Buffer serial_buff(task_size);
  google::protobuf::io::ArrayOutputStream array_stream(serial_buff.GetData(),
                                                       static_cast<int32_t>(serial_buff.GetSize()));
  google::protobuf::io::CodedOutputStream output_stream(&array_stream);
  output_stream.SetSerializationDeterministic(true);
  ASSERT_TRUE(model_task_def->SerializeToCodedStream(&output_stream));
  ASSERT_TRUE(AttrUtils::SetZeroCopyBytes(model, MODEL_ATTR_TASKS, std::move(serial_buff)));
}

void SetTbeKernelAttrs(const OpDescPtr &op_desc, const std::string &kernel_name, const Buffer &kernel_buffer) {
  ASSERT_TRUE(AttrUtils::SetStr(op_desc, ATTR_NAME_TBE_KERNEL_NAME, kernel_name));
  ASSERT_TRUE(AttrUtils::SetBytes(op_desc, ATTR_NAME_TBE_KERNEL_BUFFER, kernel_buffer));
}
}  // namespace

TEST_F(UtestModelBuilderTest, SaveDataToModelUsesLastBinForSameKernelNameWithDifferentBins) {
  auto graph = std::make_shared<ComputeGraph>("duplicate_tbe_kernel_graph");
  auto first_op = std::make_shared<OpDesc>("first_op", RELU);
  auto second_op = std::make_shared<OpDesc>("second_op", RELU);
  ASSERT_NE(graph->AddNode(first_op), nullptr);
  ASSERT_NE(graph->AddNode(second_op), nullptr);
  SetTbeKernelAttrs(first_op, "shared_kernel", Buffer(4U, 0x1U));
  SetTbeKernelAttrs(second_op, "shared_kernel", Buffer(4U, 0x2U));

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);
  ge::Model model;
  ge::GeModel ge_model;
  SetDummyModelTaskDef(model);

  ASSERT_EQ(builder.SaveDataToModel(model, ge_model), SUCCESS);
  const auto stored_kernel = ge_model.GetTBEKernelStore().FindKernel("shared_kernel");
  ASSERT_NE(stored_kernel, nullptr);
  ASSERT_EQ(stored_kernel->GetBinDataSize(), 4U);
  for (size_t i = 0U; i < stored_kernel->GetBinDataSize(); ++i) {
    EXPECT_EQ(static_cast<uint8_t>(stored_kernel->GetBinData()[i]), 0x2U);
  }
}

TEST_F(UtestModelBuilderTest, AddTBEKernelToStoreTracksCustomUsageForCurrentBin) {
  auto graph = std::make_shared<ComputeGraph>("custom_kernel_origin_graph");
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  auto custom_op = std::make_shared<OpDesc>("custom_op", "CustomOp");
  custom_op->SetOpKernelLibName(kCustomOpKernelLibName);
  auto first_non_custom_op = std::make_shared<OpDesc>("first_non_custom_op", RELU);
  first_non_custom_op->SetOpKernelLibName("AIcoreEngine");
  auto second_non_custom_op = std::make_shared<OpDesc>("second_non_custom_op", RELU);
  second_non_custom_op->SetOpKernelLibName("AIcoreEngine");
  auto third_non_custom_op = std::make_shared<OpDesc>("third_non_custom_op", RELU);
  third_non_custom_op->SetOpKernelLibName("AIcoreEngine");
  auto custom_kernel = std::make_shared<OpKernelBin>("shared_kernel", std::vector<char>{0x1, 0x1});
  auto same_kernel = std::make_shared<OpKernelBin>("shared_kernel", std::vector<char>{0x1, 0x1});
  auto conflicting_kernel = std::make_shared<OpKernelBin>("shared_kernel", std::vector<char>{0x2, 0x2});
  auto next_kernel = std::make_shared<OpKernelBin>("shared_kernel", std::vector<char>{0x3, 0x3});

  ASSERT_EQ(builder.AddTBEKernelToStore(custom_op, custom_kernel, "normal"), SUCCESS);
  ASSERT_EQ(builder.AddTBEKernelToStore(first_non_custom_op, same_kernel, "normal"), SUCCESS);
  auto origin_iter = builder.tbe_kernel_origins_.find("shared_kernel");
  ASSERT_NE(origin_iter, builder.tbe_kernel_origins_.end());
  EXPECT_FALSE(origin_iter->second.is_custom);
  EXPECT_TRUE(origin_iter->second.current_bin_has_custom_user);

  ASSERT_EQ(builder.AddTBEKernelToStore(second_non_custom_op, conflicting_kernel, "normal"), SUCCESS);
  origin_iter = builder.tbe_kernel_origins_.find("shared_kernel");
  ASSERT_NE(origin_iter, builder.tbe_kernel_origins_.end());
  EXPECT_FALSE(origin_iter->second.current_bin_has_custom_user);

  ASSERT_EQ(builder.AddTBEKernelToStore(third_non_custom_op, next_kernel, "normal"), SUCCESS);

  origin_iter = builder.tbe_kernel_origins_.find("shared_kernel");
  ASSERT_NE(origin_iter, builder.tbe_kernel_origins_.end());
  EXPECT_FALSE(origin_iter->second.is_custom);
  EXPECT_FALSE(origin_iter->second.current_bin_has_custom_user);
  EXPECT_EQ(origin_iter->second.op_name, "third_non_custom_op");
  EXPECT_EQ(origin_iter->second.op_type, RELU);
  EXPECT_EQ(origin_iter->second.kernel_type, "normal");
  EXPECT_EQ(builder.tbe_kernel_store_.FindKernel("shared_kernel"), next_kernel);
}

TEST_F(UtestModelBuilderTest, SaveDataToModelReusesSameKernelNameWithSameBin) {
  auto graph = std::make_shared<ComputeGraph>("duplicate_same_tbe_kernel_graph");
  auto first_op = std::make_shared<OpDesc>("first_op", RELU);
  auto second_op = std::make_shared<OpDesc>("second_op", RELU);
  ASSERT_NE(graph->AddNode(first_op), nullptr);
  ASSERT_NE(graph->AddNode(second_op), nullptr);
  const Buffer kernel_buffer(4U, 0x1U);
  SetTbeKernelAttrs(first_op, "shared_kernel", kernel_buffer);
  SetTbeKernelAttrs(second_op, "shared_kernel", kernel_buffer);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);
  ge::Model model;
  ge::GeModel ge_model;
  SetDummyModelTaskDef(model);

  EXPECT_EQ(builder.SaveDataToModel(model, ge_model), SUCCESS);
  const size_t expected_size =
      sizeof(KernelStoreItemHead) + std::string("shared_kernel").length() + kernel_buffer.GetSize();
  EXPECT_EQ(ge_model.GetTBEKernelStore().DataSize(), expected_size);
}

TEST_F(UtestModelBuilderTest, CompileSingleOp) {
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("testmix");
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  EXPECT_NE(builder.CompileSingleOp(), SUCCESS);
}

TEST_F(UtestModelBuilderTest, CompileSingleOpGe) {
  InitGeLib();
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("testmix");
  auto op_desc = std::make_shared<OpDesc>("aac", MEMSET);
  ge::NodePtr node = graph->AddNode(op_desc);
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);
  std::string aicore_engine;
  op_desc->SetOpKernelLibName("GeLocal");
  auto kernel_info_store = MakeShared<ge_local::GeLocalOpsKernelInfoStore>();
  ge::GELib::GetInstance()->OpsKernelManagerObj().ops_kernel_store_["GeLocal"] = kernel_info_store;
  EXPECT_EQ(builder.CompileSingleOp(), SUCCESS);
  FinalizeGeLib();
}

TEST_F(UtestModelBuilderTest, ReuseWeightTest) {
  InitGeLib();
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("testmix");
  MakeGraphWithConst(graph);
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  EXPECT_EQ(builder.PreBuildModel(), SUCCESS);
  size_t expect_weight_size = 2048;
  EXPECT_EQ(builder.weight_offset_, expect_weight_size);
  EXPECT_EQ(builder.MergeWeights(), SUCCESS);
  FinalizeGeLib();
}

TEST_F(UtestModelBuilderTest, build_model_fail_for_l1fusiong_virtual) {
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ComputeGraphPtr graph = std::make_shared<ge::ComputeGraph>("");
  MakeSessionScopeReuseGraph(graph);

  std::string buffer_optimize = "l1_optimize";
  std::string virtual_type = "1";
  std::map<std::string, std::string> options;
  options[BUFFER_OPTIMIZE] = buffer_optimize;
  options[VIRTUAL_TYPE] = virtual_type;
  GetThreadLocalContext().SetGraphOption(options);
  (void)AttrUtils::SetBool(graph, ATTR_NAME_OFF_SUPERKERNEL_ATTR, false);
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  MemoryAssigner mem_assigner(graph);
  EXPECT_EQ(mem_assigner.AssignMemory(builder.mem_type_to_mem_offset_, builder.zero_copy_mem_size_), SUCCESS);

  ge::Model model;

  EXPECT_NE(builder.BuildModelDef(model), SUCCESS);

  std::map<std::string, std::string> options_map_recovery;
  GetThreadLocalContext().SetGraphOption(options_map_recovery);
}

TEST_F(UtestModelBuilderTest, SaveSoftSyncOpWeight_success) {
  std::vector<int64_t> shape = {16};
  DEF_GRAPH(add_graph) {
    auto add = OP_CFG(ADDN)
                   .Attr(TVM_ATTR_NAME_MAGIC, "RT_DEV_BINARY_MAGIC_ELF")
                   .TensorDesc(FORMAT_ND, DT_INT32, shape)
                   .InCnt(3)
                   .Build("add");
    auto data1 =
        OP_CFG(DATA).Attr(ATTR_NAME_INDEX, 0).TensorDesc(FORMAT_ND, DT_INT32, shape).InCnt(1).OutCnt(1).Build("data1");
    data1->SetOutputOffset({0});

    int32_t data_value_vec1[2] = {2, 4};
    GeTensorDesc data_tensor_desc(GeShape({2}), FORMAT_ND, DT_INT32);
    TensorUtils::SetDataOffset(data_tensor_desc, 16);
    TensorUtils::SetWeightSize(data_tensor_desc, 16);
    TensorUtils::SetSize(data_tensor_desc, 16);
    GeTensorPtr data_tensor1 = make_shared<GeTensor>(data_tensor_desc, (uint8_t *)data_value_vec1, 2 * sizeof(int32_t));
    auto const1 = OP_CFG(CONSTANT).Weight(data_tensor1);

    CHAIN(NODE(data1)->EDGE(0, 0)->NODE(add)->NODE("noop", NOOP));
    CHAIN(NODE("const_op", const1)->EDGE(0, 1)->NODE(add));
    CHAIN(NODE("variable", VARIABLE)->EDGE(0, 2)->NODE(add));
  };

  auto graph = ToComputeGraph(add_graph);
  EXPECT_NE(graph, nullptr);

  auto add = graph->FindNode("add");
  EXPECT_NE(add, nullptr);
  auto op_desc = add->GetOpDesc();
  EXPECT_NE(op_desc, nullptr);
  std::map<std::string, uint32_t> input_name_idx;
  input_name_idx["x"] = 0;
  input_name_idx["axes"] = 1;
  input_name_idx["vvv"] = 2;
  op_desc->UpdateInputName(input_name_idx);
  AttrUtils::SetBool(op_desc, ge::ATTR_NAME_STATIC_TO_DYNAMIC_SOFT_SYNC_OP, true);
  std::vector<std::string> depends = {"x", "axes", "vvv", "666"};
  AttrUtils::SetListStr(op_desc, "_op_infer_depends", depends);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);

  EXPECT_EQ(builder.SaveSoftSyncOpWeight(), SUCCESS);
}
// make sure topo sorting before  AssignStreamForDynamicShapeGraph
TEST_F(UtestModelBuilderTest, AssignStreamForDynamicShapeGraph_KeepTopoIdRight) {
  int64_t stream_num, event_num = 0;
  auto graph = gert::ShareGraph::MultiStreamTwoNodeGraph(stream_num, event_num);
  EXPECT_NE(graph, nullptr);
  graph->TopologicalSorting();
  auto add_node = graph->FindFirstNodeMatchType("Add");
  auto relu_node = graph->FindFirstNodeMatchType("Relu");
  EXPECT_EQ(add_node->GetOpDescBarePtr()->GetId(), 2);
  EXPECT_EQ(relu_node->GetOpDescBarePtr()->GetId(), 3);

  // make topo id a mess
  add_node->GetOpDescBarePtr()->SetId(3);
  relu_node->GetOpDescBarePtr()->SetId(2);
  EXPECT_EQ(add_node->GetOpDescBarePtr()->GetId(), 3);
  EXPECT_EQ(relu_node->GetOpDescBarePtr()->GetId(), 2);

  setenv("ENABLE_DYNAMIC_SHAPE_MULTI_STREAM", "1", 0);

  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);
  builder.AssignStreamForDynamicShapeGraph(graph);

  // check topo id is right
  EXPECT_EQ(add_node->GetOpDescBarePtr()->GetId(), 2);
  EXPECT_EQ(relu_node->GetOpDescBarePtr()->GetId(), 3);
  unsetenv("ENABLE_DYNAMIC_SHAPE_MULTI_STREAM");
}

TEST_F(UtestModelBuilderTest, AssignStreamForDynamicShapeGraph_EnableCvParallel_Fail) {
  int64_t stream_num, event_num = 0;
  auto graph = gert::ShareGraph::MultiStreamTwoNodeGraph(stream_num, event_num);
  EXPECT_NE(graph, nullptr);
  graph->TopologicalSorting();

  setenv("ENABLE_DYNAMIC_SHAPE_MULTI_STREAM", "1", 0);

  const auto back_options = ge::GetThreadLocalContext().GetAllSessionOptions();
  auto options = back_options;
  options["ge.autoMultistreamParallelMode"] = "cv";
  ge::GetThreadLocalContext().SetSessionOption(options);
  Graph2SubGraphInfoList subgraphs;
  std::map<std::string, int> stream_max_parallel_num;
  ge::ModelBuilder builder(0, graph, subgraphs, stream_max_parallel_num, false);
  ASSERT_NE(builder.AssignStreamForDynamicShapeGraph(graph), SUCCESS);

  unsetenv("ENABLE_DYNAMIC_SHAPE_MULTI_STREAM");
}
}  // namespace ge
