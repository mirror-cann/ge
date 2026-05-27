/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
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
constexpr size_t kKeyIdx = 1U;
constexpr size_t kValueIdx = 2U;

// Output indices
constexpr size_t kSoftmaxMaxIdx = 0U;
constexpr size_t kSoftmaxSumIdx = 1U;
constexpr size_t kSoftmaxOutIdx = 2U;
constexpr size_t kAttentionOutIdx = 3U;

// Attribute indices
constexpr size_t kHeadNumIdx = 4U;
constexpr size_t kInputLayoutIdx = 5U;

// Layout constants
constexpr char kLayoutBSH[] = "BSH";
constexpr char kLayoutSBH[] = "SBH";
constexpr char kLayoutBSND[] = "BSND";
constexpr char kLayoutBNSD[] = "BNSD";
constexpr char kLayoutTND[] = "TND";

// Other constants
constexpr size_t kZero = 0U;
constexpr size_t kOne = 1U;
constexpr size_t kTwo = 2U;
constexpr size_t kThree = 3U;
constexpr int64_t kEight = 8;

// Structure to hold dimension variables
typedef struct {
  Expression shape_b;
  Expression shape_s;
  Expression shape_t;
} FlashAttentionDims;

// Calculate dimensions based on input layout
ge::graphStatus CalculateDimensions(const gert::InferSymbolShapeContext *context, const std::string &input_layout,
                                    FlashAttentionDims &dims) {
  auto query_shape = context->GetInputSymbolShape(kQueryIdx);
  GE_UNSUPPORTED_IF_NULL(query_shape);
  if (input_layout == kLayoutSBH) {
    dims.shape_s = query_shape->GetDim(kZero);
    dims.shape_b = query_shape->GetDim(kOne);
  } else if (input_layout == kLayoutTND) {
    dims.shape_t = query_shape->GetDim(kZero);
  } else if (input_layout == kLayoutBSND || input_layout == kLayoutBSH) {
    dims.shape_b = query_shape->GetDim(kZero);
    dims.shape_s = query_shape->GetDim(kOne);
  } else if (input_layout == kLayoutBNSD) {
    dims.shape_b = query_shape->GetDim(kZero);
    dims.shape_s = query_shape->GetDim(kTwo);
  }
  return ge::GRAPH_SUCCESS;
}

// Set softmax shapes
ge::graphStatus SetSoftmaxShapes(gert::InferSymbolShapeContext *context, const std::string &input_layout,
                                 bool is_hifloat8, int64_t head_num, const FlashAttentionDims &dims) {
  auto softmax_max_shape = context->GetOutputSymbolShape(kSoftmaxMaxIdx);
  GE_ASSERT_NOTNULL(softmax_max_shape);
  auto softmax_sum_shape = context->GetOutputSymbolShape(kSoftmaxSumIdx);
  GE_ASSERT_NOTNULL(softmax_sum_shape);
  auto softmax_out_shape = context->GetOutputSymbolShape(kSoftmaxOutIdx);
  GE_ASSERT_NOTNULL(softmax_out_shape);

  softmax_max_shape->Clear();
  softmax_sum_shape->Clear();
  softmax_out_shape->Clear();

  // Calculate softmax_max shape dimensions
  if (is_hifloat8) {
    // For HIFLOAT8 input, shape is [B, head_num, S, 1]
    softmax_max_shape->AppendDim(dims.shape_b);
    softmax_max_shape->AppendDim(Symbol(head_num));
    softmax_max_shape->AppendDim(dims.shape_s);
    softmax_max_shape->AppendDim(kSymbolOne);
  } else if (input_layout == kLayoutTND) {
    // For TND layout, shape is [T, head_num, 8]
    softmax_max_shape->AppendDim(dims.shape_t);
    softmax_max_shape->AppendDim(Symbol(head_num));
    softmax_max_shape->AppendDim(Symbol(kEight));
  } else {
    // For other layouts, shape is [B, head_num, S, 8]
    softmax_max_shape->AppendDim(dims.shape_b);
    softmax_max_shape->AppendDim(Symbol(head_num));
    softmax_max_shape->AppendDim(dims.shape_s);
    softmax_max_shape->AppendDim(Symbol(kEight));
  }

  // softmax_sum_shape has the same shape as softmax_max_shape
  *softmax_sum_shape = *softmax_max_shape;

  // Set softmax_out shape (initialized to [0, 0, 0, 0]
  softmax_out_shape->AppendDim(Symbol(0));
  softmax_out_shape->AppendDim(Symbol(0));
  softmax_out_shape->AppendDim(Symbol(0));
  softmax_out_shape->AppendDim(Symbol(0));

  return ge::GRAPH_SUCCESS;
}

// Set attention_out shape
ge::graphStatus SetAttentionOutShape(gert::InferSymbolShapeContext *context, const std::string &input_layout,
                                     int64_t head_num) {
  auto query_shape = context->GetInputSymbolShape(kQueryIdx);
  auto key_shape = context->GetInputSymbolShape(kKeyIdx);
  auto value_shape = context->GetInputSymbolShape(kValueIdx);

  auto attention_out_shape = context->GetOutputSymbolShape(kAttentionOutIdx);
  GE_ASSERT_NOTNULL(attention_out_shape);

  attention_out_shape->Clear();
  *attention_out_shape = *query_shape;
  if (input_layout == kLayoutBSND || input_layout == kLayoutBNSD) {
    // For BSND or BNSD layout, use the same layout as input
    attention_out_shape->MutableDim(kThree) = value_shape->GetDim(kThree);
  } else if (input_layout == kLayoutTND) {
    // For TND layout: [T, N, D]
    attention_out_shape->MutableDim(kTwo) = value_shape->GetDim(kTwo);
  } else if (input_layout == kLayoutBSH || input_layout == kLayoutSBH) {
    // For BSH or SBH layout, use the same layout as input
    if (head_num == 0) {
      attention_out_shape->MutableDim(kTwo) = Symbol(0);
      return ge::GRAPH_SUCCESS;
    }

    auto n1 = Symbol(head_num);

    auto d1 = query_shape->GetDim(kTwo) / n1;
    if (EXPECT_SYMBOL_EQ(d1, kSymbolZero)) {
      attention_out_shape->MutableDim(kTwo) = Symbol(0);
      return ge::GRAPH_SUCCESS;
    }

    auto n2 = key_shape->GetDim(kTwo) / d1;
    if (EXPECT_SYMBOL_EQ(n2, kSymbolZero)) {
      attention_out_shape->MutableDim(kTwo) = d1 * n1;
      return ge::GRAPH_SUCCESS;
    }

    auto d2 = value_shape->GetDim(kTwo) / n2;
    attention_out_shape->MutableDim(kTwo) = d2 * n1;
  }
  return ge::GRAPH_SUCCESS;
}

/**
 * FlashAttentionScore 算子的符号化shape推导
 * 【算子功能】计算Flash注意力分数，支持多种输入布局格式
 *            输入：query、key、value三个张量
 *            输出：softmax_max、softmax_sum、softmax_out、attention_out四个张量
 * 【算子约束】
 *      1. 支持的输入布局：BSH、SBH、BSND、BNSD、TND
 *      2. head_num属性指定注意力头数
 *      3. input_layout属性指定输入张量的布局格式
 * 【推导逻辑】
 *      1. 获取输入张量形状和属性参数(head_num, input_layout)
 *      2. 验证输入布局的有效性
 *      3. 根据输入布局计算维度B(批次)、S(序列长度)、T(时间步)
 *      4. 根据输入数据类型和布局设置softmax相关输出形状
 *      5. 根据布局类型设置attention_out输出形状
 *      举例：输入query_shape=(2,32,8,64)，key_shape=(2,48,8,64)，value_shape=(2,48,8,128)，
 *            input_layout=BSND，head_num=8，则
 *            softmax_max_shape=(2,8,32,8)，softmax_sum_shape=(2,8,32,8)，
 *            softmax_out_shape=(0,0,0,0)，attention_out_shape=(2,32,8,128)
 */
ge::graphStatus InferShape4FlashAttentionScore(gert::InferSymbolShapeContext *context) {
  // Get input symbol shapes
  auto query_shape = context->GetInputSymbolShape(kQueryIdx);
  GE_UNSUPPORTED_IF_NULL(query_shape);
  auto key_shape = context->GetInputSymbolShape(kKeyIdx);
  GE_UNSUPPORTED_IF_NULL(key_shape);
  auto value_shape = context->GetInputSymbolShape(kValueIdx);
  GE_UNSUPPORTED_IF_NULL(value_shape);

  // Get attributes
  auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const int64_t *head_num_ptr = attrs->GetAttrPointer<int64_t>(kHeadNumIdx);
  GE_ASSERT_NOTNULL(head_num_ptr);
  const char *input_layout_ptr = attrs->GetAttrPointer<char>(kInputLayoutIdx);
  GE_ASSERT_NOTNULL(input_layout_ptr);

  int64_t head_num = *head_num_ptr;
  std::string input_layout = input_layout_ptr;

  // Convert layout to uppercase for case-insensitive comparison
  for (char &c : input_layout) {
    c = toupper(c);
  }
  // Validate input layout
  if (input_layout != kLayoutBSH && input_layout != kLayoutSBH && input_layout != kLayoutBSND &&
      input_layout != kLayoutBNSD && input_layout != kLayoutTND) {
    GELOGE(GRAPH_FAILED, "Input layout %s is not in [BSH, SBH, BSND, BNSD, TND]", input_layout.c_str());
    return GRAPH_FAILED;
  }

  // Get input data type
  const gert::CompileTimeTensorDesc *query_desc = context->GetInputDesc(kQueryIdx);
  GE_ASSERT_NOTNULL(query_desc);
  ge::DataType input_dtype = query_desc->GetDataType();
  bool is_hifloat8 = (input_dtype == ge::DataType::DT_HIFLOAT8);

  // Calculate dimensions based on input layout
  FlashAttentionDims dims;
  GE_ASSERT_GRAPH_SUCCESS(CalculateDimensions(context, input_layout, dims));

  // Set softmax shapes
  GE_ASSERT_GRAPH_SUCCESS(SetSoftmaxShapes(context, input_layout, is_hifloat8, head_num, dims));

  // Set attention_out shape
  GE_ASSERT_GRAPH_SUCCESS(SetAttentionOutShape(context, input_layout, head_num));

  return ge::GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(FlashAttentionScore).InferSymbolShape(InferShape4FlashAttentionScore);

}  // namespace
}  // namespace ge