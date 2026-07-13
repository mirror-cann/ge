/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef METADEF_CXX_INC_GRAPH_BASE_CUSTOM_OP_H
#define METADEF_CXX_INC_GRAPH_BASE_CUSTOM_OP_H
#include "exe_graph/runtime/annotated_args_context.h"
#include "exe_graph/runtime/eager_op_execution_context.h"
#include <functional>
#include <memory>
#include <vector>
#include "graph/ge_error_codes.h"
#include "exe_graph/runtime/infer_datatype_context.h"
#include "exe_graph/runtime/infer_shape_context.h"
#include "exe_graph/runtime/op_compile_context.h"
#include "exe_graph/runtime/update_args_context.h"

namespace ge {
/**
 * 自定义算子能力接口的公共基类。
 * 用户可按需组合继承 CompilableOp、EagerExecuteOp、ShapeInferOp，
 * 以声明算子支持的构图编译、Eager 执行和推理能力。
 */
class BaseCustomOp {
 public:
  virtual ~BaseCustomOp() = default;
};

class PortableOp : virtual public BaseCustomOp {
 public:
  /**
   * 序列化自定义算子的 kernel bin 数据
   * @param buffer 输出的二进制数据，由算子自定义格式，GE 不解析只透传
   * @return 状态码，默认实现返回 GRAPH_SUCCESS
   */
  virtual graphStatus Serialize(std::vector<uint8_t> &buffer) = 0;

  /**
   * 反序列化自定义算子的 kernel bin 数据
   * @param buffer 输入的二进制数据
   * @return 状态码，默认实现返回 GRAPH_SUCCESS
   */
  virtual graphStatus Deserialize(const std::vector<uint8_t> &buffer) = 0;

  ~PortableOp() override = default;
};

/**
 * 自定义算子的构图编译接口。
 * 当算子进入 GE 构图编译流程后，若实现该接口，GE 会回调 Compile
 * 完成算子编译相关处理。
 */
class CompilableOp : virtual public BaseCustomOp {
 public:
  ~CompilableOp() override = default;
  /**
   * 自定义算子及时编译函数
   * @param ctx 算子编译上下文
   * @return 状态码
   */
  virtual graphStatus Compile(gert::OpCompileContext *ctx) = 0;
};

/**
 * 自定义算子的 Eager 执行接口。
 * 适用于算子基于运行时上下文执行的场景。
 */
class EagerExecuteOp : virtual public BaseCustomOp {
 public:
  ~EagerExecuteOp() override = default;
  /**
   * 自定义算子的执行函数
   * @param ctx 执行时上下文，可通过上下文获取input tensor，分配输出内存，分配workspace等
   * @return 状态码
   */
  virtual graphStatus Execute(gert::EagerOpExecutionContext *ctx) = 0;
};

/**
 * 自定义算子的 kernel launch 参数声明接口。
 * 用于自定义算子在编译期通过 DeclareLaunchArgs 声明 kernel launch 所需的参数。
 * 适用于生成端侧离线模型。
 * @since 9.2.0(2026-07)
 */
class AnnotatedArgsOp : virtual public BaseCustomOp {
 public:
  ~AnnotatedArgsOp() override = default;
  /**
   * 声明 kernel launch 参数，编译期调用。
   * 当前端侧离线模型的 TaskDef 生成仅支持单次 AddLaunch 和主 stream launch。
   * @param ctx 声明式参数上下文，可通过 ctx 分配 workspace、获取输入/输出 tensor、添加 launch 任务
   * @return GRAPH_SUCCESS 表示声明成功，否则返回错误码
   * @since 9.2.0(2026-07)
   */
  virtual graphStatus DeclareLaunchArgs(gert::AnnotatedArgsContext &ctx) = 0;
};

/**
 * 自定义算子的  Args 刷新能力接口。
 * 继承此接口的算子会在 I/O 地址变化时被框架回调 UpdateHostArgs。
 */
class ArgsUpdater : virtual public BaseCustomOp {
 public:
  ~ArgsUpdater() override = default;
  /**
   * @param ctx UpdateArgsContext，可通过上下文获取更新后的 I/O 地址和 args buffer
   * @return 状态码，GRAPH_FAILED 将终止后续刷新
   */
  virtual graphStatus UpdateHostArgs(gert::UpdateArgsContext *ctx) = 0;
};

/**
 * 自定义算子的 Shape 推理接口。
 * 适用于算子基于推理上下文执行形状和数据类型推导的场景。
 */
class ShapeInferOp : virtual public BaseCustomOp {
 public:
  ~ShapeInferOp() override = default;
  /**
   * 形状推理函数，用于推导算子输出的形状
   * @param ctx 形状推理上下文，可通过上下文获取输入张量形状，设置输出张量形状等
   * @return 状态码
   */
  virtual graphStatus InferShape(gert::InferShapeContext *ctx) = 0;
  /**
   * 数据类型推理函数，用于推导算子输出的数据类型
   * @param ctx 数据类型推理上下文，可通过上下文获取输入张量数据类型，设置输出张量数据类型等
   * @return 状态码
   */
  virtual graphStatus InferDataType(gert::InferDataTypeContext *ctx) = 0;
};

using BaseOpCreator = std::function<std::unique_ptr<BaseCustomOp>()>;
using CustomOpCreateFunc = ge::BaseCustomOp *(*)();

/**
 * 自定义算子创建器注册辅助类。
 * 通常配合 REG_AUTO_MAPPING_OP 宏静态注册算子类型和创建函数。
 */
class CustomOpCreatorRegister {
 public:
  CustomOpCreatorRegister(const AscendString &operator_type, const BaseOpCreator &op_creator);
  CustomOpCreatorRegister(const AscendString &operator_type, const CustomOpCreateFunc op_creator);
  ~CustomOpCreatorRegister() = default;
};
}  // namespace ge

#define REG_JOIN(g_register, y) g_register##y
#define REG_AUTO_MAPPING_OP(custom_op_class) REG_AUTO_MAPPING_OP_UNIQ(__COUNTER__, custom_op_class)
#define REG_AUTO_MAPPING_OP_UNIQ(ctr, custom_op_class)                                         \
  static ge::BaseCustomOp *REG_JOIN(custom_op_pull_creator, ctr)() {                           \
    return new custom_op_class();                                                              \
  }                                                                                            \
  static const ge::CustomOpCreatorRegister REG_JOIN(custom_op_register, ctr)(#custom_op_class, \
                                                                             REG_JOIN(custom_op_pull_creator, ctr))

#endif  // METADEF_CXX_INC_GRAPH_BASE_CUSTOM_OP_H
