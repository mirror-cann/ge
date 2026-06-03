/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of 
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, 
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_EXTERNAL_GE_GRAPH_PATTERN_FUSION_PASS_H
#define INC_EXTERNAL_GE_GRAPH_PATTERN_FUSION_PASS_H
#include "fusion_base_pass.h"
#include "ge/fusion/match_result.h"
#include "fusion_pass_reg.h"
#include "ge/fusion/pattern_matcher_config.h"
namespace ge {
namespace fusion {
using GraphUniqPtr = std::unique_ptr<Graph>;
using PatternUniqPtr = std::unique_ptr<Pattern>;
class PatternFusionPass : public FusionBasePass {
 public:
  /**
   * PatternFusionPass的默认构造函数
   * 通过此构造函数构造的pass，其match config中的配置项均为默认值，即不开启任何配置的状态
   */
  PatternFusionPass();

  /**
   * 通过PatternMatcherConfig来构造PatternFusionPass
   * 子类可通过传入PatternMatcherConfig使能各项匹配的选项
   * @param match_config
   */
  explicit PatternFusionPass(std::unique_ptr<PatternMatcherConfig> match_config);
  /**
   * 执行pass的主函数
   * 该函数将获取到Patterns构造的pattern graph，用来构造pattern
   * 在目标graph中逐一进行匹配，调用Requires函数对匹配结果做是否可被替换的校验，若为true，则进行进一步的替换
   * 通过调用Replacement获取到替换的目标结构，将match result对应的子图结构替换为replacement子图
   *
   * 注意: Run函数只处理当前图，若需要处理子图，由Pass调用者来负责
   * @param graph
   * @return
   */
  Status Run(GraphPtr &graph, CustomPassContext &pass_context) override;

 protected:
  /**
   * 定义一个或多个pattern，用于表达希望匹配的图结构
   * @return 多个pattern指针
   */
  virtual std::vector<PatternUniqPtr> Patterns() = 0;
  /**
   * 通过对匹配到的node进行条件判断，符合条件返回true
   * 该函数为匹配节点的过滤器
   *
   * 若子类不重新实现，默认返回true
   * @param match_result
   * @return
   */
  virtual bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result);

  /**
   * 定义替换结构，入参中传入的match_result可以用于获取输入shape，子类可对replacement进行shape推导或checksupport
   * @param match_result
   * @return
   */
virtual GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result) = 0;

  private:
  std::unique_ptr<PatternMatcherConfig> match_config_;
};

/**
 * PatternFusionPass V2：在 V1 基础上把 CustomPassContext 透传到 MeetRequirements / Replacement，
 * 子类可在钩子内读取配置项或写入错误信息。
 *
 * 与 V1 共用 REG_FUSION_PASS 注册（工厂返回 FusionBasePass*，对版本无感知）。V1 与 V2 的
 * 匹配/替换主循环共享同一份实现（compiler/graph/fusion/pass/pattern_fusion_run.h）。
 */
class PatternFusionPassV2 : public FusionBasePass {
 public:
  PatternFusionPassV2();
  explicit PatternFusionPassV2(std::unique_ptr<PatternMatcherConfig> match_config);

  Status Run(GraphPtr &graph, CustomPassContext &pass_context) override;

 protected:
  virtual std::vector<PatternUniqPtr> Patterns() = 0;

  /**
   * 对匹配结果做条件校验，返回 true 表示允许替换。默认返回 true。
   * 相比 V1，本钩子可读取 pass_context 获取配置项 / 写入错误信息后再决策。
   */
  virtual bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result,
                                CustomPassContext &pass_context);

  /**
   * 返回与 match_result 对应的替换子图。子类可向 pass_context 写入错误信息后返回 nullptr 终止替换。
   */
  virtual GraphUniqPtr Replacement(const std::unique_ptr<MatchResult> &match_result,
                                   CustomPassContext &pass_context) = 0;

 private:
  std::unique_ptr<PatternMatcherConfig> match_config_;
};

#define REG_FUSION_PASS(pass_class) REG_FUSION_PASS_UNIQ_HELPER(__COUNTER__, #pass_class, pass_class)
#define REG_FUSION_PASS_UNIQ_HELPER(ctr, pass_name, pass_class) REG_FUSION_PASS_UNIQ(ctr, pass_name, pass_class)
#define REG_FUSION_PASS_UNIQ(ctr, pass_name, pass_class)                                                          \
  static ::ge::fusion::PassRegistrar register_pass##ctr __attribute__((unused)) =                                 \
      ::ge::fusion::FusionPassRegistrationData((pass_name)).CreatePassFn([]() -> ::ge::fusion::FusionBasePass * { \
        return new (std::nothrow) pass_class();                                                                   \
      })
} // namespace fusion
} // namespace ge
#endif  // INC_EXTERNAL_GE_GRAPH_PATTERN_FUSION_PASS_H
