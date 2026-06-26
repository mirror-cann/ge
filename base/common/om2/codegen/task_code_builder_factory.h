/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_TASK_CODE_BUILDER_FACTORY_H_
#define AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_TASK_CODE_BUILDER_FACTORY_H_
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "framework/common/debug/ge_log.h"
#include "common/plugin/ge_make_unique_util.h"
#include "common/opskernel/ops_kernel_info_types.h"

namespace ge {
class TaskCodeBuilder;
class AstBuildContext;
using TaskCodeBuilderPtr = std::shared_ptr<TaskCodeBuilder>;

class TaskCodeBuilderFactory {
 public:
  using TaskCodeBuilderCreatorFun = std::function<TaskCodeBuilderPtr(AstBuildContext &)>;

  static TaskCodeBuilderFactory &Instance() {
    if (g_user_defined_instance_ != nullptr) {
      return *g_user_defined_instance_;
    }
    static TaskCodeBuilderFactory instance;
    return instance;
  }

  static void Replace(std::shared_ptr<TaskCodeBuilderFactory> ins) {
    g_user_defined_instance_ = std::move(ins);
  }

  TaskCodeBuilderPtr Create(const ModelTaskType builder_type, AstBuildContext &ast) {
    const auto iter = creator_map_.find(builder_type);
    if (iter == creator_map_.end()) {
      GELOGW("Cannot find builder type %d in inner map.", static_cast<int32_t>(builder_type));
      return nullptr;
    }

    return iter->second(ast);
  }

  class Registerar {
   public:
    Registerar(const ModelTaskType type, const TaskCodeBuilderCreatorFun &func) noexcept {
      TaskCodeBuilderFactory::Instance().RegisterCreator(type, func);
    }

    ~Registerar() = default;
  };

 private:
  TaskCodeBuilderFactory() = default;

  ~TaskCodeBuilderFactory() = default;

  void RegisterCreator(const ModelTaskType type, const TaskCodeBuilderCreatorFun &func) {
    const auto iter = creator_map_.find(type);
    if (iter != creator_map_.end()) {
      GELOGD("TaskCodeBuilderFactory::RegisterCreator: %d creator already exist", static_cast<int32_t>(type));
      return;
    }

    creator_map_[type] = func;
  }

  std::map<ModelTaskType, TaskCodeBuilderCreatorFun> creator_map_;

  inline static std::shared_ptr<TaskCodeBuilderFactory> g_user_defined_instance_ = nullptr;
};

#define REGISTER_TASK_CODE_BUILDER(type, clazz)                                                             \
  namespace {                                                                                               \
  TaskCodeBuilderPtr Creator_Task_Code_Builder_##type(AstBuildContext &ast) {                               \
    std::shared_ptr<clazz> ptr = nullptr;                                                                   \
    ptr = MakeShared<clazz>(ast);                                                                           \
    return ptr;                                                                                             \
  }                                                                                                         \
  TaskCodeBuilderFactory::Registerar g_Task_Code_Builder_Creator_##type(ModelTaskType::type,                \
                                                                        &Creator_Task_Code_Builder_##type); \
  }  // namespace
}  // namespace ge
#endif  // AIR_CXX_BASE_COMMON_OM2_CODEGEN_HANDLER_TASK_CODE_BUILDER_FACTORY_H_
