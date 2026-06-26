/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_PYTHON_RUNTIME_GE_PYTHON_RUNTIME_MANAGER_HELPER_H_
#define BASE_COMMON_PYTHON_RUNTIME_GE_PYTHON_RUNTIME_MANAGER_HELPER_H_

#include <dlfcn.h>
#include <string>

#include "framework/common/debug/ge_log.h"
#include "ge/ge_api_error_codes.h"
#include "graph/utils/file_utils.h"

namespace ge {

using PyIsInitializedFn = int (*)();
using PyInitializeFn = void (*)();
using PyFinalizeFn = void (*)();
using PyGetVersionFn = const char *(*)();
using PyEvalInitThreadsFn = void (*)();
using PyEvalThreadsInitializedFn = int (*)();
using PyEvalSaveThreadFn = void *(*)();
using PyEvalRestoreThreadFn = void (*)(void *);
using PyGILStateCheckFn = int (*)();

struct PythonCApi {
  PyIsInitializedFn py_is_initialized{nullptr};
  PyInitializeFn py_initialize{nullptr};
  PyFinalizeFn py_finalize{nullptr};
  PyGetVersionFn py_get_version{nullptr};
  PyEvalInitThreadsFn py_eval_init_threads{nullptr};
  PyEvalThreadsInitializedFn py_eval_threads_initialized{nullptr};
  PyEvalSaveThreadFn py_eval_save_thread{nullptr};
  PyEvalRestoreThreadFn py_eval_restore_thread{nullptr};
  PyGILStateCheckFn py_gil_state_check{nullptr};
};

struct PythonProbeResult {
  std::string python_command;
  std::string python_tag;
  std::string libpython_path;
};

constexpr const char *kPyIsInitializedSymbol = "Py_IsInitialized";
constexpr const char *kPyInitializeSymbol = "Py_Initialize";
constexpr const char *kPyFinalizeSymbol = "Py_Finalize";
constexpr const char *kPyGetVersionSymbol = "Py_GetVersion";
constexpr const char *kPyEvalInitThreadsSymbol = "PyEval_InitThreads";
constexpr const char *kPyEvalThreadsInitializedSymbol = "PyEval_ThreadsInitialized";
constexpr const char *kPyEvalSaveThreadSymbol = "PyEval_SaveThread";
constexpr const char *kPyEvalRestoreThreadSymbol = "PyEval_RestoreThread";
constexpr const char *kPyGILStateCheckSymbol = "PyGILState_Check";
constexpr const char *kPythonRuntimeProbeScript =
    " -c \"import sys; print('cp%d%d' % sys.version_info[:2])\" 2>/dev/null";
constexpr const char *kLibpythonProbeScript =
    " -c \"import os, sysconfig\n"
    "version = sysconfig.get_config_var('VERSION') or ''\n"
    "libdir = sysconfig.get_config_var('LIBDIR') or ''\n"
    "candidates = [sysconfig.get_config_var('LDLIBRARY'), sysconfig.get_config_var('INSTSONAME'), "
    "sysconfig.get_config_var('LIBRARY')]\n"
    "candidates.extend([('libpython%s.so.1.0' % version) if version else '', "
    "('libpython%s.so' % version) if version else ''])\n"
    "seen = []\n"
    "for item in candidates:\n"
    "    if item and item not in seen:\n"
    "        seen.append(item)\n"
    "for item in seen:\n"
    "    if '.so' not in item:\n"
    "        continue\n"
    "    path = os.path.join(libdir, item) if libdir else ''\n"
    "    if path and os.path.exists(path):\n"
    "        print(path)\n"
    "        break\" 2>/dev/null";
constexpr int kOpenFlags = RTLD_NOW | RTLD_GLOBAL;

inline PythonCApi g_python_api;

#define GE_BIND_PY_SYMBOL(func_field, py_symbol) \
  do { \
    resolved.func_field = reinterpret_cast<decltype(resolved.func_field)>(dlsym(handle, py_symbol)); \
    if (resolved.func_field == nullptr) { \
      if (log_failure) { \
        GELOGW("[GePythonRuntime] Failed to dlsym python symbol[%s].", py_symbol); \
      } \
      return false; \
    } \
  } while (0)

inline bool ResolvePythonCApi(void *libpython_handle, const bool log_failure) {
  void *handle = (libpython_handle != nullptr) ? libpython_handle : RTLD_DEFAULT;
  PythonCApi resolved;
  GE_BIND_PY_SYMBOL(py_is_initialized, kPyIsInitializedSymbol);
  GE_BIND_PY_SYMBOL(py_initialize, kPyInitializeSymbol);
  GE_BIND_PY_SYMBOL(py_finalize, kPyFinalizeSymbol);
  GE_BIND_PY_SYMBOL(py_get_version, kPyGetVersionSymbol);
  GE_BIND_PY_SYMBOL(py_eval_init_threads, kPyEvalInitThreadsSymbol);
  GE_BIND_PY_SYMBOL(py_eval_threads_initialized, kPyEvalThreadsInitializedSymbol);
  GE_BIND_PY_SYMBOL(py_eval_save_thread, kPyEvalSaveThreadSymbol);
  GE_BIND_PY_SYMBOL(py_eval_restore_thread, kPyEvalRestoreThreadSymbol);
  GE_BIND_PY_SYMBOL(py_gil_state_check, kPyGILStateCheckSymbol);
  g_python_api = resolved;
  return true;
}

#undef GE_BIND_PY_SYMBOL

inline void ResetPythonCApi() {
  g_python_api = PythonCApi {};
}

inline std::string FirstLine(const std::string &text) {
  const auto pos = text.find('\n');
  if (pos == std::string::npos) {
    return text;
  }
  return text.substr(0U, pos);
}

inline bool ReadCommandOutput(const std::string &command, std::string &output) {
  FILE *fp = popen(command.c_str(), "r");
  if (fp == nullptr) {
    return false;
  }
  char buffer[256] = {0};
  while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
    output += buffer;
  }
  const auto ret = pclose(fp);
  return (ret == 0) && (!output.empty());
}

inline bool ProbePythonRuntimeFromCommand(const char *python_command, PythonProbeResult &result) {
  if ((python_command == nullptr) || (python_command[0] == '\0')) {
    return false;
  }
  std::string output;
  if (!ReadCommandOutput(std::string(python_command) + kPythonRuntimeProbeScript, output)) {
    return false;
  }
  const auto python_tag = FirstLine(output);
  if (python_tag.empty()) {
    return false;
  }
  result.python_command = python_command;
  result.python_tag = python_tag;
  return true;
}

inline std::string ResolveLibpythonPathFromCommand(const std::string &python_command) {
  if (python_command.empty()) {
    return "";
  }
  std::string output;
  if (!ReadCommandOutput(python_command + kLibpythonProbeScript, output)) {
    return "";
  }
  const auto libpython_path = FirstLine(output);
  if (libpython_path.empty()) {
    return "";
  }
  const auto real_path = RealPath(libpython_path.c_str());
  return real_path.empty() ? libpython_path : real_path;
}

inline bool ProbeRuntimeCandidate(const char *candidate, PythonProbeResult &probed) {
  if (!ProbePythonRuntimeFromCommand(candidate, probed)) {
    return false;
  }
  probed.libpython_path = ResolveLibpythonPathFromCommand(probed.python_command);
  if (probed.libpython_path.empty()) {
    GELOGW("[GePythonRuntime] Skip python command[%s] because libpython path is unresolved.",
           probed.python_command.c_str());
    return false;
  }
  return true;
}

inline bool EnsureLibpythonLoaded(void **handle) {
  if (handle == nullptr) {
    return false;
  }
  if (*handle != nullptr) {
    return true;
  }

  bool has_probe_result = false;
  for (const char *candidate : {"python3", "python"}) {
    PythonProbeResult probe_result;
    if (!ProbeRuntimeCandidate(candidate, probe_result)) {
      continue;
    }
    has_probe_result = true;
    void *opened_handle = dlopen(probe_result.libpython_path.c_str(), kOpenFlags);
    if (opened_handle == nullptr) {
      const char *open_error = dlerror();
      GELOGW("[GePythonRuntime] dlopen libpython[%s] failed: %s, try next candidate.",
             probe_result.libpython_path.c_str(), open_error == nullptr ? "" : open_error);
      continue;
    }
    *handle = opened_handle;
    GELOGI("[GePythonRuntime] Loaded libpython[%s] for python tag[%s].", probe_result.libpython_path.c_str(),
           probe_result.python_tag.c_str());
    return true;
  }

  if (!has_probe_result) {
    GELOGE(FAILED, "[GePythonRuntime] Probe target python runtime failed.");
    return false;
  }
  GELOGE(FAILED, "[GePythonRuntime] Load libpython failed for all probed python runtime candidates.");
  return false;
}

}  // namespace ge

#endif  // BASE_COMMON_PYTHON_RUNTIME_GE_PYTHON_RUNTIME_MANAGER_HELPER_H_
