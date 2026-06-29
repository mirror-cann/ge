/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef LOG_H
#define LOG_H

#include <iostream>
#include <mutex>

/**
 * @brief 日志宏，自动附加文件名和行号
 *
 * 使用 variadic template 实现，支持逗号分隔的参数拼接
 * 线程安全：内部使用 mutex 保护输出，防止多线程日志交错
 * LOG_ERROR: 错误日志，用于致命错误
 * LOG_WARNING: 警告日志，用于非致命问题
 * LOG_INFO: 信息日志，用于一般信息输出
 */

namespace {
inline std::mutex &GetLogMutex() {
  static std::mutex log_mutex;
  return log_mutex;
}
}  // namespace

template <typename... Args>
void LogError(const char *file, int line, Args &&...args) {
  std::lock_guard<std::mutex> lock(GetLogMutex());
  std::cerr << "[ERROR] " << file << ":" << line << " ";
  (std::cerr << ... << std::forward<Args>(args)) << std::endl;
}

template <typename... Args>
void LogWarning(const char *file, int line, Args &&...args) {
  std::lock_guard<std::mutex> lock(GetLogMutex());
  std::cerr << "[WARNING] " << file << ":" << line << " ";
  (std::cerr << ... << std::forward<Args>(args)) << std::endl;
}

template <typename... Args>
void LogInfo(const char *file, int line, Args &&...args) {
  std::lock_guard<std::mutex> lock(GetLogMutex());
  std::cout << "[INFO] " << file << ":" << line << " ";
  (std::cout << ... << std::forward<Args>(args)) << std::endl;
}

#define LOG_ERROR(...) LogError(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARNING(...) LogWarning(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) LogInfo(__FILE__, __LINE__, __VA_ARGS__)

#endif  // LOG_H
