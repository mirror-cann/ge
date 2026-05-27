/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/checker.h"
#include "common/types.h"
#include "graph/compute_graph.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"

namespace ge {
namespace {

/**
 * TopKV2 算子的符号化shape推导
 * 【算子功能】在指定维度上找到前K个最大元素及其索引
 *            输入：x(输入张量)、k(要选择的元素个数)
 *            输出：values(前K个值)、indices(前K个值的索引)
 * 【算子约束】
 *      1. k必须是标量，且k <= 指定维度的大小
 *      2. dim参数指定操作维度，范围为[-dim_num, dim_num)
 * 【推导逻辑】
 *      1. 获取输入x的形状和k的值
 *      2. 获取并处理dim属性，转换为正数索引
 *      3. 校验k的值不能大于指定维度的大小
 *      4. 构建输出形状：将操作维度替换为k，其他维度保持不变
 *      举例：输入x_shape=(2,5,4)，k=3，dim=1，则values_shape=(2,3,4)，indices_shape=(2,3,4)
 */
graphStatus InferShape4TopKV2(gert::InferSymbolShapeContext *context) {
  // 获取输入形状
  auto shape_x = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(shape_x);
  auto dim_num_x = shape_x->GetDimNum();
  
  // 获取k的值
  auto k_tensor = context->GetInputSymbolTensor(1);
  GE_UNSUPPORTED_IF_NULL(k_tensor);
  auto k_value = k_tensor->GetSymbolicValue();
  GE_UNSUPPORTED_IF_NULL(k_value);
  GE_ASSERT_TRUE(k_value->size() == 1, "k must be a scalar");
  auto k = (*k_value)[0];

  // 获取属性
  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto dim = attrs->GetAttrPointer<int64_t>(1);
  GE_ASSERT_NOTNULL(dim);
  
  // 确定操作维度
  int64_t actual_dim = *dim;
  if (actual_dim < 0) {
    actual_dim = dim_num_x + actual_dim;
  }
  GE_ASSERT_TRUE(actual_dim >= 0 && static_cast<size_t>(actual_dim) < dim_num_x, 
                 "dim %ld out of range for input with %zu dimensions", actual_dim, dim_num_x);
  
  // 校验k的值不能大于输入x在指定dim维度上的大小
  auto dim_size = shape_x->GetDim(static_cast<size_t>(actual_dim));
  GE_ASSERT_TRUE(k.Compare(dim_size) <= 0, "k must be less than or equal to dim_size");
  
  // 计算输出形状
  auto shape_values = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(shape_values);
  auto shape_indices = context->GetOutputSymbolShape(1);
  GE_ASSERT_NOTNULL(shape_indices);
  
  // 构建输出形状
  for (size_t i = 0; i < dim_num_x; ++i) {
    if (i == static_cast<size_t>(actual_dim)) {
      // 替换为k值
      shape_values->AppendDim(k);
      shape_indices->AppendDim(k);
    } else {
      // 保持原维度
      shape_values->AppendDim(shape_x->GetDim(i));
      shape_indices->AppendDim(shape_x->GetDim(i));
    }
  }
  
  return GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(TopKV2).InferSymbolShape(InferShape4TopKV2);

}  // namespace
}  // namespace ge