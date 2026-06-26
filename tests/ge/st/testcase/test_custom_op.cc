/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <memory>
#include <numeric>
#include <string>
#include "common/share_graph.h"
#include "faker/global_data_faker.h"
#include "faker/fake_value.h"
#include "rt_external_base.h"
#include "ge/ge_api.h"
#include "ge/ge_api_error_codes.h"
#include "ge/ge_graph_compile_summary.h"
#include "graph/execute/model_executor.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/load/model_manager/model_utils.h"
#include "ge_graph_dsl/assert/graph_assert.h"
#include "utils/mock_ops_kernel_builder.h"
#include "utils/taskdef_builder.h"
#include "stub/gert_runtime_stub.h"
#include "easy_graph/builder/graph_dsl.h"
#include "ge_graph_dsl/op_desc/op_desc_cfg_box.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "ge_running_env/ge_running_env_faker.h"
#include "ge_running_env/fake_op.h"
#include "utils/taskdef_builder.h"
#include "common/args_checker.h"
#include "graph/load/model_manager/model_manager.h"
#include "init_ge.h"
#include "common/opskernel/ops_kernel_info_store.h"
#include "graph/ge_local_context.h"
#include "graph/ge_global_options.h"
#include "utils/synchronizer.h"
#include "common/global_variables/diagnose_switch.h"
#include "hcom/hcom_topo_info.h"
#include "common/opskernel/ops_kernel_info_types.h"
#include "engines/custom_engine/custom_graph_optimizer.h"
#include "graph/custom_op_factory.h"
#include "graph/custom_op.h"

namespace ge {
using namespace gert;
namespace {
Status GenerateTaskForCustomOp(const Node &node, RunContext &run_context, std::vector<domi::TaskDef> &tasks) {
  (void)node;
  (void)run_context;
  domi::TaskDef task_def = {};
  task_def.set_stream_id(node.GetOpDesc()->GetStreamId());
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_sqe_num(5);

  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(node.GetOpDesc()->GetId());
  tasks.push_back(task_def);
  return SUCCESS;
}

Status GenerateTaskForMemCopyAync(const Node &node, RunContext &run_context, std::vector<domi::TaskDef> &tasks) {
  if ((node.GetType() != MEMCPYASYNC) && (node.GetType() != IDENTITY)) {
    return SUCCESS;
  }
  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_MEMCPY_ASYNC));
  auto kernel_def = task_def.mutable_memcpy_async();
  kernel_def->set_op_index(node.GetOpDesc()->GetId());
  kernel_def->set_kind(RT_MEMCPY_ADDR_DEVICE_TO_DEVICE);
  uint8_t *membase = run_context.dataMemBase;
  kernel_def->set_src((uintptr_t)membase + node.GetOpDesc()->GetInputOffset()[0]);
  kernel_def->set_dst((uintptr_t)membase + node.GetOpDesc()->GetOutputOffset()[0]);
  tasks.emplace_back(task_def);
  return SUCCESS;
}
void ConstructCustomInputOutputTensor(size_t input_num, size_t output_num, std::vector<ge::Tensor> &inputs,
                                      std::vector<ge::Tensor> &outputs) {
  for (size_t i = 0; i < input_num; i++) {
    std::vector<float32_t> input_data(2 * 2 * 2, 0);
    TensorDesc desc(Shape({2, 2, 2}));
    ge::Tensor input_tensor{desc};
    input_tensor.SetData(reinterpret_cast<uint8_t *>(input_data.data()), input_data.size() * sizeof(float32_t));
    inputs.emplace_back(input_tensor);
  }

  for (size_t i = 0; i < output_num; ++i) {
    std::vector<uint8_t> output_data_1(32, 0xff);
    TensorDesc output_desc_1(Shape({2, 2, 2}));
    ge::Tensor output_tensor_1{output_desc_1};
    output_tensor_1.SetData(output_data_1.data(), output_data_1.size());
    outputs.emplace_back(output_tensor_1);
  }
  return;
}
void MockGenerateTask() {
  auto aicore_func = [](const Node &node, RunContext &context, std::vector<domi::TaskDef> &tasks) -> Status {
    if (node.GetType() == CONSTANT) {
      return SUCCESS;
    }

    auto op_desc = node.GetOpDesc();
    op_desc->SetOpKernelLibName("AiCoreLib");
    ge::AttrUtils::SetStr(op_desc, ge::TVM_ATTR_NAME_MAGIC, "RT_DEV_BINARY_MAGIC_ELF");
    ge::AttrUtils::SetStr(op_desc, ge::ATTR_NAME_KERNEL_BIN_ID, op_desc->GetName() + "_fake_id");
    const char kernel_bin[] = "kernel_bin";
    vector<char> buffer(kernel_bin, kernel_bin + strlen(kernel_bin));
    ge::OpKernelBinPtr kernel_bin_ptr = std::make_shared<ge::OpKernelBin>("test", std::move(buffer));
    op_desc->SetExtAttr(ge::OP_EXTATTR_NAME_TBE_KERNEL, kernel_bin_ptr);
    size_t arg_size = 100;
    std::vector<uint8_t> args(arg_size, 0);
    domi::TaskDef task_def;
    task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_KERNEL));
    auto kernel_info = task_def.mutable_kernel();
    kernel_info->set_args(args.data(), args.size());
    kernel_info->set_args_size(arg_size);
    kernel_info->mutable_context()->set_kernel_type(static_cast<uint32_t>(ccKernelType::TE));
    kernel_info->set_kernel_name(node.GetName());
    kernel_info->set_block_dim(1);
    uint16_t args_offset[2] = {0};
    kernel_info->mutable_context()->set_args_offset(args_offset, 2 * sizeof(uint16_t));
    kernel_info->mutable_context()->set_op_index(node.GetOpDesc()->GetId());

    tasks.emplace_back(task_def);
    return SUCCESS;
  };

  auto rts_func = [](const Node &node, RunContext &context, std::vector<domi::TaskDef> &tasks) -> Status {
    return SUCCESS;
  };

  MockForGenerateTask("AiCoreLib", aicore_func);
  MockForGenerateTask("RTSLib", rts_func);
}
void *output_addr = nullptr;
void **args_table = nullptr;
}  // namespace

class CustomOpRefreshTest : public testing::Test {
 protected:
  void SetUp() {
    ModelManager::GetInstance().ClearAicpuSo();
    MockGenerateTask();
  }
  void TearDown() {
    OpsKernelBuilderRegistry::GetInstance().Unregister("AiCoreLib");
    OpsKernelBuilderRegistry::GetInstance().Unregister("RTSLib");
  }
};

class TestBaseCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto input_tensor0 = ctx->GetInputTensor(0);
    GE_ASSERT_NOTNULL(input_tensor0);
    auto input_shape0 = input_tensor0->GetShape().GetStorageShape();
    std::cout << "input shape dimnum " << input_shape0.GetDimNum() << std::endl;
    GE_ASSERT_TRUE(input_shape0.GetDimNum() == 3);
    GE_ASSERT_TRUE(input_shape0.GetDim(0) == 2);
    auto input_tensor1 = ctx->GetInputTensor(1);
    GE_ASSERT_NOTNULL(input_tensor1);
    auto input_shape1 = input_tensor1->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape1.GetDimNum() == 3);
    GE_ASSERT_TRUE(input_shape1.GetDim(0) == 2);
    auto input_tensor2 = ctx->GetInputTensor(2);
    GE_ASSERT_NOTNULL(input_tensor2);
    auto input_shape2 = input_tensor2->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape2.GetDimNum() == 3);
    GE_ASSERT_TRUE(input_shape2.GetDim(0) == 2);

    // allocator
    // 申请workspace有问题，taskinfo传入的是MemoryBlockManager但是在eager_op_execution_context里是按照GertAllocator来使用的
    auto workspaces = ctx->MallocWorkSpace(1024);
    GE_ASSERT_NOTNULL(workspaces);

    auto output_tensor = ctx->MallocOutputTensor(0, gert::StorageShape({2, 2, 2}, {2, 2, 2}),
                                                 gert::StorageFormat(FORMAT_ND, FORMAT_ND, ExpandDimsType()), DT_FLOAT);
    GE_ASSERT_NOTNULL(output_tensor);
    auto output_shape = output_tensor->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(output_shape.GetDimNum() == 3);
    GE_ASSERT_TRUE(output_shape.GetDim(0) == 2);
    output_addr = output_tensor->GetAddr();
    GE_ASSERT_NOTNULL(output_addr);

    rtSetTaskTag("custom_op");
    void *input_0 = const_cast<void *>(ctx->GetInputTensor(0)->GetAddr());
    void *input_1 = const_cast<void *>(ctx->GetInputTensor(1)->GetAddr());
    void *input_2 = const_cast<void *>(ctx->GetInputTensor(2)->GetAddr());
    void *output_0 = const_cast<void *>(ctx->GetOutputTensor(0)->GetAddr());
    args_table[0] = static_cast<void *>(input_0);
    args_table[1] = static_cast<void *>(input_1);
    args_table[2] = static_cast<void *>(input_2);
    args_table[3] = static_cast<void *>(output_0);

    aclrtLaunchKernelWithHostArgs(nullptr, 0, nullptr, nullptr, &args_table[0], 32, nullptr, 0);
    return SUCCESS;
  }
};

class TestCompileOutputCustomOp : public EagerExecuteOp, public CompilableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    return SUCCESS;
  }

  graphStatus Compile(gert::OpCompileContext *ctx) override {
    GE_ASSERT_NOTNULL(ctx);
    const auto required_output = ctx->GetRequiredOutputTensor(0U);
    GE_ASSERT_NOTNULL(required_output);
    GE_ASSERT_TRUE(required_output->GetShape().GetStorageShape() == gert::Shape({8, 16}));
    GE_ASSERT_TRUE(required_output->GetDataType() == DT_FLOAT16);
    GE_ASSERT_TRUE(required_output->GetStorageFormat() == FORMAT_ND);

    const auto dynamic_output0 = ctx->GetDynamicOutputTensor(1U, 0U);
    GE_ASSERT_NOTNULL(dynamic_output0);
    GE_ASSERT_TRUE(dynamic_output0->GetShape().GetStorageShape() == gert::Shape({16, 16}));
    GE_ASSERT_TRUE(dynamic_output0->GetDataType() == DT_FLOAT);

    const auto dynamic_output1 = ctx->GetDynamicOutputTensor(1U, 1U);
    GE_ASSERT_NOTNULL(dynamic_output1);
    GE_ASSERT_TRUE(dynamic_output1->GetShape().GetStorageShape() == gert::Shape({32, 16}));
    GE_ASSERT_TRUE(dynamic_output1->GetDataType() == DT_INT32);
    return SUCCESS;
  }
};

/**
 * 用例描述：fm外部设置，fm地址段不支持刷新，单次执行模型，args table正确
 *
 * 预置条件：
 * 1.构造计算图1，自定义算子直联Data和输出
 *  data0  data1  data2
 *     \    |      /
 *     \    |     /
 *       customop
 *          |
 *          |
 *       netoutput
 *
 * 测试步骤
 * 1.构造单个计算图1，设置fm地址段
 * 2.编译后执行计算图1
 * 3.判断argstable的一致性和正确性及args更新策略
 * 预期结果
 * 1.argstable的一致性和正确性均为成功，custom_op的args table为Execute流程分配，不走model args table的统一更新
 * 2.从dump图看产生了MemcpyAsyncTaskInfo
 */
TEST_F(CustomOpRefreshTest, model_execute_ok_with_customop_link_to_data) {
  MockForGenerateTask("DNN_VM_CUSTOM_OP_STORE", GenerateTaskForCustomOp);
  MockForGenerateTask("RTSLib", GenerateTaskForMemCopyAync);
  DUMP_GRAPH_WHEN("PreRunAfterBuild");

  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  gert::GertRuntimeStub runtime_stub;
  std::unique_ptr<ArgsChecker> args_checker;
  args_table = new void *[4];

  std::map<AscendString, AscendString> options;
  options.emplace(ge::OPTION_CONST_LIFECYCLE, "graph");
  options.emplace(ge::OPTION_GRAPH_RUN_MODE, "1");
  Session session(options);
  auto compute_graph = ShareGraph::BuildOnlyCustomOpKnowShapeGraph();
  auto graph = GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);

  CustomOpFactory::RegisterCustomOpCreator(
      "CustomOp", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestBaseCustomOp>(); });

  const auto infer_shape_func = [](gert::InferShapeContext *context) -> graphStatus {
    const auto input_shape = context->GetInputShape(0U);
    auto output = context->GetOutputShape(0);
    for (size_t dim = 0UL; dim < input_shape->GetDimNum(); dim++) {
      output->AppendDim(input_shape->GetDim(dim));
    }
    output->SetDimNum(input_shape->GetDimNum());
    return GRAPH_SUCCESS;
  };
  const auto infer_data_type_func = [](gert::InferDataTypeContext *context) -> graphStatus {
    const auto date_type = context->GetInputDataType(0U);
    EXPECT_EQ(context->SetOutputDataType(0, date_type), SUCCESS);
    return GRAPH_SUCCESS;
  };
  const auto infer_shape_range_func = [](gert::InferShapeRangeContext *context) -> graphStatus {
    auto input_shape_range = context->GetInputShapeRange(0U);
    auto output_shape_range = context->GetOutputShapeRange(0U);
    output_shape_range->SetMin(const_cast<gert::Shape *>(input_shape_range->GetMin()));
    output_shape_range->SetMax(const_cast<gert::Shape *>(input_shape_range->GetMax()));
    return GRAPH_SUCCESS;
  };

  gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2(true);
  auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  ASSERT_NE(space_registry, nullptr);
  auto op_impl_func = space_registry->CreateOrGetOpImpl("CustomOp");

  op_impl_func->infer_shape = infer_shape_func;
  op_impl_func->infer_datatype = infer_data_type_func;
  op_impl_func->infer_shape_range = infer_shape_range_func;
  op_impl_func->output_shape_depend_compute = 1UL;

  uint32_t graph_id = 1;
  session.AddGraph(graph_id, graph);
  auto ret = session.CompileGraph(graph_id);
  EXPECT_EQ(ret, SUCCESS);

  const CompiledGraphSummaryPtr summary = session.GetCompiledGraphSummary(graph_id);
  EXPECT_NE(summary, nullptr);
  size_t weight_size, feature_size;
  EXPECT_EQ(SUCCESS, summary->GetConstMemorySize(weight_size));
  EXPECT_EQ(SUCCESS, summary->GetFeatureMemorySize(feature_size));

  std::vector<std::pair<uint32_t, uint32_t>> io_indexes;
  EXPECT_EQ(summary->GetIOIndexesWithSameAddr(io_indexes), SUCCESS);
  EXPECT_EQ(io_indexes.size(), 0U);

  std::vector<uint8_t> weight_mem(weight_size, 0);
  std::vector<uint8_t> feature_mem(feature_size, 0);
  EXPECT_EQ(SUCCESS, session.SetGraphConstMemoryBase(graph_id, weight_mem.data(), weight_size));
  EXPECT_EQ(SUCCESS, session.UpdateGraphFeatureMemoryBase(graph_id, feature_mem.data(), feature_size));

  std::vector<ge::Tensor> inputs;
  std::vector<ge::Tensor> outputs;
  ConstructCustomInputOutputTensor(3, 1, inputs, outputs);
  ge::diagnoseSwitch::DisableDumper();
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs, outputs));

  CHECK_GRAPH(PreRunAfterBuild) {
    args_checker = std::make_unique<ArgsChecker>(graph, graph_id, session.GetSessionId(), runtime_stub);
  };

  EXPECT_EQ(SUCCESS, args_checker->SetFmAddr((uint64_t)feature_mem.data(), feature_size));
  EXPECT_EQ(SUCCESS, args_checker->SetModelInputAddr({0, 1, 2}, inputs));
  EXPECT_EQ(SUCCESS, args_checker->SetModelOutputAddr({0}, outputs));
  EXPECT_EQ(SUCCESS, args_checker->TaskIoAddressesAreCorrect());
  EXPECT_EQ(SUCCESS, args_checker->CheckNodesArgsNotUpdated({"custom_op"}));

  delete[] args_table;
  runtime_stub.Clear();
  mmSetEnv(kEnvValue, "", 1);
  ReInitGe();
}

/**
 * 用例描述：fm外部设置，fm地址段不支持刷新，单次执行模型，args table正确
 *
 * 预置条件：
 * 1.构造计算图1，自定义算子不在模型边界
 *
 *  data0  data1    data2  data3    data4  data5
 *     \    |         \     /          /    /
 *     \    |         \   /          /     /
 *         add0       add1          add2
 *              \       |         /
 *                \     |       /
 *                  customop           data6
 *                    |             /
 *                     |          /
 *                       add3
 *                        |
 *                      netoutput
 *
 *
 * 测试步骤
 * 1.构造单个计算图1，设置fm地址段
 * 2.编译后执行计算图1
 * 3.判断argstable的一致性和正确性及args更新策略
 * 预期结果
 * 1.argstable的一致性和正确性均为成功，custom_op的args table为Execute流程分配，不走model args table的统一更新
 * 2.从dump图看产生了未插入MemcpyAsyncTaskInfo
 */
TEST_F(CustomOpRefreshTest, model_execute_ok_with_customop_link_to_add) {
  MockForGenerateTask("DNN_VM_CUSTOM_OP_STORE", GenerateTaskForCustomOp);
  MockForGenerateTask("RTSLib", GenerateTaskForMemCopyAync);
  DUMP_GRAPH_WHEN("PreRunAfterBuild");

  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  gert::GertRuntimeStub runtime_stub;
  std::unique_ptr<ArgsChecker> args_checker;
  args_table = new void *[4];

  std::map<AscendString, AscendString> options;
  options.emplace(ge::OPTION_CONST_LIFECYCLE, "graph");
  options.emplace(ge::OPTION_GRAPH_RUN_MODE, "1");
  Session session(options);
  auto compute_graph = ShareGraph::BuildCustomOpWithAddKnowShapeGraph();
  auto graph = GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);

  CustomOpFactory::RegisterCustomOpCreator(
      "CustomOp", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestBaseCustomOp>(); });

  const auto infer_shape_func = [](gert::InferShapeContext *context) -> graphStatus {
    const auto input_shape = context->GetInputShape(0U);
    auto output = context->GetOutputShape(0);
    for (size_t dim = 0UL; dim < input_shape->GetDimNum(); dim++) {
      output->AppendDim(input_shape->GetDim(dim));
    }
    output->SetDimNum(input_shape->GetDimNum());
    return GRAPH_SUCCESS;
  };
  const auto infer_data_type_func = [](gert::InferDataTypeContext *context) -> graphStatus {
    const auto date_type = context->GetInputDataType(0U);
    EXPECT_EQ(context->SetOutputDataType(0, date_type), SUCCESS);
    return GRAPH_SUCCESS;
  };
  const auto infer_shape_range_func = [](gert::InferShapeRangeContext *context) -> graphStatus {
    auto input_shape_range = context->GetInputShapeRange(0U);
    auto output_shape_range = context->GetOutputShapeRange(0U);
    output_shape_range->SetMin(const_cast<gert::Shape *>(input_shape_range->GetMin()));
    output_shape_range->SetMax(const_cast<gert::Shape *>(input_shape_range->GetMax()));
    return GRAPH_SUCCESS;
  };

  gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2(true);
  auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  ASSERT_NE(space_registry, nullptr);
  auto op_impl_func = space_registry->CreateOrGetOpImpl("CustomOp");

  op_impl_func->infer_shape = infer_shape_func;
  op_impl_func->infer_datatype = infer_data_type_func;
  op_impl_func->infer_shape_range = infer_shape_range_func;
  op_impl_func->output_shape_depend_compute = 1UL;

  uint32_t graph_id = 1;
  session.AddGraph(graph_id, graph);
  auto ret = session.CompileGraph(graph_id);
  EXPECT_EQ(ret, SUCCESS);

  const CompiledGraphSummaryPtr summary = session.GetCompiledGraphSummary(graph_id);
  EXPECT_NE(summary, nullptr);
  size_t weight_size, feature_size;
  EXPECT_EQ(SUCCESS, summary->GetConstMemorySize(weight_size));
  EXPECT_EQ(SUCCESS, summary->GetFeatureMemorySize(feature_size));

  std::vector<std::pair<uint32_t, uint32_t>> io_indexes;
  EXPECT_EQ(summary->GetIOIndexesWithSameAddr(io_indexes), SUCCESS);
  EXPECT_EQ(io_indexes.size(), 0U);

  std::vector<uint8_t> weight_mem(weight_size, 0);
  std::vector<uint8_t> feature_mem(feature_size, 0);
  EXPECT_EQ(SUCCESS, session.SetGraphConstMemoryBase(graph_id, weight_mem.data(), weight_size));
  EXPECT_EQ(SUCCESS, session.UpdateGraphFeatureMemoryBase(graph_id, feature_mem.data(), feature_size));

  std::vector<ge::Tensor> inputs;
  std::vector<ge::Tensor> outputs;
  ConstructCustomInputOutputTensor(7, 1, inputs, outputs);
  ge::diagnoseSwitch::DisableDumper();
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs, outputs));

  CHECK_GRAPH(PreRunAfterBuild) {
    args_checker = std::make_unique<ArgsChecker>(graph, graph_id, session.GetSessionId(), runtime_stub);
  };

  EXPECT_EQ(SUCCESS, args_checker->SetFmAddr((uint64_t)feature_mem.data(), feature_size));
  EXPECT_EQ(SUCCESS, args_checker->SetModelInputAddr({0, 1, 2, 3, 4, 5, 6}, inputs));
  EXPECT_EQ(SUCCESS, args_checker->SetModelOutputAddr({0}, outputs));
  EXPECT_EQ(SUCCESS, args_checker->TaskIoAddressesAreCorrect());
  EXPECT_EQ(SUCCESS, args_checker->CheckNodesArgsNotUpdated({"custom_op"}));

  delete[] args_table;
  runtime_stub.Clear();
  mmSetEnv(kEnvValue, "", 1);
  ReInitGe();
}

/**
 * 用例描述：fm外部设置，fm地址段支持刷新，单次执行模型，args table正确
 *
 * 预置条件：
 * 1.构造计算图1，自定义算子不在模型边界
 *
 *  data0  data1    data2  data3    data4  data5
 *     \    |         \     /          /    /
 *     \    |         \   /          /     /
 *         add0       add1          add2
 *              \       |         /
 *                \     |       /
 *                  customop           data6
 *                    |             /
 *                     |          /
 *                       add3
 *                        |
 *                      netoutput
 *
 *
 * 测试步骤
 * 1.构造单个计算图1，设置fm地址段
 * 2.编译后执行计算图1
 * 3.判断argstable的一致性和正确性及args更新策略
 * 预期结果
 * 1.argstable的一致性和正确性均为成功，custom_op的args table为Execute流程分配，不走model args table的统一更新
 * 2.从dump图看插入MemcpyAsyncTaskInfo
 */
TEST_F(CustomOpRefreshTest, model_execute_ok_with_customop_link_to_add_and_fm_refresh) {
  MockForGenerateTask("DNN_VM_CUSTOM_OP_STORE", GenerateTaskForCustomOp);
  MockForGenerateTask("RTSLib", GenerateTaskForMemCopyAync);
  DUMP_GRAPH_WHEN("PreRunAfterBuild");

  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  gert::GertRuntimeStub runtime_stub;
  std::unique_ptr<ArgsChecker> args_checker;
  args_table = new void *[4];

  std::map<AscendString, AscendString> options;
  options.emplace(ge::OPTION_CONST_LIFECYCLE, "graph");
  options.emplace(ge::OPTION_FEATURE_BASE_REFRESHABLE, "1");
  options.emplace(ge::OPTION_GRAPH_RUN_MODE, "1");
  Session session(options);
  auto compute_graph = ShareGraph::BuildCustomOpWithAddKnowShapeGraph();
  auto graph = GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);

  CustomOpFactory::RegisterCustomOpCreator(
      "CustomOp", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestBaseCustomOp>(); });

  const auto infer_shape_func = [](gert::InferShapeContext *context) -> graphStatus {
    const auto input_shape = context->GetInputShape(0U);
    auto output = context->GetOutputShape(0);
    for (size_t dim = 0UL; dim < input_shape->GetDimNum(); dim++) {
      output->AppendDim(input_shape->GetDim(dim));
    }
    output->SetDimNum(input_shape->GetDimNum());
    return GRAPH_SUCCESS;
  };
  const auto infer_data_type_func = [](gert::InferDataTypeContext *context) -> graphStatus {
    const auto date_type = context->GetInputDataType(0U);
    EXPECT_EQ(context->SetOutputDataType(0, date_type), SUCCESS);
    return GRAPH_SUCCESS;
  };
  const auto infer_shape_range_func = [](gert::InferShapeRangeContext *context) -> graphStatus {
    auto input_shape_range = context->GetInputShapeRange(0U);
    auto output_shape_range = context->GetOutputShapeRange(0U);
    output_shape_range->SetMin(const_cast<gert::Shape *>(input_shape_range->GetMin()));
    output_shape_range->SetMax(const_cast<gert::Shape *>(input_shape_range->GetMax()));
    return GRAPH_SUCCESS;
  };

  gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2(true);
  auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  ASSERT_NE(space_registry, nullptr);
  auto op_impl_func = space_registry->CreateOrGetOpImpl("CustomOp");

  op_impl_func->infer_shape = infer_shape_func;
  op_impl_func->infer_datatype = infer_data_type_func;
  op_impl_func->infer_shape_range = infer_shape_range_func;
  op_impl_func->output_shape_depend_compute = 1UL;

  uint32_t graph_id = 1;
  session.AddGraph(graph_id, graph);
  auto ret = session.CompileGraph(graph_id);
  EXPECT_EQ(ret, SUCCESS);

  const CompiledGraphSummaryPtr summary = session.GetCompiledGraphSummary(graph_id);
  EXPECT_NE(summary, nullptr);
  size_t weight_size, feature_size;
  EXPECT_EQ(SUCCESS, summary->GetConstMemorySize(weight_size));
  EXPECT_EQ(SUCCESS, summary->GetFeatureMemorySize(feature_size));

  std::vector<std::pair<uint32_t, uint32_t>> io_indexes;
  EXPECT_EQ(summary->GetIOIndexesWithSameAddr(io_indexes), SUCCESS);
  EXPECT_EQ(io_indexes.size(), 0U);

  std::vector<uint8_t> weight_mem(weight_size, 0);
  std::vector<uint8_t> feature_mem(feature_size, 0);
  EXPECT_EQ(SUCCESS, session.SetGraphConstMemoryBase(graph_id, weight_mem.data(), weight_size));
  EXPECT_EQ(SUCCESS, session.UpdateGraphFeatureMemoryBase(graph_id, feature_mem.data(), feature_size));

  std::vector<ge::Tensor> inputs;
  std::vector<ge::Tensor> outputs;
  ConstructCustomInputOutputTensor(7, 1, inputs, outputs);
  ge::diagnoseSwitch::DisableDumper();
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs, outputs));

  CHECK_GRAPH(PreRunAfterBuild) {
    args_checker = std::make_unique<ArgsChecker>(graph, graph_id, session.GetSessionId(), runtime_stub);
  };

  EXPECT_EQ(SUCCESS, args_checker->SetFmAddr((uint64_t)feature_mem.data(), feature_size));
  EXPECT_EQ(SUCCESS, args_checker->SetModelInputAddr({0, 1, 2, 3, 4, 5, 6}, inputs));
  EXPECT_EQ(SUCCESS, args_checker->SetModelOutputAddr({0}, outputs));
  EXPECT_EQ(SUCCESS, args_checker->TaskIoAddressesAreCorrect());
  EXPECT_EQ(SUCCESS, args_checker->CheckNodesArgsNotUpdated({"custom_op"}));

  delete[] args_table;
  runtime_stub.Clear();
  mmSetEnv(kEnvValue, "", 1);
  ReInitGe();
}

/**
 * 用例描述：验证自定义编译算子编译上下文可构造输出Tensor
 * 预置条件：
 * 1. 注册一个包含1个required输出和2个dynamic输出实例的CompilableOp
 * 2. 构造仅包含该自定义算子的计算图
 * 测试步骤：
 * 1. 调用CustomGraphOptimizer执行自定义算子编译
 * 2. 在自定义算子的Compile函数中按IR输出读取输出Tensor
 * 预期结果：
 * 1. 自定义算子编译成功
 * 2. Compile函数中可以获取到3个输出Tensor，shape、format、datatype符合OpDesc描述
 */
TEST_F(CustomOpRefreshTest, custom_op_compile_context_construct_outputs_success) {
  const char *const op_type = "StCompileOutputCustomOp";
  auto graph = std::make_shared<ComputeGraph>("compile_output_custom_op_graph");
  auto op_desc = std::make_shared<OpDesc>("custom_op", op_type);
  op_desc->AppendIrOutput("y", kIrOutputRequired);
  op_desc->AppendIrOutput("dy", kIrOutputDynamic);

  GeTensorDesc required_output_desc(GeShape({8, 16}), FORMAT_ND, DT_FLOAT16);
  required_output_desc.SetOriginFormat(FORMAT_NCHW);
  ASSERT_EQ(op_desc->AddOutputDesc("y", required_output_desc), GRAPH_SUCCESS);
  GeTensorDesc dynamic_output_desc0(GeShape({16, 16}), FORMAT_ND, DT_FLOAT);
  dynamic_output_desc0.SetOriginFormat(FORMAT_ND);
  ASSERT_EQ(op_desc->AddOutputDesc("dy0", dynamic_output_desc0), GRAPH_SUCCESS);
  GeTensorDesc dynamic_output_desc1(GeShape({32, 16}), FORMAT_FRACTAL_NZ, DT_INT32);
  dynamic_output_desc1.SetOriginFormat(FORMAT_ND);
  ASSERT_EQ(op_desc->AddOutputDesc("dy1", dynamic_output_desc1), GRAPH_SUCCESS);

  ASSERT_NE(graph->AddNode(op_desc), nullptr);
  ASSERT_EQ(
      CustomOpFactory::RegisterCustomOpCreator(
          op_type, []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestCompileOutputCustomOp>(); }),
      GRAPH_SUCCESS);

  CustomGraphOptimizer optimizer;
  ASSERT_EQ(optimizer.OptimizeSubgraphPostProc(*graph), GRAPH_SUCCESS);
}

class TestArgsUpdaterCustomOp : public ArgsUpdater, public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto input_tensor0 = ctx->GetInputTensor(0);
    GE_ASSERT_NOTNULL(input_tensor0);
    auto input_shape0 = input_tensor0->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape0.GetDimNum() == 3);
    GE_ASSERT_TRUE(input_shape0.GetDim(0) == 2);
    auto input_tensor1 = ctx->GetInputTensor(1);
    GE_ASSERT_NOTNULL(input_tensor1);
    auto input_shape1 = input_tensor1->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape1.GetDimNum() == 3);
    GE_ASSERT_TRUE(input_shape1.GetDim(0) == 2);
    auto input_tensor2 = ctx->GetInputTensor(2);
    GE_ASSERT_NOTNULL(input_tensor2);
    auto input_shape2 = input_tensor2->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(input_shape2.GetDimNum() == 3);
    GE_ASSERT_TRUE(input_shape2.GetDim(0) == 2);

    auto workspaces = ctx->MallocWorkSpace(1024);
    GE_ASSERT_NOTNULL(workspaces);

    auto output_tensor = ctx->MallocOutputTensor(0, gert::StorageShape({2, 2, 2}, {2, 2, 2}),
                                                 gert::StorageFormat(FORMAT_ND, FORMAT_ND, ExpandDimsType()), DT_FLOAT);
    GE_ASSERT_NOTNULL(output_tensor);
    auto output_shape = output_tensor->GetShape().GetStorageShape();
    GE_ASSERT_TRUE(output_shape.GetDimNum() == 3);
    GE_ASSERT_TRUE(output_shape.GetDim(0) == 2);
    output_addr = output_tensor->GetAddr();
    GE_ASSERT_NOTNULL(output_addr);

    rtSetTaskTag("custom_op");
    void *input_0 = const_cast<void *>(ctx->GetInputTensor(0)->GetAddr());
    void *input_1 = const_cast<void *>(ctx->GetInputTensor(1)->GetAddr());
    void *input_2 = const_cast<void *>(ctx->GetInputTensor(2)->GetAddr());
    void *output_0 = const_cast<void *>(ctx->GetOutputTensor(0)->GetAddr());
    args_table[0] = static_cast<void *>(input_0);
    args_table[1] = static_cast<void *>(input_1);
    args_table[2] = static_cast<void *>(input_2);
    args_table[3] = static_cast<void *>(output_0);

    aclrtLaunchKernelWithHostArgs(nullptr, 0, nullptr, nullptr, &args_table[0], 32, nullptr, 0);
    return SUCCESS;
  }

  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    return GRAPH_SUCCESS;
  }
};

/**
 * 用例描述：fm地址段支持刷新，ArgsUpdater算子端到端执行，args table被刷新
 *
 * 预置条件：
 * 1.构造计算图1，自定义ArgsUpdater算子直联Data和输出
 *  data0  data1  data2
 *     \    |      /
 *     \    |     /
 *       ArgsUpdaterOp
 *          |
 *          |
 *       netoutput
 *
 * 测试步骤
 * 1.构造单个计算图1，设置fm地址段且支持刷新
 * 2.注册ArgsUpdater类型算子（继承ArgsUpdater+EagerExecuteOp）
 * 3.编译后执行计算图1
 * 4.判断argstable的一致性和正确性及args更新策略
 * 预期结果
 * 1.argstable的一致性和正确性均为成功
 * 2.ArgsUpdater算子的args table通过预留段分配，走model args table的统一更新(H2D memcpy)
 * 3.CheckNodesArgsUpdated验证custom_op的args被刷新
 */
TEST_F(CustomOpRefreshTest, args_updater_end_to_end_with_fm_refresh) {
  MockForGenerateTask("DNN_VM_CUSTOM_OP_STORE", GenerateTaskForCustomOp);
  MockForGenerateTask("RTSLib", GenerateTaskForMemCopyAync);
  DUMP_GRAPH_WHEN("PreRunAfterBuild");

  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  gert::GertRuntimeStub runtime_stub;
  std::unique_ptr<ArgsChecker> args_checker;
  args_table = new void *[4];

  std::map<AscendString, AscendString> options;
  options.emplace(ge::OPTION_CONST_LIFECYCLE, "graph");
  options.emplace(ge::OPTION_FEATURE_BASE_REFRESHABLE, "1");
  options.emplace(ge::OPTION_GRAPH_RUN_MODE, "1");
  Session session(options);
  auto compute_graph = ShareGraph::BuildOnlyCustomOpKnowShapeGraph();
  auto custom_op_node = compute_graph->FindNode("custom_op");
  custom_op_node->GetOpDesc()->SetType("ArgsUpdaterOp");
  auto graph = GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);

  CustomOpFactory::RegisterCustomOpCreator(
      "ArgsUpdaterOp", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestArgsUpdaterCustomOp>(); });

  const auto infer_shape_func = [](gert::InferShapeContext *context) -> graphStatus {
    const auto input_shape = context->GetInputShape(0U);
    auto output = context->GetOutputShape(0);
    for (size_t dim = 0UL; dim < input_shape->GetDimNum(); dim++) {
      output->AppendDim(input_shape->GetDim(dim));
    }
    output->SetDimNum(input_shape->GetDimNum());
    return GRAPH_SUCCESS;
  };
  const auto infer_data_type_func = [](gert::InferDataTypeContext *context) -> graphStatus {
    const auto date_type = context->GetInputDataType(0U);
    EXPECT_EQ(context->SetOutputDataType(0, date_type), SUCCESS);
    return GRAPH_SUCCESS;
  };
  const auto infer_shape_range_func = [](gert::InferShapeRangeContext *context) -> graphStatus {
    auto input_shape_range = context->GetInputShapeRange(0U);
    auto output_shape_range = context->GetOutputShapeRange(0U);
    output_shape_range->SetMin(const_cast<gert::Shape *>(input_shape_range->GetMin()));
    output_shape_range->SetMax(const_cast<gert::Shape *>(input_shape_range->GetMax()));
    return GRAPH_SUCCESS;
  };

  gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2(true);
  auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  ASSERT_NE(space_registry, nullptr);
  auto op_impl_func = space_registry->CreateOrGetOpImpl("ArgsUpdaterOp");

  op_impl_func->infer_shape = infer_shape_func;
  op_impl_func->infer_datatype = infer_data_type_func;
  op_impl_func->infer_shape_range = infer_shape_range_func;
  op_impl_func->output_shape_depend_compute = 1UL;

  uint32_t graph_id = 1;
  session.AddGraph(graph_id, graph);
  auto ret = session.CompileGraph(graph_id);
  EXPECT_EQ(ret, SUCCESS);

  const CompiledGraphSummaryPtr summary = session.GetCompiledGraphSummary(graph_id);
  EXPECT_NE(summary, nullptr);
  size_t weight_size, feature_size;
  EXPECT_EQ(SUCCESS, summary->GetConstMemorySize(weight_size));
  EXPECT_EQ(SUCCESS, summary->GetFeatureMemorySize(feature_size));

  std::vector<std::pair<uint32_t, uint32_t>> io_indexes;
  EXPECT_EQ(summary->GetIOIndexesWithSameAddr(io_indexes), SUCCESS);
  EXPECT_EQ(io_indexes.size(), 0U);

  std::vector<uint8_t> weight_mem(weight_size, 0);
  std::vector<uint8_t> feature_mem(feature_size, 0);
  EXPECT_EQ(SUCCESS, session.SetGraphConstMemoryBase(graph_id, weight_mem.data(), weight_size));
  EXPECT_EQ(SUCCESS, session.UpdateGraphFeatureMemoryBase(graph_id, feature_mem.data(), feature_size));

  std::vector<ge::Tensor> inputs;
  std::vector<ge::Tensor> outputs;
  ConstructCustomInputOutputTensor(3, 1, inputs, outputs);
  ge::diagnoseSwitch::DisableDumper();
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs, outputs));

  CHECK_GRAPH(PreRunAfterBuild) {
    args_checker = std::make_unique<ArgsChecker>(graph, graph_id, session.GetSessionId(), runtime_stub);
  };

  EXPECT_EQ(SUCCESS, args_checker->SetFmAddr((uint64_t)feature_mem.data(), feature_size));
  EXPECT_EQ(SUCCESS, args_checker->SetModelInputAddr({0, 1, 2}, inputs));
  EXPECT_EQ(SUCCESS, args_checker->SetModelOutputAddr({0}, outputs));
  EXPECT_EQ(SUCCESS, args_checker->TaskIoAddressesAreCorrect());

  EXPECT_TRUE(CustomOpFactory::IsAddressRefreshable(AscendString("ArgsUpdaterOp")));

  delete[] args_table;
  runtime_stub.Clear();
  mmSetEnv(kEnvValue, "", 1);
  ReInitGe();
}

class TestArgsUpdaterWithMallocCustomOp : public ArgsUpdater, public EagerExecuteOp {
 public:
  static int update_host_args_count_;

  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto input_tensor0 = ctx->GetInputTensor(0);
    GE_ASSERT_NOTNULL(input_tensor0);
    auto input_tensor1 = ctx->GetInputTensor(1);
    GE_ASSERT_NOTNULL(input_tensor1);
    auto input_tensor2 = ctx->GetInputTensor(2);
    GE_ASSERT_NOTNULL(input_tensor2);

    auto output_tensor = ctx->MallocOutputTensor(0, gert::StorageShape({2, 2, 2}, {2, 2, 2}),
                                                 gert::StorageFormat(FORMAT_ND, FORMAT_ND, ExpandDimsType()), DT_FLOAT);
    GE_ASSERT_NOTNULL(output_tensor);

    void *input_0 = const_cast<void *>(input_tensor0->GetAddr());
    void *input_1 = const_cast<void *>(input_tensor1->GetAddr());
    void *input_2 = const_cast<void *>(input_tensor2->GetAddr());
    void *output_0 = const_cast<void *>(ctx->GetOutputTensor(0)->GetAddr());

    uint64_t host_args[4] = {reinterpret_cast<uint64_t>(input_0), reinterpret_cast<uint64_t>(input_1),
                             reinterpret_cast<uint64_t>(input_2), reinterpret_cast<uint64_t>(output_0)};

    auto *dev_args = ctx->MallocReadOnlyDevArgs(host_args, sizeof(host_args));
    GE_ASSERT_NOTNULL(dev_args);

    rtSetTaskTag("custom_op");
    args_table[0] = input_0;
    args_table[1] = input_1;
    args_table[2] = input_2;
    args_table[3] = output_0;
    aclrtLaunchKernelWithHostArgs(nullptr, 0, nullptr, nullptr, &args_table[0], 32, nullptr, 0);
    return SUCCESS;
  }

  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    update_host_args_count_++;

    auto *input_tensor = ctx->GetInputTensor(0);
    auto *output_tensor = ctx->GetOutputTensor(0);
    auto *host_args = ctx->GetKernelArgs(gert::Placement::kPlacementHost, 0);

    if (host_args != nullptr && host_args->args_size >= sizeof(uint64_t) * 4 && input_tensor != nullptr &&
        output_tensor != nullptr) {
      auto *args = static_cast<uint64_t *>(host_args->args_data);
      args[0] = reinterpret_cast<uint64_t>(input_tensor->GetData<void>());
      args[3] = reinterpret_cast<uint64_t>(output_tensor->GetData<void>());
    }

    return GRAPH_SUCCESS;
  }
};
int TestArgsUpdaterWithMallocCustomOp::update_host_args_count_ = 0;

class TestArgsUpdaterMultiAllocCustomOp : public ArgsUpdater, public EagerExecuteOp {
 public:
  static int update_host_args_count_;

  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto output_tensor = ctx->MallocOutputTensor(0, gert::StorageShape({2, 2, 2}, {2, 2, 2}),
                                                 gert::StorageFormat(FORMAT_ND, FORMAT_ND, ExpandDimsType()), DT_FLOAT);
    GE_ASSERT_NOTNULL(output_tensor);

    for (int i = 0; i < 5; i++) {
      uint64_t host_args[8] = {static_cast<uint64_t>(i), 0, 0, 0, 0, 0, 0, 0};
      auto *dev_args = ctx->MallocReadOnlyDevArgs(host_args, sizeof(host_args));
      GE_ASSERT_NOTNULL(dev_args);
    }

    rtSetTaskTag("custom_op");
    void *input_0 = const_cast<void *>(ctx->GetInputTensor(0)->GetAddr());
    void *input_1 = const_cast<void *>(ctx->GetInputTensor(1)->GetAddr());
    void *input_2 = const_cast<void *>(ctx->GetInputTensor(2)->GetAddr());
    void *output_0 = const_cast<void *>(ctx->GetOutputTensor(0)->GetAddr());
    args_table[0] = input_0;
    args_table[1] = input_1;
    args_table[2] = input_2;
    args_table[3] = output_0;
    aclrtLaunchKernelWithHostArgs(nullptr, 0, nullptr, nullptr, &args_table[0], 32, nullptr, 0);
    return SUCCESS;
  }

  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    update_host_args_count_++;
    auto *host_args = ctx->GetKernelArgs(gert::Placement::kPlacementHost, 0);
    (void)host_args;
    return GRAPH_SUCCESS;
  }
};
int TestArgsUpdaterMultiAllocCustomOp::update_host_args_count_ = 0;

class TestEagerOnlyWithMallocCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    auto output_tensor = ctx->MallocOutputTensor(0, gert::StorageShape({2, 2, 2}, {2, 2, 2}),
                                                 gert::StorageFormat(FORMAT_ND, FORMAT_ND, ExpandDimsType()), DT_FLOAT);
    GE_ASSERT_NOTNULL(output_tensor);

    uint64_t host_args[4] = {0xAAAA, 0xBBBB, 0xCCCC, 0xDDDD};
    auto *dev_args = ctx->MallocReadOnlyDevArgs(host_args, sizeof(host_args));
    GE_ASSERT_NOTNULL(dev_args);

    rtSetTaskTag("custom_op");
    void *input_0 = const_cast<void *>(ctx->GetInputTensor(0)->GetAddr());
    void *input_1 = const_cast<void *>(ctx->GetInputTensor(1)->GetAddr());
    void *input_2 = const_cast<void *>(ctx->GetInputTensor(2)->GetAddr());
    void *output_0 = const_cast<void *>(ctx->GetOutputTensor(0)->GetAddr());
    args_table[0] = input_0;
    args_table[1] = input_1;
    args_table[2] = input_2;
    args_table[3] = output_0;
    aclrtLaunchKernelWithHostArgs(nullptr, 0, nullptr, nullptr, &args_table[0], 32, nullptr, 0);
    return SUCCESS;
  }
};

/**
 * 用例描述：ArgsUpdater算子完整生命周期：MallocReadOnlyDevArgs分配kernel args + 两轮执行触发UpdateHostArgs
 *
 * 预置条件：
 * 1. 构造计算图，ArgsUpdater算子直联Data和输出，FM支持刷新
 *  data0  data1  data2
 *     \    |      /
 *     \    |     /
 *    ArgsUpdaterLifecycleOp
 *          |
 *       netoutput
 *
 * 测试步骤：
 * 1. 注册ArgsUpdater算子，Execute中调用MallocReadOnlyDevArgs分配kernel args
 * 2. 编译并第一轮执行（触发Distribute + IntegrateCustomOpArgs + 预留段分配）
 * 3. 使用不同输入地址进行第二轮执行（触发UpdateForExecute + UpdateHostArgs回调）
 * 预期结果：
 * 1. 两轮执行均成功
 * 2. 第二轮执行触发UpdateHostArgs回调（计数 > 0）
 * 3. ArgsUpdater算子走预留段分配路径，args通过统一H2D刷新
 */
TEST_F(CustomOpRefreshTest, args_updater_lifecycle_with_malloc_and_two_rounds) {
  MockForGenerateTask("DNN_VM_CUSTOM_OP_STORE", GenerateTaskForCustomOp);
  MockForGenerateTask("RTSLib", GenerateTaskForMemCopyAync);
  DUMP_GRAPH_WHEN("PreRunAfterBuild");

  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  gert::GertRuntimeStub runtime_stub;
  args_table = new void *[4];
  TestArgsUpdaterWithMallocCustomOp::update_host_args_count_ = 0;

  std::map<AscendString, AscendString> options;
  options.emplace(ge::OPTION_CONST_LIFECYCLE, "graph");
  options.emplace(ge::OPTION_FEATURE_BASE_REFRESHABLE, "1");
  options.emplace(ge::OPTION_GRAPH_RUN_MODE, "1");
  Session session(options);
  auto compute_graph = ShareGraph::BuildOnlyCustomOpKnowShapeGraph();
  auto custom_op_node = compute_graph->FindNode("custom_op");
  custom_op_node->GetOpDesc()->SetType("ArgsUpdaterLifecycleOp");
  auto graph = GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);

  CustomOpFactory::RegisterCustomOpCreator("ArgsUpdaterLifecycleOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterWithMallocCustomOp>();
  });

  const auto infer_shape_func = [](gert::InferShapeContext *context) -> graphStatus {
    const auto input_shape = context->GetInputShape(0U);
    auto output = context->GetOutputShape(0);
    for (size_t dim = 0UL; dim < input_shape->GetDimNum(); dim++) {
      output->AppendDim(input_shape->GetDim(dim));
    }
    output->SetDimNum(input_shape->GetDimNum());
    return GRAPH_SUCCESS;
  };
  const auto infer_data_type_func = [](gert::InferDataTypeContext *context) -> graphStatus {
    const auto date_type = context->GetInputDataType(0U);
    EXPECT_EQ(context->SetOutputDataType(0, date_type), SUCCESS);
    return GRAPH_SUCCESS;
  };
  const auto infer_shape_range_func = [](gert::InferShapeRangeContext *context) -> graphStatus {
    auto input_shape_range = context->GetInputShapeRange(0U);
    auto output_shape_range = context->GetOutputShapeRange(0U);
    output_shape_range->SetMin(const_cast<gert::Shape *>(input_shape_range->GetMin()));
    output_shape_range->SetMax(const_cast<gert::Shape *>(input_shape_range->GetMax()));
    return GRAPH_SUCCESS;
  };

  gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2(true);
  auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  ASSERT_NE(space_registry, nullptr);
  auto op_impl_func = space_registry->CreateOrGetOpImpl("ArgsUpdaterLifecycleOp");
  op_impl_func->infer_shape = infer_shape_func;
  op_impl_func->infer_datatype = infer_data_type_func;
  op_impl_func->infer_shape_range = infer_shape_range_func;
  op_impl_func->output_shape_depend_compute = 1UL;

  uint32_t graph_id = 1;
  session.AddGraph(graph_id, graph);
  EXPECT_EQ(session.CompileGraph(graph_id), SUCCESS);

  const CompiledGraphSummaryPtr summary = session.GetCompiledGraphSummary(graph_id);
  EXPECT_NE(summary, nullptr);
  size_t weight_size, feature_size;
  EXPECT_EQ(SUCCESS, summary->GetConstMemorySize(weight_size));
  EXPECT_EQ(SUCCESS, summary->GetFeatureMemorySize(feature_size));

  std::vector<uint8_t> weight_mem(weight_size, 0);
  std::vector<uint8_t> feature_mem(feature_size, 0);
  EXPECT_EQ(SUCCESS, session.SetGraphConstMemoryBase(graph_id, weight_mem.data(), weight_size));
  EXPECT_EQ(SUCCESS, session.UpdateGraphFeatureMemoryBase(graph_id, feature_mem.data(), feature_size));

  std::vector<ge::Tensor> inputs1;
  std::vector<ge::Tensor> outputs1;
  ConstructCustomInputOutputTensor(3, 1, inputs1, outputs1);
  ge::diagnoseSwitch::DisableDumper();
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs1, outputs1));

  std::vector<ge::Tensor> inputs2;
  std::vector<ge::Tensor> outputs2;
  ConstructCustomInputOutputTensor(3, 1, inputs2, outputs2);
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs2, outputs2));

  EXPECT_GT(TestArgsUpdaterWithMallocCustomOp::update_host_args_count_, 0);

  delete[] args_table;
  runtime_stub.Clear();
  mmSetEnv(kEnvValue, "", 1);
  ReInitGe();
}

/**
 * 用例描述：ArgsUpdater算子多次MallocReadOnlyDevArgs，预留段耗尽后回退到Extra Pool
 *
 * 预置条件：
 * 1. 构造计算图，ArgsUpdater算子直联Data和输出，FM支持刷新
 * 2. 预留段大小 = (3 input + 1 output + 16 reserved) * 8 = 160 bytes
 * 3. 算子Execute中调用5次MallocReadOnlyDevArgs，每次64 bytes (总计320 > 160)
 *    调用1-3: 预留段分配 (192 > 160, 第3次溢出)
 *    调用3: 回退到新建Extra Pool (Tier3)
 *    调用4-5: 从已有Extra Pool分配 (Tier2)
 *
 * 测试步骤：
 * 1. 注册ArgsUpdater算子，Execute中5次调用MallocReadOnlyDevArgs
 * 2. 编译并执行（覆盖AllocateFromReservedSegment/AllocateFromNewPool/AllocateFromExistingPool）
 * 3. 第二轮执行（覆盖IntegrateExtraH2DCopyDatas/IntegrateExtraUpdateDatas的UpdateForExecute路径）
 * 预期结果：
 * 1. 两轮执行均成功
 * 2. 三级分配策略全部覆盖：reserved segment → new pool → existing pool
 * 3. Extra pool的H2D刷新和UpdateHostArgs回调正常工作
 */
TEST_F(CustomOpRefreshTest, args_updater_reserved_exhausted_fallback_to_extra_pool) {
  MockForGenerateTask("DNN_VM_CUSTOM_OP_STORE", GenerateTaskForCustomOp);
  MockForGenerateTask("RTSLib", GenerateTaskForMemCopyAync);
  DUMP_GRAPH_WHEN("PreRunAfterBuild");

  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  gert::GertRuntimeStub runtime_stub;
  args_table = new void *[4];
  TestArgsUpdaterMultiAllocCustomOp::update_host_args_count_ = 0;

  std::map<AscendString, AscendString> options;
  options.emplace(ge::OPTION_CONST_LIFECYCLE, "graph");
  options.emplace(ge::OPTION_FEATURE_BASE_REFRESHABLE, "1");
  options.emplace(ge::OPTION_GRAPH_RUN_MODE, "1");
  Session session(options);
  auto compute_graph = ShareGraph::BuildOnlyCustomOpKnowShapeGraph();
  auto custom_op_node = compute_graph->FindNode("custom_op");
  custom_op_node->GetOpDesc()->SetType("MultiAllocOp");
  auto graph = GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);

  CustomOpFactory::RegisterCustomOpCreator("MultiAllocOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterMultiAllocCustomOp>();
  });

  const auto infer_shape_func = [](gert::InferShapeContext *context) -> graphStatus {
    const auto input_shape = context->GetInputShape(0U);
    auto output = context->GetOutputShape(0);
    for (size_t dim = 0UL; dim < input_shape->GetDimNum(); dim++) {
      output->AppendDim(input_shape->GetDim(dim));
    }
    output->SetDimNum(input_shape->GetDimNum());
    return GRAPH_SUCCESS;
  };
  const auto infer_data_type_func = [](gert::InferDataTypeContext *context) -> graphStatus {
    const auto date_type = context->GetInputDataType(0U);
    EXPECT_EQ(context->SetOutputDataType(0, date_type), SUCCESS);
    return GRAPH_SUCCESS;
  };
  const auto infer_shape_range_func = [](gert::InferShapeRangeContext *context) -> graphStatus {
    auto input_shape_range = context->GetInputShapeRange(0U);
    auto output_shape_range = context->GetOutputShapeRange(0U);
    output_shape_range->SetMin(const_cast<gert::Shape *>(input_shape_range->GetMin()));
    output_shape_range->SetMax(const_cast<gert::Shape *>(input_shape_range->GetMax()));
    return GRAPH_SUCCESS;
  };

  gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2(true);
  auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  ASSERT_NE(space_registry, nullptr);
  auto op_impl_func = space_registry->CreateOrGetOpImpl("MultiAllocOp");
  op_impl_func->infer_shape = infer_shape_func;
  op_impl_func->infer_datatype = infer_data_type_func;
  op_impl_func->infer_shape_range = infer_shape_range_func;
  op_impl_func->output_shape_depend_compute = 1UL;

  uint32_t graph_id = 1;
  session.AddGraph(graph_id, graph);
  EXPECT_EQ(session.CompileGraph(graph_id), SUCCESS);

  const CompiledGraphSummaryPtr summary = session.GetCompiledGraphSummary(graph_id);
  EXPECT_NE(summary, nullptr);
  size_t weight_size, feature_size;
  EXPECT_EQ(SUCCESS, summary->GetConstMemorySize(weight_size));
  EXPECT_EQ(SUCCESS, summary->GetFeatureMemorySize(feature_size));

  std::vector<uint8_t> weight_mem(weight_size, 0);
  std::vector<uint8_t> feature_mem(feature_size, 0);
  EXPECT_EQ(SUCCESS, session.SetGraphConstMemoryBase(graph_id, weight_mem.data(), weight_size));
  EXPECT_EQ(SUCCESS, session.UpdateGraphFeatureMemoryBase(graph_id, feature_mem.data(), feature_size));

  std::vector<ge::Tensor> inputs1;
  std::vector<ge::Tensor> outputs1;
  ConstructCustomInputOutputTensor(3, 1, inputs1, outputs1);
  ge::diagnoseSwitch::DisableDumper();
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs1, outputs1));

  std::vector<ge::Tensor> inputs2;
  std::vector<ge::Tensor> outputs2;
  ConstructCustomInputOutputTensor(3, 1, inputs2, outputs2);
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs2, outputs2));

  EXPECT_GT(TestArgsUpdaterMultiAllocCustomOp::update_host_args_count_, 0);

  delete[] args_table;
  runtime_stub.Clear();
  mmSetEnv(kEnvValue, "", 1);
  ReInitGe();
}

/**
 * 用例描述：EagerOnly算子（非ArgsUpdater）调用MallocReadOnlyDevArgs，走动态内存分配路径
 *
 * 预置条件：
 * 1. 构造计算图，EagerOnly算子直联Data和输出，FM不支持刷新
 *  data0  data1  data2
 *     \    |      /
 *     \    |     /
 *    EagerOnlyMallocOp
 *          |
 *       netoutput
 *
 * 测试步骤：
 * 1. 注册仅继承EagerExecuteOp的算子（不继承ArgsUpdater），Execute中调用MallocReadOnlyDevArgs
 * 2. 编译并执行
 * 预期结果：
 * 1. 执行成功
 * 2. MallocReadOnlyDevArgs走MallocDynamicMemory + H2D拷贝路径（非预留段）
 * 3. 算子不参与统一地址刷新（NeedReserveArgsTable=false）
 */
TEST_F(CustomOpRefreshTest, eager_only_op_with_malloc_read_only_dev_args) {
  MockForGenerateTask("DNN_VM_CUSTOM_OP_STORE", GenerateTaskForCustomOp);
  MockForGenerateTask("RTSLib", GenerateTaskForMemCopyAync);
  DUMP_GRAPH_WHEN("PreRunAfterBuild");

  const char_t *const kEnvValue = "SET_CAPA_VALUE";
  char_t npu_collect_path[MMPA_MAX_PATH] = {};
  mmRealPath(".", &npu_collect_path[0U], MMPA_MAX_PATH);
  const std::string fail_collect_path = (std::string(&npu_collect_path[0U]) + "/mock_fail");
  mmSetEnv(kEnvValue, fail_collect_path.c_str(), 1);

  gert::GertRuntimeStub runtime_stub;
  args_table = new void *[4];

  std::map<AscendString, AscendString> options;
  options.emplace(ge::OPTION_CONST_LIFECYCLE, "graph");
  options.emplace(ge::OPTION_GRAPH_RUN_MODE, "1");
  Session session(options);
  auto compute_graph = ShareGraph::BuildOnlyCustomOpKnowShapeGraph();
  auto custom_op_node = compute_graph->FindNode("custom_op");
  custom_op_node->GetOpDesc()->SetType("EagerOnlyMallocOp");
  auto graph = GraphUtilsEx::CreateGraphFromComputeGraph(compute_graph);

  CustomOpFactory::RegisterCustomOpCreator("EagerOnlyMallocOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestEagerOnlyWithMallocCustomOp>();
  });

  const auto infer_shape_func = [](gert::InferShapeContext *context) -> graphStatus {
    const auto input_shape = context->GetInputShape(0U);
    auto output = context->GetOutputShape(0);
    for (size_t dim = 0UL; dim < input_shape->GetDimNum(); dim++) {
      output->AppendDim(input_shape->GetDim(dim));
    }
    output->SetDimNum(input_shape->GetDimNum());
    return GRAPH_SUCCESS;
  };
  const auto infer_data_type_func = [](gert::InferDataTypeContext *context) -> graphStatus {
    const auto date_type = context->GetInputDataType(0U);
    EXPECT_EQ(context->SetOutputDataType(0, date_type), SUCCESS);
    return GRAPH_SUCCESS;
  };
  const auto infer_shape_range_func = [](gert::InferShapeRangeContext *context) -> graphStatus {
    auto input_shape_range = context->GetInputShapeRange(0U);
    auto output_shape_range = context->GetOutputShapeRange(0U);
    output_shape_range->SetMin(const_cast<gert::Shape *>(input_shape_range->GetMin()));
    output_shape_range->SetMax(const_cast<gert::Shape *>(input_shape_range->GetMax()));
    return GRAPH_SUCCESS;
  };

  gert::SpaceRegistryFaker::CreateDefaultSpaceRegistryImpl2(true);
  auto space_registry = gert::DefaultOpImplSpaceRegistryV2::GetInstance().GetSpaceRegistry();
  ASSERT_NE(space_registry, nullptr);
  auto op_impl_func = space_registry->CreateOrGetOpImpl("EagerOnlyMallocOp");
  op_impl_func->infer_shape = infer_shape_func;
  op_impl_func->infer_datatype = infer_data_type_func;
  op_impl_func->infer_shape_range = infer_shape_range_func;
  op_impl_func->output_shape_depend_compute = 1UL;

  uint32_t graph_id = 1;
  session.AddGraph(graph_id, graph);
  EXPECT_EQ(session.CompileGraph(graph_id), SUCCESS);

  const CompiledGraphSummaryPtr summary = session.GetCompiledGraphSummary(graph_id);
  EXPECT_NE(summary, nullptr);
  size_t weight_size, feature_size;
  EXPECT_EQ(SUCCESS, summary->GetConstMemorySize(weight_size));
  EXPECT_EQ(SUCCESS, summary->GetFeatureMemorySize(feature_size));

  std::vector<uint8_t> weight_mem(weight_size, 0);
  std::vector<uint8_t> feature_mem(feature_size, 0);
  EXPECT_EQ(SUCCESS, session.SetGraphConstMemoryBase(graph_id, weight_mem.data(), weight_size));
  EXPECT_EQ(SUCCESS, session.UpdateGraphFeatureMemoryBase(graph_id, feature_mem.data(), feature_size));

  std::vector<ge::Tensor> inputs;
  std::vector<ge::Tensor> outputs;
  ConstructCustomInputOutputTensor(3, 1, inputs, outputs);
  ge::diagnoseSwitch::DisableDumper();
  runtime_stub.Clear();
  EXPECT_EQ(SUCCESS, session.RunGraphWithStreamAsync(graph_id, nullptr, inputs, outputs));

  EXPECT_FALSE(CustomOpFactory::IsAddressRefreshable(AscendString("EagerOnlyMallocOp")));

  delete[] args_table;
  runtime_stub.Clear();
  mmSetEnv(kEnvValue, "", 1);
  ReInitGe();
}

}  // namespace ge
