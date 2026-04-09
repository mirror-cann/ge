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

#include "macro_utils/dt_public_scope.h"
#include "common/dump/dump_op.h"
#include "common/debug/log.h"
#include "framework/common/types.h"
#include "common/ge_inner_error_codes.h"
#include "common/dump/dump_properties.h"
#include "graph/utils/attr_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "macro_utils/dt_public_unscope.h"

namespace ge {
class UTEST_dump_op : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

TEST_F(UTEST_dump_op, launch_dump_op_success) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  OpDescPtr op_desc = std::make_shared<OpDesc>("GatherV2", "GatherV2");
  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, {}, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);
}

TEST_F(UTEST_dump_op, launch_dump_op_success_2) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  OpDescPtr op_desc = std::make_shared<OpDesc>("GatherV2", "GatherV2");
  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_op.SetDynamicModelInfo("modle2", "model2", 1);
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, {}, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);
}

TEST_F(UTEST_dump_op, launch_dump_op_success_3) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  OpDescPtr op_desc = std::make_shared<OpDesc>("GatherV2", "GatherV2");
  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_debug_ = "1";
  dump_properties.is_train_op_debug_ = true;
  dump_op.SetDynamicModelInfo("modle2", "model2", 1);
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, {}, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);
}

TEST_F(UTEST_dump_op, launch_dump_op_workspace) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  OpDescPtr op_desc = std::make_shared<OpDesc>("GatherV2", "GatherV2");

  std::vector<int64_t> tvm_workspace_memory_type = {ge::AicpuWorkSpaceType::CUST_LOG};
  ge::AttrUtils::SetListInt(op_desc, ATTR_NAME_AICPU_WORKSPACE_TYPE, tvm_workspace_memory_type);
  op_desc->SetWorkspaceBytes(vector<int64_t>{32});
  const std::vector<std::pair<uintptr_t, int64_t>> space_addr = {{23333, 32}};
  dump_op.SetWorkspaceAddrs(space_addr);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "all";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, {}, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);
}

TEST_F(UTEST_dump_op, LaunchDumpOp_Success_WithEmptyAddr) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  GeTensorDesc tensor1(GeShape(), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor1, ATTR_DATA_DUMP_REF, "conv:input:1");
  GeTensorDesc tensor2(GeShape(), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor2, ATTR_DATA_DUMP_REF, "conv:input:2");
  op_desc->AddOptionalInputDesc("input_2", tensor2);
  op_desc->AddInputDesc(tensor1);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "input";
  dump_op.SetLoopAddr(2U, 2U, 2U);
  EXPECT_EQ(dump_op.global_step_, 2U);
  EXPECT_EQ(dump_op.loop_per_iter_, 2U);
  EXPECT_EQ(dump_op.loop_cond_, 2U);
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  int64_t *addr_in = (int64_t *)malloc(1024);
  int64_t *addr_out = (int64_t *)malloc(1024);
  input_addrs.emplace_back(reinterpret_cast<uintptr_t>(nullptr));
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in));
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);
  free(addr_in);
  free(addr_out);
}

TEST_F(UTEST_dump_op, launch_dump_op_input) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  op_desc->SetWorkspace({});
  op_desc->SetWorkspaceBytes({});
  op_desc->SetInputOffset({100, 200});
  op_desc->SetOutputOffset({100, 200});

  GeTensorDesc tensor(GeShape(), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor, ATTR_DATA_DUMP_REF, "conv:input:1");
  ge::AttrUtils::SetBool(tensor, ATTR_NAME_TENSOR_NO_TILING_MEM_TYPE, true);
  op_desc->AddInputDesc(tensor);
  op_desc->AddInputDesc(tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "input";
  EXPECT_EQ(dump_op.global_step_, 0U);
  EXPECT_EQ(dump_op.loop_per_iter_, 0U);
  EXPECT_EQ(dump_op.loop_cond_, 0U);
  dump_op.SetLoopAddr(2U, 2U, 2U);
  EXPECT_EQ(dump_op.global_step_, 2U);
  EXPECT_EQ(dump_op.loop_per_iter_, 2U);
  EXPECT_EQ(dump_op.loop_cond_, 2U);
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  int64_t *addr_in = (int64_t *)malloc(1024);
  int64_t *addr_out = (int64_t *)malloc(1024);
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in));
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in));
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);

  toolkit::aicpu::dump::Task task;
  EXPECT_EQ(dump_op.DumpInput(task, op_desc, input_addrs), ge::SUCCESS);

  free(addr_in);
  free(addr_out);
}

TEST_F(UTEST_dump_op, launch_dump_op_with_invalid_input) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  op_desc->SetWorkspace({});
  op_desc->SetWorkspaceBytes({});
  op_desc->SetInputOffset({100, 200});
  op_desc->SetOutputOffset({100, 200});

  GeTensorDesc tensor0(GeShape(), FORMAT_RESERVED, DT_UNDEFINED);
  ge::AttrUtils::SetStr(tensor0, ATTR_DATA_DUMP_REF, "conv:input:1");
  ge::AttrUtils::SetBool(tensor0, ATTR_NAME_TENSOR_NO_TILING_MEM_TYPE, true);
  op_desc->AddInputDesc(tensor0);
  op_desc->AddInputDesc(tensor0);

  GeTensorDesc tensor1(GeShape(), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor1, ATTR_DATA_DUMP_REF, "conv:input:2");
  ge::AttrUtils::SetBool(tensor1, ATTR_NAME_TENSOR_NO_TILING_MEM_TYPE, true);
  op_desc->AddInputDesc(tensor1);
  op_desc->AddInputDesc(tensor1);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "input";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  int64_t *addr_in0 = nullptr;
  int64_t *addr_in1 = (int64_t *)malloc(1024);
  int64_t *addr_out = (int64_t *)malloc(1024);
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in0));
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in1));
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in0));
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in1));
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);

  toolkit::aicpu::dump::Task task;
  EXPECT_EQ(dump_op.DumpInput(task, op_desc, input_addrs), ge::SUCCESS);

  free(addr_in1);
  free(addr_out);
}

TEST_F(UTEST_dump_op, launch_dump_op_output) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  op_desc->SetWorkspace({});
  op_desc->SetWorkspaceBytes({});
  op_desc->SetInputOffset({100, 200});
  op_desc->SetOutputOffset({100, 200});

  GeTensorDesc tensor(GeShape(), FORMAT_NCHW, DT_FLOAT);
  op_desc->AddOutputDesc(tensor);
  op_desc->AddOutputDesc(tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "output";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> output_addrs;
  int64_t *addr_out = (int64_t *)malloc(1024);
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, output_addrs, nullptr);

  toolkit::aicpu::dump::Task task;
  auto ret = dump_op.DumpOutput(task, op_desc, output_addrs);
  EXPECT_EQ(ret, ge::SUCCESS);
  
  free(addr_out);
}

TEST_F(UTEST_dump_op, launch_dump_op_all) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  OpDescPtr op_desc = std::make_shared<OpDesc>("GatherV2", "GatherV2");

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "all";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, {}, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);
}

TEST_F(UTEST_dump_op, generate_ffts_dump_ok) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  OpDescPtr op_desc = std::make_shared<OpDesc>("GatherV2", "GatherV2");
  std::string str_slice_info =
        "{\"dependencies\":[],\"thread_scopeId\":0,\"is_first_node_in_topo_order\":false,\"node_num_in_thread_scope\":0,\"is_input_node_of_thread_scope\":false,\"is_output_node_of_thread_scope\":false,\"threadMode\":0,\"slice_instance_num\":0,\"parallel_window_size\":0,\"thrad_id\":0,\"oriInputTensorShape\":[],\"oriOutputTensorShape\":[],\"original_node\":\"\",\"core_num\":[],\"cutType\":[],\"atomic_types\":[],\"thread_id\":0,\"same_atomic_clean_nodes\":[],\"input_axis\":[],\"output_axis\":[],\"input_tensor_indexes\":[],\"output_tensor_indexes\":[],\"input_tensor_slice\":[],\"output_tensor_slice\":[],\"ori_input_tensor_slice\":[],\"ori_output_tensor_slice\":[]}";
  (void)ge::AttrUtils::SetStr(op_desc, "_sgt_json_info", str_slice_info);
  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "all";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, {}, nullptr);
  ge::Context context;
  context.context_id = 0;
  context.thread_id = 0;
  ge::RealAddressAndSize real_address_and_size;
  real_address_and_size.address = 1U;
  real_address_and_size.size = 1U;
  context.input.emplace_back(real_address_and_size);
  context.output.emplace_back(real_address_and_size);
  std::vector<ge::Context> dump_context;
  dump_context.emplace_back(context);
  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  int64_t *addr_in = (int64_t *)malloc(1024);
  int64_t *addr_out = (int64_t *)malloc(1024);
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in));
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SaveFftsSubOpInfo(op_desc, dump_context);
  void *load_dump_info = nullptr;
  uint32_t load_dump_len = 0U;
  void *unload_dump_info = nullptr;
  uint32_t unload_dump_len = 0U;
  EXPECT_EQ(dump_op.GenerateFftsDump(dump_properties, load_dump_info, load_dump_len,
                                     unload_dump_info, unload_dump_len, true), ge::SUCCESS);

  dump_properties.dump_mode_ = "input";
  dump_op.SaveFftsSubOpInfo(op_desc, dump_context);
  EXPECT_EQ(dump_op.GenerateFftsDump(dump_properties, load_dump_info, load_dump_len,
                                     unload_dump_info, unload_dump_len, true), ge::SUCCESS);

  (void)ge::AttrUtils::SetStr(op_desc, "_sgt_json_info", "");
  dump_properties.dump_mode_ = "output";
  dump_op.SaveFftsSubOpInfo(op_desc, dump_context);
  EXPECT_EQ(dump_op.GenerateFftsDump(dump_properties, load_dump_info, load_dump_len,
                                     unload_dump_info, unload_dump_len, true), ge::SUCCESS);

  dump_properties.dump_mode_ = "bad_mode";
  dump_op.SaveFftsSubOpInfo(op_desc, dump_context);
  EXPECT_EQ(dump_op.GenerateFftsDump(dump_properties, load_dump_info, load_dump_len,
                                     unload_dump_info, unload_dump_len, true), ge::SUCCESS);
  free(addr_in);
  free(addr_out);
}

TEST_F(UTEST_dump_op, get_addr_type) {
  DumpOp dump_op;
  toolkit::aicpu::dump::Task task;
  GeTensorDesc desc;
  EXPECT_EQ(dump_op.GetAddrType(task, desc), toolkit::aicpu::dump::AddressType::TRADITIONAL_ADDR);

  ge::AttrUtils::SetBool(desc, ATTR_NAME_TENSOR_NO_TILING_MEM_TYPE, true);
  EXPECT_EQ(dump_op.GetAddrType(task, desc), toolkit::aicpu::dump::AddressType::NOTILING_ADDR);

  task.add_context();
  EXPECT_EQ(dump_op.GetAddrType(task, desc), toolkit::aicpu::dump::AddressType::RAW_ADDR);
}

TEST_F(UTEST_dump_op, dump_modelname_opblacklist_output_ok) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  op_desc->SetWorkspace({});
  op_desc->SetWorkspaceBytes({});
  op_desc->SetInputOffset({100, 200});
  op_desc->SetOutputOffset({100, 200});

  GeTensorDesc tensor(GeShape(), FORMAT_NCHW, DT_FLOAT);
  op_desc->AddOutputDesc(tensor);
  op_desc->AddOutputDesc(tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "output";
  std::map<std::string, ModelOpBlacklist> new_blacklist_map;
  ModelOpBlacklist model1_bl;
  model1_bl.dump_opname_blacklist["conv"].output_indices = {0};
  model1_bl.dump_optype_blacklist["conv"].output_indices = {1};
  new_blacklist_map["model1"] = model1_bl;
  dump_properties.SetModelDumpBlacklistMap(new_blacklist_map);
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> output_addrs;
  int64_t *addr_out = (int64_t *)malloc(1024);
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, output_addrs, nullptr);

  toolkit::aicpu::dump::Task task;
  auto ret = dump_op.DumpOutput(task, op_desc, output_addrs);
  EXPECT_EQ(ret, ge::SUCCESS);
  
  free(addr_out);
}

TEST_F(UTEST_dump_op, dump_omname_opblacklist_output_ok) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  op_desc->SetWorkspace({});
  op_desc->SetWorkspaceBytes({});
  op_desc->SetInputOffset({100, 200});
  op_desc->SetOutputOffset({100, 200});

  GeTensorDesc tensor(GeShape(), FORMAT_NCHW, DT_FLOAT);
  op_desc->AddOutputDesc(tensor);
  op_desc->AddOutputDesc(tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model2", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "output";
  std::map<std::string, ModelOpBlacklist> new_blacklist_map;
  ModelOpBlacklist model1_bl;
  model1_bl.dump_opname_blacklist["conv"].output_indices = {0};
  model1_bl.dump_optype_blacklist["conv"].output_indices = {1};
  new_blacklist_map["model2"] = model1_bl;
  dump_properties.SetModelDumpBlacklistMap(new_blacklist_map);
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> output_addrs;
  int64_t *addr_out = (int64_t *)malloc(1024);
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SetDumpInfo(dump_properties, op_desc, {}, output_addrs, nullptr);

  toolkit::aicpu::dump::Task task;
  auto ret = dump_op.DumpOutput(task, op_desc, output_addrs);
  EXPECT_EQ(ret, ge::SUCCESS);
  
  free(addr_out);
}

TEST_F(UTEST_dump_op, dump_modelname_opblacklist_input_ok) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  op_desc->SetWorkspace({});
  op_desc->SetWorkspaceBytes({});
  op_desc->SetInputOffset({100, 200});
  op_desc->SetOutputOffset({100, 200});

  GeTensorDesc tensor(GeShape(), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor, ATTR_DATA_DUMP_REF, "conv:input:1");
  ge::AttrUtils::SetBool(tensor, ATTR_NAME_TENSOR_NO_TILING_MEM_TYPE, true);
  op_desc->AddInputDesc(tensor);
  op_desc->AddInputDesc(tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "input";
  std::map<std::string, ModelOpBlacklist> new_blacklist_map;
  ModelOpBlacklist model1_bl;
  model1_bl.dump_optype_blacklist["conv"].input_indices = {0};
  model1_bl.dump_opname_blacklist["conv"].input_indices = {1};
  new_blacklist_map["model1"] = model1_bl;
  dump_properties.SetModelDumpBlacklistMap(new_blacklist_map);
  EXPECT_EQ(dump_op.global_step_, 0U);
  EXPECT_EQ(dump_op.loop_per_iter_, 0U);
  EXPECT_EQ(dump_op.loop_cond_, 0U);
  dump_op.SetLoopAddr(2U, 2U, 2U);
  EXPECT_EQ(dump_op.global_step_, 2U);
  EXPECT_EQ(dump_op.loop_per_iter_, 2U);
  EXPECT_EQ(dump_op.loop_cond_, 2U);
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  int64_t *addr_in = (int64_t *)malloc(1024);
  int64_t *addr_out = (int64_t *)malloc(1024);
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in));
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);

  toolkit::aicpu::dump::Task task;
  EXPECT_EQ(dump_op.DumpInput(task, op_desc, input_addrs), ge::SUCCESS);

  free(addr_in);
  free(addr_out);
}

TEST_F(UTEST_dump_op, dump_omname_opblacklist_input_ok) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  op_desc->SetWorkspace({});
  op_desc->SetWorkspaceBytes({});
  op_desc->SetInputOffset({100, 200});
  op_desc->SetOutputOffset({100, 200});

  GeTensorDesc tensor(GeShape(), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor, ATTR_DATA_DUMP_REF, "conv:input:1");
  ge::AttrUtils::SetBool(tensor, ATTR_NAME_TENSOR_NO_TILING_MEM_TYPE, true);
  op_desc->AddInputDesc(tensor);
  op_desc->AddInputDesc(tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model2", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "input";
  std::map<std::string, ModelOpBlacklist> new_blacklist_map;
  ModelOpBlacklist model1_bl;
  model1_bl.dump_optype_blacklist["conv"].input_indices = {0};
  model1_bl.dump_opname_blacklist["conv"].input_indices = {1};
  new_blacklist_map["model2"] = model1_bl;
  dump_properties.SetModelDumpBlacklistMap(new_blacklist_map);
  EXPECT_EQ(dump_op.global_step_, 0U);
  EXPECT_EQ(dump_op.loop_per_iter_, 0U);
  EXPECT_EQ(dump_op.loop_cond_, 0U);
  dump_op.SetLoopAddr(2U, 2U, 2U);
  EXPECT_EQ(dump_op.global_step_, 2U);
  EXPECT_EQ(dump_op.loop_per_iter_, 2U);
  EXPECT_EQ(dump_op.loop_cond_, 2U);
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  int64_t *addr_in = (int64_t *)malloc(1024);
  int64_t *addr_out = (int64_t *)malloc(1024);
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in));
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));
  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);

  toolkit::aicpu::dump::Task task;
  EXPECT_EQ(dump_op.DumpInput(task, op_desc, input_addrs), ge::SUCCESS);

  free(addr_in);
  free(addr_out);
}

TEST_F(UTEST_dump_op, IsInBlacklist_NoBlacklist) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  dump_op.SetDynamicModelInfo("model1", "model2", 1);
  dump_op.SetRootGraphName("root_graph");

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_op.SetDumpInfo(dump_properties, std::make_shared<OpDesc>("test_op", "TestType"), {}, {}, nullptr);

  // No blacklist configured, should not skip
  EXPECT_FALSE(dump_op.IsInBlacklist("test_op", "TestType", 0, true));
  EXPECT_FALSE(dump_op.IsInBlacklist("test_op", "TestType", 0, false));
}

TEST_F(UTEST_dump_op, IsInBlacklist_InputOpNameBlacklist_ModelName) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";

  std::map<std::string, ModelOpBlacklist> blacklist;
  ModelOpBlacklist bl;
  bl.dump_opname_blacklist["test_op"].input_indices = {0};
  blacklist["model1"] = bl;
  dump_properties.SetModelDumpBlacklistMap(blacklist);

  dump_op.SetDumpInfo(dump_properties, std::make_shared<OpDesc>("test_op", "TestType"), {}, {}, nullptr);

  // Index 0 is blacklisted for input
  EXPECT_TRUE(dump_op.IsInBlacklist("test_op", "TestType", 0, true));
  EXPECT_FALSE(dump_op.IsInBlacklist("test_op", "TestType", 1, true));
  EXPECT_FALSE(dump_op.IsInBlacklist("test_op", "TestType", 0, false));
}

TEST_F(UTEST_dump_op, IsInBlacklist_OutputOpTypeBlacklist_RootGraphName) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  dump_op.SetDynamicModelInfo("subgraph", "model2", 1);
  dump_op.SetRootGraphName("root_graph");

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";

  std::map<std::string, ModelOpBlacklist> blacklist;
  ModelOpBlacklist bl;
  bl.dump_optype_blacklist["TestType"].output_indices = {0};
  blacklist["root_graph"] = bl;
  dump_properties.SetModelDumpBlacklistMap(blacklist);

  dump_op.SetDumpInfo(dump_properties, std::make_shared<OpDesc>("test_op", "TestType"), {}, {}, nullptr);

  // Index 0 is blacklisted for output via root_graph name
  EXPECT_TRUE(dump_op.IsInBlacklist("test_op", "TestType", 0, false));
  EXPECT_FALSE(dump_op.IsInBlacklist("test_op", "TestType", 1, false));
  EXPECT_FALSE(dump_op.IsInBlacklist("test_op", "TestType", 0, true));
}

TEST_F(UTEST_dump_op, IsInBlacklist_OutputOpTypeBlacklist_OmName) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  dump_op.SetDynamicModelInfo("model1", "model_om", 1);
  dump_op.SetRootGraphName("root_graph");

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";

  std::map<std::string, ModelOpBlacklist> blacklist;
  ModelOpBlacklist bl;
  bl.dump_optype_blacklist["TestType"].output_indices = {2};
  blacklist["model_om"] = bl;
  dump_properties.SetModelDumpBlacklistMap(blacklist);

  dump_op.SetDumpInfo(dump_properties, std::make_shared<OpDesc>("test_op", "TestType"), {}, {}, nullptr);

  // Index 2 is blacklisted for output via om name
  EXPECT_TRUE(dump_op.IsInBlacklist("test_op", "TestType", 2, false));
  EXPECT_FALSE(dump_op.IsInBlacklist("test_op", "TestType", 0, false));
}

TEST_F(UTEST_dump_op, IsInBlacklist_GlobalBlacklist_DumpLayerOpModel) {
  DumpOp dump_op;
  DumpProperties dump_properties;
  dump_op.SetDynamicModelInfo("model1", "model2", 1);
  dump_op.SetRootGraphName("root_graph");

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";

  std::map<std::string, ModelOpBlacklist> blacklist;
  ModelOpBlacklist bl;
  bl.dump_opname_blacklist["test_op"].input_indices = {0};
  bl.dump_optype_blacklist["TestType"].output_indices = {0};
  blacklist[DUMP_LAYER_OP_MODEL] = bl;
  dump_properties.SetModelDumpBlacklistMap(blacklist);

  dump_op.SetDumpInfo(dump_properties, std::make_shared<OpDesc>("test_op", "TestType"), {}, {}, nullptr);

  // Global blacklist should be checked
  EXPECT_TRUE(dump_op.IsInBlacklist("test_op", "TestType", 0, true));
  EXPECT_TRUE(dump_op.IsInBlacklist("test_op", "TestType", 0, false));
}

TEST_F(UTEST_dump_op, SetRootGraphName_Ok) {
  DumpOp dump_op;
  dump_op.SetRootGraphName("test_root_graph");
}

TEST_F(UTEST_dump_op, UpdateAddrs_DynamicShape) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  GeTensorDesc tensor1(GeShape({-1, -1, 3, 3}), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor1, ATTR_DATA_DUMP_REF, "conv:input:1");
  op_desc->AddInputDesc(tensor1);
  op_desc->AddInputDesc(tensor1);

  GeTensorDesc tensor2(GeShape({-1, 16, 14, 14}), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor2, ATTR_DATA_DUMP_REF, "conv:output:0");
  op_desc->AddOutputDesc(tensor2);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model2", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "all";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  int64_t *addr_in = (int64_t *)malloc(1024);
  int64_t *addr_out = (int64_t *)malloc(1024);
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in));
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in) + 512);
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));

  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.UpdateAddrs(input_addrs, output_addrs);
  EXPECT_EQ(ret, ge::SUCCESS);

  free(addr_in);
  free(addr_out);
}

TEST_F(UTEST_dump_op, UpdateAddrs_EmptyAddrs) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("conv", "conv");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  GeTensorDesc tensor(GeShape({1, 3, 224, 224}), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(tensor, ATTR_DATA_DUMP_REF, "conv:input:1");
  op_desc->AddInputDesc(tensor);
  op_desc->AddOutputDesc(tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model2", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "all";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;

  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.UpdateAddrs(input_addrs, output_addrs);
  EXPECT_EQ(ret, ge::SUCCESS);
}

TEST_F(UTEST_dump_op, LaunchDumpOp_WithMultipleInputs) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("addn", "AddN");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  for (int i = 0; i < 5; ++i) {
    GeTensorDesc tensor(GeShape({1, 3, 224, 224}), FORMAT_NCHW, DT_FLOAT);
    ge::AttrUtils::SetStr(tensor, ATTR_DATA_DUMP_REF, "addn:input:" + std::to_string(i));
    op_desc->AddInputDesc(tensor);
  }

  GeTensorDesc output_tensor(GeShape({1, 3, 224, 224}), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(output_tensor, ATTR_DATA_DUMP_REF, "addn:output:0");
  op_desc->AddOutputDesc(output_tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "all";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  for (int i = 0; i < 5; ++i) {
    int64_t *addr = (int64_t *)malloc(1024);
    input_addrs.push_back(reinterpret_cast<uintptr_t>(addr));
  }
  int64_t *addr_out = (int64_t *)malloc(1024);
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));

  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);

  for (auto addr : input_addrs) {
    free(reinterpret_cast<void*>(addr));
  }
  free(addr_out);
}

TEST_F(UTEST_dump_op, LaunchDumpOp_WithDifferentDataTypes) {
  DumpOp dump_op;
  DumpProperties dump_properties;

  OpDescPtr op_desc = std::make_shared<ge::OpDesc>("cast", "Cast");
  op_desc->SetStreamId(0);
  op_desc->SetId(0);

  GeTensorDesc input_tensor(GeShape({1, 3, 224, 224}), FORMAT_NCHW, DT_FLOAT16);
  ge::AttrUtils::SetStr(input_tensor, ATTR_DATA_DUMP_REF, "cast:input:0");
  op_desc->AddInputDesc(input_tensor);

  GeTensorDesc output_tensor(GeShape({1, 3, 224, 224}), FORMAT_NCHW, DT_FLOAT);
  ge::AttrUtils::SetStr(output_tensor, ATTR_DATA_DUMP_REF, "cast:output:0");
  op_desc->AddOutputDesc(output_tensor);

  std::set<std::string> temp;
  dump_properties.model_dump_properties_map_.emplace("model1", temp);
  dump_properties.enable_dump_ = "1";
  dump_properties.dump_mode_ = "all";
  dump_op.SetDynamicModelInfo("model1", "model2", 1);

  std::vector<uintptr_t> input_addrs;
  std::vector<uintptr_t> output_addrs;
  int16_t *addr_in = (int16_t *)malloc(512);
  int64_t *addr_out = (int64_t *)malloc(1024);
  input_addrs.push_back(reinterpret_cast<uintptr_t>(addr_in));
  output_addrs.push_back(reinterpret_cast<uintptr_t>(addr_out));

  dump_op.SetDumpInfo(dump_properties, op_desc, input_addrs, output_addrs, nullptr);
  auto ret = dump_op.LaunchDumpOp(false);
  EXPECT_EQ(ret, ge::SUCCESS);

  free(addr_in);
  free(addr_out);
}

TEST_F(UTEST_dump_op, SetLoopAddr) {
  DumpOp dump_op;

  dump_op.SetLoopAddr(100, 10, 1);
  EXPECT_EQ(dump_op.global_step_, 100U);
  EXPECT_EQ(dump_op.loop_per_iter_, 10U);
  EXPECT_EQ(dump_op.loop_cond_, 1U);

  dump_op.SetLoopAddr(0, 0, 0);
  EXPECT_EQ(dump_op.global_step_, 0U);
  EXPECT_EQ(dump_op.loop_per_iter_, 0U);
  EXPECT_EQ(dump_op.loop_cond_, 0U);
}

TEST_F(UTEST_dump_op, SetTaskIdAndStreamId) {
  DumpOp dump_op;

  dump_op.SetTaskId(12345);
  dump_op.SetStreamId(67890);

  EXPECT_EQ(dump_op.task_id_, 12345U);
  EXPECT_EQ(dump_op.stream_id_, 67890U);
}

TEST_F(UTEST_dump_op, SetWorkspaceAddrs) {
  DumpOp dump_op;

  std::vector<std::pair<uintptr_t, int64_t>> workspace_addrs = {
    {0x1000, 1024},
    {0x2000, 2048},
    {0x3000, 4096}
  };

  dump_op.SetWorkspaceAddrs(workspace_addrs);

  EXPECT_EQ(dump_op.space_addrs_.size(), 3U);
  EXPECT_EQ(dump_op.space_addrs_[0].first, 0x1000UL);
  EXPECT_EQ(dump_op.space_addrs_[0].second, 1024);
  EXPECT_EQ(dump_op.space_addrs_[1].first, 0x2000UL);
  EXPECT_EQ(dump_op.space_addrs_[1].second, 2048);
  EXPECT_EQ(dump_op.space_addrs_[2].first, 0x3000UL);
  EXPECT_EQ(dump_op.space_addrs_[2].second, 4096);
}
}  // namespace ge
