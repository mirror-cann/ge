/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include <gtest/gtest.h>

#include "common/python_runtime/ge_python_runtime_manager.h"
#include "common/python_runtime/ge_python_runtime_manager_helper.h"

namespace ge {
namespace {
int g_py_initialized = 0;
int g_threads_initialized = 0;
int g_py_initialize_count = 0;
int g_py_finalize_count = 0;
int g_py_eval_init_threads_count = 0;
int g_py_eval_save_thread_count = 0;
int g_py_eval_restore_thread_count = 0;
int g_py_gil_state_check = 0;

int FakePyIsInitialized() {
  return g_py_initialized;
}

void FakePyInitialize() {
  ++g_py_initialize_count;
  g_py_initialized = 1;
}

void FakePyFinalize() {
  ++g_py_finalize_count;
  g_py_initialized = 0;
}

const char *FakePyGetVersion() {
  return "3.13.0";
}

void FakePyEvalInitThreads() {
  ++g_py_eval_init_threads_count;
  g_threads_initialized = 1;
}

int FakePyEvalThreadsInitialized() {
  return g_threads_initialized;
}

void *FakePyEvalSaveThread() {
  ++g_py_eval_save_thread_count;
  return reinterpret_cast<void *>(0x100);
}

void FakePyEvalRestoreThread(void *thread_state) {
  (void)thread_state;
  ++g_py_eval_restore_thread_count;
}

int FakePyGILStateCheck() {
  return g_py_gil_state_check;
}

extern "C" int Py_IsInitialized() {
  return FakePyIsInitialized();
}

extern "C" void Py_Initialize() {
  FakePyInitialize();
}

extern "C" void Py_Finalize() {
  FakePyFinalize();
}

extern "C" const char *Py_GetVersion() {
  return FakePyGetVersion();
}

extern "C" void PyEval_InitThreads() {
  FakePyEvalInitThreads();
}

extern "C" int PyEval_ThreadsInitialized() {
  return FakePyEvalThreadsInitialized();
}

extern "C" void *PyEval_SaveThread() {
  return FakePyEvalSaveThread();
}

extern "C" void PyEval_RestoreThread(void *thread_state) {
  FakePyEvalRestoreThread(thread_state);
}

extern "C" int PyGILState_Check() {
  return FakePyGILStateCheck();
}

void ResetFakePythonState() {
  g_py_initialized = 0;
  g_threads_initialized = 0;
  g_py_initialize_count = 0;
  g_py_finalize_count = 0;
  g_py_eval_init_threads_count = 0;
  g_py_eval_save_thread_count = 0;
  g_py_eval_restore_thread_count = 0;
  g_py_gil_state_check = 0;
}

class UtestGePythonRuntimeManager : public testing::Test {
 protected:
  void SetUp() override {
    auto &manager = GePythonRuntimeManager::Instance();
    (void)manager.ShutdownProcess();
    ResetFakePythonState();
    g_py_gil_state_check = 1;
  }

  void TearDown() override {
    auto &manager = GePythonRuntimeManager::Instance();
    (void)manager.ShutdownProcess();
    ResetFakePythonState();
  }
};

TEST_F(UtestGePythonRuntimeManager, EnsureReadyAttachesExistingInterpreter) {
  g_py_initialized = 1;

  auto &manager = GePythonRuntimeManager::Instance();
  EXPECT_EQ(manager.EnsureReady(), SUCCESS);

  EXPECT_EQ(g_py_initialize_count, 0);
  EXPECT_EQ(g_py_eval_init_threads_count, 0);
  EXPECT_EQ(g_py_eval_save_thread_count, 0);

  EXPECT_EQ(manager.ShutdownProcess(), SUCCESS);
  EXPECT_EQ(g_py_finalize_count, 0);
  EXPECT_EQ(g_py_initialized, 1);
}

TEST_F(UtestGePythonRuntimeManager, EnsureReadyInitializesOwnedInterpreterAndShutdownFinalizes) {
  auto &manager = GePythonRuntimeManager::Instance();
  EXPECT_EQ(manager.EnsureReady(), SUCCESS);

  EXPECT_EQ(g_py_initialize_count, 1);
  EXPECT_EQ(g_py_eval_init_threads_count, 1);
  EXPECT_EQ(g_py_eval_save_thread_count, 1);

  EXPECT_EQ(manager.EnsureReady(), SUCCESS);
  EXPECT_EQ(g_py_initialize_count, 1);
  EXPECT_EQ(g_py_eval_save_thread_count, 1);

  EXPECT_EQ(manager.ShutdownProcess(), SUCCESS);
  EXPECT_EQ(g_py_finalize_count, 1);
  EXPECT_EQ(g_py_eval_restore_thread_count, 1);

  EXPECT_EQ(manager.EnsureReady(), SUCCESS);
  EXPECT_EQ(g_py_initialize_count, 2);
  EXPECT_EQ(g_py_eval_save_thread_count, 2);
}

TEST_F(UtestGePythonRuntimeManager, FinalizeOwnedInterpreterSkipsWhenNotInitialized) {
  auto &manager = GePythonRuntimeManager::Instance();
  EXPECT_EQ(manager.EnsureReady(), SUCCESS);

  g_py_initialized = 0;

  EXPECT_EQ(manager.ShutdownProcess(), SUCCESS);
  EXPECT_EQ(g_py_finalize_count, 0);
}

TEST_F(UtestGePythonRuntimeManager, ShutdownProcessWhenNotReady) {
  auto &manager = GePythonRuntimeManager::Instance();
  EXPECT_EQ(manager.ShutdownProcess(), SUCCESS);
  EXPECT_EQ(g_py_finalize_count, 0);
}

TEST_F(UtestGePythonRuntimeManager, EnsureReadyWithThreadsAlreadyInitialized) {
  g_py_initialized = 0;
  g_threads_initialized = 1;

  auto &manager = GePythonRuntimeManager::Instance();
  EXPECT_EQ(manager.EnsureReady(), SUCCESS);

  EXPECT_EQ(g_py_initialize_count, 1);
  EXPECT_EQ(g_py_eval_init_threads_count, 0);
}

TEST_F(UtestGePythonRuntimeManager, EnsureReadyIdempotentAfterShutdown) {
  auto &manager = GePythonRuntimeManager::Instance();
  EXPECT_EQ(manager.EnsureReady(), SUCCESS);

  EXPECT_EQ(manager.ShutdownProcess(), SUCCESS);

  EXPECT_EQ(manager.EnsureReady(), SUCCESS);
  EXPECT_EQ(g_py_initialize_count, 2);
}

TEST_F(UtestGePythonRuntimeManager, FinalizeNonOwnerPreservesInterpreterState) {
  g_py_initialized = 1;

  auto &manager = GePythonRuntimeManager::Instance();
  EXPECT_EQ(manager.EnsureReady(), SUCCESS);

  EXPECT_EQ(manager.ShutdownProcess(), SUCCESS);
  EXPECT_EQ(g_py_finalize_count, 0);
  EXPECT_EQ(g_py_initialized, 1);
}
}  // namespace
}  // namespace ge
