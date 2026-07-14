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
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engines/custom_engine/custom_graph_optimizer.h"
#include "engines/custom_engine/custom_ops_kernel_builder.h"
#include "engines/custom_engine/custom_ops_kernel_info_store.h"
#include "common/checker.h"
#include "exe_graph/runtime/annotated_args_context.h"
#include "graph/compute_graph.h"
#include "graph/custom_op_factory.h"
#include "graph/ascend_string.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/ge_tensor.h"
#include "graph/op_kernel_bin.h"
#include "graph/op_desc.h"
#include "graph/custom_op.h"
#include "graph/ge_context.h"
#include "graph/ge_local_context.h"
#include "debug/ge_util.h"
#include "graph/utils/args_format_desc_utils.h"
#include "graph/utils/attr_utils.h"
#include "graph/utils/tensor_utils.h"
#include "securec.h"

namespace ge {
namespace custom {

class MockCustomOp : public EagerExecuteOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }
};

class MockCompilableCustomOp : public EagerExecuteOp, public CompilableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }

  graphStatus Compile(gert::OpCompileContext *ctx) override {
    (void)ctx;
    ++compile_count_;
    return GRAPH_SUCCESS;
  }

  static void ResetCompileCount() {
    compile_count_ = 0;
  }

  static int32_t GetCompileCount() {
    return compile_count_;
  }

 private:
  static int32_t compile_count_;
};

int32_t MockCompilableCustomOp::compile_count_ = 0;
class MockBaseOnlyCustomOp : public BaseCustomOp {};

class MockPortableCustomOp : public PortableOp {
 public:
  graphStatus Serialize(std::vector<uint8_t> &buffer) override {
    static const uint8_t kBin[] = {0x51U, 0x52U};
    buffer.assign(kBin, kBin + sizeof(kBin));
    return GRAPH_SUCCESS;
  }

  graphStatus Deserialize(const std::vector<uint8_t> &buffer) override {
    (void)buffer;
    return GRAPH_SUCCESS;
  }
};

std::atomic_bool g_compile_context_output_called{false};
constexpr uintptr_t kLogicDataMemBase = 0x80000000UL;
constexpr uintptr_t kLogicWeightMemBase = 0x90000000UL;

uint64_t ReadUint64Slot(const std::string &args, const size_t index) {
  uint64_t value = 0U;
  EXPECT_EQ(memcpy_s(&value, sizeof(value), args.data() + (index * sizeof(value)), sizeof(value)), EOK);
  return value;
}

uint16_t ReadUint16Slot(const std::string &args, const size_t index) {
  uint16_t value = 0U;
  EXPECT_EQ(memcpy_s(&value, sizeof(value), args.data() + (index * sizeof(value)), sizeof(value)), EOK);
  return value;
}

class MockCompileContextOutputOp : public EagerExecuteOp, public CompilableOp {
 public:
  graphStatus Execute(gert::EagerOpExecutionContext *ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }

  graphStatus Compile(gert::OpCompileContext *ctx) override {
    g_compile_context_output_called.store(true);
    if (ctx == nullptr) {
      return GRAPH_FAILED;
    }
    if (ctx->GetInputTensor(0U) != nullptr) {
      return GRAPH_FAILED;
    }

    const auto output0 = ctx->GetOutputTensor(0U);
    const auto output1 = ctx->GetOutputTensor(1U);
    const auto output2 = ctx->GetOutputTensor(2U);
    if ((output0 == nullptr) || (output1 == nullptr) || (output2 == nullptr)) {
      return GRAPH_FAILED;
    }
    if ((output0 != ctx->GetRequiredOutputTensor(0U)) || (output1 != ctx->GetDynamicOutputTensor(1U, 0U)) ||
        (output2 != ctx->GetDynamicOutputTensor(1U, 1U))) {
      return GRAPH_FAILED;
    }
    if ((ctx->GetOutputTensor(3U) != nullptr) || (ctx->GetRequiredOutputTensor(2U) != nullptr) ||
        (ctx->GetDynamicOutputTensor(1U, 2U) != nullptr)) {
      return GRAPH_FAILED;
    }
    if ((output0->GetStorageShape() != gert::Shape({8, 16})) || (output0->GetDataType() != DT_FLOAT16) ||
        (output0->GetStorageFormat() != FORMAT_ND)) {
      return GRAPH_FAILED;
    }
    if ((output1->GetStorageShape() != gert::Shape({16, 16})) || (output1->GetDataType() != DT_FLOAT)) {
      return GRAPH_FAILED;
    }
    if ((output2->GetStorageShape() != gert::Shape({32, 16})) || (output2->GetDataType() != DT_INT32)) {
      return GRAPH_FAILED;
    }
    return GRAPH_SUCCESS;
  }
};

class MockAnnotatedArgsCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x11U, 0x22U, 0x33U, 0x44U};
    auto input = ctx.GetInputTensor(0U);
    auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()}, gert::OutputAddr{0U, output->GetAddr()},
                                   uint64_t{7U});
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_add_kernel", kBin, sizeof(kBin), 8U, ctx.GetStreamId()},
        std::move(args));
  }
};

class MockAnnotatedArgsWithConstInputCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x11U, 0x22U, 0x33U, 0x44U};
    auto input0 = ctx.GetInputTensor(0U);
    auto input1 = ctx.GetInputTensor(1U);
    auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input0);
    GE_ASSERT_NOTNULL(input1);
    GE_ASSERT_NOTNULL(output);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input0->GetAddr()}, gert::InputAddr{1U, input1->GetAddr()},
                                   gert::OutputAddr{0U, output->GetAddr()}, uint64_t{7U});
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_add_kernel", kBin, sizeof(kBin), 8U, ctx.GetStreamId()},
        std::move(args));
  }
};

class MockAnnotatedArgsWithInvalidOptionalAndRequiredInputCustomOp : public AnnotatedArgsOp,
                                                                     public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x15U, 0x16U, 0x17U, 0x18U};
    const auto input = ctx.GetRequiredInputTensor(0U);
    const auto optional_input = ctx.GetOptionalInputTensor(1U);
    const auto input_after_optional = ctx.GetRequiredInputTensor(2U);
    const auto output = ctx.GetOutputTensor(0U);
    if ((input == nullptr) || (optional_input != nullptr) || (input_after_optional == nullptr) || (output == nullptr)) {
      return GRAPH_FAILED;
    }
    if (input_after_optional->GetAddr() != reinterpret_cast<void *>(kLogicDataMemBase + 3072U)) {
      return GRAPH_FAILED;
    }
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()},
                                   gert::InputAddr{2U, input_after_optional->GetAddr()},
                                   gert::OutputAddr{0U, output->GetAddr()});
    return ctx.AddLaunch(gert::AnnotatedKernelLaunchInfo{"custom_invalid_optional_required_kernel", kBin, sizeof(kBin),
                                                         8U, ctx.GetStreamId()},
                         std::move(args));
  }
};

class MockAnnotatedArgsWithInvalidOptionalAndConstInputCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x19U, 0x1AU, 0x1BU, 0x1CU};
    const auto input = ctx.GetRequiredInputTensor(0U);
    const auto optional_input = ctx.GetOptionalInputTensor(1U);
    const auto const_input_after_optional = ctx.GetRequiredInputTensor(2U);
    const auto output = ctx.GetOutputTensor(0U);
    if ((input == nullptr) || (optional_input != nullptr) || (const_input_after_optional == nullptr) ||
        (output == nullptr)) {
      return GRAPH_FAILED;
    }
    if (const_input_after_optional->GetAddr() != reinterpret_cast<void *>(kLogicWeightMemBase + 4096U)) {
      return GRAPH_FAILED;
    }
    const uint32_t stream_id = ctx.GetStreamId();
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()},
                                   gert::InputAddr{2U, const_input_after_optional->GetAddr()},
                                   gert::OutputAddr{0U, output->GetAddr()});
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_invalid_optional_const_kernel", kBin, sizeof(kBin), 8U, stream_id},
        std::move(args));
  }
};

class MockAnnotatedArgsWithWorkspaceCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x21U, 0x22U, 0x23U, 0x24U};
    auto input = ctx.GetInputTensor(0U);
    auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    auto workspace = ctx.MallocWorkSpace(100U);
    GE_ASSERT_NOTNULL(workspace.addr);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()}, gert::OutputAddr{0U, output->GetAddr()},
                                   workspace);
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_workspace_kernel", kBin, sizeof(kBin), 4U, ctx.GetStreamId()},
        std::move(args));
  }
};

class MockAnnotatedArgsWithMultipleWorkspacesCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x25U, 0x26U, 0x27U, 0x28U};
    auto input = ctx.GetInputTensor(0U);
    auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    auto workspace0 = ctx.MallocWorkSpace(100U);
    auto workspace1 = ctx.MallocWorkSpace(600U);
    GE_ASSERT_NOTNULL(workspace0.addr);
    GE_ASSERT_NOTNULL(workspace1.addr);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()}, gert::OutputAddr{0U, output->GetAddr()},
                                   workspace0, workspace1);
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_multi_workspace_kernel", kBin, sizeof(kBin), 4U, ctx.GetStreamId()},
        std::move(args));
  }
};

class MockAnnotatedArgsWithMultipleLaunchesCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin0[] = {0x61U, 0x62U};
    static const uint8_t kBin1[] = {0x71U, 0x72U};
    auto input = ctx.GetInputTensor(0U);
    auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    gert::AnnotatedKernelArgs args0(gert::InputAddr{0U, input->GetAddr()});
    GE_ASSERT_SUCCESS(ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_multi_launch_kernel0", kBin0, sizeof(kBin0), 4U, ctx.GetStreamId()},
        std::move(args0)));
    gert::AnnotatedKernelArgs args1(gert::OutputAddr{0U, output->GetAddr()});
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_multi_launch_kernel1", kBin1, sizeof(kBin1), 4U, ctx.GetStreamId()},
        std::move(args1));
  }
};

class MockAnnotatedArgsWithMismatchStreamCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x91U, 0x92U};
    auto input = ctx.GetInputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()});
    return ctx.AddLaunch(gert::AnnotatedKernelLaunchInfo{"custom_mismatch_stream_kernel", kBin, sizeof(kBin), 4U,
                                                         ctx.GetStreamId() + 1U},
                         std::move(args));
  }
};

std::atomic_uint32_t g_mismatch_workspace_execute_count{0U};

class MockAnnotatedArgsWithMismatchedWorkspaceCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x31U, 0x32U, 0x33U, 0x34U};
    const uint32_t count = g_mismatch_workspace_execute_count.fetch_add(1U);
    const size_t workspace_size = (count == 0U) ? 100U : 200U;
    auto workspace = ctx.MallocWorkSpace(workspace_size);
    GE_ASSERT_NOTNULL(workspace.addr);
    auto input = ctx.GetInputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()}, workspace);
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_mismatch_workspace_kernel", kBin, sizeof(kBin), 4U, ctx.GetStreamId()},
        std::move(args));
  }
};

class MockAnnotatedArgsWithConstInputAndWorkspaceCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x41U, 0x42U, 0x43U, 0x44U};
    auto input0 = ctx.GetInputTensor(0U);
    auto input1 = ctx.GetInputTensor(1U);
    auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input0);
    GE_ASSERT_NOTNULL(input1);
    GE_ASSERT_NOTNULL(output);
    auto workspace = ctx.MallocWorkSpace(100U);
    GE_ASSERT_NOTNULL(workspace.addr);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input0->GetAddr()}, gert::InputAddr{1U, input1->GetAddr()},
                                   gert::OutputAddr{0U, output->GetAddr()}, workspace);
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_const_workspace_kernel", kBin, sizeof(kBin), 4U, ctx.GetStreamId()},
        std::move(args));
  }
};

enum class InvalidAnnotatedArgsCase {
  kEmptyName,
  kEmptyBin,
  kZeroBlockDim,
  kEmptyArgs,
};

class MockAnnotatedArgsWithoutPortableCustomOp : public AnnotatedArgsOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0xA1U, 0xA2U, 0xA3U, 0xA4U};
    auto input = ctx.GetInputTensor(0U);
    auto output = ctx.GetOutputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    GE_ASSERT_NOTNULL(output);
    gert::AnnotatedKernelArgs args(gert::InputAddr{0U, input->GetAddr()}, gert::OutputAddr{0U, output->GetAddr()},
                                   uint64_t{9U});
    return ctx.AddLaunch(
        gert::AnnotatedKernelLaunchInfo{"custom_without_portable_kernel", kBin, sizeof(kBin), 2U, ctx.GetStreamId()},
        std::move(args));
  }
};

class MockAnnotatedArgsWithoutAddLaunchCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    (void)ctx;
    return GRAPH_SUCCESS;
  }
};

class MockAnnotatedArgsReturnsInternalErrorCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    (void)ctx;
    return INTERNAL_ERROR;
  }
};

class MockInvalidAnnotatedArgsCustomOp : public AnnotatedArgsOp, public MockPortableCustomOp {
 public:
  explicit MockInvalidAnnotatedArgsCustomOp(const InvalidAnnotatedArgsCase invalid_case)
      : invalid_case_(invalid_case) {}

  graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) override {
    static const uint8_t kBin[] = {0x11U, 0x22U, 0x33U, 0x44U};
    auto input = ctx.GetInputTensor(0U);
    GE_ASSERT_NOTNULL(input);
    gert::AnnotatedKernelArgs valid_args(gert::InputAddr{0U, input->GetAddr()});
    gert::AnnotatedKernelArgs empty_args;
    switch (invalid_case_) {
      case InvalidAnnotatedArgsCase::kEmptyName:
        return ctx.AddLaunch(gert::AnnotatedKernelLaunchInfo{"", kBin, sizeof(kBin), 8U, ctx.GetStreamId()},
                             std::move(valid_args));
      case InvalidAnnotatedArgsCase::kEmptyBin:
        return ctx.AddLaunch(gert::AnnotatedKernelLaunchInfo{"custom_add_kernel", kBin, 0U, 8U, ctx.GetStreamId()},
                             std::move(valid_args));
      case InvalidAnnotatedArgsCase::kZeroBlockDim:
        return ctx.AddLaunch(
            gert::AnnotatedKernelLaunchInfo{"custom_add_kernel", kBin, sizeof(kBin), 0U, ctx.GetStreamId()},
            std::move(valid_args));
      case InvalidAnnotatedArgsCase::kEmptyArgs:
        return ctx.AddLaunch(
            gert::AnnotatedKernelLaunchInfo{"custom_add_kernel", kBin, sizeof(kBin), 8U, ctx.GetStreamId()},
            std::move(empty_args));
      default:
        return GRAPH_FAILED;
    }
  }

 private:
  InvalidAnnotatedArgsCase invalid_case_;
};

class UtestCustomOpsKernelInfoStore : public testing::Test {
 protected:
  void SetUp() override {
    graph_options_ = GetThreadLocalContext().GetAllGraphOptions();
    session_options_ = GetThreadLocalContext().GetAllSessionOptions();
    global_options_ = GetThreadLocalContext().GetAllGlobalOptions();
    GetThreadLocalContext().SetGraphOption({{ge::SOC_VERSION, "KirinX90"}});
  }

  void TearDown() override {
    GetThreadLocalContext().SetGraphOption(graph_options_);
    GetThreadLocalContext().SetSessionOption(session_options_);
    GetThreadLocalContext().SetGlobalOption(global_options_);
  }

 private:
  std::map<std::string, std::string> graph_options_;
  std::map<std::string, std::string> session_options_;
  std::map<std::string, std::string> global_options_;
};

NodePtr BuildStaticCustomNode(const std::string &op_type, ComputeGraphPtr &graph) {
  graph = std::make_shared<ComputeGraph>("custom_builder_graph");
  auto op_desc = std::make_shared<OpDesc>("custom_builder_node", op_type);
  op_desc->SetId(7);
  op_desc->SetStreamId(3);
  op_desc->AppendIrInput("x", kIrInputRequired);
  op_desc->AppendIrOutput("y", kIrOutputRequired);

  GeTensorDesc input_desc(GeShape({1, 16}), FORMAT_ND, DT_FLOAT16);
  input_desc.SetOriginShape(GeShape({1, 16}));
  GeTensorDesc output_desc(GeShape({1, 16}), FORMAT_ND, DT_FLOAT16);
  output_desc.SetOriginShape(GeShape({1, 16}));
  (void)op_desc->AddInputDesc("x", input_desc);
  (void)op_desc->AddOutputDesc("y", output_desc);
  op_desc->SetInputOffset({1024});
  op_desc->SetOutputOffset({2048});
  auto node = graph->AddNode(op_desc);
  if (node != nullptr) {
    node->GetOpDesc()->SetId(7);
  }
  return node;
}

NodePtr BuildStaticCustomNodeWithConstInput(const std::string &op_type, ComputeGraphPtr &graph) {
  auto node = BuildStaticCustomNode(op_type, graph);
  if (node == nullptr) {
    return nullptr;
  }
  auto op_desc = node->GetOpDesc();
  if (op_desc == nullptr) {
    return nullptr;
  }

  op_desc->AppendIrInput("w", kIrInputRequired);
  GeTensorDesc weight_desc(GeShape({1, 16}), FORMAT_ND, DT_FLOAT16);
  weight_desc.SetOriginShape(GeShape({1, 16}));
  TensorUtils::SetDataOffset(weight_desc, 4096);
  (void)op_desc->AddInputDesc("w", weight_desc);
  op_desc->SetInputOffset({1024, 8192});
  op_desc->SetIsInputConst({false, true});
  return node;
}

NodePtr BuildStaticCustomNodeWithInvalidOptionalInput(const std::string &op_type, const bool last_input_is_const,
                                                      ComputeGraphPtr &graph) {
  graph = std::make_shared<ComputeGraph>("custom_builder_invalid_optional_graph");
  auto op_desc = std::make_shared<OpDesc>("custom_builder_invalid_optional_node", op_type);
  op_desc->SetId(7);
  op_desc->SetStreamId(3);
  op_desc->AppendIrInput("x", kIrInputRequired);
  op_desc->AppendIrInput("optional", kIrInputOptional);
  const std::string last_input_name = last_input_is_const ? "w" : "z";
  op_desc->AppendIrInput(last_input_name, kIrInputRequired);
  op_desc->AppendIrOutput("y", kIrOutputRequired);

  GeTensorDesc input_desc(GeShape({1, 16}), FORMAT_ND, DT_FLOAT16);
  input_desc.SetOriginShape(GeShape({1, 16}));
  GeTensorDesc invalid_optional_desc(GeShape(), FORMAT_RESERVED, DT_UNDEFINED);
  GeTensorDesc last_input_desc(GeShape({1, 16}), FORMAT_ND, DT_FLOAT16);
  last_input_desc.SetOriginShape(GeShape({1, 16}));
  if (last_input_is_const) {
    TensorUtils::SetDataOffset(last_input_desc, 4096);
  }
  GeTensorDesc output_desc(GeShape({1, 16}), FORMAT_ND, DT_FLOAT16);
  output_desc.SetOriginShape(GeShape({1, 16}));

  (void)op_desc->AddInputDesc("x", input_desc);
  (void)op_desc->AddOptionalInputDesc("optional", invalid_optional_desc);
  (void)op_desc->AddInputDesc(last_input_name, last_input_desc);
  (void)op_desc->AddOutputDesc("y", output_desc);
  op_desc->SetInputOffset(last_input_is_const ? std::vector<int64_t>{1024, 8192} : std::vector<int64_t>{1024, 3072});
  op_desc->SetOutputOffset({2048});
  op_desc->SetIsInputConst({false, false, last_input_is_const});

  auto node = graph->AddNode(op_desc);
  if (node != nullptr) {
    node->GetOpDesc()->SetId(7);
  }
  return node;
}

Status GenerateTaskForNode(const NodePtr &node, std::vector<domi::TaskDef> &tasks) {
  CustomOpsKernelBuilder builder;
  RunContext context = {};
  context.dataMemBase = reinterpret_cast<uint8_t *>(kLogicDataMemBase);
  context.dataMemSize = 4096U;
  context.weightMemBase = reinterpret_cast<uint8_t *>(kLogicWeightMemBase);
  context.weightMemSize = 4096U;
  return builder.GenerateTask(*node, context, tasks);
}

void FinalizeCustomWorkspaceForDirectBuilderTest(const NodePtr &node, const int64_t workspace_offset) {
  ASSERT_NE(node, nullptr);
  auto op_desc = node->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  std::vector<int64_t> append_ws;
  ASSERT_TRUE(AttrUtils::GetListInt(op_desc, "_append_ws", append_ws));
  ASSERT_EQ(append_ws.size(), 1U);
  op_desc->SetWorkspace({workspace_offset});
  op_desc->SetWorkspaceBytes(append_ws);
}

void ExpectGenerateTaskFailed(const std::string &op_type) {
  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(op_type, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  EXPECT_NE(GenerateTaskForNode(node, tasks), SUCCESS);
  EXPECT_TRUE(tasks.empty());
}

void ExpectGenerateTaskFailedWithStatus(const std::string &op_type, const Status expected_status) {
  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(op_type, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  EXPECT_EQ(GenerateTaskForNode(node, tasks), expected_status);
  EXPECT_TRUE(tasks.empty());
}

TEST_F(UtestCustomOpsKernelInfoStore, InitializeSuccess) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
}

TEST_F(UtestCustomOpsKernelInfoStore, FinalizeSuccess) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
  EXPECT_EQ(store.Finalize(), SUCCESS);
}

TEST_F(UtestCustomOpsKernelInfoStore, RefreshSuccess) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);
  EXPECT_EQ(store.Refresh(), SUCCESS);
}

TEST_F(UtestCustomOpsKernelInfoStore, GetAllOpsKernelInfo) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);

  std::map<std::string, OpInfo> infos;
  store.GetAllOpsKernelInfo(infos);

  std::vector<AscendString> registered_ops;
  CustomOpFactory::GetAllRegisteredOps(registered_ops);
  EXPECT_EQ(infos.size(), registered_ops.size());
}

TEST_F(UtestCustomOpsKernelInfoStore, CheckSupported) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);

  std::vector<AscendString> registered_ops;
  CustomOpFactory::GetAllRegisteredOps(registered_ops);

  if (!registered_ops.empty()) {
    auto op_desc = std::make_shared<OpDesc>(std::string(registered_ops[0].GetString()),
                                            std::string(registered_ops[0].GetString()));
    std::string reason;
    EXPECT_TRUE(store.CheckSupported(op_desc, reason));
  }
}

TEST_F(UtestCustomOpsKernelInfoStore, RefreshCapturesNewRegisteredOp) {
  const std::string kTestOpType = "TestDynamicRegisteredOp_RefreshTest";

  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);

  std::map<std::string, OpInfo> infos_before;
  store.GetAllOpsKernelInfo(infos_before);
  size_t count_before = infos_before.size();
  EXPECT_EQ(infos_before.count(kTestOpType), 0U);

  auto creator = []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MockCustomOp>(); };
  EXPECT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  std::map<std::string, OpInfo> infos_no_refresh;
  store.GetAllOpsKernelInfo(infos_no_refresh);
  EXPECT_EQ(infos_no_refresh.count(kTestOpType), 0U);

  EXPECT_EQ(store.Refresh(), SUCCESS);

  std::map<std::string, OpInfo> infos_after;
  store.GetAllOpsKernelInfo(infos_after);
  EXPECT_GT(infos_after.size(), count_before);
  EXPECT_NE(infos_after.count(kTestOpType), 0U);

  auto op_desc = std::make_shared<OpDesc>(kTestOpType, kTestOpType);
  std::string reason;
  EXPECT_TRUE(store.CheckSupported(op_desc, reason));
}

TEST_F(UtestCustomOpsKernelInfoStore, ThreadSafety) {
  CustomOpsKernelInfoStore store;
  std::map<std::string, std::string> options;
  EXPECT_EQ(store.Initialize(options), SUCCESS);

  std::map<std::string, OpInfo> infos1;
  std::map<std::string, OpInfo> infos2;

  store.GetAllOpsKernelInfo(infos1);
  store.GetAllOpsKernelInfo(infos2);

  EXPECT_EQ(infos1.size(), infos2.size());
}

TEST_F(UtestCustomOpsKernelInfoStore, CustomGraphOptimizerCompileRunsDuringWholeGraphOnly) {
  const std::string kTestOpType = "TestCompilableOp_CompileDuringWholeGraphOnly";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MockCompilableCustomOp>(); };
  const auto register_ret = CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator);
  EXPECT_TRUE((register_ret == GRAPH_SUCCESS) || (register_ret == GRAPH_FAILED));

  auto graph = ComGraphMakeShared<ComputeGraph>("custom_compile_hook_graph");
  ASSERT_NE(graph, nullptr);
  auto op_desc = ComGraphMakeShared<OpDesc>("custom_compile_hook_node", kTestOpType);
  ASSERT_NE(op_desc, nullptr);
  ASSERT_NE(graph->AddNode(op_desc), nullptr);

  MockCompilableCustomOp::ResetCompileCount();
  CustomGraphOptimizer optimizer;
  EXPECT_EQ(optimizer.OptimizeWholeGraph(*graph), SUCCESS);
  EXPECT_EQ(MockCompilableCustomOp::GetCompileCount(), 1);

  EXPECT_EQ(optimizer.OptimizeGraphBeforeBuild(*graph), SUCCESS);
  EXPECT_EQ(MockCompilableCustomOp::GetCompileCount(), 1);
}

TEST_F(UtestCustomOpsKernelInfoStore, OOptimizeWholeGraphConstructsCompileContextOutputs) {
  const std::string kTestOpType = "TestCompileContextOutputOp_OptimizerTest";
  auto graph = std::make_shared<ComputeGraph>("compile_context_output_graph");
  auto op_desc = std::make_shared<OpDesc>("compile_context_output_op", kTestOpType);
  op_desc->AppendIrOutput("y", kIrOutputRequired);
  op_desc->AppendIrOutput("dy", kIrOutputDynamic);

  GeTensorDesc required_output_desc(GeShape({8, 16}), FORMAT_ND, DT_FLOAT16);
  op_desc->AddOutputDesc("y", required_output_desc);
  GeTensorDesc dynamic_output_desc0(GeShape({16, 16}), FORMAT_ND, DT_FLOAT);
  op_desc->AddOutputDesc("dy0", dynamic_output_desc0);
  GeTensorDesc dynamic_output_desc1(GeShape({32, 16}), FORMAT_ND, DT_INT32);
  op_desc->AddOutputDesc("dy1", dynamic_output_desc1);
  ASSERT_NE(graph->AddNode(op_desc), nullptr);

  auto creator = []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MockCompileContextOutputOp>(); };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  g_compile_context_output_called.store(false);
  CustomGraphOptimizer optimizer;
  EXPECT_EQ(optimizer.OptimizeWholeGraph(*graph), SUCCESS);
  EXPECT_TRUE(g_compile_context_output_called.load());
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskDeclaresAnnotatedArgsAndFillsKernelDef) {
  const std::string kTestOpType = "TestAnnotatedArgsCustomOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithConstInputCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNodeWithConstInput(kTestOpType, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  EXPECT_EQ(GenerateTaskForNode(node, tasks), SUCCESS);

  ASSERT_EQ(tasks.size(), 1U);
  const auto &task = tasks[0];
  EXPECT_EQ(task.stream_id(), 3U);
  EXPECT_EQ(task.type(), static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  const auto &kernel = task.kernel();
  EXPECT_EQ(kernel.stub_func(), "custom_add_kernel");
  EXPECT_EQ(kernel.kernel_name(), "custom_add_kernel");
  EXPECT_EQ(kernel.block_dim(), 8U);
  EXPECT_EQ(kernel.args_size(), 32U);
  ASSERT_EQ(kernel.args().size(), 32);

  EXPECT_EQ(ReadUint64Slot(kernel.args(), 0U), static_cast<uint64_t>(kLogicDataMemBase + 1024U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 1U), static_cast<uint64_t>(kLogicWeightMemBase + 4096U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 2U), static_cast<uint64_t>(kLogicDataMemBase + 2048U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 3U), 7U);

  const auto &kernel_context = kernel.context();
  EXPECT_EQ(kernel_context.op_index(), 7);
  EXPECT_EQ(kernel_context.args_count(), 4U);
  ASSERT_EQ(kernel_context.args_offset().size(), static_cast<int>(4U * sizeof(uint16_t)));
  EXPECT_EQ(ReadUint16Slot(kernel_context.args_offset(), 0U), 0U);
  EXPECT_EQ(ReadUint16Slot(kernel_context.args_offset(), 1U), 8U);
  EXPECT_EQ(ReadUint16Slot(kernel_context.args_offset(), 2U), 16U);
  EXPECT_EQ(ReadUint16Slot(kernel_context.args_offset(), 3U), 24U);

  std::vector<ArgDesc> parsed_arg_descs;
  ASSERT_EQ(ArgsFormatDescUtils::Parse(kernel_context.args_format(), parsed_arg_descs), GRAPH_SUCCESS);
  ASSERT_EQ(parsed_arg_descs.size(), 4U);
  EXPECT_EQ(parsed_arg_descs[0].addr_type, AddrType::INPUT);
  EXPECT_EQ(parsed_arg_descs[0].ir_idx, 0);
  EXPECT_EQ(parsed_arg_descs[1].addr_type, AddrType::INPUT);
  EXPECT_EQ(parsed_arg_descs[1].ir_idx, 1);
  EXPECT_EQ(parsed_arg_descs[2].addr_type, AddrType::OUTPUT);
  EXPECT_EQ(parsed_arg_descs[2].ir_idx, 0);
  EXPECT_EQ(parsed_arg_descs[3].addr_type, AddrType::CUSTOM_VALUE);

  auto op_desc = node->GetOpDesc();
  const auto *kernel_name = AttrUtils::GetStr(op_desc, ATTR_NAME_TBE_KERNEL_NAME);
  ASSERT_NE(kernel_name, nullptr);
  EXPECT_EQ(*kernel_name, "custom_add_kernel");
  auto tbe_kernel = op_desc->TryGetExtAttr(OP_EXTATTR_NAME_TBE_KERNEL, OpKernelBinPtr());
  ASSERT_NE(tbe_kernel, nullptr);
  EXPECT_EQ(tbe_kernel->GetName(), "custom_add_kernel");
  ASSERT_EQ(tbe_kernel->GetBinDataSize(), 4U);
  const uint8_t expected_bin[] = {0x11U, 0x22U, 0x33U, 0x44U};
  EXPECT_EQ(std::memcmp(tbe_kernel->GetBinData(), expected_bin, sizeof(expected_bin)), 0);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskSupportsInvalidOptionalBeforeRequiredInput) {
  const std::string kTestOpType = "TestInvalidOptionalBeforeRequiredInput_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithInvalidOptionalAndRequiredInputCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNodeWithInvalidOptionalInput(kTestOpType, false, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  ASSERT_EQ(GenerateTaskForNode(node, tasks), SUCCESS);

  ASSERT_EQ(tasks.size(), 1U);
  const auto &kernel = tasks[0].kernel();
  ASSERT_EQ(kernel.args().size(), 24);
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 0U), static_cast<uint64_t>(kLogicDataMemBase + 1024U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 1U), static_cast<uint64_t>(kLogicDataMemBase + 3072U));
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskSupportsInvalidOptionalBeforeConstInput) {
  const std::string kTestOpType = "TestInvalidOptionalBeforeConstInput_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithInvalidOptionalAndConstInputCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNodeWithInvalidOptionalInput(kTestOpType, true, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  ASSERT_EQ(GenerateTaskForNode(node, tasks), SUCCESS);

  ASSERT_EQ(tasks.size(), 1U);
  const auto &kernel = tasks[0].kernel();
  ASSERT_EQ(kernel.args().size(), 24);
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 0U), static_cast<uint64_t>(kLogicDataMemBase + 1024U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 1U), static_cast<uint64_t>(kLogicWeightMemBase + 4096U));
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskOnNonMobileSocFillsBasicCustomKernelTask) {
  GetThreadLocalContext().SetGraphOption({{ge::SOC_VERSION, "Ascend910B"}});

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode("TestUnregisteredCustomOp_NonMobileSoc", graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  EXPECT_EQ(GenerateTaskForNode(node, tasks), SUCCESS);

  ASSERT_EQ(tasks.size(), 1U);
  const auto &task = tasks[0];
  EXPECT_EQ(task.stream_id(), 3U);
  EXPECT_EQ(task.type(), static_cast<uint32_t>(ModelTaskType::MODEL_TASK_CUSTOM_KERNEL));
  EXPECT_EQ(task.sqe_num(), 5U);
  EXPECT_EQ(task.kernel().context().op_index(), 7);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskProbeRecordsDynamicWorkspace) {
  const std::string kTestOpType = "TestAnnotatedArgsWithWorkspaceProbe_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithWorkspaceCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(kTestOpType, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  EXPECT_EQ(GenerateTaskForNode(node, tasks), SUCCESS);

  std::vector<int64_t> append_ws;
  ASSERT_TRUE(AttrUtils::GetListInt(node->GetOpDesc(), "_append_ws", append_ws));
  ASSERT_EQ(append_ws.size(), 1U);
  EXPECT_EQ(append_ws[0], 512);

  bool custom_append_ws = false;
  EXPECT_TRUE(AttrUtils::GetBool(node->GetOpDesc(), "_custom_omc_append_ws", custom_append_ws));
  EXPECT_TRUE(custom_append_ws);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskSupportsMultipleWorkspaceArgs) {
  const std::string kTestOpType = "TestAnnotatedArgsWithMultipleWorkspaces_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithMultipleWorkspacesCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(kTestOpType, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  EXPECT_EQ(GenerateTaskForNode(node, tasks), SUCCESS);

  ASSERT_EQ(tasks.size(), 1U);
  const auto &kernel = tasks[0].kernel();
  ASSERT_EQ(kernel.args().size(), 32);
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 0U), static_cast<uint64_t>(kLogicDataMemBase + 1024U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 1U), static_cast<uint64_t>(kLogicDataMemBase + 2048U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 2U), static_cast<uint64_t>(kLogicDataMemBase + 4096U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 3U), static_cast<uint64_t>(kLogicDataMemBase + 4096U + 512U));

  std::vector<ArgDesc> parsed_arg_descs;
  ASSERT_EQ(ArgsFormatDescUtils::Parse(kernel.context().args_format(), parsed_arg_descs), GRAPH_SUCCESS);
  ASSERT_EQ(parsed_arg_descs.size(), 4U);
  EXPECT_EQ(parsed_arg_descs[2].addr_type, AddrType::WORKSPACE);
  EXPECT_EQ(parsed_arg_descs[2].ir_idx, 0);
  EXPECT_EQ(parsed_arg_descs[3].addr_type, AddrType::WORKSPACE);
  EXPECT_EQ(parsed_arg_descs[3].ir_idx, 1);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWhenDeclareAddsMultipleLaunches) {
  const std::string kTestOpType = "TestMultipleLaunchesCustomOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithMultipleLaunchesCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ExpectGenerateTaskFailedWithStatus(kTestOpType, INTERNAL_ERROR);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWhenDeclareSetsMismatchStreamId) {
  const std::string kTestOpType = "TestMismatchStreamCustomOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithMismatchStreamCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ExpectGenerateTaskFailedWithStatus(kTestOpType, INTERNAL_ERROR);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFinalDeclareUsesPlannedWorkspaceOffset) {
  const std::string kTestOpType = "TestAnnotatedArgsWithWorkspaceFinal_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithWorkspaceCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(kTestOpType, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> probe_tasks;
  ASSERT_EQ(GenerateTaskForNode(node, probe_tasks), SUCCESS);
  FinalizeCustomWorkspaceForDirectBuilderTest(node, 4096);

  CustomOpsKernelBuilder builder;
  RunContext final_context = {};
  final_context.dataMemBase = reinterpret_cast<uint8_t *>(kLogicDataMemBase);
  final_context.dataMemSize = 4608U;
  final_context.weightMemBase = reinterpret_cast<uint8_t *>(kLogicWeightMemBase);
  final_context.weightMemSize = 4096U;

  std::vector<domi::TaskDef> final_tasks;
  EXPECT_EQ(builder.GenerateTask(*node, final_context, final_tasks), SUCCESS);

  ASSERT_EQ(final_tasks.size(), 1U);
  const auto &kernel = final_tasks[0].kernel();
  ASSERT_EQ(kernel.args().size(), 24);
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 0U), static_cast<uint64_t>(kLogicDataMemBase + 1024U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 1U), static_cast<uint64_t>(kLogicDataMemBase + 2048U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 2U), static_cast<uint64_t>(kLogicDataMemBase + 4096U));

  std::vector<ArgDesc> parsed_arg_descs;
  ASSERT_EQ(ArgsFormatDescUtils::Parse(kernel.context().args_format(), parsed_arg_descs), GRAPH_SUCCESS);
  ASSERT_EQ(parsed_arg_descs.size(), 3U);
  EXPECT_EQ(parsed_arg_descs[2].addr_type, AddrType::WORKSPACE);
  EXPECT_EQ(parsed_arg_descs[2].ir_idx, 0);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWhenFinalWorkspaceSizeDiffersFromProbe) {
  const std::string kTestOpType = "TestAnnotatedArgsWithWorkspaceMismatch_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithMismatchedWorkspaceCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  g_mismatch_workspace_execute_count.store(0U);
  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(kTestOpType, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> probe_tasks;
  ASSERT_EQ(GenerateTaskForNode(node, probe_tasks), SUCCESS);
  FinalizeCustomWorkspaceForDirectBuilderTest(node, 4096);

  std::vector<domi::TaskDef> final_tasks;
  EXPECT_NE(GenerateTaskForNode(node, final_tasks), SUCCESS);
  EXPECT_TRUE(final_tasks.empty());
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFinalDeclareRefreshesConstInputWeightBase) {
  const std::string kTestOpType = "TestConstInputWorkspaceFinal_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithConstInputAndWorkspaceCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNodeWithConstInput(kTestOpType, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> probe_tasks;
  ASSERT_EQ(GenerateTaskForNode(node, probe_tasks), SUCCESS);
  FinalizeCustomWorkspaceForDirectBuilderTest(node, 4096);

  CustomOpsKernelBuilder builder;
  RunContext final_context = {};
  final_context.dataMemBase = reinterpret_cast<uint8_t *>(kLogicDataMemBase);
  final_context.dataMemSize = 4608U;
  final_context.weightMemBase = reinterpret_cast<uint8_t *>(kLogicWeightMemBase + 512U);
  final_context.weightMemSize = 4096U;

  std::vector<domi::TaskDef> final_tasks;
  EXPECT_EQ(builder.GenerateTask(*node, final_context, final_tasks), SUCCESS);

  ASSERT_EQ(final_tasks.size(), 1U);
  const auto &kernel = final_tasks[0].kernel();
  ASSERT_EQ(kernel.args().size(), 32);
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 0U), static_cast<uint64_t>(kLogicDataMemBase + 1024U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 1U), static_cast<uint64_t>(kLogicWeightMemBase + 512U + 4096U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 2U), static_cast<uint64_t>(kLogicDataMemBase + 2048U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 3U), static_cast<uint64_t>(kLogicDataMemBase + 4096U));
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWhenCustomOpNotRegistered) {
  ExpectGenerateTaskFailed("TestUnregisteredCustomOp_BuilderTest");
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWhenRegisteredOpIsNotAnnotatedArgsOp) {
  const std::string kTestOpType = "TestBaseOnlyCustomOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MockBaseOnlyCustomOp>(); };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ExpectGenerateTaskFailed(kTestOpType);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskSucceedsWhenRegisteredOpIsNotPortableOp) {
  const std::string kTestOpType = "TestAnnotatedArgsWithoutPortableOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithoutPortableCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(kTestOpType, graph);
  ASSERT_NE(node, nullptr);

  std::vector<domi::TaskDef> tasks;
  EXPECT_EQ(GenerateTaskForNode(node, tasks), SUCCESS);

  ASSERT_EQ(tasks.size(), 1U);
  const auto &kernel = tasks[0].kernel();
  EXPECT_EQ(kernel.kernel_name(), "custom_without_portable_kernel");
  EXPECT_EQ(kernel.block_dim(), 2U);
  ASSERT_EQ(kernel.args().size(), 24);
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 0U), static_cast<uint64_t>(kLogicDataMemBase + 1024U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 1U), static_cast<uint64_t>(kLogicDataMemBase + 2048U));
  EXPECT_EQ(ReadUint64Slot(kernel.args(), 2U), 9U);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWhenDeclareDoesNotAddLaunch) {
  const std::string kTestOpType = "TestNoAnnotatedArgsCustomOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsWithoutAddLaunchCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ExpectGenerateTaskFailedWithStatus(kTestOpType, INTERNAL_ERROR);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWithGraphFailedWhenDeclareLaunchArgsFails) {
  const std::string kTestOpType = "TestDeclareLaunchArgsFailsCustomOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> {
    return std::make_unique<MockAnnotatedArgsReturnsInternalErrorCustomOp>();
  };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ExpectGenerateTaskFailedWithStatus(kTestOpType, GRAPH_FAILED);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskSupportsUnknownTensorShapesInKnownGraph) {
  const std::string kTestOpType = "TestUnknownTensorShapesCustomOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MockAnnotatedArgsCustomOp>(); };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(kTestOpType, graph);
  ASSERT_NE(node, nullptr);
  auto op_desc = node->GetOpDesc();
  ASSERT_NE(op_desc, nullptr);
  op_desc->MutableInputDesc(0U)->SetShape(GeShape({-1, 16}));
  op_desc->MutableOutputDesc(0U)->SetShape(GeShape({-1, 16}));

  std::vector<domi::TaskDef> tasks;
  EXPECT_EQ(GenerateTaskForNode(node, tasks), SUCCESS);
  EXPECT_EQ(tasks.size(), 1U);
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWhenOwnerGraphIsUnknown) {
  const std::string kTestOpType = "TestUnknownGraphCustomOp_BuilderTest";
  auto creator = []() -> std::unique_ptr<BaseCustomOp> { return std::make_unique<MockAnnotatedArgsCustomOp>(); };
  ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(kTestOpType.c_str()), creator), GRAPH_SUCCESS);

  ComputeGraphPtr graph;
  auto node = BuildStaticCustomNode(kTestOpType, graph);
  ASSERT_NE(node, nullptr);
  auto *const owner_graph = node->GetOwnerComputeGraphBarePtr();
  ASSERT_NE(owner_graph, nullptr);
  owner_graph->SetGraphUnknownFlag(true);

  std::vector<domi::TaskDef> tasks;
  EXPECT_NE(GenerateTaskForNode(node, tasks), SUCCESS);
  EXPECT_TRUE(tasks.empty());
}

TEST_F(UtestCustomOpsKernelInfoStore, GenerateTaskFailsWhenAnnotatedArgsRejectsParameters) {
  const std::vector<std::pair<std::string, InvalidAnnotatedArgsCase>> cases = {
      {"TestInvalidEmptyKernelNameCustomOp_BuilderTest", InvalidAnnotatedArgsCase::kEmptyName},
      {"TestInvalidEmptyKernelBinCustomOp_BuilderTest", InvalidAnnotatedArgsCase::kEmptyBin},
      {"TestInvalidZeroBlockDimCustomOp_BuilderTest", InvalidAnnotatedArgsCase::kZeroBlockDim},
      {"TestInvalidEmptyArgsCustomOp_BuilderTest", InvalidAnnotatedArgsCase::kEmptyArgs},
  };

  for (const auto &test_case : cases) {
    const std::string &op_type = test_case.first;
    const InvalidAnnotatedArgsCase invalid_case = test_case.second;
    auto creator = [invalid_case]() -> std::unique_ptr<BaseCustomOp> {
      return std::make_unique<MockInvalidAnnotatedArgsCustomOp>(invalid_case);
    };
    ASSERT_EQ(CustomOpFactory::RegisterCustomOpCreator(AscendString(op_type.c_str()), creator), GRAPH_SUCCESS);
    ExpectGenerateTaskFailed(op_type);
  }
}

}  // namespace custom
}  // namespace ge
