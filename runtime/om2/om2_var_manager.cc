/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "om2_var_manager.h"

#include <cinttypes>

#include "acl/acl_rt.h"
#include "common/checker.h"
#include "common/ge_inner_error_codes.h"
#include "common/ge_common/debug/ge_log.h"
#include "runtime/mem.h"

namespace gert {
Om2VarManager::~Om2VarManager() {
  Finalize();
}

ge::Status Om2VarManager::TryGetExistingVarAddr(const std::string &key, const uint32_t device_id, const size_t size,
                                                void *&addr) const {
  const std::lock_guard<std::mutex> lock(mutex_);
  const auto device_iter = device_to_vars_.find(device_id);
  if (device_iter != device_to_vars_.end()) {
    const auto var_iter = device_iter->second.find(key);
    if (var_iter != device_iter->second.end()) {
      if (var_iter->second.size != size) {
        GELOGE(ge::FAILED, "[OM2][Check][Var] key=%s size mismatch, stored=%zu, requested=%zu.", key.c_str(),
               var_iter->second.size, size);
        return ge::FAILED;
      }
      addr = var_iter->second.addr;
      return ge::SUCCESS;
    }
  }
  return ge::SUCCESS;
}

ge::Status Om2VarManager::GetOrCreateVarAddr(const std::string &key, const uint32_t device_id, const size_t size,
                                             void *&addr) {
  addr = nullptr;
  if (size == 0U) {
    return ge::SUCCESS;
  }

  const auto get_ret = TryGetExistingVarAddr(key, device_id, size, addr);
  if ((get_ret != ge::SUCCESS) || (addr != nullptr)) {
    return get_ret;
  }

  void *new_addr = nullptr;
  if (size > 0U) {
    const auto malloc_ret = rtMalloc(&new_addr, size, RT_MEMORY_HBM, 0);
    if (malloc_ret != RT_ERROR_NONE) {
      GELOGE(ge::FAILED, "[OM2][Alloc][Var] rtMalloc failed, key=%s, size=%zu, rt_ret=%u", key.c_str(), size,
             malloc_ret);
      return ge::FAILED;
    }
  }

  {
    const std::lock_guard<std::mutex> lock(mutex_);
    auto &device_vars = device_to_vars_[device_id];
    const auto var_iter = device_vars.find(key);
    if (var_iter != device_vars.end()) {
      if (var_iter->second.size != size) {
        GELOGE(ge::FAILED, "[OM2][Check][Var] key=%s size mismatch, stored=%zu, requested=%zu.", key.c_str(),
               var_iter->second.size, size);
        if (new_addr != nullptr) {
          (void)rtFree(new_addr);
        }
        return ge::FAILED;
      }
      addr = var_iter->second.addr;
      if (new_addr != nullptr) {
        (void)rtFree(new_addr);
      }
      return ge::SUCCESS;
    }
    device_vars[key] = VarAddrInfo{new_addr, size};
    addr = new_addr;
  }
  return ge::SUCCESS;
}

bool Om2VarManager::TryGetVarAddr(const std::string &key, const uint32_t device_id, void *&addr) const {
  const std::lock_guard<std::mutex> lock(mutex_);
  addr = nullptr;
  const auto device_iter = device_to_vars_.find(device_id);
  if (device_iter == device_to_vars_.end()) {
    return false;
  }
  const auto record_iter = device_iter->second.find(key);
  if (record_iter == device_iter->second.end()) {
    return false;
  }
  addr = record_iter->second.addr;
  return addr != nullptr;
}

void Om2VarManager::Finalize() noexcept {
  std::map<uint32_t, std::map<std::string, VarAddrInfo>> vars_to_free;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    vars_to_free.swap(device_to_vars_);
  }
  for (auto &device_and_vars : vars_to_free) {
    for (auto &key_and_info : device_and_vars.second) {
      if (key_and_info.second.addr != nullptr) {
        (void)rtFree(key_and_info.second.addr);
        key_and_info.second.addr = nullptr;
      }
    }
  }
}

Om2VarManagerPool &Om2VarManagerPool::Instance() {
  static Om2VarManagerPool om2_var_manager_pool;
  return om2_var_manager_pool;
}

Om2VarManagerPool::~Om2VarManagerPool() {
  Destroy();
}

Om2VarManagerPtr Om2VarManagerPool::GetManager(const uint64_t session_id) {
  const std::lock_guard<std::mutex> lock(mutex_);
  auto &var_manager = session_id_to_manager_[session_id];
  if (var_manager == nullptr) {
    var_manager = std::make_shared<Om2VarManager>();
    if (var_manager == nullptr) {
      GELOGE(ge::INTERNAL_ERROR, "[OM2][New][VarManager] failed, session_id:%" PRIu64 ".", session_id);
      return nullptr;
    }
  }
  return var_manager;
}

void Om2VarManagerPool::RemoveManager(const uint64_t session_id) {
  Om2VarManagerPtr var_manager = nullptr;
  {
    const std::lock_guard<std::mutex> lock(mutex_);
    const auto iter = session_id_to_manager_.find(session_id);
    if (iter != session_id_to_manager_.end()) {
      var_manager = iter->second;
      (void)session_id_to_manager_.erase(iter);
    }
  }
  if (var_manager != nullptr) {
    var_manager->Finalize();
  }
}

void Om2VarManagerPool::Destroy() noexcept {
  const std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &session_and_manager : session_id_to_manager_) {
    if (session_and_manager.second != nullptr) {
      session_and_manager.second->Finalize();
    }
  }
  session_id_to_manager_.clear();
}
}  // namespace gert
