/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_RUNTIME_OM2_VAR_MANAGER_H_
#define AIR_RUNTIME_OM2_VAR_MANAGER_H_

#include <cstdint>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "common/ge_common/ge_types.h"

namespace gert {
class Om2VarManager {
 public:
  Om2VarManager() = default;
  ~Om2VarManager();

  ge::Status GetOrCreateVarAddr(const std::string &key, uint32_t device_id, size_t size, void *&addr);
  bool TryGetVarAddr(const std::string &key, uint32_t device_id, void *&addr) const;
  void Finalize() noexcept;

 private:
  struct VarAddrInfo {
    void *addr = nullptr;
    size_t size = 0U;
  };

  ge::Status TryGetExistingVarAddr(const std::string &key, uint32_t device_id, size_t size, void *&addr) const;

  mutable std::mutex mutex_;
  std::map<uint32_t, std::map<std::string, VarAddrInfo>> device_to_vars_;
};

using Om2VarManagerPtr = std::shared_ptr<Om2VarManager>;

class Om2VarManagerPool {
 public:
  static Om2VarManagerPool &Instance();

  ~Om2VarManagerPool();

  Om2VarManagerPtr GetManager(uint64_t session_id);
  void RemoveManager(uint64_t session_id);
  void Destroy() noexcept;

 private:
  Om2VarManagerPool() = default;

  std::mutex mutex_;
  std::map<uint64_t, Om2VarManagerPtr> session_id_to_manager_;
};
}  // namespace gert

#endif  // AIR_RUNTIME_OM2_VAR_MANAGER_H_
