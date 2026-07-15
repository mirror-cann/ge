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
#include <map>
#include <string>
#include <vector>
#include "register_custom_pass.h"
#include "all_ops.h"

using namespace std;
using namespace ge;

namespace {
constexpr const char *kOpNameData = "Data";
constexpr const char *kOpNameConst = "Const";
constexpr const char *kOpNameConstant = "Constant";
constexpr const char *kOpNameMatmulV2 = "MatMulV2";
constexpr const char *kOpNameMatmulV3 = "MatMulV3";
constexpr const char *kOpNameBatchMatmulV2 = "BatchMatMulV2";
constexpr int32_t kRecursionSearchMaxLevel = 10;
constexpr int32_t kIndex0 = 0;
constexpr int32_t kIndex1 = 1;
constexpr int32_t kPackAxis = 0;
constexpr int32_t kMinDimsForMatmul = 2;
constexpr int32_t kMinNoConstInputsForRoot = 2;

bool CheckNodeType(const GNode &node, const char *node_type) {
  AscendString type;
  if (node.GetType(type) != GRAPH_SUCCESS) {
    return false;
  }
  return type == node_type;
}

bool RecursionSearchRoot(const GNode &src_node, string &root_name, int32_t search_level, int32_t &matmul_level) {
  if (search_level > kRecursionSearchMaxLevel) {
    return false;
  }

  if (CheckNodeType(src_node, kOpNameData)) {
    AscendString name;
    if (src_node.GetName(name) != GRAPH_SUCCESS) {
      return false;
    }
    root_name = string(name.GetString());
    return true;
  }

  auto inputs_size = src_node.GetInputsSize();
  if (inputs_size <= 0) {
    return false;
  }

  if (CheckNodeType(src_node, kOpNameConst) || CheckNodeType(src_node, kOpNameConstant)) {
    return false;
  }

  GNodePtr next_node;
  int32_t no_const_input_num = 0;
  for (auto i = static_cast<size_t>(kIndex0); i < static_cast<size_t>(inputs_size); i++) {
    auto [in_node, _] = src_node.GetInDataNodesAndPortIndexs(i);
    if (in_node == nullptr) {
      continue;
    }
    if (CheckNodeType(*in_node, kOpNameConst) || CheckNodeType(*in_node, kOpNameConstant)) {
      continue;
    }
    if (no_const_input_num == 0) {
      next_node = in_node;
    }
    no_const_input_num++;
  }

  if (no_const_input_num >= kMinNoConstInputsForRoot) {
    AscendString name;
    if (src_node.GetName(name) != GRAPH_SUCCESS) {
      return false;
    }
    root_name = string(name.GetString());
    return true;
  }

  if (CheckNodeType(src_node, kOpNameMatmulV2) || CheckNodeType(src_node, kOpNameMatmulV3) ||
      CheckNodeType(src_node, kOpNameBatchMatmulV2)) {
    matmul_level++;
  }

  if (next_node == nullptr) {
    return false;
  }
  return RecursionSearchRoot(*next_node, root_name, search_level + 1, matmul_level);
}

void FindSameLevelMatmulNode(const GraphPtr &graph, map<string, map<int32_t, vector<GNode>>> &matmul_node_map) {
  auto nodes = graph->GetAllNodes();
  for (auto &node : nodes) {
    if (CheckNodeType(node, kOpNameMatmulV2) || CheckNodeType(node, kOpNameMatmulV3) ||
        CheckNodeType(node, kOpNameBatchMatmulV2)) {
      int32_t matmul_level = 0;
      string root_name;
      if (RecursionSearchRoot(node, root_name, 0, matmul_level)) {
        matmul_node_map[root_name][matmul_level].push_back(node);
      }
    }
  }
}

bool GetTransposeAttr(const GNode &node, bool &tx1, bool &tx2) {
  if (node.GetAttr("transpose_x1", tx1) == GRAPH_SUCCESS && node.GetAttr("transpose_x2", tx2) == GRAPH_SUCCESS) {
    return true;
  }
  if (node.GetAttr("adj_x1", tx1) == GRAPH_SUCCESS && node.GetAttr("adj_x2", tx2) == GRAPH_SUCCESS) {
    return true;
  }
  tx1 = false;
  tx2 = false;
  return true;
}

graphStatus CheckNodeShapeMatch(const GNode &node, bool transpose_x1, bool transpose_x2,
                                const vector<int64_t> &base_dims0, const vector<int64_t> &base_dims1) {
  bool tx1 = false;
  bool tx2 = false;
  if (!GetTransposeAttr(node, tx1, tx2)) {
    return GRAPH_FAILED;
  }
  if (tx1 != transpose_x1 || tx2 != transpose_x2) {
    return GRAPH_FAILED;
  }

  TensorDesc input0;
  TensorDesc input1;
  if (node.GetInputDesc(kIndex0, input0) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }
  if (node.GetInputDesc(kIndex1, input1) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }
  auto dims0 = input0.GetShape().GetDims();
  auto dims1 = input1.GetShape().GetDims();
  if (dims0.size() != base_dims0.size()) {
    return GRAPH_FAILED;
  }
  if (dims0[kIndex0] != base_dims0[kIndex0] || dims0[kIndex1] != base_dims0[kIndex1]) {
    return GRAPH_FAILED;
  }
  if (dims1[kIndex0] != base_dims1[kIndex0] || dims1[kIndex1] != base_dims1[kIndex1]) {
    return GRAPH_FAILED;
  }
  return GRAPH_SUCCESS;
}

graphStatus CheckMatmulShape(vector<GNode> &matmul_node_list, bool &transpose_x1, bool &transpose_x2) {
  if (matmul_node_list.empty()) {
    return GRAPH_FAILED;
  }

  GNode &base = matmul_node_list[kIndex0];
  if (!GetTransposeAttr(base, transpose_x1, transpose_x2)) {
    return GRAPH_FAILED;
  }

  TensorDesc base_input0;
  TensorDesc base_input1;
  if (base.GetInputDesc(kIndex0, base_input0) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }
  if (base.GetInputDesc(kIndex1, base_input1) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }
  auto base_dims0 = base_input0.GetShape().GetDims();
  auto base_dims1 = base_input1.GetShape().GetDims();
  if (base_dims0.size() < kMinDimsForMatmul || base_dims1.size() < kMinDimsForMatmul) {
    return GRAPH_FAILED;
  }

  for (size_t i = 1; i < matmul_node_list.size(); i++) {
    if (CheckNodeShapeMatch(matmul_node_list[i], transpose_x1, transpose_x2, base_dims0, base_dims1) != GRAPH_SUCCESS) {
      return GRAPH_FAILED;
    }
  }
  return GRAPH_SUCCESS;
}

TensorDesc MakePackOutputDesc(const TensorDesc &input_desc, int32_t n) {
  auto dims = input_desc.GetShape().GetDims();
  dims.insert(dims.begin(), static_cast<int64_t>(n));
  TensorDesc desc(Shape(dims), input_desc.GetFormat(), input_desc.GetDataType());
  desc.SetOriginShape(Shape(dims));
  desc.SetOriginFormat(input_desc.GetFormat());
  return desc;
}

TensorDesc MakeBmmOutputDesc(const TensorDesc &desc0, const TensorDesc &desc1) {
  auto dims0 = desc0.GetShape().GetDims();
  auto dims1 = desc1.GetShape().GetDims();
  vector<int64_t> out_dims = {dims0[kIndex0], dims0[kIndex1], dims1[kIndex1]};
  TensorDesc desc(Shape(out_dims), desc0.GetFormat(), desc0.GetDataType());
  desc.SetOriginShape(Shape(out_dims));
  desc.SetOriginFormat(desc0.GetFormat());
  return desc;
}

TensorDesc MakeSplitOutputDesc(const TensorDesc &bmm_desc) {
  auto dims = bmm_desc.GetShape().GetDims();
  dims[kIndex0] = 1;
  TensorDesc desc(Shape(dims), bmm_desc.GetFormat(), bmm_desc.GetDataType());
  desc.SetOriginShape(Shape(dims));
  desc.SetOriginFormat(bmm_desc.GetFormat());
  return desc;
}

TensorDesc MakeSqueezeOutputDesc(const TensorDesc &split_desc) {
  auto dims = split_desc.GetShape().GetDims();
  if (!dims.empty()) {
    dims.erase(dims.begin());
  }
  TensorDesc desc(Shape(dims), split_desc.GetFormat(), split_desc.GetDataType());
  desc.SetOriginShape(Shape(dims));
  desc.SetOriginFormat(split_desc.GetFormat());
  return desc;
}

graphStatus RewireMatmulInputsToPack(const GraphPtr &graph, GNode &matmul, GNode &pack_0_node, GNode &pack_1_node,
                                     int32_t i) {
  TensorDesc input_desc_0;
  TensorDesc input_desc_1;
  if (matmul.GetInputDesc(kIndex0, input_desc_0) != GRAPH_SUCCESS ||
      matmul.GetInputDesc(kIndex1, input_desc_1) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }

  pack_0_node.UpdateInputDesc(i, input_desc_0);
  pack_1_node.UpdateInputDesc(i, input_desc_1);

  auto [in_node_0, in_port_0] = matmul.GetInDataNodesAndPortIndexs(kIndex0);
  if (in_node_0 == nullptr) {
    return GRAPH_FAILED;
  }
  if (graph->RemoveEdge(*in_node_0, in_port_0, matmul, kIndex0) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }
  if (graph->AddDataEdge(*in_node_0, in_port_0, pack_0_node, i) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }

  auto [in_node_1, in_port_1] = matmul.GetInDataNodesAndPortIndexs(kIndex1);
  if (in_node_1 == nullptr) {
    return GRAPH_FAILED;
  }
  if (graph->RemoveEdge(*in_node_1, in_port_1, matmul, kIndex1) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }
  if (graph->AddDataEdge(*in_node_1, in_port_1, pack_1_node, i) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }

  return GRAPH_SUCCESS;
}

graphStatus ProcessPackNode(const GraphPtr &graph, vector<GNode> &matmul_node_list, GNode &pack_0_node,
                            GNode &pack_1_node, const string &prefix, int32_t index) {
  int32_t n = static_cast<int32_t>(matmul_node_list.size());

  string pack_0_name = prefix + "pack_0_" + to_string(index);
  auto pack_0 = op::Pack(pack_0_name.c_str())
                    .create_dynamic_input_x(static_cast<uint32_t>(n))
                    .set_attr_N(static_cast<int64_t>(n))
                    .set_attr_axis(static_cast<int64_t>(kPackAxis));
  pack_0_node = graph->AddNodeByOp(pack_0);

  string pack_1_name = prefix + "pack_1_" + to_string(index);
  auto pack_1 = op::Pack(pack_1_name.c_str())
                    .create_dynamic_input_x(static_cast<uint32_t>(n))
                    .set_attr_N(static_cast<int64_t>(n))
                    .set_attr_axis(static_cast<int64_t>(kPackAxis));
  pack_1_node = graph->AddNodeByOp(pack_1);

  for (int32_t i = kIndex0; i < n; i++) {
    if (RewireMatmulInputsToPack(graph, matmul_node_list[i], pack_0_node, pack_1_node, i) != GRAPH_SUCCESS) {
      return GRAPH_FAILED;
    }
  }

  TensorDesc base_input0;
  TensorDesc base_input1;
  if (matmul_node_list[kIndex0].GetInputDesc(kIndex0, base_input0) != GRAPH_SUCCESS ||
      matmul_node_list[kIndex0].GetInputDesc(kIndex1, base_input1) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }
  pack_0_node.UpdateOutputDesc(kIndex0, MakePackOutputDesc(base_input0, n));
  pack_1_node.UpdateOutputDesc(kIndex0, MakePackOutputDesc(base_input1, n));

  return GRAPH_SUCCESS;
}

graphStatus ProcessBatchMatmulNode(const GraphPtr &graph, GNode &pack_0_node, GNode &pack_1_node, GNode &bmm_node,
                                   const string &prefix, int32_t index, bool transpose_x1, bool transpose_x2) {
  string bmm_name = prefix + "batch_matmul_" + to_string(index);
  auto bmm = op::BatchMatMulV2(bmm_name.c_str());
  bmm_node = graph->AddNodeByOp(bmm);
  bmm_node.SetAttr("adj_x1", transpose_x1);
  bmm_node.SetAttr("adj_x2", transpose_x2);

  TensorDesc pack_output_0;
  TensorDesc pack_output_1;
  if (pack_0_node.GetOutputDesc(kIndex0, pack_output_0) != GRAPH_SUCCESS ||
      pack_1_node.GetOutputDesc(kIndex0, pack_output_1) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }

  bmm_node.UpdateInputDesc(kIndex0, pack_output_0);
  bmm_node.UpdateInputDesc(kIndex1, pack_output_1);
  bmm_node.UpdateOutputDesc(kIndex0, MakeBmmOutputDesc(pack_output_0, pack_output_1));

  if (graph->AddDataEdge(pack_0_node, kIndex0, bmm_node, kIndex0) != GRAPH_SUCCESS ||
      graph->AddDataEdge(pack_1_node, kIndex0, bmm_node, kIndex1) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }

  return GRAPH_SUCCESS;
}

graphStatus ProcessSplitAndSqueezeNode(const GraphPtr &graph, vector<GNode> &matmul_node_list, GNode &bmm_node,
                                       const string &prefix, int32_t index) {
  int32_t n = static_cast<int32_t>(matmul_node_list.size());

  int32_t split_dim_val = kPackAxis;
  TensorDesc split_dim_desc(Shape({1}), FORMAT_ND, DT_INT32);
  split_dim_desc.SetOriginShape(Shape({1}));
  split_dim_desc.SetOriginFormat(FORMAT_ND);
  Tensor split_dim_tensor(split_dim_desc, reinterpret_cast<const uint8_t *>(&split_dim_val), sizeof(int32_t));
  string split_dim_name = prefix + "split_dim_" + to_string(index);
  auto split_dim_op = op::Const(split_dim_name.c_str()).set_attr_value(split_dim_tensor);
  GNode split_dim_node = graph->AddNodeByOp(split_dim_op);

  string split_name = prefix + "split_" + to_string(index);
  auto split = op::Split(split_name.c_str())
                   .set_attr_num_split(static_cast<int64_t>(n))
                   .create_dynamic_output_y(static_cast<uint32_t>(n));
  GNode split_node = graph->AddNodeByOp(split);

  TensorDesc bmm_output_desc;
  if (bmm_node.GetOutputDesc(kIndex0, bmm_output_desc) != GRAPH_SUCCESS) {
    return GRAPH_FAILED;
  }

  split_node.UpdateInputDesc(kIndex0, split_dim_desc);
  split_node.UpdateInputDesc(kIndex1, bmm_output_desc);

  TensorDesc split_output_desc = MakeSplitOutputDesc(bmm_output_desc);
  for (int32_t i = kIndex0; i < n; i++) {
    split_node.UpdateOutputDesc(i, split_output_desc);
  }

  graph->AddDataEdge(split_dim_node, kIndex0, split_node, kIndex0);
  graph->AddDataEdge(bmm_node, kIndex0, split_node, kIndex1);

  for (int32_t i = kIndex0; i < n; i++) {
    GNode &matmul = matmul_node_list[i];

    auto squeeze = op::Squeeze((prefix + "squeeze_" + to_string(index) + "_" + to_string(i)).c_str());
    GNode squeeze_node = graph->AddNodeByOp(squeeze);

    squeeze_node.UpdateInputDesc(kIndex0, split_output_desc);
    squeeze_node.UpdateOutputDesc(kIndex0, MakeSqueezeOutputDesc(split_output_desc));

    graph->AddDataEdge(split_node, i, squeeze_node, kIndex0);

    for (size_t j = 0; j < matmul.GetOutputsSize(); j++) {
      auto out_nodes = matmul.GetOutDataNodesAndPortIndexs(j);
      for (auto &[out_node, out_port] : out_nodes) {
        if (out_node == nullptr) {
          continue;
        }
        graph->RemoveEdge(matmul, j, *out_node, out_port);
        graph->AddDataEdge(squeeze_node, kIndex0, *out_node, out_port);
      }
    }

    graph->RemoveNode(matmul);
  }

  return GRAPH_SUCCESS;
}

bool ReplaceMatmul(const GraphPtr &graph, map<string, map<int32_t, vector<GNode>>> &matmul_node_map) {
  int32_t affect_count = 0;
  for (auto &root_pair : matmul_node_map) {
    int32_t index = 0;
    const string &root_name = root_pair.first;
    for (auto &level_pair : root_pair.second) {
      int32_t matmul_level = level_pair.first;
      vector<GNode> &matmul_node_list = level_pair.second;
      if (matmul_node_list.size() <= 1 || matmul_level == 1) {
        continue;
      }

      bool transpose_x1 = false;
      bool transpose_x2 = false;
      if (CheckMatmulShape(matmul_node_list, transpose_x1, transpose_x2) != GRAPH_SUCCESS) {
        continue;
      }

      GNode pack_0_node;
      GNode pack_1_node;
      GNode bmm_node;
      if (ProcessPackNode(graph, matmul_node_list, pack_0_node, pack_1_node, root_name, index) != GRAPH_SUCCESS) {
        return false;
      }
      if (ProcessBatchMatmulNode(graph, pack_0_node, pack_1_node, bmm_node, root_name, index, transpose_x1,
                                 transpose_x2) != GRAPH_SUCCESS) {
        return false;
      }
      if (ProcessSplitAndSqueezeNode(graph, matmul_node_list, bmm_node, root_name, index) != GRAPH_SUCCESS) {
        return false;
      }

      index++;
      affect_count++;
    }
  }
  cout << "MmoeBmmSplitPass: merged " << affect_count << " groups." << endl;
  return true;
}
}  // namespace

graphStatus MmoeBmmSplitPass(GraphPtr &graph, CustomPassContext &custom_context) {
  cout << "MmoeBmmSplitPass begin." << endl;
  map<string, map<int32_t, vector<GNode>>> matmul_node_map;
  FindSameLevelMatmulNode(graph, matmul_node_map);
  if (!ReplaceMatmul(graph, matmul_node_map)) {
    cout << "MmoeBmmSplitPass failed." << endl;
    return GRAPH_FAILED;
  }
  cout << "MmoeBmmSplitPass end." << endl;
  return GRAPH_SUCCESS;
}

REGISTER_CUSTOM_PASS("MmoeBmmSplitPass").CustomPassFn(MmoeBmmSplitPass).Stage(CustomPassStage::kAfterInferShape);
