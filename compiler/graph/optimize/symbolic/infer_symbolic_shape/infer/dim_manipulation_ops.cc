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
#include "graph/utils/type_utils.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"
#include "common/checker.h"
#include "common/framework_types_internal.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"

namespace ge {
namespace {

graphStatus ProcessSqueezeAxes(const gert::SymbolShape *in_shape, gert::SymbolShape *out_shape,
                               const std::vector<int64_t> &axes) {
  if (axes.empty()) {
    // axes is empty, squeeze all dims that are 1
    out_shape->Clear();
    for (size_t i = 0; i < in_shape->GetDimNum(); i++) {
      if (EXPECT_SYMBOL_NE(in_shape->GetDim(i), Symbol(1))) {
        out_shape->AppendDim(in_shape->GetDim(i));
      }
    }
  } else {
    // axes is not empty, squeeze dims that are 1 by axes
    bool squeeze_dims[gert::Shape::kMaxDimNum] = {false};
    const auto dim_num = static_cast<int64_t>(in_shape->GetDimNum());
    for (size_t i = 0; i < axes.size(); i++) {
      const int64_t real_axis = (axes[i] >= 0 ? axes[i] : axes[i] + dim_num);
      GE_ASSERT(real_axis >= 0 && real_axis < dim_num, "Squeeze failed, as axes val[%ld] is out of range[-%ld, %ld].",
                axes[i], dim_num, dim_num);
      squeeze_dims[real_axis] = true;
    }
    out_shape->Clear();
    for (size_t i = 0; i < in_shape->GetDimNum(); i++) {
      if (!squeeze_dims[i]) {
        out_shape->AppendDim(in_shape->GetDim(i));
      } else {
        const auto dim = in_shape->GetDim(i);
        ASSERT_SYMBOL_EQ(dim, Symbol(1));
      }
    }
  }
  return ge::GRAPH_SUCCESS;
}

graphStatus InferShape4Squeeze(gert::InferSymbolShapeContext *context) {
  const auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);
  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  const auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto axes = attrs->GetListInt(0);
  GE_ASSERT_NOTNULL(axes);

  std::vector<int64_t> axes_vec;
  for (size_t i = 0; i < axes->GetSize(); i++) {
    axes_vec.push_back(axes->GetData()[i]);
  }
  return ProcessSqueezeAxes(in_shape, out_shape, axes_vec);
}

graphStatus GetAxesFromTensor(const std::vector<Expression> *axes, std::vector<int64_t> &axes_vec) {
  axes_vec.clear();
  if (axes == nullptr) {
    return GRAPH_SUCCESS;
  }
  axes_vec.reserve(axes->size());
  for (size_t i = 0; i < axes->size(); i++) {
    int64_t raw_axis;
    if (!axes->at(i).GetConstValue<int64_t>(raw_axis)) {
      GELOGW("GetAxesFromTensor failed, axis[%zu] is not const value", i);
      return UNSUPPORTED;
    }
    axes_vec.push_back(raw_axis);
  }
  return GRAPH_SUCCESS;
}

graphStatus InferShape4SqueezeV3(gert::InferSymbolShapeContext *context) {
  const auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);
  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  const auto axes_tensor = context->GetInputSymbolTensor(1);
  std::vector<int64_t> axes_vec;
  if (axes_tensor == nullptr) {
    GELOGD("SqueezeV3: axes is empty, squeeze all dims with value 1. node %s[%s]", context->GetNodeName(),
           context->GetNodeType());
  } else {
    GE_UNSUPPORTED_IF_NULL(axes_tensor->GetSymbolicValue());
    const auto axes = axes_tensor->GetSymbolicValue();
    GELOGD("SqueezeV3: axes is not empty, squeeze dims with axes. axes size: %ld. node %s[%s]", axes->size(),
           context->GetNodeName(), context->GetNodeType());
    auto ret = GetAxesFromTensor(axes, axes_vec);
    if (ret != GRAPH_SUCCESS) {
      return ret;
    }
  }
  return ProcessSqueezeAxes(in_shape, out_shape, axes_vec);
}

graphStatus ProcessUnsqueezeAxes(const gert::SymbolShape *in_shape, gert::SymbolShape *out_shape,
                                 const std::vector<int64_t> &axes) {
  const auto in_dim_num = in_shape->GetDimNum();
  const auto out_dim_num = in_dim_num + axes.size();
  GE_ASSERT(out_dim_num <= gert::Shape::kMaxDimNum,
            "unsqueeze failed, DimNum of output shape is %zu, larger than kMaxDimNum is %zu!", out_dim_num,
            gert::Shape::kMaxDimNum);
  out_shape->Clear();
  for (size_t i = 0; i < out_dim_num; i++) {
    out_shape->AppendDim(ge::Symbol(0));
  }
  const auto out_dim_num_signed = static_cast<int64_t>(out_dim_num);
  for (size_t i = 0; i < axes.size(); i++) {
    const int64_t real_axis = (axes[i] >= 0 ? axes[i] : axes[i] + out_dim_num_signed);
    GE_ASSERT(real_axis >= 0 && real_axis < out_dim_num_signed,
              "unsqueeze failed, as axes val[%zu] is out of range[-%zu, %zu].", axes[i], out_dim_num, out_dim_num);
    GE_ASSERT(out_shape->GetDim(real_axis) != 1, "unsqueeze failed, axis repeated");
    out_shape->MutableDim(real_axis) = ge::Symbol(1);
  }
  size_t in_index = 0;
  for (size_t i = 0; i < out_dim_num; i++) {
    if (out_shape->GetDim(i) != 1) {
      out_shape->MutableDim(i) = in_shape->GetDim(in_index++);
    }
  }
  return ge::GRAPH_SUCCESS;
}

graphStatus InferShape4UnSqueeze(gert::InferSymbolShapeContext *context) {
  const auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);
  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  const auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto axes = attrs->GetListInt(0);
  GE_ASSERT_NOTNULL(axes);
  if (axes->GetSize() == 0) {
    *out_shape = *in_shape;
    return ge::GRAPH_SUCCESS;
  }
  std::vector<int64_t> axes_vec;
  axes_vec.reserve(axes->GetSize());
  for (size_t i = 0; i < axes->GetSize(); i++) {
    axes_vec.push_back(axes->GetData()[i]);
  }
  return ProcessUnsqueezeAxes(in_shape, out_shape, axes_vec);
}

graphStatus InferShape4UnsqueezeV3(gert::InferSymbolShapeContext *context) {
  const auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);
  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  const auto axes_tensor = context->GetInputSymbolTensor(1);
  GE_UNSUPPORTED_IF_NULL(axes_tensor);
  const auto axes = axes_tensor->GetSymbolicValue();
  GE_UNSUPPORTED_IF_NULL(axes);
  if (axes->empty()) {
    GELOGD("UnsqueezeV3: axes is empty, out shape is same as input shape. node %s[%s]", context->GetNodeName(),
           context->GetNodeType());
    *out_shape = *in_shape;
    return ge::GRAPH_SUCCESS;
  }

  std::vector<int64_t> axes_vec;
  auto ret = GetAxesFromTensor(axes, axes_vec);
  if (ret != GRAPH_SUCCESS) {
    return ret;
  }
  return ProcessUnsqueezeAxes(in_shape, out_shape, axes_vec);
}

template <typename T>
graphStatus ExpandDimsInferShapeImpl(const gert::InferSymbolShapeContext *context, const gert::SymbolShape *x_shape,
                                     const gert::SymbolTensor *axis_tensor, gert::SymbolShape *output_shape) {
  T axis_data;
  if (axis_tensor->GetSymbolicValue()->at(0).GetConstValue<T>(axis_data) == false) {
    GELOGW("Symbol Infer unsupported, axis value is not constvalue, node %s[%s]", context->GetNodeName(),
           context->GetNodeType());
    return UNSUPPORTED;
  }
  axis_data = (axis_data < 0) ? (axis_data + static_cast<T>(x_shape->GetDimNum() + 1)) : axis_data;
  if (axis_data < 0 || axis_data > static_cast<int64_t>(x_shape->GetDimNum())) {
    GELOGE(PARAM_INVALID, "axis[%d] is not in [-%d,%d]", static_cast<int64_t>(axis_data), x_shape->GetDimNum() + 1,
           x_shape->GetDimNum());
    return GRAPH_FAILED;
  }
  *output_shape = *x_shape;
  auto &dims = output_shape->MutableDims();
  dims.insert(dims.begin() + axis_data, Symbol(1));
  return GRAPH_SUCCESS;
}

graphStatus InferShape4ExpandDims(gert::InferSymbolShapeContext *context) {
  auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);
  auto axes_tensor = context->GetInputSymbolTensor(1);
  GE_UNSUPPORTED_IF_NULL(axes_tensor);
  if (axes_tensor->GetSymbolicValue() == nullptr) {
    GELOGW("Symbol Infer unsupported, get symbolic value is nullptr, node %s[%s]", context->GetNodeName(),
           context->GetNodeType());
    return UNSUPPORTED;
  }
  auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);

  auto axes_size = static_cast<int32_t>(axes_tensor->GetSymbolicValue()->size());
  if (axes_size != 1) {
    GELOGE(PARAM_INVALID, "expand dims failed, axis input must be a tensor with a single value, actually rank = %d",
           axes_size);
    return GRAPH_FAILED;
  }
  GE_ASSERT_NOTNULL(context->GetInputDesc(1));
  auto dtype = context->GetInputDesc(1)->GetDataType();
  GE_ASSERT(dtype == DT_INT32 || dtype == DT_INT64, "axes datatype %s, must in (DT_INT32, DT_INT64)",
            TypeUtils::DataTypeToSerialString(dtype).c_str());
  if (dtype == DT_INT32) {
    return ExpandDimsInferShapeImpl<int32_t>(context, in_shape, axes_tensor, out_shape);
  }
  return ExpandDimsInferShapeImpl<int64_t>(context, in_shape, axes_tensor, out_shape);
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(Squeeze).InferSymbolShape(InferShape4Squeeze);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(SqueezeV3).InferSymbolShape(InferShape4SqueezeV3);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(Unsqueeze).InferSymbolShape(InferShape4UnSqueeze);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(UnsqueezeV3).InferSymbolShape(InferShape4UnsqueezeV3);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ExpandDims).InferSymbolShape(InferShape4ExpandDims);
}  // namespace
}  // namespace ge
