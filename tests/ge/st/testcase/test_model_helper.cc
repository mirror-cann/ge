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
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include "faker/fake_value.h"
#include "faker/global_data_faker.h"
#include "faker/aicpu_taskdef_faker.h"
#include "faker/aicore_taskdef_faker.h"
#include "faker/model_data_faker.h"
#include "faker/ge_model_builder.h"
#include "faker/label_switch_task_def_faker.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/tensor_utils.h"
#include "common/share_graph.h"
#include "mmpa/mmpa_api.h"
#include "securec.h"
#include "graph/operator_reg.h"
#include "graph_metadef/graph/custom_op_factory.h"
#include "graph/utils/op_desc_utils.h"
#include "framework/common/ge_types.h"
#include "stub/hostcpu_mmpa_stub.h"
#include "check/executor_statistician.h"
#include "ge_running_env/path_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "framework/common/helper/model_helper.h"
#include "common/helper/visual_json_converter.h"
#include "framework/common/helper/nano_model_save_helper.h"
#include "framework/common/helper/pre_model_helper.h"
#include "framework/common/tlv/nano_dbg_desc.h"
#include "common/op_so_store/op_so_store.h"
#include "common/preload/dbg/nano_dbg_data.h"
#include "common/preload/model/pre_model_partition_utils.h"
#include "ge_local_context.h"
#include "common/path_utils.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "common/env_path.h"
#include "graph_metadef/common/ge_common/util.h"
#include "nlohmann/json.hpp"

namespace ge {
using namespace gert;
class ModelHelperTest : public testing::Test {
 protected:
  void SetUp() {}
  void TearDown() {}
};

static const nlohmann::json *FindJsonMapValue(const nlohmann::json &entries, const std::string &key) {
  for (const auto &entry : entries) {
    if (entry.contains("key") && entry["key"] == key && entry.contains("value")) {
      return &entry["value"];
    }
  }
  return nullptr;
}

GeModelPtr BuildNameIndexVisualModel() {
  auto graph = MakeShared<ComputeGraph>("visual_graph");
  GeTensorDesc tensor_desc(GeShape({1}), FORMAT_ND, DT_FLOAT);
  auto op_desc = MakeShared<OpDesc>("visual_op", "VisualOp");
  if ((graph == nullptr) || (op_desc == nullptr)) {
    return nullptr;
  }
  (void)op_desc->AddInputDesc(tensor_desc);
  (void)op_desc->AddInputDesc(tensor_desc);
  (void)op_desc->AddOutputDesc(tensor_desc);
  (void)op_desc->UpdateInputName({{"x", 0U}, {"y", 1U}});
  (void)op_desc->UpdateOutputName({{"z", 0U}});
  if (graph->AddNode(op_desc) == nullptr) {
    return nullptr;
  }
  auto ge_model = MakeShared<GeModel>();
  if (ge_model != nullptr) {
    ge_model->SetGraph(graph);
  }
  return ge_model;
}

void ExpectVisualNameIndexAttrs(const nlohmann::json &attrs) {
  ASSERT_TRUE(attrs.contains("_input_name_key"));
  ASSERT_TRUE(attrs.contains("_input_name_value"));
  ASSERT_TRUE(attrs.contains("_output_name_key"));
  ASSERT_TRUE(attrs.contains("_output_name_value"));
  EXPECT_EQ(attrs["_input_name_key"]["type"], "list_string");
  EXPECT_EQ(attrs["_input_name_key"]["value"][0], "x");
  EXPECT_EQ(attrs["_input_name_key"]["value"][1], "y");
  EXPECT_EQ(attrs["_input_name_value"]["type"], "list_int");
  EXPECT_EQ(attrs["_input_name_value"]["value"][0], 0);
  EXPECT_EQ(attrs["_input_name_value"]["value"][1], 1);
  EXPECT_EQ(attrs["_output_name_key"]["type"], "list_string");
  EXPECT_EQ(attrs["_output_name_key"]["value"][0], "z");
  EXPECT_EQ(attrs["_output_name_value"]["type"], "list_int");
  EXPECT_EQ(attrs["_output_name_value"]["value"][0], 0);
}

void ExpectPbNameIndexAttrs(const nlohmann::json &pb_attrs) {
  ASSERT_TRUE(pb_attrs.is_array());
  const auto *input_name_key = FindJsonMapValue(pb_attrs, "_input_name_key");
  const auto *input_name_value = FindJsonMapValue(pb_attrs, "_input_name_value");
  const auto *output_name_key = FindJsonMapValue(pb_attrs, "_output_name_key");
  const auto *output_name_value = FindJsonMapValue(pb_attrs, "_output_name_value");
  ASSERT_NE(input_name_key, nullptr);
  ASSERT_NE(input_name_value, nullptr);
  ASSERT_NE(output_name_key, nullptr);
  ASSERT_NE(output_name_value, nullptr);
  EXPECT_EQ((*input_name_key)["list"]["val_type"], proto::AttrDef_ListValue_ListValueType_VT_LIST_STRING);
  EXPECT_EQ((*input_name_key)["list"]["s"][0], "x");
  EXPECT_EQ((*input_name_key)["list"]["s"][1], "y");
  EXPECT_EQ((*input_name_value)["list"]["val_type"], proto::AttrDef_ListValue_ListValueType_VT_LIST_INT);
  EXPECT_EQ((*input_name_value)["list"]["i"][0], 0);
  EXPECT_EQ((*input_name_value)["list"]["i"][1], 1);
  EXPECT_EQ((*output_name_key)["list"]["val_type"], proto::AttrDef_ListValue_ListValueType_VT_LIST_STRING);
  EXPECT_EQ((*output_name_key)["list"]["s"][0], "z");
  EXPECT_EQ((*output_name_value)["list"]["val_type"], proto::AttrDef_ListValue_ListValueType_VT_LIST_INT);
  EXPECT_EQ((*output_name_value)["list"]["i"][0], 0);
}

GeModelPtr BuildStructuredSubgraphVisualModel(const std::string &bytes_payload) {
  auto root_graph = MakeShared<ComputeGraph>("root_graph");
  auto sub_graph = MakeShared<ComputeGraph>("branch_graph");
  GeTensorDesc tensor_desc(GeShape({2, 2}), FORMAT_ND, DT_FLOAT);
  auto data_desc = MakeShared<OpDesc>("data", DATA);
  auto call_desc = MakeShared<OpDesc>("partition_call", PARTITIONEDCALL);
  auto output_desc = MakeShared<OpDesc>("net_output", NETOUTPUT);
  auto sub_data_desc = MakeShared<OpDesc>("sub_data", DATA);
  auto sub_output_desc = MakeShared<OpDesc>("sub_net_output", NETOUTPUT);
  if ((root_graph == nullptr) || (sub_graph == nullptr) || (data_desc == nullptr) || (call_desc == nullptr) ||
      (output_desc == nullptr) || (sub_data_desc == nullptr) || (sub_output_desc == nullptr)) {
    return nullptr;
  }
  (void)data_desc->AddOutputDesc(tensor_desc);
  (void)call_desc->AddInputDesc(tensor_desc);
  (void)call_desc->AddOutputDesc(tensor_desc);
  call_desc->AddSubgraphName(sub_graph->GetName());
  call_desc->SetSubgraphInstanceName(0U, sub_graph->GetName());
  (void)AttrUtils::SetBytes(
      call_desc, "func_def",
      Buffer::CopyFrom(reinterpret_cast<const uint8_t *>(bytes_payload.data()), bytes_payload.size()));
  (void)output_desc->AddInputDesc(tensor_desc);
  (void)sub_data_desc->AddOutputDesc(tensor_desc);
  (void)sub_output_desc->AddInputDesc(tensor_desc);
  const auto data = root_graph->AddNode(data_desc);
  const auto call = root_graph->AddNode(call_desc);
  const auto output = root_graph->AddNode(output_desc);
  const auto sub_data = sub_graph->AddNode(sub_data_desc);
  const auto sub_output = sub_graph->AddNode(sub_output_desc);
  if ((data == nullptr) || (call == nullptr) || (output == nullptr) || (sub_data == nullptr) ||
      (sub_output == nullptr)) {
    return nullptr;
  }
  sub_graph->SetParentNode(call);
  sub_graph->SetParentGraph(root_graph);
  root_graph->AddSubgraph(sub_graph);
  auto ge_model = MakeShared<GeModel>();
  if (ge_model != nullptr) {
    ge_model->SetGraph(root_graph);
  }
  return ge_model;
}

void ExpectStructuredSubgraphVisualJson(const nlohmann::json &graphs, const std::string &bytes_payload) {
  const nlohmann::json *root_json = nullptr;
  const nlohmann::json *sub_json = nullptr;
  for (const auto &graph_json : graphs) {
    if (graph_json.contains("name") && graph_json["name"] == "root_graph") {
      root_json = &graph_json;
    }
    if (graph_json.contains("name") && graph_json["name"] == "branch_graph") {
      sub_json = &graph_json;
    }
  }
  ASSERT_NE(root_json, nullptr);
  ASSERT_NE(sub_json, nullptr);
  ASSERT_TRUE(sub_json->contains("op"));
  ASSERT_EQ((*sub_json)["op"].size(), 2U);
  const auto &root_ops = (*root_json)["op"];
  auto call_it = std::find_if(root_ops.begin(), root_ops.end(), [](const nlohmann::json &op_json) {
    return op_json.contains("name") && op_json["name"] == "partition_call";
  });
  ASSERT_NE(call_it, root_ops.end());
  ASSERT_TRUE(call_it->contains("subgraph_name"));
  ASSERT_EQ((*call_it)["subgraph_name"][0], "branch_graph");
  ASSERT_TRUE((*call_it)["attr"].contains("func_def"));
  EXPECT_EQ((*call_it)["attr"]["func_def"]["type"], "bytes");
  EXPECT_EQ((*call_it)["attr"]["func_def"]["value"], bytes_payload);
}

std::vector<char> CreateStubBin() {
  auto ascend_install_path = ge::EnvPath().GetAscendInstallPath();
  std::string op_bin_path = ascend_install_path + "/fwkacllib/lib64/switch_by_index.o";
  std::vector<char> buf;
  std::ifstream file(op_bin_path.c_str(), std::ios::binary | std::ios::in);
  if (!file.is_open()) {
    std::cout << "file:" << op_bin_path << "does not exist or is unaccessible." << std::endl;
    return buf;
  }
  GE_MAKE_GUARD(file_guard, [&file]() { (void)file.close(); });
  const std::streampos begin = file.tellg();
  (void)file.seekg(0, std::ios::end);
  const std::streampos end = file.tellg();
  auto buf_len = static_cast<uint32_t>(end - begin);
  buf.resize(buf_len + 1);
  (void)file.seekg(0, std::ios::beg);
  (void)file.read(const_cast<char *>(buf.data()), buf_len);
  std::cout << std::string(buf.data()) << std::endl;
  return buf;
}

static void CheckDbgFileTLv(const std::string &filename) {
  std::ifstream fs(filename, std::ifstream::in);
  EXPECT_EQ(fs.is_open(), true);

  std::ostringstream ctx;
  ctx << fs.rdbuf();
  fs.close();

  string dbg_tlv = ctx.str();
  const char *buff = dbg_tlv.c_str();
  size_t buff_size = dbg_tlv.length();

  size_t len = 0;
  const DbgDataHead *head = (const DbgDataHead *)buff;
  EXPECT_EQ(head->version_id, 0);
  EXPECT_EQ(head->magic, DBG_DATA_HEAD_MAGIC);
  len = len + sizeof(DbgDataHead);

  const TlvHead *tlv = (TlvHead *)head->tlv;
  EXPECT_EQ(tlv->type, DBG_L1_TLV_TYPE_MODEL_NAME);
  len = len + sizeof(TlvHead) + tlv->len;

  tlv = (const TlvHead *)(tlv->data + tlv->len);
  EXPECT_EQ(tlv->type, DBG_L1_TLV_TYPE_OP_DESC);
  DbgOpDescTlv1 *op_tlv = (DbgOpDescTlv1 *)tlv->data;
  EXPECT_EQ(op_tlv->num, 3);
  len = len + sizeof(TlvHead) + tlv->len;

  EXPECT_EQ(buff_size, len);
}

void SetNanoLdLibraryPath(char old_env[MMPA_MAX_PATH]) {
  auto ascend_install_path = EnvPath().GetAscendInstallPath();
  (void)mmGetEnv("LD_LIBRARY_PATH", old_env, MMPA_MAX_PATH);
  setenv("LD_LIBRARY_PATH", (ascend_install_path + "/fwkacllib/lib64").c_str(), 1);
}

void RestoreLdLibraryPath(const char old_env[MMPA_MAX_PATH]) {
  unsetenv("LD_LIBRARY_PATH");
  mmSetEnv("LD_LIBRARY_PATH", old_env, 1);
}

std::string GetNanoOutputPrefix() {
  auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  return PathJoin(om_path.c_str(), "pb_exeom_for_nano");
}

void ExpectNanoSaveFiles(const std::string &om_path) {
  EXPECT_EQ(IsFile((om_path + ".exeom").c_str()), true);
  EXPECT_EQ(IsFile((om_path + ".dbg").c_str()), true);
}

void SaveNanoRootModelAndCheck(const GeRootModelPtr &ge_root_model, const std::string &om_path) {
  ModelBufferData model_buff;
  NanoModelSaveHelper helper;
  helper.SetSaveMode(true);
  Status ret = helper.SaveToOmRootModel(ge_root_model, om_path + ".exeom", model_buff, false);
  EXPECT_EQ(ret, SUCCESS);
  ExpectNanoSaveFiles(om_path);
  CheckDbgFileTLv((om_path + ".dbg").c_str());
}

void SaveNanoRootModelAndExpectFiles(const GeRootModelPtr &ge_root_model, const std::string &om_path) {
  ModelBufferData model_buff;
  NanoModelSaveHelper helper;
  helper.SetSaveMode(true);
  Status ret = helper.SaveToOmRootModel(ge_root_model, om_path + ".exeom", model_buff, false);
  EXPECT_EQ(ret, SUCCESS);
  ExpectNanoSaveFiles(om_path);
}

void SaveNanoExeModelAndCheck(const GeModelPtr &ge_model, const std::string &om_path) {
  ModelBufferData model_buff;
  NanoModelSaveHelper helper;
  helper.SetSaveMode(true);
  Status ret = helper.SaveToExeOmModel(ge_model, om_path + ".exeom", model_buff);
  EXPECT_EQ(ret, SUCCESS);
  ExpectNanoSaveFiles(om_path);
  CheckDbgFileTLv((om_path + ".dbg").c_str());
}

void AddLabelSetTask(const std::shared_ptr<domi::ModelTaskDef> &model_def, const OpDescPtr &op_desc) {
  domi::TaskDef *task_def = model_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_LABEL_SET));
  task_def->set_stream_id(op_desc->GetStreamId());
  task_def->mutable_label_set()->set_op_index(op_desc->GetId());
}

void AddStreamActiveTask(const std::shared_ptr<domi::ModelTaskDef> &model_def, const OpDescPtr &op_desc) {
  domi::TaskDef *task_def = model_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_STREAM_ACTIVE));
  task_def->set_stream_id(op_desc->GetStreamId());
  task_def->mutable_stream_active()->set_op_index(op_desc->GetId());
}

domi::KernelDef *AddKernelTask(const std::shared_ptr<domi::ModelTaskDef> &model_def, const OpDescPtr &op_desc,
                               const std::string &kernel_name, ccKernelType kernel_type) {
  domi::TaskDef *task_def = model_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_KERNEL));
  task_def->set_stream_id(op_desc->GetStreamId());
  domi::KernelDef *kernel_def = task_def->mutable_kernel();
  kernel_def->set_stub_func("stub_func");
  kernel_def->set_args_size(64);
  string args(64, '1');
  kernel_def->set_args(args.data(), 64);
  kernel_def->set_kernel_name(kernel_name);
  domi::KernelContext *context = kernel_def->mutable_context();
  context->set_kernel_type(static_cast<uint32_t>(kernel_type));
  context->set_op_index(op_desc->GetId());
  uint16_t args_offset[9] = {0};
  context->set_args_offset(args_offset, 9 * sizeof(uint16_t));
  return kernel_def;
}

void AddKernelBin(TBEKernelStore &tbe_kernel_store, const GeModelPtr &ge_model, const std::string &kernel_name) {
  const auto kernel = MakeShared<OpKernelBin>(kernel_name, CreateStubBin());
  tbe_kernel_store.AddTBEKernel(kernel);
  ge_model->SetTBEKernelStore(tbe_kernel_store);
}

void SetSwitchOutput(const OpDescPtr &op_desc) {
  GeTensorDesc tensor(GeShape({1, 4, 4, 8}), FORMAT_NCHW, DT_FLOAT);
  TensorUtils::SetSize(tensor, 512);
  op_desc->AddOutputDesc(tensor);
  op_desc->SetOutputOffset({1024});
}

void AddSwitchByIndexTask(const std::shared_ptr<domi::ModelTaskDef> &model_def, const OpDescPtr &op_desc) {
  SetSwitchOutput(op_desc);
  domi::TaskDef *task_def = model_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_STREAM_LABEL_SWITCH_BY_INDEX));
  task_def->set_stream_id(op_desc->GetStreamId());
  domi::KernelDef *kernel_def = task_def->mutable_kernel();
  kernel_def->set_stub_func("stub_func");
  kernel_def->set_args_size(64);
  string args(64, '1');
  kernel_def->set_args(args.data(), 64);
  kernel_def->set_kernel_name("switch_by_index.o");
  domi::LabelSwitchByIndexDef *switch_task_def = task_def->mutable_label_switch_by_index();
  switch_task_def->set_op_index(op_desc->GetId());
  switch_task_def->set_label_max(2);
}

void AddLabelGotoTask(const std::shared_ptr<domi::ModelTaskDef> &model_def, const OpDescPtr &op_desc) {
  SetSwitchOutput(op_desc);
  domi::TaskDef *task_def = model_def->add_task();
  task_def->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_STREAM_LABEL_GOTO));
  task_def->set_stream_id(op_desc->GetStreamId());
  domi::KernelDef *kernel_def = task_def->mutable_kernel();
  kernel_def->set_stub_func("stub_func");
  kernel_def->set_args_size(64);
  string args(64, '1');
  kernel_def->set_args(args.data(), 64);
  kernel_def->set_kernel_name("switch_by_index.o");
  task_def->mutable_label_goto_ex()->set_op_index(op_desc->GetId());
}

GeRootModelPtr BuildAtcNanoRootModel(ComputeGraphPtr &graph) {
  graph = ShareGraph::AtcNanoGraph();
  graph->TopologicalSorting();
  GeModelBuilder builder(graph);
  auto ge_root_model =
      builder.AddTaskDef("Add", AiCoreTaskDefFaker("AddStubBin1")).FakeTbeBin({"Add"}).BuildGeRootModel();
  auto ge_model_map = ge_root_model->GetSubgraphInstanceNameToModel();
  auto ge_model = ge_model_map[graph->GetName()];
  auto model_task_def = ge_model->GetModelTaskDefPtr();
  model_task_def->mutable_task(0)->mutable_kernel()->set_kernel_name("add1_faked_kernel");
  model_task_def->mutable_task(1)->mutable_kernel()->set_kernel_name("add2_faked_kernel");
  model_task_def->mutable_task(2)->mutable_kernel()->set_kernel_name("add3_faked_kernel");
  graph->FindNode("add1")->GetOpDesc()->SetInputOffset({0, 2048});
  graph->FindNode("add1")->GetOpDesc()->SetOutputOffset({0});
  graph->FindNode("add2")->GetOpDesc()->SetInputOffset({0, 2048});
  graph->FindNode("add2")->GetOpDesc()->SetOutputOffset({0});
  graph->FindNode("add3")->GetOpDesc()->SetInputOffset({0, 2048});
  graph->FindNode("add3")->GetOpDesc()->SetOutputOffset({0});
  return ge_root_model;
}

void SetHostFuncAttrs(const OpDescPtr &op_desc) {
  op_desc->SetAttr("int_test", GeAttrValue::CreateFrom<int64_t>(100));
  op_desc->SetAttr("str_test", GeAttrValue::CreateFrom<std::string>("Hello!"));
  op_desc->SetAttr("float_test", GeAttrValue::CreateFrom<float>(10.101));
  op_desc->SetAttr("list_int_test", GeAttrValue::CreateFrom<std::vector<int64_t>>({1, 2, 3}));
  op_desc->SetAttr("list_float_test", GeAttrValue::CreateFrom<std::vector<float>>({1.2, 3.4, 4.5}));
  op_desc->SetAttr("list_str_test", GeAttrValue::CreateFrom<std::vector<std::string>>({"Hello!", "ABCD"}));
  op_desc->SetAttr("list_bool_test", GeAttrValue::CreateFrom<std::vector<bool>>({true, false}));
  op_desc->SetAttr("list_list_int_test", GeAttrValue::CreateFrom<std::vector<std::vector<int64_t>>>({{1, 2}, {3, 4}}));
  op_desc->SetAttr("list_list_float_test",
                   GeAttrValue::CreateFrom<std::vector<std::vector<float>>>({{1.2, 3.4}, {5.6, 7.8}}));
}

void BuildNanoHostFuncModel(const ComputeGraphPtr &graph) {
  GeModelPtr ge_model = MakeShared<GeModel>();
  ge_model->SetGraph(graph);
  const auto model_def = MakeShared<domi::ModelTaskDef>();
  ge_model->SetModelTaskDef(model_def);
  const auto &node = graph->FindNode("add1");
  const auto &op_desc = node->GetOpDesc();
  op_desc->SetOutputOffset({8});
  op_desc->SetInputOffset({0, 2048});
  PreModelPartitionUtils::GetInstance().SetZeroCopyTable(128, 0);
  PreModelPartitionUtils::GetInstance().SetZeroCopyTable(1024, 8);
  SetHostFuncAttrs(op_desc);

  TBEKernelStore tbe_kernel_store;
  const auto kernel = MakeShared<OpKernelBin>("test", CreateStubBin());
  tbe_kernel_store.AddTBEKernel(kernel);
  ge_model->SetTBEKernelStore(tbe_kernel_store);
  auto *kernel_def = AddKernelTask(model_def, op_desc, "test", ccKernelType::AI_CPU);
  kernel_def->mutable_context()->set_op_index(op_desc->GetId());
}

GeModelPtr CreateNanoExeGeModel(const ComputeGraphPtr &graph, std::shared_ptr<domi::ModelTaskDef> &model_def,
                                TBEKernelStore &tbe_kernel_store) {
  GeModelPtr ge_model = MakeShared<GeModel>();
  ge_model->SetGraph(graph);
  model_def = MakeShared<domi::ModelTaskDef>();
  ge_model->SetModelTaskDef(model_def);
  tbe_kernel_store = ge_model->GetTBEKernelStore();
  return ge_model;
}

GeModelPtr BuildWhileSwitchGeModel(const ComputeGraphPtr &graph) {
  std::shared_ptr<domi::ModelTaskDef> model_def;
  TBEKernelStore tbe_kernel_store;
  auto ge_model = CreateNanoExeGeModel(graph, model_def, tbe_kernel_store);
  auto cond_graph = graph->GetSubgraph("cond_instance");
  auto body_graph = graph->GetSubgraph("body_instance");
  AddLabelSetTask(model_def, cond_graph->FindNode("LabelSet_0")->GetOpDesc());
  AddStreamActiveTask(model_def, cond_graph->FindNode("Stream_0")->GetOpDesc());
  AddKernelBin(tbe_kernel_store, ge_model, "LessThan_5");
  AddKernelTask(model_def, cond_graph->FindNode("LessThan_5")->GetOpDesc(), "LessThan_5", ccKernelType::TE);
  AddSwitchByIndexTask(model_def, cond_graph->FindNode("SwitchByIndex")->GetOpDesc());
  AddLabelSetTask(model_def, body_graph->FindNode("LabelSet_1")->GetOpDesc());
  AddStreamActiveTask(model_def, body_graph->FindNode("Stream_1")->GetOpDesc());
  AddKernelBin(tbe_kernel_store, ge_model, "add");
  AddKernelTask(model_def, body_graph->FindNode("add")->GetOpDesc(), "add", ccKernelType::TE);
  AddLabelGotoTask(model_def, body_graph->FindNode("LabelGoto")->GetOpDesc());
  AddLabelSetTask(model_def, body_graph->FindNode("LabelSet_2")->GetOpDesc());
  AddKernelBin(tbe_kernel_store, ge_model, "NetOutput");
  AddKernelTask(model_def, graph->FindNode("NetOutput")->GetOpDesc(), "NetOutput", ccKernelType::TE);
  return ge_model;
}

GeModelPtr BuildIfSwitchGeModel(const ComputeGraphPtr &graph) {
  std::shared_ptr<domi::ModelTaskDef> model_def;
  TBEKernelStore tbe_kernel_store;
  auto ge_model = CreateNanoExeGeModel(graph, model_def, tbe_kernel_store);
  auto then_graph = graph->GetSubgraph("then");
  auto else_graph = graph->GetSubgraph("else");
  AddSwitchByIndexTask(model_def, then_graph->FindNode("SwitchByIndex")->GetOpDesc());
  AddLabelSetTask(model_def, then_graph->FindNode("LabelSet_0")->GetOpDesc());
  AddStreamActiveTask(model_def, then_graph->FindNode("Stream_0")->GetOpDesc());
  AddKernelBin(tbe_kernel_store, ge_model, "shape");
  AddKernelTask(model_def, then_graph->FindNode("shape")->GetOpDesc(), "shape", ccKernelType::TE);
  AddLabelGotoTask(model_def, then_graph->FindNode("LabelGoto")->GetOpDesc());
  AddLabelSetTask(model_def, else_graph->FindNode("LabelSet_1")->GetOpDesc());
  AddStreamActiveTask(model_def, else_graph->FindNode("Stream_1")->GetOpDesc());
  AddKernelBin(tbe_kernel_store, ge_model, "shape");
  AddKernelTask(model_def, else_graph->FindNode("shape")->GetOpDesc(), "shape", ccKernelType::TE);
  AddLabelSetTask(model_def, else_graph->FindNode("LabelSet_2")->GetOpDesc());
  AddKernelBin(tbe_kernel_store, ge_model, "NetOutput");
  AddKernelTask(model_def, graph->FindNode("NetOutput")->GetOpDesc(), "NetOutput", ccKernelType::TE);
  return ge_model;
}

void PrepareFifoWindowCacheModel(const GeModelPtr &ge_model, const ComputeGraphPtr &graph) {
  std::vector<uint64_t> weights_value(64, 1024);
  size_t weight_size = weights_value.size() * sizeof(uint64_t);
  ge_model->SetGraph(graph);
  ge_model->SetWeight(Buffer::CopyFrom((uint8_t *)weights_value.data(), weight_size));
  EXPECT_TRUE(AttrUtils::SetInt(ge_model, ATTR_MODEL_MEMORY_SIZE, 10240));
  EXPECT_TRUE(AttrUtils::SetInt(ge_model, ATTR_MODEL_STREAM_NUM, 3));
  EXPECT_TRUE(AttrUtils::SetInt(ge_model, ATTR_MODEL_EVENT_NUM, 1));
  EXPECT_TRUE(AttrUtils::SetInt(ge_model, ATTR_MODEL_LABEL_NUM, 0));
  EXPECT_TRUE(AttrUtils::SetInt(ge_model, ATTR_MODEL_VAR_SIZE, 5120));
}

void AddFifoWindowCacheTasks(const GeModelPtr &ge_model, const ComputeGraphPtr &graph) {
  const auto model_def = MakeShared<domi::ModelTaskDef>();
  ge_model->SetModelTaskDef(model_def);
  TBEKernelStore tbe_kernel_store = ge_model->GetTBEKernelStore();
  AddKernelBin(tbe_kernel_store, ge_model, "fillwindowcache");
  AddKernelTask(model_def, graph->FindNode("fillwindowcache")->GetOpDesc(), "fillwindowcache", ccKernelType::TE);

  const auto &conv_op_desc = graph->FindNode("conv")->GetOpDesc();
  conv_op_desc->SetInputOffset({0});
  conv_op_desc->SetOutputOffset({32});
  ge::AttrUtils::SetListInt(conv_op_desc, ATTR_NAME_INPUT_MEM_TYPE_LIST, {RT_MEMORY_L1});
  ge::AttrUtils::SetListInt(conv_op_desc, ATTR_NAME_OUTPUT_MEM_TYPE_LIST, {RT_MEMORY_L1});
  AddKernelBin(tbe_kernel_store, ge_model, "conv");
  AddKernelTask(model_def, conv_op_desc, "conv", ccKernelType::TE);
}

ComputeGraphPtr BuildAutofuseGraphWithStub(const std::string &om_path) {
  auto graph = ShareGraph::AutoFuseNodeGraph();
  EXPECT_NE(graph, nullptr);
  auto autofuse_stub = PathJoin(om_path.c_str(), "libautofuse_stub.so");
  std::ofstream(autofuse_stub) << "autofuse_bin";
  for (auto n : graph->GetAllNodesPtr()) {
    (void)ge::AttrUtils::SetStr(n->GetOpDesc(), "bin_file_path", autofuse_stub);
  }
  (void)AttrUtils::SetStr(graph, "_guard_check_so_data", "guard_stub_bin_content");
  return graph;
}

std::string SaveAutofuseRootModel(const GeRootModelPtr &ge_root_model, const std::string &om_path) {
  (void)GetThreadLocalContext().SetGlobalOption({{"ge.host_env_os", "linux"}, {"ge.host_env_cpu", "x86_64"}});
  const std::string output = PathJoin(om_path.c_str(), "autofuse_repack") + "_linux_x86_64.om";
  ModelBufferData first;
  ModelHelper helper;
  helper.SetSaveMode(true);
  EXPECT_EQ(helper.SaveToOmRootModel(ge_root_model, output, first, false), SUCCESS);
  return output;
}

std::string GetAutofuseActualOutput(const std::string &output) {
  std::string actual_output = output;
  const auto dot_pos = actual_output.find(".om");
  if (dot_pos < actual_output.length()) {
    actual_output.insert(dot_pos, "_linux_x86_64");
  } else {
    actual_output.append("_linux_x86_64");
  }
  return actual_output;
}

void ExpectAutofuseRepackKeepsSo(const std::string &output) {
  const auto actual_output = GetAutofuseActualOutput(output);
  std::ifstream ifs(actual_output, std::ios::binary);
  ASSERT_TRUE(ifs.is_open());
  std::vector<uint8_t> file_data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
  ifs.close();
  ASSERT_FALSE(file_data.empty());

  ModelHelper repack;
  repack.SetRepackSoFlag(true);
  ModelData data{file_data.data(), file_data.size()};
  ASSERT_EQ(repack.LoadRootModel(data), SUCCESS);
  ModelBufferData repacked;
  repack.PackSoToModelData(data, actual_output, repacked, false);
  EXPECT_GT(repack.GetOpStoreDataSize(), 0U);

  auto repack_root = repack.GetGeRootModel();
  ASSERT_NE(repack_root, nullptr);
  std::string restored_guard_data;
  EXPECT_TRUE(AttrUtils::GetStr(repack_root->GetRootGraph(), "_guard_check_so_data", restored_guard_data));
}

TEST_F(ModelHelperTest, SaveToOmRootModel_For_Nano) {
  char old_env[MMPA_MAX_PATH] = {'\0'};
  SetNanoLdLibraryPath(old_env);
  ComputeGraphPtr graph;
  auto ge_root_model = BuildAtcNanoRootModel(graph);
  SaveNanoRootModelAndCheck(ge_root_model, GetNanoOutputPrefix());
  RestoreLdLibraryPath(old_env);
}

TEST_F(ModelHelperTest, SaveToOmRootModel_For_NanoHostFunc) {
  char old_env[MMPA_MAX_PATH] = {'\0'};
  SetNanoLdLibraryPath(old_env);
  ComputeGraphPtr graph;
  auto ge_root_model = BuildAtcNanoRootModel(graph);
  BuildNanoHostFuncModel(graph);
  SaveNanoRootModelAndCheck(ge_root_model, GetNanoOutputPrefix());
  RestoreLdLibraryPath(old_env);
}

TEST_F(ModelHelperTest, SaveToOmRootModel_For_NanoWHILESwitch) {
  char old_env[MMPA_MAX_PATH] = {'\0'};
  SetNanoLdLibraryPath(old_env);
  auto graph = ShareGraph::WhileGraph3();
  graph->TopologicalSorting();
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  (void)ge_root_model;
  auto ge_model = BuildWhileSwitchGeModel(graph);

  dlog_setlevel(-1, 0, 1);
  SaveNanoExeModelAndCheck(ge_model, GetNanoOutputPrefix());
  RestoreLdLibraryPath(old_env);
}

TEST_F(ModelHelperTest, SaveToOmRootModel_For_NanoIFSwitch) {
  char old_env[MMPA_MAX_PATH] = {'\0'};
  SetNanoLdLibraryPath(old_env);
  auto graph = ShareGraph::IfGraphWithSwitch();
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  (void)ge_root_model;
  auto ge_model = BuildIfSwitchGeModel(graph);

  SaveNanoExeModelAndCheck(ge_model, GetNanoOutputPrefix());
  RestoreLdLibraryPath(old_env);
}

TEST_F(ModelHelperTest, SaveToOmRootModel_For_NanoFIFOtest) {
  char old_env[MMPA_MAX_PATH] = {'\0'};
  SetNanoLdLibraryPath(old_env);
  auto graph = ShareGraph::GraphWithFifoWindowCache();
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  auto sub_models = ge_root_model->GetSubgraphInstanceNameToModel();
  EXPECT_EQ(sub_models.size(), 1);
  auto ge_model = sub_models[graph->GetName()];
  EXPECT_NE(ge_model, nullptr);

  PreModelPartitionUtils::GetInstance().SetZeroCopyTable(128, 0);
  PreModelPartitionUtils::GetInstance().SetZeroCopyTable(136, 8);
  PrepareFifoWindowCacheModel(ge_model, graph);
  AddFifoWindowCacheTasks(ge_model, graph);

  SaveNanoRootModelAndExpectFiles(ge_root_model, GetNanoOutputPrefix());
  dlog_setlevel(-1, 3, 0);
  RestoreLdLibraryPath(old_env);
}

TEST_F(ModelHelperTest, SaveToOm_for_SplitAndUpgraded_Opp) {
  auto options = GetThreadLocalContext().GetAllGlobalOptions();
  std::vector<std::string> paths;
  CreateBuiltInSplitAndUpgradedSo(paths);

  GeModelPtr ge_model = ge::MakeShared<GeModel>();
  ComputeGraphPtr graph = ge::MakeShared<ComputeGraph>("g1");
  ge_model->SetGraph(graph);
  std::string output_file{"split_opp.om"};
  ModelBufferData buffer_data;
  GeRootModelPtr ge_root_model = ge::MakeShared<GeRootModel>();
  ComputeGraphPtr root_graph = ge::MakeShared<ComputeGraph>("subgraph");
  (void)AttrUtils::SetBool(root_graph, ATTR_NAME_DYNAMIC_SHAPE_PARTITIONED, true);
  ge_root_model->SetRootGraph(root_graph);
  ge_root_model->SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  EXPECT_EQ(ge_root_model->GetSoInOmFlag(), 0x8000);

  std::map<string, string> env_options;
  env_options["ge.host_env_os"] = "linux";
  env_options["ge.host_env_cpu"] = "x86_64";
  (void)GetThreadLocalContext().SetGlobalOption(env_options);

  ModelHelper model_helper;
  auto ret = model_helper.SaveToOmModel(ge_model, output_file, buffer_data, ge_root_model);
  EXPECT_NE(ret, SUCCESS);
  (void)GetThreadLocalContext().SetGlobalOption(options);
  for (const auto &path : paths) {
    ge::PathUtils::RemoveDirectories(path);
  }
}

// 验证Repack路径下autofuse SO不丢失：构建带Autofuse节点的模型，首次保存SO_BINS含autofuse SO，
// 加载后执行PackSoToModelData，验证repack后SO store数据非空。
TEST_F(ModelHelperTest, SaveAutofuseSoRepackPathPreservesSoBins) {
  auto options = GetThreadLocalContext().GetAllGlobalOptions();
  std::vector<std::string> paths;
  CreateBuiltInSplitAndUpgradedSo(paths);

  const auto om_path = PathJoin(GetRunPath().c_str(), "temp");
  Mkdir(om_path.c_str());
  auto graph = BuildAutofuseGraphWithStub(om_path);

  gert::GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  EXPECT_EQ(ge_root_model->CheckAndSetNeedSoInOM(), SUCCESS);
  ExpectAutofuseRepackKeepsSo(SaveAutofuseRootModel(ge_root_model, om_path));

  (void)GetThreadLocalContext().SetGlobalOption(options);
  for (const auto &path : paths) {
    ge::PathUtils::RemoveDirectories(path);
  }
}

TEST_F(ModelHelperTest, VisualJsonFromGeModelKeepsOpNameIndexAttrsWithoutValType) {
  auto ge_model = BuildNameIndexVisualModel();
  ASSERT_NE(ge_model, nullptr);
  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromGeModel(ge_model, json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  ExpectVisualNameIndexAttrs(j["model"]["graph"][0]["op"][0]["attr"]);

  nlohmann::json pb_json;
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(json_str, pb_json), SUCCESS);
  ExpectPbNameIndexAttrs(pb_json["graph"][0]["op"][0]["attr"]);
}

TEST_F(ModelHelperTest, VisualJsonFromGeModelKeepsStructuredSubgraphAndBytesAttr) {
  const std::string bytes_payload = "serialized_function_def";
  auto ge_model = BuildStructuredSubgraphVisualModel(bytes_payload);
  ASSERT_NE(ge_model, nullptr);
  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromGeModel(ge_model, json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  const auto &graphs = j["model"]["graph"];
  ASSERT_TRUE(graphs.is_array());
  ASSERT_GE(graphs.size(), 2U);
  ExpectStructuredSubgraphVisualJson(graphs, bytes_payload);
}

}  // namespace ge
