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
#include "engine/custom/converter/custom_node_converter.h"
#include "common/share_graph.h"
#include "check/executor_statistician.h"
#include "framework/runtime/model_v2_executor.h"
#include "faker/ge_model_builder.h"
#include "lowering/model_converter.h"
#include "faker/fake_value.h"
#include "faker/kernel_run_context_facker.h"
#include "framework/runtime/executor_option/multi_thread_executor_option.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op.h"
#include "graph/custom_op_registry.h"
#include "core/executor/multi_thread_topological/executor/schedule/producer/task_producer_factory.h"
#include "core/executor/multi_thread_topological/executor/schedule/config/task_scheduler_config.h"
#include "core/executor/multi_thread_topological/execution_data/multi_thread_execution_data.h"
#include "operator_reg.h"
#include "common/executor_tracer_on.h"
#include "common/global_variables/diagnose_switch.h"
#include "graph/utils/inference_rule.h"
#include "graph/utils/execute_graph_utils.h"
#include "graph/debug/ge_attr_define.h"
#include "register/kernel_registry.h"
#include "register/kernel_registry_impl.h"
#include "exe_graph/runtime/extended_kernel_context.h"
#include "exe_graph/runtime/eager_op_execution_context.h"
#include "exe_graph/runtime/storage_shape.h"
#include "exe_graph/runtime/gert_tensor_data.h"
#include "framework/runtime/args_handler.h"
#include "kernel/common_kernel_impl/infer_shape.h"

using namespace ge;
using namespace gert::bg;

namespace gert {
namespace kernel {
class CustomNodeKernelUT : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};
class TestBaseCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto input_tensor0 = ctx->GetInputTensor(0);
    GE_ASSERT_NOTNULL(input_tensor0);
    auto input_shape0 = input_tensor0->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape0.GetDimNum() == 1);
    GE_ASSERT_TRUE(input_shape0.GetDim(0) == 2048);
    auto input_tensor1 = ctx->GetInputTensor(1);
    GE_ASSERT_NOTNULL(input_tensor1);
    auto input_shape1 = input_tensor1->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape1.GetDimNum() == 1);
    GE_ASSERT_TRUE(input_shape1.GetDim(0) == 2048);
    auto input_tensor2 = ctx->GetInputTensor(2);
    GE_ASSERT_NOTNULL(input_tensor2);
    auto input_shape2 = input_tensor2->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape2.GetDimNum() == 1);
    GE_ASSERT_TRUE(input_shape2.GetDim(0) == 2048);
    auto workspaces = ctx->MallocWorkSpace(1024);
    GE_ASSERT_NOTNULL(workspaces);
    auto output_tensor = ctx->MallocOutputTensor(0, StorageShape({2048}, {2048}),
                                                 StorageFormat(FORMAT_ND, FORMAT_ND, ExpandDimsType()), DT_FLOAT);
    GE_ASSERT_NOTNULL(output_tensor);
    return SUCCESS;
  }
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

    const gert::Tensor *output_desc_0 = ctx->GetOutputTensor(0);
    GE_ASSERT_NOTNULL(output_desc_0);
    gert::StorageShape out_shape_0 = output_desc_0->GetShape();
    GE_ASSERT_TRUE(out_shape_0.GetStorageShape().GetDimNum() == 1);
    GE_ASSERT_TRUE(out_shape_0.GetStorageShape().GetDim(0) == 2048);
    ge::DataType out_dtype_0 = output_desc_0->GetDataType();
    gert::StorageFormat out_format_0 = output_desc_0->GetFormat();
    gert::Tensor *ge_output_0 = ctx->MallocOutputTensor(0, out_shape_0, out_format_0, out_dtype_0);
    GE_ASSERT_NOTNULL(ge_output_0);
    return SUCCESS;
  }
};

class TestRegistryOnlyCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return SUCCESS;
  }
};

REG_OP(CustomOp)
    .INPUT(x1, TensorType::BasicType())
    .INPUT(x2, TensorType::BasicType())
    .INPUT(x3, TensorType::BasicType())
    .OUTPUT(y, TensorType::BasicType())
    .OP_END_FACTORY_REG(CustomOp)

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
      chain->SetWithDefaultDeleter(new (std::nothrow)
                                       Tensor(StorageShape(), output_desc->GetFormat(), output_desc->GetDataType()));
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

static ComputeGraphPtr BuildCustomOpGraphWithInferRule(const std::string &rule) {
  auto graph = ShareGraph::BuildCustomOpGraph();
  auto custom_op = graph->FindNode("custom_op");
  AttrUtils::SetStr(custom_op->GetOpDesc(), ge::ATTR_NAME_INFER_RULE, rule);
  std::vector<uint8_t> binary;
  if (ShapeInferenceRule::CompileJsonString(rule, binary) != ge::GRAPH_SUCCESS) {
    return nullptr;
  }
  AttrUtils::SetBytes(custom_op->GetOpDesc(), ge::COMPILED_INFERENCE_RULE_BINARY,
                      ge::Buffer::CopyFrom(binary.data(), binary.size()));
  graph->TopologicalSorting();
  return graph;
}

static CustomOpRegistryPtr BuildCustomOpRegistryForRt2(const BaseOpCreator &creator) {
  auto custom_op_registry = std::make_shared<CustomOpRegistry>();
  EXPECT_NE(custom_op_registry, nullptr);
  EXPECT_EQ(custom_op_registry->RegisterCreator("CustomOp", creator), GRAPH_SUCCESS);
  EXPECT_NE(custom_op_registry->CreateOrGetCustomOp("CustomOp"), nullptr);
  return custom_op_registry;
}

TEST_F(CustomNodeKernelUT, custom_op_kernel_execute_test) {
  auto graph = ShareGraph::BuildCustomOpGraph();
  graph->TopologicalSorting();
  auto custom_op_registry = BuildCustomOpRegistryForRt2(
      []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestBaseCustomOp>(); });
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  ge_root_model->SetCustomOpRegistry(custom_op_registry);
  bg::ValueHolder::PopGraphFrame();
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model, {});
  ASSERT_NE(exe_graph, nullptr);
  ASSERT_EQ(3, exe_graph->GetDirectNodesSize());
  TaskProducerFactory::GetInstance().SetProducerType(TaskProducerType::KERNEL);
  ASSERT_EQ(TaskProducerFactory::GetInstance().GetProducerType(), TaskProducerType::KERNEL);
  auto model_executor =
      ModelV2Executor::Create(exe_graph, ExecutorOption(ExecutorType::kTopologicalPriority), ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  ASSERT_EQ(model_executor->Load(), GRAPH_SUCCESS);

  auto outputs = FakeTensors({2048}, 1);
  auto inputs = FakeTensors({2048}, 3);
  rtStream_t stream;
  ASSERT_EQ(aclrtCreateStreamWithConfig(&stream, static_cast<uint32_t>(RT_STREAM_PRIORITY_DEFAULT), 0U), RT_ERROR_NONE);
  auto i3 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  auto ess = StartExecutorStatistician(model_executor);
  ess->Clear();
  ExecutorTracerOn executor_tracer_on;
  ge::diagnoseSwitch::EnableProfiling({gert::ProfilingType::kTaskTime, gert::ProfilingType::kDevice});
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    reinterpret_cast<Tensor **>(outputs.GetAddrList()), outputs.size()),
            GRAPH_SUCCESS);
  ge::diagnoseSwitch::DisableProfiling();
  aclrtDestroyStream(stream);
}

TEST_F(CustomNodeKernelUT, find_custom_op_uses_model_registry) {
  const std::string node_type = "RegistryOnlyCustomOpForRt2";
  auto custom_op_registry = std::make_shared<CustomOpRegistry>();
  ASSERT_EQ(custom_op_registry->RegisterCreator(
                node_type.c_str(),
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestRegistryOnlyCustomOp>(); }),
            GRAPH_SUCCESS);
  auto *expected_op = custom_op_registry->CreateOrGetCustomOp(node_type.c_str());
  ASSERT_NE(expected_op, nullptr);

  auto run_context = BuildKernelRunContext(3, 1);
  run_context.value_holder[0].Set(const_cast<char *>(node_type.c_str()), nullptr);
  run_context.value_holder[1].Set(custom_op_registry.get(), nullptr);
  run_context.value_holder[2].Set((void *)1, nullptr);

  auto find_func = KernelRegistry::GetInstance().FindKernelFuncs("FindCustomOp");
  ASSERT_NE(find_func, nullptr);
  ASSERT_EQ(find_func->run_func(run_context), GRAPH_SUCCESS);
  ASSERT_EQ(*run_context.GetContext<KernelContext>()->GetOutputPointer<BaseCustomOp *>(0), expected_op);
}

TEST_F(CustomNodeKernelUT, find_custom_op_does_not_fallback_to_global_registry) {
  const std::string node_type = "GlobalOnlyCustomOpForRt2";
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(
                node_type.c_str(),
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestRegistryOnlyCustomOp>(); }),
            GRAPH_SUCCESS);
  auto *global_op = CustomOpFactory::CreateOrGetCustomOp(node_type.c_str());
  ASSERT_NE(global_op, nullptr);

  auto custom_op_registry = std::make_shared<CustomOpRegistry>();
  auto run_context = BuildKernelRunContext(3, 1);
  run_context.value_holder[0].Set(const_cast<char *>(node_type.c_str()), nullptr);
  run_context.value_holder[1].Set(custom_op_registry.get(), nullptr);
  run_context.value_holder[2].Set((void *)1, nullptr);

  auto find_func = KernelRegistry::GetInstance().FindKernelFuncs("FindCustomOp");
  ASSERT_NE(find_func, nullptr);
  ASSERT_NE(find_func->run_func(run_context), GRAPH_SUCCESS);
}

TEST_F(CustomNodeKernelUT, find_custom_op_uses_global_registry) {
  const std::string node_type = "OnlineGlobalCustomOpForRt2";
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(
                node_type.c_str(),
                []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestRegistryOnlyCustomOp>(); }),
            GRAPH_SUCCESS);
  auto *global_op = CustomOpFactory::CreateOrGetCustomOp(node_type.c_str());
  ASSERT_NE(global_op, nullptr);

  auto run_context = BuildKernelRunContext(3, 1);
  run_context.value_holder[0].Set(const_cast<char *>(node_type.c_str()), nullptr);
  run_context.value_holder[1].Set(CustomOpFactory::GetGlobalRegistryPtr().get(), nullptr);
  run_context.value_holder[2].Set((void *)0, nullptr);

  auto find_func = KernelRegistry::GetInstance().FindKernelFuncs("FindCustomOp");
  ASSERT_NE(find_func, nullptr);
  ASSERT_EQ(find_func->run_func(run_context), GRAPH_SUCCESS);
  ASSERT_EQ(*run_context.GetContext<KernelContext>()->GetOutputPointer<BaseCustomOp *>(0), global_op);
}

TEST_F(CustomNodeKernelUT, custom_op_with_inference_rule_execute_test) {
  RegisterInferShapeKernels();
  const std::string rule = R"({"shape":{"inputs":[["s0"],["s1"],["s2"]],"outputs":[["s0"]]}})";
  auto graph = BuildCustomOpGraphWithInferRule(rule);
  if (graph == nullptr) {
    GTEST_SKIP() << "JIT compiler not available, skip test";
  }

  auto custom_op_registry = BuildCustomOpRegistryForRt2(
      []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestBaseCustomOpWithInferShape>(); });
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  ge_root_model->SetCustomOpRegistry(custom_op_registry);
  bg::ValueHolder::PopGraphFrame();
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model, {});
  ASSERT_NE(exe_graph, nullptr);

  TaskProducerFactory::GetInstance().SetProducerType(TaskProducerType::KERNEL);
  auto model_executor =
      ModelV2Executor::Create(exe_graph, ExecutorOption(ExecutorType::kTopologicalPriority), ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  ASSERT_EQ(model_executor->Load(), GRAPH_SUCCESS);

  auto outputs = FakeTensors({2048}, 1);
  auto inputs = FakeTensors({2048}, 3);
  rtStream_t stream;
  ASSERT_EQ(aclrtCreateStreamWithConfig(&stream, static_cast<uint32_t>(RT_STREAM_PRIORITY_DEFAULT), 0U), RT_ERROR_NONE);
  auto i3 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  auto ess = StartExecutorStatistician(model_executor);
  ess->Clear();
  ExecutorTracerOn executor_tracer_on;
  ge::diagnoseSwitch::EnableProfiling({gert::ProfilingType::kTaskTime, gert::ProfilingType::kDevice});
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    reinterpret_cast<Tensor **>(outputs.GetAddrList()), outputs.size()),
            GRAPH_SUCCESS);
  ge::diagnoseSwitch::DisableProfiling();
  aclrtDestroyStream(stream);
}

// 验证 CreateCustomOpOutputs 成功创建 EagerArgsHandler 并注册到正确的 output chain，
// chain 上设有 deleter，确保 resource guard 析构时能释放 args_handler
TEST_F(CustomNodeKernelUT, create_custom_op_outputs_success_args_handler_has_deleter) {
  const size_t node_output_num = 1U;
  const size_t kernel_output_num =
      node_output_num + static_cast<size_t>(EagerOpExecutionContext::AdditionalOutputIndex::kNum);  // 1 + 2 = 3
  auto run_context = KernelRunContextFaker()
                         .KernelIONum(0U, kernel_output_num)
                         .NodeIoNum(0U, node_output_num)
                         .NodeOutputTd(0, ge::DT_FLOAT, ge::FORMAT_ND, ge::FORMAT_ND)
                         .Build();
  auto *context = run_context.GetContext<KernelContext>();
  ASSERT_NE(context, nullptr);

  auto funcs = KernelRegistry::GetInstance().FindKernelFuncs("ExecuteCustomOp");
  ASSERT_NE(funcs, nullptr);
  ASSERT_NE(funcs->outputs_creator, nullptr);
  ASSERT_EQ(funcs->outputs_creator(nullptr, context), ge::GRAPH_SUCCESS);

  const size_t args_handler_idx =
      node_output_num + static_cast<size_t>(EagerOpExecutionContext::AdditionalOutputIndex::kArgsHandler);
  auto *args_chain = context->GetOutput(args_handler_idx);
  ASSERT_NE(args_chain, nullptr);
  // deleter 已注册，说明 Chain 析构时会 delete args_handler，不会泄漏
  EXPECT_TRUE(args_chain->HasDeleter());
  auto *args_handler = args_chain->GetValue<ArgsHandler *>();
  EXPECT_NE(args_handler, nullptr);
}

// 验证 CreateCustomOpOutputs 在缺少 args_handler output 槽位时返回失败，
// 且不会泄漏 args_handler（unique_ptr 在早退时自动释放）
TEST_F(CustomNodeKernelUT, create_custom_op_outputs_fails_without_args_handler_output) {
  const size_t node_output_num = 1U;
  // 只分配 workspace output，不分配 args_handler output
  const size_t kernel_output_num = node_output_num +
                                   static_cast<size_t>(EagerOpExecutionContext::AdditionalOutputIndex::kWorkSpace) +
                                   1U;  // 1 + 1 = 2
  auto run_context = KernelRunContextFaker()
                         .KernelIONum(0U, kernel_output_num)
                         .NodeIoNum(0U, node_output_num)
                         .NodeOutputTd(0, ge::DT_FLOAT, ge::FORMAT_ND, ge::FORMAT_ND)
                         .Build();
  auto *context = run_context.GetContext<KernelContext>();
  ASSERT_NE(context, nullptr);

  auto funcs = KernelRegistry::GetInstance().FindKernelFuncs("ExecuteCustomOp");
  ASSERT_NE(funcs, nullptr);
  ASSERT_NE(funcs->outputs_creator, nullptr);
  // GetOutput(args_handler_idx) 返回 null → GE_ASSERT_NOTNULL 失败 → 早退
  // unique_ptr 确保 args_handler 在早退时被 delete，不会泄漏
  EXPECT_NE(funcs->outputs_creator(nullptr, context), ge::GRAPH_SUCCESS);
}
}  // namespace kernel
}  // namespace gert
