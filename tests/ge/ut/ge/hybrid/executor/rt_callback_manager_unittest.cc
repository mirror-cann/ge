/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "hybrid/executor/rt_callback_manager.h"
#include <gtest/gtest.h>
#include <functional>
#include <atomic>
#include <thread>
#include <chrono>

#include "depends/ascendcl/src/ascendcl_stub.h"

namespace ge {
namespace hybrid {

class UtestRtCallbackManager : public ::testing::Test {
 protected:
  void SetUp() override {
    // 确保每次测试开始时清除 Stub 的错误模拟设置
    AclRuntimeStub::SetErrorResultApiName("");
  }

  void TearDown() override {
    AclRuntimeStub::SetErrorResultApiName("");
  }
};

// 用例 1：覆盖 CallbackProcess 中 aclrtSynchronizeEventWithTimeout 调用失败的分支
// 预期：即使同步事件超时或失败，系统记录日志后仍继续执行回调函数，且不崩溃
TEST_F(UtestRtCallbackManager, CallbackProcess_SyncEventTimeoutFailureBranchFail) {
  std::atomic<bool> callback_executed{false};
  RtCallbackManager manager;

  // 1. 打桩：使 aclrtSynchronizeEventWithTimeout 始终返回失败
  // 根据 stub 实现，匹配函数名即可触发失败返回
  AclRuntimeStub::SetErrorResultApiName("aclrtSynchronizeEventWithTimeout");

  // 2. 初始化并启动后台处理线程
  EXPECT_EQ(manager.Init(), SUCCESS);

  // 3. 注册一个流和回调函数
  // 使用非空指针作为 Stream 句柄
  aclrtStream fake_stream = reinterpret_cast<aclrtStream>(0x123456);
  manager.RegisterCallbackFunc(fake_stream, [&callback_executed]() { callback_executed.store(true); });

  // 4. 等待一段时间让后台线程处理队列中的任务
  // BlockingQueue 可能会有一定的延时
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // 5. 销毁管理器，停止线程
  EXPECT_EQ(manager.Destroy(), SUCCESS);

  // 6. 验证回调函数最终被成功执行
  // 这证明了尽管中间的 SynchronizeEvent 失败，流程并未中断
  EXPECT_TRUE(callback_executed.load());
}

TEST_F(UtestRtCallbackManager, CallbackProcess_SyncEventTimeoutFailureBranchTimeOut) {
  class MockAclRuntime : public ge::AclRuntimeStub {
   public:
    aclError aclrtSynchronizeEventWithTimeout(aclrtEvent event, int32_t timeout) override {
      return ACL_ERROR_RT_STREAM_SYNC_TIMEOUT;
    }
  };
  auto mock_runtime = std::make_shared<MockAclRuntime>();
  ge::AclRuntimeStub::SetInstance(mock_runtime);

  std::atomic<bool> callback_executed{false};
  RtCallbackManager manager;

  // 1. 打桩：使 aclrtSynchronizeEventWithTimeout 始终返回失败
  // 根据 stub 实现，匹配函数名即可触发失败返回
  AclRuntimeStub::SetErrorResultApiName("aclrtSynchronizeEventWithTimeout");

  // 2. 初始化并启动后台处理线程
  EXPECT_EQ(manager.Init(), SUCCESS);

  // 3. 注册一个流和回调函数
  // 使用非空指针作为 Stream 句柄
  aclrtStream fake_stream = reinterpret_cast<aclrtStream>(0x123456);
  manager.RegisterCallbackFunc(fake_stream, [&callback_executed]() { callback_executed.store(true); });

  // 4. 等待一段时间让后台线程处理队列中的任务
  // BlockingQueue 可能会有一定的延时
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // 5. 销毁管理器，停止线程
  EXPECT_NE(manager.Destroy(), SUCCESS);

  // 6. 验证回调函数最终被成功执行
  // 这证明了尽管中间的 SynchronizeEvent 失败，流程并未中断
  EXPECT_TRUE(callback_executed.load());
  ge::AclRuntimeStub::SetInstance(nullptr);
}

// 用例 2：覆盖 CallbackProcess 中 aclrtSetCurrentContext 失败的分支
// 预期：如果无法设置 Context，处理逻辑应终止，不会导致程序崩溃
TEST_F(UtestRtCallbackManager, CallbackProcess_SetContextFailedBranch) {
  RtCallbackManager manager;

  // 1. 打桩：使 aclrtSetCurrentContext 失败
  AclRuntimeStub::SetErrorResultApiName("aclrtSetCurrentContext");

  // 2. 初始化线程
  // 线程启动后立即因上下文设置失败而退出
  EXPECT_EQ(manager.Init(), SUCCESS);

  // 等待线程有足够的时间尝试设置 Context 并退出
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 3. 注册回调
  // 此时线程已退出，队列可能无人消费，但这不应引发异常
  aclrtStream fake_stream = reinterpret_cast<aclrtStream>(0x123456);
  EXPECT_NO_THROW({ manager.RegisterCallbackFunc(fake_stream, []() {}); });

  // 4. 销毁
  EXPECT_EQ(manager.Destroy(), SUCCESS);
}

// 用例 3：正常执行回调的情况
TEST_F(UtestRtCallbackManager, CallbackProcessSuccess) {
  std::atomic<bool> callback_executed{false};
  RtCallbackManager manager;

  // 确保 aclrtSetCurrentContext 返回成功（默认行为）
  AclRuntimeStub::SetErrorResultApiName("");

  // 初始化
  EXPECT_EQ(manager.Init(), SUCCESS);

  // 注册回调
  aclrtStream fake_stream = reinterpret_cast<aclrtStream>(0x123456);
  manager.RegisterCallbackFunc(fake_stream, [&callback_executed]() { callback_executed.store(true); });

  // 等待执行
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  // 销毁
  EXPECT_EQ(manager.Destroy(), SUCCESS);

  // 验证
  EXPECT_TRUE(callback_executed.load());
}

}  // namespace hybrid
}  // namespace ge
