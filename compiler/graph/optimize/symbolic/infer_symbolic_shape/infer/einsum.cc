/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "common/checker.h"
#include "common/types.h"
#include "exe_graph/runtime/infer_symbol_shape_context.h"
#include "graph/compute_graph.h"
#include "graph/optimize/symbolic/infer_symbolic_shape/symbolic_infer_util.h"

namespace ge {
namespace {
// 与 host 侧保持一致：Einsum 当前仅支持最多 2 个输入。
constexpr size_t kInputNumLimitEinsum = 2U;
constexpr size_t kOutputIdxEinsum = 0U;
// attr 顺序与 IR 对齐：0=equation, 1=N。
constexpr size_t kEquationIdxEinsum = 0U;
constexpr size_t kNIdxEinsum = 1U;
constexpr size_t kEllipsisLength = 3U;
constexpr char kComma = ',';
const std::string kEllipsis = "...";

// 判断 equation 片段是否包含 "..."。
bool HasEllipsis(const std::string &equation_part) {
  return equation_part.find(kEllipsis) != std::string::npos;
}

// 与 host 语义一致：先去空格再做 equation 解析。
void TrimWhitespace(std::string &equation) {
  equation.erase(std::remove(equation.begin(), equation.end(), ' '), equation.end());
}

// 将 "in->out" 拆分为输入表达式和输出表达式。
bool SplitEquation(const std::string &equation, std::string &input_equation, std::string &output_equation) {
  const auto arrow_pos = equation.find("->");
  if (arrow_pos == std::string::npos) {
    return false;
  }
  input_equation = equation.substr(0U, arrow_pos);
  output_equation = equation.substr(arrow_pos + 2U);
  return true;
}

// 以 ',' 切分输入表达式，例如 "ab,bc" -> ["ab", "bc"]。
void SplitInputEquation(const std::string &input_equation, std::vector<std::string> &input_equation_parts) {
  input_equation_parts.clear();
  std::string part;
  for (const auto c : input_equation) {
    if (c == kComma) {
      input_equation_parts.emplace_back(part);
      part.clear();
      continue;
    }
    part.push_back(c);
  }
  input_equation_parts.emplace_back(part);
}

// 校验单个 equation 片段：
// 1) '.' 只能以 "..." 形式出现；
// 2) 标签必须是字母；
// 3) 输出侧是否出现重复标签需要单独记录，后续与输入侧联合判定。
bool CheckEllipsisAndDuplicatedLabel(std::string equation_part, bool &has_duplicated_label) {
  const auto ellipsis_pos = equation_part.find(kEllipsis);
  if (ellipsis_pos != std::string::npos) {
    equation_part.erase(ellipsis_pos, kEllipsisLength);
    if (equation_part.find('.') != std::string::npos) {
      return false;
    }
  } else if (equation_part.find('.') != std::string::npos) {
    return false;
  }
  for (const auto c : equation_part) {
    if (!std::isalpha(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  if (!has_duplicated_label) {
    std::set<char> label_set(equation_part.begin(), equation_part.end());
    has_duplicated_label = (label_set.size() != equation_part.size());
  }
  return true;
}

// 校验整条 equation 的基本合法性：
// - 输入数量匹配；
// - 输入/输出片段合法；
// - 不允许“输入和输出同时出现重复标签”。
bool CheckEquation(const std::string &equation, const size_t input_num, std::vector<std::string> &input_equation_parts,
                   std::string &output_equation) {
  std::string input_equation;
  bool input_has_duplicated_label = false;
  bool output_has_duplicated_label = false;
  if (!SplitEquation(equation, input_equation, output_equation)) {
    return false;
  }
  SplitInputEquation(input_equation, input_equation_parts);
  if ((input_equation_parts.size() != input_num) || input_equation_parts.empty()) {
    return false;
  }
  for (const auto &input_equation_part : input_equation_parts) {
    if (input_equation_part.empty()) {
      return false;
    }
    if (!CheckEllipsisAndDuplicatedLabel(input_equation_part, input_has_duplicated_label)) {
      return false;
    }
  }
  if (!CheckEllipsisAndDuplicatedLabel(output_equation, output_has_duplicated_label)) {
    return false;
  }
  if (input_has_duplicated_label && output_has_duplicated_label) {
    return false;
  }
  return true;
}

// 合并 ellipsis 对应维度时使用广播规则（仅 ellipsis 允许广播）：
// - 相等：保留；
// - 任一为 1：可广播；
// - 其余情况：报错。
graphStatus MergeBroadcastDim(const Expression &new_dim, Expression &cur_dim) {
  if (EXPECT_SYMBOL_EQ(cur_dim, new_dim)) {
    if (new_dim.IsConstExpr()) {
      cur_dim = new_dim;
    }
    return GRAPH_SUCCESS;
  }
  if (EXPECT_SYMBOL_EQ(new_dim, kSymbolOne)) {
    return GRAPH_SUCCESS;
  }
  if (EXPECT_SYMBOL_EQ(cur_dim, kSymbolOne)) {
    cur_dim = new_dim;
    return GRAPH_SUCCESS;
  }
  GELOGE(PARAM_INVALID, "Cannot broadcast dim[%s] with dim[%s].", cur_dim.Serialize().get(), new_dim.Serialize().get());
  return PARAM_INVALID;
}

// 合并普通标签维度时使用“严格相等”规则：
// 与 host InsertLabelToMap 保持一致，不允许 1 维广播。
graphStatus MergeEqualDim(const Expression &new_dim, Expression &cur_dim) {
  if (EXPECT_SYMBOL_EQ(cur_dim, new_dim)) {
    if (new_dim.IsConstExpr()) {
      cur_dim = new_dim;
    }
    return GRAPH_SUCCESS;
  }
  GELOGE(PARAM_INVALID, "Dim[%s] does not match dim[%s].", cur_dim.Serialize().get(), new_dim.Serialize().get());
  return PARAM_INVALID;
}

// 将标签映射到维度表达式。
// 如果标签已存在，则走严格相等合并。
graphStatus InsertLabelToMap(const char label, const Expression &shape, std::map<char, Expression> &label_map) {
  if (!std::isalpha(static_cast<unsigned char>(label))) {
    GELOGE(PARAM_INVALID, "Label[%c] is invalid.", label);
    return PARAM_INVALID;
  }
  auto iter = label_map.find(label);
  if (iter == label_map.end()) {
    (void)label_map.emplace(label, shape);
    return GRAPH_SUCCESS;
  }
  return MergeEqualDim(shape, iter->second);
}

// 无 ellipsis 输入：输入 rank 必须与 equation 片段长度相等，再逐标签建图。
graphStatus MapInputWithoutEllipsis(const std::string &equation_part, const gert::SymbolShape *input_shape,
                                    std::map<char, Expression> &label_map) {
  if (input_shape->GetDimNum() != equation_part.size()) {
    GELOGE(PARAM_INVALID, "Input dim num[%zu] not match equation[%s].", input_shape->GetDimNum(), equation_part.c_str());
    return PARAM_INVALID;
  }
  for (size_t i = 0U; i < equation_part.size(); ++i) {
    GE_ASSERT_GRAPH_SUCCESS(InsertLabelToMap(equation_part[i], input_shape->GetDim(i), label_map));
  }
  return GRAPH_SUCCESS;
}

/*
 * 含 ellipsis 输入：
 * - 先映射 ellipsis 前缀标签；
 * - 抽取 ellipsis 真实维度列表；
 * - 再映射 ellipsis 后缀标签。
 * 各输入的 ellipsis 长度必须一致。
 */
graphStatus MapInputWithEllipsis(const std::string &equation_part, const gert::SymbolShape *input_shape,
                                 std::map<char, Expression> &label_map,
                                 std::vector<std::vector<Expression>> &ellipsis_inputs) {
  const auto ellipsis_pos = equation_part.find(kEllipsis);
  GE_ASSERT_TRUE(ellipsis_pos != std::string::npos, "Input equation[%s] has no ellipsis.", equation_part.c_str());
  const auto shape_dim_num = input_shape->GetDimNum();
  const auto equation_len = equation_part.size();
  GE_ASSERT_TRUE((shape_dim_num + kEllipsisLength) >= equation_len, "Input dim num[%zu] mismatch equation[%s].",
                 shape_dim_num, equation_part.c_str());
  const auto ellipsis_dim_num = shape_dim_num + kEllipsisLength - equation_len;
  for (size_t i = 0U; i < ellipsis_pos; ++i) {
    GE_ASSERT_GRAPH_SUCCESS(InsertLabelToMap(equation_part[i], input_shape->GetDim(i), label_map));
  }
  std::vector<Expression> ellipsis_dims;
  ellipsis_dims.reserve(ellipsis_dim_num);
  for (size_t i = 0U; i < ellipsis_dim_num; ++i) {
    ellipsis_dims.emplace_back(input_shape->GetDim(ellipsis_pos + i));
  }
  if (!ellipsis_inputs.empty()) {
    GE_ASSERT_TRUE(ellipsis_inputs.front().size() == ellipsis_dims.size(),
                   "Ellipsis dim num mismatch: %zu vs %zu.", ellipsis_inputs.front().size(), ellipsis_dims.size());
  }
  ellipsis_inputs.emplace_back(std::move(ellipsis_dims));
  for (size_t i = ellipsis_pos + kEllipsisLength; i < equation_len; ++i) {
    const auto shape_idx = i + ellipsis_dim_num - kEllipsisLength;
    GE_ASSERT_GRAPH_SUCCESS(InsertLabelToMap(equation_part[i], input_shape->GetDim(shape_idx), label_map));
  }
  return GRAPH_SUCCESS;
}

// 把多输入的 ellipsis 维度合并为输出 ellipsis 维度（按广播规则）。
graphStatus MergeEllipsisDims(const std::vector<std::vector<Expression>> &ellipsis_inputs,
                              std::vector<Expression> &ellipsis_output) {
  ellipsis_output.clear();
  if (ellipsis_inputs.empty()) {
    return GRAPH_SUCCESS;
  }
  ellipsis_output = ellipsis_inputs.front();
  for (size_t i = 1U; i < ellipsis_inputs.size(); ++i) {
    GE_ASSERT_TRUE(ellipsis_inputs[i].size() == ellipsis_output.size(), "Ellipsis size mismatch.");
    for (size_t j = 0U; j < ellipsis_output.size(); ++j) {
      GE_ASSERT_GRAPH_SUCCESS(MergeBroadcastDim(ellipsis_inputs[i][j], ellipsis_output[j]));
    }
  }
  return GRAPH_SUCCESS;
}

// 输出标签不能引入输入中未出现的新标签。
bool CheckOutputNewLabel(const std::string &output_equation, const std::map<char, Expression> &label_map) {
  std::string output_equation_backup = output_equation;
  const auto ellipsis_pos = output_equation_backup.find(kEllipsis);
  if (ellipsis_pos != std::string::npos) {
    output_equation_backup.erase(ellipsis_pos, kEllipsisLength);
  }
  for (const auto label : output_equation_backup) {
    if (label_map.find(label) == label_map.end()) {
      return false;
    }
  }
  return true;
}

// 输出不含 ellipsis：按标签顺序直接拼接输出 shape。
void AppendOutputWithoutEllipsis(gert::SymbolShape *output_shape, const std::string &output_equation,
                                 const std::map<char, Expression> &label_map) {
  for (const auto label : output_equation) {
    output_shape->AppendDim(label_map.at(label));
  }
}

// 输出含 ellipsis：前缀标签 + ellipsis 维度 + 后缀标签。
void AppendOutputWithEllipsis(gert::SymbolShape *output_shape, const std::string &output_equation,
                              const std::map<char, Expression> &label_map,
                              const std::vector<Expression> &ellipsis_output) {
  const auto ellipsis_pos = output_equation.find(kEllipsis);
  for (size_t i = 0U; i < ellipsis_pos; ++i) {
    output_shape->AppendDim(label_map.at(output_equation[i]));
  }
  for (const auto &dim : ellipsis_output) {
    output_shape->AppendDim(dim);
  }
  for (size_t i = ellipsis_pos + kEllipsisLength; i < output_equation.size(); ++i) {
    output_shape->AppendDim(label_map.at(output_equation[i]));
  }
}

/**
 * Einsum 算子的符号化shape推导
 * 【算子功能】实现爱因斯坦求和约定，根据equation表达式对输入张量进行收缩运算
 *            输入：多个张量（最多2个）
 *            输出：单个张量，形状由equation决定
 * 【算子约束】
 *      1. 输入张量数量不超过2个
 *      2. equation表达式必须合法，输出标签不能包含输入中未出现的新标签
 *      3. attr N必须与实际输入数量匹配
 *      4. 支持省略号(...)表示任意数量的维度
 * 【推导逻辑】
 *      1. 获取并校验equation和N属性
 *      2. 解析equation，分离输入输出部分
 *      3. 映射输入张量的维度标签到表达式
 *      4. 合并多个输入的ellipsis维度
 *      5. 根据输出equation拼装输出形状
 *      举例：输入equation="ab,bc->ac"，input0_shape=(2,3)，input1_shape=(3,4)，
 *            则output_shape=(2,4)
 */
graphStatus InferShape4Einsum(gert::InferSymbolShapeContext *context) {
  GE_ASSERT_NOTNULL(context);
  const auto attrs = context->GetAttrs();
  GE_ASSERT_NOTNULL(attrs);
  const auto *equation_ptr = attrs->GetAttrPointer<char>(kEquationIdxEinsum);
  GE_ASSERT_NOTNULL(equation_ptr);
  const auto *n_ptr = attrs->GetAttrPointer<int64_t>(kNIdxEinsum);
  GE_ASSERT_NOTNULL(n_ptr);
  const auto input_num = context->GetComputeNodeInputNum();
  GE_ASSERT_TRUE(input_num <= kInputNumLimitEinsum, "Einsum input num[%zu] should not exceed 2.", input_num);
  GE_ASSERT_TRUE(*n_ptr >= 0, "Einsum attr N[%ld] is invalid.", *n_ptr);
  GE_ASSERT_TRUE(static_cast<size_t>(*n_ptr) == input_num, "Einsum attr N[%ld] mismatch input num[%zu].", *n_ptr, input_num);

  std::string equation(equation_ptr);
  // host 对齐：先清理空格，防止 equation 解析歧义。
  TrimWhitespace(equation);
  std::vector<std::string> input_equation_parts;
  std::string output_equation;
  GE_ASSERT_TRUE(CheckEquation(equation, input_num, input_equation_parts, output_equation), "Einsum equation[%s] is invalid.",
                 equation.c_str());

  std::map<char, Expression> label_map;
  std::vector<std::vector<Expression>> ellipsis_inputs;
  for (size_t i = 0U; i < input_num; ++i) {
    const auto input_shape = context->GetInputSymbolShape(i);
    GE_UNSUPPORTED_IF_NULL(input_shape);
    // 输入含 ellipsis 与不含 ellipsis 分开处理，保证索引映射清晰。
    if (HasEllipsis(input_equation_parts[i])) {
      GE_ASSERT_GRAPH_SUCCESS(MapInputWithEllipsis(input_equation_parts[i], input_shape, label_map, ellipsis_inputs));
      continue;
    }
    GE_ASSERT_GRAPH_SUCCESS(MapInputWithoutEllipsis(input_equation_parts[i], input_shape, label_map));
  }
  GE_ASSERT_TRUE(CheckOutputNewLabel(output_equation, label_map),
                 "Einsum output equation[%s] contains new label.", output_equation.c_str());

  std::vector<Expression> ellipsis_output;
  GE_ASSERT_GRAPH_SUCCESS(MergeEllipsisDims(ellipsis_inputs, ellipsis_output));
  const auto output_shape = context->GetOutputSymbolShape(kOutputIdxEinsum);
  GE_ASSERT_NOTNULL(output_shape);
  output_shape->MutableDims().clear();
  // 输出有 ellipsis 时，要求输入也必须能得到 ellipsis 维度。
  if (HasEllipsis(output_equation)) {
    AppendOutputWithEllipsis(output_shape, output_equation, label_map, ellipsis_output);
    return GRAPH_SUCCESS;
  }
  GE_ASSERT_TRUE(ellipsis_output.empty(), "Output equation[%s] should contain ellipsis.", output_equation.c_str());
  AppendOutputWithoutEllipsis(output_shape, output_equation, label_map);
  return GRAPH_SUCCESS;
}

IMPL_OP_INFER_SYMBOL_SHAPE_INNER(Einsum).InferSymbolShape(InferShape4Einsum);
}  // namespace
}  // namespace ge