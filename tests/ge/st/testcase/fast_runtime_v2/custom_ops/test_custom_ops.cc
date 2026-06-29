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
#include "framework/runtime/executor_option/multi_thread_executor_option.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op.h"
#include "core/executor/multi_thread_topological/executor/schedule/producer/task_producer_factory.h"
#include "core/executor/multi_thread_topological/executor/schedule/config/task_scheduler_config.h"
#include "core/executor/multi_thread_topological/execution_data/multi_thread_execution_data.h"
#include "operator_reg.h"
#include "common/executor_tracer_on.h"
#include "stub/gert_runtime_stub.h"
#include "depends/checker/mem_trace_checker.h"
#include "common/global_variables/diagnose_switch.h"
#include "engines/custom_engine/custom_graph_optimizer.h"

using namespace ge;
using namespace gert::bg;

namespace {
std::string MockCompile() {
  return "mock_compile_path";
}
}  // namespace
namespace gert {
namespace kernel {
class TestCustomNodeKernel : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

void *output_addr = nullptr;
uint32_t custom_shape_infer_count = 0U;
uint32_t custom_shape_infer_execute_count = 0U;

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
    auto output_shape = output_tensor->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(output_shape.GetDimNum() == 1);
    GE_ASSERT_TRUE(output_shape.GetDim(0) == 2048);
    output_addr = output_tensor->GetAddr();
    GE_ASSERT_NOTNULL(output_addr);
    return SUCCESS;
  }
};

class TestCompilableCustomOp : public EagerExecuteOp, CompilableOp {
 public:
  graphStatus Execute(EagerOpExecutionContext *ctx) override {
    return SUCCESS;
  }
  graphStatus Compile(OpCompileContext *ctx) override {
    auto input_tensor0 = ctx->GetInputTensor(0);
    GE_ASSERT_NOTNULL(input_tensor0);
    auto input_shape0 = input_tensor0->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape0.GetDimNum() == 1);
    GE_ASSERT_TRUE(input_shape0.GetDim(0) == 2048);
    GE_ASSERT_TRUE(input_tensor0->GetDataType() == DT_FLOAT);
    GE_ASSERT_TRUE(input_tensor0->GetStorageFormat() == FORMAT_ND);
    mock_compile_path_ = MockCompile();
    return SUCCESS;
  }

 private:
  std::string mock_compile_path_;
};

class TestMallocReadOnlyDevArgsCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    uint64_t host_args[4] = {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD};
    auto *dev_args = ctx->MallocReadOnlyDevArgs(host_args, sizeof(host_args));
    GE_ASSERT_NOTNULL(dev_args);
    GE_ASSERT_NOTNULL(dev_args->args_data);
    GE_ASSERT_EQ(dev_args->args_size, sizeof(host_args));
    GE_ASSERT_EQ(dev_args->placement, gert::Placement::kPlacementDevice);
    auto output_tensor = ctx->MallocOutputTensor(0, StorageShape({2048}, {2048}),
        StorageFormat(FORMAT_ND, FORMAT_ND, ExpandDimsType()), DT_FLOAT);
    GE_ASSERT_NOTNULL(output_tensor);
    output_addr = output_tensor->GetAddr();
    GE_ASSERT_NOTNULL(output_addr);
    return SUCCESS;
  }
};

class TestCustomOpWithShapeInfer : public EagerExecuteOp, public ShapeInferOp {
 public:
  graphStatus Execute(EagerOpExecutionContext *ctx) override {
    ++custom_shape_infer_execute_count;
    auto output_desc = ctx->GetOutputTensor(0);
    GE_ASSERT_NOTNULL(output_desc);
    auto output_shape = output_desc->GetStorageShape();
    GE_ASSERT_TRUE(output_shape.GetDimNum() == 1);
    GE_ASSERT_TRUE(output_shape.GetDim(0) == 2048);
    auto output_tensor =
        ctx->MallocOutputTensor(0, output_desc->GetShape(), output_desc->GetFormat(), output_desc->GetDataType());
    GE_ASSERT_NOTNULL(output_tensor);
    return SUCCESS;
  }

  graphStatus InferShape(InferShapeContext *ctx) override {
    ++custom_shape_infer_count;
    auto input = ctx->GetInputShape(0);
    auto output = ctx->GetOutputShape(0);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    *output = *input;
    return GRAPH_SUCCESS;
  }

  graphStatus InferDataType(InferDataTypeContext *ctx) override {
    return ctx->SetOutputDataType(0U, DT_FLOAT);
  }
};

class TestNoInputCompileOutputCustomOp : public EagerExecuteOp, CompilableOp {
 public:
  graphStatus Execute(EagerOpExecutionContext *ctx) override {
    return SUCCESS;
  }

  graphStatus Compile(OpCompileContext *ctx) override {
    const auto output0 = ctx->GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(output0);
    GE_ASSERT_TRUE(output0->GetShape().GetStorageShape() == gert::Shape({8, 16}));
    GE_ASSERT_TRUE(output0->GetDataType() == DT_FLOAT16);
    GE_ASSERT_TRUE(output0->GetStorageFormat() == FORMAT_ND);

    const auto output1 = ctx->GetOutputTensor(1U);
    GE_ASSERT_NOTNULL(output1);
    GE_ASSERT_TRUE(output1->GetShape().GetStorageShape() == gert::Shape({16, 16}));
    GE_ASSERT_TRUE(output1->GetDataType() == DT_FLOAT);

    const auto output2 = ctx->GetOutputTensor(2U);
    GE_ASSERT_NOTNULL(output2);
    GE_ASSERT_TRUE(output2->GetShape().GetStorageShape() == gert::Shape({32, 16}));
    GE_ASSERT_TRUE(output2->GetDataType() == DT_INT32);
    GE_ASSERT_TRUE(ctx->GetOutputTensor(3U) == nullptr);

    const auto required_output = ctx->GetRequiredOutputTensor(0U);
    GE_ASSERT_TRUE(required_output == output0);

    const auto dynamic_output0 = ctx->GetDynamicOutputTensor(1U, 0U);
    GE_ASSERT_TRUE(dynamic_output0 == output1);

    const auto dynamic_output1 = ctx->GetDynamicOutputTensor(1U, 1U);
    GE_ASSERT_TRUE(dynamic_output1 == output2);
    GE_ASSERT_TRUE(ctx->GetRequiredOutputTensor(2U) == nullptr);
    GE_ASSERT_TRUE(ctx->GetDynamicOutputTensor(1U, 2U) == nullptr);
    mock_compile_path_ = MockCompile();
    return SUCCESS;
  }

 private:
  std::string mock_compile_path_;
};

class TestEmptyOutputInstanceCompileCustomOp : public EagerExecuteOp, CompilableOp {
 public:
  graphStatus Execute(EagerOpExecutionContext *ctx) override {
    return SUCCESS;
  }

  graphStatus Compile(OpCompileContext *ctx) override {
    GE_ASSERT_NOTNULL(ctx);
    GE_ASSERT_TRUE(ctx->GetOutputTensor(0U) == nullptr);
    GE_ASSERT_TRUE(ctx->GetRequiredOutputTensor(0U) == nullptr);
    GE_ASSERT_TRUE(ctx->GetDynamicOutputTensor(0U, 0U) == nullptr);
    return SUCCESS;
  }
};

REG_OP(CustomOp)
  .INPUT(x1, TensorType::BasicType())
  .INPUT(x2, TensorType::BasicType())
  .INPUT(x3, TensorType::BasicType())
  .OUTPUT(y, TensorType::BasicType())
  .OP_END_FACTORY_REG(CustomOp)

REG_OP(NoInputCompileOutputCustomOp)
  .OUTPUT(y, TensorType::BasicType())
  .DYNAMIC_OUTPUT(dy, TensorType::BasicType())
  .OP_END_FACTORY_REG(NoInputCompileOutputCustomOp)

REG_OP(EmptyOutputInstanceCompileCustomOp)
  .OUTPUT(y, TensorType::BasicType())
  .OP_END_FACTORY_REG(EmptyOutputInstanceCompileCustomOp)

REG_OP(StCustomOpWithShapeInfer)
  .INPUT(x1, TensorType::BasicType())
  .INPUT(x2, TensorType::BasicType())
  .INPUT(x3, TensorType::BasicType())
  .OUTPUT(y, TensorType::BasicType())
  .OP_END_FACTORY_REG(StCustomOpWithShapeInfer)

REG_OP(MallocReadOnlyDevArgsCustomOp)
  .INPUT(x1, TensorType::BasicType())
  .INPUT(x2, TensorType::BasicType())
  .INPUT(x3, TensorType::BasicType())
  .OUTPUT(y, TensorType::BasicType())
  .OP_END_FACTORY_REG(MallocReadOnlyDevArgsCustomOp)

TEST_F(TestCustomNodeKernel, custom_op_kernel_execute_test) {
  auto graph = ShareGraph::BuildCustomOpGraph();
  graph->TopologicalSorting();
  CustomOpFactory::RegisterCustomOpCreator(
      "CustomOp", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestBaseCustomOp>(); });
  GertRuntimeStub runtime_stub;
  runtime_stub.GetKernelStub().StubTiling();
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
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
  ASSERT_EQ(aclrtCreateStreamWithConfig(&stream, static_cast<uint32_t>(RT_STREAM_PRIORITY_DEFAULT), 0), RT_ERROR_NONE);
  auto i3 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  auto ess = StartExecutorStatistician(model_executor);
  // 第一次执行，无缓存，全部算子调用tiling_func
  ess->Clear();
  // 打开info日志验证traceprinter
  ExecutorTracerOn executor_tracer_on;  // 开启trace
  ge::diagnoseSwitch::EnableProfiling({gert::ProfilingType::kTaskTime, gert::ProfilingType::kDevice});
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    reinterpret_cast<Tensor **>(outputs.GetAddrList()), outputs.size()),
            GRAPH_SUCCESS);
  EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType("CustomOp", "ExecuteCustomOp"), 1);
  EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType("CustomOp", "FreeCustomOpWorkspaces"), 1);
  EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType("CustomOp", "FreeArgsGuarder"), 1);
  EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType("CustomOp", "FreeMemory"), 1);
  EXPECT_TRUE(MemoryTraceChecker(runtime_stub.GetSlogStub(), output_addr)
                  .AppendExpectEvent(kAllocRe, 0)  // (1) alloc in stream 0
                  .AppendExpectEvent(kFreeRe, 0)   // (3) free on stream 0, trigger LocalRecycle
                  .AsYouWish());
  ge::diagnoseSwitch::DisableProfiling();
  aclrtDestroyStream(stream);
}

/**
 * 用例描述：验证无输入自定义编译算子可在Compile阶段获取输出Tensor
 * 预置条件：
 *   1. 注册一个无输入、包含1个required输出和2个dynamic输出实例的CompilableOp
 *   2. 构造仅包含该自定义算子的计算图
 * 测试步骤：
 *   1. 调用CustomGraphOptimizer执行自定义算子编译
 *   2. 在自定义算子的Compile函数中调用GetOutputTensor、GetRequiredOutputTensor和GetDynamicOutputTensor
 *   3. 调用上述接口的越界场景
 * 预期结果：
 *   1. 自定义算子编译成功
 *   2. Compile函数中可以获取到3个输出Tensor，shape、format、datatype符合OpDesc描述
 *   3. 越界访问返回nullptr
 */
TEST_F(TestCustomNodeKernel, no_input_custom_op_compile_get_output_tensor_success) {
  auto graph = std::make_shared<ge::ComputeGraph>("no_input_custom_op_compile_graph");
  auto op_desc = std::make_shared<ge::OpDesc>("custom_op", "NoInputCompileOutputCustomOp");
  op_desc->AppendIrOutput("y", ge::kIrOutputRequired);
  op_desc->AppendIrOutput("dy", ge::kIrOutputDynamic);

  ge::GeTensorDesc required_output_desc(ge::GeShape({8, 16}), FORMAT_ND, DT_FLOAT16);
  required_output_desc.SetOriginFormat(FORMAT_NCHW);
  op_desc->AddOutputDesc("y", required_output_desc);
  ge::GeTensorDesc dynamic_output_desc0(ge::GeShape({16, 16}), FORMAT_ND, DT_FLOAT);
  dynamic_output_desc0.SetOriginFormat(FORMAT_ND);
  op_desc->AddOutputDesc("dy0", dynamic_output_desc0);
  ge::GeTensorDesc dynamic_output_desc1(ge::GeShape({32, 16}), FORMAT_FRACTAL_NZ, DT_INT32);
  dynamic_output_desc1.SetOriginFormat(FORMAT_ND);
  op_desc->AddOutputDesc("dy1", dynamic_output_desc1);

  auto custom_node = graph->AddNode(op_desc);
  ASSERT_NE(custom_node, nullptr);
  CustomOpFactory::RegisterCustomOpCreator("NoInputCompileOutputCustomOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestNoInputCompileOutputCustomOp>();
  });

  ge::CustomGraphOptimizer optimizer;
  ASSERT_EQ(optimizer.OptimizeSubgraphPostProc(*graph), ge::GRAPH_SUCCESS);
}

/**
 * 用例描述：验证自定义编译算子输出Tensor获取接口在无ComputeNodeInfo时返回nullptr
 * 预置条件：
 *   1. 默认构造OpCompileContext，不挂接ComputeNodeInfo
 * 测试步骤：
 *   1. 调用GetOutputTensor、GetRequiredOutputTensor和GetDynamicOutputTensor
 * 预期结果：
 *   1. 三个接口均返回nullptr
 */
TEST_F(TestCustomNodeKernel, compile_context_output_getters_return_nullptr_without_compute_node_info) {
  OpCompileContext ctx{};
  EXPECT_EQ(ctx.GetOutputTensor(0U), nullptr);
  EXPECT_EQ(ctx.GetRequiredOutputTensor(0U), nullptr);
  EXPECT_EQ(ctx.GetDynamicOutputTensor(0U, 0U), nullptr);
}

/**
 * 用例描述：验证自定义编译算子输出Tensor获取接口在IR输出无实例时返回nullptr
 * 预置条件：
 *   1. 注册一个仅声明IR输出、但没有实际输出实例的CompilableOp
 *   2. 构造仅包含该自定义算子的计算图
 * 测试步骤：
 *   1. 调用CustomGraphOptimizer执行自定义算子编译
 *   2. 在自定义算子的Compile函数中调用GetOutputTensor、GetRequiredOutputTensor和GetDynamicOutputTensor
 * 预期结果：
 *   1. 自定义算子编译成功
 *   2. 三个接口均返回nullptr
 */
TEST_F(TestCustomNodeKernel, custom_op_compile_get_output_tensor_nullptr_without_output_instance) {
  auto graph = std::make_shared<ge::ComputeGraph>("empty_output_instance_compile_graph");
  auto op_desc = std::make_shared<ge::OpDesc>("custom_op", "EmptyOutputInstanceCompileCustomOp");
  op_desc->AppendIrOutput("y", ge::kIrOutputRequired);

  auto custom_node = graph->AddNode(op_desc);
  ASSERT_NE(custom_node, nullptr);
  CustomOpFactory::RegisterCustomOpCreator("EmptyOutputInstanceCompileCustomOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestEmptyOutputInstanceCompileCustomOp>();
  });

  ge::CustomGraphOptimizer optimizer;
  ASSERT_EQ(optimizer.OptimizeSubgraphPostProc(*graph), ge::GRAPH_SUCCESS);
}

TEST_F(TestCustomNodeKernel, custom_op_shape_infer_op_execute_test) {
  const char *const op_type = "StCustomOpWithShapeInfer";
  auto graph = ShareGraph::BuildCustomOpGraph();
  auto custom_op = graph->FindNode("custom_op");
  ASSERT_NE(custom_op, nullptr);
  custom_op->GetOpDesc()->SetType(op_type);
  graph->TopologicalSorting();
  ASSERT_EQ(
      CustomOpFactory::RegisterCustomOpCreator(
          op_type, []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestCustomOpWithShapeInfer>(); }),
      GRAPH_SUCCESS);

  GertRuntimeStub runtime_stub;
  runtime_stub.GetKernelStub().StubTiling();
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
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
  ASSERT_EQ(aclrtCreateStreamWithConfig(&stream, static_cast<uint32_t>(RT_STREAM_PRIORITY_DEFAULT), 0), RT_ERROR_NONE);
  auto i3 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  custom_shape_infer_count = 0U;
  custom_shape_infer_execute_count = 0U;
  auto ess = StartExecutorStatistician(model_executor);
  ess->Clear();
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    reinterpret_cast<Tensor **>(outputs.GetAddrList()), outputs.size()),
            GRAPH_SUCCESS);
  EXPECT_EQ(custom_shape_infer_count, 1U);
  EXPECT_EQ(custom_shape_infer_execute_count, 1U);
  // 这里没有单独的 "InferShape" kernel 事件；ShapeInferOp::InferShape 是在
  // ExecuteCustomOpWithInferShape 内部直接被调用的，所以统计应看执行 kernel。
  EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType(op_type, "ExecuteCustomOpWithInferShape"), 1);
  EXPECT_EQ(model_executor->UnLoad(), GRAPH_SUCCESS);
  aclrtDestroyStream(stream);
}

/**
 * 用例描述：验证自定义算子MallocReadOnlyDevArgs接口在V2运行时中正确分配设备侧只读args
 * 预置条件：
 *   1. 注册一个继承EagerExecuteOp的自定义算子，Execute中调用MallocReadOnlyDevArgs分配kernel args
 *   2. 构造包含该自定义算子的计算图，并通过ModelV2Executor加载执行
 * 测试步骤：
 *   1. 创建计算图并注册MallocReadOnlyDevArgsCustomOp算子
 *   2. 通过ModelV2Executor编译加载并执行
 *   3. 验证Execute/FreeArgsGuarder/FreeCustomOpWorkspaces kernel执行计数
 *   4. 通过MemoryTraceChecker验证输出Tensor内存的分配和释放事件
 *   5. 调用UnLoad卸载模型
 * 预期结果：
 *   1. 算子执行成功，各kernel执行计数符合预期
 *   2. 输出Tensor内存在stream 0上正确分配和释放
 *   3. 模型卸载成功
 */
TEST_F(TestCustomNodeKernel, custom_op_malloc_read_only_dev_args_test) {
  const char *const op_type = "MallocReadOnlyDevArgsCustomOp";
  auto graph = ShareGraph::BuildCustomOpGraph();
  auto custom_op = graph->FindNode("custom_op");
  ASSERT_NE(custom_op, nullptr);
  custom_op->GetOpDesc()->SetType(op_type);
  graph->TopologicalSorting();
  CustomOpFactory::RegisterCustomOpCreator(op_type, []()->std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestMallocReadOnlyDevArgsCustomOp>();
  });
  GertRuntimeStub runtime_stub;
  runtime_stub.GetKernelStub().StubTiling();
  GeModelBuilder builder(graph);
  auto ge_root_model = builder.BuildGeRootModel();
  bg::ValueHolder::PopGraphFrame();
  auto exe_graph = ModelConverter().ConvertGeModelToExecuteGraph(ge_root_model, {});
  ASSERT_NE(exe_graph, nullptr);
  TaskProducerFactory::GetInstance().SetProducerType(TaskProducerType::KERNEL);
  auto model_executor = ModelV2Executor::Create(exe_graph,
      ExecutorOption(ExecutorType::kTopologicalPriority), ge_root_model);
  ASSERT_NE(model_executor, nullptr);
  ASSERT_EQ(model_executor->Load(), GRAPH_SUCCESS);

  auto outputs = FakeTensors({2048}, 1);
  auto inputs = FakeTensors({2048}, 3);
  rtStream_t stream;
  ASSERT_EQ(aclrtCreateStreamWithConfig(&stream, static_cast<uint32_t>(RT_STREAM_PRIORITY_DEFAULT), 0), RT_ERROR_NONE);
  auto i3 = FakeValue<uint64_t>(reinterpret_cast<uint64_t>(stream));

  auto ess = StartExecutorStatistician(model_executor);
  ess->Clear();
  ExecutorTracerOn executor_tracer_on;  // 开启trace以验证内存事件
  ASSERT_EQ(model_executor->Execute({i3.value}, inputs.GetTensorList(), inputs.size(),
                                    reinterpret_cast<Tensor **>(outputs.GetAddrList()), outputs.size()),
            GRAPH_SUCCESS);
  EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType(op_type, "ExecuteCustomOp"), 1);
  EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType(op_type, "FreeArgsGuarder"), 1);
  EXPECT_EQ(ess->GetExecuteCountByNodeTypeAndKernelType(op_type, "FreeCustomOpWorkspaces"), 1);
  EXPECT_TRUE(MemoryTraceChecker(runtime_stub.GetSlogStub(), output_addr)
      .AppendExpectEvent(kAllocRe, 0)
      .AppendExpectEvent(kFreeRe, 0)
      .AsYouWish());
  EXPECT_EQ(model_executor->UnLoad(), GRAPH_SUCCESS);
  aclrtDestroyStream(stream);
}
}
}
