/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
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
#include "common/types.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"

namespace ge {
namespace {

/**
 * EmbeddingHashTableLookupOrInsert 算子的符号化shape推导
 * 【算子功能】根据输入的keys从embedding哈希表中查找对应的embedding向量，
 *            若key不存在则插入新的embedding向量
 * 【算子约束】
 *      1. 输入0为embedding表，输入1为查询keys
 *      2. embedding_dim属性指定每个embedding向量的维度
 * 【推导逻辑】
 *      1. 获取输入embedding表形状、keys形状和embedding_dim属性
 *      2. 计算keys的总元素数量（将所有维度相乘）
 *      3. 输出shape为[keys总元素数, embedding_dim]
 *      举例：输入keys_shape=(2,3)，embedding_dim=128，则输出=(6,128)
 */
graphStatus InferShape4EmbeddingHashTableLookupOrInsert(gert::InferSymbolShapeContext *context) {
  const auto in_table_shape = context->GetInputSymbolShape(0);
  GE_UNSUPPORTED_IF_NULL(in_table_shape);
  const auto in_keys_shape = context->GetInputSymbolShape(1);
  GE_UNSUPPORTED_IF_NULL(in_keys_shape);
  const auto out_shape = context->GetOutputSymbolShape(0);
  GE_ASSERT_NOTNULL(out_shape);
  const auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto embedding_dim_attr = attrs->GetInt(1);
  GE_ASSERT_NOTNULL(embedding_dim_attr);
  const int64_t embedding_dim = *embedding_dim_attr;

  const int64_t keys_dim = in_keys_shape->GetDimNum();
  auto product = kSymbolOne;
  for (int64_t i = 0; i < keys_dim; i++) {
    product = product * in_keys_shape->GetDim(i);
  }

  out_shape->Clear();
  out_shape->AppendDim(product);
  out_shape->AppendDim(Symbol(embedding_dim));

  return ge::GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(EmbeddingHashTableLookupOrInsert)
    .InferSymbolShape(InferShape4EmbeddingHashTableLookupOrInsert);
}  // namespace
}  // namespace ge