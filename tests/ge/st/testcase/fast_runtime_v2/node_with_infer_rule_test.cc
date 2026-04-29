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
#include <nlohmann/json.hpp>

#include "runtime/model_v2_executor.h"
#include "lowering/model_converter.h"
#include "runtime/v2/graph_builder/bg_infer_shape.h"

// stub and faker
#include "graph_utils_ex.h"
#include "register/node_converter_registry.h"
#include "graph/utils/inference_rule.h"
#include "common/share_graph.h"
#include "faker/ge_model_builder.h"
#include "faker/fake_value.h"
#include "faker/magic_ops.h"
#include "stub/gert_runtime_stub.h"
#include "check/executor_statistician.h"
#include "graph/operator_reg.h"
#include "graph/utils/graph_dump_utils.h"
#include "common/checker.h"
#include "framework/common/ge_types.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/op_desc_utils.h"
#include "runtime/mem.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "graph/optimize/symbolic/expect_node_info_check_test.h"
#include "faker/space_registry_faker.h"
#include "graph/utils/execute_graph_utils.h"
#include "graph/utils/fast_node_utils.h"
#include "register/kernel_registry.h"
#include "register/kernel_registry_impl.h"
#include "exe_graph/runtime/extended_kernel_context.h"
#include "exe_graph/runtime/storage_shape.h"
#include "exe_graph/runtime/gert_tensor_data.h"
#include "kernel/common_kernel_impl/infer_shape.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op.h"
#include "graph/debug/ge_attr_define.h"

using namespace ge;
using Json = nlohmann::json;

REG_OP(ShapeRuleOp)
    .DYNAMIC_INPUT(x, TensorType({NumberType(), DT_VARIANT}))
    .OUTPUT(y, TensorType({NumberType(), DT_VARIANT}))
    .REQUIRED_ATTR(N, Int)
    .OP_END_FACTORY_REG(ShapeRuleOp);

namespace gert {
LowerResult LoweringShapeRuleOp(const ge::NodePtr &node, const LowerInput &lower_input) {
  const auto output_shapes = bg::InferStorageShape(node, lower_input.input_shapes, *lower_input.global_data);
  std::vector<bg::DevMemValueHolderPtr> output_addrs;
  output_addrs.resize(output_shapes.size(), lower_input.input_addrs[0]);
  return {HyperStatus::Success(), {}, output_shapes, output_addrs};
}

REGISTER_NODE_CONVERTER("ShapeRuleOp", LoweringShapeRuleOp);

namespace {
ComputeGraphPtr ShapeRuleOpGraph(const std::string &rule, const bool &with_binary = true) {
  auto rule_json = Json::parse(rule);
  const size_t num_inputs = rule_json["shape"]["inputs"].size();
  const size_t num_outputs = rule_json["shape"]["outputs"].size();
  DEF_GRAPH(g1) {
    for (size_t i = 0; i < num_inputs; i++) {
      CHAIN(NODE("data" + std::to_string(i), "Data")->EDGE(0, i)->NODE("rule_op", "ShapeRuleOp"));
    }
    for (size_t i = 0; i < num_outputs; i++) {
      CHAIN(NODE("rule_op", "ShapeRuleOp")->EDGE(i, 0)->NODE("shape" + std::to_string(i), "Shape"));
    }
    for (size_t i = 0; i < num_outputs; i++) {
      CHAIN(NODE("shape" + std::to_string(i), "Shape")->EDGE(0, i)->NODE("NetOutput", "NetOutput"));
    }
  };
  auto graph = ToComputeGraph(g1);
  graph->TopologicalSorting();

  for (size_t i = 0; i < num_inputs; i++) {
    const auto data = graph->FindNode("data" + std::to_string(i));
    AttrUtils::SetInt(data->GetOpDesc(), "index", i);
    data->GetOpDesc()->SetOpKernelLibName(kEngineNameGeLocal);
    data->GetOpDesc()->SetOpEngineName(kEngineNameGeLocal);
  }

  const auto rule_op = graph->FindNode("rule_op");
  std::vector<uint8_t> binary;
  ShapeInferenceRule::CompileJsonString(rule, binary);
  AttrUtils::SetStr(rule_op->GetOpDesc(), "_inference_rule", rule);
  if (with_binary) {
    AttrUtils::SetBytes(rule_op->GetOpDesc(), "_inference_rule_binary", Buffer::CopyFrom(binary.data(), binary.size()));
  }

  std::vector<std::string> src_names;
  std::vector<int64_t> src_indexes;
  for (size_t i = 0; i < num_outputs; i++) {
    src_names.push_back("rule_op");
    src_indexes.push_back(i);
  }

  const auto net_output = graph->FindNode("NetOutput");
  net_output->GetOpDesc()->SetSrcName(src_names);
  net_output->GetOpDesc()->SetSrcIndex(src_indexes);
  net_output->GetOpDesc()->SetOpKernelLibName(kEngineNameGeLocal);
  net_output->GetOpDesc()->SetOpEngineName(kEngineNameGeLocal);
  SetGraphOutShapeRange(graph);
  return graph;
}

std::string ValueEqual(const gert::Tensor *shape_tensor, std::vector<int64_t> dims) {
  const size_t shape_size = shape_tensor->GetShapeSize();
  if (shape_size != dims.size()) {
    return "shape size not equal, expect " + std::to_string(dims.size()) + ", got " + std::to_string(shape_size);
  }
  auto *value = shape_tensor->GetData<int32_t>();
  for (size_t i = 0; i < dims.size(); ++i) {
    if (value[i] != dims[i]) {
      return "value[" + std::to_string(i) + "] not equal, expect " + std::to_string(*(dims.begin() + i)) + ", got " +
             std::to_string(value[i]);
    }
  }

  return "";
}

class RuleMaker {
 public:
  RuleMaker() {
    json["shape"]["inputs"] = Json::array();
    json["shape"]["outputs"] = Json::array();
    json["dtype"] = Json::array();
  }

  RuleMaker &Input(const Json::array_t &input, std::initializer_list<int64_t> dims) {
    json["shape"]["inputs"].push_back(input);
    inputs.emplace_back(FakeTensors(dims, 1));
    input_ptrs.push_back(&inputs.back().at(0));
    return *this;
  }

  RuleMaker &Output(const Json::array_t &output, std::vector<int64_t> expected) {
    json["shape"]["outputs"].push_back(output);
    const auto holder = std::make_shared<std::vector<int32_t>>();
    holder->resize(expected.size());
    output_holders.emplace_back(holder);
    outputs.emplace_back(FakeTensors({static_cast<int64_t>(expected.size())}, 1, output_holders.back()->data(), kOnHost,
                                     FORMAT_ND, DT_INT32));
    output_ptrs.push_back(&outputs.back().at(0));
    expected_outputs.emplace_back(expected);
    return *this;
  }

  std::string CheckEqual() const {
    for (size_t i = 0; i < outputs.size(); i++) {
      std::string error_msg = ValueEqual(output_ptrs[i], expected_outputs[i]);
      if (!error_msg.empty()) {
        return "output[" + std::to_string(i) + "] not equal: " + error_msg;
      }
    }
    return "";
  }

  std::string Str() const {
    return json.dump();
  }

  Json json;

  std::vector<FakeTensors> inputs;
  std::vector<FakeTensors> outputs;
  std::vector<std::shared_ptr<std::vector<int32_t>>> output_holders;

  std::vector<Tensor *> input_ptrs;
  std::vector<Tensor *> output_ptrs;

  std::vector<std::vector<int64_t>> expected_outputs;
};

class TestBaseCustomOpWithInferShape : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto input_tensor0 = ctx->GetInputTensor(0);
    GE_ASSERT_NOTNULL(input_tensor0);
    auto input_tensor1 = ctx->GetInputTensor(1);
    GE_ASSERT_NOTNULL(input_tensor1);
    auto input_tensor2 = ctx->GetInputTensor(2);
    GE_ASSERT_NOTNULL(input_tensor2);
    auto workspaces = ctx->MallocWorkSpace(1024);
    GE_ASSERT_NOTNULL(workspaces);

    const gert::Tensor* output_desc_0 = ctx->GetOutputTensor(0);
    GE_ASSERT_NOTNULL(output_desc_0);
    gert::StorageShape out_shape_0 = output_desc_0->GetShape();
    GE_ASSERT_TRUE(out_shape_0.GetStorageShape().GetDimNum() == 1);
    GE_ASSERT_TRUE(out_shape_0.GetStorageShape().GetDim(0) == 2048);
    ge::DataType out_dtype_0 = output_desc_0->GetDataType();
    gert::StorageFormat out_format_0 = output_desc_0->GetFormat();
    gert::Tensor* ge_output_0 = ctx->MallocOutputTensor(0, out_shape_0, out_format_0, out_dtype_0);
    GE_ASSERT_NOTNULL(ge_output_0);
    return SUCCESS;
  }
};

static void RegisterInferShapeByRuleKernel() {
  KernelRegistry::KernelFuncs infer_shape_funcs = {};
  infer_shape_funcs.run_func = [](KernelContext *context) -> ge::graphStatus {
    const auto ctx = reinterpret_cast<ExtendedKernelContext *>(context);
    const auto input_num = context->GetInputNum();
    GE_ASSERT(input_num > 0U);
    const auto compute_node_info = ctx->GetComputeNodeInfo();
    GE_ASSERT_NOTNULL(compute_node_info);

    const auto *rule = context->GetInputValue<std::shared_ptr<ge::ShapeInferenceRule> *>(input_num - 1);
    GE_ASSERT_NOTNULL(rule);
    GE_ASSERT_NOTNULL(*rule);
    GE_ASSERT_EQ((*rule)->Error(), "");
    auto ret = (*rule)->InferOnRuntime(reinterpret_cast<InferShapeContext *>(context));
    if (ret != ge::GRAPH_SUCCESS) {
      return ret;
    }
    ret = kernel::TransformAllOutputsShape(compute_node_info, context);
    if (ret != ge::GRAPH_SUCCESS) {
      return ret;
    }
    return ge::GRAPH_SUCCESS;
  };
  infer_shape_funcs.outputs_creator = [](const ge::FastNode *n, KernelContext *context) -> ge::graphStatus {
    (void)n;
    auto extend_context = reinterpret_cast<ExtendedKernelContext *>(context);
    GE_ASSERT_NOTNULL(extend_context);
    for (size_t index = 0; index < context->GetOutputNum(); index++) {
      auto chain = context->GetOutput(index);
      GE_ASSERT_NOTNULL(chain);
      auto output_desc = extend_context->GetOutputDesc(index);
      GE_ASSERT_NOTNULL(output_desc);
      chain->SetWithDefaultDeleter(new (std::nothrow) Tensor(StorageShape(),
          output_desc->GetFormat(), output_desc->GetDataType()));
    }
    return ge::GRAPH_SUCCESS;
  };
  KernelRegistry::GetInstance().RegisterKernel("InferShapeByRule", {infer_shape_funcs, ""});
}

static void RegisterLoadShapeRuleKernels() {
  KernelRegistry::KernelFuncs load_rule_funcs = {};
  load_rule_funcs.run_func = [](KernelContext *context) -> ge::graphStatus {
    const auto input_num = context->GetInputNum();
    auto *handle = context->GetOutputPointer<std::shared_ptr<ge::ShapeInferenceRule>>(0);
    GE_ASSERT_NOTNULL(handle);

    if (input_num == 1U) {
      auto *rule_json = context->GetInputValue<const char *>(0);
      GE_ASSERT_NOTNULL(rule_json);
      auto rule = ge::ShapeInferenceRule::FromJsonString(rule_json);
      handle->swap(rule);
    } else if (input_num == 3U) {
      auto *compiled_rule = context->GetInputValue<const uint8_t *>(0);
      const auto compiled_rule_size = context->GetInputValue<const size_t>(1);
      GE_ASSERT_NOTNULL(compiled_rule);
      auto *rule_json = context->GetInputValue<const char *>(2);
      GE_ASSERT_NOTNULL(rule_json);
      auto rule = std::make_shared<ge::ShapeInferenceRule>(
          ge::ShapeInferenceRule::FromCompiledBinary(compiled_rule, compiled_rule_size));
      handle->swap(rule);
    }
    return ge::GRAPH_SUCCESS;
  };
  load_rule_funcs.outputs_creator = [](const ge::FastNode *n, KernelContext *context) -> ge::graphStatus {
    GE_ASSERT_NOTNULL(context->GetOutput(0));
    context->GetOutput(0)->SetWithDefaultDeleter(new (std::nothrow) std::shared_ptr<ge::ShapeInferenceRule>());
    return ge::GRAPH_SUCCESS;
  };
  KernelRegistry::GetInstance().RegisterKernel("LoadShapeRuleFromJson", {load_rule_funcs, ""});
  KernelRegistry::GetInstance().RegisterKernel("LoadShapeRuleFromBinary", {load_rule_funcs, ""});
}

static void RegisterInferShapeKernels() {
  RegisterInferShapeByRuleKernel();
  RegisterLoadShapeRuleKernels();
}
}  // namespace

class ShapeRuleOpST : public testing::Test {
 public:
  void SetUp() override {
    const auto env_ptr = getenv("LD_PRELOAD");
    if (env_ptr != nullptr) {
      env = env_ptr;
      unsetenv("LD_PRELOAD");
    }
  }
  void TearDown() override {
    if (!env.empty()) {
      setenv("LD_PRELOAD", env.c_str(), 1);
    }
  }
  std::string env;
};

TEST_F(ShapeRuleOpST, ComplexRuleVertical) {
  RuleMaker rule_maker;
  int64_t s0 = 8;
  int64_t s1 = 24;
  const std::string rule = rule_maker.Input({"s0", "s1"}, {8, 24})
                               .Output({"s1+s0"}, {s1 + s0})
                               .Output({"s1-s0"}, {s1 - s0})
                               .Output({"s1*s0"}, {s1 * s0})
                               .Output({"Div(s1,s0)"}, {s1 / s0})
                               .Output({"Floor(Div(s1,3))"}, {s1 / 3})
                               .Output({"Ceil(Div(s1,3))"}, {(s1 + 2) / 3})
                               .Output({"Pow(s0,2)"}, {s0 * s0})
                               .Output({"Mod(s1,7)"}, {s1 % 7})
                               .Str();
  auto cg = ShapeRuleOpGraph(rule);

  GeModelBuilder builder(cg);
  auto ge_root_model = builder.BuildGeRootModel();

  bg::ValueHolder::PopGraphFrame();  // 不需要BgTest自带的Frame
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model);
  ASSERT_NE(exe_graph, nullptr);

  auto model_executor = ModelV2Executor::Create(exe_graph, ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  ASSERT_EQ(model_executor->Load(), ge::GRAPH_SUCCESS);

  rtStream_t stream;
  ASSERT_EQ(rtStreamCreate(&stream, static_cast<int32_t>(RT_STREAM_PRIORITY_DEFAULT)), RT_ERROR_NONE);
  auto i1 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  ASSERT_EQ(model_executor->Execute({i1.value}, rule_maker.input_ptrs.data(), rule_maker.input_ptrs.size(),
                                    rule_maker.output_ptrs.data(), rule_maker.output_ptrs.size()),
            ge::GRAPH_SUCCESS);

  EXPECT_EQ(rule_maker.CheckEqual(), "");

  ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
  rtStreamDestroy(stream);
}

TEST_F(ShapeRuleOpST, ComplexRuleHorizontal) {
  RuleMaker rule_maker;
  int64_t s0 = 8;
  int64_t s1 = 24;
  const std::string rule = rule_maker.Input({"s0", "s1"}, {8, 24})
                               .Output({"s1+s0", "s1-s0", "s1*s0", "Div(s1,s0)", "Floor(Div(s1,3))", "Ceil(Div(s1,3))",
                                        "Pow(s0,2)", "Mod(s1,7)"},
                                       {s1 + s0, s1 - s0, s1 * s0, s1 / s0, s1 / 3, (s1 + 2) / 3, s0 * s0, s1 % 7})
                               .Str();
  auto cg = ShapeRuleOpGraph(rule);

  GeModelBuilder builder(cg);
  auto ge_root_model = builder.BuildGeRootModel();

  bg::ValueHolder::PopGraphFrame();  // 不需要BgTest自带的Frame
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model);
  ASSERT_NE(exe_graph, nullptr);

  auto model_executor = ModelV2Executor::Create(exe_graph, ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  ASSERT_EQ(model_executor->Load(), ge::GRAPH_SUCCESS);

  rtStream_t stream;
  ASSERT_EQ(rtStreamCreate(&stream, static_cast<int32_t>(RT_STREAM_PRIORITY_DEFAULT)), RT_ERROR_NONE);
  auto i1 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  ASSERT_EQ(model_executor->Execute({i1.value}, rule_maker.input_ptrs.data(), rule_maker.input_ptrs.size(),
                                    rule_maker.output_ptrs.data(), rule_maker.output_ptrs.size()),
            ge::GRAPH_SUCCESS);

  EXPECT_EQ(rule_maker.CheckEqual(), "");

  ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
  rtStreamDestroy(stream);
}

TEST_F(ShapeRuleOpST, ComplexRuleVerticalJit) {
  RuleMaker rule_maker;
  int64_t s0 = 8;
  int64_t s1 = 24;
  const std::string rule = rule_maker.Input({"s0", "s1"}, {8, 24})
                               .Output({"s1+s0"}, {s1 + s0})
                               .Output({"s1-s0"}, {s1 - s0})
                               .Output({"s1*s0"}, {s1 * s0})
                               .Output({"Div(s1,s0)"}, {s1 / s0})
                               .Output({"Floor(Div(s1,3))"}, {s1 / 3})
                               .Output({"Ceil(Div(s1,3))"}, {(s1 + 2) / 3})
                               .Output({"Pow(s0,2)"}, {s0 * s0})
                               .Output({"Mod(s1,7)"}, {s1 % 7})
                               .Str();
  auto cg = ShapeRuleOpGraph(rule, false);

  GeModelBuilder builder(cg);
  auto ge_root_model = builder.BuildGeRootModel();

  bg::ValueHolder::PopGraphFrame();  // 不需要BgTest自带的Frame
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model);
  ASSERT_NE(exe_graph, nullptr);

  auto model_executor = ModelV2Executor::Create(exe_graph, ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  ASSERT_EQ(model_executor->Load(), ge::GRAPH_SUCCESS);

  rtStream_t stream;
  ASSERT_EQ(rtStreamCreate(&stream, static_cast<int32_t>(RT_STREAM_PRIORITY_DEFAULT)), RT_ERROR_NONE);
  auto i1 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  ASSERT_EQ(model_executor->Execute({i1.value}, rule_maker.input_ptrs.data(), rule_maker.input_ptrs.size(),
                                    rule_maker.output_ptrs.data(), rule_maker.output_ptrs.size()),
            ge::GRAPH_SUCCESS);

  EXPECT_EQ(rule_maker.CheckEqual(), "");

  ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
  rtStreamDestroy(stream);
}

TEST_F(ShapeRuleOpST, ComplexRuleHorizontalJit) {
  RuleMaker rule_maker;
  int64_t s0 = 8;
  int64_t s1 = 24;
  const std::string rule = rule_maker.Input({"s0", "s1"}, {8, 24})
                               .Output({"s1+s0", "s1-s0", "s1*s0", "Div(s1,s0)", "Floor(Div(s1,3))", "Ceil(Div(s1,3))",
                                        "Pow(s0,2)", "Mod(s1,7)"},
                                       {s1 + s0, s1 - s0, s1 * s0, s1 / s0, s1 / 3, (s1 + 2) / 3, s0 * s0, s1 % 7})
                               .Str();
  auto cg = ShapeRuleOpGraph(rule, false);

  GeModelBuilder builder(cg);
  auto ge_root_model = builder.BuildGeRootModel();

  bg::ValueHolder::PopGraphFrame();  // 不需要BgTest自带的Frame
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model);
  ASSERT_NE(exe_graph, nullptr);

  auto model_executor = ModelV2Executor::Create(exe_graph, ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  ASSERT_EQ(model_executor->Load(), ge::GRAPH_SUCCESS);

  rtStream_t stream;
  ASSERT_EQ(rtStreamCreate(&stream, static_cast<int32_t>(RT_STREAM_PRIORITY_DEFAULT)), RT_ERROR_NONE);
  auto i1 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  ASSERT_EQ(model_executor->Execute({i1.value}, rule_maker.input_ptrs.data(), rule_maker.input_ptrs.size(),
                                    rule_maker.output_ptrs.data(), rule_maker.output_ptrs.size()),
            ge::GRAPH_SUCCESS);

  EXPECT_EQ(rule_maker.CheckEqual(), "");

  ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
  rtStreamDestroy(stream);
}

class CustomOpWithInferRuleST : public testing::Test {
 public:
  void SetUp() override {
    const auto env_ptr = getenv("LD_PRELOAD");
    if (env_ptr != nullptr) {
      env = env_ptr;
      unsetenv("LD_PRELOAD");
    }
    gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2();
  }
  void TearDown() override {
    if (!env.empty()) {
      setenv("LD_PRELOAD", env.c_str(), 1);
    }
  }
  std::string env;
};

TEST_F(CustomOpWithInferRuleST, CustomOpShapeInferenceByRule) {
  RegisterInferShapeKernels();
  const std::string rule = R"({"shape":{"inputs":[["s0"],["s1"],["s2"]],"outputs":[["s0"]]}})";
  std::vector<uint8_t> binary;
  if (ShapeInferenceRule::CompileJsonString(rule, binary) != ge::GRAPH_SUCCESS) {
    GTEST_SKIP() << "JIT compiler not available, skip test";
  }

  auto graph = ShareGraph::BuildCustomOpGraph();
  auto custom_op = graph->FindNode("custom_op");
  AttrUtils::SetStr(custom_op->GetOpDesc(), ge::ATTR_NAME_INFER_RULE, rule);
  AttrUtils::SetBytes(custom_op->GetOpDesc(), ge::COMPILED_INFERENCE_RULE_BINARY,
                      ge::Buffer::CopyFrom(binary.data(), binary.size()));
  graph->TopologicalSorting();

  CustomOpFactory::RegisterCustomOpCreator("CustomOp", []()->std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestBaseCustomOpWithInferShape>();
  });

  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  bg::ValueHolder::PopGraphFrame();
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model, {});
  ASSERT_NE(exe_graph, nullptr);

  // 在 Main 子图中查找 InferShapeByRule
  auto main_node = ge::ExecuteGraphUtils::FindFirstNodeMatchType(exe_graph.get(), "Main");
  ASSERT_NE(main_node, nullptr);
  auto main_graph = ge::FastNodeUtils::GetSubgraphFromNode(main_node, 0);
  ASSERT_NE(main_graph, nullptr);
  auto infer_shape_node = ge::ExecuteGraphUtils::FindFirstNodeMatchType(main_graph, "InferShapeByRule");
  ASSERT_NE(infer_shape_node, nullptr);

  auto model_executor = ModelV2Executor::Create(exe_graph, ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  ASSERT_EQ(model_executor->Load(), ge::GRAPH_SUCCESS);

  auto outputs = FakeTensors({2048}, 1);
  auto inputs = FakeTensors({2048}, 3);
  rtStream_t stream;
  ASSERT_EQ(rtStreamCreate(&stream, static_cast<int32_t>(RT_STREAM_PRIORITY_DEFAULT)), RT_ERROR_NONE);
  auto i3 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    reinterpret_cast<Tensor **>(outputs.GetAddrList()), outputs.size()),
            ge::GRAPH_SUCCESS);

  ASSERT_EQ(model_executor->UnLoad(), ge::GRAPH_SUCCESS);
  rtStreamDestroy(stream);
}
}  // namespace gert