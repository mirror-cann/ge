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
constexpr int32_t kIndex0 = 0;
constexpr int32_t kIndex1 = 1;
constexpr int64_t kTileInputDim0 = 1;
constexpr int64_t kPermsFirstDim = 0;
constexpr int64_t kReshapeFirstDimOne = 1;
constexpr int64_t kMultiplesRestOne = 1;

constexpr size_t kTileCaptureIdx = 0;
constexpr size_t kBmmCaptureIdx = 1;
constexpr size_t kReshapeCaptureIdx = 2;
constexpr size_t kTransposeCaptureIdx = 3;

bool GetConstInt64Values(const GNode &node, int32_t input_index, std::vector<int64_t> &values) {
  Tensor const_tensor;
  if (node.GetInputConstData(input_index, const_tensor) != SUCCESS) {
    return false;
  }
  TensorDesc tensor_desc = const_tensor.GetTensorDesc();
  DataType dt = tensor_desc.GetDataType();
  const uint8_t *data = const_tensor.GetData();
  size_t data_len = const_tensor.GetSize();
  if (data == nullptr || data_len == 0) {
    return false;
  }
  if (dt == DT_INT32) {
    if ((data_len % sizeof(int32_t)) != 0) {
      return false;
    }
    const int32_t *buffer = reinterpret_cast<const int32_t *>(data);
    size_t count = data_len / sizeof(int32_t);
    values.assign(buffer, buffer + count);
  } else if (dt == DT_INT64) {
    if ((data_len % sizeof(int64_t)) != 0) {
      return false;
    }
    const int64_t *buffer = reinterpret_cast<const int64_t *>(data);
    size_t count = data_len / sizeof(int64_t);
    values.assign(buffer, buffer + count);
  } else {
    return false;
  }
  return true;
}

bool HasSingleConsumer(const GNode &node) {
  auto consumers = node.GetOutDataNodesAndPortIndexs(kIndex0);
  return consumers.size() == 1;
}

bool IsChainOnBmmInput(const GNode &bmm_node, const GNode &chain_tail_node, int32_t input_idx) {
  auto [in_node, _] = bmm_node.GetInDataNodesAndPortIndexs(input_idx);
  if (in_node == nullptr) {
    return false;
  }
  AscendString tail_name, in_name;
  chain_tail_node.GetName(tail_name);
  in_node->GetName(in_name);
  return tail_name == in_name;
}

bool CheckBmmTileShape(const GNode &tile_node, const GNode &bmm_node, bool tile_on_input0) {
  TensorDesc tile_input_desc;
  if (tile_node.GetInputDesc(kIndex0, tile_input_desc) != SUCCESS) {
    return false;
  }
  ge::Shape tile_input_shape = tile_input_desc.GetShape();
  if (tile_input_shape.GetDimNum() == 0 || tile_input_shape.GetDim(0) != kTileInputDim0) {
    std::cout << "Tile input dim[0] is not 1, skip" << std::endl;
    return false;
  }

  int32_t other_idx = tile_on_input0 ? kIndex1 : kIndex0;
  TensorDesc other_desc;
  if (bmm_node.GetInputDesc(other_idx, other_desc) != SUCCESS) {
    return false;
  }
  ge::Shape other_shape = other_desc.GetShape();
  if (other_shape.GetDimNum() == 0 || other_shape.GetDim(0) == kTileInputDim0) {
    std::cout << "BMM other side dim[0] is 1, skip" << std::endl;
    return false;
  }
  return true;
}

bool CheckTileMultiplesOnlyFirstDim(const GNode &tile_node) {
  std::vector<int64_t> multiples;
  if (!GetConstInt64Values(tile_node, kIndex1, multiples)) {
    std::cout << "Tile multiples is not Const, skip" << std::endl;
    return false;
  }
  for (size_t i = 1; i < multiples.size(); ++i) {
    if (multiples[i] != kMultiplesRestOne) {
      std::cout << "Tile multiples[" << i << "] is not 1, skip" << std::endl;
      return false;
    }
  }
  return true;
}

bool CheckPattern2Requirements(const GNode &tile_node, const GNode &reshape_node, const GNode &transpose_node) {
  if (!HasSingleConsumer(reshape_node)) {
    std::cout << "Reshape output has more than 1 consumer, skip" << std::endl;
    return false;
  }
  if (!HasSingleConsumer(transpose_node)) {
    std::cout << "Transpose output has more than 1 consumer, skip" << std::endl;
    return false;
  }

  std::vector<int64_t> perm_values;
  if (!GetConstInt64Values(transpose_node, kIndex1, perm_values)) {
    std::cout << "Transpose perm is not Const, skip" << std::endl;
    return false;
  }
  if (perm_values.empty() || perm_values[0] != kPermsFirstDim) {
    std::cout << "Transpose perm[0] is not 0, skip" << std::endl;
    return false;
  }

  std::vector<int64_t> shape_values;
  if (!GetConstInt64Values(reshape_node, kIndex1, shape_values)) {
    std::cout << "Reshape shape is not Const, skip" << std::endl;
    return false;
  }
  if (shape_values.empty()) {
    std::cout << "Reshape shape is empty, skip" << std::endl;
    return false;
  }
  int64_t axis_first_dim = shape_values[0];

  std::vector<int64_t> multiples_values;
  if (!GetConstInt64Values(tile_node, kIndex1, multiples_values)) {
    std::cout << "Tile multiples is not Const, skip" << std::endl;
    return false;
  }
  if (multiples_values.empty() || multiples_values[0] != axis_first_dim) {
    std::cout << "Tile multiples[0] != reshape axis first dim, skip" << std::endl;
    return false;
  }
  for (size_t i = 1; i < multiples_values.size(); ++i) {
    if (multiples_values[i] != kMultiplesRestOne) {
      std::cout << "Tile multiples[" << i << "] != 1, skip" << std::endl;
      return false;
    }
  }
  return true;
}

void LogCreatedNodes(const std::vector<GNode> &nodes) {
  for (GNode node : nodes) {
    AscendString node_type;
    node.GetType(node_type);
    std::string type_str(node_type.GetString());
    if (type_str == "Const" || type_str == "Data") {
      continue;
    }
    std::cout << "Created node: " << type_str << std::endl;
  }
}

void UpdateDataNodeDescs(const std::vector<GNode> &nodes, const TensorDesc &tile_input_desc) {
  for (GNode node : nodes) {
    AscendString node_type;
    node.GetType(node_type);
    std::string type_str(node_type.GetString());
    if (type_str == "Data") {
      (void)node.UpdateInputDesc(0, tile_input_desc);
      (void)node.UpdateOutputDesc(0, tile_input_desc);
    }
  }
}

PatternUniqPtr BuildPattern1a() {
  auto builder = es::EsGraphBuilder("pattern_1a_tile_on_input0");
  auto [tile_data, bmm_other] = builder.CreateInputs<2>();
  auto tile_multiples = es::Const(builder);
  auto tile = es::Tile(tile_data, tile_multiples);
  auto bmm = es::BatchMatMulV2(tile, bmm_other);
  auto graph = builder.BuildAndReset({bmm});
  auto pattern = std::make_unique<Pattern>(std::move(*graph));
  pattern->CaptureTensor({*tile.GetProducer(), 0}).CaptureTensor({*bmm.GetProducer(), 0});
  return pattern;
}

PatternUniqPtr BuildPattern1b() {
  auto builder = es::EsGraphBuilder("pattern_1b_tile_on_input1");
  auto [bmm_other, tile_data] = builder.CreateInputs<2>();
  auto tile_multiples = es::Const(builder);
  auto tile = es::Tile(tile_data, tile_multiples);
  auto bmm = es::BatchMatMulV2(bmm_other, tile);
  auto graph = builder.BuildAndReset({bmm});
  auto pattern = std::make_unique<Pattern>(std::move(*graph));
  pattern->CaptureTensor({*tile.GetProducer(), 0}).CaptureTensor({*bmm.GetProducer(), 0});
  return pattern;
}

PatternUniqPtr BuildPattern2a() {
  auto builder = es::EsGraphBuilder("pattern_2a_tile_reshape_transpose_on_input0");
  auto [tile_data, bmm_other] = builder.CreateInputs<2>();
  auto tile_multiples = es::Const(builder);
  auto reshape_shape = es::Const(builder);
  auto transpose_perm = es::Const(builder);
  auto tile = es::Tile(tile_data, tile_multiples);
  auto reshape = es::Reshape(tile, reshape_shape);
  auto transpose = es::Transpose(reshape, transpose_perm);
  auto bmm = es::BatchMatMulV2(transpose, bmm_other);
  auto graph = builder.BuildAndReset({bmm});
  auto pattern = std::make_unique<Pattern>(std::move(*graph));
  pattern->CaptureTensor({*tile.GetProducer(), 0})
      .CaptureTensor({*bmm.GetProducer(), 0})
      .CaptureTensor({*reshape.GetProducer(), 0})
      .CaptureTensor({*transpose.GetProducer(), 0});
  return pattern;
}

PatternUniqPtr BuildPattern2b() {
  auto builder = es::EsGraphBuilder("pattern_2b_tile_reshape_transpose_on_input1");
  auto [bmm_other, tile_data] = builder.CreateInputs<2>();
  auto tile_multiples = es::Const(builder);
  auto reshape_shape = es::Const(builder);
  auto transpose_perm = es::Const(builder);
  auto tile = es::Tile(tile_data, tile_multiples);
  auto reshape = es::Reshape(tile, reshape_shape);
  auto transpose = es::Transpose(reshape, transpose_perm);
  auto bmm = es::BatchMatMulV2(bmm_other, transpose);
  auto graph = builder.BuildAndReset({bmm});
  auto pattern = std::make_unique<Pattern>(std::move(*graph));
  pattern->CaptureTensor({*tile.GetProducer(), 0})
      .CaptureTensor({*bmm.GetProducer(), 0})
      .CaptureTensor({*reshape.GetProducer(), 0})
      .CaptureTensor({*transpose.GetProducer(), 0});
  return pattern;
}
}  // namespace

// |o>-------------------------------------------
// |o>  Pattern 1:                        After:
// |o>
// |o>  Input0 [1,k,n]   Input1 [b,m,k]   Input0 [1,k,n]   Input1 [b,m,k]
// |o>       |                |                |                |
// |o>     Tile [b,k,n]        |                |                |
// |o>       |                |                |                |
// |o>   BatchMatMulV2 [b,m,n]          BatchMatMulV2 [b,m,n]
// |o>            |                          |
// |o>          y [b,m,n]                 y [b,m,n]
// |o>
// |o>  Pattern 2:                        After:
// |o>
// |o>  Input0 [1,k,n]   Input1 [b,m,k]   Input0 [1,k,n]   Input1 [b,m,k]
// |o>       |                |                |                |
// |o>     Tile [b,k,n]        |             Reshape [1,k,n]     |
// |o>       |                |                |                |
// |o>   Reshape [b,...]       |            Transpose [1,...]   |
// |o>       |                |                |                |
// |o>  Transpose [b,...]      |                |                |
// |o>       |                |                |                |
// |o>   BatchMatMulV2 [b,m,n]          BatchMatMulV2 [b,m,n]
// |o>            |                          |
// |o>          y [b,m,n]                 y [b,m,n]
// |o>-------------------------------------------
class BmmTilePass : public PatternFusionPass {
 protected:
  std::vector<PatternUniqPtr> Patterns() override {
    std::cout << "Define pattern for BmmTilePass" << std::endl;
    std::vector<PatternUniqPtr> patterns;
    patterns.emplace_back(BuildPattern1a());
    patterns.emplace_back(BuildPattern1b());
    patterns.emplace_back(BuildPattern2a());
    patterns.emplace_back(BuildPattern2b());
    return patterns;
  }

  bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
    std::cout << "Define MeetRequirements for BmmTilePass" << std::endl;

    NodeIo tile_io, bmm_io;
    if (match_result->GetCapturedTensor(kTileCaptureIdx, tile_io) != SUCCESS) {
      return false;
    }
    if (match_result->GetCapturedTensor(kBmmCaptureIdx, bmm_io) != SUCCESS) {
      return false;
    }
    GNode tile_node = tile_io.node;
    GNode bmm_node = bmm_io.node;

    bool is_pattern2 = false;
    NodeIo reshape_io, transpose_io;
    if (match_result->GetCapturedTensor(kReshapeCaptureIdx, reshape_io) == SUCCESS) {
      is_pattern2 = true;
      if (match_result->GetCapturedTensor(kTransposeCaptureIdx, transpose_io) != SUCCESS) {
        return false;
      }
    }

    if (!HasSingleConsumer(tile_node)) {
      std::cout << "Tile output has more than 1 consumer, skip" << std::endl;
      return false;
    }

    GNode chain_tail = is_pattern2 ? transpose_io.node : tile_io.node;
    bool tile_on_input0 = IsChainOnBmmInput(bmm_node, chain_tail, kIndex0);
    if (!CheckBmmTileShape(tile_node, bmm_node, tile_on_input0)) {
      return false;
    }

    if (!CheckTileMultiplesOnlyFirstDim(tile_node)) {
      return false;
    }

    if (is_pattern2) {
      if (!CheckPattern2Requirements(tile_node, reshape_io.node, transpose_io.node)) {
        return false;
      }
    }

    return true;
  }

  GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
    std::cout << "Define replacement for BmmTilePass" << std::endl;

    NodeIo tile_io, bmm_io;
    match_result->GetCapturedTensor(kTileCaptureIdx, tile_io);
    match_result->GetCapturedTensor(kBmmCaptureIdx, bmm_io);
    GNode tile_node = tile_io.node;
    GNode bmm_node = bmm_io.node;

    bool is_pattern2 = false;
    NodeIo reshape_io, transpose_io;
    if (match_result->GetCapturedTensor(kReshapeCaptureIdx, reshape_io) == SUCCESS) {
      is_pattern2 = true;
      match_result->GetCapturedTensor(kTransposeCaptureIdx, transpose_io);
    }

    GNode chain_tail = is_pattern2 ? transpose_io.node : tile_io.node;
    bool tile_on_input0 = IsChainOnBmmInput(bmm_node, chain_tail, kIndex0);

    std::vector<int64_t> reshape_shape_values;
    std::vector<int64_t> perm_values;
    if (is_pattern2) {
      GNode reshape_node = reshape_io.node;
      GNode transpose_node = transpose_io.node;
      if (!GetConstInt64Values(reshape_node, kIndex1, reshape_shape_values)) {
        std::cout << "Failed to read Reshape shape Const" << std::endl;
        return nullptr;
      }
      reshape_shape_values[0] = kReshapeFirstDimOne;
      if (!GetConstInt64Values(transpose_node, kIndex1, perm_values)) {
        std::cout << "Failed to read Transpose perm Const" << std::endl;
        return nullptr;
      }
    }

    auto builder = es::EsGraphBuilder("replacement_bmm_tile");

    if (is_pattern2) {
      return BuildPattern2Replacement(builder, tile_node, tile_on_input0, bmm_node, reshape_shape_values, perm_values);
    }
    return BuildPattern1Replacement(builder, tile_on_input0, bmm_node);
  }

 private:
  bool InferAndLog(Graph &graph, const GNode &matched_node) {
    if (InferShapeUtil::InferShape(graph, matched_node) != SUCCESS) {
      std::cout << "InferShape failed, skipping this match" << std::endl;
      return false;
    }
    LogCreatedNodes(graph.GetAllNodes());
    std::cout << "InferShape success" << std::endl;
    return true;
  }

  bool InferAndLogWithInputDesc(Graph &graph, const GNode &matched_node, const TensorDesc &tile_input_desc,
                                int32_t tile_data_index, const std::vector<int64_t> &reshape_shape_values,
                                const std::vector<int64_t> &perm_values) {
    auto nodes = graph.GetAllNodes();
    UpdateDataNodeDescs(nodes, tile_input_desc);

    TensorDesc bmm_other_desc;
    int32_t other_idx = (tile_data_index == 0) ? kIndex1 : kIndex0;
    if (matched_node.GetInputDesc(other_idx, bmm_other_desc) != SUCCESS) {
      std::cout << "Failed to get BMM other input desc" << std::endl;
      return false;
    }

    TensorDesc reshape_out_desc = tile_input_desc;
    ge::Shape reshape_shape(reshape_shape_values);
    reshape_out_desc.SetShape(reshape_shape);
    reshape_out_desc.SetOriginShape(reshape_shape);
    TensorDesc transpose_out_desc = reshape_out_desc;

    for (GNode node : nodes) {
      AscendString node_type;
      node.GetType(node_type);
      std::string type_str(node_type.GetString());
      if (type_str == "Reshape") {
        (void)node.UpdateInputDesc(0, tile_input_desc);
        (void)node.UpdateOutputDesc(0, reshape_out_desc);
      } else if (type_str == "Transpose" || type_str == "TransposeD") {
        (void)node.UpdateInputDesc(0, reshape_out_desc);
        TensorDesc perm_desc(ge::Shape({static_cast<int64_t>(perm_values.size())}), FORMAT_ND, DT_INT64);
        (void)node.UpdateInputDesc(1, perm_desc);
        (void)node.UpdateOutputDesc(0, transpose_out_desc);
      } else if (type_str == "BatchMatMulV2") {
        if (tile_data_index == 0) {
          (void)node.UpdateInputDesc(0, transpose_out_desc);
          (void)node.UpdateInputDesc(1, bmm_other_desc);
        } else {
          (void)node.UpdateInputDesc(0, bmm_other_desc);
          (void)node.UpdateInputDesc(1, transpose_out_desc);
        }
        TensorDesc bmm_out_desc;
        if (matched_node.GetOutputDesc(0, bmm_out_desc) == SUCCESS) {
          (void)node.UpdateOutputDesc(0, bmm_out_desc);
        }
      }
    }

    LogCreatedNodes(nodes);
    std::cout << "InferShape success (manual desc)" << std::endl;
    return true;
  }

  GraphUniqPtr BuildPattern2Replacement(es::EsGraphBuilder &builder, const GNode &tile_node, bool tile_on_input0,
                                        const GNode &bmm_node, const std::vector<int64_t> &reshape_shape_values,
                                        const std::vector<int64_t> &perm_values) {
    TensorDesc tile_input_desc;
    if (tile_node.GetInputDesc(kIndex0, tile_input_desc) != SUCCESS) {
      std::cout << "Failed to get Tile input desc" << std::endl;
      return nullptr;
    }

    if (tile_on_input0) {
      auto [tile_data, bmm_other] = builder.CreateInputs<2>();
      auto new_shape_const = builder.CreateConst(
          reshape_shape_values, std::vector<int64_t>{static_cast<int64_t>(reshape_shape_values.size())});
      auto new_perm_const =
          builder.CreateConst(perm_values, std::vector<int64_t>{static_cast<int64_t>(perm_values.size())});
      auto reshape = es::Reshape(tile_data, new_shape_const);
      auto transpose = es::Transpose(reshape, new_perm_const);
      auto bmm = es::BatchMatMulV2(transpose, bmm_other);
      auto graph = builder.BuildAndReset({bmm});
      if (!InferAndLogWithInputDesc(*graph, bmm_node, tile_input_desc, 0, reshape_shape_values, perm_values)) {
        return nullptr;
      }
      return graph;
    } else {
      auto [bmm_other, tile_data] = builder.CreateInputs<2>();
      auto new_shape_const = builder.CreateConst(
          reshape_shape_values, std::vector<int64_t>{static_cast<int64_t>(reshape_shape_values.size())});
      auto new_perm_const =
          builder.CreateConst(perm_values, std::vector<int64_t>{static_cast<int64_t>(perm_values.size())});
      auto reshape = es::Reshape(tile_data, new_shape_const);
      auto transpose = es::Transpose(reshape, new_perm_const);
      auto bmm = es::BatchMatMulV2(bmm_other, transpose);
      auto graph = builder.BuildAndReset({bmm});
      if (!InferAndLogWithInputDesc(*graph, bmm_node, tile_input_desc, 1, reshape_shape_values, perm_values)) {
        return nullptr;
      }
      return graph;
    }
  }

  GraphUniqPtr BuildPattern1Replacement(es::EsGraphBuilder &builder, bool tile_on_input0, const GNode &bmm_node) {
    if (tile_on_input0) {
      auto [tile_data, bmm_other] = builder.CreateInputs<2>();
      auto bmm = es::BatchMatMulV2(tile_data, bmm_other);
      auto graph = builder.BuildAndReset({bmm});
      if (!InferAndLog(*graph, bmm_node)) {
        return nullptr;
      }
      return graph;
    } else {
      auto [bmm_other, tile_data] = builder.CreateInputs<2>();
      auto bmm = es::BatchMatMulV2(bmm_other, tile_data);
      auto graph = builder.BuildAndReset({bmm});
      if (!InferAndLog(*graph, bmm_node)) {
        return nullptr;
      }
      return graph;
    }
  }
};

REG_FUSION_PASS(BmmTilePass).Stage(CustomPassStage::kAfterInferShape);
