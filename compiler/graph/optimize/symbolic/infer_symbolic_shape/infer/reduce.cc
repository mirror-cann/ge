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
#include "common/checker.h"
#include "common/framework_types_internal.h"
#include "graph/compute_graph.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"
#include "graph/utils/type_utils.h"

namespace ge {
namespace {
graphStatus InferShape4ReduceCommon(gert::InferSymbolShapeContext *context) {
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
  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const bool *keep_dims = attrs->GetAttrPointer<bool>(0);
  GE_ASSERT_NOTNULL(keep_dims);
  if (axes_tensor->GetSymbolicValue()->empty()) {
    *out_shape = *in_shape;
    GELOGD("axes is empty tensor, will ignore infer, set output shape = input shape");
    return ge::GRAPH_SUCCESS;
  }
  auto axes_size = axes_tensor->GetSymbolicValue()->size();
  auto input1_desc = context->GetInputDesc(1);
  GE_ASSERT_NOTNULL(input1_desc);
  auto dtype = input1_desc->GetDataType();
  GE_ASSERT(dtype == DT_INT32 || dtype == DT_INT64, "axes datatype %s, must in (DT_INT32，DT_INT64)",
            TypeUtils::DataTypeToSerialString(dtype).c_str());
  if (dtype == DT_INT32) {
    return SymbolicInferUtil::ReduceDims<int32_t>(in_shape, axes_tensor, axes_size, *keep_dims, out_shape);
  }
  return SymbolicInferUtil::ReduceDims<int64_t>(in_shape, axes_tensor, axes_size, *keep_dims, out_shape);
}

graphStatus InferShape4ReduceDCommon(gert::InferSymbolShapeContext *context) {
  auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);
  auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  auto axes = attrs->GetListInt(0);
  GE_ASSERT_NOTNULL(axes);
  const bool *keep_dims = attrs->GetAttrPointer<bool>(1);
  GE_ASSERT_NOTNULL(keep_dims);
  auto axes_size = axes->GetSize();
  if (axes_size == 0) {
    *out_shape = *in_shape;
    GELOGD("axes is empty tensor, will ignore infer, set output shape = input shape");
    return ge::GRAPH_SUCCESS;
  }
  std::vector<int64_t> reduce_axes;
  reduce_axes.resize(axes->GetSize());
  for (size_t i = 0; i < axes->GetSize(); ++i) {
    reduce_axes.at(i) = axes->GetData()[i];
  }
  if (*keep_dims) {
    return SymbolicInferUtil::ReduceDimsWithKeepDims<int64_t>(in_shape, reduce_axes, axes_size, out_shape);
  }
  return SymbolicInferUtil::ReduceDimsWithoutKeepDims<int64_t>(in_shape, reduce_axes, axes_size, out_shape);
}

graphStatus InferShape4BiasAddGrad(gert::InferSymbolShapeContext *context) {
  auto in_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_shape);
  auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  auto data_format = attrs->GetStr(0);
  GE_ASSERT_NOTNULL(data_format);
  std::vector<int64_t> reduce_axes;
  if (strcmp(data_format, "NCHW") != 0 && strcmp(data_format, "NHWC") != 0) {
    GELOGE(PARAM_INVALID, "data_format should be NHWC or NCHW");
    return ge::PARAM_INVALID;
  }
  // 降维到只有特征维度，NHWC特征维度在最后一个， NCHW特征维度在倒数第三个
  const int64_t nhwc_min_dim_size = 1;
  int64_t dim_size = static_cast<int64_t>(in_shape->GetDimNum());
  GE_ASSERT_TRUE(dim_size >= nhwc_min_dim_size);
  int64_t ignore_reduce_axes = dim_size - nhwc_min_dim_size;
  if (strcmp(data_format, "NCHW") == 0) {
    const int64_t nchw_min_dim_size = 3;
    GE_ASSERT_TRUE(dim_size >= nchw_min_dim_size);
    ignore_reduce_axes = dim_size - nchw_min_dim_size;
  }
  for (int64_t i = 0; i < dim_size; i++) {
    if (ignore_reduce_axes != i) {
      reduce_axes.emplace_back(i);
    }
  }

  return SymbolicInferUtil::ReduceDimsWithoutKeepDims<int64_t>(in_shape, reduce_axes, reduce_axes.size(), out_shape);
}

graphStatus InferShape4LayerNormBetaGammaBackpropV2(gert::InferSymbolShapeContext *context) {
  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  auto dy_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(dy_shape);
  auto res_for_gamma_shape = context->GetInputSymbolShape(1);
  GE_UNSUPPORTED_IF_NULL(res_for_gamma_shape);
  auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  auto out1_shape = context->GetOutputSymbolShape(1);
  GE_ASSERT_NOTNULL(out1_shape);

  // shape_gamma
  auto cv = attrs->GetListInt(0);
  GE_ASSERT_NOTNULL(cv);
  std::vector<int64_t> dims;
  for (size_t i = 0; i < cv->GetSize(); i++) {
    int64_t expr_value = 0;
    expr_value = cv->GetData()[i];
    dims.push_back(expr_value);
  }

  auto it_found = std::find(dims.begin(), dims.end(), -1);
  std::vector<int64_t> reduce_axes;
  if (it_found == dims.end()) {
    // shape is shape as shape_gamma
    out_shape->Clear();
    for (auto dim : dims) {
      out_shape->AppendDim(Symbol(dim));
      out1_shape->AppendDim(Symbol(dim));
    }
    return ge::GRAPH_SUCCESS;
  }

  // dy_shape[s0, s1, s2] dims[-1, 0, -1] reduce_axs[0, 2]  output[1, s1, 1]
  for (int64_t i = 0; i < static_cast<int64_t>(dims.size()); i++) {
    if (dims[i] == -1) {
      reduce_axes.emplace_back(i);
    }
  }
  GE_ASSERT_GRAPH_SUCCESS(
      SymbolicInferUtil::Broadcast({dy_shape->GetDims(), res_for_gamma_shape->GetDims()}, out_shape->MutableDims()));
  GE_ASSERT_GRAPH_SUCCESS(
      SymbolicInferUtil::ReduceDimsWithKeepDims<int64_t>(out_shape, reduce_axes, reduce_axes.size(), out_shape));
  GE_ASSERT_GRAPH_SUCCESS(
      SymbolicInferUtil::ReduceDimsWithKeepDims<int64_t>(dy_shape, reduce_axes, reduce_axes.size(), out1_shape));
  return ge::GRAPH_SUCCESS;
}

graphStatus InferShape4LayerNormV3(gert::InferSymbolShapeContext *context) {
  auto x_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(x_shape);
  auto gamma_shape = context->GetInputSymbolShape(1);
  GE_UNSUPPORTED_IF_NULL(gamma_shape);
  auto beta_shape = context->GetInputSymbolShape(2);
  GE_UNSUPPORTED_IF_NULL(beta_shape);
  auto out_y_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_y_shape);
  auto out_mean_shape = context->GetOutputSymbolShape(1);
  GE_ASSERT_NOTNULL(out_mean_shape);
  auto out_rstd_shape = context->GetOutputSymbolShape(2);
  GE_ASSERT_NOTNULL(out_rstd_shape);
  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);

  size_t real_dim_num = x_shape->GetDimNum();
  auto begin_norm_axis_ptr = attrs->GetInt(0);
  GE_ASSERT_NOTNULL(begin_norm_axis_ptr);
  int64_t begin_norm_axis = *(begin_norm_axis_ptr);
  if (begin_norm_axis < 0) {
    begin_norm_axis += static_cast<int64_t>(real_dim_num);
  }
  if (begin_norm_axis < 0 || static_cast<size_t>(begin_norm_axis) >= real_dim_num) {
    GELOGE(PARAM_INVALID,
           "the op layernormv3 does not support beginNormAxis"
           "(%ld) large than shape dims(%lu)",
           begin_norm_axis, real_dim_num);
    return ge::PARAM_INVALID;
  }
  // the shape of y is equal with input x
  *out_y_shape = *x_shape;

  vector<int64_t> reduce_axes;
  for (int64_t i = begin_norm_axis; i < static_cast<int64_t>(real_dim_num); i++) {
    reduce_axes.emplace_back(i);
  }

  GE_ASSERT_GRAPH_SUCCESS(
      SymbolicInferUtil::ReduceDimsWithKeepDims<int64_t>(x_shape, reduce_axes, reduce_axes.size(), out_mean_shape));
  GE_ASSERT_GRAPH_SUCCESS(
      SymbolicInferUtil::ReduceDimsWithKeepDims<int64_t>(x_shape, reduce_axes, reduce_axes.size(), out_rstd_shape));
  return ge::GRAPH_SUCCESS;
}

/**
 * LayerNormV4 算子的符号化shape推导
 * 【算子功能】对输入张量进行层归一化操作，输出归一化后的值、均值和反向标准差
 *            输入：x(输入张量)、norm_shape(归一化维度)
 *            输出：y(归一化后的张量)、mean(均值)、rstd(反向标准差)
 * 【算子约束】
 *      1. norm_shape的长度必须 <= 输入张量的维度数
 *      2. 归一化从倒数第norm_shape_len个维度开始到最后一个维度
 * 【推导逻辑】
 *      1. 获取输入x的形状，从norm_shape的shape获取norm_shape_len
 *      2. y的形状与输入x相同
 *      3. 计算归一化起始轴begin_norm_axis = x_dim_num - norm_shape_len
 *      4. 从begin_norm_axis到最后一个维度进行reduce操作(keep_dims=True)得到mean和rstd的形状
 *      举例：输入x_shape=(2,3,4,5)，norm_shape=(4,5)，则y_shape=(2,3,4,5)，
 *            mean_shape=(2,3,1,1)，rstd_shape=(2,3,1,1)
 */
graphStatus InferShape4LayerNormV4(gert::InferSymbolShapeContext *context) {
  auto x_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(x_shape);
  auto norm_shape_shape = context->GetInputSymbolShape(1);
  GE_UNSUPPORTED_IF_NULL(norm_shape_shape);

  auto out_y_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_y_shape);
  auto out_mean_shape = context->GetOutputSymbolShape(1);
  GE_ASSERT_NOTNULL(out_mean_shape);
  auto out_rstd_shape = context->GetOutputSymbolShape(2);
  GE_ASSERT_NOTNULL(out_rstd_shape);

  // the shape of y is equal with input x
  *out_y_shape = *x_shape;

  int64_t real_dim_num = static_cast<int64_t>(x_shape->GetDimNum());

  // get norm_shape_len from norm_shape's shape
  GE_ASSERT_TRUE(norm_shape_shape->GetDimNum() <= 1, "Shape of norm_shape should be 1 dimensions!");
  int64_t norm_shape_len = norm_shape_shape->IsScalar() ? 1 : 0;
  if (!norm_shape_shape->IsScalar()) {
    GE_ASSERT_TRUE(norm_shape_shape->GetDim(0).GetConstValue<int64_t>(norm_shape_len),
                   "norm_shape_len must be a constant value");
  }

  GE_ASSERT_TRUE(real_dim_num >= norm_shape_len, "norm_shape_len(%ld) must be <= xshape rank(%ld)", norm_shape_len,
                 real_dim_num);

  int64_t begin_norm_axis_val = real_dim_num - norm_shape_len;
  vector<int64_t> reduce_axes;
  reduce_axes.reserve(real_dim_num - begin_norm_axis_val);
  for (int64_t i = begin_norm_axis_val; i < real_dim_num; i++) {
    reduce_axes.emplace_back(i);
  }

  GE_ASSERT_GRAPH_SUCCESS(
      SymbolicInferUtil::ReduceDimsWithKeepDims<int64_t>(x_shape, reduce_axes, reduce_axes.size(), out_mean_shape));
  GE_ASSERT_GRAPH_SUCCESS(
      SymbolicInferUtil::ReduceDimsWithKeepDims<int64_t>(x_shape, reduce_axes, reduce_axes.size(), out_rstd_shape));
  return ge::GRAPH_SUCCESS;
}

graphStatus InferShapeForBNTrainingUpdateGrad(gert::InferSymbolShapeContext *context) {
  const auto input_batch_mean_shape = context->GetInputSymbolShape(2);
  GE_UNSUPPORTED_IF_NULL(input_batch_mean_shape);
  auto output_diff_scale_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(output_diff_scale_shape);
  auto output_diff_offset_shape = context->GetOutputSymbolShape(1);
  GE_ASSERT_NOTNULL(output_diff_offset_shape);
  *output_diff_scale_shape = *input_batch_mean_shape;
  *output_diff_offset_shape = *input_batch_mean_shape;
  return ge::GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(BNTrainingUpdateGrad).InferSymbolShape(InferShapeForBNTrainingUpdateGrad);

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceSum).InferSymbolShape(InferShape4ReduceCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceMax).InferSymbolShape(InferShape4ReduceCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceMin).InferSymbolShape(InferShape4ReduceCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceProd).InferSymbolShape(InferShape4ReduceCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceMean).InferSymbolShape(InferShape4ReduceCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceAll).InferSymbolShape(InferShape4ReduceCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceAny).InferSymbolShape(InferShape4ReduceCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceSumD).InferSymbolShape(InferShape4ReduceDCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceMaxD).InferSymbolShape(InferShape4ReduceDCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceMinD).InferSymbolShape(InferShape4ReduceDCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceProdD).InferSymbolShape(InferShape4ReduceDCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceMeanD).InferSymbolShape(InferShape4ReduceDCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceAllD).InferSymbolShape(InferShape4ReduceDCommon);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(ReduceAnyD).InferSymbolShape(InferShape4ReduceDCommon);

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(BiasAddGrad).InferSymbolShape(InferShape4BiasAddGrad);

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(LayerNormBetaGammaBackpropV2)
    .InferSymbolShape(InferShape4LayerNormBetaGammaBackpropV2);

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(LayerNormV3).InferSymbolShape(InferShape4LayerNormV3);
IMPL_OP_INFER_SYMBOL_SHAPE_INNER(LayerNormV4).InferSymbolShape(InferShape4LayerNormV4);
}  // namespace
}  // namespace ge
