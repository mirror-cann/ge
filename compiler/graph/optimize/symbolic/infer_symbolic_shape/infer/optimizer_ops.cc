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

graphStatus InferShape4ApplyAdagradD(gert::InferSymbolShapeContext *context) {
  auto var_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(var_shape);
  auto accum_shape = context->GetInputSymbolShape(1);
  GE_UNSUPPORTED_IF_NULL(accum_shape);

  auto varout_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(varout_shape);
  auto accumout_shape = context->GetOutputSymbolShape(1);
  GE_ASSERT_NOTNULL(accumout_shape);

  *varout_shape = *var_shape;
  *accumout_shape = *accum_shape;
  return ge::GRAPH_SUCCESS;
}

graphStatus InferShape4ApplyAdamD(gert::InferSymbolShapeContext *context) {
  auto var_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(var_shape);
  auto m_shape = context->GetInputSymbolShape(1);
  GE_UNSUPPORTED_IF_NULL(m_shape);
  auto v_shape = context->GetInputSymbolShape(2);
  GE_UNSUPPORTED_IF_NULL(v_shape);

  auto varout_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(varout_shape);
  auto mout_shape = context->GetOutputSymbolShape(1);
  GE_ASSERT_NOTNULL(mout_shape);
  auto vout_shape = context->GetOutputSymbolShape(2);
  GE_ASSERT_NOTNULL(vout_shape);

  *varout_shape = *var_shape;
  *mout_shape = *m_shape;
  *vout_shape = *v_shape;
  return ge::GRAPH_SUCCESS;
}

/**
 * SquareSumV1算子 infer shape函数实现
 *
 */
graphStatus InferShape4SquareSumV1(gert::InferSymbolShapeContext *context) {
  auto input_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(input_shape);

  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);

  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  auto axes = attrs->GetListInt(0);
  GE_ASSERT_NOTNULL(axes);

  std::vector<int64_t> reduce_axes;
  reduce_axes.resize(axes->GetSize());
  for (size_t i = 0U; i < axes->GetSize(); ++i) {
    reduce_axes.at(i) = axes->GetData()[i];
  }

  auto axes_size = axes->GetSize();
  auto keep_dims = attrs->GetBool(1);
  GE_ASSERT_NOTNULL(keep_dims);

  if (*keep_dims) {
    return SymbolicInferUtil::ReduceDimsWithKeepDims<int64_t>(input_shape, reduce_axes, axes_size, out_shape);
  }
  return SymbolicInferUtil::ReduceDimsWithoutKeepDims<int64_t>(input_shape, reduce_axes, axes_size, out_shape);
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ApplyAdagradD).InferSymbolShape(InferShape4ApplyAdagradD);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ApplyAdamD).InferSymbolShape(InferShape4ApplyAdamD);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(SquareSumV1).InferSymbolShape(InferShape4SquareSumV1);
}  // namespace
}  // namespace ge
