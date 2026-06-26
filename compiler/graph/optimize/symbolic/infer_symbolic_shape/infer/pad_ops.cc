/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <cstdlib>
#include "graph/compute_graph.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"
#include "common/checker.h"
#include "common/framework_types_internal.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"

namespace ge {
namespace {
// 要求paddings的长度是input_dim的两倍
constexpr size_t kPadSizeParam = 2U;
constexpr size_t PAIR = 2UL;

/**
 * Pad算子 算子的符号化shape推导
 * 【算子功能】对张量进行填充
 * 【算子约束】
 *         1. paddings入参是一个整数张量，形状为[n, 2]，其中n是输入tensor的秩
 * 【推导逻辑】
 *         1.输出shape等于输入shape + 在tensor前填充的数量 + 在tensor后填充的数量
 * 【举例】
 *      in_shape = [2, 3, 2]
 *      paddings = [[1, 2], [2, 1], [3, 3]]
 *      out_shape = [1+2+2, 2+3+1, 3+2+3]
 */
graphStatus InferShape4Pad(gert::InferSymbolShapeContext *context) {
  const auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);

  // paddings不能为空
  const auto paddings_tensor = context->GetInputSymbolTensor(1);
  GE_UNSUPPORTED_IF_NULL(paddings_tensor);
  if (paddings_tensor->GetSymbolicValue() == nullptr) {
    GELOGW("Symbol Infer unsupported, get symbolic value is nullptr, node %s[%s]", context->GetNodeName(),
           context->GetNodeType());
    return UNSUPPORTED;
  }

  const auto paddings_size = paddings_tensor->GetSymbolicValue()->size();
  GE_ASSERT(paddings_size > 0, "Invalid paddings, must be non-empty!");

  GE_ASSERT(paddings_size == in_shape->GetDimNum() * kPadSizeParam,
            "Paddings failed, padding size[%u] must be twice of the input rank[%u].", paddings_size,
            in_shape->GetDimNum());

  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);

  out_shape->Clear();
  auto paddings = *paddings_tensor->GetSymbolicValue();
  for (size_t i = 0; i < in_shape->GetDimNum(); ++i) {
    const auto dim = in_shape->GetDim(i) + paddings[kPadSizeParam * i] + paddings[kPadSizeParam * i + 1];
    out_shape->AppendDim(dim);
  }

  return GRAPH_SUCCESS;
}

graphStatus InferShape4PadD(gert::InferSymbolShapeContext *context) {
  const auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);
  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);

  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const gert::ContinuousVectorVector *cvv_padding = nullptr;
  cvv_padding = attrs->GetListListInt(0);
  GE_ASSERT_NOTNULL(cvv_padding);

  out_shape->Clear();
  GE_ASSERT_EQ(cvv_padding->GetSize(), in_shape->GetDimNum());
  for (size_t i = 0U; i < cvv_padding->GetSize(); ++i) {
    const auto cv_padding = cvv_padding->Get(i);
    GE_ASSERT_NOTNULL(cv_padding);
    GE_ASSERT_EQ(cv_padding->GetSize(), 2U);  // 如果不为2是异常场景
    const int64_t *data = static_cast<const int64_t *>(cv_padding->GetData());
    GE_ASSERT_NOTNULL(data);
    const auto dim0 = *(data + 0U);
    const auto dim1 = *(data + 1U);
    auto sym0 = Symbol(dim0);
    auto sym1 = Symbol(dim1);
    const auto dim = in_shape->GetDim(i) + sym0 + sym1;
    out_shape->AppendDim(dim);
  }
  return GRAPH_SUCCESS;
}

graphStatus PadV3InferShape(const gert::InferSymbolShapeContext *context, const gert::SymbolShape *x_shape,
                            const gert::SymbolTensor *paddings_tensor, gert::SymbolShape *y_shape) {
  const auto paddings_value = paddings_tensor->GetSymbolicValue();
  GE_ASSERT_NOTNULL(paddings_value);
  const auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto paddings_contiguous = attrs->GetAttrPointer<bool>(1);
  GE_ASSERT_NOTNULL(paddings_contiguous);
  const size_t input_dim_size = x_shape->GetDimNum();
  GE_ASSERT(input_dim_size != 0UL, "input shape cannot empty");
  const auto paddings_size = paddings_tensor->GetSymbolicValue()->size();
  GE_ASSERT(paddings_size > 0UL, "Invalid paddings, must be non-empty!");

  const auto &paddings_num = paddings_tensor->GetOriginSymbolShape().GetSymbolShapeSize();
  ASSERT_SYMBOL_EQ(paddings_num, Symbol(input_dim_size * PAIR));
  size_t index_cof = 1UL;
  size_t index_offset = input_dim_size;
  if (*paddings_contiguous) {
    index_cof = PAIR;
    index_offset = 1UL;
  }
  for (size_t i = 0UL; i < input_dim_size; ++i) {
    auto pad_front = paddings_value->at(index_cof * i);
    auto pad_end = paddings_value->at(index_cof * i + index_offset);
    y_shape->AppendDim(x_shape->GetDim(i) + pad_front + pad_end);
  }
  return GRAPH_SUCCESS;
}

graphStatus InferShape4PadV3(gert::InferSymbolShapeContext *context) {
  const auto x_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(x_shape);
  auto y_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(y_shape);
  const auto paddings_tensor = context->GetInputSymbolTensor(1);
  GE_UNSUPPORTED_IF_NULL(paddings_tensor);
  const auto paddings_desc = context->GetInputDesc(1);
  GE_ASSERT_NOTNULL(paddings_desc);
  const auto paddings_dtype = paddings_desc->GetDataType();
  GE_ASSERT(paddings_dtype == DT_INT32 || paddings_dtype == DT_INT64,
            "paddings data type must is int32 or int64, it is %d", paddings_dtype);
  return PadV3InferShape(context, x_shape, paddings_tensor, y_shape);
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(Pad).InferSymbolShape(InferShape4Pad);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(PadD).InferSymbolShape(InferShape4PadD);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(PadV3).InferSymbolShape(InferShape4PadV3);
}  // namespace
}  // namespace ge
