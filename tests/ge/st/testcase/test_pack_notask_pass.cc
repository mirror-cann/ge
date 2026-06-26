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
#include <array>
#include <vector>
#include "ge_graph_dsl/graph_dsl.h"
#include "ge/ge_api.h"
#include "graph/utils/graph_utils.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/op_desc_utils.h"
#include "graph/utils/tensor_utils.h"
#include "ge_running_env/ge_running_env_faker.h"
#include "ge_running_env/fake_op.h"
#include "ge_running_env/op_reg.h"
#include "ge_running_env/tensor_utils.h"
#include "utils/mock_ops_kernel_builder.h"
#include "common/opskernel/ops_kernel_info_types.h"

namespace ge {
namespace {
constexpr char kAiCoreLib[] = "AiCoreLib";
constexpr char kAiCoreEngine[] = "AIcoreEngine";
constexpr char kGeLocalEngine[] = "DNN_VM_GE_LOCAL";
constexpr char kGeLocalOpStore[] = "DNN_VM_GE_LOCAL_OP_STORE";
constexpr char kDataPackNode[] = "pack_data";
constexpr char kConstPackNode[] = "pack_const";
constexpr char kDumpFile[] = "/tmp/pack_notask_pass_after_build.txt";
constexpr int64_t kPackInputNum = 4;

GeTensorPtr MakeShapeTensor(const std::vector<int64_t> &shape_value) {
  GeTensorDesc desc(GeShape({static_cast<int64_t>(shape_value.size())}), FORMAT_ND, DT_INT64);
  auto data = std::vector<int64_t>(shape_value);
  return std::make_shared<GeTensor>(desc, reinterpret_cast<const uint8_t *>(data.data()),
                                    data.size() * sizeof(int64_t));
}

void MockGenerateTask() {
  auto aicore_func = [](const ge::Node &node, RunContext &, std::vector<domi::TaskDef> &tasks) -> Status {
    auto op_desc = node.GetOpDesc();
    op_desc->SetOpKernelLibName(kAiCoreLib);
    std::vector<uint8_t> args(64U, 0U);
    domi::TaskDef task_def;
    task_def.set_type(static_cast<uint32_t>(ModelTaskType::MODEL_TASK_KERNEL));
    auto kernel_info = task_def.mutable_kernel();
    kernel_info->set_args(args.data(), args.size());
    kernel_info->set_args_size(args.size());
    kernel_info->mutable_context()->set_kernel_type(2U);  // ccKernelType::TE
    kernel_info->set_kernel_name(node.GetName());
    kernel_info->set_block_dim(1U);
    uint16_t args_offset[2] = {0U};
    kernel_info->mutable_context()->set_args_offset(args_offset, sizeof(args_offset));
    kernel_info->mutable_context()->set_op_index(op_desc->GetId());
    tasks.emplace_back(task_def);
    return SUCCESS;
  };
  MockForGenerateTask(kAiCoreLib, aicore_func);
}

graphStatus ForwardInfer(Operator &op) {
  auto op_desc = OpDescUtils::GetOpDescFromOperator(op);
  *op_desc->MutableOutputDesc(0) = *op_desc->GetInputDescPtr(0);
  return GRAPH_SUCCESS;
}

graphStatus PackInfer(Operator &op) {
  auto op_desc = OpDescUtils::GetOpDescFromOperator(op);
  auto output_desc = *op_desc->GetInputDescPtr(0);
  auto output_dims = output_desc.GetShape().GetDims();

  int64_t axis = 0;
  int64_t n = 0;
  (void)AttrUtils::GetInt(op_desc, "axis", axis);
  (void)AttrUtils::GetInt(op_desc, "N", n);
  if (axis < 0) {
    axis += static_cast<int64_t>(output_dims.size()) + 1;
  }

  output_dims.insert(output_dims.begin() + axis, n);
  output_desc.SetShape(GeShape(output_dims));
  output_desc.SetOriginShape(GeShape(output_dims));
  *op_desc->MutableOutputDesc(0) = output_desc;
  return GRAPH_SUCCESS;
}

graphStatus BatchMatMulInfer(Operator &op) {
  auto op_desc = OpDescUtils::GetOpDescFromOperator(op);
  auto output_desc = *op_desc->GetInputDescPtr(0);
  const auto input_y_dims = op_desc->GetInputDescPtr(1)->GetShape().GetDims();
  auto output_dims = output_desc.GetShape().GetDims();
  if ((output_dims.size() < 2U) || (input_y_dims.size() < 2U)) {
    return GRAPH_PARAM_INVALID;
  }
  output_dims.back() = input_y_dims.back();
  output_desc.SetShape(GeShape(output_dims));
  output_desc.SetOriginShape(GeShape(output_dims));
  *op_desc->MutableOutputDesc(0) = output_desc;
  return GRAPH_SUCCESS;
}

graphStatus SplitInfer(Operator &op) {
  auto op_desc = OpDescUtils::GetOpDescFromOperator(op);
  auto output_desc = *op_desc->GetInputDescPtr(0);
  auto output_dims = output_desc.GetShape().GetDims();

  int64_t split_dim = 0;
  int64_t num_split = 1;
  (void)AttrUtils::GetInt(op_desc, "split_dim", split_dim);
  (void)AttrUtils::GetInt(op_desc, "num_split", num_split);
  if (split_dim < 0) {
    split_dim += static_cast<int64_t>(output_dims.size());
  }
  if ((split_dim < 0) || (static_cast<size_t>(split_dim) >= output_dims.size()) || (num_split <= 0)) {
    return GRAPH_PARAM_INVALID;
  }

  output_dims[static_cast<size_t>(split_dim)] /= num_split;
  output_desc.SetShape(GeShape(output_dims));
  output_desc.SetOriginShape(GeShape(output_dims));
  for (size_t i = 0U; i < op_desc->GetOutputsSize(); ++i) {
    *op_desc->MutableOutputDesc(i) = output_desc;
  }
  return GRAPH_SUCCESS;
}

graphStatus ReshapeInfer(Operator &op) {
  auto op_desc = OpDescUtils::GetOpDescFromOperator(op);
  op_desc->SetOpInferDepends({"shape"});

  Tensor shape_tensor;
  op.GetInputConstData("shape", shape_tensor);

  auto shape_desc = op_desc->MutableInputDesc("shape");
  const auto shape_dims = shape_desc->GetShape().GetDims();
  if (shape_dims.empty()) {
    return GRAPH_PARAM_INVALID;
  }

  const auto dim_num = shape_dims[0];
  const auto shape_data = reinterpret_cast<const int64_t *>(shape_tensor.GetData());
  std::vector<int64_t> output_dims;
  output_dims.reserve(static_cast<size_t>(dim_num));
  for (int64_t i = 0; i < dim_num; ++i) {
    output_dims.push_back(shape_data[i]);
  }

  auto output_desc = *op_desc->MutableInputDesc("x");
  output_desc.SetShape(GeShape(output_dims));
  output_desc.SetOriginShape(GeShape(output_dims));
  *op_desc->MutableOutputDesc("y") = output_desc;
  return GRAPH_SUCCESS;
}

class MockPackNotaskPassEnv {
 public:
  MockPackNotaskPassEnv() {
    auto ge_env = GeRunningEnvFaker();
    ge_env.Reset()
        .Install(FakeEngine(kGeLocalEngine).KernelInfoStore(kGeLocalOpStore))
        .Install(FakeEngine(kAiCoreEngine).KernelInfoStore(kAiCoreLib).GraphOptimizer(kAiCoreEngine))
        .Install(FakeEngine("DNN_VM_RTS").KernelInfoStore(kEngineNameRts))
        .Install(FakeOp(DATA).InfoStoreAndBuilder(kGeLocalOpStore))
        .Install(FakeOp(CONSTANTOP).InfoStoreAndBuilder(kGeLocalOpStore))
        .Install(FakeOp(CONSTANT).InfoStoreAndBuilder(kGeLocalOpStore))
        .Install(FakeOp(VARIABLE).InfoStoreAndBuilder(kGeLocalOpStore))
        .Install(FakeOp(SHAPE).InfoStoreAndBuilder(kGeLocalOpStore))
        .Install(FakeOp(IF).InfoStoreAndBuilder(kGeLocalOpStore))
        .Install(FakeOp(PARTITIONEDCALL).InfoStoreAndBuilder(kGeLocalOpStore))
        .Install(FakeOp(NETOUTPUT).InfoStoreAndBuilder(kGeLocalOpStore))
        .Install(FakeOp("PhonyConcat").InfoStoreAndBuilder(kGeLocalOpStore).InferShape(ForwardInfer))
        .Install(FakeOp("PhonySplit").InfoStoreAndBuilder(kGeLocalOpStore).InferShape(ForwardInfer))
        .Install(FakeOp("Identity").InfoStoreAndBuilder(kAiCoreLib).InferShape(ForwardInfer))
        .Install(FakeOp(RELU).InfoStoreAndBuilder(kAiCoreLib).InferShape(ForwardInfer))
        .Install(FakeOp("Pack").InfoStoreAndBuilder(kAiCoreLib).InferShape(PackInfer))
        .Install(FakeOp(BATCHMATMUL).InfoStoreAndBuilder(kAiCoreLib).InferShape(BatchMatMulInfer))
        .Install(FakeOp("SplitD").InfoStoreAndBuilder(kAiCoreLib).InferShape(SplitInfer))
        .Install(FakeOp(RESHAPE)
                     .Inputs({"x", "shape"})
                     .Outputs({"y"})
                     .AttrsDef("axis", 0)
                     .AttrsDef("num_axes", -1)
                     .InfoStoreAndBuilder(kAiCoreLib)
                     .InferShape(ReshapeInfer))
        .Install(FakeOp(SEND).InfoStoreAndBuilder(kEngineNameRts))
        .Install(FakeOp(RECV).InfoStoreAndBuilder(kEngineNameRts))
        .Install(FakeOp(STREAMACTIVE).InfoStoreAndBuilder(kEngineNameRts));
  }

  ~MockPackNotaskPassEnv() {
    GeRunningEnvFaker().InstallDefault();
  }
};

void SetAiCoreEngine(const ComputeGraphPtr &compute_graph, const std::vector<std::string> &node_names) {
  for (const auto &node_name : node_names) {
    const auto node = compute_graph->FindNode(node_name);
    EXPECT_NE(node, nullptr);
    if (node != nullptr) {
      node->GetOpDesc()->SetOpEngineName(kAiCoreEngine);
    }
  }
}

Graph BuildPackConnectBatchMatmulSplitReshapeGraph(GeTensorPtr const_weight, GeTensorPtr reshape_shape) {
  DEF_GRAPH(g1) {
    auto data_nodes = std::array{OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).Build("data0"),
                                 OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).Build("data1"),
                                 OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).Build("data2"),
                                 OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).Build("data3")};
    auto const_nodes = std::array{OP_CFG(CONSTANT)
                                      .Weight(const_weight)
                                      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 192})
                                      .InCnt(0)
                                      .OutCnt(1)
                                      .Build("const0"),
                                  OP_CFG(CONSTANT)
                                      .Weight(const_weight)
                                      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 192})
                                      .InCnt(0)
                                      .OutCnt(1)
                                      .Build("const1"),
                                  OP_CFG(CONSTANT)
                                      .Weight(const_weight)
                                      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 192})
                                      .InCnt(0)
                                      .OutCnt(1)
                                      .Build("const2"),
                                  OP_CFG(CONSTANT)
                                      .Weight(const_weight)
                                      .TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 192})
                                      .InCnt(0)
                                      .OutCnt(1)
                                      .Build("const3")};
    auto data_pre_nodes =
        std::array{OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).InCnt(1).OutCnt(1).Build("data_pre0"),
                   OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).InCnt(1).OutCnt(1).Build("data_pre1"),
                   OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).InCnt(1).OutCnt(1).Build("data_pre2"),
                   OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).InCnt(1).OutCnt(1).Build("data_pre3")};
    auto const_pre_nodes =
        std::array{OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 192}).InCnt(1).OutCnt(1).Build("const_pre0"),
                   OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 192}).InCnt(1).OutCnt(1).Build("const_pre1"),
                   OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 192}).InCnt(1).OutCnt(1).Build("const_pre2"),
                   OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {256, 192}).InCnt(1).OutCnt(1).Build("const_pre3")};
    auto pack_data = OP_CFG("Pack")
                         .InCnt(kPackInputNum)
                         .OutCnt(1)
                         .TensorDesc(FORMAT_ND, DT_FLOAT16, {4, 100, 256})
                         .Attr("axis", 0)
                         .Attr("N", kPackInputNum)
                         .Build(kDataPackNode);
    auto pack_const = OP_CFG("Pack")
                          .InCnt(kPackInputNum)
                          .OutCnt(1)
                          .TensorDesc(FORMAT_ND, DT_FLOAT16, {4, 256, 192})
                          .Attr("axis", 0)
                          .Attr("N", kPackInputNum)
                          .Build(kConstPackNode);
    auto batch_matmul =
        OP_CFG(BATCHMATMUL).InCnt(2).OutCnt(1).TensorDesc(FORMAT_ND, DT_FLOAT16, {4, 100, 192}).Build("batch_matmul");
    auto split = OP_CFG("SplitD")
                     .InCnt(1)
                     .OutCnt(kPackInputNum)
                     .TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 100, 192})
                     .Attr("split_dim", 0)
                     .Attr("num_split", kPackInputNum)
                     .Build("split");
    auto shape_nodes = std::array{
        OP_CFG(CONSTANT).Weight(reshape_shape).TensorDesc(FORMAT_ND, DT_INT64, {2}).InCnt(0).OutCnt(1).Build("shape0"),
        OP_CFG(CONSTANT).Weight(reshape_shape).TensorDesc(FORMAT_ND, DT_INT64, {2}).InCnt(0).OutCnt(1).Build("shape1"),
        OP_CFG(CONSTANT).Weight(reshape_shape).TensorDesc(FORMAT_ND, DT_INT64, {2}).InCnt(0).OutCnt(1).Build("shape2"),
        OP_CFG(CONSTANT).Weight(reshape_shape).TensorDesc(FORMAT_ND, DT_INT64, {2}).InCnt(0).OutCnt(1).Build("shape3")};
    auto reshape_nodes =
        std::array{OP_CFG(RESHAPE).InCnt(2).OutCnt(1).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 192}).Build("reshape0"),
                   OP_CFG(RESHAPE).InCnt(2).OutCnt(1).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 192}).Build("reshape1"),
                   OP_CFG(RESHAPE).InCnt(2).OutCnt(1).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 192}).Build("reshape2"),
                   OP_CFG(RESHAPE).InCnt(2).OutCnt(1).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 192}).Build("reshape3")};
    for (size_t i = 0U; i < data_nodes.size(); ++i) {
      CHAIN(NODE(data_nodes[i])->NODE(data_pre_nodes[i])->EDGE(0, static_cast<int32_t>(i))->NODE(pack_data));
      CHAIN(NODE(const_nodes[i])->NODE(const_pre_nodes[i])->EDGE(0, static_cast<int32_t>(i))->NODE(pack_const));
    }
    CHAIN(NODE(pack_data)->EDGE(0, 0)->NODE(batch_matmul));
    CHAIN(NODE(pack_const)->EDGE(0, 1)->NODE(batch_matmul));
    CHAIN(NODE(batch_matmul)->NODE(split));
    for (size_t i = 0U; i < reshape_nodes.size(); ++i) {
      CHAIN(NODE(split)->EDGE(static_cast<int32_t>(i), 0)->NODE(reshape_nodes[i]));
      CHAIN(NODE(shape_nodes[i])->EDGE(0, 1)->NODE(reshape_nodes[i]));
      ADD_OUTPUT(reshape_nodes[i], 0);
    }
  };
  return ToGeGraph(g1);
}

void ExpectPackNodeAttrs(const ComputeGraphPtr &compute_graph, const std::array<const char *, 2> &node_names) {
  for (const auto *const node_name : node_names) {
    const auto node = compute_graph->FindNode(node_name);
    ASSERT_NE(node, nullptr);
    bool notask = false;
    bool output_reuse_input = false;
    bool nopadding_continuous_input = false;
    int64_t reuse_input_dim = -1;
    (void)AttrUtils::GetBool(node->GetOpDesc(), ATTR_NAME_NOTASK, notask);
    (void)AttrUtils::GetBool(node->GetOpDesc(), ATTR_NAME_OUTPUT_REUSE_INPUT, output_reuse_input);
    (void)AttrUtils::GetBool(node->GetOpDesc(), ATTR_NAME_NOPADDING_CONTINUOUS_INPUT, nopadding_continuous_input);
    (void)AttrUtils::GetInt(node->GetOpDesc(), ATTR_NAME_REUSE_INPUT_ON_DIM_INDEX, reuse_input_dim);
    EXPECT_TRUE(notask);
    EXPECT_TRUE(output_reuse_input);
    EXPECT_TRUE(nopadding_continuous_input);
    EXPECT_EQ(reuse_input_dim, 0);
  }
}

void ExpectPackReuseLocked(const ComputeGraphPtr &compute_graph, const std::array<const char *, 2> &node_names) {
  for (const auto *const node_name : node_names) {
    const auto node = compute_graph->FindNode(node_name);
    ASSERT_NE(node, nullptr);
    bool can_reuse = true;
    (void)AttrUtils::GetBool(node->GetOpDesc()->MutableOutputDesc(0), "can_reused_for_pack_optimize", can_reuse);
    EXPECT_FALSE(can_reuse);
  }
}
}  // namespace

class PackNotaskPassTest : public testing::Test {
 protected:
  void SetUp() override {
    MockGenerateTask();
  }

  void TearDown() override {
    TearDownForGenerateTask(kAiCoreLib);
  }
};

/*
 * Graph topology under test:
 *
 *   data0[100x256] -> ReLU(data_pre0) --\
 *   data1[100x256] -> ReLU(data_pre1) ---\
 *   data2[100x256] -> ReLU(data_pre2) ----> Pack(pack_data, axis=0) ----\
 *   data3[100x256] -> ReLU(data_pre3) ---/   output:[4x100x256]         \
 *                                                                                BatchMatMul
 *   const0[256x192] -> ReLU(const_pre0) --\                               /  output:[4x100x192]
 *   const1[256x192] -> ReLU(const_pre1) ---\                             /
 *   const2[256x192] -> ReLU(const_pre2) ----> Pack(pack_const, axis=0) -/
 *   const3[256x192] -> ReLU(const_pre3) ---/   output:[4x256x192]
 *
 *   BatchMatMul -> SplitD(axis=0, num_split=4)
 *                |- y0[1x100x192] -> Reshape0(shape=[100,192]) -> out0[100x192]
 *                |- y1[1x100x192] -> Reshape1(shape=[100,192]) -> out1[100x192]
 *                |- y2[1x100x192] -> Reshape2(shape=[100,192]) -> out2[100x192]
 *                `- y3[1x100x192] -> Reshape3(shape=[100,192]) -> out3[100x192]
 *
 * ReLU is inserted before Pack because PackNotaskPass does not optimize Pack fed directly by Data/Const,
 * and Identity would be removed by earlier cleanup passes.
 */
TEST_F(PackNotaskPassTest, pack_connect_batch_matmul_split_reshape_success) {
  MockPackNotaskPassEnv mock_env;
  auto const_weight = GenerateTensor(DT_FLOAT16, {256, 192});
  auto reshape_shape = MakeShapeTensor({100, 192});
  dlog_setlevel(0, 0, 0);
  auto graph = BuildPackConnectBatchMatmulSplitReshapeGraph(const_weight, reshape_shape);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);
  SetAiCoreEngine(compute_graph, {kDataPackNode, kConstPackNode, "data_pre0", "data_pre1", "data_pre2", "data_pre3",
                                  "const_pre0", "const_pre1", "const_pre2", "const_pre3", "batch_matmul", "split",
                                  "reshape0", "reshape1", "reshape2", "reshape3"});
  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(1, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(1, inputs), SUCCESS);
  EXPECT_EQ(GraphUtils::DumpGEGraphByPath(compute_graph, kDumpFile, ge::DumpLevel::NO_DUMP), GRAPH_SUCCESS);
  ExpectPackNodeAttrs(compute_graph, std::array<const char *, 2>{kDataPackNode, kConstPackNode});
  ExpectPackReuseLocked(compute_graph, std::array<const char *, 2>{"data_pre0", "const_pre0"});
}
/*
 * FRACTAL_NZ 格式 Pack, axis=0, 应标记 NoTask
 *
 *   data0[1x2048] -> ReLU(pre0) --\
 *   data1[1x2048] -> ReLU(pre1) ----> Pack(pack_nz, axis=0)
 *   data2[1x2048] -> ReLU(pre2) ---/    output:[3x1x2048]
 *   Pack -> Identity(post) -> out
 *
 *   format: ND -> FRACTAL_NZ (origin NCHW -> storage FRACTAL_NZ)
 */
TEST_F(PackNotaskPassTest, pack_fractal_nz_format_success) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kNzInputNum = 3;

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2048}).Build("data0");
    auto data1 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2048}).Build("data1");
    auto data2 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2048}).Build("data2");

    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2048}).InCnt(1).OutCnt(1).Build("pre0");
    auto pre1 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2048}).InCnt(1).OutCnt(1).Build("pre1");
    auto pre2 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2048}).InCnt(1).OutCnt(1).Build("pre2");

    auto pack_nz = OP_CFG("Pack")
                       .InCnt(kNzInputNum)
                       .OutCnt(1)
                       .TensorDesc(FORMAT_ND, DT_FLOAT16, {3, 1, 2048})
                       .Attr("axis", 0)
                       .Attr("N", kNzInputNum)
                       .Build("pack_nz");
    auto post = OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {3, 1, 2048}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack_nz));
    CHAIN(NODE(data1)->NODE(pre1)->EDGE(0, 1)->NODE(pack_nz));
    CHAIN(NODE(data2)->NODE(pre2)->EDGE(0, 2)->NODE(pack_nz));
    CHAIN(NODE(pack_nz)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);

  // 手动将 Pack 输入设为 FRACTAL_NZ 格式
  auto pack_node = compute_graph->FindNode("pack_nz");
  ASSERT_NE(pack_node, nullptr);
  for (size_t i = 0; i < static_cast<size_t>(kNzInputNum); ++i) {
    auto input_desc = pack_node->GetOpDesc()->MutableInputDesc(i);
    input_desc->SetFormat(FORMAT_FRACTAL_NZ);
    input_desc->SetOriginFormat(FORMAT_ND);
    input_desc->SetOriginShape(GeShape({1, 2048}));
    input_desc->SetShape(GeShape({1, 128, 16}));
  }

  SetAiCoreEngine(compute_graph, {"pack_nz", "pre0", "pre1", "pre2", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(2, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(2, inputs), SUCCESS);

  const auto op_desc = pack_node->GetOpDesc();
  bool notask = false;
  (void)AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, notask);
  EXPECT_TRUE(notask);
}

/*
 * NC1HWC0 私有格式（非FRACTAL_NZ）Pack, 不应标记 NoTask
 *
 *   data0 -> ReLU(pre0) --\
 *   data1 -> ReLU(pre1) ----> Pack(pack_5hd, axis=0)
 *   Pack -> Identity(post) -> out
 *
 *   format: NCHW -> NC1HWC0
 */
TEST_F(PackNotaskPassTest, pack_nc1hwc0_format_reject) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kInputNum = 2;

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 32, 16, 16}).Build("data0");
    auto data1 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 32, 16, 16}).Build("data1");

    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 32, 16, 16}).InCnt(1).OutCnt(1).Build("pre0");
    auto pre1 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 32, 16, 16}).InCnt(1).OutCnt(1).Build("pre1");

    auto pack_5hd = OP_CFG("Pack")
                        .InCnt(kInputNum)
                        .OutCnt(1)
                        .TensorDesc(FORMAT_ND, DT_FLOAT16, {2, 1, 32, 16, 16})
                        .Attr("axis", 0)
                        .Attr("N", kInputNum)
                        .Build("pack_5hd");
    auto post =
        OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {2, 1, 32, 16, 16}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack_5hd));
    CHAIN(NODE(data1)->NODE(pre1)->EDGE(0, 1)->NODE(pack_5hd));
    CHAIN(NODE(pack_5hd)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);

  // 手动将 Pack 输入设为 NC1HWC0 格式
  auto pack_node = compute_graph->FindNode("pack_5hd");
  ASSERT_NE(pack_node, nullptr);
  for (size_t i = 0; i < static_cast<size_t>(kInputNum); ++i) {
    auto input_desc = pack_node->GetOpDesc()->MutableInputDesc(i);
    input_desc->SetFormat(FORMAT_NC1HWC0);
    input_desc->SetOriginFormat(FORMAT_NCHW);
    input_desc->SetOriginShape(GeShape({1, 32, 16, 16}));
    input_desc->SetShape(GeShape({1, 2, 16, 16, 16}));
  }

  SetAiCoreEngine(compute_graph, {"pack_5hd", "pre0", "pre1", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(3, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(3, inputs), SUCCESS);

  const auto op_desc = pack_node->GetOpDesc();
  bool notask = false;
  (void)AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, notask);
  EXPECT_FALSE(notask);
}

/*
 * axis=1, dim0=1 场景, ND 格式, 应标记 NoTask
 *
 *   data0[1x256] -> ReLU(pre0) --\
 *   data1[1x256] -> ReLU(pre1) ----> Pack(pack_axis1, axis=1)
 *   Pack -> Identity(post) -> out
 */
TEST_F(PackNotaskPassTest, pack_axis1_dim0_is1_nd_success) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kInputNum = 2;

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).Build("data0");
    auto data1 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).Build("data1");

    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).InCnt(1).OutCnt(1).Build("pre0");
    auto pre1 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).InCnt(1).OutCnt(1).Build("pre1");

    auto pack_axis1 = OP_CFG("Pack")
                          .InCnt(kInputNum)
                          .OutCnt(1)
                          .TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2, 256})
                          .Attr("axis", 1)
                          .Attr("N", kInputNum)
                          .Build("pack_axis1");
    auto post = OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2, 256}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack_axis1));
    CHAIN(NODE(data1)->NODE(pre1)->EDGE(0, 1)->NODE(pack_axis1));
    CHAIN(NODE(pack_axis1)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);
  SetAiCoreEngine(compute_graph, {"pack_axis1", "pre0", "pre1", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(4, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(4, inputs), SUCCESS);

  const auto pack_node = compute_graph->FindNode("pack_axis1");
  ASSERT_NE(pack_node, nullptr);
  const auto op_desc = pack_node->GetOpDesc();
  bool notask = false;
  (void)AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, notask);
  EXPECT_TRUE(notask);
}

/*
 * axis=1, dim0!=1 场景, ND 格式, 不应标记 NoTask
 *
 *   data0[8x256] -> ReLU(pre0) --\
 *   data1[8x256] -> ReLU(pre1) ----> Pack(pack_axis1_fail, axis=1)
 *   Pack -> Identity(post) -> out
 */
TEST_F(PackNotaskPassTest, pack_axis1_dim0_not1_nd_reject) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kInputNum = 2;

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {8, 256}).Build("data0");
    auto data1 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {8, 256}).Build("data1");

    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {8, 256}).InCnt(1).OutCnt(1).Build("pre0");
    auto pre1 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {8, 256}).InCnt(1).OutCnt(1).Build("pre1");

    auto pack_axis1_fail = OP_CFG("Pack")
                               .InCnt(kInputNum)
                               .OutCnt(1)
                               .TensorDesc(FORMAT_ND, DT_FLOAT16, {8, 2, 256})
                               .Attr("axis", 1)
                               .Attr("N", kInputNum)
                               .Build("pack_axis1_fail");
    auto post = OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {8, 2, 256}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack_axis1_fail));
    CHAIN(NODE(data1)->NODE(pre1)->EDGE(0, 1)->NODE(pack_axis1_fail));
    CHAIN(NODE(pack_axis1_fail)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);
  SetAiCoreEngine(compute_graph, {"pack_axis1_fail", "pre0", "pre1", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(5, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(5, inputs), SUCCESS);

  const auto pack_node = compute_graph->FindNode("pack_axis1_fail");
  ASSERT_NE(pack_node, nullptr);
  const auto op_desc = pack_node->GetOpDesc();
  bool notask = false;
  (void)AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, notask);
  EXPECT_FALSE(notask);
}

/*
 * 负轴场景端到端：axis=-2, rank=2, 转换后 pack_dim=1, dim0=1, 应标记 NoTask
 *
 *   data0[1x256] -> ReLU(pre0) --\
 *   data1[1x256] -> ReLU(pre1) ----> Pack(pack_neg_axis, axis=-2)
 *   Pack -> Identity(post) -> out
 */
TEST_F(PackNotaskPassTest, pack_negative_axis_nd_success) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kInputNum = 2;

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).Build("data0");
    auto data1 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).Build("data1");

    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).InCnt(1).OutCnt(1).Build("pre0");
    auto pre1 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).InCnt(1).OutCnt(1).Build("pre1");

    auto pack_neg = OP_CFG("Pack")
                        .InCnt(kInputNum)
                        .OutCnt(1)
                        .TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2, 256})
                        .Attr("axis", static_cast<int64_t>(-2))
                        .Attr("N", kInputNum)
                        .Build("pack_neg_axis");
    auto post = OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 2, 256}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack_neg));
    CHAIN(NODE(data1)->NODE(pre1)->EDGE(0, 1)->NODE(pack_neg));
    CHAIN(NODE(pack_neg)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);
  SetAiCoreEngine(compute_graph, {"pack_neg_axis", "pre0", "pre1", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(6, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(6, inputs), SUCCESS);

  const auto pack_node = compute_graph->FindNode("pack_neg_axis");
  ASSERT_NE(pack_node, nullptr);
  bool notask = false;
  (void)AttrUtils::GetBool(pack_node->GetOpDesc(), ATTR_NAME_NOTASK, notask);
  EXPECT_TRUE(notask);
}

/*
 * 单输入 Pack, axis=0, 端到端验证: 不需要tensor对齐检查
 *
 *   data0[100x192] -> ReLU(pre0) -> Pack(pack_single, axis=0)
 *   Pack -> Identity(post) -> out
 */
TEST_F(PackNotaskPassTest, pack_single_input_success) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kInputNum = 1;

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 192}).Build("data0");
    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 192}).InCnt(1).OutCnt(1).Build("pre0");

    auto pack_single = OP_CFG("Pack")
                           .InCnt(kInputNum)
                           .OutCnt(1)
                           .TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 100, 192})
                           .Attr("axis", 0)
                           .Attr("N", kInputNum)
                           .Build("pack_single");
    auto post = OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 100, 192}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack_single));
    CHAIN(NODE(pack_single)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);
  SetAiCoreEngine(compute_graph, {"pack_single", "pre0", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(7, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(7, inputs), SUCCESS);

  const auto pack_node = compute_graph->FindNode("pack_single");
  ASSERT_NE(pack_node, nullptr);
  bool notask = false;
  (void)AttrUtils::GetBool(pack_node->GetOpDesc(), ATTR_NAME_NOTASK, notask);
  EXPECT_TRUE(notask);
}

/*
 * FRACTAL_Z 私有格式（非FRACTAL_NZ）Pack, 端到端验证不应标记 NoTask
 *
 *   data0 -> ReLU(pre0) --\
 *   data1 -> ReLU(pre1) ----> Pack(pack_fz, axis=0)
 *   Pack -> Identity(post) -> out
 */
TEST_F(PackNotaskPassTest, pack_fractal_z_format_reject) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kInputNum = 2;

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 32, 16, 16}).Build("data0");
    auto data1 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 32, 16, 16}).Build("data1");

    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 32, 16, 16}).InCnt(1).OutCnt(1).Build("pre0");
    auto pre1 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 32, 16, 16}).InCnt(1).OutCnt(1).Build("pre1");

    auto pack_fz = OP_CFG("Pack")
                       .InCnt(kInputNum)
                       .OutCnt(1)
                       .TensorDesc(FORMAT_ND, DT_FLOAT16, {2, 1, 32, 16, 16})
                       .Attr("axis", 0)
                       .Attr("N", kInputNum)
                       .Build("pack_fz");
    auto post =
        OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {2, 1, 32, 16, 16}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack_fz));
    CHAIN(NODE(data1)->NODE(pre1)->EDGE(0, 1)->NODE(pack_fz));
    CHAIN(NODE(pack_fz)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);

  auto pack_node = compute_graph->FindNode("pack_fz");
  ASSERT_NE(pack_node, nullptr);
  for (size_t i = 0; i < static_cast<size_t>(kInputNum); ++i) {
    auto input_desc = pack_node->GetOpDesc()->MutableInputDesc(i);
    input_desc->SetFormat(FORMAT_FRACTAL_Z);
    input_desc->SetOriginFormat(FORMAT_NCHW);
    input_desc->SetOriginShape(GeShape({1, 32, 16, 16}));
    input_desc->SetShape(GeShape({2, 16, 16, 16}));
  }

  SetAiCoreEngine(compute_graph, {"pack_fz", "pre0", "pre1", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(8, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(8, inputs), SUCCESS);

  const auto op_desc = pack_node->GetOpDesc();
  bool notask = false;
  (void)AttrUtils::GetBool(op_desc, ATTR_NAME_NOTASK, notask);
  EXPECT_FALSE(notask);
}

/*
 * Pack 后直接接 Pack（级联场景），第二个 Pack 不应标记 NoTask
 * 因为前驱 Pack 在第一轮被标记 notask，OutputCheck 会拦截第二个
 *
 *   data0 -> ReLU(pre0) --\
 *   data1 -> ReLU(pre1) ----> Pack1(pack1, axis=0)
 *   data2 -> ReLU(pre2) --\
 *   Pack1 --------->              Pack2(pack2, axis=0)
 *   Pack2 -> Identity(post) -> out
 */
TEST_F(PackNotaskPassTest, pack_cascaded_second_pack_reject) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kInputNum = 2;

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).Build("data0");
    auto data1 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).Build("data1");
    auto data2 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).Build("data2");

    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).InCnt(1).OutCnt(1).Build("pre0");
    auto pre1 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).InCnt(1).OutCnt(1).Build("pre1");
    auto pre2 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {100, 256}).InCnt(1).OutCnt(1).Build("pre2");

    auto pack1 = OP_CFG("Pack")
                     .InCnt(kInputNum)
                     .OutCnt(1)
                     .TensorDesc(FORMAT_ND, DT_FLOAT16, {2, 100, 256})
                     .Attr("axis", 0)
                     .Attr("N", kInputNum)
                     .Build("pack1");
    auto pack2 = OP_CFG("Pack")
                     .InCnt(kInputNum)
                     .OutCnt(1)
                     .TensorDesc(FORMAT_ND, DT_FLOAT16, {2, 100, 256})
                     .Attr("axis", 0)
                     .Attr("N", kInputNum)
                     .Build("pack2");
    auto post = OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {2, 100, 256}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack1));
    CHAIN(NODE(data1)->NODE(pre1)->EDGE(0, 1)->NODE(pack1));
    CHAIN(NODE(pack1)->EDGE(0, 0)->NODE(pack2));
    CHAIN(NODE(data2)->NODE(pre2)->EDGE(0, 1)->NODE(pack2));
    CHAIN(NODE(pack2)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);
  SetAiCoreEngine(compute_graph, {"pack1", "pack2", "pre0", "pre1", "pre2", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(9, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(9, inputs), SUCCESS);

  // pack1 应被标记 NoTask
  const auto pack1_node = compute_graph->FindNode("pack1");
  ASSERT_NE(pack1_node, nullptr);
  bool notask1 = false;
  (void)AttrUtils::GetBool(pack1_node->GetOpDesc(), ATTR_NAME_NOTASK, notask1);
  EXPECT_TRUE(notask1);

  // pack2 不应被标记 NoTask（前驱 pack1 已是 notask）
  const auto pack2_node = compute_graph->FindNode("pack2");
  ASSERT_NE(pack2_node, nullptr);
  bool notask2 = false;
  (void)AttrUtils::GetBool(pack2_node->GetOpDesc(), ATTR_NAME_NOTASK, notask2);
  EXPECT_FALSE(notask2);
}

/*
 * 校验 NoTask 零拷贝地址布局 (axis=0, ND):
 *   data0[1x256] -> ReLU(pre0) --\
 *   data1[1x256] -> ReLU(pre1) ----> Pack(pack_zc, axis=0)  output:[3,1,256]
 *   data2[1x256] -> ReLU(pre2) --/
 *   Pack -> Identity(post) -> out
 *
 * 期望 (零拷贝语义):
 *   1) pack.output_offset[0] == pack.input_offset[0]           输出复用首输入
 *   2) pack.input_offset[i]  == pre_i.output_offset[0]         上游直写 Pack 输入槽
 *   3) pack.input_offset[i]  == pack.input_offset[i-1] + size(input[i-1])  连续无 padding
 */
TEST_F(PackNotaskPassTest, pack_notask_zero_copy_address_layout) {
  MockPackNotaskPassEnv mock_env;
  constexpr int64_t kInputNum = 3;
  constexpr char kPackNode[] = "pack_zc";
  const std::array<const char *, kInputNum> kPreNames{"pre0", "pre1", "pre2"};

  DEF_GRAPH(g1) {
    auto data0 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).Build("data0");
    auto data1 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).Build("data1");
    auto data2 = OP_CFG(DATA).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).Build("data2");

    auto pre0 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).InCnt(1).OutCnt(1).Build("pre0");
    auto pre1 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).InCnt(1).OutCnt(1).Build("pre1");
    auto pre2 = OP_CFG(RELU).TensorDesc(FORMAT_ND, DT_FLOAT16, {1, 256}).InCnt(1).OutCnt(1).Build("pre2");

    auto pack_zc = OP_CFG("Pack")
                       .InCnt(kInputNum)
                       .OutCnt(1)
                       .TensorDesc(FORMAT_ND, DT_FLOAT16, {kInputNum, 1, 256})
                       .Attr("axis", 0)
                       .Attr("N", kInputNum)
                       .Build(kPackNode);
    auto post =
        OP_CFG("Identity").TensorDesc(FORMAT_ND, DT_FLOAT16, {kInputNum, 1, 256}).InCnt(1).OutCnt(1).Build("post");

    CHAIN(NODE(data0)->NODE(pre0)->EDGE(0, 0)->NODE(pack_zc));
    CHAIN(NODE(data1)->NODE(pre1)->EDGE(0, 1)->NODE(pack_zc));
    CHAIN(NODE(data2)->NODE(pre2)->EDGE(0, 2)->NODE(pack_zc));
    CHAIN(NODE(pack_zc)->NODE(post));
    ADD_OUTPUT(post, 0);
  };

  auto graph = ToGeGraph(g1);
  auto compute_graph = GraphUtilsEx::GetComputeGraph(graph);
  ASSERT_NE(compute_graph, nullptr);
  SetAiCoreEngine(compute_graph, {kPackNode, "pre0", "pre1", "pre2", "post"});

  std::map<AscendString, AscendString> options = {{ge::OO_LEVEL, "O3"}, {ge::OO_CONSTANT_FOLDING, "false"}};
  Session session(options);
  ASSERT_EQ(session.AddGraph(10, graph, options), SUCCESS);

  std::vector<InputTensorInfo> inputs;
  ASSERT_EQ(session.BuildGraph(10, inputs), SUCCESS);

  const auto pack_node = compute_graph->FindNode(kPackNode);
  ASSERT_NE(pack_node, nullptr);

  // 前置条件：确认 NoTask 属性已打标（否则 offset 校验无意义）
  bool notask = false;
  (void)AttrUtils::GetBool(pack_node->GetOpDesc(), ATTR_NAME_NOTASK, notask);
  ASSERT_TRUE(notask);

  const auto pack_in_offsets = pack_node->GetOpDesc()->GetInputOffset();
  const auto pack_out_offsets = pack_node->GetOpDesc()->GetOutputOffset();
  ASSERT_EQ(pack_in_offsets.size(), static_cast<size_t>(kInputNum));
  ASSERT_FALSE(pack_out_offsets.empty());

  // (1) 输出复用首输入
  EXPECT_EQ(pack_out_offsets[0], pack_in_offsets[0]);

  // (2) 上游直写 Pack 输入槽位 + (3) 段间连续无 padding
  int64_t prev_size = 0;
  for (size_t i = 0; i < static_cast<size_t>(kInputNum); ++i) {
    const auto pre_node = compute_graph->FindNode(kPreNames[i]);
    ASSERT_NE(pre_node, nullptr);
    const auto pre_out_offsets = pre_node->GetOpDesc()->GetOutputOffset();
    ASSERT_FALSE(pre_out_offsets.empty());

    EXPECT_EQ(pre_out_offsets[0], pack_in_offsets[i])
        << "pre" << i << " output should directly write to pack input slot " << i;

    if (i > 0U) {
      EXPECT_EQ(pack_in_offsets[i], pack_in_offsets[i - 1U] + prev_size)
          << "pack input " << i << " must be contiguous with input " << (i - 1U);
    }

    int64_t cur_size = 0;
    (void)TensorUtils::GetTensorSizeInBytes(pre_node->GetOpDesc()->GetOutputDesc(0), cur_size);
    prev_size = cur_size;
  }
}
}  // namespace ge
