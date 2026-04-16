/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "om2_external_weight_manager.h"

#include <cinttypes>

#include "common/ge_inner_error_codes.h"
#include "common/ge_common/debug/ge_log.h"

namespace gert {
bool Om2ExternalWeightManager::CheckAndSetWeightLoading(const std::string &key, const uint32_t device_id) {
  const std::lock_guard<std::mutex> lock(mutex_);
  auto &loading_keys = device_to_loading_keys_[device_id];
  if (loading_keys.count(key) > 0U) {
    return true;
  }
  (void)loading_keys.insert(key);
  return false;
}

void Om2ExternalWeightManager::Finalize() noexcept {
  const std::lock_guard<std::mutex> lock(mutex_);
  device_to_loading_keys_.clear();
}

Om2ExternalWeightManagerPool &Om2ExternalWeightManagerPool::Instance() {
  static Om2ExternalWeightManagerPool manager_pool;
  return manager_pool;
}

Om2ExternalWeightManagerPool::~Om2ExternalWeightManagerPool() {
  Destroy();
}

Om2ExternalWeightManagerPtr Om2ExternalWeightManagerPool::GetManager(const uint64_t session_id) {
  const std::lock_guard<std::mutex> lock(mutex_);
  auto &manager = session_id_to_manager_[session_id];
  if (manager == nullptr) {
    manager = std::make_shared<Om2ExternalWeightManager>();
    if (manager == nullptr) {
      GELOGE(ge::INTERNAL_ERROR, "[OM2][New][ExternalWeightManager] failed, session_id:%" PRIu64 ".", session_id);
      return nullptr;
    }
  }
  return manager;
}

void Om2ExternalWeightManagerPool::RemoveManager(const uint64_t session_id) {
  Om2ExternalWeightManagerPtr manager = nullptr;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto iter = session_id_to_manager_.find(session_id);
    if (iter != session_id_to_manager_.end()) {
      manager = iter->second;
      (void)session_id_to_manager_.erase(iter);
    }
  }
  if (manager != nullptr) {
    manager->Finalize();
  }
}

void Om2ExternalWeightManagerPool::Destroy() noexcept {
  const std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &item : session_id_to_manager_) {
    if (item.second != nullptr) {
      item.second->Finalize();
    }
  }
  session_id_to_manager_.clear();
}
}  // namespace gert
