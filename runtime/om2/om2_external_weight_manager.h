/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef AIR_RUNTIME_OM2_EXTERNAL_WEIGHT_MANAGER_H_
#define AIR_RUNTIME_OM2_EXTERNAL_WEIGHT_MANAGER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace gert {
class Om2ExternalWeightManager {
 public:
  Om2ExternalWeightManager() = default;
  ~Om2ExternalWeightManager() = default;

  bool CheckAndSetWeightLoading(const std::string &key, uint32_t device_id);
  void Finalize() noexcept;

 private:
  std::mutex mutex_;
  std::map<uint32_t, std::set<std::string>> device_to_loading_keys_;
};

using Om2ExternalWeightManagerPtr = std::shared_ptr<Om2ExternalWeightManager>;

class Om2ExternalWeightManagerPool {
 public:
  static Om2ExternalWeightManagerPool &Instance();

  ~Om2ExternalWeightManagerPool();

  Om2ExternalWeightManagerPtr GetManager(uint64_t session_id);
  void RemoveManager(uint64_t session_id);
  void Destroy() noexcept;

 private:
  Om2ExternalWeightManagerPool() = default;

  std::mutex mutex_;
  std::map<uint64_t, Om2ExternalWeightManagerPtr> session_id_to_manager_;
};
}  // namespace gert

#endif  // AIR_RUNTIME_OM2_EXTERNAL_WEIGHT_MANAGER_H_
