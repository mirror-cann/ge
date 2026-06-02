/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#ifndef BASE_COMMON_HELPER_CUSTOM_OP_SO_LOADER_H_
#define BASE_COMMON_HELPER_CUSTOM_OP_SO_LOADER_H_

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

#include "external/ge_common/ge_common_api_types.h"
#include "graph/op_so_bin.h"

namespace ge {
class CustomOpSoLoader {
 public:
  static CustomOpSoLoader &GetInstance();
  Status LoadCustomOpSoBins(const std::vector<OpSoBinPtr> &custom_so_bins);

 private:
  CustomOpSoLoader() = default;
  ~CustomOpSoLoader();
  CustomOpSoLoader(const CustomOpSoLoader &) = delete;
  CustomOpSoLoader &operator=(const CustomOpSoLoader &) = delete;

  struct SoBinFingerprint {
    uint32_t bin_size;
    uint64_t content_hash;

    bool operator==(const SoBinFingerprint &other) const {
      return (bin_size == other.bin_size) && (content_hash == other.content_hash);
    }
  };

  struct LoadedSoState {
    SoBinFingerprint fingerprint;
    void *handle;
    int32_t mem_fd;
  };

  Status GetSoKey(const OpSoBinPtr &op_so_bin, std::string &so_key) const;
  Status CalculateSoBinFingerprint(const OpSoBinPtr &op_so_bin, SoBinFingerprint &fingerprint) const;
  Status CreateSoMemFd(const std::string &so_key, int32_t &mem_fd) const;
  Status WriteSoBinToFd(const OpSoBinPtr &op_so_bin, const int32_t mem_fd) const;
  Status DlopenSoByFd(const int32_t mem_fd, void *&handle) const;
  void Cleanup();

  mutable std::mutex mutex_;
  std::unordered_map<std::string, LoadedSoState> loaded_states_;
};
}  // namespace ge

#endif  // BASE_COMMON_HELPER_CUSTOM_OP_SO_LOADER_H_
