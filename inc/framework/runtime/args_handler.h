/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_INC_FRAMEWORK_RUNTIME_ARGS_HANDLER_H_
#define GE_INC_FRAMEWORK_RUNTIME_ARGS_HANDLER_H_

#include <deque>
#include "exe_graph/runtime/kernel_args.h"

namespace gert {

/// args 内存管理抽象接口（VA 分配 + H2D 拷贝）
/// 算子调用 MallocReadOnlyDevArgs 获取设备侧只读 args 地址，框架会在地址刷新周期自动更新设备侧内容，用户不可修改
class ArgsHandler {
 public:
  virtual ~ArgsHandler() = default;

  /// 为 void* buffer 分配 args 内存，框架会在地址刷新周期自动更新设备侧内容，用户不需要手动 set
  /// @param host_args Host 侧 args buffer 指针
  /// @param args_size Args buffer 大小（字节）
  /// @return 设备侧 KernelArgs 指针（VA 地址），失败返回 nullptr
  virtual const KernelArgs* MallocReadOnlyDevArgs(void *host_args, size_t args_size) = 0;

  /// 获取所有 KernelArgs（用于 UpdateHostArgs）
  /// @param placement kPlacementHost 或 kPlacementDevice
  /// @return KernelArgs deque 的引用（支持多次 malloc，deque push_back 不失效已有元素指针）
  virtual const std::deque<KernelArgs>& GetKernelArgs(Placement placement) const = 0;
};

}  // namespace gert

#endif  // GE_INC_FRAMEWORK_RUNTIME_ARGS_HANDLER_H_
