/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "python_fusion_base_pass_adapter.h"

#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "framework/common/debug/ge_log.h"

namespace ge {
namespace fusion {
namespace {
struct PythonFusionBasePassRuntimeEntry {
  explicit PythonFusionBasePassRuntimeEntry(PythonPassDescriptor desc, PythonFusionBasePassCallbacks cb)
      : pass_desc(std::move(desc)), callbacks(cb) {}

  PythonPassDescriptor pass_desc;
  PythonFusionBasePassCallbacks callbacks;
  // 同一个 descriptor_key 只对应一条共享的 runtime_entry，其映射关系为：
  //   descriptor_key/runtime_entry : adapter = 1 : N
  //   adapter : holder/Python pass instance = 1 : 1
  // active_adapter_count 记录当前还有多少个存活的 adapter 正在引用这条
  // runtime_entry，避免在回调仍被使用时提前执行 unregister。
  size_t active_adapter_count{0U};
  std::mutex mutex;
};

class PythonFusionBasePassRuntimeRegistryImpl {
 public:
  bool Register(const PythonPassDescriptor &pass_desc, const PythonFusionBasePassCallbacks &callbacks) {
    if ((pass_desc.kind != PythonPassKind::kFusionBase) || (!callbacks.IsValid())) {
      GELOGW("Register python fusion base runtime failed, descriptor key[%s].", pass_desc.descriptor_key.c_str());
      return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (descriptor_key_2_runtime_entry_.find(pass_desc.descriptor_key) != descriptor_key_2_runtime_entry_.cend()) {
      GELOGI("Python fusion base runtime descriptor key [%s] has already registered.", pass_desc.descriptor_key.c_str());
      return false;
    }

    descriptor_key_2_runtime_entry_.emplace(
        pass_desc.descriptor_key, std::make_shared<PythonFusionBasePassRuntimeEntry>(pass_desc, callbacks));
    return true;
  }

  bool Unregister(const std::string &descriptor_key) {
    const std::lock_guard<std::mutex> map_lock(mutex_);
    const auto iter = descriptor_key_2_runtime_entry_.find(descriptor_key);
    if (iter == descriptor_key_2_runtime_entry_.cend()) {
      return false;
    }
    const auto &runtime_entry = iter->second;
    std::lock_guard<std::mutex> runtime_lock(runtime_entry->mutex);
    if (runtime_entry->active_adapter_count != 0U) {
      GELOGW("Python fusion base runtime descriptor key [%s] is still in use.", descriptor_key.c_str());
      return false;
    }
    descriptor_key_2_runtime_entry_.erase(iter);
    return true;
  }

  bool Acquire(const PythonPassDescriptor &pass_desc, PythonFusionBasePassCallbacks &callbacks) {
    auto runtime_entry = Get(pass_desc.descriptor_key);
    if (runtime_entry == nullptr) {
      GELOGW("Acquire python fusion base runtime failed because descriptor key [%s] is not registered.",
             pass_desc.descriptor_key.c_str());
      return false;
    }

    std::lock_guard<std::mutex> lock(runtime_entry->mutex);
    ++runtime_entry->active_adapter_count;
    callbacks = runtime_entry->callbacks;
    return true;
  }

  void Release(const PythonPassDescriptor &pass_desc) {
    auto runtime_entry = Get(pass_desc.descriptor_key);
    if (runtime_entry == nullptr) {
      return;
    }

    std::lock_guard<std::mutex> lock(runtime_entry->mutex);
    if (runtime_entry->active_adapter_count == 0U) {
      return;
    }
    --runtime_entry->active_adapter_count;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    descriptor_key_2_runtime_entry_.clear();
  }

 private:
  std::shared_ptr<PythonFusionBasePassRuntimeEntry> Get(const std::string &descriptor_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iter = descriptor_key_2_runtime_entry_.find(descriptor_key);
    if (iter == descriptor_key_2_runtime_entry_.cend()) {
      return nullptr;
    }
    return iter->second;
  }

  std::mutex mutex_;
  std::map<std::string, std::shared_ptr<PythonFusionBasePassRuntimeEntry>> descriptor_key_2_runtime_entry_;
};

PythonFusionBasePassRuntimeRegistryImpl &GetPythonFusionBasePassRuntimeRegistryImpl() {
  static PythonFusionBasePassRuntimeRegistryImpl runtime_registry;
  return runtime_registry;
}
}  // namespace

struct PythonFusionBasePassAdapter::Impl {
  explicit Impl(PythonPassDescriptor desc) : pass_desc(std::move(desc)) {}

  PythonPassDescriptor pass_desc;
  PythonFusionBasePassCallbacks callbacks;
  void *holder{nullptr};
  bool valid{false};
};

PythonFusionBasePassRuntimeRegistry &PythonFusionBasePassRuntimeRegistry::GetInstance() {
  static PythonFusionBasePassRuntimeRegistry runtime_registry;
  return runtime_registry;
}

bool PythonFusionBasePassRuntimeRegistry::Register(const PythonPassDescriptor &pass_desc,
                                                   const PythonFusionBasePassCallbacks &callbacks) {
  return GetPythonFusionBasePassRuntimeRegistryImpl().Register(pass_desc, callbacks);
}

bool PythonFusionBasePassRuntimeRegistry::Unregister(const std::string &descriptor_key) {
  return GetPythonFusionBasePassRuntimeRegistryImpl().Unregister(descriptor_key);
}

bool PythonFusionBasePassRuntimeRegistry::Acquire(const PythonPassDescriptor &pass_desc,
                                                  PythonFusionBasePassCallbacks &callbacks) {
  return GetPythonFusionBasePassRuntimeRegistryImpl().Acquire(pass_desc, callbacks);
}

void PythonFusionBasePassRuntimeRegistry::Release(const PythonPassDescriptor &pass_desc) {
  GetPythonFusionBasePassRuntimeRegistryImpl().Release(pass_desc);
}

void PythonFusionBasePassRuntimeRegistry::Clear() {
  GetPythonFusionBasePassRuntimeRegistryImpl().Clear();
}

PythonFusionBasePassAdapter::PythonFusionBasePassAdapter(const PythonPassDescriptor &pass_desc)
    : impl_(new (std::nothrow) Impl(pass_desc)) {
  if (impl_ == nullptr) {
    return;
  }
  if (!PythonFusionBasePassRuntimeRegistry::GetInstance().Acquire(impl_->pass_desc, impl_->callbacks)) {
    return;
  }
  if (impl_->callbacks.create == nullptr) {
    PythonFusionBasePassRuntimeRegistry::GetInstance().Release(impl_->pass_desc);
    return;
  }
  impl_->holder = impl_->callbacks.create(&impl_->pass_desc);
  if (impl_->holder == nullptr) {
    PythonFusionBasePassRuntimeRegistry::GetInstance().Release(impl_->pass_desc);
    return;
  }
  impl_->valid = true;
}

PythonFusionBasePassAdapter::~PythonFusionBasePassAdapter() {
  if ((impl_ != nullptr) && impl_->valid) {
    if ((impl_->holder != nullptr) && (impl_->callbacks.destroy != nullptr)) {
      impl_->callbacks.destroy(impl_->holder);
      impl_->holder = nullptr;
    }
    PythonFusionBasePassRuntimeRegistry::GetInstance().Release(impl_->pass_desc);
  }
}

Status PythonFusionBasePassAdapter::Run(GraphPtr &graph, CustomPassContext &pass_context) {
  if ((impl_ == nullptr) || (!impl_->valid)) {
    pass_context.SetErrorMessage("python fusion base adapter is invalid");
    return FAILED;
  }
  if ((impl_->holder == nullptr) || (impl_->callbacks.run == nullptr)) {
    pass_context.SetErrorMessage("python fusion base adapter callback is invalid");
    return FAILED;
  }
  return impl_->callbacks.run(impl_->holder, graph, pass_context);
}

bool PythonFusionBasePassAdapter::IsValid() const {
  return (impl_ != nullptr) && impl_->valid;
}

FusionBasePass *CreatePythonFusionBasePassAdapter() {
  PythonPassDescriptor pass_desc;
  if (!PassRegistry::GetInstance().ResolveCurrentPythonPassDescriptor(pass_desc)) {
    GELOGW("Create python fusion base adapter failed because current python pass descriptor is missing.");
    return nullptr;
  }
  if (pass_desc.kind != PythonPassKind::kFusionBase) {
    GELOGW("Create python fusion base adapter failed because pass kind[%u] is unsupported.",
           static_cast<uint32_t>(pass_desc.kind));
    return nullptr;
  }

  auto *adapter = new (std::nothrow) PythonFusionBasePassAdapter(pass_desc);
  if ((adapter == nullptr) || (!adapter->IsValid())) {
    delete adapter;
    return nullptr;
  }
  return adapter;
}

bool RegisterPythonFusionBasePass(const PythonPassDescriptor &pass_desc,
                                  const PythonFusionBasePassCallbacks &callbacks) {
  if (!PythonFusionBasePassRuntimeRegistry::GetInstance().Register(pass_desc, callbacks)) {
    return false;
  }
  if (PassRegistry::GetInstance().RegisterPythonPass(pass_desc, CreatePythonFusionBasePassAdapter)) {
    return true;
  }
  (void)PythonFusionBasePassRuntimeRegistry::GetInstance().Unregister(pass_desc.descriptor_key);
  return false;
}

void ClearPythonFusionBasePassRuntimeRegistry() {
  PythonFusionBasePassRuntimeRegistry::GetInstance().Clear();
}
}  // namespace fusion
}  // namespace ge
