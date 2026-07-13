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
#include <google/protobuf/text_format.h>
#include <set>
#include "nlohmann/json.hpp"
#include "proto/ge_ir.pb.h"
#include "common/helper/visual_json_converter.h"
#include "graph/model.h"
#include "graph/compute_graph.h"
#include "graph/op_desc.h"
#include "graph/ge_tensor.h"

namespace ge {
namespace {
class VisualizationTest : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

const nlohmann::json *FindMapValue(const nlohmann::json &entries, const std::string &key) {
  for (const auto &entry : entries) {
    if (entry.contains("key") && entry["key"] == key && entry.contains("value")) {
      return &entry["value"];
    }
  }
  return nullptr;
}

const nlohmann::json &RequireMapValue(const nlohmann::json &entries, const std::string &key) {
  const auto *value = FindMapValue(entries, key);
  EXPECT_NE(value, nullptr);
  static const nlohmann::json empty_value;
  return (value != nullptr) ? *value : empty_value;
}

void InitBasicModel(proto::ModelDef &model_def) {
  model_def.set_name("test_model");
  model_def.set_version(1);
  model_def.set_custom_version("1.0.0");
  auto *model_attr = model_def.mutable_attr();
  (*model_attr)["model_attr_str"].set_s("model_value");
  (*model_attr)["model_attr_int"].set_i(123);
  (*model_attr)["model_attr_float"].set_f(3.14f);
  (*model_attr)["model_attr_bool"].set_b(true);
  (*model_attr)["model_attr_dtype"].set_dt(DT_FLOAT);
  auto *other_attr = model_def.mutable_attr_groups()->mutable_other_group_def()->mutable_attr();
  (*other_attr)["other_attr_1"].set_s("other_value");
  (*other_attr)["other_attr_2"].set_i(456);
  auto *shape_env = model_def.mutable_attr_groups()->add_attr_group_def()->mutable_shape_env_attr_group();
  (*shape_env->mutable_symbol_to_value())["sym0"] = 64;
}

proto::OpDef *AddBasicOp(proto::ModelDef &model_def) {
  auto *graph = model_def.add_graph();
  graph->set_name("main_graph");
  graph->add_input("input:0");
  graph->add_output("output:0");
  (*graph->mutable_attr())["graph_attr_str"].set_s("graph_value");
  auto *op = graph->add_op();
  op->set_name("add_op");
  op->set_type("Add");
  op->add_input("input:0");
  op->set_id(1);
  op->set_has_out_attr(true);
  op->set_stream_id(7);
  op->add_input_name("x");
  op->add_src_name("input");
  op->add_src_index(0);
  op->add_dst_name("output");
  op->add_dst_index(0);
  op->add_input_i(0);
  op->add_output_i(0);
  op->add_workspace(0);
  op->add_workspace_bytes(1024);
  op->add_is_input_const(false);
  op->add_subgraph_name("");
  return op;
}

void AddBasicScalarAttrs(proto::OpDef *op) {
  auto *op_attr = op->mutable_attr();
  (*op_attr)["attr_s"].set_s("string_value");
  (*op_attr)["attr_i"].set_i(42);
  (*op_attr)["attr_f"].set_f(2.718f);
  (*op_attr)["attr_b"].set_b(false);
  (*op_attr)["attr_dt"].set_dt(DT_INT32);
  (*op_attr)["attr_bt"].set_bt("bytes_data");
  (*op_attr)["attr_expr"].set_expression("x + y");
}

void AddBasicListAttrs(proto::OpDef *op) {
  auto *op_attr = op->mutable_attr();
  auto *list_str = (*op_attr)["attr_list_str"].mutable_list();
  list_str->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_STRING);
  list_str->add_s("str1");
  list_str->add_s("str2");
  auto *list_int = (*op_attr)["attr_list_int"].mutable_list();
  list_int->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_INT);
  list_int->add_i(1);
  list_int->add_i(2);
  list_int->add_i(3);
  auto *list_float = (*op_attr)["attr_list_float"].mutable_list();
  list_float->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_FLOAT);
  list_float->add_f(1.1f);
  list_float->add_f(2.2f);
  auto *list_bool = (*op_attr)["attr_list_bool"].mutable_list();
  list_bool->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_BOOL);
  list_bool->add_b(true);
  list_bool->add_b(false);
  auto *list_dt = (*op_attr)["attr_list_dt"].mutable_list();
  list_dt->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_DATA_TYPE);
  list_dt->add_dt(DT_FLOAT);
  list_dt->add_dt(DT_INT32);
}

void AddBasicNestedListAttrs(proto::OpDef *op) {
  auto *op_attr = op->mutable_attr();
  auto *list_list_int = (*op_attr)["attr_list_list_int"].mutable_list_list_int();
  auto *inner1 = list_list_int->add_list_list_i();
  inner1->add_list_i(1);
  inner1->add_list_i(2);
  auto *inner2 = list_list_int->add_list_list_i();
  inner2->add_list_i(3);
  inner2->add_list_i(4);
  auto *list_list_float = (*op_attr)["attr_list_list_float"].mutable_list_list_float();
  auto *inner_f1 = list_list_float->add_list_list_f();
  inner_f1->add_list_f(1.1f);
  inner_f1->add_list_f(2.2f);
}

void AddBasicComplexAttrs(proto::OpDef *op) {
  auto *op_attr = op->mutable_attr();
  auto *func = (*op_attr)["attr_func"].mutable_func();
  func->set_name("my_func");
  (*func->mutable_attr())["func_attr_1"].set_s("func_value");
  (*func->mutable_attr())["func_attr_2"].set_i(999);
  auto *td_attr = (*op_attr)["attr_td"].mutable_td();
  td_attr->set_name("tensor_desc_attr");
  td_attr->set_dtype(proto::DT_INT32);
  td_attr->mutable_shape()->add_dim(10);
  td_attr->set_layout("ND");
  auto *t_desc = (*op_attr)["attr_t"].mutable_t()->mutable_desc();
  t_desc->set_name("tensor_def_attr");
  t_desc->set_dtype(proto::DT_FLOAT16);
  t_desc->mutable_shape()->add_dim(5);
  t_desc->set_layout("ND");
  auto *g_attr = (*op_attr)["attr_g"].mutable_g();
  g_attr->set_name("sub_graph");
  g_attr->add_input("sub_input:0");
  g_attr->add_output("sub_output:0");
}

void AddBasicStructuredListAttrs(proto::OpDef *op) {
  auto *op_attr = op->mutable_attr();
  auto *list_td = (*op_attr)["attr_list_td"].mutable_list();
  list_td->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_TENSOR_DESC);
  list_td->add_td()->set_name("list_td_1");
  list_td->mutable_td(0)->set_dtype(proto::DT_FLOAT);
  list_td->add_td()->set_name("list_td_2");
  list_td->mutable_td(1)->set_dtype(proto::DT_INT32);
  auto *list_t = (*op_attr)["attr_list_t"].mutable_list();
  list_t->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_TENSOR);
  list_t->add_t()->mutable_desc()->set_name("list_t_1");
  list_t->mutable_t(0)->mutable_desc()->set_dtype(proto::DT_FLOAT);
  list_t->add_t()->mutable_desc()->set_name("list_t_2");
  list_t->mutable_t(1)->mutable_desc()->set_dtype(proto::DT_INT32);
  auto *list_g = (*op_attr)["attr_list_g"].mutable_list();
  list_g->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_GRAPH);
  list_g->add_g()->set_name("list_g_1");
  list_g->add_g()->set_name("list_g_2");
  auto *list_na = (*op_attr)["attr_list_na"].mutable_list();
  list_na->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_NAMED_ATTRS);
  auto *na1 = list_na->add_na();
  na1->set_name("named_attrs_1");
  (*na1->mutable_attr())["na_attr_1"].set_s("value_1");
}

void AddBasicTensorDescs(proto::OpDef *op) {
  auto *input_desc = op->add_input_desc();
  input_desc->set_name("input_tensor");
  input_desc->set_dtype(proto::DT_FLOAT);
  for (const auto dim : {1, 3, 224, 224}) {
    input_desc->mutable_shape()->add_dim(dim);
  }
  input_desc->set_layout("NCHW");
  input_desc->set_size(1 * 3 * 224 * 224 * 4);
  input_desc->set_has_out_attr(true);
  (*input_desc->mutable_attr())["format"].set_s("NCHW");
  (*input_desc->mutable_attr())["origin_format"].set_s("NCHW");
  auto *output_desc = op->add_output_desc();
  output_desc->set_name("output_tensor");
  output_desc->set_dtype(proto::DT_FLOAT);
  for (const auto dim : {1, 3, 224, 224}) {
    output_desc->mutable_shape()->add_dim(dim);
  }
  output_desc->set_layout("NCHW");
}

proto::ModelDef BuildBasicFieldsModelDef() {
  proto::ModelDef model_def;
  InitBasicModel(model_def);
  auto *op = AddBasicOp(model_def);
  AddBasicScalarAttrs(op);
  AddBasicListAttrs(op);
  AddBasicNestedListAttrs(op);
  AddBasicComplexAttrs(op);
  AddBasicStructuredListAttrs(op);
  AddBasicTensorDescs(op);
  return model_def;
}

void ExpectBasicModelJson(const nlohmann::json &model) {
  EXPECT_EQ(model["name"], "test_model");
  EXPECT_EQ(model["version"], 1);
  EXPECT_EQ(model["custom_version"], "1.0.0");
  const auto &model_attrs = model["attr"];
  EXPECT_EQ(model_attrs["model_attr_str"], "model_value");
  EXPECT_EQ(model_attrs["model_attr_int"], 123);
  EXPECT_FLOAT_EQ(model_attrs["model_attr_float"].get<float>(), 3.14f);
  EXPECT_EQ(model_attrs["model_attr_bool"], true);
  EXPECT_EQ(model_attrs["model_attr_dtype"], DT_FLOAT);
  const auto &other_group_json = model["attr_groups"]["other_group_def"]["attr"];
  EXPECT_EQ(other_group_json["other_attr_1"]["s"], "other_value");
  EXPECT_EQ(other_group_json["other_attr_2"]["i"], 456);
  const auto &shape_env_json = model["attr_groups"]["attr_group_def"][0]["shape_env_attr_group"];
  EXPECT_EQ(shape_env_json["symbol_to_value"]["sym0"], 64);
  ASSERT_EQ(model["graph"].size(), 1);
  const auto &graph_json = model["graph"][0];
  EXPECT_EQ(graph_json["name"], "main_graph");
  EXPECT_EQ(graph_json["input"][0], "input:0");
  EXPECT_EQ(graph_json["output"][0], "output:0");
  EXPECT_EQ(graph_json["attr"]["graph_attr_str"], "graph_value");
}

void ExpectBasicOpJson(const nlohmann::json &op_json) {
  EXPECT_EQ(op_json["name"], "add_op");
  EXPECT_EQ(op_json["type"], "Add");
  EXPECT_EQ(op_json["input"][0], "input:0");
  EXPECT_EQ(op_json["has_out_attr"], true);
  EXPECT_EQ(op_json["id"], 1);
  EXPECT_EQ(op_json["stream_id"], 7);
  EXPECT_EQ(op_json["input_name"][0], "x");
  EXPECT_EQ(op_json["input_i"][0], 0);
  EXPECT_EQ(op_json["output_i"][0], 0);
  EXPECT_EQ(op_json["workspace"][0], 0);
  EXPECT_EQ(op_json["workspace_bytes"][0], 1024);
  EXPECT_EQ(op_json["is_input_const"][0], false);
  EXPECT_EQ(op_json["subgraph_name"][0], "");
}

void ExpectBasicScalarAndListAttrs(const nlohmann::json &attrs) {
  EXPECT_EQ(attrs["attr_s"], "string_value");
  EXPECT_EQ(attrs["attr_i"], 42);
  EXPECT_FLOAT_EQ(attrs["attr_f"].get<float>(), 2.718f);
  EXPECT_EQ(attrs["attr_b"], false);
  EXPECT_EQ(attrs["attr_dt"], DT_INT32);
  EXPECT_EQ(attrs["attr_bt"]["type"], "bytes");
  EXPECT_EQ(attrs["attr_bt"]["value"], "bytes_data");
  EXPECT_EQ(attrs["attr_expr"]["type"], "expression");
  EXPECT_EQ(attrs["attr_expr"]["value"], "x + y");
  EXPECT_EQ(attrs["attr_list_str"]["type"], "list_string");
  EXPECT_EQ(attrs["attr_list_str"]["value"][1], "str2");
  EXPECT_EQ(attrs["attr_list_int"]["type"], "list_int");
  EXPECT_EQ(attrs["attr_list_int"]["value"][2], 3);
  EXPECT_EQ(attrs["attr_list_float"]["type"], "list_float");
  EXPECT_FLOAT_EQ(attrs["attr_list_float"]["value"][1].get<float>(), 2.2f);
  EXPECT_EQ(attrs["attr_list_bool"]["type"], "list_bool");
  EXPECT_EQ(attrs["attr_list_bool"]["value"][1], false);
  EXPECT_EQ(attrs["attr_list_dt"]["type"], "list_data_type");
  EXPECT_EQ(attrs["attr_list_dt"]["value"][1], DT_INT32);
}

void ExpectBasicNestedAndComplexAttrs(const nlohmann::json &attrs) {
  ASSERT_EQ(attrs["attr_list_list_int"].size(), 2);
  EXPECT_EQ(attrs["attr_list_list_int"][1][1], 4);
  ASSERT_EQ(attrs["attr_list_list_float"].size(), 1);
  EXPECT_FLOAT_EQ(attrs["attr_list_list_float"][0][1].get<float>(), 2.2f);
  EXPECT_EQ(attrs["attr_func"]["type"], "func");
  EXPECT_EQ(attrs["attr_func"]["value"]["name"], "my_func");
  EXPECT_EQ(attrs["attr_func"]["value"]["attr"]["func_attr_1"], "func_value");
  EXPECT_EQ(attrs["attr_func"]["value"]["attr"]["func_attr_2"], 999);
  EXPECT_EQ(attrs["attr_td"]["type"], "tensor_desc");
  EXPECT_EQ(attrs["attr_td"]["value"]["name"], "tensor_desc_attr");
  EXPECT_EQ(attrs["attr_td"]["value"]["dtype"], "DT_INT32");
  EXPECT_EQ(attrs["attr_td"]["value"]["shape"]["dim"][0], 10);
  EXPECT_EQ(attrs["attr_t"]["type"], "tensor");
  EXPECT_EQ(attrs["attr_t"]["value"]["desc"]["name"], "tensor_def_attr");
  EXPECT_EQ(attrs["attr_t"]["value"]["desc"]["dtype"], "DT_FLOAT16");
  EXPECT_FALSE(attrs["attr_t"]["value"].contains("data"));
  EXPECT_EQ(attrs["attr_g"]["type"], "graph");
  EXPECT_EQ(attrs["attr_g"]["value"]["name"], "sub_graph");
  EXPECT_EQ(attrs["attr_g"]["value"]["input"][0], "sub_input:0");
}

void ExpectBasicStructuredListAttrs(const nlohmann::json &attrs) {
  EXPECT_EQ(attrs["attr_list_td"]["type"], "list_tensor_desc");
  ASSERT_EQ(attrs["attr_list_td"]["value"].size(), 2);
  EXPECT_EQ(attrs["attr_list_td"]["value"][0]["name"], "list_td_1");
  EXPECT_EQ(attrs["attr_list_td"]["value"][1]["dtype"], "DT_INT32");
  EXPECT_EQ(attrs["attr_list_t"]["type"], "list_tensor");
  ASSERT_EQ(attrs["attr_list_t"]["value"].size(), 2);
  EXPECT_EQ(attrs["attr_list_t"]["value"][0]["desc"]["name"], "list_t_1");
  EXPECT_EQ(attrs["attr_list_t"]["value"][1]["desc"]["dtype"], "DT_INT32");
  EXPECT_EQ(attrs["attr_list_g"]["type"], "list_graph");
  ASSERT_EQ(attrs["attr_list_g"]["value"].size(), 2);
  EXPECT_EQ(attrs["attr_list_g"]["value"][1]["name"], "list_g_2");
  EXPECT_EQ(attrs["attr_list_na"]["type"], "list_named_attrs");
  ASSERT_EQ(attrs["attr_list_na"]["value"].size(), 1);
  EXPECT_EQ(attrs["attr_list_na"]["value"][0]["name"], "named_attrs_1");
  EXPECT_EQ(attrs["attr_list_na"]["value"][0]["attr"]["na_attr_1"], "value_1");
}

void ExpectBasicTensorDescJson(const nlohmann::json &op_json) {
  ASSERT_EQ(op_json["input_desc"].size(), 1);
  const auto &input_desc_json = op_json["input_desc"][0];
  EXPECT_EQ(input_desc_json["name"], "input_tensor");
  EXPECT_EQ(input_desc_json["dtype"], "DT_FLOAT");
  EXPECT_EQ(input_desc_json["shape"]["dim"][3], 224);
  EXPECT_EQ(input_desc_json["layout"], "NCHW");
  EXPECT_EQ(input_desc_json["size"], 1 * 3 * 224 * 224 * 4);
  EXPECT_EQ(input_desc_json["has_out_attr"], true);
  EXPECT_EQ(input_desc_json["attr"]["format"], "NCHW");
  EXPECT_EQ(input_desc_json["attr"]["origin_format"], "NCHW");
  ASSERT_EQ(op_json["output_desc"].size(), 1);
  EXPECT_EQ(op_json["output_desc"][0]["name"], "output_tensor");
  EXPECT_EQ(op_json["output_desc"][0]["dtype"], "DT_FLOAT");
}

TEST_F(VisualizationTest, SerializeFromModelDef_BasicFields) {
  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromModelDef(BuildBasicFieldsModelDef(), json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  EXPECT_EQ(j["format"], "ge_visual_json");
  EXPECT_EQ(j["format_version"], 1);

  const auto &model = j["model"];
  ExpectBasicModelJson(model);
  const auto &op_json = model["graph"][0]["op"][0];
  ExpectBasicOpJson(op_json);
  const auto &attrs = op_json["attr"];
  ExpectBasicScalarAndListAttrs(attrs);
  ExpectBasicNestedAndComplexAttrs(attrs);
  ExpectBasicStructuredListAttrs(attrs);
  ExpectBasicTensorDescJson(op_json);
}

TEST_F(VisualizationTest, SerializeFromModelDef_TensorDefNoData) {
  // 验证 TensorDef 不包含 data 字段
  proto::ModelDef model_def;
  model_def.set_name("test_model");

  auto *graph = model_def.add_graph();
  graph->set_name("main_graph");

  auto *op = graph->add_op();
  op->set_name("const_op");
  op->set_type("Const");

  // 添加一个 TensorDef 属性，包含 data
  auto *t_attr = (*op->mutable_attr())["value"].mutable_t();
  auto *t_desc = t_attr->mutable_desc();
  t_desc->set_name("const_tensor");
  t_desc->set_dtype(proto::DT_FLOAT);
  auto *t_shape = t_desc->mutable_shape();
  t_shape->add_dim(2);
  t_shape->add_dim(3);
  t_desc->set_layout("ND");

  // 设置 data（应该在 JSON 中被忽略）
  std::string data(24, '\x00');  // 2*3*4 bytes for float32
  t_attr->set_data(data);

  // 执行序列化
  std::string json_str;
  auto status = VisualJsonConverter::SerializeFromModelDef(model_def, json_str);
  ASSERT_EQ(status, SUCCESS);

  // 解析 JSON
  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));

  // 验证 TensorDef 的 desc 存在但 data 不存在
  auto &attrs = j["model"]["graph"][0]["op"][0]["attr"];
  EXPECT_TRUE(attrs["value"].contains("type"));
  EXPECT_EQ(attrs["value"]["type"], "tensor");
  EXPECT_TRUE(attrs["value"]["value"].contains("desc"));
  EXPECT_EQ(attrs["value"]["value"]["desc"]["name"], "const_tensor");
  EXPECT_EQ(attrs["value"]["value"]["desc"]["dtype"], "DT_FLOAT");
  EXPECT_FALSE(attrs["value"]["value"].contains("data"));
}

TEST_F(VisualizationTest, SerializeFromModelDef_ListValueWithoutValTypeUsesPayloadField) {
  proto::ModelDef model_def;
  model_def.set_name("test_model");
  auto *graph = model_def.add_graph();
  graph->set_name("main_graph");
  auto *op = graph->add_op();
  op->set_name("list_op");
  op->set_type("ListOp");
  auto *attrs = op->mutable_attr();

  (*attrs)["list_s"].mutable_list()->add_s("x");
  (*attrs)["list_i"].mutable_list()->add_i(7);
  (*attrs)["list_f"].mutable_list()->add_f(1.5f);
  (*attrs)["list_b"].mutable_list()->add_b(true);
  (*attrs)["list_bt"].mutable_list()->add_bt("raw_bytes");
  (*attrs)["list_dt"].mutable_list()->add_dt(DT_INT32);
  (*attrs)["list_td"].mutable_list()->add_td()->set_name("td0");
  (*attrs)["list_t"].mutable_list()->add_t()->mutable_desc()->set_name("tensor0");
  (*attrs)["list_g"].mutable_list()->add_g()->set_name("graph0");
  (*attrs)["list_na"].mutable_list()->add_na()->set_name("named0");

  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromModelDef(model_def, json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  const auto &visual_attrs = j["model"]["graph"][0]["op"][0]["attr"];
  EXPECT_EQ(visual_attrs["list_s"]["type"], "list_string");
  EXPECT_EQ(visual_attrs["list_s"]["value"][0], "x");
  EXPECT_EQ(visual_attrs["list_i"]["type"], "list_int");
  EXPECT_EQ(visual_attrs["list_i"]["value"][0], 7);
  EXPECT_EQ(visual_attrs["list_f"]["type"], "list_float");
  EXPECT_FLOAT_EQ(visual_attrs["list_f"]["value"][0].get<float>(), 1.5f);
  EXPECT_EQ(visual_attrs["list_b"]["type"], "list_bool");
  EXPECT_EQ(visual_attrs["list_b"]["value"][0], true);
  EXPECT_EQ(visual_attrs["list_bt"]["type"], "list_bytes");
  EXPECT_EQ(visual_attrs["list_bt"]["value"][0], "raw_bytes");
  EXPECT_EQ(visual_attrs["list_dt"]["type"], "list_data_type");
  EXPECT_EQ(visual_attrs["list_dt"]["value"][0], DT_INT32);
  EXPECT_EQ(visual_attrs["list_td"]["type"], "list_tensor_desc");
  EXPECT_EQ(visual_attrs["list_td"]["value"][0]["name"], "td0");
  EXPECT_EQ(visual_attrs["list_t"]["type"], "list_tensor");
  EXPECT_EQ(visual_attrs["list_t"]["value"][0]["desc"]["name"], "tensor0");
  EXPECT_EQ(visual_attrs["list_g"]["type"], "list_graph");
  EXPECT_EQ(visual_attrs["list_g"]["value"][0]["name"], "graph0");
  EXPECT_EQ(visual_attrs["list_na"]["type"], "list_named_attrs");
  EXPECT_EQ(visual_attrs["list_na"]["value"][0]["name"], "named0");
}

std::string BuildAllAttrKindsVisualJson() {
  return R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "round_trip_model",
      "graph": [{
        "name": "main_graph",
        "op": [{
          "name": "all_attr_op",
          "type": "AllAttrOp",
          "attr": {
            "attr_s": "text",
            "attr_i": 7,
            "attr_f": 1.25,
            "attr_b": true,
            "attr_dt": 0,
            "attr_bt": {"type": "bytes", "value": "raw"},
            "attr_expression": {"type": "expression", "value": "x + y"},
            "attr_func": {"type": "func", "value": {"name": "fn", "attr": {"inner": "v"}}},
            "attr_td": {"type": "tensor_desc", "value": {"name": "td0", "dtype": "DT_FLOAT"}},
            "attr_t": {"type": "tensor", "value": {"desc": {"name": "tensor0", "dtype": "DT_FLOAT"}}},
            "attr_g": {"type": "graph", "value": {"name": "sub_graph"}},
            "list_float": {"type": "list_float", "value": [1.5, 2.5]},
            "list_bytes": {"type": "list_bytes", "value": ["a", "b"]},
            "list_tensor_desc": {"type": "list_tensor_desc", "value": [{"name": "td_list"}]},
            "list_tensor": {"type": "list_tensor", "value": [{"desc": {"name": "tensor_list"}}]},
            "list_graph": {"type": "list_graph", "value": [{"name": "graph_list"}]},
            "list_named_attrs": {"type": "list_named_attrs", "value": [{"name": "na", "attr": {"k": "v"}}]},
            "list_data_type": {"type": "list_data_type", "value": [0, 3]},
            "list_list_float": [[1.25, 2.5]]
          }
        }]
      }]
    }
  })";
}

TEST_F(VisualizationTest, LoadFromVisualJson_AllAttrKindsCanRoundTripToPbJson) {
  nlohmann::json pb_json;
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(BuildAllAttrKindsVisualJson(), pb_json), SUCCESS);

  const auto &attrs = pb_json["graph"][0]["op"][0]["attr"];
  ASSERT_TRUE(attrs.is_array());

  EXPECT_EQ(RequireMapValue(attrs, "attr_s")["s"], "text");
  EXPECT_EQ(RequireMapValue(attrs, "attr_i")["i"], 7);
  EXPECT_FLOAT_EQ(RequireMapValue(attrs, "attr_f")["f"].get<float>(), 1.25f);
  EXPECT_EQ(RequireMapValue(attrs, "attr_b")["b"], true);
  EXPECT_EQ(RequireMapValue(attrs, "attr_dt")["i"], 0);
  EXPECT_EQ(RequireMapValue(attrs, "attr_bt")["bt"], "raw");
  EXPECT_EQ(RequireMapValue(attrs, "attr_expression")["expression"], "x + y");
  EXPECT_EQ(RequireMapValue(attrs, "attr_func")["func"]["name"], "fn");
  EXPECT_EQ(RequireMapValue(attrs, "attr_func")["func"]["attr"][0]["key"], "inner");
  EXPECT_EQ(RequireMapValue(attrs, "attr_td")["td"]["name"], "td0");
  EXPECT_EQ(RequireMapValue(attrs, "attr_t")["t"]["desc"]["name"], "tensor0");
  EXPECT_EQ(RequireMapValue(attrs, "attr_g")["g"]["name"], "sub_graph");

  EXPECT_EQ(RequireMapValue(attrs, "list_float")["list"]["val_type"], 3);
  EXPECT_FLOAT_EQ(RequireMapValue(attrs, "list_float")["list"]["f"][0].get<float>(), 1.5f);
  EXPECT_EQ(RequireMapValue(attrs, "list_bytes")["list"]["val_type"], 5);
  EXPECT_EQ(RequireMapValue(attrs, "list_bytes")["list"]["bt"][0], "a");
  EXPECT_EQ(RequireMapValue(attrs, "list_tensor_desc")["list"]["val_type"], 6);
  EXPECT_EQ(RequireMapValue(attrs, "list_tensor_desc")["list"]["td"][0]["name"], "td_list");
  EXPECT_EQ(RequireMapValue(attrs, "list_tensor")["list"]["val_type"], 7);
  EXPECT_EQ(RequireMapValue(attrs, "list_tensor")["list"]["t"][0]["desc"]["name"], "tensor_list");
  EXPECT_EQ(RequireMapValue(attrs, "list_graph")["list"]["val_type"], 8);
  EXPECT_EQ(RequireMapValue(attrs, "list_graph")["list"]["g"][0]["name"], "graph_list");
  EXPECT_EQ(RequireMapValue(attrs, "list_named_attrs")["list"]["val_type"], 9);
  EXPECT_EQ(RequireMapValue(attrs, "list_named_attrs")["list"]["na"][0]["name"], "na");
  EXPECT_EQ(RequireMapValue(attrs, "list_data_type")["list"]["val_type"], 10);
  EXPECT_EQ(RequireMapValue(attrs, "list_data_type")["list"]["dt"][1], 3);
  EXPECT_FLOAT_EQ(
      RequireMapValue(attrs, "list_list_float")["list_list_float"]["list_list_f"][0]["list_f"][1].get<float>(), 2.5f);
}

TEST_F(VisualizationTest, SerializeFromGeModel_NullOrMissingGraphReturnsFailed) {
  std::string json_str;
  EXPECT_NE(VisualJsonConverter::SerializeFromGeModel(nullptr, json_str), SUCCESS);

  auto ge_model = MakeShared<GeModel>();
  ASSERT_NE(ge_model, nullptr);
  EXPECT_NE(VisualJsonConverter::SerializeFromGeModel(ge_model, json_str), SUCCESS);
}

TEST_F(VisualizationTest, SerializeFromGeModel_SimpleGraphSuccess) {
  auto graph = MakeShared<ComputeGraph>("ge_model_graph");
  ASSERT_NE(graph, nullptr);

  GeTensorDesc tensor_desc(GeShape({1}), FORMAT_ND, DT_FLOAT);
  auto op_desc = MakeShared<OpDesc>("data", "Data");
  ASSERT_NE(op_desc, nullptr);
  (void)op_desc->AddOutputDesc(tensor_desc);
  ASSERT_NE(graph->AddNode(op_desc), nullptr);

  auto ge_model = MakeShared<GeModel>();
  ASSERT_NE(ge_model, nullptr);
  ge_model->SetGraph(graph);

  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromGeModel(ge_model, json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  EXPECT_EQ(j["format"], "ge_visual_json");
  ASSERT_FALSE(j["model"]["graph"].empty());
  EXPECT_EQ(j["model"]["graph"][0]["name"], "ge_model_graph");
  ASSERT_FALSE(j["model"]["graph"][0]["op"].empty());
  EXPECT_EQ(j["model"]["graph"][0]["op"][0]["name"], "data");
}

TEST_F(VisualizationTest, LoadFromVisualJson_InvalidTopLevelReturnsFailed) {
  nlohmann::json pb_json;
  EXPECT_NE(VisualJsonConverter::LoadFromVisualJson("{ invalid json", pb_json), SUCCESS);
  EXPECT_NE(VisualJsonConverter::LoadFromVisualJson("[]", pb_json), SUCCESS);
  EXPECT_NE(VisualJsonConverter::LoadFromVisualJson(R"({"format":"ge_visual_json"})", pb_json), SUCCESS);
}

std::string BuildEdgeAttrKindsVisualJson() {
  return R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "edge_model",
      "version": 123,
      "graph": [{
        "name": "main_graph",
        "attr": {
          "null_attr": null,
          "wrapped_list_attr": {"type": "list", "value": {"val_type": 2, "i": [8, 9]}},
          "unknown_typed_attr": {"type": "unknown_type", "value": 10},
          "list_list_int_attr": [[1, 2], []]
        },
        "op": [{
          "name": "edge_op",
          "type": "EdgeOp",
          "attr": {
            "plain_list_string": {"type": "list_string", "value": ["a", "b"]},
            "plain_list_int": {"type": "list_int", "value": [1, 2]},
            "plain_list_float": {"type": "list_float", "value": [1.25]},
            "plain_list_bool": {"type": "list_bool", "value": [true, false]},
            "plain_list_empty": {"type": "list_int", "value": []},
            "plain_list_object": {"type": "list_tensor_desc", "value": [{"name": "tensor_desc_from_array"}]},
            "fallback_list_string": {"type": "list", "value": ["x", "y"]},
            "fallback_list_int": {"type": "list", "value": [3, 4]},
            "fallback_list_float": {"type": "list", "value": [3.5]},
            "fallback_list_bool": {"type": "list", "value": [true, false]},
            "fallback_list_empty": {"type": "list", "value": []},
            "fallback_list_object": {"type": "list", "value": [{"name": "fallback_tensor_desc"}]}
          },
          "input_desc": [{
            "name": "input0",
            "dtype": "DT_FLOAT",
            "shape": {"dim": [1, 2, "dynamic"]},
            "attr": {"td_attr": "value"}
          }],
          "output_desc": [{
            "name": "output0",
            "dtype": "UNKNOWN_DTYPE"
          }]
        }]
      }]
    }
  })";
}

void ExpectEdgeGraphAttrs(const nlohmann::json &graph_attrs) {
  ASSERT_TRUE(graph_attrs.is_array());
  EXPECT_TRUE(RequireMapValue(graph_attrs, "null_attr").empty());
  EXPECT_EQ(RequireMapValue(graph_attrs, "wrapped_list_attr")["list"]["val_type"], 2);
  EXPECT_EQ(RequireMapValue(graph_attrs, "wrapped_list_attr")["list"]["i"][1], 9);
  EXPECT_EQ(RequireMapValue(graph_attrs, "unknown_typed_attr")["type"], "unknown_type");
  EXPECT_EQ(RequireMapValue(graph_attrs, "unknown_typed_attr")["value"], 10);
  EXPECT_EQ(RequireMapValue(graph_attrs, "list_list_int_attr")["list_list_int"]["list_list_i"][0]["list_i"][1], 2);
  EXPECT_TRUE(RequireMapValue(graph_attrs, "list_list_int_attr")["list_list_int"]["list_list_i"][1]["list_i"].empty());
}

void ExpectEdgeOpListAttrs(const nlohmann::json &op_attrs) {
  ASSERT_TRUE(op_attrs.is_array());
  EXPECT_EQ(RequireMapValue(op_attrs, "plain_list_string")["list"]["val_type"], 1);
  EXPECT_EQ(RequireMapValue(op_attrs, "plain_list_string")["list"]["s"][0], "a");
  EXPECT_EQ(RequireMapValue(op_attrs, "plain_list_int")["list"]["i"][1], 2);
  EXPECT_FLOAT_EQ(RequireMapValue(op_attrs, "plain_list_float")["list"]["f"][0].get<float>(), 1.25f);
  EXPECT_EQ(RequireMapValue(op_attrs, "plain_list_bool")["list"]["b"][1], false);
  EXPECT_FALSE(RequireMapValue(op_attrs, "plain_list_empty")["list"].contains("i"));
  EXPECT_EQ(RequireMapValue(op_attrs, "plain_list_object")["list"]["td"][0]["name"], "tensor_desc_from_array");
  EXPECT_EQ(RequireMapValue(op_attrs, "fallback_list_string")["list"]["s"][1], "y");
  EXPECT_EQ(RequireMapValue(op_attrs, "fallback_list_int")["list"]["i"][1], 4);
  EXPECT_FLOAT_EQ(RequireMapValue(op_attrs, "fallback_list_float")["list"]["f"][0].get<float>(), 3.5f);
  EXPECT_EQ(RequireMapValue(op_attrs, "fallback_list_bool")["list"]["b"][1], false);
  EXPECT_TRUE(RequireMapValue(op_attrs, "fallback_list_empty")["list"].empty());
  EXPECT_EQ(RequireMapValue(op_attrs, "fallback_list_object")["list"]["td"][0]["name"], "fallback_tensor_desc");
}

void ExpectEdgeTensorDescs(const nlohmann::json &pb_json) {
  const auto &input_desc = pb_json["graph"][0]["op"][0]["input_desc"][0];
  EXPECT_EQ(input_desc["dtype"], proto::DT_FLOAT);
  EXPECT_EQ(input_desc["shape"]["dim"][0], 1);
  EXPECT_EQ(input_desc["shape"]["dim"][2], "dynamic");
  const auto &input_desc_attrs = input_desc["attr"];
  ASSERT_TRUE(input_desc_attrs.is_array());
  ASSERT_NE(FindMapValue(input_desc_attrs, "td_attr"), nullptr);
  EXPECT_EQ((*FindMapValue(input_desc_attrs, "td_attr"))["s"], "value");

  const auto &output_desc = pb_json["graph"][0]["op"][0]["output_desc"][0];
  EXPECT_EQ(output_desc["dtype"], "UNKNOWN_DTYPE");
}

TEST_F(VisualizationTest, LoadFromVisualJson_EdgeAttrKindsAndFallbacks) {
  nlohmann::json pb_json;
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(BuildEdgeAttrKindsVisualJson(), pb_json), SUCCESS);
  EXPECT_EQ(pb_json["version"], 123);
  ExpectEdgeGraphAttrs(pb_json["graph"][0]["attr"]);
  ExpectEdgeOpListAttrs(pb_json["graph"][0]["op"][0]["attr"]);
  ExpectEdgeTensorDescs(pb_json);
}

TEST_F(VisualizationTest, SerializeFromModelDef_ListValueKeepsTypedEmptyList) {
  proto::ModelDef model_def;
  auto *graph = model_def.add_graph();
  graph->set_name("main_graph");
  auto *op = graph->add_op();
  op->set_name("empty_list_op");
  op->set_type("ListOp");
  auto *list = (*op->mutable_attr())["empty_i"].mutable_list();
  list->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_INT);

  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromModelDef(model_def, json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  const auto &attr = j["model"]["graph"][0]["op"][0]["attr"]["empty_i"];
  EXPECT_EQ(attr["type"], "list_int");
  EXPECT_TRUE(attr["value"].is_array());
  EXPECT_TRUE(attr["value"].empty());
}

TEST_F(VisualizationTest, SerializeFromModelDef_ListValueUsesNonEmptyPayloadWhenValTypeIsStale) {
  proto::ModelDef model_def;
  auto *graph = model_def.add_graph();
  graph->set_name("main_graph");
  auto *op = graph->add_op();
  op->set_name("stale_type_op");
  op->set_type("ListOp");
  auto *list = (*op->mutable_attr())["stale_list"].mutable_list();
  list->set_val_type(proto::AttrDef_ListValue_ListValueType_VT_LIST_STRING);
  list->add_i(9);

  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromModelDef(model_def, json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  const auto &attr = j["model"]["graph"][0]["op"][0]["attr"]["stale_list"];
  EXPECT_EQ(attr["type"], "list_int");
  EXPECT_EQ(attr["value"][0], 9);
}

TEST_F(VisualizationTest, SerializeFromModelDef_ListValueWithoutTypeAndPayloadEmitsEmptyObject) {
  proto::ModelDef model_def;
  auto *graph = model_def.add_graph();
  graph->set_name("main_graph");
  auto *op = graph->add_op();
  op->set_name("unknown_empty_list_op");
  op->set_type("ListOp");
  (void)(*op->mutable_attr())["unknown_empty"].mutable_list();

  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromModelDef(model_def, json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  const auto &attr = j["model"]["graph"][0]["op"][0]["attr"]["unknown_empty"];
  EXPECT_TRUE(attr.is_object());
  EXPECT_TRUE(attr.empty());
}

TEST_F(VisualizationTest, LoadFromVisualJson_DeepNestedGraphAttrReturnsFailed) {
  nlohmann::json nested_graph = {{"name", "deep_graph"}};
  for (int i = 0; i < 80; ++i) {
    nested_graph = {{"name", "deep_graph"}, {"attr", {{"next", {{"type", "graph"}, {"value", nested_graph}}}}}};
  }
  const nlohmann::json visual_json = {
      {"format", "ge_visual_json"},
      {"format_version", 1},
      {"model", {{"name", "deep_model"}, {"graph", nlohmann::json::array({nested_graph})}}}};

  nlohmann::json pb_json;
  EXPECT_NE(VisualJsonConverter::LoadFromVisualJson(visual_json.dump(), pb_json), SUCCESS);
}

TEST_F(VisualizationTest, LoadFromVisualJson_ModelScalarAndArrayBranches) {
  nlohmann::json pb_json;
  const std::string scalar_model = R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": "plain_model"
  })";
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(scalar_model, pb_json), SUCCESS);
  EXPECT_EQ(pb_json, "plain_model");

  const std::string array_model = R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": [{"name": "array_model"}, "loose_value"]
  })";
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(array_model, pb_json), SUCCESS);
  ASSERT_TRUE(pb_json.is_array());
  ASSERT_EQ(pb_json.size(), 2);
  EXPECT_EQ(pb_json[0]["name"], "array_model");
  EXPECT_EQ(pb_json[1], "loose_value");
}

std::string BuildObjectAttrFallbackVisualJson() {
  return R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "fallback_model",
      "attr_groups": {
        "attr_group_def": [{
          "shape_env_attr_group": {
            "symbol_to_value": {"s0": 7}
          }
        }]
      },
      "graph": [{
        "name": "main_graph",
        "attr": {
          "plain_object": {"raw": 1},
          "list_list_float": [[1.25], [2.5, 3.5]]
        },
        "op": [{
          "name": "op0",
          "type": "FallbackOp",
          "attr": {
            "list_tensor_fallback": {"type": "list", "value": [{"desc": {"name": "tensor0"}}]},
            "unknown_typed": {"type": "unknown", "value": {"keep": true}}
          }
        }]
      }]
    }
  })";
}

void ExpectObjectAttrFallbackModelAttrs(const nlohmann::json &pb_json) {
  const auto &symbol_to_value = pb_json["attr_groups"]["attr_group_def"][0]["shape_env_attr_group"]["symbol_to_value"];
  ASSERT_TRUE(symbol_to_value.is_array());
  EXPECT_EQ(symbol_to_value[0]["key"], "s0");
  EXPECT_EQ(symbol_to_value[0]["value"], 7);
}

void ExpectObjectAttrFallbackGraphAttrs(const nlohmann::json &graph_attrs) {
  ASSERT_TRUE(graph_attrs.is_array());
  const auto *plain_object = FindMapValue(graph_attrs, "plain_object");
  ASSERT_NE(plain_object, nullptr);
  EXPECT_EQ((*plain_object)["raw"], 1);
  const auto *list_list_float = FindMapValue(graph_attrs, "list_list_float");
  ASSERT_NE(list_list_float, nullptr);
  EXPECT_FLOAT_EQ((*list_list_float)["list_list_float"]["list_list_f"][1]["list_f"][1].get<float>(), 3.5f);
}

void ExpectObjectAttrFallbackOpAttrs(const nlohmann::json &op_attrs) {
  ASSERT_TRUE(op_attrs.is_array());
  const auto *list_tensor = FindMapValue(op_attrs, "list_tensor_fallback");
  ASSERT_NE(list_tensor, nullptr);
  EXPECT_EQ((*list_tensor)["list"]["val_type"],
            static_cast<int>(proto::AttrDef_ListValue_ListValueType_VT_LIST_TENSOR_DESC));
  EXPECT_EQ((*list_tensor)["list"]["td"][0]["desc"]["name"], "tensor0");
  const auto *unknown_typed = FindMapValue(op_attrs, "unknown_typed");
  ASSERT_NE(unknown_typed, nullptr);
  EXPECT_EQ((*unknown_typed)["type"], "unknown");
  EXPECT_EQ((*unknown_typed)["value"]["keep"], true);
}

TEST_F(VisualizationTest, LoadFromVisualJson_ObjectAttrAndFallbackLists) {
  nlohmann::json pb_json;
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(BuildObjectAttrFallbackVisualJson(), pb_json), SUCCESS);
  ExpectObjectAttrFallbackModelAttrs(pb_json);
  ExpectObjectAttrFallbackGraphAttrs(pb_json["graph"][0]["attr"]);
  ExpectObjectAttrFallbackOpAttrs(pb_json["graph"][0]["op"][0]["attr"]);
}

TEST_F(VisualizationTest, LoadFromVisualJson_OmJsonOptionsAndLegacyListValue) {
  const std::string visual_json = R"({
    "format": "ge_visual_json",
    "format_version": 1,
    "model": {
      "name": "option_model",
      "custom_version": "drop",
      "graph": [{
        "name": "main_graph",
        "op": [{
          "name": "op0",
          "type": "OptionOp",
          "id": 99,
          "input_desc": [{"name": "x", "dtype": 1}],
          "attr": {
            "legacy_typed_list": {"list": {"type": "list_int", "value": [4, 5]}},
            "legacy_array_list": {"list": ["x", "y"]},
            "list_data_type": {"type": "list_data_type", "value": [1, "bad"]}
          }
        }]
      }]
    }
  })";

  nlohmann::json pb_json;
  const std::set<std::string> black_fields = {"custom_version", "id"};
  ASSERT_EQ(VisualJsonConverter::LoadFromVisualJson(visual_json, black_fields, pb_json, true), SUCCESS);

  EXPECT_FALSE(pb_json.contains("custom_version"));
  const auto &op = pb_json["graph"][0]["op"][0];
  EXPECT_FALSE(op.contains("id"));
  EXPECT_EQ(op["input_desc"][0]["dtype"], "DT_FLOAT");
  const auto &attrs = op["attr"];
  EXPECT_EQ(RequireMapValue(attrs, "legacy_typed_list")["list"]["i"][1], 5);
  EXPECT_EQ(RequireMapValue(attrs, "legacy_array_list")["list"]["s"][0], "x");
  EXPECT_EQ(RequireMapValue(attrs, "list_data_type")["list"]["dt"][0], "DT_FLOAT");
  EXPECT_EQ(RequireMapValue(attrs, "list_data_type")["list"]["dt"][1], "bad");
}

TEST_F(VisualizationTest, SerializeFromModelDef_AttrGroupNumericFields) {
  proto::ModelDef model_def;
  model_def.set_name("attr_group_model");
  auto *shape_env = model_def.mutable_attr_groups()->add_attr_group_def()->mutable_shape_env_attr_group();
  shape_env->set_unique_sym_id(99U);
  shape_env->mutable_shape_setting()->set_dynamic_mode(2);
  (*shape_env->mutable_value_to_symbol())[8].add_symbols("s8");

  std::string json_str;
  ASSERT_EQ(VisualJsonConverter::SerializeFromModelDef(model_def, json_str), SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));
  const auto &shape_env_json = j["model"]["attr_groups"]["attr_group_def"][0]["shape_env_attr_group"];
  EXPECT_EQ(shape_env_json["unique_sym_id"], 99U);
  EXPECT_EQ(shape_env_json["shape_setting"]["dynamic_mode"], 2);
  EXPECT_EQ(shape_env_json["value_to_symbol"]["8"]["symbols"][0], "s8");
}

TEST_F(VisualizationTest, SerializeFromModelDef_EmptyModel) {
  // 验证空模型也能正常序列化
  proto::ModelDef model_def;
  model_def.set_name("empty_model");

  std::string json_str;
  auto status = VisualJsonConverter::SerializeFromModelDef(model_def, json_str);
  ASSERT_EQ(status, SUCCESS);

  nlohmann::json j;
  ASSERT_NO_THROW(j = nlohmann::json::parse(json_str));

  EXPECT_EQ(j["format"], "ge_visual_json");
  EXPECT_EQ(j["format_version"], 1);
  EXPECT_EQ(j["model"]["name"], "empty_model");
  EXPECT_FALSE(j["model"].contains("version"));
  EXPECT_FALSE(j["model"].contains("custom_version"));
  EXPECT_FALSE(j["model"].contains("graph"));
}

}  // namespace
}  // namespace ge
