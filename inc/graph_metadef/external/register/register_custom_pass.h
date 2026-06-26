/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef INC_EXTERNAL_REGISTER_REGISTER_PASS_H_
#define INC_EXTERNAL_REGISTER_REGISTER_PASS_H_

#include <functional>
#include <memory>
#include <string>

#include "graph/graph.h"
#include "external/ge_common/ge_api_error_codes.h"
#include "register/register_types.h"

namespace ge {
class PassRegistrationDataImpl;
class CustomPassContext;
class CustomPassContextImpl;
class StreamPassContext;
class StreamPassContextImpl;
using ConstGraphPtr = std::shared_ptr<const Graph>;
using CustomPassFunc = std::function<Status(ge::GraphPtr &, CustomPassContext &)>;
using CustomAllocateStreamPassFunc = std::function<Status(const ConstGraphPtr &, StreamPassContext &)>;
constexpr int64_t INVALID_STREAM_ID = -1;

/**
 * 自定义pass执行阶段，若需扩展，请在kInvalid之前添加
 * @since 8.5.0(2025-12)
 */
enum class CustomPassStage : uint32_t {
  kBeforeInferShape = 0,          // @since 8.5.0(2025-12)
  kAfterInferShape = 1,           // @since 8.5.0(2025-12)
  kAfterAssignLogicStream = 2,    // @since 8.5.0(2025-12) only support CustomAllocateStreamPassFunc in this stage
  kAfterBuiltinFusionPass = 3,    // @since 8.5.0(2025-12)
  kAfterOriginGraphOptimize = 4,  // @since 9.0.0(2026-02)
  kCompatibleInherited = 5,       // @since 9.0.0(2026-02)
  kInvalid                        // @since 8.5.0(2025-12)
};

/**
 * @since 8.5.0(2025-12)
 */
class FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY PassRegistrationData {
 public:
  /**
   * @since 8.5.0(2025-12)
   */
  PassRegistrationData() = default;
  /**
   * @since 8.5.0(2025-12)
   */
  ~PassRegistrationData() = default;

  /**
   * 通过pass名称构造注册数据
   * @param pass_name 自定义pass的名称
   * @since 8.5.0(2025-12)
   */
  PassRegistrationData(std::string pass_name);

  /**
   * 注册自定义pass的执行函数
   * @param custom_pass_fn 自定义pass的执行回调
   * @return 自身引用，支持链式调用
   * @since 8.5.0(2025-12)
   */
  PassRegistrationData &CustomPassFn(const CustomPassFunc &custom_pass_fn);

  /**
   * 获取自定义pass的名称
   * @return pass名称
   * @since 8.5.0(2025-12)
   */
  std::string GetPassName() const;

  /**
   * 获取自定义pass的执行函数
   * @return 自定义pass执行回调
   * @since 8.5.0(2025-12)
   */
  CustomPassFunc GetCustomPassFn() const;

  /**
   * 设置自定义pass的执行阶段
   * @param stage pass执行阶段
   * @return 自身引用，支持链式调用
   * @since 8.5.0(2025-12)
   */
  PassRegistrationData &Stage(const CustomPassStage stage);

  /**
   * 获取自定义pass所属的执行阶段
   * @return pass执行阶段
   * @since 8.5.0(2025-12)
   */
  CustomPassStage GetStage() const;

  /**
   * 注册自定义流分配pass的执行函数
   * @param allocate_stream_pass_fn 自定义流分配pass的执行回调
   * @return 自身引用，支持链式调用
   * @since 8.5.0(2025-12)
   */
  PassRegistrationData &CustomAllocateStreamPassFn(const CustomAllocateStreamPassFunc &allocate_stream_pass_fn);

  /**
   * 获取自定义流分配pass的执行函数
   * @return 自定义流分配pass执行回调
   * @since 8.5.0(2025-12)
   */
  CustomAllocateStreamPassFunc GetCustomAllocateStreamPass() const;

 private:
  std::shared_ptr<PassRegistrationDataImpl> impl_;
};

/**
 * @since 8.5.0(2025-12)
 */
class FMK_FUNC_HOST_VISIBILITY FMK_FUNC_DEV_VISIBILITY PassReceiver {
 public:
  /**
   * 接收自定义pass注册数据，完成pass注册
   * @param reg_data pass注册数据
   * @since 8.5.0(2025-12)
   */
  PassReceiver(PassRegistrationData &reg_data);
  /**
   * @since 8.5.0(2025-12)
   */
  ~PassReceiver() = default;
};

/**
 * @since 8.5.0(2025-12)
 */
class CustomPassContext {
 public:
  /**
   * @since 8.5.0(2025-12)
   */
  CustomPassContext();
  /**
   * @since 8.5.0(2025-12)
   */
  virtual ~CustomPassContext();

  /**
   * 设置pass执行过程中的错误信息
   * @param error_message 错误信息
   * @since 8.5.0(2025-12)
   */
  void SetErrorMessage(const AscendString &error_message);

  /**
   * 设置pass名称
   * @param pass_name pass名称
   * @since 9.0.0(2026-02)
   */
  void SetPassName(const AscendString &pass_name);

  /**
   * 获取pass执行过程中的错误信息
   * @return 错误信息
   * @since 8.5.0(2025-12)
   */
  AscendString GetErrorMessage() const;

  /**
   * 获取pass名称
   * @return pass名称
   * @since 9.0.0(2026-02)
   */
  AscendString GetPassName() const;

  /**
   * 通过option的key，从上下文中获取option的值
   * 若option key不存在，返回失败
   * @param option_key
   * @param option_value 出参
   * @return graphStatus
   * @since 9.0.0(2026-01)
   */
  graphStatus GetOptionValue(const AscendString &option_key, AscendString &option_value) const;

 private:
  std::unique_ptr<CustomPassContextImpl> impl_;
};

/**
 * @since 8.5.0(2025-12)
 */
class StreamPassContext : public CustomPassContext {
 public:
  /**
   * @since 8.5.0(2025-12)
   */
  explicit StreamPassContext(int64_t current_max_stream_id);

  /**
   * @since 8.5.0(2025-12)
   */
  ~StreamPassContext() override = default;

  /**
   * 为指定节点设置流ID
   * @param node 目标节点
   * @param stream_id 要设置的流ID
   * @return graphStatus
   * @since 8.5.0(2025-12)
   */
  graphStatus SetStreamId(const GNode &node, int64_t stream_id);

  /**
   * 获取指定节点的流ID
   * @param node 目标节点
   * @return 节点对应的流ID，未设置时返回INVALID_STREAM_ID
   * @since 8.5.0(2025-12)
   */
  int64_t GetStreamId(const GNode &node) const;

  /**
   * 分配下一个可用的流ID
   * @return 新分配的流ID
   * @since 8.5.0(2025-12)
   */
  int64_t AllocateNextStreamId();

  /**
   * 获取当前已分配的最大流ID
   * @return 当前最大流ID
   * @since 8.5.0(2025-12)
   */
  int64_t GetCurrMaxStreamId() const;

 private:
  std::unique_ptr<StreamPassContextImpl> impl_;
};
}  // namespace ge

#define REGISTER_CUSTOM_PASS(name) REGISTER_CUSTOM_PASS_UNIQ_HELPER(__COUNTER__, (name))
#define REGISTER_CUSTOM_PASS_UNIQ_HELPER(ctr, name) REGISTER_CUSTOM_PASS_UNIQ(ctr, (name))
#define REGISTER_CUSTOM_PASS_UNIQ(ctr, name) \
  static ::ge::PassReceiver register_pass##ctr __attribute__((unused)) = ::ge::PassRegistrationData((name))

#endif  // INC_EXTERNAL_REGISTER_REGISTER_PASS_H_
