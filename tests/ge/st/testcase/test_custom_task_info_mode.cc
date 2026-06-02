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
#include <memory>
#include "common/share_graph.h"
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

Status GenerateTaskForCustomOp(const Node &node,
                                RunContext &run_context,
                                std::vector<domi::TaskDef> &tasks) {
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
  graphStatus Execute(gert::EagerOpExecutionContext* ctx) override {
    return GRAPH_SUCCESS;
  }
  
  graphStatus UpdateHostArgs(gert::UpdateArgsContext* ctx) override {
    const auto* input_tensor = ctx->GetInputTensor(0);
    const auto* output_tensor = ctx->GetOutputTensor(0);
    const auto* host_args = ctx->GetKernelArgs(gert::Placement::kPlacementHost, 0);

    if (host_args != nullptr && host_args->args_size >= sizeof(uint64_t) * 2 &&
        input_tensor != nullptr && output_tensor != nullptr) {
      auto* args = static_cast<uint64_t*>(host_args->args_data);
      args[0] = reinterpret_cast<uint64_t>(input_tensor->GetData<void>());
      args[1] = reinterpret_cast<uint64_t>(output_tensor->GetData<void>());
    }

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
  CustomOpFactory::RegisterCustomOpCreator("ArgsUpdaterTestOp", []()->std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterCustomOp>();
  });

  auto custom_op_ptr = CustomOpFactory::CreateOrGetCustomOp(AscendString("ArgsUpdaterTestOp"));
  ASSERT_NE(custom_op_ptr, nullptr);
  
  auto* args_update_op = dynamic_cast<ArgsUpdater*>(custom_op_ptr);
  ASSERT_NE(args_update_op, nullptr);
}

TEST_F(CustomTaskInfoModeTest, CustomOpFactory_RegisterArgsUpdater_Success) {
  CustomOpFactory::RegisterCustomOpCreator("ArgsUpdaterTestOp2", []()->std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterCustomOp>();
  });

  auto custom_op_ptr = CustomOpFactory::CreateOrGetCustomOp(AscendString("ArgsUpdaterTestOp2"));
  ASSERT_NE(custom_op_ptr, nullptr);
  
  auto* eager_execute_op = dynamic_cast<EagerExecuteOp*>(custom_op_ptr);
  ASSERT_NE(eager_execute_op, nullptr);
  
  auto* args_update_op = dynamic_cast<ArgsUpdater*>(custom_op_ptr);
  ASSERT_NE(args_update_op, nullptr);
}

TEST_F(CustomTaskInfoModeTest, TaskDef_Generation_Success) {
  DavinciModel model(0, nullptr);
  domi::ModelTaskDef model_task_def;
  domi::TaskDef* task = model_task_def.add_task();
  task->set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  
  domi::KernelDef* kernel_def = task->mutable_kernel();
  domi::KernelContext* ctx = kernel_def->mutable_context();
  
  auto op_desc = std::make_shared<OpDesc>("custom_test", "CustomOp");
  op_desc->SetId(0);
  model.op_list_[op_desc->GetId()] = op_desc;
  ctx->set_op_index(op_desc->GetId());
  
  EXPECT_EQ(ctx->op_index(), 0U);
  EXPECT_EQ(task->type(), static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
}

TEST_F(CustomTaskInfoModeTest, IsAddressRefreshable_ArgsUpdaterOp_ReturnsTrue) {
  CustomOpFactory::RegisterCustomOpCreator("IsRefreshableTestOp", []()->std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterCustomOp>();
  });

  EXPECT_TRUE(CustomOpFactory::IsAddressRefreshable(AscendString("IsRefreshableTestOp")));
}

TEST_F(CustomTaskInfoModeTest, IsAddressRefreshable_NonArgsUpdaterOp_ReturnsFalse) {
  class TestNonArgsUpdaterOp : public EagerExecuteOp {
   public:
    graphStatus Execute(gert::EagerOpExecutionContext* ctx) override { return GRAPH_SUCCESS; }
  };

  CustomOpFactory::RegisterCustomOpCreator("NonRefreshableTestOp", []()->std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestNonArgsUpdaterOp>();
  });

  EXPECT_FALSE(CustomOpFactory::IsAddressRefreshable(AscendString("NonRefreshableTestOp")));
}

TEST_F(CustomTaskInfoModeTest, IsAddressRefreshable_UnregisteredOp_ReturnsFalse) {
  EXPECT_FALSE(CustomOpFactory::IsAddressRefreshable(AscendString("UnregisteredTestOp_999")));
}

TEST_F(CustomTaskInfoModeTest, NeedReserveArgsTable_MatchesIsAddressRefreshable) {
  CustomOpFactory::RegisterCustomOpCreator("NeedReserveTestOp", []()->std::unique_ptr<BaseCustomOp> {
    return std::make_unique<TestArgsUpdaterCustomOp>();
  });

  DavinciModel model(0, nullptr);
  auto op_desc = std::make_shared<OpDesc>("reserve_op", "NeedReserveTestOp");
  GeTensorDesc desc;
  op_desc->AddInputDesc(desc);
  op_desc->AddOutputDesc(desc);
  op_desc->SetId(0);
  model.op_list_[0] = op_desc;

  CustomTaskInfo task_info;
  task_info.op_desc_ = op_desc;
  AscendString op_type(op_desc->GetType().c_str());
  task_info.is_args_refreshable_ = CustomOpFactory::IsAddressRefreshable(op_type);
  EXPECT_EQ(task_info.NeedReserveArgsTable(), task_info.is_args_refreshable_);
}

}  // namespace ge