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
#include "ge_graph_dsl/graph_dsl.h"
#include "es_ge_test_ops.h"
#include "graph/utils/graph_utils_ex.h"
#include "framework/common/framework_types_internal.h"
#include "jit_execution/exe_points/execution_order.h"
#include "ge_common/debug/ge_log.h"
#include "faker/space_registry_faker.h"
#include "graph/ge_local_context.h"
#include <vector>
using namespace std;
using namespace testing;
using namespace ge;

class ExecutionOrderUT : public testing::Test {
 protected:
  void SetUp() override {
    gert::SpaceRegistryFaker::CreateDefaultSpaceRegistry();
    std::map<std::string, std::string> options = {{ge::SOC_VERSION, "Ascend310"}};
    GetThreadLocalContext().SetGlobalOption(options);
    es_graph_ = std::unique_ptr<es::EsGraphBuilder>(new es::EsGraphBuilder("Hi Lowering graph"));
  }
  void TearDown() override {}
  std::unique_ptr<es::EsGraphBuilder> es_graph_;
};

/**
 *      data
 *        |
 *       relu
 *        |
 *       relu1
 *        |
 *    netoutput
 *    该图没有切分机会，进入EO以后，不发生切分，first ep就是last ep。
 */
TEST_F(ExecutionOrderUT, no_slice_tests) {
  [this]() {
    auto data = es_graph_->CreateInput(0, "data", DATA);
    data.SetShape({-1, -1, -1, -1});
    auto relu = es::Relu(data);
    auto relu1 = es::Relu(relu);
    es::EsGraphBuilder::SetOutput(relu1, 0);
  }();
  auto graph = es_graph_->BuildAndReset();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());

  compute_graph->SetOutputSize(2);  // make a wrong output size

  UserGraph user_graph{0, compute_graph};
  ExecutionOrder eo(user_graph);

  std::vector<GeTensor> input_tensors(1);
  GeTensorDesc td;
  td.SetShape((GeShape({2, 3, 3, 2})));
  td.SetOriginShape((GeShape({2, 3, 3, 2})));
  td.SetFormat(FORMAT_ND);
  td.SetOriginFormat(FORMAT_ND);
  td.SetPlacement(Placement::kPlacementDevice);
  td.SetDataType(DT_FLOAT16);

  input_tensors[0] = GeTensor(td);
  ExecutionPoint *first_ep = nullptr;
  ASSERT_EQ(eo.FirstPoint(input_tensors, first_ep), SUCCESS);
  ASSERT_NE(first_ep, nullptr);
  EXPECT_TRUE(first_ep->IsLast());
  EXPECT_EQ(first_ep->GetEpOutNum(), 1);

  ExecutionPoint *next_ep = nullptr;
  EXPECT_EQ(eo.NextPoint(*first_ep, input_tensors, next_ep), SUCCESS);
  EXPECT_EQ(next_ep, nullptr);
}

TEST_F(ExecutionOrderUT, test_has_next_ep) {
  // prepare graph
  [this]() {
    auto data = es_graph_->CreateInput(0, "data", DATA);
    data.SetShape({-1, -1, -1, -1});
    auto relu = es::Relu(data);
    auto relu1 = es::Relu(relu);
    es::EsGraphBuilder::SetOutput(relu1, 0);
  }();
  auto graph = es_graph_->BuildAndReset();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());
  compute_graph->SetOutputSize(1);

  // fake eo with 3 ep
  UserGraph user_graph{0, compute_graph};
  ExecutionOrder eo(user_graph);
  std::map<std::string, std::string> graph_options;
  auto ep0 = MakeUnique<ExecutionPoint>(0, compute_graph, compute_graph, graph_options);
  auto ep1 = MakeUnique<ExecutionPoint>(1, compute_graph, compute_graph, graph_options);
  auto ep2 = MakeUnique<ExecutionPoint>(2, compute_graph, nullptr, graph_options);

  eo.slice_graphs_.emplace_back(std::move(ep0));
  eo.slice_graphs_.emplace_back(std::move(ep1));
  eo.slice_graphs_.emplace_back(std::move(ep2));

  // fake input tensor
  std::vector<GeTensor> input_tensors(1);
  GeTensorDesc td;
  td.SetShape((GeShape({-1, -1, -1, -1})));
  td.SetOriginShape((GeShape({-1, -1, -1, -1})));
  td.SetFormat(FORMAT_ND);
  td.SetOriginFormat(FORMAT_ND);
  td.SetPlacement(Placement::kPlacementDevice);
  td.SetDataType(DT_FLOAT16);
  input_tensors[0] = GeTensor(td);

  // check get first,next,last
  ExecutionPoint *first_ep = nullptr;
  EXPECT_EQ(eo.FirstPoint(input_tensors, first_ep), SUCCESS);
  EXPECT_NE(first_ep, nullptr);
  EXPECT_FALSE(first_ep->IsLast());

  ExecutionPoint *next_ep = nullptr;
  EXPECT_EQ(eo.NextPoint(*first_ep, input_tensors, next_ep), SUCCESS);
  EXPECT_NE(next_ep, nullptr);
  EXPECT_FALSE(next_ep->IsLast());

  ExecutionPoint *last_ep = nullptr;
  EXPECT_EQ(eo.NextPoint(*next_ep, input_tensors, last_ep), SUCCESS);
  EXPECT_NE(last_ep, nullptr);
  EXPECT_TRUE(last_ep->IsLast());

  ExecutionPoint *try_last_ep = nullptr;
  EXPECT_EQ(eo.NextPoint(*last_ep, input_tensors, try_last_ep), SUCCESS);
  EXPECT_EQ(try_last_ep, nullptr);
}

TEST_F(ExecutionOrderUT, seperate_graph_options_and_select_ep_option) {
  [this]() {
    auto data = es_graph_->CreateInput(0, "data", DATA);
    data.SetShape({-1, -1, -1, -1});
    auto relu = es::Relu(data);
    es::EsGraphBuilder::SetOutput(relu, 0);
  }();
  auto graph = es_graph_->BuildAndReset();
  auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph.get());

  std::map<std::string, std::string> graph_options;
  graph_options["ge.inputShape"] = "1,2,3,4";
  graph_options["ge.outputDatatype"] = "float16";
  graph_options["another.middle"] = "another_value";
  UserGraph user_graph{0, compute_graph, graph_options};
  ExecutionOrder eo(user_graph);

  ASSERT_EQ(eo.first_ep_options_, graph_options);
  ASSERT_EQ(eo.middle_ep_options_.size(), 1U);
  EXPECT_NE(eo.middle_ep_options_.find("another.middle"), eo.middle_ep_options_.end());
  ASSERT_EQ(eo.last_ep_options_.size(), 2U);
  EXPECT_NE(eo.last_ep_options_.find("ge.outputDatatype"), eo.last_ep_options_.end());
  EXPECT_NE(eo.last_ep_options_.find("another.middle"), eo.last_ep_options_.end());

  EXPECT_EQ(eo.SelectEpOption({}), eo.first_ep_options_);

  auto ep = MakeUnique<ExecutionPoint>(0, compute_graph, compute_graph, graph_options);
  eo.slice_graphs_.emplace_back(std::move(ep));
  EXPECT_EQ(eo.SelectEpOption(PartionResult{nullptr, compute_graph, {}}), eo.middle_ep_options_);
  EXPECT_EQ(eo.SelectEpOption(PartionResult{nullptr, nullptr, {}}), eo.last_ep_options_);
}
