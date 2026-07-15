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

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "common/taskdown_common.h"
#include "es_ge_test_ops.h"
#include "ge/st/stubs/utils/mock_ops_kernel_builder.h"
#include "ge_running_env/ge_running_env_faker.h"
#include "graph/build/graph_builder.h"
#include "graph/build/input_h2d_overlap_constants.h"
#include "graph/build/input_h2d_overlap_planner.h"
#include "graph/build/input_h2d_overlap_test_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/ge_local_context.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "register/ops_kernel_builder_registry.h"

namespace ge {
namespace {
constexpr char kSessionId[] = "input_h2d_overlap_compile_st";

class ScopedGraphOptions {
 public:
  explicit ScopedGraphOptions(const std::string &input_h2d_overlap_config, const bool has_option = true)
      : old_options_(GetThreadLocalContext().GetAllGraphOptions()) {
    auto new_options = old_options_;
    if (has_option) {
      new_options[kInputH2DOverlapOptionName] = input_h2d_overlap_config;
    } else {
      (void)new_options.erase(kInputH2DOverlapOptionName);
    }
    GetThreadLocalContext().SetGraphOption(new_options);
  }

  ~ScopedGraphOptions() {
    GetThreadLocalContext().SetGraphOption(old_options_);
  }

 private:
  std::map<std::string, std::string> old_options_;
};

std::vector<int64_t> MakeShape(const int64_t size) {
  constexpr int64_t kFloatBytes = 4;
  return {(size + kFloatBytes - 1) / kFloatBytes};
}

void SetInputH2DOverlapKernelLibs(const ComputeGraphPtr &graph) {
  if (graph == nullptr) {
    return;
  }
  for (const auto &node : graph->GetDirectNode()) {
    const auto op_desc = node->GetOpDesc();
    if (node->GetType() == DATA) {
      op_desc->SetOpEngineName("DNN_VM_GE_LOCAL");
      op_desc->SetOpKernelLibName("DNN_VM_GE_LOCAL_OP_STORE");
    } else {
      op_desc->SetOpEngineName("AIcoreEngine");
      op_desc->SetOpKernelLibName("AiCoreLib");
    }
  }
}

ComputeGraphPtr FinishInputH2DOverlapGraph(const std::unique_ptr<Graph> &graph_holder) {
  if (graph_holder == nullptr) {
    return nullptr;
  }
  auto graph = GraphUtilsEx::GetComputeGraph(*graph_holder);
  SetInputH2DOverlapKernelLibs(graph);
  EXPECT_EQ(graph->TopologicalSorting(), GRAPH_SUCCESS);
  return graph;
}

ComputeGraphPtr BuildInputH2DOverlapCompileGraph() {
  es::EsGraphBuilder es_graph("input_h2d_overlap_compile_st");
  auto data0 = es_graph.CreateInput(0, "data0", DATA).SetShape(MakeShape(64));
  auto data1 = es_graph.CreateInput(1, "data1", DATA).SetShape(MakeShape(128));
  auto no_task = es::Identity(data0);
  EXPECT_EQ(no_task.SetAttrForNode(ATTR_NAME_NOTASK.c_str(), true), SUCCESS);
  auto relu0 = es::Relu(no_task);
  auto add = es::Add(relu0, data1);
  EXPECT_EQ(es::EsGraphBuilder::SetOutput(add, 0), 0);
  return FinishInputH2DOverlapGraph(es_graph.BuildAndReset());
}

ComputeGraphPtr BuildInputH2DOverlapDefaultDpGraph() {
  constexpr int64_t kLargeInputSize = 1024 * 1024;
  es::EsGraphBuilder es_graph("input_h2d_overlap_default_dp_st");
  auto data0 = es_graph.CreateInput(0, "data0", DATA).SetShape(MakeShape(kLargeInputSize));
  auto data1 = es_graph.CreateInput(1, "data1", DATA).SetShape(MakeShape(kLargeInputSize));
  auto data2 = es_graph.CreateInput(2, "data2", DATA).SetShape(MakeShape(kLargeInputSize));
  auto data3 = es_graph.CreateInput(3, "data3", DATA).SetShape(MakeShape(kLargeInputSize));
  auto relu0 = es::Relu(data0);
  auto relu1 = es::Relu(relu0);
  auto add0 = es::Add(relu1, data1);
  auto add1 = es::Add(add0, data2);
  auto add2 = es::Add(add1, data3);
  EXPECT_EQ(es::EsGraphBuilder::SetOutput(add2, 0), 0);
  return FinishInputH2DOverlapGraph(es_graph.BuildAndReset());
}

ComputeGraphPtr BuildInputH2DOverlapNoCandidateGraph() {
  es::EsGraphBuilder es_graph("input_h2d_overlap_no_candidate_st");
  auto data0 = es_graph.CreateInput(0, "data0", DATA).SetShape(MakeShape(64));
  EXPECT_EQ(es::EsGraphBuilder::SetOutput(data0, 0), 0);
  return FinishInputH2DOverlapGraph(es_graph.BuildAndReset());
}

ComputeGraphPtr BuildInputH2DOverlapMultiStreamGraph() {
  es::EsGraphBuilder es_graph("input_h2d_overlap_multi_stream_st");
  auto data0 = es_graph.CreateInput(0, "data0", DATA).SetShape(MakeShape(64));
  auto data1 = es_graph.CreateInput(1, "data1", DATA).SetShape(MakeShape(64));
  auto relu0 = es::Relu(data0);
  auto relu1 = es::Relu(data1);
  EXPECT_EQ(relu0.SetAttrForNode(public_attr::USER_STREAM_LABEL, "stream0"), SUCCESS);
  EXPECT_EQ(relu1.SetAttrForNode(public_attr::USER_STREAM_LABEL, "stream1"), SUCCESS);
  EXPECT_EQ(es::EsGraphBuilder::SetOutput(relu0, 0), 0);
  EXPECT_EQ(es::EsGraphBuilder::SetOutput(relu1, 1), 0);
  return FinishInputH2DOverlapGraph(es_graph.BuildAndReset());
}

Status BuildRootModelWithOption(ComputeGraphPtr root_graph, const std::string &option, GeRootModelPtr &root_model) {
  GraphBuilder graph_builder;
  ScopedGraphOptions option_guard(option);
  return graph_builder.Build(root_graph, root_model);
}

GeModelPtr GetOnlyModel(const GeRootModelPtr &root_model) {
  if (root_model == nullptr) {
    return nullptr;
  }
  const auto &name_to_model = root_model->GetSubgraphInstanceNameToModel();
  if (name_to_model.size() != 1U) {
    return nullptr;
  }
  return name_to_model.begin()->second;
}

bool DecodeInputH2DOverlapPlanFromModel(const GeModelPtr &model, input_h2d_overlap::InputH2DOverlapFinalPlan &plan) {
  NamedAttrs plan_attr;
  return (model != nullptr) && AttrUtils::GetNamedAttrs(model, input_h2d_overlap_test::kPlanAttrName, plan_attr) &&
         input_h2d_overlap_test::DecodePlan(plan_attr, plan);
}

Status GenerateTaskForInputH2DOverlapSt(const Node &node, RunContext &, std::vector<domi::TaskDef> &tasks) {
  const auto op_desc = node.GetOpDesc();
  GE_CHECK_NOTNULL(op_desc);
  if (node.GetType() == DATA) {
    return SUCCESS;
  }

  domi::TaskDef task_def;
  const auto stream_id = op_desc->GetStreamId();
  if (stream_id >= 0) {
    task_def.set_stream_id(static_cast<uint32_t>(stream_id));
  }
  if (node.GetType() == RECV) {
    int64_t event_id = 0;
    (void)AttrUtils::GetInt(op_desc, RECV_ATTR_EVENT_ID, event_id);
    task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
    task_def.set_event_id(static_cast<uint32_t>(event_id));
  } else if (node.GetType() == SEND) {
    int64_t event_id = 0;
    (void)AttrUtils::GetInt(op_desc, SEND_ATTR_EVENT_ID, event_id);
    task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_RECORD));
    task_def.set_event_id(static_cast<uint32_t>(event_id));
  } else {
    std::vector<uint8_t> args(16U, 0U);
    task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_KERNEL));
    auto kernel_info = task_def.mutable_kernel();
    kernel_info->set_args(args.data(), args.size());
    kernel_info->set_args_size(args.size());
    kernel_info->mutable_context()->set_kernel_type(static_cast<uint32_t>(ccKernelType::TE));
    kernel_info->set_kernel_name(node.GetName());
    kernel_info->set_block_dim(1U);
    uint16_t args_offset[2] = {0U, 0U};
    kernel_info->mutable_context()->set_args_offset(args_offset, sizeof(args_offset));
    kernel_info->mutable_context()->set_op_index(op_desc->GetId());
  }
  tasks.emplace_back(task_def);
  return SUCCESS;
}

class InputH2DOverlapCompileSt : public testing::Test {
 protected:
  void SetUp() override {
    ge_env_.InstallDefault();
    MockForGenerateTask("AiCoreLib", GenerateTaskForInputH2DOverlapSt);
    MockForGenerateTask("RTSLib", GenerateTaskForInputH2DOverlapSt);
  }

  void TearDown() override {
    TearDownForGenerateTask("AiCoreLib");
    TearDownForGenerateTask("RTSLib");
    ge_env_.Reset().InstallDefault();
  }

 private:
  GeRunningEnvFaker ge_env_;
};
}  // namespace

TEST_F(InputH2DOverlapCompileSt, BuildGraphWritesInputH2DOverlapPlan) {
  auto root_graph = BuildInputH2DOverlapCompileGraph();
  ASSERT_NE(root_graph, nullptr);
  (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);

  GeRootModelPtr root_model;
  ASSERT_EQ(BuildRootModelWithOption(root_graph, "0,1,2", root_model), SUCCESS);

  ASSERT_NE(root_model, nullptr);
  const auto ge_model = GetOnlyModel(root_model);
  ASSERT_NE(ge_model, nullptr);
  const auto model_task_def = ge_model->GetModelTaskDefPtr();
  ASSERT_NE(model_task_def, nullptr);

  NamedAttrs plan_attr;
  ASSERT_TRUE(AttrUtils::GetNamedAttrs(ge_model, input_h2d_overlap_test::kPlanAttrName, plan_attr));
  input_h2d_overlap::InputH2DOverlapFinalPlan plan;
  ASSERT_TRUE(input_h2d_overlap_test::DecodePlan(plan_attr, plan));
  int64_t stream_num = 0;
  int64_t event_num = 0;
  ASSERT_TRUE(AttrUtils::GetInt(ge_model, ATTR_MODEL_STREAM_NUM, stream_num));
  ASSERT_TRUE(AttrUtils::GetInt(ge_model, ATTR_MODEL_EVENT_NUM, event_num));

  EXPECT_EQ(plan.version, 1U);
  EXPECT_LT(plan.copy_stream_id, static_cast<uint32_t>(stream_num));
  ASSERT_EQ(plan.groups.size(), 2U);

  std::set<uint32_t> planned_inputs;
  std::set<uint32_t> planned_events;
  uint32_t wait_point_count = 0U;
  for (const auto &group : plan.groups) {
    ASSERT_FALSE(group.inputs.empty());
    ASSERT_FALSE(group.wait_points.empty());
    for (const auto &input : group.inputs) {
      planned_inputs.emplace(input.input_index);
      EXPECT_GT(input.size, 0U);
    }
    for (const auto &wait_point : group.wait_points) {
      ++wait_point_count;
      EXPECT_LT(wait_point.stream_id, static_cast<uint32_t>(stream_num));
      EXPECT_LT(wait_point.event_id, static_cast<uint32_t>(event_num));
      ASSERT_LT(wait_point.wait_task_id, static_cast<uint32_t>(model_task_def->task_size()));
      const auto &wait_task = model_task_def->task(wait_point.wait_task_id);
      EXPECT_EQ(wait_task.type(), static_cast<uint32_t>(ModelTaskType::MODEL_TASK_EVENT_WAIT));
      EXPECT_EQ(wait_task.stream_id(), wait_point.stream_id);
      EXPECT_EQ(wait_task.event_id(), wait_point.event_id);
      EXPECT_NE(wait_task.stream_id(), plan.copy_stream_id);
      planned_events.emplace(wait_point.event_id);
    }
  }
  EXPECT_EQ(planned_inputs, (std::set<uint32_t>{0U, 1U}));
  EXPECT_EQ(wait_point_count, 2U);
  EXPECT_EQ(planned_events.size(), 2U);
}

TEST_F(InputH2DOverlapCompileSt, IsInputH2DOverlapEnabledReflectsGraphOption) {
  bool enabled = true;
  {
    ScopedGraphOptions option_guard("", false);
    ASSERT_EQ(IsInputH2DOverlapEnabled(enabled), SUCCESS);
    EXPECT_FALSE(enabled);
  }
  {
    ScopedGraphOptions option_guard("1");
    ASSERT_EQ(IsInputH2DOverlapEnabled(enabled), SUCCESS);
    EXPECT_TRUE(enabled);
  }
  {
    ScopedGraphOptions option_guard("0");
    ASSERT_EQ(IsInputH2DOverlapEnabled(enabled), SUCCESS);
    EXPECT_FALSE(enabled);
  }
}

TEST_F(InputH2DOverlapCompileSt, DisabledOptionDoesNotWriteInputH2DOverlapPlan) {
  auto root_graph = BuildInputH2DOverlapCompileGraph();
  ASSERT_NE(root_graph, nullptr);
  (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);

  GeRootModelPtr root_model;
  ASSERT_EQ(BuildRootModelWithOption(root_graph, "0", root_model), SUCCESS);
  const auto ge_model = GetOnlyModel(root_model);
  ASSERT_NE(ge_model, nullptr);
  NamedAttrs plan_attr;
  EXPECT_FALSE(AttrUtils::GetNamedAttrs(ge_model, input_h2d_overlap_test::kPlanAttrName, plan_attr));
}

TEST_F(InputH2DOverlapCompileSt, DefaultDpWritesInputH2DOverlapPlan) {
  auto root_graph = BuildInputH2DOverlapDefaultDpGraph();
  ASSERT_NE(root_graph, nullptr);
  (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);

  GeRootModelPtr root_model;
  ASSERT_EQ(BuildRootModelWithOption(root_graph, "1", root_model), SUCCESS);
  input_h2d_overlap::InputH2DOverlapFinalPlan plan;
  ASSERT_TRUE(DecodeInputH2DOverlapPlanFromModel(GetOnlyModel(root_model), plan));
  ASSERT_FALSE(plan.groups.empty());

  std::set<uint32_t> planned_inputs;
  for (const auto &group : plan.groups) {
    ASSERT_FALSE(group.inputs.empty());
    ASSERT_FALSE(group.wait_points.empty());
    for (const auto &input : group.inputs) {
      planned_inputs.emplace(input.input_index);
    }
  }
  EXPECT_FALSE(planned_inputs.empty());
}

TEST_F(InputH2DOverlapCompileSt, ManualBoundaryWithLeadingCommaPlansTailGroups) {
  auto root_graph = BuildInputH2DOverlapCompileGraph();
  ASSERT_NE(root_graph, nullptr);
  (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);

  GeRootModelPtr root_model;
  ASSERT_EQ(BuildRootModelWithOption(root_graph, ",1", root_model), SUCCESS);
  input_h2d_overlap::InputH2DOverlapFinalPlan plan;
  ASSERT_TRUE(DecodeInputH2DOverlapPlanFromModel(GetOnlyModel(root_model), plan));
  ASSERT_EQ(plan.groups.size(), 1U);
  ASSERT_EQ(plan.groups[0].inputs.size(), 1U);
  EXPECT_EQ(plan.groups[0].inputs[0].input_index, 1U);
}

TEST_F(InputH2DOverlapCompileSt, ManualBoundaryWithTrailingCommaPlansTailGroups) {
  auto root_graph = BuildInputH2DOverlapCompileGraph();
  ASSERT_NE(root_graph, nullptr);
  (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);

  GeRootModelPtr root_model;
  ASSERT_EQ(BuildRootModelWithOption(root_graph, "1,", root_model), SUCCESS);
  input_h2d_overlap::InputH2DOverlapFinalPlan plan;
  ASSERT_TRUE(DecodeInputH2DOverlapPlanFromModel(GetOnlyModel(root_model), plan));
  ASSERT_EQ(plan.groups.size(), 1U);
  ASSERT_EQ(plan.groups[0].inputs.size(), 1U);
  EXPECT_EQ(plan.groups[0].inputs[0].input_index, 1U);
}

TEST_F(InputH2DOverlapCompileSt, InvalidManualBoundaryConfigFails) {
  const std::vector<std::string> invalid_options = {"2",     "-1", std::string("\xEF\xBC\x8C") + "44",
                                                    "5,4,3", "44", "1,,2"};
  for (const auto &option : invalid_options) {
    auto root_graph = BuildInputH2DOverlapCompileGraph();
    ASSERT_NE(root_graph, nullptr);
    (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);
    GeRootModelPtr root_model;
    EXPECT_NE(BuildRootModelWithOption(root_graph, option, root_model), SUCCESS) << option;
  }
}

TEST_F(InputH2DOverlapCompileSt, NoCandidateKeepsOriginalInputCopyPath) {
  auto root_graph = BuildInputH2DOverlapNoCandidateGraph();
  ASSERT_NE(root_graph, nullptr);
  (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);

  GeRootModelPtr root_model;
  ASSERT_EQ(BuildRootModelWithOption(root_graph, "1", root_model), SUCCESS);
  const auto ge_model = GetOnlyModel(root_model);
  ASSERT_NE(ge_model, nullptr);
  NamedAttrs plan_attr;
  EXPECT_FALSE(AttrUtils::GetNamedAttrs(ge_model, input_h2d_overlap_test::kPlanAttrName, plan_attr));
}

TEST_F(InputH2DOverlapCompileSt, DynamicShapeGraphFailsWhenInputH2DOverlapEnabled) {
  auto root_graph = BuildInputH2DOverlapCompileGraph();
  ASSERT_NE(root_graph, nullptr);
  (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);
  (void)AttrUtils::SetBool(root_graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  root_graph->SetGraphUnknownFlag(true);

  GeRootModelPtr root_model;
  EXPECT_NE(BuildRootModelWithOption(root_graph, "1", root_model), SUCCESS);
}

TEST_F(InputH2DOverlapCompileSt, MultiStreamGraphKeepsOriginalInputCopyPath) {
  auto root_graph = BuildInputH2DOverlapMultiStreamGraph();
  ASSERT_NE(root_graph, nullptr);
  (void)AttrUtils::SetStr(root_graph, ATTR_NAME_SESSION_GRAPH_ID, kSessionId);

  GeRootModelPtr root_model;
  ASSERT_EQ(BuildRootModelWithOption(root_graph, "1", root_model), SUCCESS);
  const auto ge_model = GetOnlyModel(root_model);
  ASSERT_NE(ge_model, nullptr);
  int64_t stream_num = 0;
  ASSERT_TRUE(AttrUtils::GetInt(ge_model, ATTR_MODEL_STREAM_NUM, stream_num));
  EXPECT_GT(stream_num, 1);
  NamedAttrs plan_attr;
  EXPECT_FALSE(AttrUtils::GetNamedAttrs(ge_model, input_h2d_overlap_test::kPlanAttrName, plan_attr));
}
}  // namespace ge
