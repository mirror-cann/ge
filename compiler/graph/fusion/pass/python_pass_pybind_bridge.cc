/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "python_pass_pybind_bridge.h"
#include "python_pass_bridge_c_api.h"

#include "Python.h"
#ifdef ASCEND_CI_LIMITED_PY37
#undef PyCFunction_NewEx
#endif

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <mutex>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pybind11/embed.h"
#include "pybind11/stl.h"
#include "framework/common/debug/ge_log.h"
#include "graph/ascend_string.h"
#include "graph/utils/graph_utils_ex.h"
#include "graph_metadef/register/custom_pass_context_impl.h"
#include "python_pass_adapter.h"

#undef PYBIND11_CHECK_PYTHON_VERSION
#define PYBIND11_CHECK_PYTHON_VERSION

namespace ge {
namespace fusion {
namespace py = pybind11;
namespace {
constexpr const char *kBridgeModuleName = "ge.passes._bridge";
constexpr const char *kPassNativeModuleName = "ge.passes._ge_pass_native";
constexpr const char *kPassModuleName = "ge.passes";
constexpr const char *kPassKindFusionBase = "fusion_base";
constexpr const char *kEnvPythonPassPath = "ASCEND_GE_PY_PASS_PATH";

struct PythonBridgeHolder {
  std::string descriptor_key;
  std::string instance_id;
};

CustomPassStage ParseCustomPassStage(const std::string &stage_name) {
  static const std::map<std::string, CustomPassStage> kStageMap = {
      {"BeforeInferShape", CustomPassStage::kBeforeInferShape},
      {"AfterInferShape", CustomPassStage::kAfterInferShape},
      {"AfterAssignLogicStream", CustomPassStage::kAfterAssignLogicStream},
      {"AfterBuiltinFusionPass", CustomPassStage::kAfterBuiltinFusionPass},
      {"AfterOriginGraphOptimize", CustomPassStage::kAfterOriginGraphOptimize},
      {"CompatibleInherited", CustomPassStage::kCompatibleInherited},
  };
  const auto iter = kStageMap.find(stage_name);
  return (iter == kStageMap.cend()) ? CustomPassStage::kInvalid : iter->second;
}

bool ParsePythonPassKind(const std::string &kind_name, PythonPassKind &kind) {
  if (kind_name == kPassKindFusionBase) {
    kind = PythonPassKind::kFusionBase;
    return true;
  }
  if (kind_name == "pattern_fusion") {
    kind = PythonPassKind::kPatternFusion;
    return true;
  }
  if (kind_name == "decompose") {
    kind = PythonPassKind::kDecompose;
    return true;
  }
  return false;
}

class PythonFusionPassPybindBridge {
 public:
  static PythonFusionPassPybindBridge &GetInstance() {
    static PythonFusionPassPybindBridge instance;
    return instance;
  }

  Status SetArtifactConfig(const PythonFusionPassBridgeArtifactConfig *config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bridge_module_ && (!bridge_module_.is_none())) {
      GELOGW("Set python pass artifact config failed because bridge module has been imported.");
      return FAILED;
    }
    artifact_root_ = ((config == nullptr) || (config->artifact_root == nullptr)) ? "" : config->artifact_root;
    native_module_path_ =
        ((config == nullptr) || (config->native_module_path == nullptr)) ? "" : config->native_module_path;
    GELOGI("Set python pass artifact config, artifact root[%s], native module path[%s].",
           artifact_root_.c_str(), native_module_path_.c_str());
    return SUCCESS;
  }

  Status RegisterPythonPasses(const PythonFusionPassRegistrar &registrar) {
    GELOGI("Begin to register python passes through pybind bridge.");
    const auto prepare_ret = EnsureBridgeReady();
    if (prepare_ret != SUCCESS) {
      GELOGE(prepare_ret, "Prepare python pybind bridge failed.");
      return prepare_ret;
    }
    py::gil_scoped_acquire gil;
    py::object descriptors_obj;
    try {
      descriptors_obj = bridge_module_.attr("load_and_get_pass_descriptors")();
    } catch (const py::error_already_set &err) {
      GELOGE(FAILED, "Load python pass descriptors failed: %s", err.what());
      return FAILED;
    }

    const py::list descriptor_list = descriptors_obj.cast<py::list>();
    for (const auto &item : descriptor_list) {
      PythonPassDescriptor pass_desc;
      const auto parse_ret = ParseDescriptor(item.cast<py::dict>(), pass_desc);
      if (parse_ret != SUCCESS) {
        GELOGE(parse_ret, "Parse python pass descriptor failed.");
        return parse_ret;
      }
      const auto callbacks = GetCallbacks(pass_desc.kind);
      if ((registrar.register_pass == nullptr) || (!registrar.register_pass(&pass_desc, &callbacks))) {
        GELOGE(FAILED, "Register python pass[%s] failed.", pass_desc.pass_name.c_str());
        return FAILED;
      }
      GELOGI("Python pass[%s] is registered from pybind bridge.", pass_desc.pass_name.c_str());
    }
    return SUCCESS;
  }

  void ResetBridgeState() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (Py_IsInitialized() == 0) {
      GELOGI("Skip resetting python pybind bridge state because interpreter is not initialized.");
      return;
    }
    py::gil_scoped_acquire gil;
    GELOGI("Resetting python pybind bridge state with existing interpreter.");
    ResetBridgeStateUnlocked();
  }

  void Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    GELOGI("Shutting down python pybind bridge, owns_interpreter[%d], py_initialized[%d].",
           owns_interpreter_ ? 1 : 0,
           Py_IsInitialized() != 0 ? 1 : 0);
    if (Py_IsInitialized() != 0) {
      py::gil_scoped_acquire gil;
      ResetBridgeStateUnlocked();
    }
    if (owns_interpreter_ && (Py_IsInitialized() != 0)) {
      try {
        // 只有 bridge 自己拉起了解释器，才在进程级 shutdown 时回收它；
        // 如果解释器来自进程中的其他 Python 用户，这里只清理 bridge 自己的状态。
        py::finalize_interpreter();
      } catch (const std::exception &err) {
        GELOGW("Finalize python pybind bridge interpreter failed: %s", err.what());
      }
    }
    owns_interpreter_ = false;
  }

  void *CreateHolder(const PythonPassDescriptor &pass_desc) {
    if (EnsureBridgeReady() != SUCCESS) {
      GELOGW("Prepare python bridge failed when creating holder.");
      return nullptr;
    }
    py::gil_scoped_acquire gil;
    const std::string instance_id = BuildInstanceId(pass_desc.descriptor_key);
    try {
      const bool created = bridge_module_.attr("create_pass_holder")(instance_id, pass_desc.descriptor_key).cast<bool>();
      if (!created) {
        GELOGW("Create python pass holder failed, descriptor key[%s], instance id[%s].",
               pass_desc.descriptor_key.c_str(), instance_id.c_str());
        return nullptr;
      }
    } catch (const py::error_already_set &err) {
      GELOGW("Create python pass holder failed, descriptor key[%s], instance id[%s]: %s",
             pass_desc.descriptor_key.c_str(), instance_id.c_str(), err.what());
      return nullptr;
    }
    return new (std::nothrow) PythonBridgeHolder{pass_desc.descriptor_key, instance_id};
  }

  void DestroyHolder(PythonBridgeHolder *holder) {
    if (holder == nullptr) {
      return;
    }
    if (Py_IsInitialized() != 0) {
      py::gil_scoped_acquire gil;
      try {
        EnsureBridgeModuleUnlocked();
        (void)bridge_module_.attr("destroy_pass_holder")(holder->instance_id);
      } catch (const py::error_already_set &err) {
        GELOGW("Destroy python pass holder failed, descriptor key[%s], instance id[%s]: %s",
               holder->descriptor_key.c_str(), holder->instance_id.c_str(), err.what());
      }
    }
    delete holder;
  }

  Status Run(const PythonBridgeHolder *holder, const GraphPtr &graph, CustomPassContext &pass_context) {
    if (holder == nullptr) {
      pass_context.SetErrorMessage("python bridge holder is null");
      return FAILED;
    }
    const auto prepare_ret = EnsureBridgeReady();
    if (prepare_ret != SUCCESS) {
      GELOGE(prepare_ret, "Prepare python bridge failed.");
      return prepare_ret;
    }
    py::gil_scoped_acquire gil;
    try {
      py::object result = bridge_module_.attr("run_fusion_base_pass")(holder->instance_id,
                                                                      BuildPythonGraph(graph),
                                                                      BuildPythonPassContext(pass_context));
      return TranslateRunResult(result, pass_context);
    } catch (const py::error_already_set &err) {
      pass_context.SetErrorMessage(AscendString(err.what()));
      GELOGE(FAILED, "Run python pass failed, descriptor key[%s], instance id[%s]: %s",
             holder->descriptor_key.c_str(), holder->instance_id.c_str(), err.what());
      return FAILED;
    }
  }

 private:
  Status EnsureBridgeReady() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (Py_IsInitialized() == 0) {
      try {
        py::initialize_interpreter();
        owns_interpreter_ = true;
      } catch (const std::exception &err) {
        GELOGW("Python interpreter initialization failed: %s", err.what());
        return FAILED;
      }
    }
    py::gil_scoped_acquire gil;
    try {
      SyncProcessEnvToPythonUnlocked();
      EnsureBridgeModuleUnlocked();
    } catch (const py::error_already_set &err) {
      GELOGW("Prepare GE python pass module failed: %s", err.what());
      return FAILED;
    } catch (const std::exception &err) {
      GELOGW("Prepare GE python pass module failed: %s", err.what());
      return FAILED;
    }
    return SUCCESS;
  }

  void EnsureBridgeModuleUnlocked() {
    if (bridge_module_ && (!bridge_module_.is_none())) {
      GELOGI("Reusing cached python bridge module.");
      return;
    }
    py::object pass_native_module = LoadPassNativeModuleUnlocked();
    (void)pass_native_module;
    bridge_module_ = py::module_::import(kBridgeModuleName);
    GELOGI("Imported python bridge modules [%s] and [%s].", kPassNativeModuleName, kBridgeModuleName);
  }

  py::object LoadPassNativeModuleUnlocked() const {
    if (native_module_path_.empty()) {
      return py::module_::import(kPassNativeModuleName);
    }

    py::module_ sys = py::module_::import("sys");
    py::dict modules = sys.attr("modules");
    py::object module_name = py::str(kPassNativeModuleName);
    py::object loaded_module = modules.attr("get")(module_name, py::none());
    if (!loaded_module.is_none()) {
      GELOGI("Reuse configured python native module [%s].", native_module_path_.c_str());
      return loaded_module;
    }

    py::module_ importlib_util = py::module_::import("importlib.util");
    py::object spec = importlib_util.attr("spec_from_file_location")(module_name, py::str(native_module_path_));
    if (spec.is_none()) {
      throw std::runtime_error("cannot create import spec for " + native_module_path_);
    }
    py::object module = importlib_util.attr("module_from_spec")(spec);
    modules[module_name] = module;
    try {
      spec.attr("loader").attr("exec_module")(module);
    } catch (...) {
      (void)modules.attr("pop")(module_name, py::none());
      throw;
    }
    GELOGI("Loaded configured python native module [%s].", native_module_path_.c_str());
    return module;
  }

  void ResetBridgeStateUnlocked() {
    try {
      EnsureBridgeModuleUnlocked();
      (void)bridge_module_.attr("clear_pass_holders")();
      (void)bridge_module_.attr("clear_loaded_pass_modules")();
      (void)py::module_::import(kPassModuleName).attr("clear_registered_passes")();
    } catch (const py::error_already_set &err) {
      GELOGW("Reset python pybind bridge state failed: %s", err.what());
    }
    bridge_module_ = py::object();
    try {
      (void)py::module_::import("gc").attr("collect")();
    } catch (const py::error_already_set &err) {
      GELOGW("Python gc.collect() during reset failed: %s", err.what());
    }
  }

  static void ClearPythonEnvVarUnlocked() {
    py::module_ os = py::module_::import("os");
    py::object environ = os.attr("environ");
    (void)environ.attr("pop")(py::str(kEnvPythonPassPath), py::none());
  }

  static void SyncProcessEnvToPythonUnlocked() {
    const char *env_value = std::getenv(kEnvPythonPassPath);
    const bool has_env_value = (env_value != nullptr);
    const std::string env_value_str = has_env_value ? std::string(env_value) : std::string();
    // os.environ.pop may call unsetenv(), so keep the current C env value before clearing Python's cache.
    ClearPythonEnvVarUnlocked();
    if (!has_env_value) {
      return;
    }
    py::module_ os = py::module_::import("os");
    py::object environ = os.attr("environ");
    environ[py::str(kEnvPythonPassPath)] = py::str(env_value_str);
  }

  std::string BuildInstanceId(const std::string &descriptor_key) {
    const uint64_t sequence = ++instance_seq_;
    std::ostringstream oss;
    oss << descriptor_key << "#" << sequence;
    return oss.str();
  }

  static Status ParseDescriptor(const py::dict &descriptor_dict, PythonPassDescriptor &pass_desc) {
    try {
      pass_desc.descriptor_key = py::str(descriptor_dict["descriptor_key"]);
      pass_desc.pass_name = py::str(descriptor_dict["pass_name"]);
      pass_desc.module_name = py::str(descriptor_dict["module_name"]);
      pass_desc.class_name = py::str(descriptor_dict["class_name"]);
      pass_desc.stage = ParseCustomPassStage(py::str(descriptor_dict["stage"]));
      if (pass_desc.stage == CustomPassStage::kInvalid) {
        GELOGE(FAILED, "Unsupported python pass stage for pass[%s].", pass_desc.pass_name.c_str());
        return FAILED;
      }
      PythonPassKind kind;
      if (!ParsePythonPassKind(py::str(descriptor_dict["kind"]), kind)) {
        GELOGE(FAILED, "Unsupported python pass kind for pass[%s].", pass_desc.pass_name.c_str());
        return FAILED;
      }
      pass_desc.kind = kind;
      if (descriptor_dict.contains("op_types")) {
        pass_desc.op_types = descriptor_dict["op_types"].cast<std::vector<std::string>>();
      }
    } catch (const py::error_already_set &err) {
      GELOGE(FAILED, "Parse python pass descriptor failed: %s", err.what());
      return FAILED;
    }
    return pass_desc.IsValid() ? SUCCESS : FAILED;
  }

  static py::object BuildPythonGraph(const GraphPtr &graph) {
    if (graph == nullptr) {
      return py::none();
    }
    py::module_ ctypes_module = py::module_::import("ctypes");
    py::module_ graph_module = py::module_::import("ge.graph");
    py::object graph_type = graph_module.attr("Graph");
    py::object graph_handle = ctypes_module.attr("c_void_p")(py::int_(reinterpret_cast<uintptr_t>(graph.get())));
    py::capsule graph_owner(new (std::nothrow) GraphPtr(graph), [](void *ptr) {
      delete static_cast<GraphPtr *>(ptr);
    });
    return graph_type.attr("_create_from")(graph_handle, py::bool_(false), graph_owner);
  }

  static py::object BuildPythonPassContext(CustomPassContext &pass_context) {
    return py::cast(&pass_context, py::return_value_policy::reference);
  }

  static Status TranslateRunResult(const py::object &result, CustomPassContext &pass_context) {
    if (result.is_none()) {
      return SUCCESS;
    }
    if (py::isinstance<py::bool_>(result)) {
      return result.cast<bool>() ? SUCCESS : FAILED;
    }
    if (py::isinstance<py::int_>(result)) {
      return static_cast<Status>(result.cast<uint32_t>());
    }
    std::ostringstream oss;
    oss << "python pass returned unsupported type: " << std::string(py::str(py::type::of(result)));
    pass_context.SetErrorMessage(AscendString(oss.str().c_str()));
    GELOGE(FAILED, "%s", oss.str().c_str());
    return FAILED;
  }

  static void SetCommonCallbacks(PythonFusionPassCallbacks &callbacks) {
    callbacks.create = [](const PythonPassDescriptor *pass_desc) -> void *{
      if (pass_desc == nullptr) {
        return nullptr;
      }
      return PythonFusionPassPybindBridge::GetInstance().CreateHolder(*pass_desc);
    };
    callbacks.destroy = [](void *holder) {
      PythonFusionPassPybindBridge::GetInstance().DestroyHolder(static_cast<PythonBridgeHolder *>(holder));
    };
  }

  static void SetFusionBaseCallbacks(PythonFusionPassCallbacks &callbacks) {
    callbacks.run = [](const void *holder, GraphPtr &graph, CustomPassContext &pass_context) -> Status {
      return PythonFusionPassPybindBridge::GetInstance().Run(static_cast<const PythonBridgeHolder *>(holder),
                                                             graph,
                                                             pass_context);
    };
  }

  static void SetPatternFusionCallbacks(PythonFusionPassCallbacks &callbacks) {
    callbacks.get_matcher_config = [](const void *holder,
                                      std::unique_ptr<PatternMatcherConfig> &matcher_config) -> Status {
      return PythonFusionPassPybindBridge::GetInstance().GetPatternMatcherConfig(
          static_cast<const PythonBridgeHolder *>(holder), matcher_config);
    };
    callbacks.patterns = [](const void *holder, std::vector<PatternUniqPtr> &patterns) -> Status {
      return PythonFusionPassPybindBridge::GetInstance().GetPatterns(
          static_cast<const PythonBridgeHolder *>(holder), patterns);
    };
    callbacks.meet_requirements = [](const void *holder,
                                      const std::unique_ptr<MatchResult> &match_result) -> bool {
      return PythonFusionPassPybindBridge::GetInstance().CallMeetRequirements(
          static_cast<const PythonBridgeHolder *>(holder), match_result);
    };
    callbacks.replacement =
        [](const void *holder, const std::unique_ptr<MatchResult> &match_result,
           GraphUniqPtr &replacement_graph) -> Status {
          return PythonFusionPassPybindBridge::GetInstance().CallReplacement(
              static_cast<const PythonBridgeHolder *>(holder), match_result, replacement_graph);
        };
  }

  static void SetDecomposeCallbacks(PythonFusionPassCallbacks &callbacks) {
    callbacks.decompose_meet_requirements = [](const void *holder, const GNode &matched_node) -> bool {
      return PythonFusionPassPybindBridge::GetInstance().CallDecomposeMeetRequirements(
          static_cast<const PythonBridgeHolder *>(holder), matched_node);
    };
    callbacks.decompose_replacement =
        [](const void *holder, const GNode &matched_node, GraphUniqPtr &replacement_graph) -> Status {
          return PythonFusionPassPybindBridge::GetInstance().CallDecomposeReplacement(
              static_cast<const PythonBridgeHolder *>(holder), matched_node, replacement_graph);
        };
  }

  static PythonFusionPassCallbacks GetCallbacks(const PythonPassKind kind) {
    PythonFusionPassCallbacks callbacks;
    SetCommonCallbacks(callbacks);

    switch (kind) {
      case PythonPassKind::kFusionBase:
        SetFusionBaseCallbacks(callbacks);
        break;
      case PythonPassKind::kPatternFusion:
        SetPatternFusionCallbacks(callbacks);
        break;
      case PythonPassKind::kDecompose:
        SetDecomposeCallbacks(callbacks);
        break;
      default:
        break;
    }
    return callbacks;
  }

  Status GetPatternMatcherConfig(const PythonBridgeHolder *holder,
                                 std::unique_ptr<PatternMatcherConfig> &matcher_config) {
    matcher_config.reset();
    if (holder == nullptr) {
      return FAILED;
    }
    const auto prepare_ret = EnsureBridgeReady();
    if (prepare_ret != SUCCESS) {
      GELOGE(prepare_ret, "Prepare python bridge failed for GetPatternMatcherConfig.");
      return prepare_ret;
    }
    py::gil_scoped_acquire gil;
    try {
      py::object result = bridge_module_.attr("get_pattern_matcher_config")(holder->instance_id);
      if (result.is_none()) {
        return SUCCESS;
      }
      const auto config_handle = result.cast<uintptr_t>();
      auto *config_ptr = reinterpret_cast<PatternMatcherConfig *>(config_handle);
      if (config_ptr == nullptr) {
        GELOGE(FAILED, "Python pattern fusion pass returned empty matcher config handle, instance id[%s].",
               holder->instance_id.c_str());
        return FAILED;
      }
      matcher_config.reset(config_ptr);
      return SUCCESS;
    } catch (const py::error_already_set &err) {
      GELOGE(FAILED, "Get python pass matcher config failed, instance id[%s]: %s",
             holder->instance_id.c_str(), err.what());
      return FAILED;
    }
  }

  Status GetPatterns(const PythonBridgeHolder *holder, std::vector<PatternUniqPtr> &patterns) {
    if (holder == nullptr) {
      return FAILED;
    }
    const auto prepare_ret = EnsureBridgeReady();
    if (prepare_ret != SUCCESS) {
      GELOGE(prepare_ret, "Prepare python bridge failed for GetPatterns.");
      return prepare_ret;
    }
    py::gil_scoped_acquire gil;
    try {
      py::list pattern_list = bridge_module_.attr("get_pass_patterns")(holder->instance_id);
      GELOGI("Python pattern fusion pass instance[%s] returned [%zu] patterns.",
             holder->instance_id.c_str(),
             static_cast<size_t>(py::len(pattern_list)));
      for (const auto &item : pattern_list) {
        const auto pattern_handle = item.cast<uintptr_t>();
        auto *pattern_ptr = reinterpret_cast<Pattern *>(pattern_handle);
        if (pattern_ptr == nullptr) {
          GELOGE(FAILED, "Python pattern fusion pass returned empty pattern handle, instance id[%s].",
                 holder->instance_id.c_str());
          return FAILED;
        }
        patterns.emplace_back(pattern_ptr);
      }
      return SUCCESS;
    } catch (const py::error_already_set &err) {
      GELOGE(FAILED, "Get python pass patterns failed, instance id[%s]: %s",
             holder->instance_id.c_str(), err.what());
      return FAILED;
    }
  }

  bool CallMeetRequirements(const PythonBridgeHolder *holder,
                             const std::unique_ptr<MatchResult> &match_result) {
    if ((holder == nullptr) || (match_result == nullptr)) {
      return false;
    }
    const auto prepare_ret = EnsureBridgeReady();
    if (prepare_ret != SUCCESS) {
      GELOGW("Prepare python bridge failed for MeetRequirements.");
      return false;
    }
    py::gil_scoped_acquire gil;
    try {
      py::object result = bridge_module_.attr("call_meet_requirements")(
          holder->instance_id,
          py::int_(reinterpret_cast<uintptr_t>(match_result.get())));
      return result.cast<bool>();
    } catch (const py::error_already_set &err) {
      GELOGW("Call python meet_requirements failed, instance id[%s]: %s",
             holder->instance_id.c_str(), err.what());
      return false;
    }
  }

  Status CallReplacement(const PythonBridgeHolder *holder,
                          const std::unique_ptr<MatchResult> &match_result,
                          GraphUniqPtr &replacement_graph) {
    if (holder == nullptr) {
      return FAILED;
    }
    const auto prepare_ret = EnsureBridgeReady();
    if (prepare_ret != SUCCESS) {
      GELOGE(prepare_ret, "Prepare python bridge failed for Replacement.");
      return prepare_ret;
    }
    py::gil_scoped_acquire gil;
    try {
      py::object result = bridge_module_.attr("call_replacement")(
          holder->instance_id,
          py::int_(reinterpret_cast<uintptr_t>(match_result.get())));
      if (result.is_none()) {
        GELOGW("Python pattern fusion pass instance[%s] returned None replacement.", holder->instance_id.c_str());
        return FAILED;
      }
      const auto graph_handle = result.cast<uintptr_t>();
      auto *graph_ptr = reinterpret_cast<Graph *>(graph_handle);
      if (graph_ptr == nullptr) {
        return FAILED;
      }
      replacement_graph = GraphUniqPtr(graph_ptr);
      GELOGI("Python pattern fusion pass instance[%s] returned replacement graph handle[%p].",
             holder->instance_id.c_str(),
             graph_ptr);
      return SUCCESS;
    } catch (const py::error_already_set &err) {
      GELOGE(FAILED, "Call python replacement failed, instance id[%s]: %s",
             holder->instance_id.c_str(), err.what());
      return FAILED;
    }
  }

  bool CallDecomposeMeetRequirements(const PythonBridgeHolder *holder, const GNode &matched_node) {
    if (holder == nullptr) {
      return false;
    }
    const auto prepare_ret = EnsureBridgeReady();
    if (prepare_ret != SUCCESS) {
      GELOGW("Prepare python bridge failed for decompose MeetRequirements.");
      return false;
    }
    py::gil_scoped_acquire gil;
    try {
      py::object result = bridge_module_.attr("call_decompose_meet_requirements")(
          holder->instance_id,
          py::int_(reinterpret_cast<uintptr_t>(&matched_node)));
      return result.cast<bool>();
    } catch (const py::error_already_set &err) {
      GELOGW("Call python decompose meet_requirements failed, instance id[%s]: %s",
             holder->instance_id.c_str(), err.what());
      return false;
    }
  }

  Status CallDecomposeReplacement(const PythonBridgeHolder *holder,
                                  const GNode &matched_node,
                                  GraphUniqPtr &replacement_graph) {
    if (holder == nullptr) {
      return FAILED;
    }
    const auto prepare_ret = EnsureBridgeReady();
    if (prepare_ret != SUCCESS) {
      GELOGE(prepare_ret, "Prepare python bridge failed for decompose Replacement.");
      return prepare_ret;
    }
    py::gil_scoped_acquire gil;
    try {
      py::object result = bridge_module_.attr("call_decompose_replacement")(
          holder->instance_id,
          py::int_(reinterpret_cast<uintptr_t>(&matched_node)));
      if (result.is_none()) {
        GELOGW("Python decompose pass instance[%s] returned None replacement.", holder->instance_id.c_str());
        return FAILED;
      }
      const auto graph_handle = result.cast<uintptr_t>();
      auto *graph_ptr = reinterpret_cast<Graph *>(graph_handle);
      if (graph_ptr == nullptr) {
        return FAILED;
      }
      replacement_graph = GraphUniqPtr(graph_ptr);
      GELOGI("Python decompose pass instance[%s] returned replacement graph handle[%p].",
             holder->instance_id.c_str(),
             graph_ptr);
      return SUCCESS;
    } catch (const py::error_already_set &err) {
      GELOGE(FAILED, "Call python decompose replacement failed, instance id[%s]: %s",
             holder->instance_id.c_str(), err.what());
      return FAILED;
    }
  }

  std::mutex mutex_;
  std::atomic<uint64_t> instance_seq_{0U};
  py::object bridge_module_;
  std::string artifact_root_;
  std::string native_module_path_;
  bool owns_interpreter_{false};
};
}  // namespace

Status SetPythonPassBridgeArtifactConfig(const PythonFusionPassBridgeArtifactConfig *config) {
  return PythonFusionPassPybindBridge::GetInstance().SetArtifactConfig(config);
}

Status RegisterPythonPassesFromBridge(const PythonFusionPassRegistrar *registrar) {
  if (registrar == nullptr) {
    return FAILED;
  }
  return PythonFusionPassPybindBridge::GetInstance().RegisterPythonPasses(*registrar);
}

void ResetPythonPassBridgeState() {
  PythonFusionPassPybindBridge::GetInstance().ResetBridgeState();
}

void ShutdownPythonPassBridge() {
  PythonFusionPassPybindBridge::GetInstance().Shutdown();
}
}  // namespace fusion
}  // namespace ge

extern "C" const ge::fusion::PythonFusionPassBridgeApi *GeGetPythonFusionPassBridgeApi() {
  static const ge::fusion::PythonFusionPassBridgeApi kBridgeApi = {
      ge::fusion::kPythonFusionPassBridgeAbiVersion,
      &ge::fusion::SetPythonPassBridgeArtifactConfig,
      &ge::fusion::RegisterPythonPassesFromBridge,
      &ge::fusion::ResetPythonPassBridgeState,
      &ge::fusion::ShutdownPythonPassBridge,
  };
  return &kBridgeApi;
}
