/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef RTC_KERNEL_LOADER_H
#define RTC_KERNEL_LOADER_H

#include <mutex>
#include <vector>
#include "acl/acl_rt.h"
#include "graph/custom_op.h"

/**
 * @brief RTC Kernel 加载器
 *
 * 封装 RTC (Runtime Compile) 编译和加载流程：
 * 1. 从文件加载 kernel 源码
 * 2. 使用 RTC 编译 kernel
 * 3. 加载编译后的 binary
 * 4. 获取函数句柄
 *
 * 线程安全：使用 mutex 保护 Load 操作，支持多线程并发调用
 *
 * 使用示例：
 * @code
 * RtcKernelLoader loader;
 * if (loader.Load("add_custom", "add_custom.asc") != ge::GRAPH_SUCCESS) {
 *     return ge::GRAPH_FAILED;
 * }
 * aclrtFuncHandle func = loader.GetFuncHandle();
 * // 使用 func 启动 kernel
 * @endcode
 */
class RtcKernelLoader {
 public:
  RtcKernelLoader() = default;
  ~RtcKernelLoader();

  // 禁用拷贝和赋值
  RtcKernelLoader(const RtcKernelLoader &) = delete;
  RtcKernelLoader &operator=(const RtcKernelLoader &) = delete;

  /**
   * @brief 加载并编译 kernel
   *
   * 从 source_file 加载源码，使用 RTC 编译，获取函数句柄
   * 如果已加载则直接返回成功
   * 线程安全：内部使用 mutex 保护，支持多线程并发调用
   *
   * @param kernel_name kernel 函数名称
   * @param source_file kernel 源码文件名（相对于动态库目录）
   * @return GRAPH_SUCCESS 表示成功，GRAPH_FAILED 表示失败
   */
  ge::graphStatus Load(const char *kernel_name, const char *source_file);

  /**
   * @brief 获取 kernel 函数句柄
   * @return 函数句柄，未加载时返回 nullptr
   */
  aclrtFuncHandle GetFuncHandle() const {
    return func_handle_;
  }

  /**
   * @brief 检查 kernel 是否已加载
   * @return true 表示已加载，false 表示未加载
   */
  bool IsLoaded() const {
    return func_handle_ != nullptr;
  }

  /**
   * @brief 卸载 kernel 并释放资源
   */
  void Unload();

 private:
  /**
   * @brief 获取当前动态库所在目录
   * @return 动态库目录路径，失败返回空字符串
   */
  static std::string GetCurrentLibraryDir();

  /**
   * @brief 从文件加载文本内容
   * @param file_path 文件路径
   * @return 文件内容，失败返回空字符串
   */
  static std::string LoadTextFromFile(const std::string &file_path);

  /**
   * @brief 获取 RTC 编译选项
   *
   * 从设备信息中动态获取 NPU 架构，生成编译选项
   * 例如：--npu-arch=dav-2201
   *
   * @return RTC 编译选项字符串
   */
  static std::string GetRtcCompileOption();

  ge::graphStatus CompileKernel(const char *kernel_name, const std::string &source, std::vector<char> &bin_data);

  ge::graphStatus LoadBinary(const char *kernel_name, const std::vector<char> &bin_data);

  mutable std::mutex mutex_;
  aclrtBinHandle bin_handle_ = nullptr;
  aclrtFuncHandle func_handle_ = nullptr;
};

#endif  // RTC_KERNEL_LOADER_H
