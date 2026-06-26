/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_EXTERNAL_GE_GRAPH_FUSION_DECOMPOSE_PASS_H
#define INC_EXTERNAL_GE_GRAPH_FUSION_DECOMPOSE_PASS_H
#include "graph/graph.h"
#include "ge_common/ge_common_api_types.h"
#include "fusion_base_pass.h"
#include "fusion_pass_reg.h"
namespace ge {
namespace fusion {
using GraphUniqPtr = std::unique_ptr<Graph>;
/**
 * @since 8.5.0(2025-12)
 */
class DecomposePass : public FusionBasePass {
 public:
  /**
   * 通过算子类型列表构造DecomposePass，注册时声明的op type用于在图中匹配待替换的节点
   * @param op_types 待匹配的算子类型列表
   * @since 8.5.0(2025-12)
   */
  explicit DecomposePass(const std::vector<AscendString> &op_types);
  /**
   * 使用注册时声明的op type，找到图中匹配的node
   * 对匹配到的node, 依次调用子类定义的MeetRequirements函数，判断是否可被替换
   * 获取到子类定义的replacement，对图中匹配到节点依次进行替换
   *
   * 注意: Run函数只处理当前图，若需要处理子图，由Pass调用者来负责
   * @param graph
   * @return
   * @since 8.5.0(2025-12)
   */
  Status Run(GraphPtr &graph, CustomPassContext &pass_context) override;

  /**
   * @since 8.5.0(2025-12)
   */
  virtual ~DecomposePass() = default;

 protected:
  /**
   * 通过对匹配到的node进行条件判断，符合条件返回true
   * 该函数为匹配节点的过滤器
   * @param matched_node
   * @return
   * @since 8.5.0(2025-12)
   */
  virtual bool MeetRequirements(const GNode &matched_node);
  /**
   * 定义替换结构
   * @param matched_node 目标图中匹配到的真实节点，携带shape/format等信息
   * @return
   * @since 8.5.0(2025-12)
   */
  virtual GraphUniqPtr Replacement(const GNode &matched_node) = 0;

 private:
  std::vector<AscendString> op_types_;
};

/**
 * DecomposePass V2：在 V1 基础上把 CustomPassContext 透传到 MeetRequirements / Replacement，
 * 子类可在钩子内读取配置项或写入错误信息。
 *
 * 与 V1 共用 REG_DECOMPOSE_PASS 注册（工厂返回 FusionBasePass*，对版本无感知）。V1 与 V2 的
 * 匹配/替换主循环共享同一份实现（compiler/graph/fusion/pass/decompose_pass_run.h）。
 */
/**
 * @since 9.1.0(2026-05)
 */
class DecomposePassV2 : public FusionBasePass {
 public:
  /**
   * 通过算子类型列表构造DecomposePassV2，与V1行为一致，但MeetRequirements/Replacement钩子可透传CustomPassContext
   * @param op_types 待匹配的算子类型列表
   * @since 9.1.0(2026-05)
   */
  explicit DecomposePassV2(const std::vector<AscendString> &op_types);
  /**
   * @since 9.1.0(2026-05)
   */
  ~DecomposePassV2() override = default;

  /**
   * 执行pass的主函数，与V1行为一致，但MeetRequirements/Replacement钩子可透传CustomPassContext
   * @param graph 目标图
   * @param pass_context 上下文，可用于传递error msg等信息
   * @return Status
   * @since 9.1.0(2026-05)
   */
  Status Run(GraphPtr &graph, CustomPassContext &pass_context) override;

 protected:
  /**
   * 对匹配节点做条件校验，返回 true 表示允许替换。默认返回 true。
   * 相比 V1，本钩子可读取 pass_context 获取配置项 / 写入错误信息后再决策。
   * @since 9.1.0(2026-05)
   */
  virtual bool MeetRequirements(const GNode &matched_node, CustomPassContext &pass_context);

  /**
   * 返回与 matched_node 对应的替换子图。子类可向 pass_context 写入错误信息后返回 nullptr 终止替换。
   * @since 9.1.0(2026-05)
   */
  virtual GraphUniqPtr Replacement(const GNode &matched_node, CustomPassContext &pass_context) = 0;

 private:
  std::vector<AscendString> op_types_;
};

/**
 * @since 8.5.0(2025-12)
 */
#define REG_DECOMPOSE_PASS(pass_class, decompose_op_types) \
  REG_DECOMPOSE_PASS_UNIQ_HELPER(__COUNTER__, #pass_class, pass_class, decompose_op_types)
#define REG_DECOMPOSE_PASS_UNIQ_HELPER(ctr, pass_name, pass_class, decompose_op_types) \
  REG_DECOMPOSE_PASS_UNIQ(ctr, pass_name, pass_class, decompose_op_types)
#define REG_DECOMPOSE_PASS_UNIQ(ctr, pass_name, pass_class, decompose_op_types)                                   \
  static ::ge::fusion::PassRegistrar register_pass##ctr __attribute__((unused)) =                                 \
      ::ge::fusion::FusionPassRegistrationData((pass_name)).CreatePassFn([]() -> ::ge::fusion::FusionBasePass * { \
        return new (std::nothrow) pass_class(decompose_op_types);                                                 \
      })
}  // namespace fusion
}  // namespace ge
#endif  // INC_EXTERNAL_GE_GRAPH_FUSION_DECOMPOSE_PASS_H
