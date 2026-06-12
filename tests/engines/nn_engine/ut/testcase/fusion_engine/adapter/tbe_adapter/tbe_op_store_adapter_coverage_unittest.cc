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
#include "fe_llt_utils.h"
#include "all_ops_stub.h"
#define protected public
#define private public
#include "adapter/tbe_adapter/tbe_op_store_adapter.h"
#include "adapter/adapter_itf/task_builder_adapter.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/graph_utils.h"
#include "common/configuration.h"
#include "common/fe_log.h"
#include "ops_kernel_store/fe_ops_kernel_info_store.h"
#include "common/platform_utils.h"
#include "common/util/op_info_util.h"
#include "common/fe_report_error.h"
#include "common/fe_error_code.h"
#undef protected
#undef private

using namespace std;
using namespace ge;
using namespace fe;
using namespace te;

class UTESTTbeOpStoreAdapterCoverage : public testing::Test {
 protected:
  void SetUp() {
    Configuration::Instance(AI_CORE_NAME).InitLibPath();
  }
  void TearDown() {}
};

TEST_F(UTESTTbeOpStoreAdapterCoverage, SerialPreCompileOp_PreBuildFailed) {
  TbeOpStoreAdapterPtr tbe_adapter_ptr = std::make_shared<TbeOpStoreAdapter>(AI_CORE_NAME);
  tbe_adapter_ptr->Initialize(std::map<std::string, std::string>());
  
  ComputeGraphPtr graph = std::make_shared<ComputeGraph>("test_graph");
  OpDescPtr op_desc = std::make_shared<OpDesc>("test_op", "Add");
  GeTensorDesc tensor_desc(GeShape({1, 2}), FORMAT_NCHW, DT_FLOAT);
  op_desc->AddInputDesc(tensor_desc);
  op_desc->AddOutputDesc(tensor_desc);
  AttrUtils::SetInt(op_desc, "_fe_imply_type", 6);
  NodePtr node = graph->AddNode(op_desc);
  
  vector<PreCompileNodePara> compile_para_vec;
  PreCompileNodePara comp_para;
  comp_para.node = node.get();
  compile_para_vec.push_back(comp_para);
  
  tbe_adapter_ptr->PreBuildTbeOp = [](te::TbeOpInfo &, uint64_t, uint64_t) { return false; };
  
  Status status = tbe_adapter_ptr->SerialPreCompileOp(compile_para_vec);
  EXPECT_EQ(status, FAILED);
}

TEST_F(UTESTTbeOpStoreAdapterCoverage, ParallelPreCompileOp_PreBuildFailed) {
  TbeOpStoreAdapterPtr tbe_adapter_ptr = std::make_shared<TbeOpStoreAdapter>(AI_CORE_NAME);
  tbe_adapter_ptr->Initialize(std::map<std::string, std::string>());
  
  ComputeGraphPtr graph = std::make_shared<ComputeGraph>("test_graph");
  OpDescPtr op_desc = std::make_shared<OpDesc>("test_op", "Add");
  GeTensorDesc tensor_desc(GeShape({1, 2}), FORMAT_NCHW, DT_FLOAT);
  op_desc->AddInputDesc(tensor_desc);
  op_desc->AddOutputDesc(tensor_desc);
  AttrUtils::SetInt(op_desc, "_fe_imply_type", 6);
  NodePtr node = graph->AddNode(op_desc);
  
  vector<PreCompileNodePara> compile_para_vec;
  PreCompileNodePara comp_para;
  comp_para.node = node.get();
  compile_para_vec.push_back(comp_para);
  
  tbe_adapter_ptr->PreBuildTbeOp = [](te::TbeOpInfo &, uint64_t, uint64_t) { return false; };
  
  Status status = tbe_adapter_ptr->ParallelPreCompileOp(compile_para_vec);
  EXPECT_EQ(status, FAILED);
}

TEST_F(UTESTTbeOpStoreAdapterCoverage, TaskFusion_TaskFusionFuncFailed) {
  TbeOpStoreAdapterPtr tbe_adapter_ptr = std::make_shared<TbeOpStoreAdapter>(AI_CORE_NAME);
  tbe_adapter_ptr->Initialize(std::map<std::string, std::string>());
  
  ComputeGraphPtr graph = std::make_shared<ComputeGraph>("test_graph");
  OpDescPtr op_desc = std::make_shared<OpDesc>("fusion_op", "Add");
  GeTensorDesc tensor_desc(GeShape({1, 2}), FORMAT_NCHW, DT_FLOAT);
  op_desc->AddInputDesc(tensor_desc);
  op_desc->AddOutputDesc(tensor_desc);
  AttrUtils::SetInt(op_desc, "_fe_imply_type", 6);
  NodePtr node = graph->AddNode(op_desc);
  
  vector<ge::Node*> nodes = {node.get()};
  ScopeNodeIdMap fusion_nodes_map;
  fusion_nodes_map[0] = nodes;
  
  CompileResultMap compile_ret_map;
  
  tbe_adapter_ptr->TaskFusionFunc = [](const vector<ge::Node*> &, ge::OpDescPtr, uint64_t, uint64_t) {
    return te::OP_BUILD_FAIL;
  };
  
  Status status = tbe_adapter_ptr->TaskFusion(fusion_nodes_map, compile_ret_map);
  EXPECT_NE(status, SUCCESS);
}

TEST_F(UTESTTbeOpStoreAdapterCoverage, SetTaskToTeFusion_TeFusionFailed) {
  TbeOpStoreAdapterPtr tbe_adapter_ptr = std::make_shared<TbeOpStoreAdapter>(AI_CORE_NAME);
  tbe_adapter_ptr->Initialize(std::map<std::string, std::string>());
  
  ComputeGraphPtr graph = std::make_shared<ComputeGraph>("test_graph");
  OpDescPtr op_desc = std::make_shared<OpDesc>("fusion_op", "Add");
  GeTensorDesc tensor_desc(GeShape({1, 2}), FORMAT_NCHW, DT_FLOAT);
  op_desc->AddInputDesc(tensor_desc);
  op_desc->AddOutputDesc(tensor_desc);
  AttrUtils::SetInt(op_desc, "_fe_imply_type", 6);
  NodePtr node = graph->AddNode(op_desc);
  
  vector<ge::Node*> nodes = {node.get()};
  
  TbeOpStoreAdapter::CompileTaskPara task_para(0);
  
  tbe_adapter_ptr->TeFusion = [](vector<ge::Node*> &, ge::OpDescPtr, vector<ge::NodePtr> &, uint64_t, uint64_t, const string &) {
    return te::OP_BUILD_FAIL;
  };
  
  Status status = tbe_adapter_ptr->SetTaskToTeFusion(task_para, {}, nodes, "", CompileStrategy::COMPILE_STRATEGY_NORMAL);
  EXPECT_NE(status, SUCCESS);
}