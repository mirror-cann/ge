/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "exe_graph/runtime/annotated_args_context.h"

#include <cstring>
#include <limits>

#include <gtest/gtest.h>
#include "exe_graph/lowering/kernel_run_context_builder.h"
#include "runtime/annotated_args_handler.h"
#include "faker/allocator_faker.h"
#include "graph/op_desc.h"
#include "graph/utils/args_format_desc_utils.h"

namespace gert {
namespace {
ge::OpDescPtr MakeOpDesc() {
  auto op_desc = std::make_shared<ge::OpDesc>("annotated_args_context_op", "AnnotatedArgsContextOp");
  const ge::GeTensorDesc tensor_desc(ge::GeShape({2, 2}), ge::FORMAT_ND, ge::DT_FLOAT16);
  (void)op_desc->AddInputDesc("x", tensor_desc);
  (void)op_desc->AddOutputDesc("y", tensor_desc);
  return op_desc;
}

TEST(AnnotatedArgsContextUT, AllocatesWorkspaceAndRecordsLaunchTask) {
  AllocatorFaker allocator;
  uint32_t stream_id = 3U;
  allocator.SetStreamId(stream_id);
  Tensor input_tensor({{2, 2}, {2, 2}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, kOnDeviceHbm, ge::DT_FLOAT16,
                      reinterpret_cast<void *>(0x1000U));
  Tensor output_tensor({{2, 2}, {2, 2}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, kOnDeviceHbm, ge::DT_FLOAT16,
                       reinterpret_cast<void *>(0x2000U));
  std::vector<GertMemBlock *> workspace_mems;
  AnnotatedArgsHandler args_handler;

  auto holder = KernelRunContextBuilder()
                    .Inputs({&input_tensor, &allocator})
                    .Outputs({&output_tensor, &workspace_mems, static_cast<ArgsHandler *>(&args_handler)})
                    .Build(MakeOpDesc());
  auto *context = reinterpret_cast<AnnotatedArgsContext *>(holder.GetKernelContext());
  ASSERT_NE(context, nullptr);

  EXPECT_EQ(context->GetInputTensor(0U), &input_tensor);
  EXPECT_EQ(context->GetInputTensor(1U), nullptr);
  EXPECT_EQ(context->GetInputTensor(std::numeric_limits<size_t>::max()), nullptr);
  EXPECT_EQ(context->GetOutputTensor(0U), &output_tensor);
  EXPECT_EQ(context->GetOutputTensor(1U), nullptr);
  EXPECT_EQ(context->GetStreamId(), stream_id);

  const auto workspace = context->MallocWorkSpace(64U);
  ASSERT_NE(workspace.addr, nullptr);
  EXPECT_EQ(workspace.index, 0U);
  ASSERT_EQ(workspace_mems.size(), 1U);
  EXPECT_EQ(workspace_mems[0]->GetAddr(), workspace.addr);

  uint8_t bin[] = {0x11U, 0x22U};
  constexpr uint64_t kCustomValue = 7U;
  AnnotatedKernelArgs args(InputAddr{0U, input_tensor.GetAddr()}, OutputAddr{0U, output_tensor.GetAddr()}, workspace,
                           kCustomValue);
  AnnotatedKernelLaunchInfo launch_info{"annotated_args_context_kernel", bin, sizeof(bin), 8U, stream_id};
  EXPECT_EQ(context->AddLaunch(launch_info, std::move(args)), ge::GRAPH_SUCCESS);
  bin[0] = 0xFFU;
  ASSERT_EQ(args_handler.GetLaunchCount(), 1U);
  const auto *task = args_handler.GetLaunch(0U);
  ASSERT_NE(task, nullptr);
  EXPECT_STREQ(task->GetKernelName(), "annotated_args_context_kernel");
  EXPECT_EQ(task->GetKernelBinSize(), sizeof(bin));
  ASSERT_NE(task->GetKernelBinData(), nullptr);
  EXPECT_EQ(task->GetKernelBinData()[0], 0x11U);
  EXPECT_EQ(task->GetBlockDim(), 8U);
  EXPECT_EQ(task->GetStreamId(), stream_id);
  EXPECT_EQ(task->GetArgsSize(), 4U * sizeof(uint64_t));
  ASSERT_EQ(task->GetArgDescCount(), 4U);
  ASSERT_NE(task->GetArgDescs(), nullptr);
  EXPECT_EQ(task->GetArgDescs()[0].addr_type, ge::AddrType::INPUT);
  EXPECT_EQ(task->GetArgDescs()[1].addr_type, ge::AddrType::OUTPUT);
  EXPECT_EQ(task->GetArgDescs()[2].addr_type, ge::AddrType::WORKSPACE);
  EXPECT_EQ(task->GetArgDescs()[3].addr_type, ge::AddrType::CUSTOM_VALUE);

  for (auto *mem_block : workspace_mems) {
    mem_block->Free(0);
  }
  workspace_mems.clear();
}

TEST(AnnotatedArgsContextUT, RejectsInvalidLaunchParameters) {
  AllocatorFaker allocator;
  uint32_t stream_id = 3U;
  allocator.SetStreamId(stream_id);
  Tensor input_tensor({{2, 2}, {2, 2}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, kOnDeviceHbm, ge::DT_FLOAT16,
                      reinterpret_cast<void *>(0x1000U));
  Tensor output_tensor({{2, 2}, {2, 2}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, kOnDeviceHbm, ge::DT_FLOAT16,
                       reinterpret_cast<void *>(0x2000U));
  std::vector<GertMemBlock *> workspace_mems;
  AnnotatedArgsHandler args_handler;

  auto holder = KernelRunContextBuilder()
                    .Inputs({&input_tensor, &allocator})
                    .Outputs({&output_tensor, &workspace_mems, static_cast<ArgsHandler *>(&args_handler)})
                    .Build(MakeOpDesc());
  auto *context = reinterpret_cast<AnnotatedArgsContext *>(holder.GetKernelContext());
  ASSERT_NE(context, nullptr);

  const uint8_t bin[] = {0x11U};
  auto make_args = []() { return AnnotatedKernelArgs(InputAddr{0U, reinterpret_cast<void *>(0x1000U)}); };
  EXPECT_NE(context->AddLaunch(AnnotatedKernelLaunchInfo{nullptr, bin, sizeof(bin), 8U, stream_id}, make_args()),
            ge::GRAPH_SUCCESS);
  EXPECT_NE(context->AddLaunch(AnnotatedKernelLaunchInfo{"", bin, sizeof(bin), 8U, stream_id}, make_args()),
            ge::GRAPH_SUCCESS);
  EXPECT_NE(context->AddLaunch(AnnotatedKernelLaunchInfo{"kernel", nullptr, sizeof(bin), 8U, stream_id}, make_args()),
            ge::GRAPH_SUCCESS);
  EXPECT_NE(context->AddLaunch(AnnotatedKernelLaunchInfo{"kernel", bin, 0U, 8U, stream_id}, make_args()),
            ge::GRAPH_SUCCESS);
  EXPECT_NE(context->AddLaunch(AnnotatedKernelLaunchInfo{"kernel", bin, sizeof(bin), 0U, stream_id}, make_args()),
            ge::GRAPH_SUCCESS);
  AnnotatedKernelArgs empty_args;
  EXPECT_NE(
      context->AddLaunch(AnnotatedKernelLaunchInfo{"kernel", bin, sizeof(bin), 8U, stream_id}, std::move(empty_args)),
      ge::GRAPH_SUCCESS);
  AnnotatedKernelArgs invalid_args(
      InputAddr{static_cast<uint32_t>(std::numeric_limits<int32_t>::max()) + 1U, reinterpret_cast<void *>(0x1000U)});
  EXPECT_NE(
      context->AddLaunch(AnnotatedKernelLaunchInfo{"kernel", bin, sizeof(bin), 8U, stream_id}, std::move(invalid_args)),
      ge::GRAPH_SUCCESS);
  EXPECT_EQ(args_handler.GetLaunchCount(), 0U);
}

TEST(AnnotatedArgsContextUT, RecordsMultipleLaunchesInOrder) {
  AllocatorFaker allocator;
  constexpr uint32_t kStreamId = 3U;
  allocator.SetStreamId(kStreamId);
  Tensor input_tensor({{2, 2}, {2, 2}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, kOnDeviceHbm, ge::DT_FLOAT16,
                      reinterpret_cast<void *>(0x1000U));
  Tensor output_tensor({{2, 2}, {2, 2}}, {ge::FORMAT_ND, ge::FORMAT_ND, {}}, kOnDeviceHbm, ge::DT_FLOAT16,
                       reinterpret_cast<void *>(0x2000U));
  std::vector<GertMemBlock *> workspace_mems;
  AnnotatedArgsHandler args_handler;

  auto holder = KernelRunContextBuilder()
                    .Inputs({&input_tensor, &allocator})
                    .Outputs({&output_tensor, &workspace_mems, static_cast<ArgsHandler *>(&args_handler)})
                    .Build(MakeOpDesc());
  auto *context = reinterpret_cast<AnnotatedArgsContext *>(holder.GetKernelContext());
  ASSERT_NE(context, nullptr);

  uint8_t first_bin[] = {0x11U};
  uint8_t second_bin[] = {0x22U};
  EXPECT_EQ(context->AddLaunch(AnnotatedKernelLaunchInfo{"first_kernel", first_bin, sizeof(first_bin), 1U, kStreamId},
                               AnnotatedKernelArgs(InputAddr{0U, input_tensor.GetAddr()})),
            ge::GRAPH_SUCCESS);
  EXPECT_EQ(
      context->AddLaunch(AnnotatedKernelLaunchInfo{"second_kernel", second_bin, sizeof(second_bin), 2U, kStreamId},
                         AnnotatedKernelArgs(OutputAddr{0U, output_tensor.GetAddr()})),
      ge::GRAPH_SUCCESS);
  first_bin[0] = 0xFFU;
  second_bin[0] = 0xFFU;

  ASSERT_EQ(args_handler.GetLaunchCount(), 2U);
  const auto *first_launch = args_handler.GetLaunch(0U);
  const auto *second_launch = args_handler.GetLaunch(1U);
  ASSERT_NE(first_launch, nullptr);
  ASSERT_NE(second_launch, nullptr);
  EXPECT_EQ(args_handler.GetLaunch(2U), nullptr);
  EXPECT_STREQ(first_launch->GetKernelName(), "first_kernel");
  EXPECT_STREQ(second_launch->GetKernelName(), "second_kernel");
  EXPECT_EQ(first_launch->GetKernelBinData()[0], 0x11U);
  EXPECT_EQ(second_launch->GetKernelBinData()[0], 0x22U);
  EXPECT_EQ(first_launch->GetBlockDim(), 1U);
  EXPECT_EQ(second_launch->GetBlockDim(), 2U);
  EXPECT_EQ(first_launch->GetArgDescs()[0].addr_type, ge::AddrType::INPUT);
  EXPECT_EQ(second_launch->GetArgDescs()[0].addr_type, ge::AddrType::OUTPUT);
}

TEST(AnnotatedArgsHandlerUT, RuntimeArgsInterfacesAreEmpty) {
  AnnotatedArgsHandler args_handler;

  EXPECT_EQ(args_handler.MallocReadOnlyDevArgs(nullptr, 0U), nullptr);
  EXPECT_TRUE(args_handler.GetKernelArgs(ge::kPlacementHost).empty());
  EXPECT_TRUE(args_handler.GetKernelArgs(ge::kPlacementDevice).empty());
}

TEST(AnnotatedArgsContextUT, ReturnsDefaultValuesWhenComputeNodeInfoIsMissing) {
  AnnotatedArgsContext context{};

  EXPECT_EQ(context.GetStreamId(), 0U);
  EXPECT_EQ(context.GetInputTensor(0U), nullptr);
  EXPECT_EQ(context.GetOutputTensor(0U), nullptr);
}
}  // namespace
}  // namespace gert
