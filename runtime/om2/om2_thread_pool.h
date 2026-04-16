/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_RUNTIME_OM2_OM2_THREAD_POOL_H_
#define GE_RUNTIME_OM2_OM2_THREAD_POOL_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "framework/common/debug/ge_log.h"
#include "ge/ge_api_error_codes.h"

namespace ge {
namespace om2 {
using ThreadTask = std::function<void()>;

class ThreadPool {
 public:
  explicit ThreadPool(std::string thread_name_prefix, uint32_t size = 4U);
  ~ThreadPool();
  void Destroy();

  template <class Func, class... Args>
  auto commit(Func &&func, Args &&... args) -> std::future<decltype(func(args...))> {
    GELOGD("[OM2] commit run task enter.");
    using retType = decltype(func(args...));
    std::future<retType> fail_future;
    if (is_stoped_.load()) {
      GELOGE(ge::FAILED, "[OM2] thread pool has been stopped.");
      return fail_future;
    }

    const auto bind_func = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
    const auto task = std::make_shared<std::packaged_task<retType()>>(bind_func);
    if (task == nullptr) {
      GELOGE(ge::FAILED, "[OM2] Make shared failed.");
      return fail_future;
    }
    std::future<retType> future = task->get_future();
    {
      const std::lock_guard<std::mutex> lock{m_lock_};
      (void)tasks_.emplace([task]() { (*task)(); });
    }
    cond_var_.notify_one();
    GELOGD("[OM2] commit run task end.");
    return future;
  }

  static void ThreadFunc(ThreadPool *thread_pool, uint32_t thread_idx);

 private:
  std::string thread_name_prefix_;
  std::vector<std::thread> pool_;
  std::queue<ThreadTask> tasks_;
  std::mutex m_lock_;
  std::condition_variable cond_var_;
  std::atomic<bool> is_stoped_;
  std::atomic<uint32_t> idle_thrd_num_;
};
}  // namespace om2
}  // namespace ge

#endif  // GE_RUNTIME_OM2_OM2_THREAD_POOL_H_
