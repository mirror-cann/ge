/**
 * Copyright (c) Huawei Technologies Co., Ltd. 2026. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include "es_all_ops.h"
#include "ge/fusion/pass/pattern_fusion_pass.h"

using namespace ge;
using namespace fusion;
// |o>-----------------------------------
// |o>   x   y                x   y  Const
// |o>    \ /                  \  |  /
// |o>   MatMul  Const  ==>     GEMM
// |o>       \    /
// |o>        Add
// |o>-----------------------------------
// 融合说明：本例识别上图左侧 MatMul+Add(Const) 子图，在满足 PatternMatcherConfig（常量值匹配、IR
// 属性匹配）条件时，将其替换为 GEMM 节点
class MatmulAddFusionPass : public PatternFusionPass {
  // 重写构造函数，使能const值匹配能力与ir属性及其值匹配能力
 public:
  explicit MatmulAddFusionPass()
      : PatternFusionPass(PatternMatcherConfigBuilder().EnableConstValueMatch().EnableIrAttrMatch().Build()) {}

 protected:
  std::vector<PatternUniqPtr> Patterns() override {
    std::cout << "Define pattern for MatmulAddFusionPass in matcher config sample" << std::endl;
    std::vector<PatternUniqPtr> patterns;
    auto graph_builder0 = es::EsGraphBuilder("pattern0");
    auto [x0, y0] = graph_builder0.CreateInputs<2>();
    auto z0 = graph_builder0.CreateConst(std::vector<float>{0.1f, 0.1f, 0.1f, 0.1f}, std::vector<int64_t>{2, 2});
    auto matmul0 = es::MatMul(x0, y0, nullptr, false, false);
    auto add0 = es::Add(matmul0, z0);
    auto graph0 = graph_builder0.BuildAndReset({add0});
    auto pattern0 = std::make_unique<Pattern>(std::move(*graph0));
    patterns.emplace_back(std::move(pattern0));

    auto graph_builder1 = es::EsGraphBuilder("pattern1");
    auto [x1, y1] = graph_builder1.CreateInputs<2>();
    auto z1 = graph_builder1.CreateConst(std::vector<float>{0.1f, 0.1f, 0.1f, 0.1f}, std::vector<int64_t>{2, 2});
    auto matmul1 = es::BatchMatMulV2(x1, y1);
    auto add1 = es::Add(matmul1, z1);
    auto graph1 = graph_builder1.BuildAndReset({add1});
    auto pattern1 = std::make_unique<Pattern>(std::move(*graph1));
    patterns.emplace_back(std::move(pattern1));
    return patterns;
  }
  GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) override {
    std::cout << "Define replacement for MatmulAddFusionPass in matcher config sample" << std::endl;
    auto replace_graph_builder = es::EsGraphBuilder("replacement");
    auto [r_a, r_b] = replace_graph_builder.CreateInputs<2>();
    auto r_c =
        replace_graph_builder.CreateConst(std::vector<float>{0.1f, 0.1f, 0.1f, 0.1f}, std::vector<int64_t>{2, 2});
    auto alpha_const = replace_graph_builder.CreateScalar(1);
    auto beta_const = replace_graph_builder.CreateScalar(1);
    auto gemm = es::GEMM(r_a, r_b, r_c, alpha_const, beta_const);
    return replace_graph_builder.BuildAndReset({gemm});
  }
};
REG_FUSION_PASS(MatmulAddFusionPass).Stage(CustomPassStage::kBeforeInferShape);
