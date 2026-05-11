/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "register/op_impl_registry.h"
#include "exe_graph/runtime/infer_shape_context.h"
#include "exe_graph/runtime/shape.h"

namespace ge {
graphStatus AddCustomShapeFn(gert::InferShapeContext *ctx) {
  const gert::Shape *x1_shape = ctx->GetInputShape(0);
  gert::Shape *y1_shape = ctx->GetOutputShape(0);
  *y1_shape = *x1_shape;
  return GRAPH_SUCCESS;
}

graphStatus AddCustomInferDataTypeFn(gert::InferDataTypeContext *ctx) {
  const auto inputDataType = ctx->GetInputDataType(0);
  ctx->SetOutputDataType(0, inputDataType);
  return GRAPH_SUCCESS;
}

graphStatus AddCustomInferShapeRange(gert::InferShapeRangeContext *ctx) {
  (void)ctx;
  return GRAPH_SUCCESS;
}

IMPL_OP_INFERSHAPE(AddCustom)
    .InferShape(AddCustomShapeFn)
    .InferDataType(AddCustomInferDataTypeFn)
    .InferShapeRange(AddCustomInferShapeRange);
}  // namespace ge
