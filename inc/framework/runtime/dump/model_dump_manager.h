/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_FRAMEWORK_RUNTIME_DUMP_MODEL_DUMP_MANAGER_H_
#define GE_FRAMEWORK_RUNTIME_DUMP_MODEL_DUMP_MANAGER_H_

#include <cstdint>
#include <memory>
#include "framework/common/ge_types.h"
#include "framework/common/ge_visibility.h"
#include "framework/runtime/dump/model_dump_c_api.h"
#include "runtime/base.h"

namespace ge {
namespace dump {

// ============================================================
//                   前置声明
// ============================================================
class DataDumpImpl;
class ExceptionDumpImpl;
class OverflowDumpImpl;

// ============================================================
//              复用 C API 中的结构体定义
// ============================================================
using Om2Tensor = ::Om2Tensor;
using Om2TaskIoEntry = ::Om2TaskIoEntry;
using Om2TaskInfo = ::Om2TaskInfo;

// ============================================================
//                  ModelDumpManager 内部结构体
// ============================================================
struct ModelDumpInfo {
  uint32_t model_id;
  const char* model_name;
  const char* root_graph_name;
  uint32_t device_id;
  void* rt_model_handle;  // rtModel_t
  uint64_t step_id_addr;
  uint64_t loop_cond_addr;
  uint64_t iterations_per_loop_addr;
};


// ============================================================
//                   ModelDumpManager 统一门面
// ============================================================

class VISIBILITY_EXPORT ModelDumpManager {
 public:
  // ========================================================================
  // 静态全局接口（进程初始化时调用）
  // ========================================================================
  static Status GlobalInit();

  // ========================================================================
  // 构造/析构（每个模型一个实例）
  // ========================================================================
  explicit ModelDumpManager(uint32_t model_id);
  ~ModelDumpManager();

  // 禁用拷贝
  ModelDumpManager(const ModelDumpManager&) = delete;
  ModelDumpManager& operator=(const ModelDumpManager&) = delete;

  // ========================================================================
  // 模型级信息接口
  // ========================================================================
  Status SetModelDumpInfo(const ModelDumpInfo& model_info);

  // ========================================================================
  // Task 级信息接口
  // ========================================================================
  Status AddOm2TaskInfo(const Om2TaskInfo& task_info);

  Status PreprocessOm2TaskInfo(const Om2TaskInfo& task_info);

  Status IsDataDumpEnabled(const char* op_name, uint8_t* is_data_dump) const;

  // ========================================================================
  // Dump 下发接口
  // ========================================================================
  Status DispatchDumpInfo();

  // ========================================================================
  // 异常查询接口（与 V1 exception_dumper 接口保持一致）
  // ========================================================================
  bool GetOpDescInfo(const OpDescInfoId& op_id, ge::OpDescInfo& op_info) const;

  // ========================================================================
  // 生命周期管理
  // ========================================================================
  void Clear();

 private:
  uint32_t model_id_;
  ModelDumpInfo model_info_{};

  std::unique_ptr<DataDumpImpl> data_dump_impl_;
  std::unique_ptr<ExceptionDumpImpl> exception_impl_;
  std::unique_ptr<OverflowDumpImpl> overflow_impl_;
};

}  // namespace dump
}  // namespace ge

#endif  // GE_FRAMEWORK_RUNTIME_DUMP_MODEL_DUMP_MANAGER_H_
