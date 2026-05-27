/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/compute_graph.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"
#include "common/checker.h"
#include "common/types.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"

namespace ge {
namespace {

constexpr size_t IDX_X = 0;
constexpr size_t IDX_SIZES = 1;
constexpr size_t IDX_STRIDES = 2;
constexpr size_t IDX_INDICES_START = 3;
constexpr size_t IDX_Y = 0;

bool AreIndicesContiguous(const std::vector<Expression> &sizes_value) {
  if (sizes_value.empty()) {
    return true;
  }

  int64_t first = -1L;
  int64_t last = -1L;
  for (size_t i = 0; i < sizes_value.size(); ++i) {
    int64_t size_val = 0;
    if (sizes_value[i].GetConstValue(size_val) && size_val == 1) {
      if (first == -1LL) {
        first = static_cast<int64_t>(i);
      }
      last = static_cast<int64_t>(i);
    }
  }

  if (first == -1LL) {
    return true;
  }

  for (int64_t i = first; i <= last; ++i) {
    int64_t size_val = 0;
    if (!sizes_value[i].GetConstValue(size_val) || size_val != 1) {
      return false;
    }
  }
  return true;
}

graphStatus CollectIndicesShapes(gert::InferSymbolShapeContext *context, size_t x_dim_num,
                                 const std::vector<Expression> &sizes_value,
                                 std::vector<gert::SymbolShape> &indices_shape_list) {
  int64_t indexed_sizes_num = sizes_value.size();
  int indices_idx = IDX_INDICES_START;

  for (int64_t i = 0; i < indexed_sizes_num; i++) {
    if (i >= static_cast<int64_t>(x_dim_num)) {
      GELOGW("sizes index %ld out of range for input with %zu dimensions. node %s[%s]", i, x_dim_num,
             context->GetNodeName(), context->GetNodeType());
      break;
    }

    int64_t size_val = 0;
    if (!sizes_value[i].GetConstValue(size_val)) {
      GELOGW("Symbol infer unsupported, sizes contains non-constant value. node %s[%s]", context->GetNodeName(),
             context->GetNodeType());
      return UNSUPPORTED;
    }
    if (size_val == 1) {
      auto indices_shape = context->GetInputSymbolShape(indices_idx);
      indices_idx++;
      GE_UNSUPPORTED_IF_NULL(indices_shape);
      indices_shape_list.push_back(*indices_shape);
    }
  }
  return GRAPH_SUCCESS;
}

graphStatus HandleNoIndicesCase(gert::InferSymbolShapeContext *context, const gert::SymbolShape &x_shape) {
  auto output_shape = context->GetOutputSymbolShape(IDX_Y);
  GE_ASSERT_NOTNULL(output_shape);
  *output_shape = x_shape;
  return GRAPH_SUCCESS;
}

graphStatus ComputeBroadcastShape(const std::vector<gert::SymbolShape> &indices_shape_list,
                                  std::vector<Expression> &broadcast_dims) {
  std::vector<std::vector<Expression>> indices_dims_list;
  for (const auto &shape : indices_shape_list) {
    indices_dims_list.push_back(shape.GetDims());
  }

  if (SymbolicInferUtil::Broadcast(indices_dims_list, broadcast_dims) != SUCCESS) {
    return GRAPH_FAILED;
  }
  return GRAPH_SUCCESS;
}

bool IsIndexedDimension(const std::vector<Expression> &sizes_value, size_t i) {
  if (i >= sizes_value.size()) {
    return false;
  }
  int64_t size_val = 0;
  return sizes_value[i].GetConstValue(size_val) && size_val == 1;
}

graphStatus ComputeOutputShape(gert::InferSymbolShapeContext *context, const gert::SymbolShape &x_shape,
                               const std::vector<Expression> &sizes_value,
                               const std::vector<Expression> &broadcast_dims) {
  auto output_shape = context->GetOutputSymbolShape(IDX_Y);
  GE_ASSERT_NOTNULL(output_shape);
  output_shape->Clear();

  size_t x_dim_num = x_shape.GetDimNum();
  bool indices_contiguous = AreIndicesContiguous(sizes_value);
  GELOGD("ComputeOutputShape sizes_value are contiguous: %d. node %s[%s]", indices_contiguous, context->GetNodeName(),
         context->GetNodeType());

  if (!indices_contiguous) {
    // 情况1：索引维度不连续，先添加广播形状，然后添加非索引维度
    output_shape->MutableDims() = broadcast_dims;
    for (size_t i = 0; i < x_dim_num; i++) {
      if (!IsIndexedDimension(sizes_value, i)) {
        output_shape->AppendDim(x_shape.GetDim(i));
      }
    }
    return GRAPH_SUCCESS;
  }

  // 情况2：索引维度连续，在遇到第一个索引维度时添加广播形状，然后跳过所有连续的索引维度
  bool replaced = false;
  for (size_t i = 0; i < x_dim_num; i++) {
    if (IsIndexedDimension(sizes_value, i) && !replaced) {
      // 插入广播形状
      for (const auto &dim : broadcast_dims) {
        output_shape->AppendDim(dim);
      }
      replaced = true;

      // 跳过所有连续的索引维度
      while (i + 1 < x_dim_num && IsIndexedDimension(sizes_value, i + 1)) {
        i++;
      }
    } else {
      output_shape->AppendDim(x_shape.GetDim(i));
    }
  }
  return GRAPH_SUCCESS;
}

/**
 * Index 算子的符号化shape推导
 * 【算子功能】根据索引从输入张量中提取元素，支持多维索引和广播
 *            输入：x(输入张量)、sizes(索引维度标识)、strides(步长)、indices(索引张量列表)
 *            输出：y(输出张量)
 * 【算子约束】
 *      1. sizes张量中值为1的位置表示对应维度需要索引，非1表示保持原维度
 *      2. 索引张量需要进行广播统一形状
 *      3. 支持索引维度连续和不连续两种情况
 * 【推导逻辑】
 *      1. 获取输入x的形状和sizes张量的值
 *      2. 收集所有需要索引的维度对应的索引张量形状
 *      3. 若无索引张量，输出形状等于输入形状
 *      4. 计算所有索引张量的广播形状
 *      5. 根据索引维度是否连续，计算输出形状：
 *         - 索引不连续：先添加广播形状，再添加非索引维度
 *         - 索引连续：在第一个索引维度位置插入广播形状，跳过所有连续索引维度
 *      举例：输入x_shape=(2,3,4,5)，sizes=(1,0,1,0)，indices_shape=(2,3)和(3,)，
 *            广播后=(2,3)，输出=(2,3,3,5)
 */
graphStatus InferShape4Index(gert::InferSymbolShapeContext *context) {
  GELOGD("index symbol infershape is begin");

  // Get input x shape
  auto x_shape = context->GetInputSymbolShape(IDX_X);
  GE_UNSUPPORTED_IF_NULL(x_shape);
  size_t x_dim_num = x_shape->GetDimNum();

  // Get sizes input
  auto sizes_tensor = context->GetInputSymbolTensor(IDX_SIZES);
  GE_UNSUPPORTED_IF_NULL(sizes_tensor);
  auto sizes_value = sizes_tensor->GetSymbolicValue();
  GE_UNSUPPORTED_IF_NULL(sizes_value);

  // Collect indices shapes
  std::vector<gert::SymbolShape> indices_shape_list;
  auto ret = CollectIndicesShapes(context, x_dim_num, *sizes_value, indices_shape_list);
  if (ret != GRAPH_SUCCESS) {
    GELOGE(ret, "Failed to collect indices shapes. node %s[%s]", context->GetNodeName(), context->GetNodeType());
    return ret;
  }

  // No indices, output shape is same as input shape
  if (indices_shape_list.empty()) {
    if (HandleNoIndicesCase(context, *x_shape) != GRAPH_SUCCESS) {
      return GRAPH_FAILED;
    }
    GELOGD("index symbol infershape is end. node %s[%s]", context->GetNodeName(), context->GetNodeType());
    return GRAPH_SUCCESS;
  }

  // Compute broadcast shape
  std::vector<Expression> broadcast_dims;
  if (ComputeBroadcastShape(indices_shape_list, broadcast_dims) != GRAPH_SUCCESS) {
    GELOGE(GRAPH_FAILED, "Failed to compute broadcast shape. node %s[%s]", context->GetNodeName(),
           context->GetNodeType());
    return GRAPH_FAILED;
  }

  // Compute output shape
  ComputeOutputShape(context, *x_shape, *sizes_value, broadcast_dims);

  GELOGD("index symbol infershape is end. node %s[%s]", context->GetNodeName(), context->GetNodeType());
  return GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(Index).InferSymbolShape(InferShape4Index);

}  // namespace
}  // namespace ge
