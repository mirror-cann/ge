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
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>
#include "register_custom_pass.h"
#include "all_ops.h"

using namespace std;
using namespace ge;

namespace {
constexpr const char *kMatMulV2 = "MatMulV2";
constexpr const char *kMatMulV3 = "MatMulV3";
constexpr const char *kBatchMatMulV2 = "BatchMatMulV2";
constexpr const char *kConst = "Const";
constexpr const char *kReshape = "Reshape";
constexpr const char *kSqueeze = "Squeeze";
constexpr const char *kTransposeX1 = "transpose_x1";
constexpr const char *kTransposeX2 = "transpose_x2";
constexpr int32_t kIndex0 = 0;
constexpr int32_t kIndex1 = 1;
constexpr int32_t kIndex2 = 2;
constexpr int32_t kWeightDimSize = 2;
constexpr int32_t kMinGroupSizeForMerge = 2;

bool FindConstIdx(const GNode &matmul, int32_t &const_idx) {
  for (int32_t i = kIndex0; i <= kIndex1; i++) {
    auto [in_node, _] = matmul.GetInDataNodesAndPortIndexs(i);
    if (in_node == nullptr) {
      continue;
    }
    AscendString type;
    if (in_node->GetType(type) != GRAPH_SUCCESS) {
      continue;
    }
    if (type == kConst) {
      const_idx = i;
      return true;
    }
  }
  return false;
}

bool FindRootNode(const GNode &matmul, int32_t data_idx, GNodePtr &root, int32_t &root_output_idx) {
  auto [cur, cur_port] = matmul.GetInDataNodesAndPortIndexs(data_idx);
  if (cur == nullptr) {
    return false;
  }
  constexpr int32_t kMaxDepth = 100;
  for (int32_t depth = 0; depth < kMaxDepth; depth++) {
    AscendString type;
    if (cur->GetType(type) != GRAPH_SUCCESS) {
      return false;
    }
    if (type == kReshape || type == kSqueeze) {
      auto outs = cur->GetOutDataNodesAndPortIndexs(kIndex0);
      if (outs.size() != 1) {
        root = cur;
        root_output_idx = cur_port;
        return true;
      }
      auto [parent, parent_port] = cur->GetInDataNodesAndPortIndexs(kIndex0);
      if (parent == nullptr) {
        return false;
      }
      cur = parent;
      cur_port = parent_port;
    } else {
      root = cur;
      root_output_idx = cur_port;
      return true;
    }
  }
  return false;
}

bool CheckMatch(const GNodePtr &matmul, int32_t base_const_idx, bool base_tx1, bool base_tx2,
                const vector<int64_t> &base_data_dims, int64_t base_combined_dim) {
  if (matmul == nullptr) {
    return false;
  }
  int32_t const_idx = -1;
  if (!FindConstIdx(*matmul, const_idx)) {
    return false;
  }
  if (const_idx != base_const_idx) {
    return false;
  }
  bool tx1 = false;
  bool tx2 = false;
  matmul->GetAttr(kTransposeX1, tx1);
  matmul->GetAttr(kTransposeX2, tx2);
  if (tx1 != base_tx1 || tx2 != base_tx2) {
    return false;
  }
  int32_t data_idx = kIndex1 - const_idx;
  TensorDesc data_desc;
  if (matmul->GetInputDesc(data_idx, data_desc) != GRAPH_SUCCESS) {
    return false;
  }
  if (data_desc.GetShape().GetDims() != base_data_dims) {
    return false;
  }
  TensorDesc weight_desc;
  if (matmul->GetInputDesc(const_idx, weight_desc) != GRAPH_SUCCESS) {
    return false;
  }
  auto weight_dims = weight_desc.GetShape().GetDims();
  if (weight_dims.size() != kWeightDimSize) {
    return false;
  }
  if (weight_dims[const_idx] != base_combined_dim) {
    return false;
  }
  return true;
}

void FindMatmulNodes(const GNodePtr &root, int32_t root_output_idx, int32_t base_const_idx, bool base_tx1,
                     bool base_tx2, const vector<int64_t> &base_data_dims, int64_t base_combined_dim,
                     vector<GNode> &group) {
  if (root == nullptr) {
    return;
  }
  int32_t data_idx = kIndex1 - base_const_idx;
  queue<pair<GNodePtr, int32_t>> q;
  for (auto &[node, in_port] : root->GetOutDataNodesAndPortIndexs(root_output_idx)) {
    q.push({node, in_port});
  }
  while (!q.empty()) {
    auto [node, in_port] = q.front();
    q.pop();
    if (node == nullptr) {
      continue;
    }
    AscendString type;
    if (node->GetType(type) != GRAPH_SUCCESS) {
      continue;
    }
    if (type == kMatMulV2 || type == kMatMulV3 || type == kBatchMatMulV2) {
      if (in_port != data_idx) {
        continue;
      }
      if (!CheckMatch(node, base_const_idx, base_tx1, base_tx2, base_data_dims, base_combined_dim)) {
        continue;
      }
      group.push_back(*node);
    } else if (type == kReshape || type == kSqueeze) {
      if (node->GetOutputsSize() != 1) {
        continue;
      }
      auto outs = node->GetOutDataNodesAndPortIndexs(kIndex0);
      if (outs.size() != 1) {
        continue;
      }
      for (auto &[child, child_port] : outs) {
        q.push({child, child_port});
      }
    }
  }
}

void FindNodesCanFusion(const GraphPtr &graph, vector<GNode> &matmul_nodes) {
  auto all_nodes = graph->GetAllNodes();
  for (auto &node : all_nodes) {
    AscendString type;
    if (node.GetType(type) != GRAPH_SUCCESS) {
      continue;
    }
    if (type == kMatMulV2 || type == kMatMulV3 || type == kBatchMatMulV2) {
      matmul_nodes.push_back(node);
    }
  }
}

Operator concat_matrices(const vector<GNode> &group, int32_t concat_dim, int32_t const_idx, const string &name) {
  auto concat = op::ConcatV2D(name.c_str());
  int32_t n = static_cast<int32_t>(group.size());
  concat.create_dynamic_input_x(static_cast<uint32_t>(n));
  concat.set_attr_concat_dim(static_cast<int64_t>(concat_dim));
  concat.set_attr_N(static_cast<int64_t>(n));
  TensorDesc weight_desc;
  group[kIndex0].GetInputDesc(const_idx, weight_desc);
  auto dims = weight_desc.GetShape().GetDims();
  for (int32_t i = kIndex0; i < n; i++) {
    TensorDesc desc;
    group[i].GetInputDesc(const_idx, desc);
    concat.update_dynamic_input_desc_x(static_cast<uint32_t>(i), desc);
  }
  vector<int64_t> out_dims = dims;
  out_dims[concat_dim] *= static_cast<int64_t>(n);
  TensorDesc out_desc(Shape(out_dims), weight_desc.GetFormat(), weight_desc.GetDataType());
  out_desc.SetOriginShape(Shape(out_dims));
  out_desc.SetOriginFormat(weight_desc.GetFormat());
  concat.update_output_desc_y(out_desc);
  return concat;
}

struct MmNodeDescs {
  TensorDesc data_desc;
  TensorDesc weight_desc;
  TensorDesc out_desc;
  bool tx1 = false;
  bool tx2 = false;
};

bool GetBaseDescs(const vector<GNode> &group, int32_t data_idx, int32_t const_idx, MmNodeDescs &descs) {
  if (group[kIndex0].GetInputDesc(data_idx, descs.data_desc) != GRAPH_SUCCESS) {
    return false;
  }
  if (group[kIndex0].GetInputDesc(const_idx, descs.weight_desc) != GRAPH_SUCCESS) {
    return false;
  }
  if (group[kIndex0].GetOutputDesc(kIndex0, descs.out_desc) != GRAPH_SUCCESS) {
    return false;
  }
  group[kIndex0].GetAttr(kTransposeX1, descs.tx1);
  group[kIndex0].GetAttr(kTransposeX2, descs.tx2);
  return true;
}

void SetMatmulNodeAttrs(GNode &node_matmul, const MmNodeDescs &descs) {
  bool tx1_val = descs.tx1;
  bool tx2_val = descs.tx2;
  node_matmul.SetAttr("adj_x1", tx1_val);
  node_matmul.SetAttr("adj_x2", tx2_val);
  DataType dtype_t = descs.data_desc.GetDataType();
  node_matmul.SetAttr("T", dtype_t);
  int64_t offset_x_val = 0;
  node_matmul.SetAttr("offset_x", offset_x_val);
  vector<AscendString> ir_attr_names = {"adj_x1", "adj_x2", "offset_x"};
  node_matmul.SetAttr("_ir_attr_names", ir_attr_names);
  vector<AscendString> ir_inputs_key = {"x1", "x2", "bias", "offset_w"};
  node_matmul.SetAttr("_ir_inputs_key", ir_inputs_key);
  vector<int64_t> ir_inputs_value = {0, 0, 1, 1};
  node_matmul.SetAttr("_ir_inputs_value", ir_inputs_value);
  vector<AscendString> ir_outputs_key = {"y"};
  node_matmul.SetAttr("_ir_outputs_key", ir_outputs_key);
  vector<int64_t> ir_outputs_value = {0};
  node_matmul.SetAttr("_ir_outputs_value", ir_outputs_value);
  vector<AscendString> opt_input = {"bias", "offset_w"};
  node_matmul.SetAttr("_opt_input", opt_input);
  vector<AscendString> input_name_key = {"bias", "offset_w", "x1", "x2"};
  node_matmul.SetAttr("_input_name_key", input_name_key);
  vector<int64_t> input_name_value = {2, 3, 0, 1};
  node_matmul.SetAttr("_input_name_value", input_name_value);
  vector<AscendString> output_name_key = {"y"};
  node_matmul.SetAttr("_output_name_key", output_name_key);
  vector<int64_t> output_name_value = {0};
  node_matmul.SetAttr("_output_name_value", output_name_value);
}

GNode CreateMergedMatmul(const GraphPtr &graph, const vector<GNode> &group, int32_t const_idx, int32_t data_idx,
                         const MmNodeDescs &descs, const string &prefix) {
  bool transpose_const = (const_idx == kIndex0) ? descs.tx1 : descs.tx2;
  int32_t concat_dim = (transpose_const == (const_idx == kIndex0)) ? kIndex1 : kIndex0;
  int32_t n = static_cast<int32_t>(group.size());

  auto w_dims = descs.weight_desc.GetShape().GetDims();
  w_dims[concat_dim] *= static_cast<int64_t>(n);
  TensorDesc combined_w_desc(Shape(w_dims), descs.weight_desc.GetFormat(), descs.weight_desc.GetDataType());
  combined_w_desc.SetOriginShape(Shape(w_dims));
  combined_w_desc.SetOriginFormat(descs.weight_desc.GetFormat());

  auto out_dims = descs.out_desc.GetShape().GetDims();
  out_dims[const_idx] *= static_cast<int64_t>(n);
  TensorDesc new_out_desc(Shape(out_dims), descs.out_desc.GetFormat(), descs.out_desc.GetDataType());
  new_out_desc.SetOriginShape(Shape(out_dims));
  new_out_desc.SetOriginFormat(descs.out_desc.GetFormat());

  auto matmul_op = op::BatchMatMulV2((prefix + "merged_matmul").c_str());
  matmul_op.set_attr_adj_x1(descs.tx1);
  matmul_op.set_attr_adj_x2(descs.tx2);
  if (const_idx == kIndex1) {
    matmul_op.update_input_desc_x1(descs.data_desc);
    matmul_op.update_input_desc_x2(combined_w_desc);
  } else {
    matmul_op.update_input_desc_x1(combined_w_desc);
    matmul_op.update_input_desc_x2(descs.data_desc);
  }
  matmul_op.update_output_desc_y(new_out_desc);
  GNode node_matmul = graph->AddNodeByOp(matmul_op);
  SetMatmulNodeAttrs(node_matmul, descs);
  node_matmul.UpdateInputDesc(kIndex0, (const_idx == kIndex1) ? descs.data_desc : combined_w_desc);
  node_matmul.UpdateInputDesc(kIndex1, (const_idx == kIndex1) ? combined_w_desc : descs.data_desc);
  node_matmul.UpdateOutputDesc(kIndex0, new_out_desc);
  return node_matmul;
}

GNode CreateSplitVNode(const GraphPtr &graph, int32_t n, int32_t const_idx, int64_t combined_dim,
                       const MmNodeDescs &descs, const TensorDesc &new_out_desc, const string &prefix) {
  vector<int32_t> ss_vec(n, static_cast<int32_t>(combined_dim));
  TensorDesc ss_desc(Shape({static_cast<int64_t>(n)}), FORMAT_ND, DT_INT32);
  Tensor ss_tensor(ss_desc, reinterpret_cast<const uint8_t *>(ss_vec.data()), static_cast<size_t>(n) * sizeof(int32_t));
  auto ss_op = op::Const((prefix + "size_splits").c_str()).set_attr_value(ss_tensor);
  GNode node_ss = graph->AddNodeByOp(ss_op);

  int32_t sd_val = const_idx;
  TensorDesc sd_desc(Shape({1}), FORMAT_ND, DT_INT32);
  sd_desc.SetOriginShape(Shape({1}));
  sd_desc.SetOriginFormat(FORMAT_ND);
  Tensor sd_tensor(sd_desc, reinterpret_cast<const uint8_t *>(&sd_val), sizeof(int32_t));
  auto sd_op = op::Const((prefix + "split_dim").c_str()).set_attr_value(sd_tensor);
  GNode node_sd = graph->AddNodeByOp(sd_op);

  auto splitv_op = op::SplitV((prefix + "splitv").c_str());
  splitv_op.set_attr_num_split(static_cast<int64_t>(n));
  splitv_op.create_dynamic_output_y(static_cast<uint32_t>(n));
  splitv_op.update_input_desc_x(new_out_desc);
  splitv_op.update_input_desc_size_splits(ss_desc);
  splitv_op.update_input_desc_split_dim(sd_desc);
  TensorDesc split_out_desc(descs.out_desc);
  split_out_desc.SetOriginShape(descs.out_desc.GetShape());
  split_out_desc.SetOriginFormat(descs.out_desc.GetFormat());
  for (int32_t i = kIndex0; i < n; i++) {
    splitv_op.update_dynamic_output_desc_y(static_cast<uint32_t>(i), split_out_desc);
  }
  GNode node_splitv = graph->AddNodeByOp(splitv_op);
  int64_t num_split_val = static_cast<int64_t>(n);
  node_splitv.SetAttr("num_split", num_split_val);
  vector<AscendString> split_ir_attr_names = {"num_split"};
  node_splitv.SetAttr("_ir_attr_names", split_ir_attr_names);
  vector<AscendString> split_ir_inputs_key = {"x", "size_splits", "split_dim"};
  node_splitv.SetAttr("_ir_inputs_key", split_ir_inputs_key);
  vector<int64_t> split_ir_inputs_value = {0, 0, 0};
  node_splitv.SetAttr("_ir_inputs_value", split_ir_inputs_value);
  vector<AscendString> split_ir_outputs_key = {"y"};
  node_splitv.SetAttr("_ir_outputs_key", split_ir_outputs_key);
  vector<int64_t> split_ir_outputs_value = {0};
  node_splitv.SetAttr("_ir_outputs_value", split_ir_outputs_value);
  vector<AscendString> split_input_name_key = {"x", "size_splits", "split_dim"};
  node_splitv.SetAttr("_input_name_key", split_input_name_key);
  vector<int64_t> split_input_name_value = {0, 1, 2};
  node_splitv.SetAttr("_input_name_value", split_input_name_value);
  vector<AscendString> split_output_name_key = {"y"};
  node_splitv.SetAttr("_output_name_key", split_output_name_key);
  vector<int64_t> split_output_name_value = {0};
  node_splitv.SetAttr("_output_name_value", split_output_name_value);

  graph->AddDataEdge(node_ss, kIndex0, node_splitv, kIndex1);
  graph->AddDataEdge(node_sd, kIndex0, node_splitv, kIndex2);
  return node_splitv;
}

void RemoveOldMatmulNodes(const GraphPtr &graph, vector<GNode> &group) {
  for (auto &matmul : group) {
    vector<pair<GNodePtr, int32_t>> in_edges;
    for (size_t j = 0; j < matmul.GetInputsSize(); j++) {
      auto [in_node, in_port] = matmul.GetInDataNodesAndPortIndexs(static_cast<int32_t>(j));
      in_edges.push_back({in_node, in_port});
    }
    for (size_t j = 0; j < in_edges.size(); j++) {
      if (in_edges[j].first != nullptr) {
        graph->RemoveEdge(*in_edges[j].first, in_edges[j].second, matmul, static_cast<int32_t>(j));
      }
    }
    graph->RemoveNode(matmul);
  }
}

GNode CreateConcatNode(const GraphPtr &graph, const vector<GNode> &group, int32_t concat_dim, int32_t const_idx,
                       const string &prefix) {
  int32_t n = static_cast<int32_t>(group.size());
  auto concat_op = concat_matrices(group, concat_dim, const_idx, prefix + "concat");
  GNode node_concat = graph->AddNodeByOp(concat_op);
  for (int32_t i = kIndex0; i < n; i++) {
    auto [w_node, w_port] = group[i].GetInDataNodesAndPortIndexs(const_idx);
    if (w_node == nullptr) {
      continue;
    }
    graph->AddDataEdge(*w_node, w_port, node_concat, i);
  }
  return node_concat;
}

void RewireDownstreamToSplitV(const GraphPtr &graph, vector<GNode> &group, GNode &node_splitv) {
  int32_t n = static_cast<int32_t>(group.size());
  for (int32_t i = kIndex0; i < n; i++) {
    auto outs = group[i].GetOutDataNodesAndPortIndexs(kIndex0);
    for (auto &[out_node, out_port] : outs) {
      if (out_node == nullptr) {
        continue;
      }
      graph->RemoveEdge(group[i], kIndex0, *out_node, out_port);
      graph->AddDataEdge(node_splitv, i, *out_node, out_port);
    }
  }
}

bool ReplaceMmNodes(const GraphPtr &graph, vector<GNode> &group, int32_t const_idx, int32_t data_idx) {
  int32_t n = static_cast<int32_t>(group.size());
  AscendString base_name;
  if (group[kIndex0].GetName(base_name) != GRAPH_SUCCESS) {
    return false;
  }
  string prefix = string(base_name.GetString()) + "_mmoe_";

  MmNodeDescs descs;
  if (!GetBaseDescs(group, data_idx, const_idx, descs)) {
    return false;
  }

  bool transpose_const = (const_idx == kIndex0) ? descs.tx1 : descs.tx2;
  int64_t combined_dim;
  if (transpose_const == (const_idx == kIndex0)) {
    combined_dim = descs.weight_desc.GetShape().GetDim(kIndex1);
  } else {
    combined_dim = descs.weight_desc.GetShape().GetDim(kIndex0);
  }
  if (combined_dim <= 0) {
    return false;
  }

  int32_t concat_dim = (transpose_const == (const_idx == kIndex0)) ? kIndex1 : kIndex0;
  GNode node_concat = CreateConcatNode(graph, group, concat_dim, const_idx, prefix);

  GNode node_matmul = CreateMergedMatmul(graph, group, const_idx, data_idx, descs, prefix);

  auto [data_node, data_port] = group[kIndex0].GetInDataNodesAndPortIndexs(data_idx);
  if (data_node == nullptr) {
    return false;
  }
  graph->AddDataEdge(*data_node, data_port, node_matmul, data_idx);
  graph->AddDataEdge(node_concat, kIndex0, node_matmul, const_idx);

  TensorDesc new_out_desc;
  node_matmul.GetOutputDesc(kIndex0, new_out_desc);
  GNode node_splitv = CreateSplitVNode(graph, n, const_idx, combined_dim, descs, new_out_desc, prefix);
  graph->AddDataEdge(node_matmul, kIndex0, node_splitv, kIndex0);

  RewireDownstreamToSplitV(graph, group, node_splitv);
  RemoveOldMatmulNodes(graph, group);
  cout << "MmoeMatmulPass: merged " << n << " MatMul nodes." << endl;
  return true;
}
}  // namespace

bool TryMergeFromBase(const GraphPtr &graph, const GNode &base, unordered_set<string> &merged_names) {
  int32_t const_idx = -1;
  if (!FindConstIdx(base, const_idx)) {
    return false;
  }
  int32_t data_idx = kIndex1 - const_idx;

  bool base_tx1 = false;
  bool base_tx2 = false;
  base.GetAttr(kTransposeX1, base_tx1);
  base.GetAttr(kTransposeX2, base_tx2);

  TensorDesc base_data_desc;
  if (base.GetInputDesc(data_idx, base_data_desc) != GRAPH_SUCCESS) {
    return false;
  }
  vector<int64_t> base_data_dims = base_data_desc.GetShape().GetDims();

  TensorDesc base_weight_desc;
  if (base.GetInputDesc(const_idx, base_weight_desc) != GRAPH_SUCCESS) {
    return false;
  }
  auto weight_dims = base_weight_desc.GetShape().GetDims();
  if (weight_dims.size() != kWeightDimSize) {
    return false;
  }
  int64_t base_combined_dim = weight_dims[const_idx];
  if (base_combined_dim <= 0) {
    return false;
  }

  GNodePtr root;
  int32_t root_output_idx = -1;
  if (!FindRootNode(base, data_idx, root, root_output_idx) || root == nullptr) {
    return false;
  }

  vector<GNode> group;
  FindMatmulNodes(root, root_output_idx, const_idx, base_tx1, base_tx2, base_data_dims, base_combined_dim, group);
  if (group.size() < kMinGroupSizeForMerge) {
    return false;
  }

  cout << "MmoeMatmulPass: trying to merge " << group.size() << " nodes from root." << endl;
  if (!ReplaceMmNodes(graph, group, const_idx, data_idx)) {
    cout << "MmoeMatmulPass: merge failed, skipping." << endl;
    return false;
  }

  for (auto &node : group) {
    AscendString name;
    if (node.GetName(name) == GRAPH_SUCCESS) {
      merged_names.insert(string(name.GetString()));
    }
  }
  return true;
}

graphStatus MmoeMatmulPass(GraphPtr &graph, CustomPassContext &custom_context) {
  cout << "MmoeMatmulPass begin." << endl;
  vector<GNode> matmul_nodes;
  FindNodesCanFusion(graph, matmul_nodes);
  cout << "MmoeMatmulPass: found " << matmul_nodes.size() << " MatMul nodes." << endl;

  unordered_set<string> merged_names;
  for (auto &base : matmul_nodes) {
    AscendString base_name;
    if (base.GetName(base_name) != GRAPH_SUCCESS) {
      continue;
    }
    if (merged_names.count(string(base_name.GetString())) > 0) {
      continue;
    }
    TryMergeFromBase(graph, base, merged_names);
  }

  cout << "MmoeMatmulPass end." << endl;
  return GRAPH_SUCCESS;
}

REGISTER_CUSTOM_PASS("MmoeMatmulPass").CustomPassFn(MmoeMatmulPass).Stage(CustomPassStage::kAfterInferShape);
