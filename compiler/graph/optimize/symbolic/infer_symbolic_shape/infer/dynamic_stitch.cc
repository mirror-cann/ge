/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under terms and conditionsverions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to License for details. You may not use this file except in compliance with License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "graph/compute_graph.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"
#include "common/checker.h"
#include "common/framework_types_internal.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"

namespace ge {
namespace {
// DynamicStitch: 根据indices将多个输入张量x拼接到输出张量的指定位置
// 符号化shape推导逻辑：
// 1. 输出shape第一维为所有indices中的最大值+1
// 2. 输出shape其余维度与输入x的shape除去indices维度后的部分相同
// 3. 验证所有输入x的shape除去indices维度后的部分一致
graphStatus InferShapeForDynamicStitch(gert::InferSymbolShapeContext *context) {
  GE_ASSERT_NOTNULL(context);
  GE_UNSUPPORTED_IF_NULL(context->GetInputSymbolShape(0));
  const auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const int32_t *attr_n = attrs->GetAttrPointer<int32_t>(0);
  GE_ASSERT_TRUE(attr_n != nullptr && *attr_n >= 1);
  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  auto &out_shape_dims = out_shape->MutableDims();
  out_shape_dims.clear();

  for (int32_t i = 0U; i < *attr_n; ++i) {
    const auto indices_shape = context->GetDynamicInputSymbolShape(0, i);
    GE_UNSUPPORTED_IF_NULL(indices_shape);
    const auto indices_dims = indices_shape->GetDims();
    const auto x_shape = context->GetDynamicInputSymbolShape(1, i);
    GE_UNSUPPORTED_IF_NULL(x_shape);
    const auto x_dims = x_shape->GetDims();
    GE_ASSERT_TRUE(x_dims.size() >= indices_dims.size(), "Op %s: x rank %zu is smaller than indices rank %zu.",
                   context->GetNodeName(), x_dims.size(), indices_dims.size());
    GE_ASSERT_TRUE(std::equal(indices_dims.begin(), indices_dims.end(), x_dims.begin()),
                   "Op %s: x dims and indices dims are corrupt at input index %zu.", context->GetNodeName(), i);
    if (i == 0U) {
      out_shape_dims.push_back(kSymbolZero);
      out_shape_dims.insert(out_shape_dims.end(), x_dims.begin() + indices_dims.size(), x_dims.end());
    } else {
      GE_ASSERT_TRUE(std::equal(x_dims.begin() + indices_dims.size(), x_dims.end(), out_shape_dims.begin() + 1),
                     "Op %s: the constant dims is inconstant at input index %zu.", context->GetNodeName(), i);
    }

    const auto indices_tensor = context->GetDynamicInputSymbolTensor(0, i);
    GE_UNSUPPORTED_IF_NULL(indices_tensor);
    const auto indices_tensor_val = indices_tensor->GetSymbolicValue();
    GE_UNSUPPORTED_IF_NULL(indices_tensor_val);
    for (const auto &tensor : *indices_tensor_val) {
      out_shape_dims[0] = sym::Max(out_shape_dims[0], tensor);
    }
  }

  out_shape_dims[0] = out_shape_dims[0] + kSymbolOne;
  return ge::GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(DynamicStitch).InferSymbolShape(InferShapeForDynamicStitch);
}  // namespace
}  // namespace ge
