/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <iostream>
#include <vector>
#include <limits>
#include "es_all_ops.h"
#include "ge/fusion/pass/pattern_fusion_pass.h"
#include "ge/ge_utils.h"

using namespace ge;
using namespace fusion;
using namespace ge::es;

namespace {
constexpr int64_t kInputADimNum = 3;      // input A dimension count: 3D tensor [b, m, k]
constexpr int64_t kInputBDimNum = 2;      // input B dimension count: 2D tensor [k, n]
constexpr size_t kSupportedInputNum = 2;  // only x1 and x2 are supported in this sample
constexpr int64_t kDefaultOffsetX = 0;

bool IsDynamicDim(int64_t dim) {
  return dim < 0;
}

bool IsSupportedDataType(DataType data_type) {
  return data_type == DT_FLOAT || data_type == DT_FLOAT16 || data_type == DT_BF16;
}

bool GetBoolAttrOrDefault(const GNode &node, const char *attr_name, bool default_value) {
  bool attr_value = default_value;
  if (node.GetAttr(attr_name, attr_value) != SUCCESS) {
    return default_value;
  }
  return attr_value;
}

bool HasTransposeAttr(const GNode &node, const char *transpose_attr_name, const char *adj_attr_name) {
  return GetBoolAttrOrDefault(node, transpose_attr_name, false) || GetBoolAttrOrDefault(node, adj_attr_name, false);
}

int64_t GetIntAttrOrDefault(const GNode &node, const char *attr_name, int64_t default_value) {
  int64_t attr_value = default_value;
  if (node.GetAttr(attr_name, attr_value) != SUCCESS) {
    return default_value;
  }
  return attr_value;
}
}  // namespace

// |o>-------------------------------------------
// |o>  Before:                    After:
// |o>
// |o>  A[b,m,k]  B[k,n]           A[b,m,k]       B[k,n]
// |o>      \      /                 |              |
// |o>   BatchMatMulV2[b,m,n]      Reshape[b*m,k]  /
// |o>        |                        |          /
// |o>      y[b,m,n]                 MatMulV2[b*m,n]
// |o>                                      |
// |o>                               Reshape[b,m,n]
// |o>                                      |
// |o>                                   y[b,m,n]
// |o>-------------------------------------------
// 融合说明：本例识别BatchMatMulV2结构，将A[b,m,k]@B[k,n]展平为[b*m,k]@[k,n]再reshape回[b,m,n]
// 等价条件：仅在无bias/offset、offset_x=0、transpose_a=false且transpose_b=false时融合
// 数学上A[b,m,k]@B[k,n]与Reshape(A,[b*m,k])@B[k,n]+Reshape([b,m,n])等价
// 控制边等价：PatternFusionPass框架自动迁移原节点控制边到替换子图，保证控制边等价
class BatchMatmulFlattenPass : public PatternFusionPass {
 protected:
  std::vector<PatternUniqPtr> Patterns() override {
    std::cout << "Define pattern for BatchMatmulFlattenPass" << std::endl;
    std::vector<PatternUniqPtr> patterns;

    auto builder_v2 = es::EsGraphBuilder("pattern_batchmatmul_v2");
    auto [input_a_v2, input_b_v2] = builder_v2.CreateInputs<2>();
    auto batch_matmul_v2 = es::BatchMatMulV2(input_a_v2, input_b_v2);
    auto graph_v2 = builder_v2.BuildAndReset({batch_matmul_v2});
    auto pattern_v2 = std::make_unique<Pattern>(std::move(*graph_v2));
    pattern_v2->CaptureTensor({*batch_matmul_v2.GetProducer(), 0});
    patterns.emplace_back(std::move(pattern_v2));

    auto builder = es::EsGraphBuilder("pattern_batchmatmul");
    auto [input_a, input_b] = builder.CreateInputs<2>();
    auto batch_matmul = es::BatchMatMul(input_a, input_b);
    auto graph = builder.BuildAndReset({batch_matmul});
    auto pattern = std::make_unique<Pattern>(std::move(*graph));
    pattern->CaptureTensor({*batch_matmul.GetProducer(), 0});
    patterns.emplace_back(std::move(pattern));

    return patterns;
  }

  bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
    std::cout << "Define MeetRequirements for BatchMatmulFlattenPass" << std::endl;

    NodeIo bmm_node_io;
    if (match_result->GetCapturedTensor(0, bmm_node_io) != SUCCESS) {
      return false;
    }
    GNode bmm_node = bmm_node_io.node;

    if (bmm_node.GetInputsSize() != kSupportedInputNum) {
      std::cout << "Only x1 and x2 inputs are supported, skip" << std::endl;
      return false;
    }

    TensorDesc input0_desc;
    TensorDesc input1_desc;
    if (bmm_node.GetInputDesc(0, input0_desc) != SUCCESS || bmm_node.GetInputDesc(1, input1_desc) != SUCCESS) {
      return false;
    }

    ge::Shape shape0 = input0_desc.GetShape();
    ge::Shape shape1 = input1_desc.GetShape();

    if (input0_desc.GetDataType() != input1_desc.GetDataType() || !IsSupportedDataType(input0_desc.GetDataType())) {
      std::cout << "Only same float dtype inputs are supported, skip" << std::endl;
      return false;
    }

    if (shape0.GetDimNum() != kInputADimNum) {
      std::cout << "A is not 3D, skip" << std::endl;
      return false;
    }

    if (shape1.GetDimNum() != kInputBDimNum) {
      std::cout << "B is not 2D, skip" << std::endl;
      return false;
    }

    bool transpose_a = HasTransposeAttr(bmm_node, "transpose_x1", "adj_x1");
    bool transpose_b = HasTransposeAttr(bmm_node, "transpose_x2", "adj_x2");
    if (transpose_a || transpose_b) {
      std::cout << "Has transpose attribute, skip" << std::endl;
      return false;
    }

    int64_t offset_x = GetIntAttrOrDefault(bmm_node, "offset_x", kDefaultOffsetX);
    if (offset_x != kDefaultOffsetX) {
      std::cout << "offset_x is not 0, skip" << std::endl;
      return false;
    }

    return true;
  }

  GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
    std::cout << "Define replacement for BatchMatmulFlattenPass" << std::endl;

    NodeIo bmm_node_io;
    if (match_result->GetCapturedTensor(0, bmm_node_io) != SUCCESS) {
      return nullptr;
    }
    GNode bmm_node = bmm_node_io.node;

    TensorDesc input0_desc;
    TensorDesc input1_desc;
    if (bmm_node.GetInputDesc(0, input0_desc) != SUCCESS || bmm_node.GetInputDesc(1, input1_desc) != SUCCESS) {
      return nullptr;
    }

    ge::Shape shape0 = input0_desc.GetShape();
    ge::Shape shape1 = input1_desc.GetShape();

    int64_t batch_size = shape0.GetDim(0);  // dim 0: batch dimension of A
    int64_t m_size = shape0.GetDim(1);      // dim 1: m dimension of A
    int64_t k_size = shape0.GetDim(2);      // dim 2: k dimension of A
    int64_t n_size = shape1.GetDim(1);      // dim 1: n dimension of B

    if (m_size > 0 && batch_size > std::numeric_limits<int64_t>::max() / m_size) {
      std::cout << "batch_size * m_size overflow" << std::endl;
      return nullptr;
    }

    auto replace_graph_builder = es::EsGraphBuilder("replacement_flatten");
    auto [input_a, input_b] = replace_graph_builder.CreateInputs<2>();

    return BuildReplacement(replace_graph_builder, input_a, input_b, batch_size, m_size, k_size, n_size, bmm_node);
  }

  es::EsTensorHolder GetDimValue(es::EsGraphBuilder &builder, int dim_index, int64_t static_value,
                                 es::EsTensorHolder &shape_tensor) {
    if (IsDynamicDim(static_value)) {
      auto offset =
          builder.CreateConst(std::vector<int64_t>{dim_index}, std::vector<int64_t>{1});  // {1}: 1-element shape
      auto size =
          builder.CreateConst(std::vector<int64_t>{1}, std::vector<int64_t>{1});  // {1}: 1-element shape for slice size
      return es::Slice(shape_tensor, offset, size);
    } else {
      return builder.CreateConst(std::vector<int64_t>{static_value}, std::vector<int64_t>{1});  // {1}: 1-element shape
    }
  }

  GraphUniqPtr BuildReplacement(es::EsGraphBuilder &builder, es::EsTensorHolder &input_a, es::EsTensorHolder &input_b,
                                int64_t batch_size, int64_t m_size, int64_t k_size, int64_t n_size,
                                const GNode &matched_node) {
    bool b_static = !IsDynamicDim(batch_size);
    bool m_static = !IsDynamicDim(m_size);
    bool k_static = !IsDynamicDim(k_size);
    bool n_static = !IsDynamicDim(n_size);

    bool reshape1_static = b_static && m_static && k_static;
    bool reshape2_static = b_static && m_static && n_static;

    auto concat_axis = builder.CreateScalar(0);  // axis 0: concatenate shape dimensions along first axis
    es::EsTensorHolder reshape1_shape_tensor;
    es::EsTensorHolder reshape2_shape_tensor;
    es::EsTensorHolder b_dim;
    es::EsTensorHolder m_dim;
    es::EsTensorHolder k_dim;
    es::EsTensorHolder n_dim;

    if (reshape1_static) {
      reshape1_shape_tensor =
          builder.CreateConst(std::vector<int64_t>{batch_size * m_size, k_size}, std::vector<int64_t>{2}
                              // {2}: reshape1 target shape has 2 dims: [b*m, k]
          );
      b_dim = builder.CreateConst(std::vector<int64_t>{batch_size},
                                  std::vector<int64_t>{1});  // {1}: 1-element shape for batch dim
      m_dim =
          builder.CreateConst(std::vector<int64_t>{m_size}, std::vector<int64_t>{1});  // {1}: 1-element shape for m dim
    } else {
      auto shape_a = es::Shape(input_a, ge::DT_INT64);
      b_dim = GetDimValue(builder, 0, batch_size, shape_a);  // dim 0: batch dimension of A
      m_dim = GetDimValue(builder, 1, m_size, shape_a);      // dim 1: m dimension of A
      k_dim = GetDimValue(builder, 2, k_size, shape_a);      // dim 2: k dimension of A

      auto batch_m = es::Mul(b_dim, m_dim);
      reshape1_shape_tensor =
          es::Concat(concat_axis, {batch_m, k_dim}, 2);  // 2: concat 2 dims for 2D reshape target [b*m, k]
    }

    if (reshape2_static) {
      reshape2_shape_tensor =
          builder.CreateConst(std::vector<int64_t>{batch_size, m_size, n_size}, std::vector<int64_t>{3}
                              // {3}: reshape2 target shape has 3 dims: [b, m, n]
          );
    } else {
      auto shape_b = es::Shape(input_b, ge::DT_INT64);
      n_dim = GetDimValue(builder, 1, n_size, shape_b);  // dim 1: n dimension of B
      reshape2_shape_tensor =
          es::Concat(concat_axis, {b_dim, m_dim, n_dim}, 3);  // 3: concat 3 dims for 3D reshape target [b, m, n]
    }

    auto reshape1 = es::Reshape(input_a, reshape1_shape_tensor);
    auto matmul = es::MatMulV2(reshape1, input_b);
    auto reshape2 = es::Reshape(matmul, reshape2_shape_tensor);

    auto replace_graph = builder.BuildAndReset({reshape2});
    if (!InferShapeAndCheckSupport(matched_node, *replace_graph)) {
      std::cout << "InferShapeAndCheckSupport failed" << std::endl;
      return nullptr;
    }

    return replace_graph;
  }

  bool InferShapeAndCheckSupport(const GNode &matched_node, const Graph &graph) {
    std::vector<ge::Shape> input_shapes;
    auto input_size = matched_node.GetInputsSize();
    for (size_t i = 0; i < input_size; i++) {
      TensorDesc tensor_desc;
      if (matched_node.GetInputDesc(i, tensor_desc) != SUCCESS) {
        return false;
      }
      input_shapes.emplace_back(tensor_desc.GetShape());
    }

    if (GeUtils::InferShape(graph, input_shapes) != SUCCESS) {
      std::cout << "InferShape failed" << std::endl;
      return false;
    }

    auto nodes = graph.GetAllNodes();
    for (GNode node : nodes) {
      AscendString node_type;
      node.GetType(node_type);
      std::string type_str(node_type.GetString());
      if (type_str == "Const" || type_str == "Data") {
        continue;
      }
      std::cout << "Created node: " << type_str << std::endl;
    }

    std::cout << "InferShapeAndCheckSupport success" << std::endl;
    return true;
  }
};

REG_FUSION_PASS(BatchMatmulFlattenPass).Stage(CustomPassStage::kAfterInferShape);
