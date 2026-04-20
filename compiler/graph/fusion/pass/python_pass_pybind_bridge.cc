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
#include <string>
#include <utility>
#include <vector>

#include "pybind11/embed.h"
#include "pybind11/stl.h"
#include "framework/common/debug/ge_log.h"
#include "graph/ascend_string.h"
#include "graph/node.h"
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
      return;
    }
    py::gil_scoped_acquire gil;
    ResetBridgeStateUnlocked();
  }

  void Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (Py_IsInitialized() != 0) {
      py::gil_scoped_acquire gil;
      ResetBridgeStateUnlocked();
      // 先释放 bridge 持有的 Python 对象引用，再决定是否回收解释器。
      bridge_module_ = py::object();
      // 打破 Python 对象循环引用，让 finalize 前尽可能多的对象可回收。
      try {
        (void)py::module_::import("gc").attr("collect")();
      } catch (const py::error_already_set &err) {
        GELOGW("Python gc.collect() during shutdown failed: %s", err.what());
      }
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

  Status Run(PythonBridgeHolder *holder, GraphPtr &graph, CustomPassContext &pass_context) {
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
    }
    return SUCCESS;
  }

  void EnsureBridgeModuleUnlocked() {
    if (bridge_module_ && (!bridge_module_.is_none())) {
      return;
    }
    py::module_ pass_native_module = py::module_::import(kPassNativeModuleName);
    bridge_module_ = py::module_::import(kBridgeModuleName);
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
  }

  void ClearPythonEnvVarUnlocked() {
    py::module_ os = py::module_::import("os");
    py::dict environ = os.attr("environ");
    (void)environ.attr("pop")(py::str(kEnvPythonPassPath), py::none());
  }

  void SyncProcessEnvToPythonUnlocked() {
    const char *env_value = std::getenv(kEnvPythonPassPath);
    if (env_value == nullptr) {
      ClearPythonEnvVarUnlocked();
      return;
    }
    py::module_ os = py::module_::import("os");
    py::dict environ = os.attr("environ");
    environ[py::str(kEnvPythonPassPath)] = py::str(env_value);
    os.attr("putenv")(py::str(kEnvPythonPassPath), py::str(env_value));
  }

  std::string BuildInstanceId(const std::string &descriptor_key) {
    const uint64_t sequence = ++instance_seq_;
    std::ostringstream oss;
    oss << descriptor_key << "#" << sequence;
    return oss.str();
  }

  Status ParseDescriptor(const py::dict &descriptor_dict, PythonPassDescriptor &pass_desc) const {
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

  py::object BuildPythonGraph(GraphPtr &graph) const {
    if (graph == nullptr) {
      return py::none();
    }
    py::module_ ctypes_module = py::module_::import("ctypes");
    py::module_ graph_module = py::module_::import("ge.graph");
    py::object graph_type = graph_module.attr("Graph");
    py::object graph_handle = ctypes_module.attr("c_void_p")(py::int_(reinterpret_cast<uintptr_t>(graph.get())));
    py::capsule graph_owner(new GraphPtr(graph), [](void *ptr) {
      delete static_cast<GraphPtr *>(ptr);
    });
    return graph_type.attr("_create_from")(graph_handle, py::bool_(false), graph_owner);
  }

  py::object BuildPythonPassContext(CustomPassContext &pass_context) const {
    return py::cast(&pass_context, py::return_value_policy::reference);
  }

  Status TranslateRunResult(const py::object &result, CustomPassContext &pass_context) const {
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

  PythonFusionPassCallbacks GetCallbacks(PythonPassKind kind) {
    PythonFusionPassCallbacks callbacks;
    callbacks.create = [](const PythonPassDescriptor *pass_desc) -> void* {
      if (pass_desc == nullptr) {
        return nullptr;
      }
      return PythonFusionPassPybindBridge::GetInstance().CreateHolder(*pass_desc);
    };
    callbacks.destroy = [](void *holder) {
      PythonFusionPassPybindBridge::GetInstance().DestroyHolder(static_cast<PythonBridgeHolder *>(holder));
    };

    switch (kind) {
      case PythonPassKind::kFusionBase:
        callbacks.run = [](void *holder, GraphPtr &graph, CustomPassContext &pass_context) -> Status {
          return PythonFusionPassPybindBridge::GetInstance().Run(static_cast<PythonBridgeHolder *>(holder),
                                                                     graph,
                                                                     pass_context);
        };
        break;
      case PythonPassKind::kPatternFusion:
        callbacks.patterns = [](void *holder, std::vector<PatternUniqPtr> &patterns) -> Status {
          return PythonFusionPassPybindBridge::GetInstance().GetPatterns(
              static_cast<PythonBridgeHolder *>(holder), patterns);
        };
        callbacks.meet_requirements = [](void *holder,
                                          const std::unique_ptr<MatchResult> &match_result) -> bool {
          return PythonFusionPassPybindBridge::GetInstance().CallMeetRequirements(
              static_cast<PythonBridgeHolder *>(holder), match_result);
        };
        callbacks.replacement =
            [](void *holder, const std::unique_ptr<MatchResult> &match_result,
               GraphUniqPtr &replacement_graph) -> Status {
              return PythonFusionPassPybindBridge::GetInstance().CallReplacement(
                  static_cast<PythonBridgeHolder *>(holder), match_result, replacement_graph);
            };
        break;
      default:
        break;
    }
    return callbacks;
  }

  // TTODO: GetPatterns / CallMeetRequirements / CallReplacement 三个方法为
  // PatternFusionPass 桥接的占位实现，当前代码可编译但尚不可运行，原因：
  // 1. _bridge.py 尚未提供 get_pass_patterns / call_meet_requirements /
  //    call_replacement 三个函数，桥接协议待协同设计后实现
  // 2. Pattern 的 native binding 未就绪，cast<Pattern*> 获取裸指针后交给
  //    unique_ptr 的所有权语义尚未确定
  // 3. MatchResult / Graph 跨语言传递使用 uintptr_t，需 native binding
  //    提供类型安全的 wrapper 后才能确立生命周期约定
  // 待 Pattern / MatchResult 的 native binding 就绪后完成最终实现。
  Status GetPatterns(PythonBridgeHolder *holder, std::vector<PatternUniqPtr> &patterns) {
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
      for (const auto &item : pattern_list) {
        auto *pattern_ptr = item.cast<Pattern *>();
        if (pattern_ptr != nullptr) {
          patterns.emplace_back(pattern_ptr);
        }
      }
      return SUCCESS;
    } catch (const py::error_already_set &err) {
      GELOGE(FAILED, "Get python pass patterns failed, instance id[%s]: %s",
             holder->instance_id.c_str(), err.what());
      return FAILED;
    }
  }

  bool CallMeetRequirements(PythonBridgeHolder *holder,
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

  Status CallReplacement(PythonBridgeHolder *holder,
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
        return FAILED;
      }
      auto *graph_ptr = result.cast<Graph *>();
      if (graph_ptr == nullptr) {
        return FAILED;
      }
      replacement_graph = GraphUniqPtr(graph_ptr);
      return SUCCESS;
    } catch (const py::error_already_set &err) {
      GELOGE(FAILED, "Call python replacement failed, instance id[%s]: %s",
             holder->instance_id.c_str(), err.what());
      return FAILED;
    }
  }

  std::mutex mutex_;
  std::atomic<uint64_t> instance_seq_{0U};
  py::object bridge_module_;
  bool owns_interpreter_{false};
};
}  // namespace

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
      &ge::fusion::RegisterPythonPassesFromBridge,
      &ge::fusion::ResetPythonPassBridgeState,
      &ge::fusion::ShutdownPythonPassBridge,
  };
  return &kBridgeApi;
}
