/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <atomic>
#include <dlfcn.h>
#include <sys/stat.h>
#include <fstream>
#include <mutex>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <unistd.h>
#include <vector>
#include "common/share_graph.h"
#include "common/python_runtime/python_artifact_utils.h"
#include "common/python_runtime/python_bridge_loader_utils.h"
#include "es_ge_test_ops.h"
#include "graph/debug/ge_attr_define.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph/utils/node_adapter.h"
#include "nlohmann/json.hpp"

#include "stub/gert_runtime_stub.h"
#include "ge/fusion/pattern.h"
#include "common/topo_checker.h"
#include "ge/fusion/pass/fusion_pass_reg.h"
#include "ge/fusion/pass/pattern_fusion_pass.h"
#include "register/custom_pass_context_impl.h"
#include "graph/ge_local_context.h"
#include "compiler/graph/fusion/pass/fusion_pass_executor.h"
#include "register/optimization_option_registry.h"

// 单例，为了保证ut效果，需要清理其成员
#define private public
#include "compiler/graph/fusion/pass/pass_registry.h"
#include "ge/fusion/pass/decompose_pass.h"
#undef private
#include "compiler/graph/fusion/pass/python_pass_adapter.h"
#include "compiler/graph/fusion/pass/python_pass_bridge_c_api.h"
#include "compiler/graph/fusion/pass/python_pass_bridge_loader_helper.h"
#include "compiler/graph/fusion/pass/python_pass_pybind_bridge.h"

namespace ge {
namespace fusion {
namespace {
std::string GetCodeDir() {
  ge::char_t current_path[MMPA_MAX_PATH] = {'\0'};
  getcwd(current_path, MMPA_MAX_PATH);
  return current_path;
}

constexpr const char *kEnvPythonPassPath = "ASCEND_GE_PY_PASS_PATH";
constexpr const char *kPythonPassArtifactsRelativePath = "passes/python_pass_artifacts";

class PythonPassProcessShutdownEnvironment final : public ::testing::Environment {
 public:
  void TearDown() override {
    (void)unsetenv(kEnvPythonPassPath);
    ShutdownPythonPassesForProcess();
  }
};

::testing::Environment *const kPythonPassProcessShutdownEnvironment =
    ::testing::AddGlobalTestEnvironment(new PythonPassProcessShutdownEnvironment());

class ScopedTempDir {
 public:
  ScopedTempDir() {
    char dir_template[] = "/tmp/ge_python_pass_ut_XXXXXX";
    const auto *created_dir = mkdtemp(dir_template);
    dir_path_ = (created_dir == nullptr) ? std::string() : std::string(created_dir);
  }

  ~ScopedTempDir() {
    if (dir_path_.empty()) {
      return;
    }
    for (const auto &file_name : created_files_) {
      (void)remove((dir_path_ + "/" + file_name).c_str());
    }
    for (auto dir_iter = created_dirs_.rbegin(); dir_iter != created_dirs_.rend(); ++dir_iter) {
      (void)rmdir(dir_iter->c_str());
    }
    (void)rmdir(dir_path_.c_str());
  }

  std::string FilePath(const std::string &file_name) const {
    return dir_path_ + "/" + file_name;
  }

  std::string CreateFilePath(const std::string &file_name) {
    TrackFile(file_name);
    return FilePath(file_name);
  }

  std::string MakeDir(const std::string &relative_path) {
    std::string current = dir_path_;
    size_t start = 0U;
    while (start < relative_path.size()) {
      const auto end = relative_path.find('/', start);
      const auto part = relative_path.substr(start, end == std::string::npos ? end : end - start);
      if (!part.empty()) {
        current += "/" + part;
        if (mkdir(current.c_str(), 0700) == 0) {
          created_dirs_.push_back(current);
        }
      }
      if (end == std::string::npos) {
        break;
      }
      start = end + 1U;
    }
    return current;
  }

  void TrackFile(const std::string &file_name) {
    created_files_.push_back(file_name);
  }

 private:
  std::string dir_path_;
  std::vector<std::string> created_dirs_;
  std::vector<std::string> created_files_;
};

class ScopedEnvVar {
 public:
  ScopedEnvVar(const std::string &name, const std::string &value) : name_(name) {
    const char *old_value = getenv(name_.c_str());
    if (old_value != nullptr) {
      has_old_value_ = true;
      old_value_ = old_value;
    }
    (void)setenv(name_.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvVar() {
    if (has_old_value_) {
      (void)setenv(name_.c_str(), old_value_.c_str(), 1);
      return;
    }
    (void)unsetenv(name_.c_str());
  }

 private:
  std::string name_;
  std::string old_value_;
  bool has_old_value_{false};
};

void WriteFile(const std::string &file_path, const std::string &content) {
  std::ofstream file(file_path, std::ios::out | std::ios::trunc);
  ASSERT_TRUE(file.is_open());
  file << content;
  file.close();
}

std::string ReadFile(const std::string &file_path) {
  std::ifstream file(file_path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

ScopedTempDir &GetSharedPybindPassDir() {
  static ScopedTempDir dir;
  return dir;
}

const std::string &GetSharedPybindPassFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pybind_shared_passes.py");
  return path;
}

const std::string &GetSharedPybindMarkerFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("bridge_success_marker.txt");
  return path;
}

const std::string &GetSharedPybindReportFuseMarkerFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("report_fuse_success_marker.txt");
  return path;
}

const std::string &GetSharedPybindPatternPassFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pybind_pattern_passes.py");
  return path;
}

const std::string &GetSharedPybindPatternMarkerFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pattern_bridge_success_marker.txt");
  return path;
}

const std::string &GetSharedPybindPatternMatcherConfigPassFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pybind_pattern_matcher_config_passes.py");
  return path;
}

const std::string &GetSharedPybindPatternMatcherConfigMarkerFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pattern_matcher_config_bridge_marker.txt");
  return path;
}

const std::string &GetSharedPybindDecomposePassFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("pybind_decompose_passes.py");
  return path;
}

const std::string &GetSharedPybindDecomposeMarkerFilePath() {
  static const std::string path = GetSharedPybindPassDir().CreateFilePath("decompose_bridge_success_marker.txt");
  return path;
}

void EnsureSharedPybindPassFile() {
  static std::once_flag once;
  std::call_once(once, []() {
    std::ostringstream pass_code;
    pass_code << "from pathlib import Path\n"
              << "from ge.graph import Graph\n"
              << "from ge.passes import (\n"
              << "    FusionBasePass, PassStage, register_fusion_pass, report_fuse\n"
              << ")\n\n"
              << "MARKER_FILE = r'" << GetSharedPybindMarkerFilePath() << "'\n\n"
              << "REPORT_FUSE_MARKER_FILE = r'" << GetSharedPybindReportFuseMarkerFilePath() << "'\n\n"
              << "@register_fusion_pass(name='PythonPybindBridgePass', stage=PassStage.AFTER_INFER_SHAPE)\n"
              << "class PythonPybindBridgePass(FusionBasePass):\n"
              << "    def run(self, graph, context):\n"
              << "        assert isinstance(graph, Graph)\n"
              << "        Path(MARKER_FILE).write_text(f\"{graph.name}|{context.get_pass_name()}\", encoding='utf-8')\n"
              << "        return 0\n\n"
              << "@register_fusion_pass(name='PythonPybindReportFusePass', stage=PassStage.AFTER_INFER_SHAPE)\n"
              << "class PythonPybindReportFusePass(FusionBasePass):\n"
              << "    def run(self, graph, context):\n"
              << "        nodes = graph.get_direct_nodes()\n"
              << "        report_fuse(nodes, [], context)\n"
              << "        Path(REPORT_FUSE_MARKER_FILE).write_text(context.get_pass_name(), encoding='utf-8')\n"
              << "        return 0\n\n"
              << "@register_fusion_pass(name='PythonPybindBridgeFailedPass', "
                 "stage=PassStage.AFTER_BUILTIN_FUSION_PASS)\n"
              << "class PythonPybindBridgeFailedPass(FusionBasePass):\n"
              << "    def run(self, graph, context):\n"
              << "        raise RuntimeError('python bridge run failed')\n";
    WriteFile(GetSharedPybindPassFilePath(), pass_code.str());
  });
}

void EnsureSharedPybindPatternPassFile() {
  static std::once_flag once;
  std::call_once(once, []() {
    std::ostringstream pass_code;
    pass_code << "from pathlib import Path\n"
              << "from ge.es.graph_builder import GraphBuilder\n"
              << "from es_ut_test import Add\n"
              << "from ge.passes import (\n"
              << "    PassStage,\n"
              << "    PatternFusionPass,\n"
              << "    PatternMatcherConfigBuilder,\n"
              << "    create_pattern,\n"
              << "    create_replacement,\n"
              << "    register_fusion_pass,\n"
              << ")\n\n"
              << "MARKER_FILE = r'" << GetSharedPybindPatternMarkerFilePath() << "'\n\n"
              << "@register_fusion_pass(name='PythonPybindPatternAddZeroPass', stage=PassStage.AFTER_INFER_SHAPE)\n"
              << "class PythonPybindPatternAddZeroPass(PatternFusionPass):\n"
              << "    def __init__(self):\n"
              << "        super().__init__(\n"
              << "            PatternMatcherConfigBuilder().enable_const_value_match().build()\n"
              << "        )\n\n"
              << "    def patterns(self):\n"
              << "        pattern_builder = GraphBuilder('add_zero_pattern')\n"
              << "        data = pattern_builder.create_input(0)\n"
              << "        zero = pattern_builder.create_scalar_float(0.0)\n"
              << "        add = Add(data, zero)\n"
              << "        pattern_builder.set_graph_output(add, 0)\n"
              << "        pattern = create_pattern(pattern_builder.build_and_reset())\n"
              << "        pattern.capture_tensor(data)\n"
              << "        return [pattern]\n\n"
              << "    def meet_requirements(self, match_result):\n"
              << "        return True\n\n"
              << "    def replacement(self, match_result):\n"
              << "        captured = match_result.get_captured_tensor(0)\n"
              << "        Path(MARKER_FILE).write_text(\n"
              << "            f\"{match_result.get_pattern_graph_name()}|{captured.node.name}:{captured.index}\",\n"
              << "            encoding='utf-8'\n"
              << "        )\n"
              << "        replacement_builder = GraphBuilder('add_zero_replacement')\n"
              << "        passthrough = replacement_builder.create_input(0)\n"
              << "        replacement_builder.set_graph_output(passthrough, 0)\n"
              << "        return create_replacement(replacement_builder.build_and_reset())\n";
    WriteFile(GetSharedPybindPatternPassFilePath(), pass_code.str());
  });
}

void EnsureSharedPybindPatternMatcherConfigPassFile() {
  static std::once_flag once;
  std::call_once(once, []() {
    std::ostringstream pass_code;
    pass_code << "from pathlib import Path\n"
              << "from ge.es.graph_builder import GraphBuilder\n"
              << "from es_ut_test import Add\n"
              << "from ge.passes import (\n"
              << "    PassStage,\n"
              << "    PatternFusionPass,\n"
              << "    PatternMatcherConfigBuilder,\n"
              << "    create_pattern,\n"
              << "    create_replacement,\n"
              << "    register_fusion_pass,\n"
              << ")\n\n"
              << "MARKER_FILE = r'" << GetSharedPybindPatternMatcherConfigMarkerFilePath() << "'\n\n"
              << "@register_fusion_pass(name='PythonPybindPatternMatcherConfigPass', "
                 "stage=PassStage.AFTER_INFER_SHAPE)\n"
              << "class PythonPybindPatternMatcherConfigPass(PatternFusionPass):\n"
              << "    def __init__(self):\n"
              << "        super().__init__(\n"
              << "            PatternMatcherConfigBuilder().enable_const_value_match().build()\n"
              << "        )\n\n"
              << "    def patterns(self):\n"
              << "        pattern_builder = GraphBuilder('const_one_pattern')\n"
              << "        data = pattern_builder.create_input(0)\n"
              << "        one = pattern_builder.create_const_float(1.0)\n"
              << "        add = Add(data, one)\n"
              << "        pattern_builder.set_graph_output(add, 0)\n"
              << "        return [create_pattern(pattern_builder.build_and_reset())]\n\n"
              << "    def replacement(self, match_result):\n"
              << "        Path(MARKER_FILE).write_text(match_result.get_pattern_graph_name(), encoding='utf-8')\n"
              << "        replacement_builder = GraphBuilder('const_one_replacement')\n"
              << "        passthrough = replacement_builder.create_input(0)\n"
              << "        replacement_builder.set_graph_output(passthrough, 0)\n"
              << "        return create_replacement(replacement_builder.build_and_reset())\n";
    WriteFile(GetSharedPybindPatternMatcherConfigPassFilePath(), pass_code.str());
  });
}

void EnsureSharedPybindDecomposePassFile() {
  static std::once_flag once;
  std::call_once(once, []() {
    std::ostringstream pass_code;
    pass_code << "from pathlib import Path\n"
              << "from ge.es.graph_builder import GraphBuilder\n"
              << "from ge.passes import (\n"
              << "    DecomposePass,\n"
              << "    PassStage,\n"
              << "    create_replacement,\n"
              << "    register_decompose_pass,\n"
              << ")\n\n"
              << "MARKER_FILE = r'" << GetSharedPybindDecomposeMarkerFilePath() << "'\n\n"
              << "@register_decompose_pass(name='PythonPybindDecomposeAddPass', "
                 "stage=PassStage.AFTER_INFER_SHAPE, op_types=['Add'])\n"
              << "class PythonPybindDecomposeAddPass(DecomposePass):\n"
              << "    def meet_requirements(self, node):\n"
              << "        Path(MARKER_FILE).write_text(f\"{node.name}|{node.type}\", encoding='utf-8')\n"
              << "        return node.type == 'Add'\n\n"
              << "    def replacement(self, node):\n"
              << "        replacement_builder = GraphBuilder('decompose_add_replacement')\n"
              << "        passthrough = replacement_builder.create_input(0)\n"
              << "        replacement_builder.create_input(1)\n"
              << "        replacement_builder.set_graph_output(passthrough, 0)\n"
              << "        return create_replacement(replacement_builder.build_and_reset())\n";
    WriteFile(GetSharedPybindDecomposePassFilePath(), pass_code.str());
  });
}

struct PythonFusionBasePassRuntimeSnapshot {
  int create_count{0};
  int destroy_count{0};
  int run_count{0};
  Status run_status{SUCCESS};
  std::string last_descriptor_key;
  std::string last_pass_name;
  std::string last_context_pass_name;
  std::string last_graph_name;
};

struct PythonFusionBasePassHolderForUt {
  std::string descriptor_key;
  std::string pass_name;
};

PythonFusionBasePassRuntimeSnapshot g_python_fusion_base_runtime_snapshot;

struct PatternFusionPassRuntimeSnapshot {
  int create_count{0};
  int destroy_count{0};
  int patterns_count{0};
  int meet_requirements_count{0};
  int replacement_count{0};
  Status patterns_status{SUCCESS};
  bool meet_requirements_result{true};
  Status replacement_status{SUCCESS};
};

PatternFusionPassRuntimeSnapshot g_pattern_fusion_runtime_snapshot;

void ResetPatternFusionRuntimeSnapshot() {
  g_pattern_fusion_runtime_snapshot = {};
}

std::atomic<bool> g_force_py_is_initialized_init_failure{false};
std::atomic<uint32_t> g_py_is_initialized_call_count{0U};
std::atomic<bool> g_force_py_get_version_probe_failure{false};
constexpr const char *kProbeFailurePyVersion = "invalid-runtime";

int QueryRealPyIsInitializedForUt() {
  using PyIsInitializedFn = int (*)();
  auto *py_is_initialized = reinterpret_cast<PyIsInitializedFn>(dlsym(RTLD_NEXT, "Py_IsInitialized"));
  return (py_is_initialized == nullptr) ? 0 : py_is_initialized();
}

extern "C" int Py_IsInitialized() {
  if (g_force_py_is_initialized_init_failure.load(std::memory_order_acquire)) {
    const auto call_count = g_py_is_initialized_call_count.fetch_add(1U, std::memory_order_acq_rel);
    return (call_count == 0U) ? 0 : QueryRealPyIsInitializedForUt();
  }
  return QueryRealPyIsInitializedForUt();
}

const char *QueryRealPyGetVersionForUt() {
  using PyGetVersionFn = const char *(*)();
  auto *py_get_version = reinterpret_cast<PyGetVersionFn>(dlsym(RTLD_NEXT, "Py_GetVersion"));
  return (py_get_version == nullptr) ? "" : py_get_version();
}

extern "C" const char *Py_GetVersion() {
  if (g_force_py_get_version_probe_failure.load(std::memory_order_acquire)) {
    return kProbeFailurePyVersion;
  }
  return QueryRealPyGetVersionForUt();
}

class ScopedPyIsInitializedInitFailureForUt {
 public:
  ScopedPyIsInitializedInitFailureForUt() {
    g_py_is_initialized_call_count.store(0U, std::memory_order_release);
    g_force_py_is_initialized_init_failure.store(true, std::memory_order_release);
  }

  ~ScopedPyIsInitializedInitFailureForUt() {
    g_force_py_is_initialized_init_failure.store(false, std::memory_order_release);
    g_py_is_initialized_call_count.store(0U, std::memory_order_release);
  }
};

class ScopedPyRuntimeProbeFailureForUt {
 public:
  ScopedPyRuntimeProbeFailureForUt() {
    g_force_py_is_initialized_init_failure.store(true, std::memory_order_release);
    g_py_is_initialized_call_count.store(0U, std::memory_order_release);
    g_force_py_get_version_probe_failure.store(true, std::memory_order_release);
  }

  ~ScopedPyRuntimeProbeFailureForUt() {
    g_force_py_get_version_probe_failure.store(false, std::memory_order_release);
    g_force_py_is_initialized_init_failure.store(false, std::memory_order_release);
    g_py_is_initialized_call_count.store(0U, std::memory_order_release);
  }
};

void *OpenLibraryFailedForUt(const char *path, int flags) {
  (void)path;
  (void)flags;
  return nullptr;
}

int CloseLibraryNoopForUt(void *handle) {
  (void)handle;
  return 0;
}

void *LookupSymbolUnusedForUt(void *handle, const char *symbol) {
  (void)handle;
  (void)symbol;
  return nullptr;
}

python_artifact::PythonRuntimeKey ResolveRuntimeKeyForBridgeLoaderUt() {
  python_artifact::PythonRuntimeKey runtime_key;
  runtime_key.source = "ut";
  runtime_key.python_tag = "cp399";
  return runtime_key;
}

python_bridge_loader::BridgeLoadDependencies BuildFailingBridgeLoadDepsForUt() {
  python_bridge_loader::BridgeLoadDependencies deps;
  deps.real_path = &RealPath;
  deps.open_library = &OpenLibraryFailedForUt;
  deps.close_library = &CloseLibraryNoopForUt;
  deps.lookup_symbol = &LookupSymbolUnusedForUt;
  deps.resolve_runtime_key = &ResolveRuntimeKeyForBridgeLoaderUt;
  deps.api_symbol = kPythonFusionPassBridgeGetApiSymbol;
  deps.expected_abi = kPythonFusionPassBridgeAbiVersion;
  return deps;
}

const PythonFusionPassBridgeApi *LoadBridgeApiForUt() {
  using GetApiFn = const PythonFusionPassBridgeApi *(*)();
  static void *handle = nullptr;
  static const PythonFusionPassBridgeApi *api = nullptr;
  if (api != nullptr) {
    return api;
  }
  handle = dlopen("libge_python_pass_bridge.so", RTLD_NOW | RTLD_GLOBAL);
  if (handle == nullptr) {
    return nullptr;
  }
  auto *get_api = reinterpret_cast<GetApiFn>(dlsym(handle, kPythonFusionPassBridgeGetApiSymbol));
  if (get_api == nullptr) {
    return nullptr;
  }
  api = get_api();
  return api;
}

void RunPythonSnippetForUt(const char *snippet) {
  using PyIsInitializedFn = int (*)();
  using PyGILStateEnsureFn = int (*)();
  using PyGILStateReleaseFn = void (*)(int);
  using PyRunSimpleStringFn = int (*)(const char *);
  auto *py_is_initialized = reinterpret_cast<PyIsInitializedFn>(dlsym(RTLD_DEFAULT, "Py_IsInitialized"));
  if ((py_is_initialized == nullptr) || (py_is_initialized() == 0)) {
    return;
  }
  auto *gil_ensure = reinterpret_cast<PyGILStateEnsureFn>(dlsym(RTLD_DEFAULT, "PyGILState_Ensure"));
  auto *gil_release = reinterpret_cast<PyGILStateReleaseFn>(dlsym(RTLD_DEFAULT, "PyGILState_Release"));
  auto *py_run = reinterpret_cast<PyRunSimpleStringFn>(dlsym(RTLD_DEFAULT, "PyRun_SimpleString"));
  if ((gil_ensure == nullptr) || (gil_release == nullptr) || (py_run == nullptr)) {
    return;
  }
  const auto state = gil_ensure();
  (void)py_run(snippet);
  gil_release(state);
}

void RemovePythonPathForUt(const std::string &path) {
  using PyIsInitializedFn = int (*)();
  using PyGILStateEnsureFn = int (*)();
  using PyGILStateReleaseFn = void (*)(int);
  using PySysGetObjectFn = void *(*)(const char *);
  using PyUnicodeFromStringFn = void *(*)(const char *);
  using PySequenceIndexFn = ssize_t (*)(void *, void *);
  using PySequenceDelItemFn = int (*)(void *, ssize_t);
  using PyErrClearFn = void (*)();
  using PyDecRefFn = void (*)(void *);
  auto *py_is_initialized = reinterpret_cast<PyIsInitializedFn>(dlsym(RTLD_DEFAULT, "Py_IsInitialized"));
  if ((py_is_initialized == nullptr) || (py_is_initialized() == 0)) {
    return;
  }
  auto *gil_ensure = reinterpret_cast<PyGILStateEnsureFn>(dlsym(RTLD_DEFAULT, "PyGILState_Ensure"));
  auto *gil_release = reinterpret_cast<PyGILStateReleaseFn>(dlsym(RTLD_DEFAULT, "PyGILState_Release"));
  auto *py_sys_get_object = reinterpret_cast<PySysGetObjectFn>(dlsym(RTLD_DEFAULT, "PySys_GetObject"));
  auto *py_unicode_from_string = reinterpret_cast<PyUnicodeFromStringFn>(dlsym(RTLD_DEFAULT, "PyUnicode_FromString"));
  auto *py_sequence_index = reinterpret_cast<PySequenceIndexFn>(dlsym(RTLD_DEFAULT, "PySequence_Index"));
  auto *py_sequence_del_item = reinterpret_cast<PySequenceDelItemFn>(dlsym(RTLD_DEFAULT, "PySequence_DelItem"));
  auto *py_err_clear = reinterpret_cast<PyErrClearFn>(dlsym(RTLD_DEFAULT, "PyErr_Clear"));
  auto *py_dec_ref = reinterpret_cast<PyDecRefFn>(dlsym(RTLD_DEFAULT, "Py_DecRef"));
  if ((gil_ensure == nullptr) || (gil_release == nullptr) || (py_sys_get_object == nullptr) ||
      (py_unicode_from_string == nullptr) || (py_sequence_index == nullptr) || (py_sequence_del_item == nullptr) ||
      (py_err_clear == nullptr) || (py_dec_ref == nullptr)) {
    return;
  }
  const auto state = gil_ensure();
  void *sys_path = py_sys_get_object("path");
  void *py_path = py_unicode_from_string(path.c_str());
  if ((sys_path != nullptr) && (py_path != nullptr)) {
    while (true) {
      const ssize_t index = py_sequence_index(sys_path, py_path);
      if (index < 0) {
        py_err_clear();
        break;
      }
      (void)py_sequence_del_item(sys_path, index);
    }
  }
  if (py_path != nullptr) {
    py_dec_ref(py_path);
  }
  gil_release(state);
}

void PrependPythonPathForUt(const std::string &path) {
  using PyIsInitializedFn = int (*)();
  using PyGILStateEnsureFn = int (*)();
  using PyGILStateReleaseFn = void (*)(int);
  using PySysGetObjectFn = void *(*)(const char *);
  using PyUnicodeFromStringFn = void *(*)(const char *);
  using PyListInsertFn = int (*)(void *, ssize_t, void *);
  using PyDecRefFn = void (*)(void *);
  auto *py_is_initialized = reinterpret_cast<PyIsInitializedFn>(dlsym(RTLD_DEFAULT, "Py_IsInitialized"));
  if ((py_is_initialized == nullptr) || (py_is_initialized() == 0)) {
    return;
  }
  RemovePythonPathForUt(path);
  auto *gil_ensure = reinterpret_cast<PyGILStateEnsureFn>(dlsym(RTLD_DEFAULT, "PyGILState_Ensure"));
  auto *gil_release = reinterpret_cast<PyGILStateReleaseFn>(dlsym(RTLD_DEFAULT, "PyGILState_Release"));
  auto *py_sys_get_object = reinterpret_cast<PySysGetObjectFn>(dlsym(RTLD_DEFAULT, "PySys_GetObject"));
  auto *py_unicode_from_string = reinterpret_cast<PyUnicodeFromStringFn>(dlsym(RTLD_DEFAULT, "PyUnicode_FromString"));
  auto *py_list_insert = reinterpret_cast<PyListInsertFn>(dlsym(RTLD_DEFAULT, "PyList_Insert"));
  auto *py_dec_ref = reinterpret_cast<PyDecRefFn>(dlsym(RTLD_DEFAULT, "Py_DecRef"));
  if ((gil_ensure == nullptr) || (gil_release == nullptr) || (py_sys_get_object == nullptr) ||
      (py_unicode_from_string == nullptr) || (py_list_insert == nullptr) || (py_dec_ref == nullptr)) {
    return;
  }
  const auto state = gil_ensure();
  void *sys_path = py_sys_get_object("path");
  void *py_path = py_unicode_from_string(path.c_str());
  if ((sys_path != nullptr) && (py_path != nullptr)) {
    (void)py_list_insert(sys_path, 0, py_path);
  }
  if (py_path != nullptr) {
    py_dec_ref(py_path);
  }
  gil_release(state);
}

class ScopedPythonPathForUt {
 public:
  explicit ScopedPythonPathForUt(const std::string &path) : path_(path) {
    PrependPythonPathForUt(path_);
  }
  ~ScopedPythonPathForUt() {
    RemovePythonPathForUt(path_);
  }

 private:
  std::string path_;
};

void ForgetNativeModuleForUt() {
  RunPythonSnippetForUt(
      "import sys\n"
      "for name in list(sys.modules):\n"
      "    if name == 'ge' or name == 'ge.passes' or name.startswith('ge.passes.'):\n"
      "        sys.modules.pop(name, None)\n");
}

class ScopedLoadedPythonPassForUt {
 public:
  ~ScopedLoadedPythonPassForUt() {
    UnloadPythonPasses();
  }
};

const std::string &GetBridgeConfigDirForUt() {
  static ScopedTempDir temp_dir;
  static const std::string dir = []() {
    temp_dir.MakeDir("ge/passes");
    WriteFile(temp_dir.CreateFilePath("ge/__init__.py"), "");
    WriteFile(temp_dir.CreateFilePath("ge/passes/__init__.py"),
              "def clear_registered_passes():\n"
              "    pass\n");
    WriteFile(temp_dir.CreateFilePath("ge/passes/fake_native.py"), "configured_native_loaded = True\n");
    WriteFile(temp_dir.CreateFilePath("ge/passes/_bridge.py"),
              "def clear_pass_holders():\n"
              "    pass\n"
              "\n"
              "def clear_loaded_pass_modules():\n"
              "    pass\n");
    return temp_dir.FilePath("");
  }();
  return dir;
}

const std::string &GetBridgeNativeModulePathForUt() {
  static const std::string path = GetBridgeConfigDirForUt() + "/ge/passes/fake_native.py";
  return path;
}

void RestoreBridgeConfigForUt(const PythonFusionPassBridgeApi &api) {
  if (api.set_artifact_config == nullptr) {
    return;
  }

  PythonFusionPassBridgeArtifactConfig config = {nullptr, GetBridgeNativeModulePathForUt().c_str()};
  (void)api.set_artifact_config(&config);
}

void CleanupBridgeStateForUt(const PythonFusionPassBridgeApi &api) {
  ScopedEnvVar scoped_python_path("PYTHONPATH", GetBridgeConfigDirForUt());
  ScopedPythonPathForUt scoped_sys_path(GetBridgeConfigDirForUt());
  RestoreBridgeConfigForUt(api);
  if (api.reset_bridge_state != nullptr) {
    api.reset_bridge_state();
  }
  ForgetNativeModuleForUt();
}

int g_direct_bridge_registered_count = 0;

bool RecordPythonPassFromDirectBridge(const PythonPassDescriptor *pass_desc,
                                      const PythonFusionPassCallbacks *callbacks) {
  if ((pass_desc == nullptr) || (callbacks == nullptr)) {
    return false;
  }
  ++g_direct_bridge_registered_count;
  return true;
}

void *CreatePatternFusionPassHolderForUt(const PythonPassDescriptor *pass_desc) {
  ++g_pattern_fusion_runtime_snapshot.create_count;
  if (pass_desc == nullptr) {
    return nullptr;
  }
  return new (std::nothrow) PythonFusionBasePassHolderForUt{pass_desc->descriptor_key, pass_desc->pass_name};
}

void DestroyPatternFusionPassHolderForUt(void *holder) {
  ++g_pattern_fusion_runtime_snapshot.destroy_count;
  delete static_cast<PythonFusionBasePassHolderForUt *>(holder);
}

Status GetPatternsForUt(const void *holder, std::vector<PatternUniqPtr> &patterns) {
  ++g_pattern_fusion_runtime_snapshot.patterns_count;
  return g_pattern_fusion_runtime_snapshot.patterns_status;
}

bool MeetRequirementsForUt(const void *holder, const std::unique_ptr<MatchResult> &match_result) {
  ++g_pattern_fusion_runtime_snapshot.meet_requirements_count;
  return g_pattern_fusion_runtime_snapshot.meet_requirements_result;
}

Status ReplacementForUt(const void *holder, const std::unique_ptr<MatchResult> &match_result,
                        GraphUniqPtr &replacement_graph) {
  ++g_pattern_fusion_runtime_snapshot.replacement_count;
  return g_pattern_fusion_runtime_snapshot.replacement_status;
}

struct DecomposePassRuntimeSnapshot {
  int create_count{0};
  int destroy_count{0};
  int meet_requirements_count{0};
  int replacement_count{0};
  bool meet_requirements_result{true};
  Status replacement_status{SUCCESS};
};

DecomposePassRuntimeSnapshot g_decompose_runtime_snapshot;

void ResetDecomposeRuntimeSnapshot() {
  g_decompose_runtime_snapshot = {};
}

void *CreateDecomposePassHolderForUt(const PythonPassDescriptor *pass_desc) {
  ++g_decompose_runtime_snapshot.create_count;
  if (pass_desc == nullptr) {
    return nullptr;
  }
  return new (std::nothrow) PythonFusionBasePassHolderForUt{pass_desc->descriptor_key, pass_desc->pass_name};
}

void DestroyDecomposePassHolderForUt(void *holder) {
  ++g_decompose_runtime_snapshot.destroy_count;
  delete static_cast<PythonFusionBasePassHolderForUt *>(holder);
}

bool DecomposeMeetRequirementsForUt(const void *holder, const GNode &matched_node) {
  ++g_decompose_runtime_snapshot.meet_requirements_count;
  return g_decompose_runtime_snapshot.meet_requirements_result;
}

Status DecomposeReplacementForUt(const void *holder, const GNode &matched_node, GraphUniqPtr &replacement_graph) {
  ++g_decompose_runtime_snapshot.replacement_count;
  return g_decompose_runtime_snapshot.replacement_status;
}

void ResetPythonFusionBasePassRuntimeSnapshot() {
  g_python_fusion_base_runtime_snapshot = {};
}

void *CreatePythonFusionBasePassHolderForUt(const PythonPassDescriptor *pass_desc) {
  ++g_python_fusion_base_runtime_snapshot.create_count;
  if (pass_desc == nullptr) {
    return nullptr;
  }
  auto *holder = new (std::nothrow) PythonFusionBasePassHolderForUt{pass_desc->descriptor_key, pass_desc->pass_name};
  return holder;
}

void DestroyPythonFusionBasePassHolderForUt(void *holder) {
  ++g_python_fusion_base_runtime_snapshot.destroy_count;
  delete static_cast<PythonFusionBasePassHolderForUt *>(holder);
}

Status RunPythonFusionBasePassHolderForUt(const void *holder, GraphPtr &graph, CustomPassContext &pass_context) {
  ++g_python_fusion_base_runtime_snapshot.run_count;
  const auto *typed_holder = static_cast<const PythonFusionBasePassHolderForUt *>(holder);
  if (typed_holder != nullptr) {
    g_python_fusion_base_runtime_snapshot.last_descriptor_key = typed_holder->descriptor_key;
    g_python_fusion_base_runtime_snapshot.last_pass_name = typed_holder->pass_name;
  }
  g_python_fusion_base_runtime_snapshot.last_context_pass_name = pass_context.GetPassName().GetString();
  g_python_fusion_base_runtime_snapshot.last_graph_name = graph->GetName();
  return g_python_fusion_base_runtime_snapshot.run_status;
}
}  // namespace
using namespace ge::es;
class UtestFusionPassExecutor : public testing::Test {
 public:
  void SetUp() override {
    PreparePythonPathForSt();
    (void)unsetenv(kEnvPythonPassPath);
    PassRegistry::GetInstance().name_2_fusion_pass_regs_.clear();
    PassRegistry::GetInstance().descriptor_key_2_python_pass_descs_.clear();
    PassRegistry::GetInstance().pass_name_2_python_pass_create_contexts_.clear();
    ClearCurrentPythonPassCreateContext();
    ClearPythonPassRuntimeRegistry();
    ResetPythonFusionBasePassRuntimeSnapshot();
    ResetPatternFusionRuntimeSnapshot();
    ResetDecomposeRuntimeSnapshot();
    global_options_bak_ = ge::GetThreadLocalContext().GetAllGlobalOptions();
    session_options_bak_ = ge::GetThreadLocalContext().GetAllSessionOptions();
    graph_options_bak_ = ge::GetThreadLocalContext().GetAllGraphOptions();
  }
  void TearDown() override {
    (void)unsetenv(kEnvPythonPassPath);
    PassRegistry::GetInstance().name_2_fusion_pass_regs_.clear();
    PassRegistry::GetInstance().descriptor_key_2_python_pass_descs_.clear();
    PassRegistry::GetInstance().pass_name_2_python_pass_create_contexts_.clear();
    ClearCurrentPythonPassCreateContext();
    ClearPythonPassRuntimeRegistry();
    ResetPythonFusionBasePassRuntimeSnapshot();
    ResetPatternFusionRuntimeSnapshot();
    ResetDecomposeRuntimeSnapshot();

    GetThreadLocalContext().SetGlobalOption(global_options_bak_);
    GetThreadLocalContext().SetSessionOption(session_options_bak_);
    GetThreadLocalContext().SetGraphOption(graph_options_bak_);
    GetThreadLocalContext().GetOo().Initialize({}, OptionRegistry::GetInstance().GetRegisteredOptTable());
    RestorePythonPathForSt();
  }

 private:
  void PreparePythonPathForSt() {
#ifdef ST_FUSION_PASS_PY_INSTALL_DIR
    const char *old_python_path = getenv("PYTHONPATH");
    if (old_python_path != nullptr) {
      has_python_path_bak_ = true;
      python_path_bak_ = old_python_path;
      if (python_path_bak_.find(ST_FUSION_PASS_PY_INSTALL_DIR) == std::string::npos) {
        const std::string new_python_path = std::string(ST_FUSION_PASS_PY_INSTALL_DIR) + ":" + python_path_bak_;
        (void)setenv("PYTHONPATH", new_python_path.c_str(), 1);
      }
      PrependPythonPathForUt(ST_FUSION_PASS_PY_INSTALL_DIR);
      return;
    }
    (void)setenv("PYTHONPATH", ST_FUSION_PASS_PY_INSTALL_DIR, 1);
    PrependPythonPathForUt(ST_FUSION_PASS_PY_INSTALL_DIR);
#endif
  }

  void RestorePythonPathForSt() {
#ifdef ST_FUSION_PASS_PY_INSTALL_DIR
    RemovePythonPathForUt(ST_FUSION_PASS_PY_INSTALL_DIR);
    if (has_python_path_bak_) {
      (void)setenv("PYTHONPATH", python_path_bak_.c_str(), 1);
    } else {
      (void)unsetenv("PYTHONPATH");
    }
    python_path_bak_.clear();
    has_python_path_bak_ = false;
#endif
  }

  std::map<std::string, std::string> global_options_bak_;
  std::map<std::string, std::string> graph_options_bak_;
  std::map<std::string, std::string> session_options_bak_;
  std::string python_path_bak_;
  bool has_python_path_bak_{false};
};
/**
 * single node match
 *      data
 *        |
 *     transdata
 *        |
 *       out
 */
TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "DynamicRNNV3") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}, {CONSTANT}, {"Relu"}, {"Relu"}, {CONSTANT}}),
                "success");
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(1, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(2, {{"Relu"}}), "success");
    }
    if (node->GetName() == "x_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
    }
    if (node->GetName() == "y_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}}), "success");
    }
    if (node->GetType() == "Relu") {
      std::cout << node->GetName() << std::endl;
      // check attr
      std::vector<std::string> origin_types;
      const bool has_origin_op_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types);
      EXPECT_TRUE(has_origin_op_attr);
      EXPECT_TRUE(origin_types.size() == 1);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);
    }
  }
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}
TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_OptionToOff) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(pattern.release());
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  GetThreadLocalContext().GetOo().Initialize({{ge::OPTIMIZATION_SWITCH, "TransDataToReluPass:off"}},
                                             OptionRegistry::GetInstance().GetRegisteredOptTable());
  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  bool has_relu = false;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      has_relu = true;
    }
  }
  EXPECT_FALSE(has_relu);  // 图没有被改
}
TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_ConfigToOff) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(pattern.release());
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  std::string fusion_config_json_str =
      "{\n"
      "      \"Switch\":{\n"
      "          \"GraphFusion\":{\n"
      "            \"TransDataToReluPass\" : \"off\"\n"
      "          },\n"
      "          \"UBFusion\":{\n"
      "          }\n"
      "      }}";
  std::ofstream json_file("./fusion_switch_config.json");
  json_file << fusion_config_json_str << std::endl;

  // with option config
  std::string config_file_path = GetCodeDir() + "/fusion_switch_config.json";
  GetThreadLocalContext().SetGlobalOption({{FUSION_SWITCH_FILE, config_file_path}});

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  bool has_relu = false;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      has_relu = true;
    }
  }
  EXPECT_FALSE(has_relu);  // 图没有被改
  remove("./fusion_switch_config.json");
}

TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_ConfigAllOff_PassToOn) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(pattern.release());
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  std::string fusion_config_json_str =
      "{\n"
      "      \"Switch\":{\n"
      "          \"GraphFusion\":{\n"
      "            \"TransDataToReluPass\" : \"on\"\n"
      "            \"ALL\" : \"off\"\n"
      "          },\n"
      "          \"UBFusion\":{\n"
      "          }\n"
      "      }}";
  std::ofstream json_file("./fusion_switch_config.json");
  json_file << fusion_config_json_str << std::endl;

  // with option config
  std::string config_file_path = GetCodeDir() + "/fusion_switch_config.json";
  GetThreadLocalContext().SetGlobalOption({{FUSION_SWITCH_FILE, config_file_path}});

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  bool has_relu = false;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      has_relu = true;
    }
  }
  EXPECT_TRUE(has_relu);  // 图被改
  remove("./fusion_switch_config.json");
}

TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_ConfigAllOff) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(pattern.release());
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  std::string fusion_config_json_str =
      "{\n"
      "      \"Switch\":{\n"
      "          \"GraphFusion\":{\n"
      "            \"ALL\" : \"off\"\n"
      "          },\n"
      "          \"UBFusion\":{\n"
      "          }\n"
      "      }}";
  std::ofstream json_file("./fusion_switch_config.json");
  json_file << fusion_config_json_str << std::endl;

  // with option config
  std::string config_file_path = GetCodeDir() + "/fusion_switch_config.json";
  GetThreadLocalContext().SetGlobalOption({{FUSION_SWITCH_FILE, config_file_path}});

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  bool has_relu = false;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Relu") {
      has_relu = true;
    }
  }
  EXPECT_FALSE(has_relu);  // 图没有被改
  remove("./fusion_switch_config.json");
}

/**
 * shape to relu
 */
TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_WithSubgraph) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto shape = EsShape(data, DT_INT64);
      esb_graph->SetGraphOutput(shape, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::IfGraph2();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  NodePtr relu_node = nullptr;
  for (const auto &node : target_compute_graph->GetAllNodes()) {
    if (node->GetType() == "Relu") {
      relu_node = node;
    }
  }
  EXPECT_NE(relu_node, nullptr);
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}

TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_WithSubgraph_Failed) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto shape = EsShape(data, DT_INT64);
      esb_graph->SetGraphOutput(shape, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {  // invalid replacement
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      esb_graph->SetGraphOutput(data1, 1);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::IfGraph2();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  EXPECT_NE(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_NE(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), NOT_CHANGED);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  NodePtr relu_node = nullptr;
  for (const auto &node : target_compute_graph->GetAllNodes()) {
    if (node->GetType() == "Relu") {
      relu_node = node;
    }
  }
  EXPECT_EQ(relu_node, nullptr);
}

TEST_F(UtestFusionPassExecutor, PatternFusionPassReg_Run_BeforeInferShape_FAILED) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      // build pattern graph
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto transdata = EsTransData(data, "0", "29", 0, 0, 0);
      esb_graph->SetGraphOutput(transdata, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {  // invalid replacement
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      esb_graph->SetGraphOutput(data1, 1);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  auto ret = pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape);
  EXPECT_NE(ret, SUCCESS);
  EXPECT_NE(ret, NOT_CHANGED);
}

/**
 * single node match
 *      data
 *        |
 *     transdata
 *        |
 *       out
 */
TEST_F(UtestFusionPassExecutor, DecomposePass_Run_AfterInferShape) {
  // define pass
  class RunDecomposeTransDataPass : public DecomposePass {
   public:
    RunDecomposeTransDataPass(const std::vector<AscendString> &op_types) : DecomposePass(op_types) {}

   protected:
    bool MeetRequirements(const GNode &matched_node) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const GNode &matched_node) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_DECOMPOSE_PASS(RunDecomposeTransDataPass, {"TransData"}).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  GraphUtils::DumpGEGraphToOnnx(*target_compute_graph, "after_replace");
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "DynamicRNNV3") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}, {CONSTANT}, {"Relu"}, {"Relu"}, {CONSTANT}}),
                "success");
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(1, {{"Relu"}}), "success");
      EXPECT_EQ(checker.StrictConnectTo(2, {{"Relu"}}), "success");
    }
    if (node->GetName() == "x_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectTo(0, {{"Relu"}}), "success");
    }
    if (node->GetName() == "y_reshape") {
      auto checker = gert::NodeTopoChecker(node);
      EXPECT_EQ(checker.StrictConnectFrom({{"Relu"}, {CONSTANT}}), "success");
    }
    if (node->GetType() == "Relu") {
      std::cout << node->GetName() << std::endl;
      // check attr
      std::vector<std::string> origin_types;
      const bool has_origin_op_attr =
          ge::AttrUtils::GetListStr(node->GetOpDesc(), ATTR_NAME_DATA_DUMP_ORIGIN_OP_TYPES, origin_types);
      EXPECT_TRUE(has_origin_op_attr);
      EXPECT_TRUE(origin_types.size() == 1);
      EXPECT_STREQ(origin_types[0].c_str(), TRANSDATA);
    }
  }
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}

TEST_F(UtestFusionPassExecutor, DecomposePass_Run_AfterInferShape_Failed) {
  // define pass
  class RunDecomposeTransDataPass : public DecomposePass {
   public:
    RunDecomposeTransDataPass(const std::vector<AscendString> &op_type) : DecomposePass(op_type) {}

   protected:
    bool MeetRequirements(const GNode &matched_node) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const GNode &matched_node) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto data1 = EsCreateGraphInput(esb_graph, 1);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      esb_graph->SetGraphOutput(data1, 1);
      return replace_graph.BuildAndReset();
    }
  };
  REG_DECOMPOSE_PASS(RunDecomposeTransDataPass, {"TransData"}).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::LstmpGraph();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  FusionPassExecutor pass_executor;
  auto ret = pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape);
  EXPECT_NE(ret, SUCCESS);
  EXPECT_NE(ret, NOT_CHANGED);
}

TEST_F(UtestFusionPassExecutor, PythonFusionBasePass_Run_CreateExecuteDestroy) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.fusion.base.stage2";
  pass_desc.pass_name = "PythonFusionBaseStage2Pass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonFusionBaseStage2Pass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kFusionBase;

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePythonFusionBasePassHolderForUt;
  callbacks.destroy = DestroyPythonFusionBasePassHolderForUt;
  callbacks.run = RunPythonFusionBasePassHolderForUt;

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  {
    FusionPassExecutor pass_executor;
    EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
    EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.create_count, 1);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.run_count, 2);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.last_descriptor_key, pass_desc.descriptor_key);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.last_pass_name, pass_desc.pass_name);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.last_context_pass_name, pass_desc.pass_name);
  }
  EXPECT_EQ(g_python_fusion_base_runtime_snapshot.destroy_count, 1);
}

TEST_F(UtestFusionPassExecutor, PythonFusionBasePass_Run_Failed) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.fusion.base.stage2.failed";
  pass_desc.pass_name = "PythonFusionBaseStage2FailedPass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonFusionBaseStage2FailedPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kFusionBase;

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePythonFusionBasePassHolderForUt;
  callbacks.destroy = DestroyPythonFusionBasePassHolderForUt;
  callbacks.run = RunPythonFusionBasePassHolderForUt;

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));
  g_python_fusion_base_runtime_snapshot.run_status = FAILED;

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  {
    FusionPassExecutor pass_executor;
    const auto ret = pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape);
    EXPECT_EQ(ret, FAILED);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.create_count, 1);
    EXPECT_EQ(g_python_fusion_base_runtime_snapshot.run_count, 1);
  }
  EXPECT_EQ(g_python_fusion_base_runtime_snapshot.destroy_count, 1);
}

TEST_F(UtestFusionPassExecutor, PythonPassBridgeLoader_ProbesPathPythonRuntime) {
  ScopedTempDir temp_dir;
  const auto fake_python3 = temp_dir.CreateFilePath("python3");
  WriteFile(fake_python3, "#!/bin/sh\nprintf '\\n3.99.0\\n'\n");
  ASSERT_EQ(chmod(fake_python3.c_str(), 0700), 0);
  const auto fake_python = temp_dir.CreateFilePath("python");
  WriteFile(fake_python, "#!/bin/sh\nprintf 'cp399\\n3.99.0\\n'\n");
  ASSERT_EQ(chmod(fake_python.c_str(), 0700), 0);

  ScopedEnvVar scoped_path("PATH", temp_dir.FilePath(""));
  ScopedPyRuntimeProbeFailureForUt scoped_probe_failure;
  EXPECT_EQ(RegisterPythonPassesFromPlugin(), FAILED);
  ShutdownPythonPassesForProcess();
}

TEST_F(UtestFusionPassExecutor, PythonPassBridgeLoader_SkipsBrokenPrebuiltCandidate) {
  ScopedTempDir temp_dir;
  constexpr const char *kPythonTag = "cp399";
  const auto platform_tag = python_artifact::CurrentPlatformTag();
  const auto artifact_dir =
      std::string("site-packages/ge/passes/python_pass_artifacts/") + kPythonTag + "-" + platform_tag;
  temp_dir.MakeDir(artifact_dir);
  const auto broken_bridge_path = temp_dir.CreateFilePath(artifact_dir + "/libge_python_pass_bridge.so");
  const auto native_module_path = temp_dir.CreateFilePath(artifact_dir + "/_ge_pass_native.so");
  WriteFile(broken_bridge_path, "not a shared library");
  WriteFile(native_module_path, "native");
  WriteFile(temp_dir.CreateFilePath(artifact_dir + "/manifest.json"),
            "{\n"
            "  \"python_tag\": \"" +
                std::string(kPythonTag) +
                "\",\n"
                "  \"platform\": \"" +
                platform_tag +
                "\",\n"
                "  \"bridge_abi\": 1,\n"
                "  \"artifacts\": {\n"
                "    \"bridge\": \"libge_python_pass_bridge.so\",\n"
                "    \"native\": \"_ge_pass_native.so\"\n"
                "  }\n"
                "}\n");

  ScopedEnvVar scoped_python_path("PYTHONPATH", temp_dir.FilePath("site-packages"));
  auto runtime_key = ResolveRuntimeKeyForBridgeLoaderUt();
  const auto candidates = python_artifact::BuildPrebuiltBridgeLibraryCandidates(
      runtime_key, "", kPythonPassArtifactsRelativePath, kPythonFusionPassBridgeAbiVersion);
  ASSERT_EQ(candidates.size(), 1U);
  EXPECT_EQ(candidates.front().bridge_path, RealPath(broken_bridge_path.c_str()));
  EXPECT_EQ(candidates.front().artifact_root, RealPath(temp_dir.FilePath(artifact_dir).c_str()));
  EXPECT_EQ(candidates.front().native_module_path, RealPath(native_module_path.c_str()));

  python_bridge_loader::LoadedBridgeCandidate<PythonFusionPassBridgeApi> loaded_bridge;
  const auto status =
      python_bridge_loader::TryLoadBridgeCandidate<PythonFusionPassBridgeApi, PythonFusionPassBridgeArtifactConfig>(
          runtime_key, candidates.front(), BuildFailingBridgeLoadDepsForUt(),
          &python_pass_bridge_loader::IsBridgeApiValid, loaded_bridge);
  EXPECT_EQ(status, python_bridge_loader::BridgeLoadStatus::kOpenFailed);
  EXPECT_EQ(std::string(python_bridge_loader::BridgeLoadStatusToString(status)), "open failed");
  EXPECT_EQ(python_bridge_loader::BuildBridgeLoadErrorSuffix(status, "broken shared object"),
            ", dlerror[broken shared object]");
  EXPECT_TRUE(python_bridge_loader::BuildBridgeLoadErrorSuffix(status, nullptr).empty());
  EXPECT_TRUE(
      python_bridge_loader::BuildBridgeLoadErrorSuffix(python_bridge_loader::BridgeLoadStatus::kInvalidPath, "ignored")
          .empty());
  EXPECT_EQ(loaded_bridge.handle, nullptr);
}

TEST_F(UtestFusionPassExecutor, PythonPassBridgeCApi_ConfiguredNativeModulePathFailures) {
  EnsureSharedPybindPassFile();
  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPassFilePath());
  const auto *api = LoadBridgeApiForUt();
  ASSERT_NE(api, nullptr);
  ASSERT_NE(api->set_artifact_config, nullptr);
  ASSERT_NE(api->register_passes, nullptr);
  ASSERT_NE(api->reset_bridge_state, nullptr);

  api->reset_bridge_state();
  ForgetNativeModuleForUt();

  PythonFusionPassRegistrar registrar = {&RecordPythonPassFromDirectBridge};
  ScopedTempDir temp_dir;
  const auto invalid_spec_path = temp_dir.FilePath("native_without_so_suffix");
  PythonFusionPassBridgeArtifactConfig invalid_spec_config = {nullptr, invalid_spec_path.c_str()};
  ASSERT_EQ(api->set_artifact_config(&invalid_spec_config), SUCCESS);
  g_direct_bridge_registered_count = 0;
  EXPECT_EQ(api->register_passes(&registrar), FAILED);
  EXPECT_EQ(g_direct_bridge_registered_count, 0);

  const auto broken_so_path = temp_dir.CreateFilePath("broken_native.so");
  WriteFile(broken_so_path, "not a shared library");
  PythonFusionPassBridgeArtifactConfig broken_so_config = {nullptr, broken_so_path.c_str()};
  ASSERT_EQ(api->set_artifact_config(&broken_so_config), SUCCESS);
  g_direct_bridge_registered_count = 0;
  EXPECT_EQ(api->register_passes(&registrar), FAILED);
  EXPECT_EQ(g_direct_bridge_registered_count, 0);

  const auto raising_py_path = temp_dir.CreateFilePath("raising_native.py");
  WriteFile(raising_py_path, "raise RuntimeError('native load failed')\n");
  PythonFusionPassBridgeArtifactConfig raising_py_config = {nullptr, raising_py_path.c_str()};
  ASSERT_EQ(api->set_artifact_config(&raising_py_config), SUCCESS);
  g_direct_bridge_registered_count = 0;
  EXPECT_EQ(api->register_passes(&registrar), FAILED);
  EXPECT_EQ(g_direct_bridge_registered_count, 0);

  PythonFusionPassBridgeArtifactConfig empty_config = {nullptr, nullptr};
  ASSERT_EQ(api->set_artifact_config(&empty_config), SUCCESS);
  g_direct_bridge_registered_count = 0;
  EXPECT_EQ(api->register_passes(&registrar), FAILED);
  EXPECT_EQ(g_direct_bridge_registered_count, 0);
  CleanupBridgeStateForUt(*api);
}

TEST_F(UtestFusionPassExecutor, PythonPassPybindBridge_InterpreterInitializationFailed) {
  ScopedTempDir temp_dir;
  temp_dir.MakeDir("ge/passes");
  WriteFile(temp_dir.CreateFilePath("ge/__init__.py"), "");
  WriteFile(temp_dir.CreateFilePath("ge/passes/__init__.py"),
            "def clear_registered_passes():\n"
            "    pass\n");
  const auto native_module_path = temp_dir.CreateFilePath("ge/passes/fake_native.py");
  WriteFile(native_module_path, "configured_native_loaded = True\n");
  WriteFile(temp_dir.CreateFilePath("ge/passes/_bridge.py"),
            "def load_and_get_pass_descriptors():\n"
            "    return []\n"
            "\n"
            "def clear_pass_holders():\n"
            "    pass\n"
            "\n"
            "def clear_loaded_pass_modules():\n"
            "    pass\n");
  const auto *api = LoadBridgeApiForUt();
  ASSERT_NE(api, nullptr);
  ASSERT_NE(api->set_artifact_config, nullptr);
  ASSERT_NE(api->register_passes, nullptr);
  ASSERT_NE(api->reset_bridge_state, nullptr);

  ForgetNativeModuleForUt();
  const auto temp_python_path = temp_dir.FilePath("");
  const char *old_python_path = getenv("PYTHONPATH");
  const std::string python_path = ((old_python_path == nullptr) || (old_python_path[0] == '\0'))
                                      ? temp_python_path
                                      : (temp_python_path + ":" + old_python_path);
  ScopedEnvVar scoped_python_path("PYTHONPATH", python_path);
  ScopedPythonPathForUt scoped_sys_path(temp_python_path);
  PythonFusionPassBridgeArtifactConfig config = {nullptr, native_module_path.c_str()};
  ASSERT_EQ(api->set_artifact_config(&config), SUCCESS);
  api->reset_bridge_state();

  PythonFusionPassRegistrar registrar = {&RecordPythonPassFromDirectBridge};
  ASSERT_EQ(api->register_passes(&registrar), SUCCESS);
  ASSERT_NE(QueryRealPyIsInitializedForUt(), 0);
  api->reset_bridge_state();

  g_direct_bridge_registered_count = 0;
  {
    ScopedPyIsInitializedInitFailureForUt scoped_init_failure;
    EXPECT_EQ(api->register_passes(&registrar), FAILED);
    EXPECT_GE(g_py_is_initialized_call_count.load(std::memory_order_acquire), 2U);
  }
  api->reset_bridge_state();
  ForgetNativeModuleForUt();
  EXPECT_EQ(g_direct_bridge_registered_count, 0);
  CleanupBridgeStateForUt(*api);
}

TEST_F(UtestFusionPassExecutor, PythonPassBridgeCApi_LoadsConfiguredNativeModulePath) {
  EnsureSharedPybindPassFile();
  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPassFilePath());
  ScopedTempDir temp_dir;
  temp_dir.MakeDir("ge/passes");
  WriteFile(temp_dir.CreateFilePath("ge/__init__.py"), "");
  WriteFile(temp_dir.CreateFilePath("ge/passes/__init__.py"), "");
  const auto native_module_path = temp_dir.CreateFilePath("ge/passes/fake_native.py");
  WriteFile(native_module_path, "configured_native_loaded = True\n");
  WriteFile(temp_dir.CreateFilePath("ge/passes/_bridge.py"),
            "def load_and_get_pass_descriptors():\n"
            "    return [{\n"
            "        'descriptor_key': 'ut.configured:ConfiguredNativePass:ConfiguredNativePass',\n"
            "        'pass_name': 'ConfiguredNativePass',\n"
            "        'module_name': 'ut.configured',\n"
            "        'class_name': 'ConfiguredNativePass',\n"
            "        'stage': 'BeforeInferShape',\n"
            "        'kind': 'fusion_base',\n"
            "        'op_types': [],\n"
            "    }]\n"
            "\n"
            "def clear_pass_holders():\n"
            "    pass\n"
            "\n"
            "def clear_loaded_pass_modules():\n"
            "    pass\n");

  const auto *api = LoadBridgeApiForUt();
  ASSERT_NE(api, nullptr);
  ASSERT_NE(api->set_artifact_config, nullptr);
  ASSERT_NE(api->register_passes, nullptr);
  ASSERT_NE(api->reset_bridge_state, nullptr);

  PythonFusionPassBridgeArtifactConfig empty_config = {nullptr, nullptr};
  ASSERT_EQ(api->set_artifact_config(&empty_config), SUCCESS);
  ScopedEnvVar scoped_python_path("PYTHONPATH", temp_dir.FilePath(""));
  ScopedPythonPathForUt scoped_sys_path(temp_dir.FilePath(""));
  ForgetNativeModuleForUt();

  PythonFusionPassBridgeArtifactConfig config = {nullptr, native_module_path.c_str()};
  ASSERT_EQ(api->set_artifact_config(&config), SUCCESS);
  PythonFusionPassRegistrar registrar = {&RecordPythonPassFromDirectBridge};
  g_direct_bridge_registered_count = 0;
  ASSERT_EQ(api->register_passes(&registrar), SUCCESS);
  EXPECT_GT(g_direct_bridge_registered_count, 0);

  EXPECT_EQ(api->set_artifact_config(&config), FAILED);

  api->reset_bridge_state();
  g_direct_bridge_registered_count = 0;
  ASSERT_EQ(api->register_passes(&registrar), SUCCESS);
  EXPECT_GT(g_direct_bridge_registered_count, 0);

  api->reset_bridge_state();
  EXPECT_EQ(api->set_artifact_config(&empty_config), SUCCESS);
  CleanupBridgeStateForUt(*api);
}

TEST_F(UtestFusionPassExecutor, PythonFusionBasePass_PybindBridge_RunSuccess) {
  EnsureSharedPybindPassFile();
  const auto &marker_file = GetSharedPybindMarkerFilePath();
  const auto &report_fuse_marker_file = GetSharedPybindReportFuseMarkerFilePath();
  (void)remove(marker_file.c_str());
  (void)remove(report_fuse_marker_file.c_str());

  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPassFilePath());
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);
  ScopedLoadedPythonPassForUt loaded_python_pass;

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  const auto expected_graph_name = target_compute_graph->GetName();
  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_EQ(ReadFile(marker_file), expected_graph_name + "|PythonPybindBridgePass");
  EXPECT_EQ(ReadFile(report_fuse_marker_file), "PythonPybindReportFusePass");
}

TEST_F(UtestFusionPassExecutor, PythonFusionBasePass_PybindBridge_RunFailedOnPythonException) {
  EnsureSharedPybindPassFile();
  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPassFilePath());
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);
  ScopedLoadedPythonPassForUt loaded_python_pass;

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterBuiltinFusionPass), FAILED);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_PybindBridge_RunSuccess) {
  EnsureSharedPybindPatternPassFile();
  const auto &marker_file = GetSharedPybindPatternMarkerFilePath();
  (void)remove(marker_file.c_str());

  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPatternPassFilePath());
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);
  ScopedLoadedPythonPassForUt loaded_python_pass;

  auto target_graph = ge::es::EsGraphBuilder("python_pattern_target");
  auto *esb_graph = target_graph.GetCGraphBuilder();
  auto data = EsCreateGraphInput(esb_graph, 0);
  auto zero = EsCreateScalarFloat(esb_graph, 0.0f);
  auto add = EsAdd(data, zero);
  esb_graph->SetGraphOutput(add, 0);
  auto target_ge_graph = target_graph.BuildAndReset();
  auto target_compute_graph = GraphUtilsEx::GetComputeGraph(*target_ge_graph);
  ASSERT_NE(target_compute_graph, nullptr);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_EQ(ReadFile(marker_file), "add_zero_pattern|input_0:0");

  uint32_t add_node_count = 0;
  uint32_t const_node_count = 0;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Add") {
      ++add_node_count;
    }
    if (node->GetType() == CONSTANT) {
      ++const_node_count;
    }
  }
  EXPECT_EQ(add_node_count, 0U);
  EXPECT_EQ(const_node_count, 0U);
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_PybindBridge_MatcherConfigTakesEffect) {
  EnsureSharedPybindPatternMatcherConfigPassFile();
  const auto &marker_file = GetSharedPybindPatternMatcherConfigMarkerFilePath();
  (void)remove(marker_file.c_str());

  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindPatternMatcherConfigPassFilePath());
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);
  ScopedLoadedPythonPassForUt loaded_python_pass;

  auto target_graph = ge::es::EsGraphBuilder("python_pattern_matcher_config_target");
  auto *esb_graph = target_graph.GetCGraphBuilder();
  auto data = EsCreateGraphInput(esb_graph, 0);
  auto zero = EsCreateScalarFloat(esb_graph, 0.0f);
  auto add = EsAdd(data, zero);
  esb_graph->SetGraphOutput(add, 0);
  auto target_ge_graph = target_graph.BuildAndReset();
  auto target_compute_graph = GraphUtilsEx::GetComputeGraph(*target_ge_graph);
  ASSERT_NE(target_compute_graph, nullptr);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
  EXPECT_EQ(access(marker_file.c_str(), F_OK), -1);

  uint32_t add_node_count = 0;
  uint32_t const_node_count = 0;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Add") {
      ++add_node_count;
    }
    if (node->GetType() == CONSTANT) {
      ++const_node_count;
    }
  }
  EXPECT_EQ(add_node_count, 1U);
  EXPECT_EQ(const_node_count, 1U);
}

TEST_F(UtestFusionPassExecutor, PythonDecomposePass_PybindBridge_RunSuccess) {
  EnsureSharedPybindDecomposePassFile();
  const auto &marker_file = GetSharedPybindDecomposeMarkerFilePath();
  (void)remove(marker_file.c_str());

  ScopedEnvVar scoped_py_pass_path(kEnvPythonPassPath, GetSharedPybindDecomposePassFilePath());
  ASSERT_EQ(RegisterPythonPassesFromPlugin(), SUCCESS);
  ScopedLoadedPythonPassForUt loaded_python_pass;

  auto target_graph = ge::es::EsGraphBuilder("python_decompose_target");
  auto *esb_graph = target_graph.GetCGraphBuilder();
  auto data = EsCreateGraphInput(esb_graph, 0);
  auto zero = EsCreateScalarFloat(esb_graph, 0.0f);
  auto add = EsAdd(data, zero);
  esb_graph->SetGraphOutput(add, 0);
  auto target_ge_graph = target_graph.BuildAndReset();
  auto target_compute_graph = GraphUtilsEx::GetComputeGraph(*target_ge_graph);
  ASSERT_NE(target_compute_graph, nullptr);

  FusionPassExecutor pass_executor;
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);

  const auto marker = ReadFile(marker_file);
  EXPECT_NE(marker.find("|Add"), std::string::npos);

  uint32_t add_node_count = 0;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    if (node->GetType() == "Add") {
      ++add_node_count;
    }
  }
  EXPECT_EQ(add_node_count, 0U);
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_CreateAndDestroy) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.pattern.fusion.create";
  pass_desc.pass_name = "PythonPatternFusionCreatePass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonPatternFusionCreatePass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kPatternFusion;

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePatternFusionPassHolderForUt;
  callbacks.destroy = DestroyPatternFusionPassHolderForUt;
  callbacks.patterns = GetPatternsForUt;
  callbacks.replacement = ReplacementForUt;

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));

  {
    auto adapter = std::make_unique<PythonPatternFusionPassAdapter>(pass_desc);
    ASSERT_TRUE(adapter->IsValid());
    EXPECT_EQ(g_pattern_fusion_runtime_snapshot.create_count, 1);

    // Patterns callback should be invoked
    auto patterns = adapter->Patterns();
    EXPECT_EQ(g_pattern_fusion_runtime_snapshot.patterns_count, 1);
  }
  EXPECT_EQ(g_pattern_fusion_runtime_snapshot.destroy_count, 1);
}

TEST_F(UtestFusionPassExecutor, PythonDecomposePass_CreateAndDestroy) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.decompose.create";
  pass_desc.pass_name = "PythonDecomposeCreatePass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonDecomposeCreatePass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kDecompose;
  pass_desc.op_types = {"Add"};

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreateDecomposePassHolderForUt;
  callbacks.destroy = DestroyDecomposePassHolderForUt;
  callbacks.decompose_replacement = DecomposeReplacementForUt;

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));

  {
    auto adapter = std::make_unique<PythonDecomposePassAdapter>(pass_desc);
    ASSERT_TRUE(adapter->IsValid());
    EXPECT_EQ(g_decompose_runtime_snapshot.create_count, 1);
  }
  EXPECT_EQ(g_decompose_runtime_snapshot.destroy_count, 1);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_MeetRequirements_DefaultFallback) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.pattern.fusion.meetreq";
  pass_desc.pass_name = "PythonPatternFusionMeetReqPass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonPatternFusionMeetReqPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kPatternFusion;

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePatternFusionPassHolderForUt;
  callbacks.destroy = DestroyPatternFusionPassHolderForUt;
  callbacks.patterns = GetPatternsForUt;
  callbacks.replacement = ReplacementForUt;
  // meet_requirements is nullptr -> should fall back to PatternFusionPass::MeetRequirements (return true)

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));

  auto adapter = std::make_unique<PythonPatternFusionPassAdapter>(pass_desc);
  ASSERT_TRUE(adapter->IsValid());

  // meet_requirements callback is nullptr -> falls back to PatternFusionPass::MeetRequirements (returns true)
  auto null_result = std::unique_ptr<MatchResult>();
  EXPECT_TRUE(adapter->MeetRequirements(null_result));

  // meet_requirements callback is nullptr, so count should stay 0
  EXPECT_EQ(g_pattern_fusion_runtime_snapshot.meet_requirements_count, 0);
}

TEST_F(UtestFusionPassExecutor, PythonDecomposePass_MeetRequirements_DefaultFallback) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.decompose.meetreq";
  pass_desc.pass_name = "PythonDecomposeMeetReqPass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonDecomposeMeetReqPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kDecompose;
  pass_desc.op_types = {"Add"};

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreateDecomposePassHolderForUt;
  callbacks.destroy = DestroyDecomposePassHolderForUt;
  callbacks.decompose_replacement = DecomposeReplacementForUt;

  ASSERT_TRUE(RegisterPythonPass(pass_desc, callbacks));

  auto adapter = std::make_unique<PythonDecomposePassAdapter>(pass_desc);
  ASSERT_TRUE(adapter->IsValid());

  auto target_compute_graph = gert::ShareGraph::BuildSingleNodeGraph();
  ge::NodePtr matched_node_ptr = nullptr;
  for (const auto &node : target_compute_graph->GetDirectNode()) {
    matched_node_ptr = node;
    break;
  }
  ASSERT_NE(matched_node_ptr, nullptr);
  const auto matched_node = NodeAdapter::Node2GNode(matched_node_ptr);
  EXPECT_TRUE(adapter->MeetRequirements(matched_node));
  EXPECT_EQ(g_decompose_runtime_snapshot.meet_requirements_count, 0);
}

TEST_F(UtestFusionPassExecutor, PythonPatternFusionPass_InvalidOnMissingRequiredCallbacks) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.pattern.fusion.invalid";
  pass_desc.pass_name = "PythonPatternFusionInvalidPass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonPatternFusionInvalidPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kPatternFusion;

  // Missing patterns and replacement -> IsValid() should return false from callbacks validation
  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreatePatternFusionPassHolderForUt;
  callbacks.destroy = DestroyPatternFusionPassHolderForUt;
  // patterns and replacement are nullptr

  EXPECT_FALSE(callbacks.IsValid(pass_desc.kind));
  EXPECT_FALSE(RegisterPythonPass(pass_desc, callbacks));
}

TEST_F(UtestFusionPassExecutor, PythonDecomposePass_InvalidOnMissingRequiredCallbacks) {
  PythonPassDescriptor pass_desc;
  pass_desc.descriptor_key = "python.decompose.invalid";
  pass_desc.pass_name = "PythonDecomposeInvalidPass";
  pass_desc.module_name = "python.pass.sample";
  pass_desc.class_name = "PythonDecomposeInvalidPass";
  pass_desc.stage = CustomPassStage::kAfterInferShape;
  pass_desc.kind = PythonPassKind::kDecompose;
  pass_desc.op_types = {"Add"};

  PythonFusionPassCallbacks callbacks;
  callbacks.create = CreateDecomposePassHolderForUt;
  callbacks.destroy = DestroyDecomposePassHolderForUt;

  EXPECT_FALSE(callbacks.IsValid(pass_desc.kind));
  EXPECT_FALSE(RegisterPythonPass(pass_desc, callbacks));
}

TEST_F(UtestFusionPassExecutor, PythonPassHolder_ReusedAcrossAdapters) {
  // Verify PythonPassHolder lifecycle works for FusionBase
  PythonPassDescriptor base_desc;
  base_desc.descriptor_key = "python.holder.base";
  base_desc.pass_name = "PythonHolderBasePass";
  base_desc.module_name = "python.pass.sample";
  base_desc.class_name = "PythonHolderBasePass";
  base_desc.stage = CustomPassStage::kAfterInferShape;
  base_desc.kind = PythonPassKind::kFusionBase;

  PythonFusionPassCallbacks base_callbacks;
  base_callbacks.create = CreatePythonFusionBasePassHolderForUt;
  base_callbacks.destroy = DestroyPythonFusionBasePassHolderForUt;
  base_callbacks.run = RunPythonFusionBasePassHolderForUt;

  ASSERT_TRUE(RegisterPythonPass(base_desc, base_callbacks));

  // Verify PythonPassHolder lifecycle works for PatternFusion
  PythonPassDescriptor pattern_desc;
  pattern_desc.descriptor_key = "python.holder.pattern";
  pattern_desc.pass_name = "PythonHolderPatternPass";
  pattern_desc.module_name = "python.pass.sample";
  pattern_desc.class_name = "PythonHolderPatternPass";
  pattern_desc.stage = CustomPassStage::kAfterInferShape;
  pattern_desc.kind = PythonPassKind::kPatternFusion;

  PythonFusionPassCallbacks pattern_callbacks;
  pattern_callbacks.create = CreatePatternFusionPassHolderForUt;
  pattern_callbacks.destroy = DestroyPatternFusionPassHolderForUt;
  pattern_callbacks.patterns = GetPatternsForUt;
  pattern_callbacks.replacement = ReplacementForUt;

  ASSERT_TRUE(RegisterPythonPass(pattern_desc, pattern_callbacks));

  // Both adapters should be valid and independently manageable
  auto base_adapter = std::make_unique<PythonFusionBasePassAdapter>(base_desc);
  auto pattern_adapter = std::make_unique<PythonPatternFusionPassAdapter>(pattern_desc);
  EXPECT_TRUE(base_adapter->IsValid());
  EXPECT_TRUE(pattern_adapter->IsValid());

  // Verify independent create/destroy
  EXPECT_EQ(g_python_fusion_base_runtime_snapshot.create_count, 1);
  EXPECT_EQ(g_pattern_fusion_runtime_snapshot.create_count, 1);

  base_adapter.reset();
  EXPECT_EQ(g_python_fusion_base_runtime_snapshot.destroy_count, 1);
  // Pattern adapter should still be valid
  EXPECT_TRUE(pattern_adapter->IsValid());

  pattern_adapter.reset();
  EXPECT_EQ(g_pattern_fusion_runtime_snapshot.destroy_count, 1);
}
/**
 * Subgraph 的 parent node 被人为悬空（owner_graph 置空）后，RunPasses 在遍历子图时
 * 必须跳过这种孤儿子图（fusion_pass_executor.cc:80,82），而不是喂给 PatternMatcher。
 */
TEST_F(UtestFusionPassExecutor, RunPasses_SkipOrphanSubgraph) {
  // define pass
  class TransDataToReluPass : public PatternFusionPass {
   protected:
    std::vector<PatternUniqPtr> Patterns() override {
      std::vector<PatternUniqPtr> patterns;
      auto pattern_graph = ge::es::EsGraphBuilder("pattern");
      auto esb_graph = pattern_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto shape = EsShape(data, DT_INT64);
      esb_graph->SetGraphOutput(shape, 0);
      auto pattern = std::make_unique<Pattern>(std::move(*pattern_graph.BuildAndReset()));
      patterns.emplace_back(std::move(pattern));
      return patterns;
    }
    bool MeetRequirements(const std::unique_ptr<MatchResult> &match_result) override {
      return true;
    }
    std::unique_ptr<Graph> Replacement(const unique_ptr<MatchResult> &match_result) override {
      auto replace_graph = ge::es::EsGraphBuilder("replacement");
      auto esb_graph = replace_graph.GetCGraphBuilder();
      auto data = EsCreateGraphInput(esb_graph, 0);
      auto relu = EsRelu(data);
      esb_graph->SetGraphOutput(relu, 0);
      return replace_graph.BuildAndReset();
    }
  };
  REG_FUSION_PASS(TransDataToReluPass).Stage(CustomPassStage::kAfterInferShape);

  auto target_compute_graph = gert::ShareGraph::IfGraph2();
  auto target_graph = GraphUtilsEx::CreateGraphPtrFromComputeGraph(target_compute_graph);

  // 人为制造孤儿子图：把某个子图的 parent node 的 owner_graph 置空。
  const auto subgraphs = target_compute_graph->GetAllSubgraphs();
  ASSERT_FALSE(subgraphs.empty());
  size_t orphaned = 0U;
  for (const auto &sub : subgraphs) {
    ASSERT_NE(sub, nullptr);
    const auto parent_node = sub->GetParentNode();
    if (parent_node != nullptr) {
      EXPECT_EQ(parent_node->ClearOwnerGraph(nullptr), GRAPH_SUCCESS);
      EXPECT_EQ(parent_node->GetOwnerComputeGraph(), nullptr);
      ++orphaned;
    }
  }
  ASSERT_GT(orphaned, 0U);

  FusionPassExecutor pass_executor;
  // 顶层图能正常处理，孤儿子图被安全跳过，不应崩溃。
  EXPECT_EQ(pass_executor.RunPasses(target_compute_graph, CustomPassStage::kAfterInferShape), SUCCESS);
}
}  // namespace fusion
}  // namespace ge
