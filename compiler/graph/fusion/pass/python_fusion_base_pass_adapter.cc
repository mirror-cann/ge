/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "python_pass_adapter.h"

#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "graph_metadef/graph/debug/ge_util.h"
#include "framework/common/debug/ge_log.h"

namespace ge {
namespace fusion {
namespace {
struct PythonFusionPassRuntimeEntry {
  explicit PythonFusionPassRuntimeEntry(PythonPassDescriptor desc, PythonFusionPassCallbacks cb)
      : pass_desc(std::move(desc)), callbacks(cb) {}

  PythonPassDescriptor pass_desc;
  PythonFusionPassCallbacks callbacks;
  // 同一个 descriptor_key 只对应一条共享的 runtime_entry，其映射关系为：
  //   descriptor_key/runtime_entry : adapter = 1 : N
  //   adapter : holder/Python pass instance = 1 : 1
  // active_adapter_count 记录当前还有多少个存活的 adapter 正在引用这条
  // runtime_entry，避免在回调仍被使用时提前执行 unregister。
  size_t active_adapter_count{0U};
  std::mutex mutex;
};

class PythonFusionPassRuntimeRegistryImpl {
 public:
  bool Register(const PythonPassDescriptor &pass_desc, const PythonFusionPassCallbacks &callbacks) {
    if (!callbacks.IsValid(pass_desc.kind)) {
      GELOGW("Register python pass runtime failed, descriptor key[%s], kind[%u].",
             pass_desc.descriptor_key.c_str(), static_cast<uint32_t>(pass_desc.kind));
      return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (descriptor_key_2_runtime_entry_.find(pass_desc.descriptor_key) != descriptor_key_2_runtime_entry_.cend()) {
      GELOGI("Python pass runtime descriptor key [%s] has already registered.", pass_desc.descriptor_key.c_str());
      return false;
    }

    descriptor_key_2_runtime_entry_.emplace(
        pass_desc.descriptor_key, ge::ComGraphMakeShared<PythonFusionPassRuntimeEntry>(pass_desc, callbacks));
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
      GELOGW("Python pass runtime descriptor key [%s] is still in use.", descriptor_key.c_str());
      return false;
    }
    descriptor_key_2_runtime_entry_.erase(iter);
    return true;
  }

  bool Acquire(const PythonPassDescriptor &pass_desc, PythonFusionPassCallbacks &callbacks) {
    auto runtime_entry = Get(pass_desc.descriptor_key);
    if (runtime_entry == nullptr) {
      GELOGW("Acquire python pass runtime failed because descriptor key [%s] is not registered.",
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
  std::shared_ptr<PythonFusionPassRuntimeEntry> Get(const std::string &descriptor_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto iter = descriptor_key_2_runtime_entry_.find(descriptor_key);
    if (iter == descriptor_key_2_runtime_entry_.cend()) {
      return nullptr;
    }
    return iter->second;
  }

  std::mutex mutex_;
  std::map<std::string, std::shared_ptr<PythonFusionPassRuntimeEntry>> descriptor_key_2_runtime_entry_;
};

PythonFusionPassRuntimeRegistryImpl &GetPythonFusionPassRuntimeRegistryImpl() {
  static PythonFusionPassRuntimeRegistryImpl runtime_registry;
  return runtime_registry;
}
}  // namespace

PythonPassHolder::PythonPassHolder(const PythonPassDescriptor &pass_desc) : pass_desc_(pass_desc) {
  if (!PythonFusionPassRuntimeRegistry::GetInstance().Acquire(pass_desc_, callbacks_)) {
    return;
  }
  if (callbacks_.create == nullptr) {
    PythonFusionPassRuntimeRegistry::GetInstance().Release(pass_desc_);
    return;
  }
  holder_ = callbacks_.create(&pass_desc_);
  if (holder_ == nullptr) {
    PythonFusionPassRuntimeRegistry::GetInstance().Release(pass_desc_);
    return;
  }
  valid_ = true;
}

PythonPassHolder::~PythonPassHolder() {
  if (valid_) {
    if ((holder_ != nullptr) && (callbacks_.destroy != nullptr)) {
      callbacks_.destroy(holder_);
      holder_ = nullptr;
    }
    PythonFusionPassRuntimeRegistry::GetInstance().Release(pass_desc_);
  }
}

bool PythonPassHolder::IsValid() const { return valid_; }

void *PythonPassHolder::GetHolder() const { return holder_; }

const PythonFusionPassCallbacks &PythonPassHolder::GetCallbacks() const { return callbacks_; }

const PythonPassDescriptor &PythonPassHolder::GetPassDescriptor() const { return pass_desc_; }

PythonFusionPassRuntimeRegistry &PythonFusionPassRuntimeRegistry::GetInstance() {
  static PythonFusionPassRuntimeRegistry runtime_registry;
  return runtime_registry;
}

bool PythonFusionPassRuntimeRegistry::Register(const PythonPassDescriptor &pass_desc,
                                                const PythonFusionPassCallbacks &callbacks) {
  return GetPythonFusionPassRuntimeRegistryImpl().Register(pass_desc, callbacks);
}

bool PythonFusionPassRuntimeRegistry::Unregister(const std::string &descriptor_key) {
  return GetPythonFusionPassRuntimeRegistryImpl().Unregister(descriptor_key);
}

bool PythonFusionPassRuntimeRegistry::Acquire(const PythonPassDescriptor &pass_desc,
                                               PythonFusionPassCallbacks &callbacks) {
  return GetPythonFusionPassRuntimeRegistryImpl().Acquire(pass_desc, callbacks);
}

void PythonFusionPassRuntimeRegistry::Release(const PythonPassDescriptor &pass_desc) {
  GetPythonFusionPassRuntimeRegistryImpl().Release(pass_desc);
}

void PythonFusionPassRuntimeRegistry::Clear() {
  GetPythonFusionPassRuntimeRegistryImpl().Clear();
}

PythonFusionBasePassAdapter::PythonFusionBasePassAdapter(const PythonPassDescriptor &pass_desc)
    : holder_(new (std::nothrow) PythonPassHolder(pass_desc)) {}

PythonFusionBasePassAdapter::~PythonFusionBasePassAdapter() = default;

Status PythonFusionBasePassAdapter::Run(GraphPtr &graph, CustomPassContext &pass_context) {
  if ((holder_ == nullptr) || (!holder_->IsValid())) {
    pass_context.SetErrorMessage("python fusion base adapter is invalid");
    return FAILED;
  }
  const auto &callbacks = holder_->GetCallbacks();
  if ((holder_->GetHolder() == nullptr) || (callbacks.run == nullptr)) {
    pass_context.SetErrorMessage("python fusion base adapter callback is invalid");
    return FAILED;
  }
  return callbacks.run(holder_->GetHolder(), graph, pass_context);
}

bool PythonFusionBasePassAdapter::IsValid() const {
  return (holder_ != nullptr) && holder_->IsValid();
}

FusionBasePass *CreatePythonPassAdapter() {
  PythonPassDescriptor pass_desc;
  if (!PassRegistry::GetInstance().ResolveCurrentPythonPassDescriptor(pass_desc)) {
    GELOGW("Create python pass adapter failed because current python pass descriptor is missing.");
    return nullptr;
  }

  switch (pass_desc.kind) {
    case PythonPassKind::kFusionBase: {
      auto *adapter = new (std::nothrow) PythonFusionBasePassAdapter(pass_desc);
      if ((adapter == nullptr) || (!adapter->IsValid())) {
        delete adapter;
        return nullptr;
      }
      return adapter;
    }
    case PythonPassKind::kPatternFusion: {
      auto *adapter = new (std::nothrow) PythonPatternFusionPassAdapter(pass_desc);
      if ((adapter == nullptr) || (!adapter->IsValid())) {
        delete adapter;
        return nullptr;
      }
      return adapter;
    }
    case PythonPassKind::kDecompose: {
      auto *adapter = new (std::nothrow) PythonDecomposePassAdapter(pass_desc);
      if ((adapter == nullptr) || (!adapter->IsValid())) {
        delete adapter;
        return nullptr;
      }
      return adapter;
    }
    default:
      GELOGW("Create python pass adapter failed because pass kind[%u] is unsupported.",
             static_cast<uint32_t>(pass_desc.kind));
      return nullptr;
  }
}

bool RegisterPythonPass(const PythonPassDescriptor &pass_desc,
                        const PythonFusionPassCallbacks &callbacks) {
  if (!PythonFusionPassRuntimeRegistry::GetInstance().Register(pass_desc, callbacks)) {
    return false;
  }
  if (PassRegistry::GetInstance().RegisterPythonPass(pass_desc, CreatePythonPassAdapter)) {
    return true;
  }
  (void)PythonFusionPassRuntimeRegistry::GetInstance().Unregister(pass_desc.descriptor_key);
  return false;
}

void ClearPythonPassRuntimeRegistry() {
  PythonFusionPassRuntimeRegistry::GetInstance().Clear();
}
}  // namespace fusion
}  // namespace ge
