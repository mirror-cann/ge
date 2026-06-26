/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include "engine/aicpu/converter/aicpu_node_converter.h"

#include <gtest/gtest.h>

#include "ge_graph_dsl/graph_dsl.h"
#include "engine/gelocal/inputs_converter.h"
#include "graph/utils/graph_utils.h"
#include "graph_builder/exe_graph_comparer.h"
#include "graph_builder/value_holder_generator.h"
#include "common/share_graph.h"
#include "faker/global_data_faker.h"
#include "common/bg_test.h"
#include "faker/aicpu_taskdef_faker.h"
#include "framework/common/ge_types.h"
#include "mmpa/mmpa_api.h"
#include "stub/hostcpu_mmpa_stub.h"
#include "register/kernel_registry.h"
#include "common/const_data_helper.h"
#include "common/summary_checker.h"
#include "common/topo_checker.h"
#include "lowering/model_converter.h"
#include "faker/fake_value.h"
#include "graph/utils/node_utils.h"
#include "graph/utils/graph_dump_utils.h"
#include "aicpu/kernel/aicpu_bin_handler.h"
#include "aicpu/kernel/aicpu_resource_manager.h"

using namespace ge;
namespace gert {
namespace {
void InitHostCpuUtEnv() {
  char_t opp_path[MMPA_MAX_PATH] = "./";
  mmSetEnv("ASCEND_OPP_PATH", &opp_path[0U], MMPA_MAX_PATH);
  ge::MmpaStub::GetInstance().SetImpl(std::make_shared<HostcpuMockMmpa>());
  ASSERT_EQ(AicpuResourceManager::GetInstance().LoadConstantFoldingLib(), GRAPH_SUCCESS);
}

void MakeMiddleOptionalInput(const ge::NodePtr &node, const ge::GeTensorDesc &input_desc,
                             const std::string &optional_name, const std::string &last_required_name,
                             const std::string &first_required_name = "x") {
  ASSERT_NE(node, nullptr);
  ASSERT_EQ(node->GetOpDesc()->AddInputDesc(optional_name, input_desc), GRAPH_SUCCESS);
  node->GetOpDesc()->MutableAllInputName() = {{first_required_name, 0}, {last_required_name, 1}, {optional_name, 2}};
  node->GetOpDesc()->AppendIrInput(first_required_name, ge::kIrInputRequired);
  node->GetOpDesc()->AppendIrInput(optional_name, ge::kIrInputOptional);
  node->GetOpDesc()->AppendIrInput(last_required_name, ge::kIrInputRequired);
  ASSERT_TRUE(ge::AttrUtils::SetBool(node->GetOpDesc(), "optional_input_placeholder", true));
}

void PopExecuteGraph(const std::vector<bg::DevMemValueHolderPtr> &out_addrs,
                     const std::vector<bg::ValueHolderPtr> &order_holders, ge::ExecuteGraphPtr &execute_graph) {
  auto graph_frame = bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(out_addrs), order_holders);
  ASSERT_NE(graph_frame, nullptr);
  execute_graph = graph_frame->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);
}

void LowerHostCpuKernelNodeWithMiddleOptionalInput(ge::ExecuteGraphPtr &execute_graph) {
  InitHostCpuUtEnv();

  auto graph = ShareGraph::ThirdAicpuOpGraph();
  ASSERT_NE(graph, nullptr);

  auto data1 = graph->FindNode("data1");
  auto data2 = graph->FindNode("data2");
  auto add1 = graph->FindNode("add1");
  auto nonzero = graph->FindNode("nonzero");
  ASSERT_NE(data1, nullptr);
  ASSERT_NE(data2, nullptr);
  ASSERT_NE(add1, nullptr);
  ASSERT_NE(nonzero, nullptr);

  add1->GetOpDesc()->SetOpKernelLibName(ge::kEngineNameAiCpu.c_str());
  nonzero->GetOpDesc()->SetOpKernelLibName(ge::kEngineNameHostCpu.c_str());
  MakeMiddleOptionalInput(nonzero, add1->GetOpDesc()->GetOutputDesc(0), "y", "z", "x1");
  graph->TopologicalSorting();

  AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data = GlobalDataFaker(root_model)
                         .AddTaskDef("Add", aicpu_task_def_faker.SetNeedMemcpy(true))
                         .AddTaskDef("NonZero", aicpu_task_def_faker)
                         .Build();
  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(data1, data_input);
  auto data2_ret = LoweringDataNode(data2, data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  auto add_ret = LoweringAiCpuNode(add1, add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());

  data_input.input_addrs.push_back(add_ret.out_addrs[0]);
  data_input.input_addrs.push_back(data1_ret.out_addrs[0]);
  data_input.input_shapes.push_back(add_ret.out_shapes[0]);
  data_input.input_shapes.push_back(data1_ret.out_shapes[0]);

  auto non_zero_ret = LoweringAiCpuNode(nonzero, data_input);
  ASSERT_TRUE(non_zero_ret.result.IsSuccess());
  PopExecuteGraph(non_zero_ret.out_addrs, non_zero_ret.order_holders, execute_graph);
}

void TestAicpuConvert(std::string node_type, LowerResult &add_ret) {
  auto graph = ShareGraph::BuildSingleNodeGraph();
  auto add_op_desc = graph->FindNode("add1")->GetOpDesc();
  AttrUtils::SetInt(add_op_desc, ge::ATTR_NAME_UNKNOWN_SHAPE_TYPE, 4);
  LoweringGlobalData global_data;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  if (node_type == "tf") {
    add_op_desc->SetOpKernelLibName(ge::kEngineNameAiCpuTf.c_str());
    AttrUtils::SetStr(add_op_desc, "ops_json_path", "tf_kernel.json");
    AiCpuTfTaskDefFaker aicpu_task_def_faker;
    global_data = GlobalDataFaker(root_model).AddTaskDef("Add", aicpu_task_def_faker.SetNeedMemcpy(true)).Build();
  } else if (node_type == "cc") {
    add_op_desc->SetOpKernelLibName(ge::kEngineNameAiCpu.c_str());
    AttrUtils::SetStr(add_op_desc, "ops_json_path", "aicpu_kernel.json");
    AiCpuCCTaskDefFaker aicpu_task_def_faker;
    global_data = GlobalDataFaker(root_model).AddTaskDef("Add", aicpu_task_def_faker.SetNeedMemcpy(true)).Build();
  }

  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(graph->FindNode("data1"), data_input);
  auto data2_ret = LoweringDataNode(graph->FindNode("data2"), data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  add_ret = LoweringAiCpuNode(graph->FindNode("add1"), add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());
  ASSERT_EQ(add_ret.out_addrs.size(), 1);
  ASSERT_EQ(add_ret.out_shapes.size(), 1);
  ASSERT_EQ(add_ret.order_holders.size(), 2);
}

graphStatus StubHostKernel(KernelContext *context) {
  return GRAPH_SUCCESS;
}
}  // namespace

class AicpuNodeConverterUT : public bg::BgTestAutoCreate3StageFrame {};

TEST_F(AicpuNodeConverterUT, ConvertAicpTfNode) {
  LowerResult add_ret;
  TestAicpuConvert("tf", add_ret);
}

TEST_F(AicpuNodeConverterUT, TestAicpTfInitNode) {
  LowerResult add_ret;
  TestAicpuConvert("tf", add_ret);

  auto exe_graph = add_ret.out_addrs[0]->GetFastNode()->GetExtendInfo()->GetOwnerGraphBarePtr();
  ASSERT_NE(exe_graph, nullptr);

  auto execute_graph =
      bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(add_ret.out_addrs), add_ret.order_holders)
          ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);

  auto root_graph = execute_graph->GetParentGraphBarePtr();
  ASSERT_NE(root_graph, nullptr);
  auto init_node = ge::ExecuteGraphUtils::FindFirstNodeMatchType(root_graph, "Init");
  ASSERT_NE(init_node, nullptr);

  auto init_graph = ge::FastNodeUtils::GetSubgraphFromNode(init_node, 0);
  auto rts_args = ge::ExecuteGraphUtils::FindFirstNodeMatchType(init_graph, "BuildTfArgsBinHandle");
  auto build_tf_args = ge::ExecuteGraphUtils::FindFirstNodeMatchType(init_graph, "BuildTfArgs");
  auto create_session = ge::ExecuteGraphUtils::FindFirstNodeMatchType(init_graph, "EnsureCreateTfSession");
  auto build_ext = ge::ExecuteGraphUtils::FindFirstNodeMatchType(init_graph, "BuildExtInfoHandle");
  ASSERT_NE(rts_args, nullptr);
  ASSERT_NE(build_tf_args, nullptr);
  ASSERT_NE(create_session, nullptr);
  ASSERT_NE(build_ext, nullptr);
}

TEST_F(AicpuNodeConverterUT, Convert_TensorListOp) {
  auto graph = ShareGraph::TensorListGraph();
  AiCpuTfTaskDefFaker task_def;
  graph->TopologicalSorting();
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.AddTaskDef("EmptyTensorList", task_def)
                           .AddTaskDef("TensorListPushBack", task_def)
                           .AddTaskDef("TensorListPopBack", task_def)
                           .BuildGeRootModel();

  while (bg::ValueHolder::PopGraphFrame() != nullptr) {
  }
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model);
  ASSERT_NE(exe_graph, nullptr);
  ASSERT_EQ(3, exe_graph->GetDirectNodesSize());
  auto model_executor = ModelV2Executor::Create(exe_graph, ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  EXPECT_EQ(model_executor->Load(), ge::GRAPH_SUCCESS);

  auto outputs = FakeTensors({2}, 1);
  auto inputs = FakeTensors({2048, 2, 3, 4}, 4);
  rtStream_t stream;
  ASSERT_EQ(aclrtCreateStreamWithConfig(&stream, static_cast<uint32_t>(RT_STREAM_PRIORITY_DEFAULT), 0U), RT_ERROR_NONE);
  auto i3 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(), outputs.GetTensorList(),
                                    outputs.size()),
            ge::GRAPH_SUCCESS);
  ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
  aclrtDestroyStream(stream);
}

TEST_F(AicpuNodeConverterUT, ConvertAicpuCCNode) {
  LowerResult add_ret;
  TestAicpuConvert("cc", add_ret);
}

TEST_F(AicpuNodeConverterUT, TestAicpCCInitNode) {
  LowerResult add_ret;
  TestAicpuConvert("cc", add_ret);

  auto exe_graph = add_ret.out_addrs[0]->GetFastNode()->GetExtendInfo()->GetOwnerGraphBarePtr();
  ASSERT_NE(exe_graph, nullptr);

  auto execute_graph =
      bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(add_ret.out_addrs), add_ret.order_holders)
          ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);

  auto root_graph = execute_graph->GetParentGraphBarePtr();
  ASSERT_NE(root_graph, nullptr);
  auto init_node = ge::ExecuteGraphUtils::FindFirstNodeMatchType(root_graph, "Init");
  ASSERT_NE(init_node, nullptr);

  auto init_graph = ge::FastNodeUtils::GetSubgraphFromNode(init_node, 0);
  auto rts_args = ge::ExecuteGraphUtils::FindFirstNodeMatchType(init_graph, "BuildCCArgsBinHandle");
  auto build_cc_args = ge::ExecuteGraphUtils::FindFirstNodeMatchType(init_graph, "BuildCCArgs");
  auto build_ext = ge::ExecuteGraphUtils::FindFirstNodeMatchType(init_graph, "BuildExtInfoHandle");
  ASSERT_NE(rts_args, nullptr);
  ASSERT_NE(build_cc_args, nullptr);
  ASSERT_NE(build_ext, nullptr);
}

TEST_F(AicpuNodeConverterUT, ConvertAicpuCCNodeWithMiddleOptionalInputInsertExpandNode) {
  auto graph = ShareGraph::BuildSingleNodeGraph();
  ASSERT_NE(graph, nullptr);

  auto data1 = graph->FindNode("data1");
  auto data2 = graph->FindNode("data2");
  auto add1 = graph->FindNode("add1");
  auto netoutput = graph->FindNode("NetOutput");
  ASSERT_NE(data1, nullptr);
  ASSERT_NE(data2, nullptr);
  ASSERT_NE(add1, nullptr);
  ASSERT_NE(netoutput, nullptr);

  add1->GetOpDesc()->SetOpKernelLibName(ge::kEngineNameAiCpu.c_str());
  AttrUtils::SetInt(add1->GetOpDesc(), ge::ATTR_NAME_UNKNOWN_SHAPE_TYPE, 4);
  AttrUtils::SetStr(add1->GetOpDesc(), "ops_json_path", "aicpu_kernel.json");
  ASSERT_EQ(add1->GetOpDesc()->AddInputDesc("y", data1->GetOpDesc()->GetOutputDesc(0)), GRAPH_SUCCESS);
  add1->GetOpDesc()->MutableAllInputName() = {{"x", 0}, {"z", 1}, {"y", 2}};
  add1->GetOpDesc()->AppendIrInput("x", ge::kIrInputRequired);
  add1->GetOpDesc()->AppendIrInput("y", ge::kIrInputOptional);
  add1->GetOpDesc()->AppendIrInput("z", ge::kIrInputRequired);
  ASSERT_TRUE(ge::AttrUtils::SetBool(add1->GetOpDesc(), "optional_input_placeholder", true));
  bool optional_placeholder = false;
  ASSERT_TRUE(ge::AttrUtils::GetBool(add1->GetOpDesc(), "optional_input_placeholder", optional_placeholder));
  ASSERT_TRUE(optional_placeholder);
  graph->TopologicalSorting();

  AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data = GlobalDataFaker(root_model).AddTaskDef("Add", aicpu_task_def_faker.SetNeedMemcpy(true)).Build();
  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(data1, data_input);
  auto data2_ret = LoweringDataNode(data2, data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  auto add_ret = LoweringAiCpuNode(add1, add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());

  auto execute_graph =
      bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(add_ret.out_addrs), add_ret.order_holders)
          ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);
  auto expand_node =
      ge::ExecuteGraphUtils::FindFirstNodeMatchType(execute_graph.get(), "ExpandAicpuOptionalInputAddrs");
  auto update_node = ge::ExecuteGraphUtils::FindFirstNodeMatchType(execute_graph.get(), "UpdateAicpuIoAddr");
  ASSERT_NE(expand_node, nullptr);
  ASSERT_NE(update_node, nullptr);
}

TEST_F(AicpuNodeConverterUT, ConvertAicpuCCNodeWithWorkSpaceInfo) {
  const std::string so_name = "libcust_add.so";
  vector<char> buffer(1, 'a');
  ge::OpKernelBinPtr bin = std::make_shared<ge::OpKernelBin>("op", std::move(buffer));
  rtBinHandle handle = nullptr;
  CustBinHandlerManager::Instance().LoadAndGetBinHandle(so_name, bin, handle);

  auto graph = ShareGraph::BuildSingleNodeGraph();
  auto add_op_desc = graph->FindNode("add1")->GetOpDesc();
  AttrUtils::SetInt(add_op_desc, ge::ATTR_NAME_UNKNOWN_SHAPE_TYPE, 4);
  add_op_desc->SetOpKernelLibName(ge::kEngineNameAiCpu.c_str());
  ge::AttrUtils::SetInt(add_op_desc, "workspaceSize", 1024);
  std::vector<uint32_t> mem_type;
  mem_type.push_back(ge::AicpuWorkSpaceType::CUST_LOG);
  ge::AttrUtils::SetListInt(add_op_desc, ge::TVM_ATTR_NAME_WORKSPACE_TYPE, mem_type);
  ge::AttrUtils::SetBool(add_op_desc, "_cust_aicpu_flag", true);
  ge::AttrUtils::SetStr(add_op_desc, "kernelSo", "libcust_add.so");
  AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data = GlobalDataFaker(root_model).AddTaskDef("Add", aicpu_task_def_faker.SetNeedMemcpy(true)).Build();
  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(graph->FindNode("data1"), data_input);
  auto data2_ret = LoweringDataNode(graph->FindNode("data2"), data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  auto add_ret = LoweringAiCpuNode(graph->FindNode("add1"), add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());
  ASSERT_EQ(add_ret.out_addrs.size(), 1);
  ASSERT_EQ(add_ret.out_shapes.size(), 1);
  ASSERT_EQ(add_ret.order_holders.size(), 2);

  auto exe_graph = add_ret.out_addrs[0]->GetFastNode()->GetExtendInfo()->GetOwnerGraphBarePtr();
  ASSERT_NE(exe_graph, nullptr);
  auto execute_graph =
      bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(add_ret.out_addrs), add_ret.order_holders)
          ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);
  DumpGraph(execute_graph.get(), "GeneralCCAiCpuExe");
}

TEST_F(AicpuNodeConverterUT, ConvertHostcpuCCNodeWithSmallShape) {
  char_t opp_path[MMPA_MAX_PATH] = "./";
  mmSetEnv("ASCEND_OPP_PATH", &opp_path[0U], MMPA_MAX_PATH);
  ge::MmpaStub::GetInstance().SetImpl(std::make_shared<HostcpuMockMmpa>());

  auto graph = ShareGraph::BuildSingleNodeGraph();
  graph->FindNode("add1")->GetOpDesc()->SetOpKernelLibName(ge::kEngineNameHostCpu.c_str());
  AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data = GlobalDataFaker(root_model).AddTaskDef("Add", aicpu_task_def_faker).Build();
  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(graph->FindNode("data1"), data_input);
  auto data2_ret = LoweringDataNode(graph->FindNode("data2"), data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  auto add_ret = LoweringAiCpuNode(graph->FindNode("add1"), add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());
  ASSERT_EQ(add_ret.out_addrs.size(), 1);
  ASSERT_EQ(add_ret.out_shapes.size(), 1);
  ASSERT_EQ(add_ret.order_holders.size(), 0);

  REGISTER_KERNEL(AddHostKernel).RunFunc(StubHostKernel);
  add_ret = LoweringAiCpuNode(graph->FindNode("add1"), add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());
  ASSERT_EQ(add_ret.out_addrs.size(), 1);
  ASSERT_EQ(add_ret.out_shapes.size(), 1);
  ASSERT_EQ(add_ret.order_holders.size(), 0);

  auto exe_graph = add_ret.out_addrs[0]->GetFastNode()->GetExtendInfo()->GetOwnerGraphBarePtr();
  ASSERT_NE(exe_graph, nullptr);
  auto execute_graph =
      bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(add_ret.out_addrs), add_ret.order_holders)
          ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);
  DumpGraph(execute_graph.get(), "GeneralHostAiCpuExe");
  ge::MmpaStub::GetInstance().Reset();
}

TEST_F(AicpuNodeConverterUT, ConvertHostcpuRefNodeWithSmallShape) {
  char_t opp_path[MMPA_MAX_PATH] = "./";
  mmSetEnv("ASCEND_OPP_PATH", &opp_path[0U], MMPA_MAX_PATH);
  ge::MmpaStub::GetInstance().SetImpl(std::make_shared<HostcpuMockMmpa>());

  auto graph = ShareGraph::BuildSliceWriteNormalGraph();

  auto slice_write_op_desc = graph->FindNode("slice_write")->GetOpDesc();
  ge::AttrUtils::SetBool(slice_write_op_desc, "reference", true);
  slice_write_op_desc->SetOpKernelLibName(ge::kEngineNameHostCpu.c_str());
  AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data =
      GlobalDataFaker(root_model).AddTaskDef("SliceWrite", aicpu_task_def_faker.SetNeedMemcpy(true)).Build();
  bg::LowerConstDataNode(global_data);

  LowerInput data_input = {{}, {}, &global_data};
  auto data1_ret = LoweringDataNode(graph->FindNode("x"), data_input);
  auto data2_ret = LoweringDataNode(graph->FindNode("begin"), data_input);
  auto data3_ret = LoweringDataNode(graph->FindNode("value"), data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());
  ASSERT_TRUE(data3_ret.result.IsSuccess());

  LowerInput slice_write_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0], data3_ret.out_shapes[0]},
                                  {data1_ret.out_addrs[0], data2_ret.out_addrs[0], data3_ret.out_addrs[0]},
                                  &global_data};
  auto slice_write_ret = LoweringAiCpuNode(graph->FindNode("slice_write"), slice_write_input);
  ASSERT_TRUE(slice_write_ret.result.IsSuccess());

  auto exe_graph = slice_write_ret.out_addrs[0]->GetFastNode()->GetExtendInfo()->GetOwnerGraphBarePtr();
  ASSERT_NE(exe_graph, nullptr);
  auto execute_graph =
      bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(slice_write_ret.out_addrs),
                                     slice_write_ret.order_holders)
          ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);
  DumpGraph(execute_graph.get(), "GeneralHostCupRefExe");
  ge::MmpaStub::GetInstance().Reset();
}

TEST_F(AicpuNodeConverterUT, ConvertAicpuCC3rdNode) {
  auto graph = ShareGraph::BuildSingleNodeGraph();
  auto add_op_desc = graph->FindNode("add1")->GetOpDesc();
  AttrUtils::SetInt(add_op_desc, ge::ATTR_NAME_UNKNOWN_SHAPE_TYPE, 3);
  add_op_desc->SetOpKernelLibName(ge::kEngineNameAiCpu.c_str());
  AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data = GlobalDataFaker(root_model).AddTaskDef("Add", aicpu_task_def_faker.SetNeedMemcpy(true)).Build();
  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(graph->FindNode("data1"), data_input);
  auto data2_ret = LoweringDataNode(graph->FindNode("data2"), data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  auto add_ret = LoweringAiCpuNode(graph->FindNode("add1"), add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());
  ASSERT_EQ(add_ret.out_addrs.size(), 1);
  ASSERT_EQ(add_ret.out_shapes.size(), 1);
  ASSERT_EQ(add_ret.order_holders.size(), 2);

  auto exe_graph = add_ret.out_addrs[0]->GetFastNode()->GetExtendInfo()->GetOwnerGraphBarePtr();
  ASSERT_NE(exe_graph, nullptr);
  auto execute_graph =
      bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(add_ret.out_addrs), add_ret.order_holders)
          ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);
  DumpGraph(execute_graph.get(), "GeneralCC3rdAiCpuExe");
}

TEST_F(AicpuNodeConverterUT, BlockAiCpuTf_ExecuteSuccess) {
  auto graph = ShareGraph::BuildBlockGraph();
  AiCpuTfTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data = GlobalDataFaker(root_model).AddTaskDef("Add", aicpu_task_def_faker.SetNeedMemcpy(true)).Build();
  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(graph->FindNode("data1"), data_input);
  auto data2_ret = LoweringDataNode(graph->FindNode("data2"), data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  auto add_ret = LoweringAiCpuNode(graph->FindNode("add1"), add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());
  ASSERT_EQ(add_ret.out_addrs.size(), 1);
  ASSERT_EQ(add_ret.out_shapes.size(), 1);
  ASSERT_EQ(add_ret.order_holders.size(), 2);

  auto exe_graph = add_ret.out_addrs[0]->GetFastNode()->GetExtendInfo()->GetOwnerGraphBarePtr();
  ASSERT_NE(exe_graph, nullptr);
  auto execute_graph =
      bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(add_ret.out_addrs), add_ret.order_holders)
          ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);
  DumpGraph(execute_graph.get(), "GeneralBlockAiCpuExe");
}

TEST_F(AicpuNodeConverterUT, ConvertAicpuHostCpu3rdNode) {
  char_t opp_path[MMPA_MAX_PATH] = "./";
  mmSetEnv("ASCEND_OPP_PATH", &opp_path[0U], MMPA_MAX_PATH);
  ge::MmpaStub::GetInstance().SetImpl(std::make_shared<HostcpuMockMmpa>());

  auto graph = ShareGraph::ThirdAicpuOpGraph();

  auto add_op_desc = graph->FindNode("add1")->GetOpDesc();
  add_op_desc->SetOpKernelLibName(ge::kEngineNameAiCpu.c_str());
  AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data = GlobalDataFaker(root_model)
                         .AddTaskDef("Add", aicpu_task_def_faker.SetNeedMemcpy(true))
                         .AddTaskDef("NonZero", aicpu_task_def_faker)
                         .Build();
  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(graph->FindNode("data1"), data_input);
  auto data2_ret = LoweringDataNode(graph->FindNode("data2"), data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  auto add_ret = LoweringAiCpuNode(graph->FindNode("add1"), add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());
  data_input.input_addrs.push_back(add_ret.out_addrs[0]);
  data_input.input_addrs.push_back(data1_ret.out_addrs[0]);
  data_input.input_shapes.push_back(add_ret.out_shapes[0]);
  data_input.input_shapes.push_back(data1_ret.out_shapes[0]);

  graph->FindNode("nonzero")->GetOpDesc()->SetOpKernelLibName(ge::kEngineNameHostCpu.c_str());
  auto non_zero_ret = LoweringAiCpuNode(graph->FindNode("nonzero"), data_input);
  ASSERT_TRUE(non_zero_ret.result.IsSuccess());
  auto exe_graph = non_zero_ret.out_addrs[0]->GetFastNode()->GetExtendInfo()->GetOwnerGraphBarePtr();
  ASSERT_NE(exe_graph, nullptr);
  auto execute_graph = bg::ValueHolder::PopGraphFrame(ConvertDevMemValueHoldersToValueHolders(non_zero_ret.out_addrs),
                                                      non_zero_ret.order_holders)
                           ->GetExecuteGraph();
  ASSERT_NE(execute_graph, nullptr);
  ge::MmpaStub::GetInstance().Reset();
}

TEST_F(AicpuNodeConverterUT, ConvertHostCpuNodeWithMiddleOptionalInputExpandShapesForHostExecFunc) {
  InitHostCpuUtEnv();

  auto graph = ShareGraph::BuildSingleNodeGraph();
  ASSERT_NE(graph, nullptr);

  auto data1 = graph->FindNode("data1");
  auto data2 = graph->FindNode("data2");
  auto add1 = graph->FindNode("add1");
  ASSERT_NE(data1, nullptr);
  ASSERT_NE(data2, nullptr);
  ASSERT_NE(add1, nullptr);

  add1->GetOpDesc()->SetOpKernelLibName(ge::kEngineNameHostCpu.c_str());
  MakeMiddleOptionalInput(add1, data1->GetOpDesc()->GetOutputDesc(0), "y", "z");
  bool optional_placeholder = false;
  ASSERT_TRUE(ge::AttrUtils::GetBool(add1->GetOpDesc(), "optional_input_placeholder", optional_placeholder));
  ASSERT_TRUE(optional_placeholder);
  graph->TopologicalSorting();

  AiCpuCCTaskDefFaker aicpu_task_def_faker;
  auto root_model = GeModelBuilder(graph).BuildGeRootModel();
  auto global_data = GlobalDataFaker(root_model).AddTaskDef("Add", aicpu_task_def_faker).Build();
  bg::LowerConstDataNode(global_data);
  LowerInput data_input = {{}, {}, &global_data};

  auto data1_ret = LoweringDataNode(data1, data_input);
  auto data2_ret = LoweringDataNode(data2, data_input);
  ASSERT_TRUE(data1_ret.result.IsSuccess());
  ASSERT_TRUE(data2_ret.result.IsSuccess());

  LowerInput add_input = {{data1_ret.out_shapes[0], data2_ret.out_shapes[0]},
                          {data1_ret.out_addrs[0], data2_ret.out_addrs[0]},
                          &global_data};
  REGISTER_KERNEL(AddHostKernel).RunFunc(StubHostKernel);
  auto add_ret = LoweringAiCpuNode(add1, add_input);
  ASSERT_TRUE(add_ret.result.IsSuccess());

  ge::ExecuteGraphPtr execute_graph = nullptr;
  PopExecuteGraph(add_ret.out_addrs, add_ret.order_holders, execute_graph);
  auto host_exec_node = ge::ExecuteGraphUtils::FindFirstNodeMatchType(execute_graph.get(), "AicpuHostExecFunc");
  ASSERT_NE(host_exec_node, nullptr);
  ASSERT_NE(host_exec_node->GetOpDescPtr(), nullptr);
  ASSERT_EQ(host_exec_node->GetOpDescPtr()->GetAllInputsSize(), 7U);
  ge::MmpaStub::GetInstance().Reset();
}

TEST_F(AicpuNodeConverterUT, ConvertHostCpuKernelNodeWithMiddleOptionalInputKeepCallbackFedShapes) {
  ge::ExecuteGraphPtr execute_graph = nullptr;
  LowerHostCpuKernelNodeWithMiddleOptionalInput(execute_graph);
  auto host_compute_node = ge::ExecuteGraphUtils::FindFirstNodeMatchType(execute_graph.get(), "AicpuHostCompute");
  auto update_ext_shape = ge::ExecuteGraphUtils::FindFirstNodeMatchType(execute_graph.get(), "UpdateExtShape");
  auto get_ext_output_shapes = ge::ExecuteGraphUtils::FindFirstNodeMatchType(execute_graph.get(), "GetExtOutputShapes");
  ASSERT_NE(host_compute_node, nullptr);
  ASSERT_NE(update_ext_shape, nullptr);
  ASSERT_NE(get_ext_output_shapes, nullptr);
  ASSERT_NE(update_ext_shape->GetOpDescPtr(), nullptr);
  ASSERT_EQ(update_ext_shape->GetOpDescPtr()->GetAllInputsSize(), 7U);
  ASSERT_NE(get_ext_output_shapes->GetOpDescPtr(), nullptr);
  ASSERT_EQ(get_ext_output_shapes->GetOpDescPtr()->GetOutputsSize(), 2U);
  ge::MmpaStub::GetInstance().Reset();
}
}  // namespace gert
