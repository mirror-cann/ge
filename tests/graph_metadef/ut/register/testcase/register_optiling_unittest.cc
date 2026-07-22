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
#include <iostream>
#include "register/op_tiling_registry.h"
#include "op_tiling/op_tiling.cc"
#include "common/sgt_slice_type.h"
#include "graph_builder_utils.h"
using namespace std;
using namespace ge;
using namespace ffts;
namespace optiling {
using ByteBuffer = std::stringstream;
class RegisterOpTilingUT : public testing::Test {
 protected:
  void SetUp() {}

  void TearDown() {}
};

TEST_F(RegisterOpTilingUT, byte_buffer_test) {
  EXPECT_NO_THROW(ByteBuffer stream; char *dest = nullptr; size_t size = ByteBufferGetAll(stream, dest, 2);
                  cout << size << endl;);
}

TEST_F(RegisterOpTilingUT, op_run_info_test) {
  std::shared_ptr<utils::OpRunInfo> run_info = make_shared<utils::OpRunInfo>(8, true, 64);
  int64_t work_space;
  graphStatus ret = run_info->GetWorkspace(0, work_space);
  EXPECT_EQ(ret, GRAPH_FAILED);
  vector<int64_t> work_space_vec = {10, 20, 30, 40};
  run_info->SetWorkspaces(work_space_vec);
  ret = run_info->GetWorkspace(1, work_space);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_EQ(work_space, 20);
  EXPECT_EQ(run_info->GetWorkspaceNum(), 4);
  string str = "test";
  run_info->AddTilingData(str);

  std::shared_ptr<utils::OpRunInfo> run_info_2 = make_shared<utils::OpRunInfo>(*run_info);
  ret = run_info_2->GetWorkspace(2, work_space);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_EQ(work_space, 30);

  utils::OpRunInfo run_info_3 = *run_info;
  ret = run_info_3.GetWorkspace(3, work_space);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_EQ(work_space, 40);

  utils::OpRunInfo &run_info_4 = *run_info;
  ret = run_info_4.GetWorkspace(0, work_space);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_EQ(work_space, 10);
}

TEST_F(RegisterOpTilingUT, op_compile_info_test) {
  std::shared_ptr<utils::OpCompileInfo> compile_info = make_shared<utils::OpCompileInfo>();
  string str_key = "key";
  string str_value = "value";
  AscendString key(str_key.c_str());
  AscendString value(str_value.c_str());
  compile_info->SetKey(key);
  compile_info->SetValue(value);

  std::shared_ptr<utils::OpCompileInfo> compile_info_2 = make_shared<utils::OpCompileInfo>(key, value);
  EXPECT_EQ(compile_info_2->GetKey() == key, true);
  EXPECT_EQ(compile_info_2->GetValue() == value, true);

  std::shared_ptr<utils::OpCompileInfo> compile_info_3 = make_shared<utils::OpCompileInfo>(str_key, str_value);
  EXPECT_EQ(compile_info_3->GetKey() == key, true);
  EXPECT_EQ(compile_info_3->GetValue() == value, true);

  std::shared_ptr<utils::OpCompileInfo> compile_info_4 = make_shared<utils::OpCompileInfo>(*compile_info);
  EXPECT_EQ(compile_info_4->GetKey() == key, true);
  EXPECT_EQ(compile_info_4->GetValue() == value, true);

  utils::OpCompileInfo compile_info_5 = *compile_info;
  EXPECT_EQ(compile_info_5.GetKey() == key, true);
  EXPECT_EQ(compile_info_5.GetValue() == value, true);

  utils::OpCompileInfo &compile_info_6 = *compile_info;
  EXPECT_EQ(compile_info_6.GetKey() == key, true);
  EXPECT_EQ(compile_info_6.GetValue() == value, true);
}

TEST_F(RegisterOpTilingUT, te_op_paras_test) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  GeShape shape({1, 4, 1, 1});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddInputDesc("y", tensor_desc);
  op_desc->AddOutputDesc("z", tensor_desc);
  int32_t attr_value = 1024;
  AttrUtils::SetInt(op_desc, "some_int_attr", attr_value);
  vector<int64_t> attr_vec = {11, 22, 33, 44};
  AttrUtils::SetListInt(op_desc, "some_int_vec", attr_vec);
  TeOpParas op_param;
  op_param.op_type = op_desc->GetType();
  VarAttrHelper::InitTeOpVarAttr(op_desc, op_param.var_attrs);
  size_t size = 0;
  EXPECT_NO_THROW(op_param.var_attrs.GetData("some_int_attr", "xxx", size);
                  op_param.var_attrs.GetData("some_int_attr", "Int32", size);
                  op_param.var_attrs.GetData("some_int_vec", "ListInt32", size););
}

bool op_tiling_stub(const Operator &op, const utils::OpCompileInfo &compile_info, utils::OpRunInfo &run_info) {
  return true;
}

static bool op_tiling_stub_v1(const TeOpParas &op_paras, const OpCompileInfo &compile_info, OpRunInfo &run_info) {
  return true;
}

REGISTER_OP_TILING_V2(ReluV2, op_tiling_stub);

TEST_F(RegisterOpTilingUT, OpFftsPlusCalculate_1) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  const auto &op_desc = node->GetOpDesc();
  const Operator op = OpDescUtils::CreateOperatorFromNode(node);

  ThreadSliceMapDyPtr slice_info_ptr = std::make_shared<ThreadSliceMapDy>();
  vector<int64_t> vec_1;
  vec_1.push_back(1);
  vector<vector<int64_t>> vec_2;
  vec_2.push_back(vec_1);
  vec_2.push_back(vec_1);
  slice_info_ptr->parallel_window_size = 2;
  slice_info_ptr->slice_instance_num = 2;
  slice_info_ptr->input_tensor_slice.push_back(vec_2);
  slice_info_ptr->input_tensor_slice.push_back(vec_2);
  slice_info_ptr->output_tensor_slice.push_back(vec_2);
  slice_info_ptr->output_tensor_slice.push_back(vec_2);

  (void)op_desc->SetExtAttr(ffts::kAttrSgtStructInfoDy, slice_info_ptr);
  GeShape shape({4, 1, 3, 4, 16});
  GeTensorDesc tensor_desc(shape, ge::FORMAT_NCHW, ge::DT_FLOAT);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  std::vector<OpRunInfoV2> op_run_info;
  EXPECT_EQ(OpFftsPlusCalculate(op, op_run_info), ge::GRAPH_FAILED);

  string compile_info_key = "compile_info_key";
  string compile_info_json = "compile_info_json";
  (void)ge::AttrUtils::SetStr(op_desc, COMPILE_INFO_KEY, compile_info_key);
  (void)ge::AttrUtils::SetStr(op_desc, COMPILE_INFO_JSON, compile_info_json);
  auto dstAnchor = node->GetInDataAnchor(0);
  ge::AnchorUtils::SetStatus(dstAnchor, ge::ANCHOR_DATA);
  EXPECT_EQ(OpFftsPlusCalculate(op, op_run_info), ge::GRAPH_SUCCESS);
}

// slice instance over
TEST_F(RegisterOpTilingUT, OpFftsPlusCalculate_2) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  const auto &op_desc = node->GetOpDesc();
  const Operator op = OpDescUtils::CreateOperatorFromNode(node);

  ThreadSliceMapDyPtr slice_info_ptr = std::make_shared<ThreadSliceMapDy>();
  vector<int64_t> vec_1;
  vec_1.push_back(1);
  vector<vector<int64_t>> vec_2;
  vec_2.push_back(vec_1);
  vec_2.push_back(vec_1);
  slice_info_ptr->parallel_window_size = 2;
  slice_info_ptr->slice_instance_num = 4;
  slice_info_ptr->input_tensor_slice.push_back(vec_2);
  slice_info_ptr->input_tensor_slice.push_back(vec_2);
  slice_info_ptr->output_tensor_slice.push_back(vec_2);
  slice_info_ptr->output_tensor_slice.push_back(vec_2);
  slice_info_ptr->input_tensor_indexes.push_back(0);
  slice_info_ptr->output_tensor_indexes.push_back(0);
  (void)op_desc->SetExtAttr(ffts::kAttrSgtStructInfoDy, slice_info_ptr);
  GeShape shape({4, 1, 3, 4, 16});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  string compile_info_key = "compile_info_key";
  string compile_info_json = "compile_info_json";
  (void)ge::AttrUtils::SetStr(op_desc, COMPILE_INFO_KEY, compile_info_key);
  (void)ge::AttrUtils::SetStr(op_desc, COMPILE_INFO_JSON, compile_info_json);
  std::vector<OpRunInfoV2> op_run_info;
  EXPECT_EQ(OpFftsPlusCalculate(op, op_run_info), ge::GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, PostProcCalculateV2_SUCCESS) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  Operator op = OpDescUtils::CreateOperatorFromNode(node);
  OpDescPtr op_desc = node->GetOpDesc();
  (void)ge::AttrUtils::SetStr(op_desc, "_alias_engine_name", "TEST");
  std::vector<int64_t> workspaces = {1, 2, 3};
  OpRunInfoV2 run_info;
  run_info.SetWorkspaces(workspaces);
  workspaces.emplace_back(5);
  op_desc->SetWorkspaceBytes(workspaces);
  ge::graphStatus ret = PostProcCalculateV2(op, run_info);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

TEST_F(RegisterOpTilingUT, PostProcMemoryCheck1) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 2, 1);
  GeShape shape({3, 4, 2, 1});
  GeTensorDesc tensor_desc(shape);
  OpDescPtr op_desc = node->GetOpDesc();
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddInputDesc("y", tensor_desc);
  op_desc->AddOutputDesc("z", tensor_desc);
  Operator op = OpDescUtils::CreateOperatorFromNode(node);
  std::vector<int64_t> workspaces = {1, 2, 3};
  OpRunInfoV2 run_info;
  run_info.SetWorkspaces(workspaces);
  (void)ge::AttrUtils::SetBool(op_desc, kMemoryCheck, false);
  (void)PostProcMemoryCheck(op, run_info);
  ByteBuffer &data = run_info.GetAllTilingData();
  cout << "TEST" << data.str() << endl;
  EXPECT_EQ(data.str().empty(), true);
  (void)ge::AttrUtils::SetBool(op_desc, kMemoryCheck, true);
  (void)ge::AttrUtils::SetInt(op_desc, kOriOpParaSize, 64);
  (void)PostProcMemoryCheck(op, run_info);
  ByteBuffer &data1 = run_info.GetAllTilingData();
  cout << "TEST1" << data1.str().c_str() << endl;
  EXPECT_EQ(data1.str().empty(), true);
  run_info.ResetAddrBase(nullptr, 1024);
  (void)PostProcMemoryCheck(op, run_info);
  ByteBuffer &data2 = run_info.GetAllTilingData();
  cout << "TEST2" << data2.str().c_str() << endl;
  EXPECT_EQ(data2.str().empty(), false);
}

TEST_F(RegisterOpTilingUT, UpDateNodeShapeBySliceInfo1) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  OpDescPtr op_desc = node->GetOpDesc();
  ThreadSliceMapDyPtr slice_info_ptr;
  slice_info_ptr = std::make_shared<ThreadSliceMapDy>();
  vector<int64_t> vec_1;
  vec_1.push_back(1);
  vector<vector<int64_t>> vec_2;
  vector<vector<int64_t>> vec_3;
  vec_2.push_back(vec_1);
  vec_2.push_back(vec_1);
  vec_3.push_back(vec_1);
  slice_info_ptr->parallel_window_size = 2;
  slice_info_ptr->slice_instance_num = 2;
  slice_info_ptr->input_tensor_slice.push_back(vec_2);
  slice_info_ptr->input_tensor_slice.push_back(vec_2);
  slice_info_ptr->output_tensor_slice.push_back(vec_3);
  slice_info_ptr->input_tensor_indexes.push_back(0);
  slice_info_ptr->output_tensor_indexes.push_back(0);
  (void)node->GetOpDesc()->SetExtAttr(ffts::kAttrSgtStructInfo, slice_info_ptr);
  GeShape shape({4, 1, 3, 4, 16});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  vector<int64_t> ori_shape;
  bool same_shape = false;
  auto ret = UpDateNodeShapeBySliceInfo(slice_info_ptr, op_desc, 2, ori_shape, same_shape);
  EXPECT_EQ(ret, ge::GRAPH_FAILED);
  op_desc->AddOutputDesc("y", tensor_desc);
  ret = UpDateNodeShapeBySliceInfo(slice_info_ptr, op_desc, 0, ori_shape, same_shape);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

TEST_F(RegisterOpTilingUT, UpDateNodeShapeBySliceInfo2) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  OpDescPtr op_desc = node->GetOpDesc();
  ThreadSliceMapDyPtr slice_info_ptr;
  slice_info_ptr = std::make_shared<ThreadSliceMapDy>();
  vector<int64_t> vec_1;
  vec_1.push_back(1);
  vector<vector<int64_t>> vec_2;
  vec_2.push_back(vec_1);
  vec_2.push_back(vec_1);
  slice_info_ptr->parallel_window_size = 2;
  slice_info_ptr->slice_instance_num = 2;
  slice_info_ptr->input_tensor_slice.push_back(vec_2);
  slice_info_ptr->input_tensor_slice.push_back(vec_2);
  slice_info_ptr->output_tensor_slice.push_back(vec_2);
  slice_info_ptr->output_tensor_slice.push_back(vec_2);
  slice_info_ptr->input_tensor_indexes.push_back(0);
  slice_info_ptr->input_tensor_indexes.push_back(1);
  slice_info_ptr->input_tensor_indexes.push_back(2);
  slice_info_ptr->output_tensor_indexes.push_back(0);
  slice_info_ptr->output_tensor_indexes.push_back(2);
  GeShape shape({4, 1, 3, 4, 16});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  vector<int64_t> ori_shape;
  bool same_shape = false;
  auto ret = UpDateNodeShapeBySliceInfo(slice_info_ptr, op_desc, 0, ori_shape, same_shape);
  EXPECT_EQ(ret, ge::PARAM_INVALID);
  slice_info_ptr->input_tensor_indexes.push_back(0);
  slice_info_ptr->input_tensor_indexes.push_back(1);
  slice_info_ptr->input_tensor_indexes.push_back(2);
  slice_info_ptr->output_tensor_indexes.push_back(0);
  slice_info_ptr->output_tensor_indexes.push_back(2);
  ret = UpDateNodeShapeBySliceInfo(slice_info_ptr, op_desc, 1, ori_shape, same_shape);
  EXPECT_EQ(ret, ge::PARAM_INVALID);
  ret = UpDateNodeShapeBack(op_desc, slice_info_ptr, ori_shape);
  EXPECT_EQ(ret, ge::GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, op_run_info_test_new_tiling_interface1) {
  utils::OpRunInfo run_info;
  uint64_t max_size = 0;
  void *base = run_info.GetAddrBase(max_size);
  run_info.SetAddrBaseOffset(10);
  EXPECT_TRUE(base == NULL);
}

TEST_F(RegisterOpTilingUT, op_run_info_test_new_tiling_interface2) {
  EXPECT_NO_THROW(utils::OpRunInfo run_info; int v1 = 1; int64_t v2 = 2; run_info << v1; run_info << v2;);
}

TEST_F(RegisterOpTilingUT, op_run_info_test_local_memory_size) {
  utils::OpRunInfo run_info;
  uint32_t local_memory_size = run_info.GetLocalMemorySize();
  EXPECT_EQ(local_memory_size, 0U);  // default value

  const uint32_t test_val = 100U;
  run_info.SetLocalMemorySize(test_val);
  local_memory_size = run_info.GetLocalMemorySize();
  EXPECT_EQ(local_memory_size, test_val);  // set value

  utils::OpRunInfo run_info2 = run_info;  // copy constructor
  local_memory_size = run_info2.GetLocalMemorySize();
  EXPECT_EQ(local_memory_size, test_val);

  utils::OpRunInfo run_info3(1, 2, 3);
  local_memory_size = run_info3.GetLocalMemorySize();
  EXPECT_EQ(local_memory_size, 0U);  // default value
}

TEST_F(RegisterOpTilingUT, TeOpVarAttrArgs_GetData_WrongDtype) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape({1, 4, 1, 1});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  int32_t attr_value = 1024;
  AttrUtils::SetInt(op_desc, "some_int_attr", attr_value);
  TeOpParas op_param;
  op_param.op_type = op_desc->GetType();
  VarAttrHelper::InitTeOpVarAttr(op_desc, op_param.var_attrs);
  size_t size = 0;
  EXPECT_NO_THROW(op_param.var_attrs.GetData("some_int_attr", "WrongDtype", size););
  EXPECT_EQ(size, 0U);
}

TEST_F(RegisterOpTilingUT, TeOpVarAttrArgs_GetData_FloatAttr) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  float attr_value = 3.14F;
  AttrUtils::SetFloat(op_desc, "some_float_attr", attr_value);
  TeOpParas op_param;
  VarAttrHelper::InitTeOpVarAttr(op_desc, op_param.var_attrs);
  size_t size = 0;
  EXPECT_NO_THROW(op_param.var_attrs.GetData("some_float_attr", "Float", size););
  EXPECT_EQ(size, sizeof(float));
}

TEST_F(RegisterOpTilingUT, TeOpVarAttrArgs_GetData_ListFloatAttr) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  vector<float> attr_vec = {1.1F, 2.2F, 3.3F};
  AttrUtils::SetListFloat(op_desc, "some_float_vec", attr_vec);
  TeOpParas op_param;
  VarAttrHelper::InitTeOpVarAttr(op_desc, op_param.var_attrs);
  size_t size = 0;
  EXPECT_NO_THROW(op_param.var_attrs.GetData("some_float_vec", "ListFloat", size););
  EXPECT_EQ(size, sizeof(float) * attr_vec.size());
}

TEST_F(RegisterOpTilingUT, TeOpVarAttrArgs_GetData_NotFoundAttr) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  TeOpParas op_param;
  VarAttrHelper::InitTeOpVarAttr(op_desc, op_param.var_attrs);
  size_t size = 0;
  EXPECT_NO_THROW(op_param.var_attrs.GetData("nonexistent_attr", "Int32", size););
  EXPECT_EQ(size, 0U);
}

TEST_F(RegisterOpTilingUT, TeOpVarAttrArgs_GetData_AllIntTypes) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  int64_t attr_value = 100;
  AttrUtils::SetInt(op_desc, "int64_attr", attr_value);
  vector<int64_t> attr_vec = {10, 20};
  AttrUtils::SetListInt(op_desc, "list_int64_attr", attr_vec);
  TeOpParas op_param;
  VarAttrHelper::InitTeOpVarAttr(op_desc, op_param.var_attrs);
  const vector<string> int_types = {"Int8", "Int16", "Int32", "Int64", "UInt8", "UInt16", "UInt32", "UInt64"};
  for (const auto &dtype : int_types) {
    size_t size = 0;
    EXPECT_NO_THROW(op_param.var_attrs.GetData("int64_attr", dtype, size););
  }
  const vector<string> list_types = {"ListInt8",  "ListInt16",  "ListInt32",  "ListInt64",
                                     "ListUInt8", "ListUInt16", "ListUInt32", "ListUInt64"};
  for (const auto &dtype : list_types) {
    size_t size = 0;
    EXPECT_NO_THROW(op_param.var_attrs.GetData("list_int64_attr", dtype, size););
  }
}

TEST_F(RegisterOpTilingUT, FeedTeOpTensorArg_InvalidDtype) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape, FORMAT_NCHW, static_cast<ge::DataType>(999));
  op_desc->AddInputDesc("x", tensor_desc);
  ge::OpDesc::Vistor<ge::GeTensorDescPtr> inputs = op_desc->GetAllInputsDescPtr();
  std::vector<TeOpTensorArg> tensor_arg;
  EXPECT_EQ(FeedTeOpTensorArg(inputs, tensor_arg, op_desc), false);
}

TEST_F(RegisterOpTilingUT, FeedTeOpTensorArg_EmptyShape) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape;
  GeTensorDesc tensor_desc(shape, FORMAT_NCHW, DT_FLOAT);
  op_desc->AddInputDesc("x", tensor_desc);
  ge::OpDesc::Vistor<ge::GeTensorDescPtr> inputs = op_desc->GetAllInputsDescPtr();
  std::vector<TeOpTensorArg> tensor_arg;
  EXPECT_EQ(FeedTeOpTensorArg(inputs, tensor_arg, op_desc), true);
  EXPECT_EQ(tensor_arg.size(), 1U);
  EXPECT_EQ(tensor_arg[0].tensor[0].shape, std::vector<int64_t>({1}));
}

TEST_F(RegisterOpTilingUT, FeedTeOpTensorArg_NormalShape) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape, FORMAT_NCHW, DT_FLOAT);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  ge::OpDesc::Vistor<ge::GeTensorDescPtr> inputs = op_desc->GetAllInputsDescPtr();
  std::vector<TeOpTensorArg> tensor_arg;
  EXPECT_EQ(FeedTeOpTensorArg(inputs, tensor_arg, op_desc), true);
  EXPECT_EQ(tensor_arg.size(), 1U);
  EXPECT_EQ(tensor_arg[0].tensor[0].shape, std::vector<int64_t>({4, 3, 14, 14}));
}

TEST_F(RegisterOpTilingUT, FeedTeOpConstTensor_WithDepends) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape, FORMAT_NCHW, DT_FLOAT);
  op_desc->AddInputDesc("x", tensor_desc);
  vector<string> depend_names = {"x"};
  AttrUtils::SetListStr(op_desc, "_op_infer_depends", depend_names);
  ComputeGraphPtr graph = make_shared<ComputeGraph>("test");
  NodePtr node = graph->AddNode(op_desc);
  auto op = OpDescUtils::CreateOperatorFromNode(node);
  std::map<std::string, TeConstTensorData> const_inputs;
  EXPECT_NO_THROW(FeedTeOpConstTensor(op, op_desc, const_inputs););
}

TEST_F(RegisterOpTilingUT, OpParaCalculate_V1_NoCompileInfo) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  ComputeGraphPtr graph = make_shared<ComputeGraph>("test");
  NodePtr node = graph->AddNode(op_desc);
  auto op = OpDescUtils::CreateOperatorFromNode(node);
  OpRunInfo run_info;
  graphStatus ret = OpParaCalculate(op, run_info, op_tiling_stub_v1);
  EXPECT_EQ(ret, GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, OpParaCalculate_V1_Success) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  string compile_info_key = "compile_info_key";
  string compile_info_json = "compile_info_json";
  AttrUtils::SetStr(op_desc, COMPILE_INFO_KEY, compile_info_key);
  AttrUtils::SetStr(op_desc, COMPILE_INFO_JSON, compile_info_json);
  ComputeGraphPtr graph = make_shared<ComputeGraph>("test");
  NodePtr node = graph->AddNode(op_desc);
  auto op = OpDescUtils::CreateOperatorFromNode(node);
  OpRunInfo run_info;
  graphStatus ret = OpParaCalculate(op, run_info, op_tiling_stub_v1);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
}

TEST_F(RegisterOpTilingUT, OpParaCalculate_V1_FeedTensorArgFail) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape, FORMAT_NCHW, static_cast<ge::DataType>(999));
  op_desc->AddInputDesc("x", tensor_desc);
  string compile_info_key = "compile_info_key";
  string compile_info_json = "compile_info_json";
  AttrUtils::SetStr(op_desc, COMPILE_INFO_KEY, compile_info_key);
  AttrUtils::SetStr(op_desc, COMPILE_INFO_JSON, compile_info_json);
  ComputeGraphPtr graph = make_shared<ComputeGraph>("test");
  NodePtr node = graph->AddNode(op_desc);
  auto op = OpDescUtils::CreateOperatorFromNode(node);
  OpRunInfo run_info;
  graphStatus ret = OpParaCalculate(op, run_info, op_tiling_stub_v1);
  EXPECT_EQ(ret, GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, OpParaCalculate_V1_NoCompileInfoJson) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  string compile_info_key = "compile_info_key";
  AttrUtils::SetStr(op_desc, COMPILE_INFO_KEY, compile_info_key);
  ComputeGraphPtr graph = make_shared<ComputeGraph>("test");
  NodePtr node = graph->AddNode(op_desc);
  auto op = OpDescUtils::CreateOperatorFromNode(node);
  OpRunInfo run_info;
  graphStatus ret = OpParaCalculate(op, run_info, op_tiling_stub_v1);
  EXPECT_EQ(ret, GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, GenerateCompileInfoKey_BasicTest) {
  std::vector<int64_t> workspace_size_list = {100, 200, 300};
  std::string op_compile_info_key;
  GenerateCompileInfoKey(workspace_size_list, op_compile_info_key);
  EXPECT_NE(op_compile_info_key.find("100"), std::string::npos);
  EXPECT_NE(op_compile_info_key.find("200"), std::string::npos);
  EXPECT_NE(op_compile_info_key.find("300"), std::string::npos);
}

TEST_F(RegisterOpTilingUT, GenerateCompileInfoKey_EmptyList) {
  std::vector<int64_t> workspace_size_list;
  std::string op_compile_info_key = "initial";
  GenerateCompileInfoKey(workspace_size_list, op_compile_info_key);
  EXPECT_EQ(op_compile_info_key, "initial");
}

TEST_F(RegisterOpTilingUT, AssembleCompileInfoJson_ValidJson) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  std::vector<int64_t> workspace_size_list = {100, 200};
  std::string op_compile_info_json = "{\"_workspace_size_list\":[]}";
  graphStatus ret = AssembleCompileInfoJson(op_desc, workspace_size_list, op_compile_info_json);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_NE(op_compile_info_json.find("100"), std::string::npos);
  EXPECT_NE(op_compile_info_json.find("200"), std::string::npos);
}

TEST_F(RegisterOpTilingUT, AssembleCompileInfoJson_InvalidJson) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "ReluV1");
  std::vector<int64_t> workspace_size_list = {100, 200};
  std::string op_compile_info_json = "invalid_json";
  graphStatus ret = AssembleCompileInfoJson(op_desc, workspace_size_list, op_compile_info_json);
  EXPECT_EQ(ret, GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, AssembleWorkspaceList_NoAtomicInfo) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  int64_t first_clean_size = 0;
  std::vector<int64_t> workspace_size_list;
  graphStatus ret = AssembleWorkspaceList(op_desc, first_clean_size, workspace_size_list);
  EXPECT_EQ(ret, GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, AssembleWorkspaceList_WithAtomicOutput) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddOutputDesc("y", tensor_desc);
  ge::TensorUtils::SetSize(tensor_desc, 128);
  std::vector<int64_t> atomic_output_indices = {0};
  AttrUtils::SetListInt(op_desc, ge::ATOMIC_ATTR_OUTPUT_INDEX, atomic_output_indices);
  int64_t first_clean_size = 0;
  std::vector<int64_t> workspace_size_list;
  graphStatus ret = AssembleWorkspaceList(op_desc, first_clean_size, workspace_size_list);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_FALSE(workspace_size_list.empty());
}

TEST_F(RegisterOpTilingUT, AssembleWorkspaceList_WithInvalidOutputIndex) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  std::vector<int64_t> atomic_output_indices = {5};
  AttrUtils::SetListInt(op_desc, ge::ATOMIC_ATTR_OUTPUT_INDEX, atomic_output_indices);
  int64_t first_clean_size = 0;
  std::vector<int64_t> workspace_size_list;
  graphStatus ret = AssembleWorkspaceList(op_desc, first_clean_size, workspace_size_list);
  EXPECT_EQ(ret, GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, AssembleWorkspaceList_V2_NoAtomicInfo) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  std::vector<int64_t> workspace_list;
  std::vector<int64_t> workspace_size_list;
  graphStatus ret = AssembleWorkspaceList(op_desc, workspace_list, workspace_size_list);
  EXPECT_EQ(ret, GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, AssembleWorkspaceList_V2_WithAtomicOutput) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  GeShape shape({4, 3, 14, 14});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddOutputDesc("y", tensor_desc);
  ge::TensorUtils::SetSize(tensor_desc, 256);
  std::vector<int64_t> atomic_output_indices = {0};
  AttrUtils::SetListInt(op_desc, ge::ATOMIC_ATTR_OUTPUT_INDEX, atomic_output_indices);
  std::vector<int64_t> workspace_list;
  std::vector<int64_t> workspace_size_list;
  graphStatus ret = AssembleWorkspaceList(op_desc, workspace_list, workspace_size_list);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_FALSE(workspace_list.empty());
  EXPECT_FALSE(workspace_size_list.empty());
}

TEST_F(RegisterOpTilingUT, AssembleWorkspaceList_V2_WithInvalidOutputIndex) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  std::vector<int64_t> atomic_output_indices = {10};
  AttrUtils::SetListInt(op_desc, ge::ATOMIC_ATTR_OUTPUT_INDEX, atomic_output_indices);
  std::vector<int64_t> workspace_list;
  std::vector<int64_t> workspace_size_list;
  graphStatus ret = AssembleWorkspaceList(op_desc, workspace_list, workspace_size_list);
  EXPECT_EQ(ret, GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, AssembleWorkspaceList_V2_WithAtomicWorkspace) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  op_desc->SetWorkspaceBytes({512, 1024});
  std::map<int64_t, int64_t> index_2_workspace_size = {{0, 5}};
  std::map<string, std::map<int64_t, int64_t>> atomic_workspace_info = {{"relu", index_2_workspace_size}};
  op_desc->SetExtAttr(ge::EXT_ATTR_ATOMIC_WORKSPACE_INFO, atomic_workspace_info);
  std::vector<int64_t> workspace_list;
  std::vector<int64_t> workspace_size_list;
  graphStatus ret = AssembleWorkspaceList(op_desc, workspace_list, workspace_size_list);
  EXPECT_EQ(ret, GRAPH_SUCCESS);
  EXPECT_FALSE(workspace_size_list.empty());
}

TEST_F(RegisterOpTilingUT, parse_tiling_data_BasicTest) {
  int32_t data[4] = {1, 2, 3, 4};
  EXPECT_NO_THROW(parse_tiling_data(data, sizeof(data)));
}

TEST_F(RegisterOpTilingUT, parse_tiling_data_NullPtr) {
  EXPECT_NO_THROW(parse_tiling_data(nullptr, 0));
}

TEST_F(RegisterOpTilingUT, parse_tiling_data_SizeNotAligned) {
  int32_t data[2] = {1, 2};
  EXPECT_NO_THROW(parse_tiling_data(data, sizeof(data) - 1));
}

TEST_F(RegisterOpTilingUT, GetOpTilingInfo_NullOpDesc) {
  OpDescPtr op_desc = nullptr;
  EXPECT_EQ(GetOpTilingInfo(op_desc), nullptr);
}

TEST_F(RegisterOpTilingUT, GetOpTilingInfo_NotFoundOpType) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "NonExistentOpType_xyz");
  EXPECT_EQ(GetOpTilingInfo(op_desc), nullptr);
}

TEST_F(RegisterOpTilingUT, GetOpAtomicTilingInfo_NullOpDesc) {
  OpDescPtr op_desc = nullptr;
  EXPECT_EQ(GetOpAtomicTilingInfo(op_desc), nullptr);
}

TEST_F(RegisterOpTilingUT, GetOpAtomicTilingInfo_NotFoundOpType) {
  OpDescPtr op_desc = make_shared<OpDesc>("relu", "NonExistentAtomicOpType_xyz");
  EXPECT_EQ(GetOpAtomicTilingInfo(op_desc), nullptr);
}

TEST_F(RegisterOpTilingUT, PostProcCalculateV2_WorkspaceLargerThanAll) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  Operator op = OpDescUtils::CreateOperatorFromNode(node);
  OpDescPtr op_desc = node->GetOpDesc();
  std::vector<int64_t> workspaces = {1};
  OpRunInfoV2 run_info;
  run_info.SetWorkspaces(workspaces);
  std::vector<int64_t> all_workspaces;
  op_desc->SetWorkspaceBytes(all_workspaces);
  ge::graphStatus ret = PostProcCalculateV2(op, run_info);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

TEST_F(RegisterOpTilingUT, PostProcCalculateV2_WorkspaceLessThanAll) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  Operator op = OpDescUtils::CreateOperatorFromNode(node);
  OpDescPtr op_desc = node->GetOpDesc();
  std::vector<int64_t> run_workspaces = {1, 2};
  OpRunInfoV2 run_info;
  run_info.SetWorkspaces(run_workspaces);
  std::vector<int64_t> all_workspaces = {10, 20, 30, 40};
  op_desc->SetWorkspaceBytes(all_workspaces);
  ge::graphStatus ret = PostProcCalculateV2(op, run_info);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

TEST_F(RegisterOpTilingUT, OpAtomicCalculateV2_EmptyFuncInfo) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  OpDescPtr op_desc = node->GetOpDesc();
  std::unordered_map<std::string, OpTilingFuncInfo> &tiling_func_map = OpTilingFuncRegistry::RegisteredOpFuncInfo();
  OpTilingFuncInfo op_func_info(OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
  tiling_func_map.emplace(OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN, op_func_info);
  OpRunInfoV2 run_info;
  graphStatus ret = OpAtomicCalculateV2(*node, run_info);
  EXPECT_EQ(ret, ge::GRAPH_FAILED);
  tiling_func_map.erase(OP_TYPE_DYNAMIC_ATOMIC_ADDR_CLEAN);
}

TEST_F(RegisterOpTilingUT, UpDateNodeShapeBack_Success) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  OpDescPtr op_desc = node->GetOpDesc();
  ThreadSliceMapDyPtr slice_info_ptr = std::make_shared<ThreadSliceMapDy>();
  slice_info_ptr->input_tensor_indexes.push_back(0);
  slice_info_ptr->output_tensor_indexes.push_back(0);
  GeShape shape({4, 1, 3, 4, 16});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  vector<int64_t> ori_shape = {4, 4};
  auto ret = UpDateNodeShapeBack(op_desc, slice_info_ptr, ori_shape);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

TEST_F(RegisterOpTilingUT, UpDateNodeShapeBack_SizeMismatch) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  OpDescPtr op_desc = node->GetOpDesc();
  ThreadSliceMapDyPtr slice_info_ptr = std::make_shared<ThreadSliceMapDy>();
  slice_info_ptr->input_tensor_indexes.push_back(0);
  slice_info_ptr->output_tensor_indexes.push_back(0);
  GeShape shape({4, 1, 3, 4, 16});
  GeTensorDesc tensor_desc(shape);
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  vector<int64_t> ori_shape = {4};
  auto ret = UpDateNodeShapeBack(op_desc, slice_info_ptr, ori_shape);
  EXPECT_EQ(ret, ge::GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, UpDateNodeShapeBack_NullSliceInfo) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  OpDescPtr op_desc = node->GetOpDesc();
  ThreadSliceMapDyPtr slice_info_ptr = nullptr;
  vector<int64_t> ori_shape = {4, 4};
  auto ret = UpDateNodeShapeBack(op_desc, slice_info_ptr, ori_shape);
  EXPECT_EQ(ret, ge::GRAPH_FAILED);
}

TEST_F(RegisterOpTilingUT, PostProcMemoryCheck_MemCheckDisabled) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 1, 1);
  Operator op = OpDescUtils::CreateOperatorFromNode(node);
  OpDescPtr op_desc = node->GetOpDesc();
  OpRunInfoV2 run_info;
  (void)ge::AttrUtils::SetBool(op_desc, kMemoryCheck, false);
  ge::graphStatus ret = PostProcMemoryCheck(op, run_info);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}

TEST_F(RegisterOpTilingUT, PostProcMemoryCheck_AlignOffset) {
  auto root_builder = ut::GraphBuilder("root");
  const auto &node = root_builder.AddNode("relu", "ReluV2", 2, 1);
  GeShape shape({3, 4, 2, 1});
  GeTensorDesc tensor_desc(shape);
  OpDescPtr op_desc = node->GetOpDesc();
  op_desc->AddInputDesc("x", tensor_desc);
  op_desc->AddOutputDesc("y", tensor_desc);
  Operator op = OpDescUtils::CreateOperatorFromNode(node);
  OpRunInfoV2 run_info;
  (void)ge::AttrUtils::SetBool(op_desc, kMemoryCheck, true);
  ge::graphStatus ret = PostProcMemoryCheck(op, run_info);
  EXPECT_EQ(ret, ge::GRAPH_SUCCESS);
}
}  // namespace optiling
