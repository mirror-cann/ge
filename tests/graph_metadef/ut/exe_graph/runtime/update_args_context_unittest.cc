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
#include "exe_graph/runtime/update_args_context.h"
#include "exe_graph/runtime/eager_op_execution_context.h"
#include "exe_graph/runtime/kernel_args.h"
#include "faker/kernel_run_context_faker.h"
#include "exe_graph/runtime/tensor.h"
#include "exe_graph/runtime/storage_shape.h"
#include "framework/runtime/args_handler.h"

namespace gert {
namespace {

class MockArgsHandler : public ArgsHandler {
 public:
  const KernelArgs *MallocReadOnlyDevArgs(void *host_args, size_t args_size) override {
    return nullptr;
  }

  const std::deque<KernelArgs> &GetKernelArgs(Placement placement) const override {
    if (placement == Placement::kPlacementHost) {
      return host_args_;
    }
    return device_args_;
  }

  void SetHostArgs(const std::deque<KernelArgs> &args) {
    host_args_ = args;
  }
  void SetDeviceArgs(const std::deque<KernelArgs> &args) {
    device_args_ = args;
  }

 private:
  std::deque<KernelArgs> host_args_;
  std::deque<KernelArgs> device_args_;
};

class UtestUpdateArgsContext : public testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(UtestUpdateArgsContext, GetInputTensor_Success) {
  gert::Tensor in_tensor = {{{8, 3, 224, 224}, {8, 1, 224, 224, 16}},
                            {ge::FORMAT_NCHW, ge::FORMAT_NC1HWC0, {}},
                            kOnDeviceHbm,
                            ge::DT_FLOAT16,
                            reinterpret_cast<void *>(0x12345678ULL)};

  std::vector<void *> inputs;
  inputs.push_back(reinterpret_cast<void *>(&in_tensor));

  std::vector<void *> outputs;
  outputs.push_back(reinterpret_cast<void *>(0x87654321ULL));

  auto context_holder = KernelRunContextFaker()
                            .KernelIONum(1, 1)
                            .NodeIoNum(1, 1)
                            .IrInputNum(1)
                            .IrOutputNum(1)
                            .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_NC1HWC0)
                            .Inputs(inputs)
                            .Outputs(outputs)
                            .Build();

  auto *ctx = context_holder.template GetContext<UpdateArgsContext>();
  ASSERT_NE(ctx, nullptr);

  auto *tensor = ctx->GetInputTensor(0);
  ASSERT_NE(tensor, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(tensor->GetAddr()), 0x12345678ULL);

  auto *tensor_invalid = ctx->GetInputTensor(100);
  EXPECT_EQ(tensor_invalid, nullptr);
}

TEST_F(UtestUpdateArgsContext, GetOutputTensor_Success) {
  gert::Tensor out_tensor = {{{8, 3, 224, 224}, {8, 1, 224, 224, 16}},
                             {ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ, {}},
                             kOnDeviceHbm,
                             ge::DT_FLOAT16,
                             reinterpret_cast<void *>(0x87654321ULL)};

  std::vector<void *> inputs;
  inputs.push_back(reinterpret_cast<void *>(0x12345678ULL));

  std::vector<void *> outputs;
  outputs.push_back(reinterpret_cast<void *>(&out_tensor));

  auto context_holder = KernelRunContextFaker()
                            .KernelIONum(1, 1)
                            .NodeIoNum(1, 1)
                            .IrInputNum(1)
                            .IrOutputNum(1)
                            .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_NC1HWC0)
                            .NodeOutputTd(0, ge::DT_FLOAT16, ge::FORMAT_ND, ge::FORMAT_FRACTAL_NZ)
                            .Inputs(inputs)
                            .Outputs(outputs)
                            .Build();

  auto *ctx = context_holder.template GetContext<UpdateArgsContext>();
  ASSERT_NE(ctx, nullptr);

  auto *tensor = ctx->GetOutputTensor(0);
  ASSERT_NE(tensor, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(tensor->GetAddr()), 0x87654321ULL);

  auto *tensor_invalid = ctx->GetOutputTensor(100);
  EXPECT_EQ(tensor_invalid, nullptr);
}

TEST_F(UtestUpdateArgsContext, GetKernelArgs_Host_Success) {
  MockArgsHandler handler;
  std::deque<KernelArgs> host_args;
  host_args.resize(1);
  host_args[0].args_data = reinterpret_cast<void *>(0xBBBB0000ULL);
  host_args[0].args_size = 2048ULL;
  host_args[0].placement = Placement::kPlacementHost;
  handler.SetHostArgs(host_args);

  std::vector<void *> inputs;
  inputs.push_back(reinterpret_cast<void *>(0xAAAA0000ULL));  // IR input 0
  inputs.push_back(nullptr);                                  // additional: allocator
  inputs.push_back(nullptr);                                  // additional: stream

  std::vector<void*> outputs;
  outputs.push_back(nullptr);                                 // compute output 0
  outputs.push_back(nullptr);                                 // additional: workspace
  outputs.push_back(reinterpret_cast<void*>(&handler));       // additional: args_handler

  auto context_holder = KernelRunContextFaker()
                             .KernelIONum(3, 3)
                             .NodeIoNum(1, 1)
                             .IrInputNum(1)
                             .IrOutputNum(0)
                             .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_NCHW)
                             .Inputs(inputs)
                             .Outputs(outputs)
                             .Build();

  auto* ctx = context_holder.template GetContext<UpdateArgsContext>();
  ASSERT_NE(ctx, nullptr);

  const auto *args = ctx->GetKernelArgs(Placement::kPlacementHost, 0U);
  ASSERT_NE(args, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(args->args_data), 0xBBBB0000ULL);
  EXPECT_EQ(args->args_size, 2048ULL);
  EXPECT_EQ(args->placement, Placement::kPlacementHost);
}

TEST_F(UtestUpdateArgsContext, GetKernelArgs_Device_Success) {
  MockArgsHandler handler;
  std::deque<KernelArgs> device_args;
  device_args.resize(1);
  device_args[0].args_data = reinterpret_cast<void *>(0xCCCC0000ULL);
  device_args[0].args_size = 2048ULL;
  device_args[0].placement = Placement::kPlacementDevice;
  handler.SetDeviceArgs(device_args);

  std::vector<void *> inputs;
  inputs.push_back(reinterpret_cast<void *>(0xAAAA0000ULL));  // IR input 0
  inputs.push_back(nullptr);                                  // additional: allocator
  inputs.push_back(nullptr);                                  // additional: stream

  std::vector<void*> outputs;
  outputs.push_back(nullptr);                                 // compute output 0
  outputs.push_back(nullptr);                                 // additional: workspace
  outputs.push_back(reinterpret_cast<void*>(&handler));       // additional: args_handler

  auto context_holder = KernelRunContextFaker()
                             .KernelIONum(3, 3)
                             .NodeIoNum(1, 1)
                             .IrInputNum(1)
                             .IrOutputNum(0)
                             .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_NCHW)
                             .Inputs(inputs)
                             .Outputs(outputs)
                             .Build();

  auto* ctx = context_holder.template GetContext<UpdateArgsContext>();
  ASSERT_NE(ctx, nullptr);

  const auto *args = ctx->GetKernelArgs(Placement::kPlacementDevice, 0U);
  ASSERT_NE(args, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(args->args_data), 0xCCCC0000ULL);
  EXPECT_EQ(args->args_size, 2048ULL);
  EXPECT_EQ(args->placement, Placement::kPlacementDevice);
}

TEST_F(UtestUpdateArgsContext, PODConstraintCheck) {
  EXPECT_EQ(sizeof(UpdateArgsContext), sizeof(EagerOpExecutionContext));
  EXPECT_TRUE(std::is_standard_layout<UpdateArgsContext>::value);
}

TEST_F(UtestUpdateArgsContext, GetKernelArgs_IndexOutOfRange_ReturnsNullptr) {
  MockArgsHandler handler;
  std::deque<KernelArgs> host_args;
  host_args.resize(1);
  host_args[0].args_data = reinterpret_cast<void *>(0xBBBB0000ULL);
  host_args[0].args_size = 2048ULL;
  host_args[0].placement = Placement::kPlacementHost;
  handler.SetHostArgs(host_args);

  std::vector<void*> inputs;
  inputs.push_back(reinterpret_cast<void*>(0xAAAA0000ULL));  // IR input 0
  inputs.push_back(nullptr);                                  // additional: allocator
  inputs.push_back(nullptr);                                  // additional: stream

  std::vector<void*> outputs;
  outputs.push_back(nullptr);                                 // compute output 0
  outputs.push_back(nullptr);                                 // additional: workspace
  outputs.push_back(reinterpret_cast<void*>(&handler));       // additional: args_handler

  auto context_holder = KernelRunContextFaker()
                             .KernelIONum(3, 3)
                             .NodeIoNum(1, 1)
                             .IrInputNum(1)
                             .IrOutputNum(0)
                             .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_NCHW)
                             .Inputs(inputs)
                             .Outputs(outputs)
                             .Build();

  auto* ctx = context_holder.template GetContext<UpdateArgsContext>();
  ASSERT_NE(ctx, nullptr);

  const auto *args = ctx->GetKernelArgs(Placement::kPlacementHost, 100U);
  EXPECT_EQ(args, nullptr);
}

TEST_F(UtestUpdateArgsContext, GetKernelArgs_NullHandlerInContext_ReturnsNullptr) {
  std::vector<void*> inputs;
  inputs.push_back(reinterpret_cast<void*>(0xAAAA0000ULL));  // IR input 0
  inputs.push_back(nullptr);                                  // additional: allocator
  inputs.push_back(nullptr);                                  // additional: stream

  std::vector<void*> outputs;
  outputs.push_back(nullptr);                                 // compute output 0
  outputs.push_back(nullptr);                                 // additional: workspace
  outputs.push_back(nullptr);                                 // additional: args_handler (null)

  auto context_holder = KernelRunContextFaker()
                             .KernelIONum(3, 3)
                             .NodeIoNum(1, 1)
                             .IrInputNum(1)
                             .IrOutputNum(0)
                             .NodeInputTd(0, ge::DT_FLOAT16, ge::FORMAT_NCHW, ge::FORMAT_NCHW)
                             .Inputs(inputs)
                             .Outputs(outputs)
                             .Build();

  auto* ctx = context_holder.template GetContext<UpdateArgsContext>();
  ASSERT_NE(ctx, nullptr);

  const auto *args = ctx->GetKernelArgs(Placement::kPlacementHost, 0U);
  EXPECT_EQ(args, nullptr);
}

}  // namespace
}  // namespace gert
