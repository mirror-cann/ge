/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef GE_COMPILER_GRAPH_FUSION_PASS_PYTHON_PASS_FALLBACK_CODEGEN_HELPER_H_
#define GE_COMPILER_GRAPH_FUSION_PASS_PYTHON_PASS_FALLBACK_CODEGEN_HELPER_H_

#include <dlfcn.h>

#include <string>

#include "framework/common/debug/ge_log.h"
#include "python_pass_artifact_selector.h"

namespace ge {
namespace fusion {
namespace python_pass_fallback_codegen {

using ProbePythonRuntimeFn = bool (*)(const char *python_command, python_pass_artifact::PythonRuntimeKey &runtime_key);

constexpr const char *kPyGILStateEnsureSymbol = "PyGILState_Ensure";
constexpr const char *kPyGILStateReleaseSymbol = "PyGILState_Release";
constexpr const char *kPyImportAddModuleSymbol = "PyImport_AddModule";
constexpr const char *kPyModuleGetDictSymbol = "PyModule_GetDict";
constexpr const char *kPyRunStringSymbol = "PyRun_String";
constexpr const char *kPyUnicodeAsUTF8Symbol = "PyUnicode_AsUTF8";
constexpr const char *kPyDecRefSymbol = "Py_DecRef";
constexpr const char *kPyErrOccurredSymbol = "PyErr_Occurred";
constexpr const char *kPyErrFetchSymbol = "PyErr_Fetch";
constexpr const char *kPyObjectStrSymbol = "PyObject_Str";
constexpr int kPyEvalInput = 258;

constexpr const char *kSubProcessFallbackRootPrefix = "__GE_PYTHON_PASS_FALLBACK_ROOT__=";
constexpr const char *kSubProcessFallbackScript =
    " -c \"import sys, traceback\n"
    "try:\n"
    "    from ge.passes.runtime import run_fallback_codegen\n"
    "    print('__GE_PYTHON_PASS_FALLBACK_ROOT__=' + str(run_fallback_codegen().root))\n"
    "except Exception:\n"
    "    traceback.print_exc(file=sys.stdout)\n"
    "    sys.exit(1)\" 2>/dev/null";
constexpr const char *kInProcessFallbackExpression =
    "str(__import__('ge.passes.runtime', fromlist=['run_fallback_codegen']).run_fallback_codegen().root)";

struct InProcessPythonApi {
  using PyObjectPtr = void *;
  using PyGILStateEnsureFn = int (*)();
  using PyGILStateReleaseFn = void (*)(int);
  using PyImportAddModuleFn = PyObjectPtr (*)(const char *);
  using PyModuleGetDictFn = PyObjectPtr (*)(PyObjectPtr);
  using PyRunStringFn = PyObjectPtr (*)(const char *, int, PyObjectPtr, PyObjectPtr);
  using PyUnicodeAsUTF8Fn = const char *(*)(PyObjectPtr);
  using PyDecRefFn = void (*)(PyObjectPtr);
  using PyErrOccurredFn = int (*)();
  using PyErrFetchFn = void (*)(PyObjectPtr *, PyObjectPtr *, PyObjectPtr *);
  using PyObjectStrFn = PyObjectPtr (*)(PyObjectPtr);

  PyGILStateEnsureFn gil_ensure{nullptr};
  PyGILStateReleaseFn gil_release{nullptr};
  PyImportAddModuleFn import_add_module{nullptr};
  PyModuleGetDictFn module_get_dict{nullptr};
  PyRunStringFn run_string{nullptr};
  PyUnicodeAsUTF8Fn unicode_as_utf8{nullptr};
  PyDecRefFn dec_ref{nullptr};
  PyErrOccurredFn err_occurred{nullptr};
  PyErrFetchFn err_fetch{nullptr};
  PyObjectStrFn object_str{nullptr};

  bool Resolve() {
    gil_ensure = reinterpret_cast<PyGILStateEnsureFn>(dlsym(RTLD_DEFAULT, kPyGILStateEnsureSymbol));
    gil_release = reinterpret_cast<PyGILStateReleaseFn>(dlsym(RTLD_DEFAULT, kPyGILStateReleaseSymbol));
    import_add_module = reinterpret_cast<PyImportAddModuleFn>(dlsym(RTLD_DEFAULT, kPyImportAddModuleSymbol));
    module_get_dict = reinterpret_cast<PyModuleGetDictFn>(dlsym(RTLD_DEFAULT, kPyModuleGetDictSymbol));
    run_string = reinterpret_cast<PyRunStringFn>(dlsym(RTLD_DEFAULT, kPyRunStringSymbol));
    unicode_as_utf8 = reinterpret_cast<PyUnicodeAsUTF8Fn>(dlsym(RTLD_DEFAULT, kPyUnicodeAsUTF8Symbol));
    dec_ref = reinterpret_cast<PyDecRefFn>(dlsym(RTLD_DEFAULT, kPyDecRefSymbol));
    err_occurred = reinterpret_cast<PyErrOccurredFn>(dlsym(RTLD_DEFAULT, kPyErrOccurredSymbol));
    err_fetch = reinterpret_cast<PyErrFetchFn>(dlsym(RTLD_DEFAULT, kPyErrFetchSymbol));
    object_str = reinterpret_cast<PyObjectStrFn>(dlsym(RTLD_DEFAULT, kPyObjectStrSymbol));
    return (gil_ensure != nullptr) && (gil_release != nullptr) && (import_add_module != nullptr) &&
           (module_get_dict != nullptr) && (run_string != nullptr) && (unicode_as_utf8 != nullptr) &&
           (dec_ref != nullptr) && (err_occurred != nullptr) && (err_fetch != nullptr) && (object_str != nullptr);
  }

  std::string FormatActivePythonError() const {
    if ((err_occurred == nullptr) || (err_occurred() == 0)) {
      return "";
    }
    PyObjectPtr type = nullptr;
    PyObjectPtr value = nullptr;
    PyObjectPtr traceback = nullptr;
    err_fetch(&type, &value, &traceback);
    std::string message;
    if (value != nullptr) {
      PyObjectPtr value_str = object_str(value);
      if (value_str != nullptr) {
        const char *utf8 = unicode_as_utf8(value_str);
        if ((utf8 != nullptr) && (utf8[0] != '\0')) {
          message = utf8;
        }
        dec_ref(value_str);
      }
      dec_ref(value);
    }
    if (type != nullptr) {
      dec_ref(type);
    }
    if (traceback != nullptr) {
      dec_ref(traceback);
    }
    return message;
  }
};

struct PyGilGuard {
  explicit PyGilGuard(InProcessPythonApi &api) : api_(api), state_(api.gil_ensure()) {}
  ~PyGilGuard() {
    api_.gil_release(state_);
  }

  PyGilGuard(const PyGilGuard &) = delete;
  PyGilGuard &operator=(const PyGilGuard &) = delete;

  InProcessPythonApi &api_;
  int state_;
};

struct FallbackCodegenDependencies {
  using ReadCommandOutputFn = bool (*)(const std::string &command, std::string &output);

  ReadCommandOutputFn read_command_output{nullptr};
  ProbePythonRuntimeFn probe_runtime{nullptr};
};

inline std::string FetchLineByPrefix(const std::string &content, const std::string &prefix) {
  if (prefix.empty()) {
    return "";
  }

  size_t line_start = 0U;
  while (line_start <= content.size()) {
    const auto line_end = content.find('\n', line_start);
    size_t value_end = (line_end == std::string::npos) ? content.size() : line_end;
    if ((value_end >= (line_start + prefix.size())) && (content.compare(line_start, prefix.size(), prefix) == 0)) {
      if ((value_end > (line_start + prefix.size())) && (content[value_end - 1U] == '\r')) {
        --value_end;
      }
      return content.substr(line_start + prefix.size(), value_end - line_start - prefix.size());
    }
    if (line_end == std::string::npos) {
      break;
    }
    line_start = line_end + 1U;
  }
  return "";
}

inline bool IsRuntimeKeyCompatible(const python_pass_artifact::PythonRuntimeKey &expected_key,
                                   const python_pass_artifact::PythonRuntimeKey &loaded_key) {
  return expected_key.python_tag.empty() || loaded_key.python_tag.empty() ||
         (loaded_key.python_tag == expected_key.python_tag);
}

inline std::string ResolveCompatiblePythonCommand(const python_pass_artifact::PythonRuntimeKey &expected_key,
                                                  ProbePythonRuntimeFn probe) {
  if (probe == nullptr) {
    return "";
  }
  for (const char *candidate : {"python3", "python"}) {
    python_pass_artifact::PythonRuntimeKey probed_key;
    if (!probe(candidate, probed_key)) {
      continue;
    }
    if (!IsRuntimeKeyCompatible(expected_key, probed_key)) {
      continue;
    }
    return probed_key.python_command;
  }
  return "";
}

inline bool IsFallbackCodegenDependenciesValid(const FallbackCodegenDependencies &deps) {
  return (deps.read_command_output != nullptr) && (deps.probe_runtime != nullptr);
}

inline bool RunEvalExpressionInProcess(const char *expression, std::string &result_utf8) {
  result_utf8.clear();
  InProcessPythonApi py_api;
  if (!py_api.Resolve()) {
    GELOGE(FAILED, "In-process python pass fallback codegen failed, required libpython symbols are not resolvable.");
    return false;
  }

  const PyGilGuard gil_guard(py_api);
  InProcessPythonApi::PyObjectPtr main_module = py_api.import_add_module("__main__");
  InProcessPythonApi::PyObjectPtr globals = (main_module != nullptr) ? py_api.module_get_dict(main_module) : nullptr;
  InProcessPythonApi::PyObjectPtr result =
      (globals != nullptr) ? py_api.run_string(expression, kPyEvalInput, globals, globals) : nullptr;
  if (result == nullptr) {
    const auto error_message = py_api.FormatActivePythonError();
    if (error_message.empty()) {
      GELOGE(FAILED, "In-process python pass fallback codegen failed.");
    } else {
      GELOGE(FAILED, "In-process python pass fallback codegen failed: %s", error_message.c_str());
    }
    return false;
  }
  const char *utf8 = py_api.unicode_as_utf8(result);
  if ((utf8 == nullptr) || (utf8[0] == '\0')) {
    GELOGE(FAILED, "In-process python pass fallback codegen failed: result is empty.");
    py_api.dec_ref(result);
    return false;
  }
  result_utf8 = utf8;
  py_api.dec_ref(result);
  return true;
}

inline bool RunFallbackCodegenViaSubprocess(const python_pass_artifact::PythonRuntimeKey &runtime_key,
                                            const FallbackCodegenDependencies &deps, std::string &gen_artifact_root) {
  if ((deps.read_command_output == nullptr) || (deps.probe_runtime == nullptr)) {
    return false;
  }
  std::string python_command = runtime_key.python_command;
  if (python_command.empty()) {
    python_command = ResolveCompatiblePythonCommand(runtime_key, deps.probe_runtime);
  }
  if (python_command.empty()) {
    GELOGE(FAILED, "Python pass fallback codegen failed, no python command for runtime key[%s].",
           runtime_key.ToString().c_str());
    return false;
  }

  std::string output;
  if (!deps.read_command_output(std::string(python_command) + kSubProcessFallbackScript, output)) {
    GELOGE(FAILED, "Subprocess python pass fallback codegen failed, command[%s], output[%s].", python_command.c_str(),
           output.c_str());
    return false;
  }
  gen_artifact_root = FetchLineByPrefix(output, kSubProcessFallbackRootPrefix);
  if (gen_artifact_root.empty()) {
    GELOGE(FAILED,
           "Subprocess python pass fallback codegen failed, command[%s], "
           "missing artifact root marker in output[%s].",
           python_command.c_str(), output.c_str());
    return false;
  }
  GELOGI("Subprocess python pass fallback codegen success, gen artifact root[%s].", gen_artifact_root.c_str());
  return true;
}

inline bool RunFallbackCodegenInProcess(std::string &gen_artifact_root) {
  if (!RunEvalExpressionInProcess(kInProcessFallbackExpression, gen_artifact_root)) {
    return false;
  }
  GELOGI("In-process python pass fallback codegen success, gen artifact root[%s].", gen_artifact_root.c_str());
  return true;
}

inline bool RunFallbackCodegen(const python_pass_artifact::PythonRuntimeKey &runtime_key,
                               const FallbackCodegenDependencies &deps, std::string &gen_artifact_root) {
  gen_artifact_root.clear();
  if (runtime_key.python_tag.empty() || !IsFallbackCodegenDependenciesValid(deps)) {
    return false;
  }
  if (runtime_key.has_python_symbols && runtime_key.is_initialized) {
    return RunFallbackCodegenInProcess(gen_artifact_root);
  }
  return RunFallbackCodegenViaSubprocess(runtime_key, deps, gen_artifact_root);
}

}  // namespace python_pass_fallback_codegen
}  // namespace fusion
}  // namespace ge

#endif  // GE_COMPILER_GRAPH_FUSION_PASS_PYTHON_PASS_FALLBACK_CODEGEN_HELPER_H_
