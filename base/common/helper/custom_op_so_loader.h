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

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "external/ge_common/ge_common_api_types.h"
#include "ge/ge_api_error_codes.h"
#include "graph/op_so_bin.h"

namespace ge {
class CustomOpSoHandle {
 public:
  CustomOpSoHandle(std::string fingerprint_key, void *handle, std::string so_name, const size_t bin_size,
                   const int32_t mem_fd);
  ~CustomOpSoHandle();
  CustomOpSoHandle(const CustomOpSoHandle &) = delete;
  CustomOpSoHandle &operator=(const CustomOpSoHandle &) = delete;
  CustomOpSoHandle(CustomOpSoHandle &&) = delete;
  CustomOpSoHandle &operator=(CustomOpSoHandle &&) = delete;

  void *GetHandle() const;
  const std::string &GetFingerprintKey() const;
  const std::string &GetSoName() const;

 private:
  friend class CustomOpSoLoader;
  void AdoptResource(void *handle, const int32_t mem_fd) noexcept;

  std::string fingerprint_key_;
  void *handle_;
  std::string so_name_;
  size_t bin_size_;
  int32_t mem_fd_;
};

using CustomOpSoHandlePtr = std::shared_ptr<CustomOpSoHandle>;

class CustomOpSoLoader {
 public:
  static CustomOpSoLoader &GetInstance();
  static void Finalize();
  Status LoadCustomOpSoBins(const std::vector<OpSoBinPtr> &custom_so_bins,
                            std::vector<CustomOpSoHandlePtr> &loaded_handles);

 private:
  CustomOpSoLoader();
  ~CustomOpSoLoader();
  CustomOpSoLoader(const CustomOpSoLoader &) = delete;
  CustomOpSoLoader &operator=(const CustomOpSoLoader &) = delete;

  Status GetSoKey(const OpSoBinPtr &op_so_bin, std::string &so_key) const;
  Status CalculateSoBinFingerprint(const OpSoBinPtr &op_so_bin, std::string &fingerprint_key) const;
  Status CreateSoMemFd(const std::string &so_key, int32_t &mem_fd) const;
  Status WriteSoBinToFd(const OpSoBinPtr &op_so_bin, const int32_t mem_fd) const;
  Status DlopenSoByFd(const int32_t mem_fd, void *&handle) const;
  Status LoadSingleCustomOpSoBin(const OpSoBinPtr &so_bin, std::vector<CustomOpSoHandlePtr> &loaded_handles);
  CustomOpSoHandlePtr GetLoadedHandle(const std::string &fingerprint_key);
  Status LoadCustomOpSoBinCandidate(const OpSoBinPtr &so_bin, const std::string &diagnostic_so_key,
                                    const std::string &fingerprint_key, CustomOpSoHandlePtr &candidate_handle) const;
  void PublishOrReuseLoadedHandle(const std::string &fingerprint_key, const CustomOpSoHandlePtr &candidate_handle,
                                  CustomOpSoHandlePtr &loaded_handle);
  void Cleanup();

  mutable std::mutex mutex_;
  std::map<std::string, std::weak_ptr<CustomOpSoHandle>> loaded_states_;
};
}  // namespace ge

#endif  // BASE_COMMON_HELPER_CUSTOM_OP_SO_LOADER_H_
