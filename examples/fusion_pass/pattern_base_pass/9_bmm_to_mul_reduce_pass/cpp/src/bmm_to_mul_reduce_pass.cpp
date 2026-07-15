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
#include <string>
#include "es_all_ops.h"
#include "ge/fusion/pass/pattern_fusion_pass.h"
#include "ge/fusion/infer_shape_util.h"

using namespace ge;
using namespace fusion;
using namespace ge::es;

namespace {
constexpr int64_t kTargetN = 1;
constexpr int64_t kMaxK = 16;
constexpr size_t kSupportedInputNum = 2;
constexpr int32_t kTransposeDataInputIndex = 0;
constexpr int32_t kTransposePermInputIndex = 1;
constexpr int64_t kMinDimNumForMatmul = 2;
constexpr int64_t kSecondLastDimOffset = 2;
constexpr int64_t kLastDimOffset = 1;
constexpr int64_t kMaxOutputConsumers = 1;
constexpr int32_t kBmmInputIndex0 = 0;
constexpr int32_t kBmmInputIndex1 = 1;
constexpr int32_t kBmmOutputIndex0 = 0;
constexpr int32_t kCaptureIdx0 = 0;
constexpr int32_t kCaptureIdx1 = 1;
constexpr int64_t kMinK = 0;

bool IsSupportedDataType(DataType data_type) {
  return data_type == DT_FLOAT || data_type == DT_FLOAT16;
}

bool GetBoolAttrOrDefault(const GNode &node, const char *attr_name, bool default_value) {
  bool attr_value = default_value;
  if (node.GetAttr(AscendString(attr_name), attr_value) != GRAPH_SUCCESS) {
    return default_value;
  }
  return attr_value;
}

bool HasAdjAttr(const GNode &node, const char *adj_attr_name, const char *transpose_attr_name) {
  return GetBoolAttrOrDefault(node, adj_attr_name, false) || GetBoolAttrOrDefault(node, transpose_attr_name, false);
}

bool ReadTransposePerm(const GNode &transpose_node, std::vector<int64_t> &perm) {
  Tensor perm_tensor;
  if (transpose_node.GetInputConstData(kTransposePermInputIndex, perm_tensor) != GRAPH_SUCCESS) {
    return false;
  }
  DataType dtype = perm_tensor.GetDataType();
  const uint8_t *data = perm_tensor.GetData();
  size_t data_size = perm_tensor.GetSize();
  if (data == nullptr || data_size == 0) {
    return false;
  }
  if (dtype == DT_INT32) {
    size_t count = data_size / sizeof(int32_t);
    const auto *perm_data = reinterpret_cast<const int32_t *>(data);
    for (size_t i = 0; i < count; ++i) {
      perm.push_back(static_cast<int64_t>(perm_data[i]));
    }
  } else if (dtype == DT_INT64) {
    size_t count = data_size / sizeof(int64_t);
    const auto *perm_data = reinterpret_cast<const int64_t *>(data);
    for (size_t i = 0; i < count; ++i) {
      perm.push_back(perm_data[i]);
    }
  } else {
    return false;
  }
  return true;
}

bool IsSwapLastTwoDims(const std::vector<int64_t> &perm) {
  int64_t dim_num = static_cast<int64_t>(perm.size());
  if (dim_num < kMinDimNumForMatmul) {
    return false;
  }
  for (int64_t i = 0; i < dim_num - kSecondLastDimOffset; ++i) {
    if (perm[static_cast<size_t>(i)] != i) {
      return false;
    }
  }
  return perm[static_cast<size_t>(dim_num - kSecondLastDimOffset)] == dim_num - kLastDimOffset &&
         perm[static_cast<size_t>(dim_num - kLastDimOffset)] == dim_num - kSecondLastDimOffset;
}

bool ValidateBmmNode(const std::unique_ptr<MatchResult> &match_result, GNode &bmm_node) {
  NodeIo bmm_node_io;
  if (match_result->GetCapturedTensor(kCaptureIdx0, bmm_node_io) != SUCCESS) {
    return false;
  }
  bmm_node = bmm_node_io.node;

  if (bmm_node.GetInputsSize() != kSupportedInputNum) {
    std::cout << "Only x1 and x2 inputs are supported, skip" << std::endl;
    return false;
  }

  bool adj_x1 = HasAdjAttr(bmm_node, "adj_x1", "transpose_x1");
  if (adj_x1) {
    std::cout << "adj_x1 is true, skip" << std::endl;
    return false;
  }

  TensorDesc input0_desc;
  if (bmm_node.GetInputDesc(kBmmInputIndex0, input0_desc) != SUCCESS) {
    return false;
  }
  if (!IsSupportedDataType(input0_desc.GetDataType())) {
    std::cout << "Unsupported dtype, skip" << std::endl;
    return false;
  }

  auto out_consumers = bmm_node.GetOutDataNodesAndPortIndexs(kBmmOutputIndex0);
  if (out_consumers.size() > kMaxOutputConsumers) {
    std::cout << "BMM output has more than one consumer, skip" << std::endl;
    return false;
  }

  return true;
}

bool GetPattern1Dims(const GNode &bmm_node, int64_t &n, int64_t &k) {
  bool adj_x2 = HasAdjAttr(bmm_node, "adj_x2", "transpose_x2");
  if (!adj_x2) {
    std::cout << "Pattern 1 requires adj_x2=true, skip" << std::endl;
    return false;
  }

  TensorDesc input1_desc;
  if (bmm_node.GetInputDesc(kBmmInputIndex1, input1_desc) != SUCCESS) {
    return false;
  }
  ge::Shape input1_shape = input1_desc.GetShape();
  if (input1_shape.GetDimNum() < kMinDimNumForMatmul) {
    return false;
  }
  n = input1_shape.GetDim(input1_shape.GetDimNum() - kSecondLastDimOffset);
  k = input1_shape.GetDim(input1_shape.GetDimNum() - kLastDimOffset);
  return true;
}

bool GetPattern2Dims(const GNode &bmm_node, const NodeIo &transpose_node_io, int64_t &n, int64_t &k) {
  bool adj_x2 = HasAdjAttr(bmm_node, "adj_x2", "transpose_x2");
  if (adj_x2) {
    std::cout << "Pattern 2 requires adj_x2=false, skip" << std::endl;
    return false;
  }

  GNode transpose_node = transpose_node_io.node;
  std::vector<int64_t> perm;
  if (!ReadTransposePerm(transpose_node, perm)) {
    std::cout << "Failed to read transpose perm, skip" << std::endl;
    return false;
  }
  if (!IsSwapLastTwoDims(perm)) {
    std::cout << "Transpose does not swap last two dims, skip" << std::endl;
    return false;
  }

  TensorDesc trans_input_desc;
  if (transpose_node.GetInputDesc(kTransposeDataInputIndex, trans_input_desc) != SUCCESS) {
    return false;
  }
  ge::Shape trans_input_shape = trans_input_desc.GetShape();
  if (trans_input_shape.GetDimNum() < kMinDimNumForMatmul) {
    return false;
  }
  n = trans_input_shape.GetDim(trans_input_shape.GetDimNum() - kSecondLastDimOffset);
  k = trans_input_shape.GetDim(trans_input_shape.GetDimNum() - kLastDimOffset);
  return true;
}
}  // namespace

class BmmToMulReducePass : public PatternFusionPass {
 protected:
  std::vector<PatternUniqPtr> Patterns() override {
    std::cout << "Define pattern for BmmToMulReducePass" << std::endl;
    std::vector<PatternUniqPtr> patterns;

    auto builder_p1 = es::EsGraphBuilder("pattern_bmm_adj_x2_true");
    auto [input_a_p1, input_b_p1] = builder_p1.CreateInputs<2>();
    auto bmm_p1 = es::BatchMatMulV2(input_a_p1, input_b_p1);
    auto graph_p1 = builder_p1.BuildAndReset({bmm_p1});
    auto pattern_p1 = std::make_unique<Pattern>(std::move(*graph_p1));
    pattern_p1->CaptureTensor({*bmm_p1.GetProducer(), 0});
    patterns.emplace_back(std::move(pattern_p1));

    auto builder_p2 = es::EsGraphBuilder("pattern_bmm_transpose");
    auto [input_a_p2, input_b_p2] = builder_p2.CreateInputs<2>();
    auto perm = es::Const(builder_p2);
    auto transpose = es::Transpose(input_b_p2, perm);
    auto bmm_p2 = es::BatchMatMulV2(input_a_p2, transpose);
    auto graph_p2 = builder_p2.BuildAndReset({bmm_p2});
    auto pattern_p2 = std::make_unique<Pattern>(std::move(*graph_p2));
    pattern_p2->CaptureTensor({*bmm_p2.GetProducer(), 0});
    pattern_p2->CaptureTensor({*transpose.GetProducer(), 0});
    patterns.emplace_back(std::move(pattern_p2));

    return patterns;
  }

  bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
    std::cout << "Define MeetRequirements for BmmToMulReducePass" << std::endl;

    GNode bmm_node;
    if (!ValidateBmmNode(match_result, bmm_node)) {
      return false;
    }

    NodeIo transpose_node_io;
    bool is_pattern2 = (match_result->GetCapturedTensor(kCaptureIdx1, transpose_node_io) == SUCCESS);

    int64_t n = 0;
    int64_t k = 0;

    if (is_pattern2) {
      if (!GetPattern2Dims(bmm_node, transpose_node_io, n, k)) {
        return false;
      }
    } else {
      if (!GetPattern1Dims(bmm_node, n, k)) {
        return false;
      }
    }

    if (n != kTargetN || k < kMinK || k > kMaxK) {
      std::cout << "n=" << n << " k=" << k << " does not meet n==1 && k<=16, skip" << std::endl;
      return false;
    }

    return true;
  }

  GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
    std::cout << "Define replacement for BmmToMulReducePass" << std::endl;

    NodeIo bmm_node_io;
    if (match_result->GetCapturedTensor(kCaptureIdx0, bmm_node_io) != SUCCESS) {
      return nullptr;
    }
    GNode bmm_node = bmm_node_io.node;

    auto builder = es::EsGraphBuilder("replacement_bmm_to_mul_reduce");
    auto [input_a, input_b] = builder.CreateInputs<2>();
    auto mul = es::Mul(input_a, input_b);
    auto reduce_sum = es::ReduceSumD(mul, {-1}, true);
    auto replace_graph = builder.BuildAndReset({reduce_sum});
    if (InferShapeUtil::InferShape(*replace_graph, bmm_node) != SUCCESS) {
      std::cout << "InferShape failed" << std::endl;
      return nullptr;
    }

    auto nodes = replace_graph->GetAllNodes();
    for (GNode node : nodes) {
      AscendString node_type;
      node.GetType(node_type);
      std::string type_str(node_type.GetString());
      if (type_str == "Const" || type_str == "Data") {
        continue;
      }
      std::cout << "Created node: " << type_str << std::endl;
    }

    std::cout << "Replacement success" << std::endl;
    return replace_graph;
  }
};

REG_FUSION_PASS(BmmToMulReducePass).Stage(CustomPassStage::kAfterInferShape);
