/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <memory>
#include <utility>
#include "common/share_graph.h"
#include "graph/debug/ge_attr_define.h"
#include "faker/global_data_faker.h"
#include "ge/ge_api.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/load/model_manager/model_manager.h"
#include "graph/load/model_manager/davinci_model.h"
#include "graph/load/model_manager/task_info/task_info.h"
#include "graph/load/model_manager/task_info/ge/custom_task_info.h"
#include "graph/custom_op_factory.h"
#include "ge_graph_dsl/graph_dsl.h"
#include "ge_graph_dsl/op_desc/op_desc_cfg_box.h"
#include "graph/custom_op.h"
#include "utils/taskdef_builder.h"
#include "utils/mock_ops_kernel_builder.h"
#include "init_ge.h"

namespace ge {
namespace {

Status GenerateTaskForCustomOp(const Node &node, RunContext &run_context, std::vector<domi::TaskDef> &tasks) {
  domi::TaskDef task_def = {};
  task_def.set_stream_id(node.GetOpDesc()->GetStreamId());
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_sqe_num(5);

  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  kernel_def->mutable_context()->set_op_index(node.GetOpDesc()->GetId());
  tasks.push_back(task_def);
  return SUCCESS;
}

class TestArgsUpdaterCustomOp : public ArgsUpdater, public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    return GRAPH_SUCCESS;
  }

  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    const auto *input_tensor = ctx->GetInputTensor(0);
    const auto *output_tensor = ctx->GetOutputTensor(0);
    const auto *host_args = ctx->GetKernelArgs(gert::Placement::kPlacementHost, 0);

    if (host_args != nullptr && host_args->args_size >= sizeof(uint64_t) * 2 && input_tensor != nullptr &&
        output_tensor != nullptr) {
      auto *args = static_cast<uint64_t *>(host_args->args_data);
      args[0] = reinterpret_cast<uint64_t>(input_tensor->GetData<void>());
      args[1] = reinterpret_cast<uint64_t>(output_tensor->GetData<void>());
    }

    return GRAPH_SUCCESS;
  }
};

class TestAnnotatedArgsOp : public AnnotatedArgsOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x31U};
    const auto *input = ctx.GetInputTensor(0U);
    if (input == nullptr) {
      return GRAPH_FAILED;
    }

    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetData<void>()});
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"test_annotated_args_kernel", kBin, sizeof(kBin), 1U, ctx.GetStreamId()},
        std::move(args));
  }
};

class TestBothRefreshInterfacesCustomOp : public AnnotatedArgsOp, public ArgsUpdater, public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }

  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    (void)ctx;
    return GRAPH_FAILED;
  }

  graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }
};

}  // namespace

class CustomTaskInfoModeTest : public testing::Test {
 protected:
  void SetUp() {
    MockForGenerateTask("DNN_VM_CUSTOM_OP_STORE", GenerateTaskForCustomOp);
    ReInitGe();
  }

  void TearDown() {
    ReInitGe();
  }
};

TEST_F(CustomTaskInfoModeTest, ArgsUpdater_Detection_ViaArgsUpdateOp) {
  CustomOpFactory::RegisterCustomOpCreator("ArgsUpdaterTestOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterCustomOp>();
  });

  auto custom_op_ptr = CustomOpFactory::CreateOrGetCustomOp(AscendString("ArgsUpdaterTestOp"));
  ASSERT_NE(custom_op_ptr, nullptr);

  auto *args_update_op = dynamic_cast<ArgsUpdater *>(custom_op_ptr);
  ASSERT_NE(args_update_op, nullptr);
}

TEST_F(CustomTaskInfoModeTest, CustomOpFactory_RegisterArgsUpdater_Success) {
  CustomOpFactory::RegisterCustomOpCreator("ArgsUpdaterTestOp2", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterCustomOp>();
  });

  auto custom_op_ptr = CustomOpFactory::CreateOrGetCustomOp(AscendString("ArgsUpdaterTestOp2"));
  ASSERT_NE(custom_op_ptr, nullptr);

  auto *eager_execute_op = dynamic_cast<EagerExecuteOp *>(custom_op_ptr);
  ASSERT_NE(eager_execute_op, nullptr);

  auto *args_update_op = dynamic_cast<ArgsUpdater *>(custom_op_ptr);
  ASSERT_NE(args_update_op, nullptr);
}

TEST_F(CustomTaskInfoModeTest, TaskDef_Generation_Success) {
  DavinciModel model(0, nullptr);
  domi::ModelTaskDef model_task_def;
  domi::TaskDef *task = model_task_def.add_task();
  task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));

  domi::KernelDef *kernel_def = task->mutable_kernel();
  domi::KernelContext *ctx = kernel_def->mutable_context();

  auto op_desc = std::make_shared<OpDesc>("custom_test", "CustomOp");
  op_desc->SetId(0);
  model.op_list_[op_desc->GetId()] = op_desc;
  ctx->set_op_index(op_desc->GetId());

  EXPECT_EQ(ctx->op_index(), 0U);
  EXPECT_EQ(task->type(), static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
}

TEST_F(CustomTaskInfoModeTest, IsAddressRefreshable_ArgsUpdaterOp_ReturnsTrue) {
  CustomOpFactory::RegisterCustomOpCreator("IsRefreshableTestOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterCustomOp>();
  });

  EXPECT_TRUE(CustomOpFactory::IsAddressRefreshable(AscendString("IsRefreshableTestOp")));
}

TEST_F(CustomTaskInfoModeTest, AnnotatedArgsOp_UsesAnnotatedArgsStrategy) {
  CustomOpFactory::RegisterCustomOpCreator(
      "AnnotatedArgsOp", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestAnnotatedArgsOp>(); });

  auto *custom_op_ptr = CustomOpFactory::CreateOrGetCustomOp(AscendString("AnnotatedArgsOp"));
  ASSERT_NE(custom_op_ptr, nullptr);
  EXPECT_NE(dynamic_cast<AnnotatedArgsOp *>(custom_op_ptr), nullptr);
  EXPECT_EQ(CustomOpFactory::GetArgsRefreshStrategy(AscendString("AnnotatedArgsOp")),
            ArgsRefreshStrategy::kAnnotatedArgs);
  EXPECT_TRUE(CustomOpFactory::IsAddressRefreshable(AscendString("AnnotatedArgsOp")));
}

TEST_F(CustomTaskInfoModeTest, ArgsUpdaterTakesPrecedenceWhenBothInterfacesExist) {
  CustomOpFactory::RegisterCustomOpCreator("BothRefreshInterfacesCustomOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestBothRefreshInterfacesCustomOp>();
  });

  auto *custom_op_ptr = CustomOpFactory::CreateOrGetCustomOp(AscendString("BothRefreshInterfacesCustomOp"));
  ASSERT_NE(custom_op_ptr, nullptr);
  EXPECT_NE(dynamic_cast<AnnotatedArgsOp *>(custom_op_ptr), nullptr);
  EXPECT_NE(dynamic_cast<ArgsUpdater *>(custom_op_ptr), nullptr);
  EXPECT_EQ(CustomOpFactory::GetArgsRefreshStrategy(AscendString("BothRefreshInterfacesCustomOp")),
            ArgsRefreshStrategy::kUpdateCallback);
}

TEST_F(CustomTaskInfoModeTest, IsAddressRefreshable_NonArgsUpdaterOp_ReturnsFalse) {
  class TestNonArgsUpdaterOp : public EagerExecuteOp {
   public:
    graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
      return GRAPH_SUCCESS;
    }
  };

  CustomOpFactory::RegisterCustomOpCreator("NonRefreshableTestOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestNonArgsUpdaterOp>();
  });

  EXPECT_FALSE(CustomOpFactory::IsAddressRefreshable(AscendString("NonRefreshableTestOp")));
}

TEST_F(CustomTaskInfoModeTest, IsAddressRefreshable_UnregisteredOp_ReturnsFalse) {
  EXPECT_FALSE(CustomOpFactory::IsAddressRefreshable(AscendString("UnregisteredTestOp_999")));
}

TEST_F(CustomTaskInfoModeTest, NeedReserveArgsTable_MatchesIsAddressRefreshable) {
  CustomOpFactory::RegisterCustomOpCreator("NeedReserveTestOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterCustomOp>();
  });

  DavinciModel model(0, nullptr);
  auto op_desc = std::make_shared<OpDesc>("reserve_op", "NeedReserveTestOp");
  GeTensorDesc desc;
  op_desc->AddInputDesc(desc);
  op_desc->AddOutputDesc(desc);
  op_desc->SetId(0);
  model.op_list_[0] = op_desc;

  if (model.GetCustomOpRegistry() == nullptr) {
    model.SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  }
  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0);
  task_def.mutable_kernel()->mutable_context()->set_op_index(op_desc->GetId());

  CustomTaskInfo task_info;
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);
  EXPECT_TRUE(task_info.NeedReserveArgsTable());
}

/**
 * 用例描述：测试 CustomTaskInfo 在 dump 使能时，UpdateCustomDumpAddrs 正确更新 input/output dump 地址
 * 预置条件：
 *   1. 构造 DavinciModel 并使能 dump
 *   2. 构造 CustomTaskInfo 并设置 op_desc、input/output 地址
 *   3. 调用 InsertDumpOp 初始化 input/output dump 算子
 * 测试步骤：
 *   1. 调用 UpdateCustomDumpAddrs 传入 input 和 output 地址
 * 预期结果：
 *   1. UpdateCustomDumpAddrs 返回 SUCCESS
 *   2. input_custom_dump_ 的 op_mapping_info 中记录了正确的 input 地址
 *   3. output_custom_dump_ 的 op_mapping_info 中记录了正确的 output 地址
 */
TEST_F(CustomTaskInfoModeTest, UpdateCustomDumpAddrs_DumpEnabled_Success) {
  DavinciModel model(0, nullptr);
  auto op_desc = std::make_shared<OpDesc>("dump_custom_op", "CustomOp");
  GeTensorDesc desc;
  op_desc->AddInputDesc(desc);
  op_desc->AddOutputDesc(desc);
  op_desc->SetId(0);
  op_desc->SetStreamId(0);
  model.op_list_[0] = op_desc;

  DumpProperties dump_properties;
  dump_properties.SetDumpMode("all");
  dump_properties.AddPropertyValue(DUMP_ALL_MODEL, {});
  model.SetDumpProperties(dump_properties);

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.output_data_addrs_ = {0x2000};

  ASSERT_EQ(task_info.InsertDumpOp("input"), SUCCESS);
  ASSERT_EQ(task_info.InsertDumpOp("output"), SUCCESS);

  const Status ret = task_info.UpdateCustomDumpAddrs({0x3000}, {0x3008});
  EXPECT_EQ(ret, SUCCESS);
  ASSERT_EQ(task_info.input_custom_dump_.op_mapping_info_.task_size(), 1);
  ASSERT_EQ(task_info.output_custom_dump_.op_mapping_info_.task_size(), 1);
  ASSERT_EQ(task_info.input_custom_dump_.op_mapping_info_.task(0).input_size(), 1);
  ASSERT_EQ(task_info.output_custom_dump_.op_mapping_info_.task(0).output_size(), 1);
  EXPECT_EQ(task_info.input_custom_dump_.op_mapping_info_.task(0).input(0).address(), 0x3000);
  EXPECT_EQ(task_info.output_custom_dump_.op_mapping_info_.task(0).output(0).address(), 0x3008);

  if (task_info.input_custom_dump_.proto_dev_mem_ != nullptr) {
    (void)aclrtFree(task_info.input_custom_dump_.proto_dev_mem_);
    task_info.input_custom_dump_.proto_dev_mem_ = nullptr;
  }
  if (task_info.input_custom_dump_.proto_size_dev_mem_ != nullptr) {
    (void)aclrtFree(task_info.input_custom_dump_.proto_size_dev_mem_);
    task_info.input_custom_dump_.proto_size_dev_mem_ = nullptr;
  }
  if (task_info.input_custom_dump_.launch_kernel_args_dev_mem_ != nullptr) {
    (void)aclrtFree(task_info.input_custom_dump_.launch_kernel_args_dev_mem_);
    task_info.input_custom_dump_.launch_kernel_args_dev_mem_ = nullptr;
  }
  if (task_info.input_custom_dump_.dev_mem_unload_ != nullptr) {
    (void)aclrtFree(task_info.input_custom_dump_.dev_mem_unload_);
    task_info.input_custom_dump_.dev_mem_unload_ = nullptr;
  }
  if (task_info.output_custom_dump_.proto_dev_mem_ != nullptr) {
    (void)aclrtFree(task_info.output_custom_dump_.proto_dev_mem_);
    task_info.output_custom_dump_.proto_dev_mem_ = nullptr;
  }
  if (task_info.output_custom_dump_.proto_size_dev_mem_ != nullptr) {
    (void)aclrtFree(task_info.output_custom_dump_.proto_size_dev_mem_);
    task_info.output_custom_dump_.proto_size_dev_mem_ = nullptr;
  }
  if (task_info.output_custom_dump_.launch_kernel_args_dev_mem_ != nullptr) {
    (void)aclrtFree(task_info.output_custom_dump_.launch_kernel_args_dev_mem_);
    task_info.output_custom_dump_.launch_kernel_args_dev_mem_ = nullptr;
  }
  if (task_info.output_custom_dump_.dev_mem_unload_ != nullptr) {
    (void)aclrtFree(task_info.output_custom_dump_.dev_mem_unload_);
    task_info.output_custom_dump_.dev_mem_unload_ = nullptr;
  }
}

namespace {
domi::TaskDef BuildAnnotatedArgsTaskDef(uint32_t op_index, const std::string &args_format,
                                        const std::string &kernel_name, uint32_t block_dim, uint32_t args_count,
                                        const std::vector<uint8_t> &args_data) {
  domi::TaskDef task_def;
  task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  task_def.set_stream_id(0U);

  domi::KernelDef *kernel_def = task_def.mutable_kernel();
  domi::KernelContext *ctx = kernel_def->mutable_context();
  ctx->set_op_index(op_index);
  ctx->set_args_count(args_count);
  ctx->set_args_format(args_format);

  kernel_def->set_args(args_data.data(), args_data.size());
  kernel_def->set_args_size(static_cast<uint32_t>(args_data.size()));
  kernel_def->set_kernel_name(kernel_name);
  kernel_def->set_block_dim(block_dim);
  return task_def;
}

void SetupModelForAnnotatedArgsOp(DavinciModel &model, OpDescPtr &op_desc, const std::string &op_type) {
  op_desc = std::make_shared<OpDesc>("annotated_op", op_type);
  GeTensorDesc desc;
  op_desc->AddInputDesc(desc);
  op_desc->AddOutputDesc(desc);
  op_desc->AppendIrInput("input0", IrInputType::kIrInputRequired);
  op_desc->AppendIrOutput("output0", IrOutputType::kIrOutputRequired);
  op_desc->SetId(0);
  op_desc->SetStreamId(0);
  model.op_list_[0] = op_desc;
  if (model.GetCustomOpRegistry() == nullptr) {
    model.SetCustomOpRegistry(CustomOpFactory::GetGlobalRegistryPtr());
  }
}

Status CallParseAnnotatedArgs(CustomTaskInfo &task_info, const domi::TaskDef &task_def, TaskRunParam &task_run_param) {
  task_info.args_refresh_strategy_ = ArgsRefreshStrategy::kAnnotatedArgs;
  task_info.is_args_refreshable_ = true;
  const auto &kernel_def = task_def.kernel();
  auto context = kernel_def.context();
  return task_info.ParseAnnotatedArgsTaskRunParam(kernel_def, context, task_run_param);
}
}  // namespace

/**
 * 用例描述：测试 kAnnotatedArgs 策略的 CustomTaskInfo::ParseTaskRunParam 成功解析 TaskDef 中的 args 布局
 * 预置条件：
 *   1. 注册 TestAnnotatedArgsOp（实现 AnnotatedArgsOp 接口）
 *   2. 构造 DavinciModel 和 OpDesc，设置 input/output 地址
 *   3. 构造带 args_format="{i0}{o0}" 的 TaskDef
 * 测试步骤：
 *   1. 调用 ParseTaskRunParam
 * 预期结果：
 *   1. 返回 SUCCESS
 *   2. kernel_name_ 为 "test_kernel"
 *   3. block_dim_ 为 1
 */
TEST_F(CustomTaskInfoModeTest, ParseAnnotatedArgsTaskRunParam_Success) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsParseOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });

  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsParseOp");

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};

  std::vector<uint8_t> args_data(2U * sizeof(uint64_t), 0);
  auto *args_ptr = reinterpret_cast<uint64_t *>(args_data.data());
  args_ptr[0] = 0x1000;
  args_ptr[1] = 0x2000;

  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{o0}", "test_kernel", 1U, 2U, args_data);
  TaskRunParam task_run_param;
  EXPECT_EQ(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);
  EXPECT_EQ(task_info.kernel_name_, "test_kernel");
  EXPECT_EQ(task_info.block_dim_, 1U);
  ASSERT_EQ(task_run_param.parsed_input_addrs.size(), 1U);
  ASSERT_EQ(task_run_param.parsed_output_addrs.size(), 1U);
}

/**
 * 用例描述：测试 args_format 为空时 ParseAnnotatedArgsTaskRunParam 返回失败
 * 预置条件：注册 AnnotatedArgsOp，构造 args_format 为空的 TaskDef
 * 预期结果：返回 FAILED
 */
TEST_F(CustomTaskInfoModeTest, ParseAnnotatedArgsTaskRunParam_EmptyArgsFormat_Failed) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsEmptyFmtOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });

  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsEmptyFmtOp");

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};

  std::vector<uint8_t> args_data(2U * sizeof(uint64_t), 0);
  auto task_def = BuildAnnotatedArgsTaskDef(0, "", "test_kernel", 1U, 2U, args_data);
  TaskRunParam task_run_param;
  EXPECT_NE(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);
}

/**
 * 用例描述：测试 kernel_name 为空时返回失败
 * 预期结果：返回 FAILED
 */
TEST_F(CustomTaskInfoModeTest, ParseAnnotatedArgsTaskRunParam_EmptyKernelName_Failed) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsEmptyKerneOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });

  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsEmptyKerneOp");

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};

  std::vector<uint8_t> args_data(2U * sizeof(uint64_t), 0);
  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{o0}", "", 1U, 2U, args_data);
  TaskRunParam task_run_param;
  EXPECT_NE(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);
}

/**
 * 用例描述：测试 block_dim 为 0 时返回失败
 * 预期结果：返回 FAILED
 */
TEST_F(CustomTaskInfoModeTest, ParseAnnotatedArgsTaskRunParam_ZeroBlockDim_Failed) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsZeroBlockOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });

  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsZeroBlockOp");

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};

  std::vector<uint8_t> args_data(2U * sizeof(uint64_t), 0);
  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{o0}", "test_kernel", 0U, 2U, args_data);
  TaskRunParam task_run_param;
  EXPECT_NE(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);
}

/**
 * 用例描述：测试 ir_idx 超出 input 范围时返回失败
 * 预期结果：返回 FAILED
 */
TEST_F(CustomTaskInfoModeTest, ParseAnnotatedArgsTaskRunParam_InputIrIdxOutOfRange_Failed) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsIdxOorOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });

  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsIdxOorOp");

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};

  // {i5} but only 1 input exists
  std::vector<uint8_t> args_data(1U * sizeof(uint64_t), 0);
  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i5}", "test_kernel", 1U, 1U, args_data);
  TaskRunParam task_run_param;
  EXPECT_NE(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);
}

/**
 * 用例描述：测试 args_format 包含 CUSTOM_VALUE({#})时正确跳过，不参与地址刷新
 * 预置条件：构造 args_format="{i0}{#}{o0}"，args_count=3
 * 预期结果：
 *   1. 返回 SUCCESS
 *   2. parsed_input_addrs 和 parsed_output_addrs 各有 1 个
 */
TEST_F(CustomTaskInfoModeTest, ParseAnnotatedArgsTaskRunParam_CustomValueSkipped_Success) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsCustomValOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });

  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsCustomValOp");

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};

  std::vector<uint8_t> args_data(3U * sizeof(uint64_t), 0);
  auto *args_ptr = reinterpret_cast<uint64_t *>(args_data.data());
  args_ptr[0] = 0x1000;
  args_ptr[2] = 0x2000;

  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{#42}{o0}", "test_kernel", 1U, 3U, args_data);
  TaskRunParam task_run_param;
  EXPECT_EQ(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);
  ASSERT_EQ(task_run_param.parsed_input_addrs.size(), 1U);
  ASSERT_EQ(task_run_param.parsed_output_addrs.size(), 1U);
}

/**
 * 用例描述：测试 DavinciModel::FindTbeKernelBin 在 ge_model_ 为 null 时返回 nullptr
 * 预期结果：返回 nullptr
 */
TEST_F(CustomTaskInfoModeTest, FindTbeKernelBin_NullGeModel_ReturnsNull) {
  DavinciModel model(0, nullptr);
  EXPECT_EQ(model.FindTbeKernelBin("any_kernel"), nullptr);
}

/**
 * 用例描述：测试 DavinciModel::FindTbeKernelBin 在 kernel_name 为空时返回 nullptr
 * 预期结果：返回 nullptr
 */
TEST_F(CustomTaskInfoModeTest, FindTbeKernelBin_EmptyKernelName_ReturnsNull) {
  DavinciModel model(0, nullptr);
  auto ge_model = MakeShared<GeModel>();
  model.ge_model_ = ge_model;
  EXPECT_EQ(model.FindTbeKernelBin(""), nullptr);
}

/**
 * 用例描述：测试 DavinciModel::FindTbeKernelBin 成功找到 kernel bin
 * 预置条件：ge_model_ 的 TBEKernelStore 中包含指定 kernel
 * 预期结果：返回非 null 的 TBEKernelPtr
 */
TEST_F(CustomTaskInfoModeTest, FindTbeKernelBin_Success) {
  DavinciModel model(0, nullptr);
  auto ge_model = MakeShared<GeModel>();
  auto &kernel_store = ge_model->GetTBEKernelStore();
  std::vector<char> kernel_bin_data(64, 0);
  auto kernel_bin = MakeShared<OpKernelBin>("found_kernel", std::move(kernel_bin_data));
  kernel_store.AddTBEKernel(kernel_bin);
  model.ge_model_ = ge_model;

  auto result = model.FindTbeKernelBin("found_kernel");
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(result->GetName(), "found_kernel");
}

/**
 * 用例描述：测试 UpdateHostArgs 在 kAnnotatedArgs 策略下直接返回 SUCCESS
 * 预期结果：返回 SUCCESS
 */
TEST_F(CustomTaskInfoModeTest, UpdateHostArgs_AnnotatedArgs_ReturnsSuccess) {
  CustomTaskInfo task_info;
  task_info.args_refresh_strategy_ = ArgsRefreshStrategy::kAnnotatedArgs;
  EXPECT_EQ(task_info.UpdateHostArgs(nullptr, 0), SUCCESS);
}

/**
 * 用例描述：测试 GetTaskArgsRefreshInfos 在未 Init 时返回空列表
 * 预期结果：infos 为空，返回 SUCCESS
 */
TEST_F(CustomTaskInfoModeTest, GetAnnotatedArgsRefreshInfos_EmptyWhenNotDistributed) {
  CustomTaskInfo task_info;
  std::vector<TaskArgsRefreshInfo> infos;
  EXPECT_EQ(task_info.GetTaskArgsRefreshInfos(infos), SUCCESS);
  EXPECT_TRUE(infos.empty());
}

/**
 * 用例描述：测试 kAnnotatedArgs 策略的 CustomTaskInfo::Distribute 成功下发 kernel，GetTaskArgsRefreshInfos 返回刷新信息
 * 预置条件：
 *   1. 注册 AnnotatedArgsOp
 *   2. 构造 DavinciModel，设置 ge_model_（含 TBEKernelStore）、logical_mem_allocations_
 *   3. 构造 CustomTaskInfo 并调用 ParseTaskRunParam + Init 设置 annotated args 状态
 *   4. 设置 stream_ 等成员
 * 测试步骤：
 *   1. 调用 Distribute
 *   2. 调用 GetTaskArgsRefreshInfos
 * 预期结果：
 *   1. Distribute 返回 SUCCESS
 *   2. GetTaskArgsRefreshInfos 返回非空 infos，包含 input 和 output 的刷新信息
 */
TEST_F(CustomTaskInfoModeTest, DistributeAnnotatedArgs_Success) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsDistributeOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });

  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsDistributeOp");

  auto ge_model = MakeShared<GeModel>();
  auto &kernel_store = ge_model->GetTBEKernelStore();
  std::vector<char> kernel_bin_data(64, 0);
  kernel_store.AddTBEKernel(MakeShared<OpKernelBin>("annotated_kernel", std::move(kernel_bin_data)));
  model.ge_model_ = ge_model;

  const uint64_t input_addr = 0x1000;
  const uint64_t output_addr = 0x2000;
  const uint64_t alloc_size = 4096;
  model.logical_mem_allocations_.push_back({0U, input_addr, alloc_size, MemAllocation::INPUT, 0U, 0U, 0U, 0U});
  model.logical_mem_allocations_.push_back({1U, output_addr, alloc_size, MemAllocation::OUTPUT, 0U, 0U, 0U, 0U});

  rtStream_t rt_stream = nullptr;
  (void)aclrtCreateStream(&rt_stream);
  model.reusable_stream_allocator_ = ReusableStreamAllocator::Create();
  model.stream_list_ = {rt_stream};

  std::vector<uint8_t> args_data(2U * sizeof(uint64_t), 0);
  auto *args_ptr = reinterpret_cast<uint64_t *>(args_data.data());
  args_ptr[0] = input_addr;
  args_ptr[1] = output_addr;
  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{o0}", "annotated_kernel", 1U, 2U, args_data);

  CustomTaskInfo task_info;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {input_addr};
  task_info.input_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)};
  task_info.output_data_addrs_ = {output_addr};
  task_info.output_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)};
  TaskRunParam task_run_param;
  ASSERT_EQ(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = reinterpret_cast<uint64_t>(args_data.data());
  IowAddrs iow_addrs;
  iow_addrs.input_logic_addrs = {{input_addr, static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)}};
  iow_addrs.output_logic_addrs = {{output_addr, static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)}};
  ASSERT_EQ(task_info.Init(task_def, &model, args, {}, iow_addrs), SUCCESS);
  EXPECT_EQ(task_info.Distribute(), SUCCESS);

  std::vector<TaskArgsRefreshInfo> infos;
  EXPECT_EQ(task_info.GetTaskArgsRefreshInfos(infos), SUCCESS);
  EXPECT_EQ(infos.size(), 2U);

  bool found_input = false;
  bool found_output = false;
  for (const auto &info : infos) {
    if (info.id == 0U) {
      found_input = true;
    }
    if (info.id == 1U) {
      found_output = true;
    }
  }
  EXPECT_TRUE(found_input);
  EXPECT_TRUE(found_output);

  if (rt_stream != nullptr) {
    (void)aclrtDestroyStream(rt_stream);
  }
}

/**
 * 用例描述：测试 DistributeAnnotatedArgs 在找不到 kernel bin 时返回失败
 * 预置条件：ge_model_ 的 TBEKernelStore 不包含所需 kernel
 * 预期结果：Distribute 返回失败
 */
TEST_F(CustomTaskInfoModeTest, DistributeAnnotatedArgs_KernelNotFound_Failed) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsNoKernelOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });

  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsNoKernelOp");

  auto ge_model = MakeShared<GeModel>();
  model.ge_model_ = ge_model;

  model.logical_mem_allocations_.push_back({0U, 0x1000, 4096, MemAllocation::INPUT, 0U, 0U, 0U, 0U});

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};
  task_info.args_placement_ = ArgsPlacement::kArgsPlacementHbm;

  aclrtStream stream = nullptr;
  (void)aclrtCreateStream(&stream);
  task_info.stream_ = stream;

  std::vector<uint8_t> args_data(2U * sizeof(uint64_t), 0);
  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{o0}", "nonexistent_kernel", 1U, 2U, args_data);
  TaskRunParam task_run_param;
  ASSERT_EQ(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);

  EXPECT_NE(task_info.Distribute(), SUCCESS);

  if (stream != nullptr) {
    (void)aclrtDestroyStream(stream);
  }
}

/**
 * 用例描述：测试 kAnnotatedArgs 策略下 UpdateHostArgs 在 kUpdateCallback 策略之外也返回 SUCCESS
 * 预期结果：返回 SUCCESS
 */
TEST_F(CustomTaskInfoModeTest, UpdateHostArgs_NoneStrategy_ReturnsSuccess) {
  CustomTaskInfo task_info;
  task_info.args_refresh_strategy_ = ArgsRefreshStrategy::kNone;
  EXPECT_EQ(task_info.UpdateHostArgs(nullptr, 0), SUCCESS);
}

namespace {
bool SetupForDistributeWithMagic(DavinciModel &model, OpDescPtr &op_desc, CustomTaskInfo &task_info,
                                 rtStream_t &rt_stream, std::vector<uint8_t> &args_data, const std::string &op_type,
                                 const std::string &magic) {
  CustomOpFactory::RegisterCustomOpCreator(AscendString(op_type.c_str()), []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });
  SetupModelForAnnotatedArgsOp(model, op_desc, op_type);
  if (!magic.empty()) {
    (void)AttrUtils::SetStr(op_desc, TVM_ATTR_NAME_MAGIC, magic);
  }

  auto ge_model = MakeShared<GeModel>();
  auto &kernel_store = ge_model->GetTBEKernelStore();
  std::vector<char> kernel_bin_data(64, 0);
  kernel_store.AddTBEKernel(MakeShared<OpKernelBin>("annotated_kernel", std::move(kernel_bin_data)));
  model.ge_model_ = ge_model;

  const uint64_t input_addr = 0x1000;
  const uint64_t output_addr = 0x2000;
  const uint64_t alloc_size = 4096;
  model.logical_mem_allocations_.push_back({0U, input_addr, alloc_size, MemAllocation::INPUT, 0U, 0U, 0U, 0U});
  model.logical_mem_allocations_.push_back({1U, output_addr, alloc_size, MemAllocation::OUTPUT, 0U, 0U, 0U, 0U});

  (void)aclrtCreateStream(&rt_stream);
  model.reusable_stream_allocator_ = ReusableStreamAllocator::Create();
  model.stream_list_ = {rt_stream};

  args_data.resize(2U * sizeof(uint64_t), 0);
  auto *args_ptr = reinterpret_cast<uint64_t *>(args_data.data());
  args_ptr[0] = input_addr;
  args_ptr[1] = output_addr;
  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{o0}", "annotated_kernel", 1U, 2U, args_data);

  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {input_addr};
  task_info.input_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)};
  task_info.output_data_addrs_ = {output_addr};
  task_info.output_mem_types_ = {static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)};
  TaskRunParam task_run_param;
  if (CallParseAnnotatedArgs(task_info, task_def, task_run_param) != SUCCESS) {
    return false;
  }

  PisToArgs args;
  args[static_cast<size_t>(ArgsPlacement::kArgsPlacementHbm)].dev_addr = reinterpret_cast<uint64_t>(args_data.data());
  IowAddrs iow_addrs;
  iow_addrs.input_logic_addrs = {{input_addr, static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)}};
  iow_addrs.output_logic_addrs = {{output_addr, static_cast<uint64_t>(MemoryAppType::kMemoryTypeFeatureMap)}};
  return task_info.Init(task_def, &model, args, {}, iow_addrs) == SUCCESS;
}
}  // namespace

/**
 * 用例描述：测试 ParseTaskRunParam 在 kAnnotatedArgs 策略下正确分发到 ParseAnnotatedArgsTaskRunParam
 * 预期结果：返回 SUCCESS
 */
TEST_F(CustomTaskInfoModeTest, ParseTaskRunParam_AnnotatedArgsDispatch) {
  CustomOpFactory::RegisterCustomOpCreator("AnnotatedArgsDispatchOp", []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestAnnotatedArgsOp>();
  });
  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsDispatchOp");

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};

  std::vector<uint8_t> args_data(2U * sizeof(uint64_t), 0);
  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{o0}", "test_kernel", 1U, 2U, args_data);
  TaskRunParam task_run_param;
  EXPECT_EQ(task_info.ParseTaskRunParam(task_def, &model, task_run_param), SUCCESS);
}

/**
 * 用例描述：测试 ParseAnnotatedArgsTaskRunParam 在有 workspace 地址时正确解析
 * 预期结果：返回 SUCCESS，parsed_workspace_addrs 非空
 */
TEST_F(CustomTaskInfoModeTest, ParseAnnotatedArgsTaskRunParam_WithWorkspace_Success) {
  CustomOpFactory::RegisterCustomOpCreator(
      "AnnotatedArgsWsOp", []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<TestAnnotatedArgsOp>(); });
  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  SetupModelForAnnotatedArgsOp(model, op_desc, "AnnotatedArgsWsOp");

  CustomTaskInfo task_info;
  task_info.davinci_model_ = &model;
  task_info.op_desc_ = op_desc;
  task_info.input_data_addrs_ = {0x1000};
  task_info.input_mem_types_ = {0};
  task_info.output_data_addrs_ = {0x2000};
  task_info.output_mem_types_ = {0};
  task_info.workspace_addrs_ = {0x3000};
  task_info.workspace_mem_types_ = {0};

  std::vector<uint8_t> args_data(2U * sizeof(uint64_t), 0);
  auto task_def = BuildAnnotatedArgsTaskDef(0, "{i0}{o0}", "test_kernel", 1U, 2U, args_data);
  TaskRunParam task_run_param;
  EXPECT_EQ(CallParseAnnotatedArgs(task_info, task_def, task_run_param), SUCCESS);
  ASSERT_EQ(task_run_param.parsed_workspace_addrs.size(), 1U);
}

/**
 * 用例描述：测试 AssembleIoByArgsFormat 处理 WORKSPACE(ir_idx=-1) 全量追加
 * 预期结果：返回 SUCCESS，io_addrs_ 包含所有 workspace 地址
 */
TEST_F(CustomTaskInfoModeTest, AssembleIoByArgsFormat_WorkspaceAllAddrs_Success) {
  CustomTaskInfo task_info;
  task_info.op_desc_ = std::make_shared<OpDesc>("ws_all_op", "CustomOp");
  task_info.workspace_addrs_ = {0x3000, 0x4000};
  task_info.workspace_mem_types_ = {0, 0};

  ArgDesc ws_desc;
  ws_desc.addr_type = AddrType::WORKSPACE;
  ws_desc.ir_idx = -1;
  ws_desc.folded = false;
  (void)memset_s(ws_desc.reserved, sizeof(ws_desc.reserved), 0, sizeof(ws_desc.reserved));
  task_info.args_format_holder_.arg_descs.push_back(ws_desc);

  EXPECT_EQ(task_info.AssembleIoByArgsFormat(), SUCCESS);
  ASSERT_EQ(task_info.io_addrs_.size(), 2U);
  EXPECT_EQ(task_info.io_addrs_[0], 0x3000);
  EXPECT_EQ(task_info.io_addrs_[1], 0x4000);
}

/**
 * 用例描述：测试 AssembleIoByArgsFormat 处理 WORKSPACE(ir_idx>=0) 指定索引追加
 * 预期结果：返回 SUCCESS，io_addrs_ 仅包含指定索引的 workspace 地址
 */
TEST_F(CustomTaskInfoModeTest, AssembleIoByArgsFormat_WorkspaceSpecificIndex_Success) {
  CustomTaskInfo task_info;
  task_info.op_desc_ = std::make_shared<OpDesc>("ws_idx_op", "CustomOp");
  task_info.workspace_addrs_ = {0x3000, 0x4000};
  task_info.workspace_mem_types_ = {0, 0};

  ArgDesc ws_desc;
  ws_desc.addr_type = AddrType::WORKSPACE;
  ws_desc.ir_idx = 1;
  ws_desc.folded = false;
  (void)memset_s(ws_desc.reserved, sizeof(ws_desc.reserved), 0, sizeof(ws_desc.reserved));
  task_info.args_format_holder_.arg_descs.push_back(ws_desc);

  EXPECT_EQ(task_info.AssembleIoByArgsFormat(), SUCCESS);
  ASSERT_EQ(task_info.io_addrs_.size(), 1U);
  EXPECT_EQ(task_info.io_addrs_[0], 0x4000);
}

/**
 * 用例描述：测试 AssembleIoByArgsFormat 处理 CUSTOM_VALUE 类型
 * 预期结果：返回 SUCCESS，io_addrs_ 包含 reserved 中的值
 */
TEST_F(CustomTaskInfoModeTest, AssembleIoByArgsFormat_CustomValue_Success) {
  CustomTaskInfo task_info;
  task_info.op_desc_ = std::make_shared<OpDesc>("cv_op", "CustomOp");

  ArgDesc cv_desc;
  cv_desc.addr_type = AddrType::CUSTOM_VALUE;
  cv_desc.ir_idx = 0;
  cv_desc.folded = false;
  const uint64_t custom_value = 0xDEADBEEF;
  (void)memcpy_s(cv_desc.reserved, sizeof(cv_desc.reserved), &custom_value, sizeof(custom_value));
  task_info.args_format_holder_.arg_descs.push_back(cv_desc);

  EXPECT_EQ(task_info.AssembleIoByArgsFormat(), SUCCESS);
  ASSERT_EQ(task_info.io_addrs_.size(), 1U);
  EXPECT_EQ(task_info.io_addrs_[0], 0xDEADBEEF);
}

/**
 * 用例描述：测试 AssembleIoByArgsFormat 处理 PLACEHOLDER 类型
 * 预期结果：返回 SUCCESS，io_addrs_ 包含 0
 */
TEST_F(CustomTaskInfoModeTest, AssembleIoByArgsFormat_Placeholder_Success) {
  CustomTaskInfo task_info;
  task_info.op_desc_ = std::make_shared<OpDesc>("ph_op", "CustomOp");

  ArgDesc ph_desc;
  ph_desc.addr_type = AddrType::PLACEHOLDER;
  ph_desc.ir_idx = 0;
  ph_desc.folded = false;
  (void)memset_s(ph_desc.reserved, sizeof(ph_desc.reserved), 0, sizeof(ph_desc.reserved));
  task_info.args_format_holder_.arg_descs.push_back(ph_desc);

  EXPECT_EQ(task_info.AssembleIoByArgsFormat(), SUCCESS);
  ASSERT_EQ(task_info.io_addrs_.size(), 1U);
  EXPECT_EQ(task_info.io_addrs_[0], 0UL);
}

/**
 * 用例描述：测试 AssembleIoByArgsFormat 处理不支持的 AddrType 时返回失败
 * 预期结果：返回 FAILED
 */
TEST_F(CustomTaskInfoModeTest, AssembleIoByArgsFormat_InvalidAddrType_Failed) {
  CustomTaskInfo task_info;
  task_info.op_desc_ = std::make_shared<OpDesc>("invalid_addr_op", "CustomOp");

  ArgDesc invalid_desc;
  invalid_desc.addr_type = AddrType::MAX;
  invalid_desc.ir_idx = 0;
  invalid_desc.folded = false;
  (void)memset_s(invalid_desc.reserved, sizeof(invalid_desc.reserved), 0, sizeof(invalid_desc.reserved));
  task_info.args_format_holder_.arg_descs.push_back(invalid_desc);

  EXPECT_NE(task_info.AssembleIoByArgsFormat(), SUCCESS);
}

/**
 * 用例描述：测试 AppendInputOutputAddr 在可选输入无实例时使用占位地址 0
 * 预期结果：返回 SUCCESS，io_addrs_ 包含 0
 */
TEST_F(CustomTaskInfoModeTest, AppendInputOutputAddr_OptionalInputPlaceholder_Success) {
  CustomTaskInfo task_info;
  task_info.op_desc_ = std::make_shared<OpDesc>("opt_input_op", "CustomOp");
  task_info.args_format_holder_.ir_input_2_range[0] = {0, 0};

  EXPECT_EQ(task_info.AppendInputOutputAddr(0, true), SUCCESS);
  ASSERT_EQ(task_info.io_addrs_.size(), 1U);
  EXPECT_EQ(task_info.io_addrs_[0], 0UL);
}

/**
 * 用例描述：测试 Distribute 在 magic=RT_DEV_BINARY_MAGIC_ELF 时成功
 * 预期结果：返回 SUCCESS
 */
TEST_F(CustomTaskInfoModeTest, Distribute_WithElfMagic_Success) {
  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  CustomTaskInfo task_info;
  rtStream_t rt_stream = nullptr;
  std::vector<uint8_t> args_data;

  ASSERT_TRUE(SetupForDistributeWithMagic(model, op_desc, task_info, rt_stream, args_data, "ElfMagicOp",
                                          "RT_DEV_BINARY_MAGIC_ELF"));
  EXPECT_EQ(task_info.Distribute(), SUCCESS);
  if (rt_stream != nullptr) {
    (void)aclrtDestroyStream(rt_stream);
  }
}

/**
 * 用例描述：测试 Distribute 在 magic=RT_DEV_BINARY_MAGIC_ELF_AICUBE 时成功
 * 预期结果：返回 SUCCESS
 */
TEST_F(CustomTaskInfoModeTest, Distribute_WithAiCubeMagic_Success) {
  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  CustomTaskInfo task_info;
  rtStream_t rt_stream = nullptr;
  std::vector<uint8_t> args_data;

  ASSERT_TRUE(SetupForDistributeWithMagic(model, op_desc, task_info, rt_stream, args_data, "AiCubeMagicOp",
                                          "RT_DEV_BINARY_MAGIC_ELF_AICUBE"));
  EXPECT_EQ(task_info.Distribute(), SUCCESS);
  if (rt_stream != nullptr) {
    (void)aclrtDestroyStream(rt_stream);
  }
}

/**
 * 用例描述：测试 Distribute 在 magic 无效时返回失败
 * 预期结果：返回失败
 */
TEST_F(CustomTaskInfoModeTest, Distribute_WithInvalidMagic_Failed) {
  DavinciModel model(0, nullptr);
  OpDescPtr op_desc;
  CustomTaskInfo task_info;
  rtStream_t rt_stream = nullptr;
  std::vector<uint8_t> args_data;

  ASSERT_TRUE(SetupForDistributeWithMagic(model, op_desc, task_info, rt_stream, args_data, "InvalidMagicOp",
                                          "INVALID_MAGIC_VALUE"));
  EXPECT_NE(task_info.Distribute(), SUCCESS);
  if (rt_stream != nullptr) {
    (void)aclrtDestroyStream(rt_stream);
  }
}

}  // namespace ge
