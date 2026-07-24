/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software; you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

#include "ge_tensor.h"
#include "operator_reg.h"

#include "all_ops_cpp.h"
#include "common/autofuse_backend_spec_api.h"
#include "common/autofuse_platform_api.h"
#include "depends/runtime/src/runtime_stub.h"
#include "esb_funcs_cpp.h"
#include "esb_graph.h"
#include "graph/attribute_group/attr_group_shape_env.h"
#include "graph/symbolizer/symbolic_utils.h"
#include "graph_utils_ex.h"
#include "op_creator_register.h"
#include "pattern_fusion/gather_forward_fusion_pass.h"
#include "utils/auto_fuse_config.h"
#include "graph_metadef/graph/debug/ge_util.h"

namespace ge {
REG_OP(Relu).INPUT(x, TensorType::UnaryDataType()).OUTPUT(y, TensorType::UnaryDataType()).OP_END_FACTORY_REG(Relu);

REG_OP(Abs).INPUT(x, TensorType::UnaryDataType()).OUTPUT(y, TensorType::UnaryDataType()).OP_END_FACTORY_REG(Abs);

const auto GatherForwardPointwiseInfer = [](Operator &op) {
  (void)op;
  return GRAPH_SUCCESS;
};

INFER_FUNC_REG(Relu, GatherForwardPointwiseInfer);
INFER_FUNC_REG(Abs, GatherForwardPointwiseInfer);

class GatherForwardFusionPassTest : public testing::Test {
 protected:
  struct BuildOptions {
    bool with_second_pointwise = false;
    bool with_two_gather_consumers = false;
    bool with_pointwise_side_consumer = false;
    bool set_pointwise_input_symbol_shape = true;
    bool set_gather_symbol_shape = true;
  };

  struct GatherGraphNodes {
    ComputeGraphPtr graph;
    NodePtr data;
    NodePtr indices;
    NodePtr axis;
    NodePtr first_pointwise;
    NodePtr second_pointwise;
    NodePtr gather;
    NodePtr first_consumer;
    NodePtr second_consumer;
    NodePtr side_consumer;
  };

  void SetUp() override {
    dlog_setlevel(0, 3, 0);
    RegisterAllOpCreator();
    original_lowering_gather_ = autofuse::AutoFuseConfig::LoweringConfig().experimental_lowering_gather;
    autofuse::AutoFuseConfig::MutableLoweringConfig().experimental_lowering_gather = true;
    ResetAutofusePlatform();
    runtime_stub_ = std::make_shared<RuntimeStubV2Common>();
    RuntimeStub::SetInstance(runtime_stub_);

    const auto backend_spec = GetAutofuseBackendSpec();
    ASSERT_NE(backend_spec, nullptr);
    ASSERT_TRUE(backend_spec->gather_spec.enable_gather_elementwise_forward_fusion);
  }

  void TearDown() override {
    autofuse::AutoFuseConfig::MutableLoweringConfig().experimental_lowering_gather = original_lowering_gather_;
    SetCurShapeEnvContext(nullptr);
    ResetAutofusePlatform();
    RuntimeStub::Reset();
    runtime_stub_.reset();
    dlog_setlevel(0, 3, 0);
  }

  template <typename T>
  static es::Tensor CreateConst(es::Graph &graph, ge::DataType dtype, const std::vector<int64_t> &dims,
                                std::vector<T> value) {
    auto result = es::FileConstant(graph, dims, dtype);
    GeTensorDesc desc(GeShape(dims), ge::FORMAT_ND, dtype);
    GeTensorPtr tensor =
        std::make_shared<GeTensor>(desc, reinterpret_cast<uint8_t *>(value.data()), sizeof(T) * value.size());
    AttrUtils::SetTensor(result.GetEsbTensor()->GetProducer()->GetOpDesc(), "value", tensor);
    result.GetEsbTensor()->GetProducer()->GetOpDesc()->SetType(ge::CONSTANT);
    return result;
  }

  es::Tensor BuildFirstPointwise(const BuildOptions &options, const es::Tensor &data) {
    auto abs = es::Abs(data);
    abs.SetShape({4, 3, 4});
    abs.SetSymbolShape({"s0", "s1", "s2"});
    if (options.set_pointwise_input_symbol_shape) {
      abs.SetInputSymbolShape({"s0", "s1", "s2"});
    }
    return abs;
  }

  es::Tensor BuildOptionalSecondPointwise(const BuildOptions &options, const es::Tensor &first_pointwise,
                                          NodePtr &second_pointwise) {
    if (!options.with_second_pointwise) {
      return first_pointwise;
    }
    auto relu = es::Relu(first_pointwise);
    relu.SetShape({4, 3, 4});
    relu.SetSymbolShape({"s0", "s1", "s2"});
    if (options.set_pointwise_input_symbol_shape) {
      relu.SetInputSymbolShape({"s0", "s1", "s2"});
    }
    second_pointwise = relu.GetEsbTensor()->GetProducer();
    return relu;
  }

  es::Tensor BuildOptionalSideConsumer(const BuildOptions &options, const es::Tensor &first_pointwise,
                                       NodePtr &side_consumer_node) {
    if (!options.with_pointwise_side_consumer) {
      return first_pointwise;
    }
    auto side_consumer = es::Relu(first_pointwise);
    side_consumer.SetShape({4, 3, 4});
    side_consumer.SetSymbolShape({"s0", "s1", "s2"});
    side_consumer_node = side_consumer.GetEsbTensor()->GetProducer();
    return side_consumer;
  }

  std::vector<es::Tensor> BuildGatherOutputs(const BuildOptions &options, const es::Tensor &gather,
                                             NodePtr &first_consumer, NodePtr &second_consumer) {
    if (!options.with_two_gather_consumers) {
      return {gather};
    }
    auto relu = es::Relu(gather);
    relu.SetShape({2, 3, 4});
    relu.SetSymbolShape({"s3", "s1", "s2"});
    auto output_abs = es::Abs(gather);
    output_abs.SetShape({2, 3, 4});
    output_abs.SetSymbolShape({"s3", "s1", "s2"});
    first_consumer = relu.GetEsbTensor()->GetProducer();
    second_consumer = output_abs.GetEsbTensor()->GetProducer();
    return {relu, output_abs};
  }

  void NormalizePointwiseDescs(const std::vector<NodePtr> &pointwise_nodes) {
    for (const auto &node : pointwise_nodes) {
      if (node == nullptr) {
        continue;
      }
      auto input_desc = node->GetOpDesc()->MutableInputDesc(0);
      auto output_desc = node->GetOpDesc()->MutableOutputDesc(0);
      input_desc->SetShape(output_desc->GetShape());
      input_desc->SetOriginShape(output_desc->GetOriginShape());
      input_desc->SetDataType(DT_FLOAT);
      input_desc->SetOriginDataType(DT_FLOAT);
      output_desc->SetDataType(DT_FLOAT);
      output_desc->SetOriginDataType(DT_FLOAT);
    }
  }

  GatherGraphNodes BuildGatherGraph(const BuildOptions &options) {
    es::Graph graph_builder("gather_forward_graph");
    auto data = graph_builder.CreateInput(0, "data", nullptr);
    data.SetShape({4, 3, 4});
    data.SetSymbolShape({"s0", "s1", "s2"});
    auto indices = graph_builder.CreateInput(1, "indices", nullptr);
    indices.SetShape({2});
    indices.SetSymbolShape({"s3"});
    auto axis = CreateConst(graph_builder, DT_INT64, {1}, std::vector<int64_t>{0});
    axis.SetSymbolShape({});

    auto abs = BuildFirstPointwise(options, data);
    NodePtr second_pointwise;
    auto gather_input = BuildOptionalSecondPointwise(options, abs, second_pointwise);
    NodePtr side_consumer_node;
    auto side_consumer = BuildOptionalSideConsumer(options, abs, side_consumer_node);
    auto gather = es::GatherV2(gather_input, indices, axis);
    gather.SetShape({2, 3, 4});
    if (options.set_gather_symbol_shape) {
      gather.SetSymbolShape({"s3", "s1", "s2"});
    }

    NodePtr first_consumer;
    NodePtr second_consumer;
    auto outputs = BuildGatherOutputs(options, gather, first_consumer, second_consumer);
    if (options.with_pointwise_side_consumer) {
      outputs.emplace_back(side_consumer);
    }

    auto graph = graph_builder.Build(outputs);
    auto compute_graph = GraphUtilsEx::GetComputeGraph(*graph);
    auto first_pointwise = abs.GetEsbTensor()->GetProducer();
    NormalizePointwiseDescs({first_pointwise, second_pointwise});
    auto indices_desc = indices.GetEsbTensor()->GetProducer()->GetOpDesc()->MutableOutputDesc(0);
    indices_desc->SetDataType(DT_INT64);
    indices_desc->SetOriginDataType(DT_INT64);
    return {compute_graph,
            data.GetEsbTensor()->GetProducer(),
            indices.GetEsbTensor()->GetProducer(),
            axis.GetEsbTensor()->GetProducer(),
            first_pointwise,
            second_pointwise,
            gather.GetEsbTensor()->GetProducer(),
            first_consumer,
            second_consumer,
            side_consumer_node};
  }

 private:
  bool original_lowering_gather_ = false;
  std::shared_ptr<RuntimeStub> runtime_stub_;
};

TEST_F(GatherForwardFusionPassTest, RejectNullGraph) {
  EXPECT_EQ(GatherForwardFusionPass().Run(nullptr), PARAM_INVALID);
}

TEST_F(GatherForwardFusionPassTest, SkipWhenExperimentalSwitchIsDisabled) {
  const auto nodes = BuildGatherGraph(BuildOptions{});
  autofuse::AutoFuseConfig::MutableLoweringConfig().experimental_lowering_gather = false;

  ASSERT_EQ(GatherForwardFusionPass().Run(nodes.graph), GRAPH_SUCCESS);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
}

TEST_F(GatherForwardFusionPassTest, SkipGatherWithoutSymbolicOutput) {
  BuildOptions options;
  options.set_gather_symbol_shape = false;
  const auto nodes = BuildGatherGraph(options);
  ASSERT_EQ(nodes.gather->GetOpDesc()->MutableOutputDesc(0)->GetAttrsGroup<SymbolicDescAttr>(), nullptr);

  ASSERT_EQ(GatherForwardFusionPass().Run(nodes.graph), GRAPH_SUCCESS);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
}

TEST_F(GatherForwardFusionPassTest, SkipPointwiseWithSideConsumer) {
  BuildOptions options;
  options.with_pointwise_side_consumer = true;
  const auto nodes = BuildGatherGraph(options);
  ASSERT_EQ(nodes.first_pointwise->GetOutNodesSize(), 2U);

  ASSERT_EQ(GatherForwardFusionPass().Run(nodes.graph), GRAPH_SUCCESS);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
  EXPECT_EQ(nodes.side_consumer->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
}

TEST_F(GatherForwardFusionPassTest, SkipPointwiseWithoutInputSymbolicShape) {
  BuildOptions options;
  options.set_pointwise_input_symbol_shape = false;
  const auto nodes = BuildGatherGraph(options);
  ASSERT_EQ(nodes.first_pointwise->GetOpDesc()->MutableInputDesc(0)->GetAttrsGroup<SymbolicDescAttr>(), nullptr);

  ASSERT_EQ(GatherForwardFusionPass().Run(nodes.graph), GRAPH_SUCCESS);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
}

TEST_F(GatherForwardFusionPassTest, SkipPointwiseWithDifferentSymbolicShapes) {
  const auto nodes = BuildGatherGraph(BuildOptions{});
  auto input_attr = nodes.first_pointwise->GetOpDesc()->MutableInputDesc(0)->GetAttrsGroup<SymbolicDescAttr>();
  ASSERT_NE(input_attr, nullptr);
  input_attr->symbolic_tensor.MutableOriginSymbolShape() = {Symbol("different_s0"), Symbol("s1"), Symbol("s2")};

  ASSERT_EQ(GatherForwardFusionPass().Run(nodes.graph), GRAPH_SUCCESS);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
}

TEST_F(GatherForwardFusionPassTest, SkipPointwiseWithUnsupportedInputDtype) {
  const auto nodes = BuildGatherGraph(BuildOptions{});
  auto input_desc = nodes.first_pointwise->GetOpDesc()->MutableInputDesc(0);
  auto output_desc = nodes.first_pointwise->GetOpDesc()->MutableOutputDesc(0);
  input_desc->SetDataType(DT_INT64);
  input_desc->SetOriginDataType(DT_INT64);
  output_desc->SetDataType(DT_INT64);
  output_desc->SetOriginDataType(DT_INT64);

  ASSERT_EQ(GatherForwardFusionPass().Run(nodes.graph), GRAPH_SUCCESS);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
}

TEST_F(GatherForwardFusionPassTest, SkipPointwiseWithDifferentInputAndOutputDtypes) {
  const auto nodes = BuildGatherGraph(BuildOptions{});
  auto output_desc = nodes.first_pointwise->GetOpDesc()->MutableOutputDesc(0);
  output_desc->SetDataType(DT_FLOAT16);
  output_desc->SetOriginDataType(DT_FLOAT16);

  ASSERT_EQ(GatherForwardFusionPass().Run(nodes.graph), GRAPH_SUCCESS);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
}

TEST_F(GatherForwardFusionPassTest, MoveGatherBeforePointwiseChainAndPreserveConsumers) {
  BuildOptions options;
  options.with_second_pointwise = true;
  options.with_two_gather_consumers = true;
  const auto nodes = BuildGatherGraph(options);

  ASSERT_EQ(GatherForwardFusionPass().Run(nodes.graph), GRAPH_SUCCESS);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.data);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(1)->GetPeerOutAnchor()->GetOwnerNode(), nodes.indices);
  EXPECT_EQ(nodes.gather->GetInDataAnchor(2)->GetPeerOutAnchor()->GetOwnerNode(), nodes.axis);
  EXPECT_EQ(nodes.first_pointwise->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.gather);
  EXPECT_EQ(nodes.second_pointwise->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.first_pointwise);
  EXPECT_EQ(nodes.first_consumer->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.second_pointwise);
  EXPECT_EQ(nodes.second_consumer->GetInDataAnchor(0)->GetPeerOutAnchor()->GetOwnerNode(), nodes.second_pointwise);

  const auto gather_symbol_shape = nodes.gather->GetOpDesc()
                                       ->MutableOutputDesc(0)
                                       ->GetAttrsGroup<SymbolicDescAttr>()
                                       ->symbolic_tensor.GetOriginSymbolShape();
  for (const auto &node : std::vector<NodePtr>{nodes.first_pointwise, nodes.second_pointwise}) {
    const auto input_desc = node->GetOpDesc()->MutableInputDesc(0);
    const auto output_desc = node->GetOpDesc()->MutableOutputDesc(0);
    ASSERT_NE(input_desc->GetAttrsGroup<SymbolicDescAttr>(), nullptr);
    ASSERT_NE(output_desc->GetAttrsGroup<SymbolicDescAttr>(), nullptr);
    EXPECT_EQ(input_desc->GetAttrsGroup<SymbolicDescAttr>()->symbolic_tensor.GetOriginSymbolShape(),
              gather_symbol_shape);
    EXPECT_EQ(output_desc->GetAttrsGroup<SymbolicDescAttr>()->symbolic_tensor.GetOriginSymbolShape(),
              gather_symbol_shape);
  }
}
}  // namespace ge
