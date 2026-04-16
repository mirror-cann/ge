/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "python_fusion_base_pass_pybind_bridge.h"
#include "python_fusion_base_pass_bridge_c_api.h"

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
#include "python_fusion_base_pass_adapter.h"

#undef PYBIND11_CHECK_PYTHON_VERSION
#define PYBIND11_CHECK_PYTHON_VERSION

namespace ge {
namespace fusion {
namespace py = pybind11;
namespace {
constexpr const char *kBridgeModuleName = "ge.passes._bridge";
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

class PythonFusionBasePassPybindBridge {
 public:
  static PythonFusionBasePassPybindBridge &GetInstance() {
    static PythonFusionBasePassPybindBridge instance;
    return instance;
  }

  Status RegisterFusionBasePasses(const PythonFusionBasePassRegistrar &registrar) {
    GELOGI("Begin to register python fusion base passes through pybind bridge.");
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
      if (pass_desc.kind != PythonPassKind::kFusionBase) {
        continue;
      }
      const auto callbacks = GetCallbacks();
      if ((registrar.register_pass == nullptr) || (!registrar.register_pass(&pass_desc, &callbacks))) {
        GELOGE(FAILED, "Register python fusion base pass[%s] failed.", pass_desc.pass_name.c_str());
        return FAILED;
      }
      GELOGI("Python fusion base pass[%s] is registered from pybind bridge.", pass_desc.pass_name.c_str());
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
        GELOGW("Finalize python fusion base pybind bridge interpreter failed: %s", err.what());
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
      GELOGE(FAILED, "Run python fusion base pass failed, descriptor key[%s], instance id[%s]: %s",
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
    bridge_module_ = py::module_::import(kBridgeModuleName);
  }

  void ResetBridgeStateUnlocked() {
    try {
      EnsureBridgeModuleUnlocked();
      (void)bridge_module_.attr("clear_pass_holders")();
      (void)bridge_module_.attr("clear_loaded_pass_modules")();
      (void)py::module_::import(kPassModuleName).attr("clear_registered_passes")();
    } catch (const py::error_already_set &err) {
      GELOGW("Reset python fusion base pybind bridge state failed: %s", err.what());
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

  py::dict BuildPythonPassContext(CustomPassContext &pass_context) const {
    py::dict py_context;
    py_context["pass_name"] = pass_context.GetPassName().GetString();
    py_context["options"] = py::dict();
    return py_context;
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
    oss << "python fusion base pass returned unsupported type: " << std::string(py::str(py::type::of(result)));
    pass_context.SetErrorMessage(AscendString(oss.str().c_str()));
    GELOGE(FAILED, "%s", oss.str().c_str());
    return FAILED;
  }

  PythonFusionBasePassCallbacks GetCallbacks() {
    PythonFusionBasePassCallbacks callbacks;
    callbacks.create = [](const PythonPassDescriptor *pass_desc) -> void* {
      if (pass_desc == nullptr) {
        return nullptr;
      }
      return PythonFusionBasePassPybindBridge::GetInstance().CreateHolder(*pass_desc);
    };
    callbacks.destroy = [](void *holder) {
      PythonFusionBasePassPybindBridge::GetInstance().DestroyHolder(static_cast<PythonBridgeHolder *>(holder));
    };
    callbacks.run = [](void *holder, GraphPtr &graph, CustomPassContext &pass_context) -> Status {
      return PythonFusionBasePassPybindBridge::GetInstance().Run(static_cast<PythonBridgeHolder *>(holder),
                                                                 graph,
                                                                 pass_context);
    };
    return callbacks;
  }

  std::mutex mutex_;
  std::atomic<uint64_t> instance_seq_{0U};
  py::object bridge_module_;
  bool owns_interpreter_{false};
};
}  // namespace

Status RegisterFusionBasePassesFromBridge(const PythonFusionBasePassRegistrar *registrar) {
  if (registrar == nullptr) {
    return FAILED;
  }
  return PythonFusionBasePassPybindBridge::GetInstance().RegisterFusionBasePasses(*registrar);
}

void ResetPythonFusionBasePassBridgeState() {
  PythonFusionBasePassPybindBridge::GetInstance().ResetBridgeState();
}

void ShutdownPythonFusionBasePassBridge() {
  PythonFusionBasePassPybindBridge::GetInstance().Shutdown();
}
}  // namespace fusion
}  // namespace ge

extern "C" const ge::fusion::PythonFusionBasePassBridgeApi *GeGetPythonFusionBasePassBridgeApi() {
  static const ge::fusion::PythonFusionBasePassBridgeApi kBridgeApi = {
      ge::fusion::kPythonFusionBasePassBridgeAbiVersion,
      &ge::fusion::RegisterFusionBasePassesFromBridge,
      &ge::fusion::ResetPythonFusionBasePassBridgeState,
      &ge::fusion::ShutdownPythonFusionBasePassBridge,
  };
  return &kBridgeApi;
}
