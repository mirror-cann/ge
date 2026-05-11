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
#include "common/checker.h"
#include "common/types.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"

namespace ge {
namespace {

// Input indices
constexpr size_t kQueryIdx = 0U;
constexpr size_t kValueIdx = 2U;
constexpr size_t kDynamicValueIdx = 0U;
constexpr size_t kBlockTableIdx = 14U;

// Output indices
constexpr size_t kAttentionOutIdx = 0U;
constexpr size_t kSoftmaxLseIdx = 1U;

// Attribute indices
constexpr size_t kNumHeadsIdx = 0U;
constexpr size_t kNumKeyValueHeadsIdx = 5U;
constexpr size_t kInputLayoutIdx = 4U;
constexpr size_t kSoftmaxLseFlagIdx = 10U;

// Layout constants
constexpr char kLayoutBSH[] = "BSH";
constexpr char kLayoutBSND[] = "BSND";
constexpr char kLayoutBNSD[] = "BNSD";
constexpr char kLayoutNBSD[] = "NBSD";
constexpr char kLayoutTND[] = "TND";
constexpr char kLayoutNTD[] = "NTD";
constexpr char kLayoutNSD[] = "NSD";
constexpr char kLayoutBNSDBSND[] = "BNSD_BSND";
constexpr char kLayoutBSHBNSD[] = "BSH_BNSD";
constexpr char kLayoutBSNDBSND[] = "BSND_BSND";
constexpr char kLayoutNTDTND[] = "NTD_TND";
constexpr char kLayoutBSHNBSD[] = "BSH_NBSD";
constexpr char kLayoutBSNDNBSD[] = "BSND_NBSD";
constexpr char kLayoutBNSDNBSD[] = "BNSD_NBSD";
constexpr char kLayoutTNT_NTD[] = "TND_NTD";

// Other constants
constexpr size_t kZero = 0U;
constexpr size_t kOne = 1U;
constexpr size_t kTwo = 2U;
constexpr size_t kThree = 3U;
constexpr size_t kFour = 4U;
constexpr size_t kFive = 5U;

// Structure to hold dimension variables
typedef struct {
  Expression shape_b;
  Expression shape_s1;
  Expression shape_n1;
  Expression shape_d1;
  Expression shape_t;
} FusedInferAttentionDims;

// Get query and output layout based on input layout
ge::graphStatus GetQueryAndOutLayout(std::string &queryLayout, std::string &attentionOutLayout,
                                     const std::string &inputLayout, const gert::SymbolShape *queryShape) {
  struct ParserLayout {
    std::string qLayout;
    std::string outLayout;
  };

  const std::map<std::string, ParserLayout> LAYOUT_MAP = {
      {kLayoutBSH, {kLayoutBSH, kLayoutBSH}},        {kLayoutBSND, {kLayoutBSND, kLayoutBSND}},
      {kLayoutBNSD, {kLayoutBNSD, kLayoutBNSD}},     {kLayoutTND, {kLayoutTND, kLayoutTND}},
      {kLayoutNTD, {kLayoutNTD, kLayoutNTD}},        {kLayoutBNSDBSND, {kLayoutBNSD, kLayoutBSND}},
      {kLayoutBSHBNSD, {kLayoutBSH, kLayoutBNSD}},   {kLayoutBSNDBSND, {kLayoutBSND, kLayoutBSND}},
      {kLayoutNTDTND, {kLayoutNTD, kLayoutTND}},     {kLayoutBSHNBSD, {kLayoutBSH, kLayoutNBSD}},
      {kLayoutBSNDNBSD, {kLayoutBSND, kLayoutNBSD}}, {kLayoutBNSDNBSD, {kLayoutBNSD, kLayoutNBSD}},
      {kLayoutTNT_NTD, {kLayoutTND, kLayoutNTD}},    {kLayoutNSD, {kLayoutNSD, kLayoutNSD}}};

  auto it = LAYOUT_MAP.find(inputLayout);
  if (it != LAYOUT_MAP.end()) {
    queryLayout = it->second.qLayout;
    attentionOutLayout = it->second.outLayout;
    if (queryShape->GetDimNum() != queryLayout.size()) {
      GELOGE(GRAPH_FAILED, "Query shape dim num %d is not equal to layout dim num %d.", queryShape->GetDimNum(),
             queryLayout.size());
      return ge::GRAPH_FAILED;
    }
  } else {
    GELOGE(GRAPH_FAILED,
           "Only support layout BSH, BSND, BNSD, TND, NTD, BNSD_BSND, BSH_BNSD, "
           "BSND_BSND, NTD_TND, BSH_NBSD, BSND_NBSD, BNSD_NBSD, TND_NTD, NSD, but got %s.",
           inputLayout.c_str());
    return ge::GRAPH_FAILED;
  }
  return ge::GRAPH_SUCCESS;
}

// Get query dimensions for BSH, BSND, BNSD, NSD layouts
ge::graphStatus GetQueryBSND(const gert::SymbolShape *queryShape, const std::string &queryLayout, int64_t numHeads,
                             FusedInferAttentionDims &dims) {
  if (queryLayout == kLayoutBSH) {
    dims.shape_b = queryShape->GetDim(kZero);
    dims.shape_s1 = queryShape->GetDim(kOne);
    dims.shape_n1 = Symbol(numHeads);
    dims.shape_d1 = queryShape->GetDim(kTwo) / dims.shape_n1;
  } else if (queryLayout == kLayoutBSND) {
    dims.shape_b = queryShape->GetDim(kZero);
    dims.shape_s1 = queryShape->GetDim(kOne);
    dims.shape_n1 = queryShape->GetDim(kTwo);
    dims.shape_d1 = queryShape->GetDim(kThree);
  } else if (queryLayout == kLayoutBNSD) {
    dims.shape_b = queryShape->GetDim(kZero);
    dims.shape_s1 = queryShape->GetDim(kTwo);
    dims.shape_n1 = queryShape->GetDim(kOne);
    dims.shape_d1 = queryShape->GetDim(kThree);
  } else if (queryLayout == kLayoutNSD) {
    dims.shape_b = Symbol(1);
    dims.shape_s1 = queryShape->GetDim(kOne);
    dims.shape_n1 = queryShape->GetDim(kZero);
    dims.shape_d1 = queryShape->GetDim(kTwo);
  } else {
    GELOGE(GRAPH_FAILED, "Layout %s is not supported in GetQueryBSND function!", queryLayout.c_str());
    return ge::GRAPH_FAILED;
  }
  return ge::GRAPH_SUCCESS;
}

// Get query dimensions for TND, NTD layouts
ge::graphStatus GetQueryTND(const gert::SymbolShape *queryShape, const std::string &queryLayout,
                            FusedInferAttentionDims &dims) {
  if (queryLayout == kLayoutTND) {
    dims.shape_t = queryShape->GetDim(kZero);
    dims.shape_n1 = queryShape->GetDim(kOne);
    dims.shape_d1 = queryShape->GetDim(kTwo);
  } else if (queryLayout == kLayoutNTD) {
    dims.shape_t = queryShape->GetDim(kOne);
    dims.shape_n1 = queryShape->GetDim(kZero);
    dims.shape_d1 = queryShape->GetDim(kTwo);
  } else {
    GELOGE(GRAPH_FAILED, "Layout %s is not supported in GetQueryTND function!", queryLayout.c_str());
    return ge::GRAPH_FAILED;
  }
  return ge::GRAPH_SUCCESS;
}

// Calculate value depth dimension
ge::graphStatus GetValueD(bool isPageAttention, Expression &valueD, const gert::SymbolShape *valueShape,
                          const gert::SymbolShape *queryShape, const std::string &queryLayout,
                          int64_t numKeyValueHeads) {
  if (isPageAttention) {  // PA场景
    if (valueShape->GetDimNum() == 3) {
      valueD = valueShape->GetDim(kTwo) / Symbol(numKeyValueHeads);
    } else if (valueShape->GetDimNum() == 4) {
      valueD = valueShape->GetDim(kThree);
    } else if (valueShape->GetDimNum() == 5) {
      valueD = valueShape->GetDim(kTwo) * valueShape->GetDim(kFour);
    } else {
      GELOGE(GRAPH_FAILED, "when Page Attention enabled, value's dim should be 3/4/5, but got %zu.",
             valueShape->GetDimNum());
      return ge::GRAPH_FAILED;
    }
  } else {  // 非PA场景
    if (valueShape->GetDimNum() != queryShape->GetDimNum()) {
      GELOGE(GRAPH_FAILED, "Value shape dim num %d is not equal to query shape dim num %d.", valueShape->GetDimNum(),
             queryShape->GetDimNum());
      return ge::GRAPH_FAILED;
    }

    if (queryLayout == kLayoutBSH) {
      valueD = valueShape->GetDim(kTwo) / Symbol(numKeyValueHeads);
    } else if (queryLayout == kLayoutBSND || queryLayout == kLayoutBNSD) {
      valueD = valueShape->GetDim(kThree);
    } else if (queryLayout == kLayoutTND || queryLayout == kLayoutNTD || queryLayout == kLayoutNSD) {
      valueD = valueShape->GetDim(kTwo);
    }
  }
  return ge::GRAPH_SUCCESS;
}

// Infer attention output shape
ge::graphStatus InferAttentionOutShape(std::string attentionOutLayout, gert::SymbolShape *attentionOutShape,
                                       const gert::SymbolShape *queryShape, const std::string &queryLayout,
                                       int64_t numHeads, Expression valueD) {
  FusedInferAttentionDims dims;
  attentionOutShape->Clear();

  // Helper function to calculate output dimension
  auto calculateOutD = [&]() -> Expression {
    return (valueD == kSymbolZero || dims.shape_d1 == kSymbolZero) ? dims.shape_d1 : valueD;
  };

  if (attentionOutLayout == kLayoutBSH) {
    GetQueryBSND(queryShape, queryLayout, numHeads, dims);
    Expression outH = Symbol(numHeads) * valueD;
    Expression queryH = queryShape->GetDim(kTwo);
    outH = (outH == kSymbolZero || queryH == kSymbolZero) ? queryH : outH;
    attentionOutShape->MutableDims() = {dims.shape_b, dims.shape_s1, outH};
  } else if (attentionOutLayout == kLayoutBSND) {
    GetQueryBSND(queryShape, queryLayout, numHeads, dims);
    Expression outD = calculateOutD();
    attentionOutShape->MutableDims() = {dims.shape_b, dims.shape_s1, dims.shape_n1, outD};
  } else if (attentionOutLayout == kLayoutBNSD) {
    GetQueryBSND(queryShape, queryLayout, numHeads, dims);
    Expression outD = calculateOutD();
    attentionOutShape->MutableDims() = {dims.shape_b, dims.shape_n1, dims.shape_s1, outD};
  } else if (attentionOutLayout == "NBSD") {
    GetQueryBSND(queryShape, queryLayout, numHeads, dims);
    Expression outD = calculateOutD();
    attentionOutShape->MutableDims() = {dims.shape_n1, dims.shape_b, dims.shape_s1, outD};
  } else if (attentionOutLayout == kLayoutTND) {
    GetQueryTND(queryShape, queryLayout, dims);
    Expression outD = calculateOutD();
    attentionOutShape->MutableDims() = {dims.shape_t, dims.shape_n1, outD};
  } else if (attentionOutLayout == kLayoutNTD) {
    GetQueryTND(queryShape, queryLayout, dims);
    Expression outD = calculateOutD();
    attentionOutShape->MutableDims() = {dims.shape_n1, dims.shape_t, outD};
  } else if (attentionOutLayout == kLayoutNSD) {
    GetQueryBSND(queryShape, queryLayout, numHeads, dims);
    Expression outD = calculateOutD();
    attentionOutShape->MutableDims() = {dims.shape_n1, dims.shape_s1, outD};
  }
  return ge::GRAPH_SUCCESS;
}

// Infer softmax LSE output shape
ge::graphStatus InferLseOutShape(const std::string &inputLayout, gert::SymbolShape *softmaxLseShape,
                                 const gert::SymbolShape *queryShape, const std::string &queryLayout,
                                 int64_t numHeads) {
  FusedInferAttentionDims dims;
  softmaxLseShape->Clear();

  if (inputLayout == kLayoutTND || inputLayout == kLayoutNTD || inputLayout == kLayoutTNT_NTD ||
      inputLayout == kLayoutNTDTND) {
    GetQueryTND(queryShape, queryLayout, dims);
    softmaxLseShape->MutableDims() = {dims.shape_t, dims.shape_n1, Symbol(1)};
  } else {
    GetQueryBSND(queryShape, queryLayout, numHeads, dims);
    softmaxLseShape->MutableDims() = {dims.shape_b, dims.shape_n1, dims.shape_s1, Symbol(1)};
  }
  return ge::GRAPH_SUCCESS;
}

/**
 * FusedInferAttentionScore 算子的符号化shape推导
 * 【算子功能】计算融合推理注意力分数，支持多种输入输出布局格式，支持Page Attention优化
 *            输入：query、key、value等张量
 *            输出：attention_out(注意力输出)、softmax_lse(softmax对数和)两个张量
 * 【算子约束】
 *      1. 支持的输入布局：BSH、BSND、BNSD、TND、NTD、NSD、BNSD_BSND等多种布局组合
 *      2. numHeads属性指定注意力头数，不能为0
 *      3. numKeyValueHeads属性指定KV头数，为0时等于numHeads
 *      4. softmaxLseFlag属性决定是否输出softmax_lse
 * 【推导逻辑】
 *      1. 获取输入张量形状，区分Page Attention和非Page Attention场景
 *      2. 根据input_layout确定query布局和output布局
 *      3. 计算value张量的深度维度valueD
 *      4. 根据输出布局类型推导attention_out形状
 *      5. 根据softmaxLseFlag推导softmax_lse形状
 *      举例：输入query_shape=(2,32,8,64)，value_shape=(2,48,8,128)，input_layout=BSND，
 *            numHeads=8，则attention_out_shape=(2,32,8,128)，softmax_lse_shape=(2,8,32,1)
 */
ge::graphStatus InferShape4FusedInferAttentionScore(gert::InferSymbolShapeContext *context) {
  // Get input symbol shapes
  auto query_shape = context->GetInputSymbolShape(kQueryIdx);
  GE_UNSUPPORTED_IF_NULL(query_shape);
  auto key_shape = context->GetInputSymbolShape(1);
  GE_UNSUPPORTED_IF_NULL(key_shape);
  auto value_shape_2 = context->GetInputSymbolShape(2);
  GE_UNSUPPORTED_IF_NULL(value_shape_2);

  // Get value shape (dynamic input)
  auto value_shape = context->GetDynamicInputSymbolShape(kValueIdx, kDynamicValueIdx);
  GE_UNSUPPORTED_IF_NULL(value_shape);

  // Check if Page Attention is enabled
  bool isPageAttention = (context->GetOptionalInputSymbolShape(kBlockTableIdx) == nullptr) ? false : true;

  // Get output symbol shapes
  auto attention_out_shape = context->GetOutputSymbolShape(kAttentionOutIdx);
  GE_ASSERT_NOTNULL(attention_out_shape);
  auto softmax_lse_shape = context->GetOutputSymbolShape(kSoftmaxLseIdx);
  GE_ASSERT_NOTNULL(softmax_lse_shape);

  // Get attributes
  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const char *input_layout_ptr = attrs->GetAttrPointer<char>(kInputLayoutIdx);
  GE_ASSERT_NOTNULL(input_layout_ptr);
  const int64_t *num_heads_ptr = attrs->GetAttrPointer<int64_t>(kNumHeadsIdx);
  GE_ASSERT_NOTNULL(num_heads_ptr);
  const int64_t *num_key_value_heads_ptr = attrs->GetAttrPointer<int64_t>(kNumKeyValueHeadsIdx);
  GE_ASSERT_NOTNULL(num_key_value_heads_ptr);

  // Validate num_heads
  int64_t num_heads = *num_heads_ptr;
  if (num_heads == 0) {
    GELOGE(GRAPH_FAILED, "numHeads cannot be 0!. node %s[%s]", context->GetNodeName(), context->GetNodeType());
    return ge::GRAPH_FAILED;
  }

  // Set num_key_value_heads to num_heads if it's 0
  int64_t num_key_value_heads = (*num_key_value_heads_ptr == 0) ? num_heads : *num_key_value_heads_ptr;

  // Set default attention_out shape
  attention_out_shape->Clear();
  *attention_out_shape = *query_shape;

  // Get query and output layout
  std::string query_layout = kLayoutBSH;
  std::string attention_out_layout = kLayoutBSH;
  GE_ASSERT_GRAPH_SUCCESS(
      GetQueryAndOutLayout(query_layout, attention_out_layout, std::string(input_layout_ptr), query_shape));

  // Calculate value depth dimension
  Expression value_d;
  GE_ASSERT_GRAPH_SUCCESS(
      GetValueD(isPageAttention, value_d, value_shape, query_shape, query_layout, num_key_value_heads));

  // Infer attention_out shape
  GE_ASSERT_GRAPH_SUCCESS(
      InferAttentionOutShape(attention_out_layout, attention_out_shape, query_shape, query_layout, num_heads, value_d));

  // Get softmax_lse_flag
  const bool *softmax_lse_ptr = attrs->GetAttrPointer<bool>(kSoftmaxLseFlagIdx);
  bool softmax_lse_flag = (softmax_lse_ptr != nullptr) ? *softmax_lse_ptr : false;

  // Infer softmax_lse shape
  if (softmax_lse_flag) {
    GE_ASSERT_GRAPH_SUCCESS(
        InferLseOutShape(std::string(input_layout_ptr), softmax_lse_shape, query_shape, query_layout, num_heads));
  } else {
    softmax_lse_shape->Clear();
    softmax_lse_shape->AppendDim(Symbol(0));
  }

  return ge::GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(FusedInferAttentionScore).InferSymbolShape(InferShape4FusedInferAttentionScore);

}  // namespace
}  // namespace ge