/**
 * Copyright (c) 2025 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/helper/custom_op_so_loader.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <utility>
#include <vector>

#include "common/checker.h"
#include "framework/common/debug/log.h"
#include "graph/utils/file_utils.h"
#include "mmpa/mmpa_api.h"

/*
 * 文件职责：
 *   本文件实现 CustomOpSoLoader，用于将 OM 中携带的自定义算子 so 二进制以“仅内存”方式加载到当前进程。
 *
 * 核心流程：
 *   1) 生成稳定标识：使用 vendor_name + so_name 生成 so_key，并计算内容指纹（bin_size + FNV1a64）。
 *   2) 内存加载：通过 memfd_create 创建匿名 fd，写入 so 数据，再以 /proc/self/fd/<fd> 调用 mmDlopen。
 *   3) 状态去重：以 so_key 缓存 {fingerprint, handle, mem_fd}：
 *      - 同 key 且同内容：跳过重复加载；
 *      - 同 key 但内容不同：直接报错失败，避免运行时行为不确定。
 *   4) 生命周期管理：Cleanup 中统一执行 mmDlclose + mmClose，确保句柄和 fd 被可靠释放。
 *
 * 设计约束：
 *   严格禁止任何“回退落盘”路径；当 memfd_create 或 /proc/self/fd 路径不可用时，直接返回失败。
 */
namespace ge {
namespace {
constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;
constexpr int32_t kInvalidFd = -1;
constexpr const char_t *kProcFdPrefix = "/proc/self/fd/";
constexpr const char_t *kNoDiskFallbackHint = "strict no-disk-fallback is enabled.";
constexpr const char *kReleaseOpsRegInfoSymbol = "ReleaseOpsRegInfo";

using ReleaseOpsRegInfoFunc = void (*)();

uint64_t CalculateFnv1a64(const uint8_t *data, const size_t data_len) {
  uint64_t hash = kFnvOffsetBasis;
  for (size_t i = 0U; i < data_len; ++i) {
    hash ^= static_cast<uint64_t>(data[i]);
    hash *= kFnvPrime;
  }
  return hash;
}

int32_t CreateMemFdBySyscall(const std::string &name) {
#if defined(__NR_memfd_create)
  return static_cast<int32_t>(syscall(__NR_memfd_create, name.c_str(), 0));
#else
  (void)name;
  errno = ENOSYS;
  return kInvalidFd;
#endif
}

ReleaseOpsRegInfoFunc GetReleaseOpsRegInfoFunc() {
  return reinterpret_cast<ReleaseOpsRegInfoFunc>(mmDlsym(RTLD_DEFAULT, kReleaseOpsRegInfoSymbol));
}
}

CustomOpSoLoader &CustomOpSoLoader::GetInstance() {
  static CustomOpSoLoader instance;
  return instance;
}

CustomOpSoLoader::~CustomOpSoLoader() {
  if (!loaded_states_.empty()) {
    const auto release_ops_reg_info = GetReleaseOpsRegInfoFunc();
    if (release_ops_reg_info != nullptr) {
      release_ops_reg_info();
    }
  }
  Cleanup();
}

Status CustomOpSoLoader::GetSoKey(const OpSoBinPtr &op_so_bin, std::string &so_key) const {
  GE_ASSERT_NOTNULL(op_so_bin);
  so_key = GetSanitizedName(op_so_bin->GetVendorName()) + "_" + GetSanitizedName(op_so_bin->GetSoName());
  GE_ASSERT_TRUE(!so_key.empty(), "custom op so key is empty.");
  return SUCCESS;
}

Status CustomOpSoLoader::CalculateSoBinFingerprint(const OpSoBinPtr &op_so_bin,
                                                   SoBinFingerprint &fingerprint) const {
  GE_ASSERT_NOTNULL(op_so_bin);
  GE_ASSERT_TRUE(op_so_bin->GetBinData() != nullptr, "so[%s] bin data is null.", op_so_bin->GetSoName().c_str());
  GE_ASSERT_TRUE(op_so_bin->GetBinDataSize() > 0U, "so[%s] bin data size is zero.", op_so_bin->GetSoName().c_str());

  const auto *bin_data = op_so_bin->GetBinData();
  fingerprint.bin_size = op_so_bin->GetBinDataSize();
  fingerprint.content_hash = CalculateFnv1a64(bin_data, fingerprint.bin_size);
  return SUCCESS;
}

Status CustomOpSoLoader::CreateSoMemFd(const std::string &so_key, int32_t &mem_fd) const {
  GE_ASSERT_TRUE(!so_key.empty(), "custom op so key is empty.");
  const int32_t fd = CreateMemFdBySyscall(so_key);
  GE_ASSERT_TRUE(fd != kInvalidFd,
                 "[CustomOpSoLoader] create memfd for custom op so[%s] failed, errno:%d, %s",
                 so_key.c_str(), errno, kNoDiskFallbackHint);
  const std::string proc_fd_path = std::string(kProcFdPrefix) + std::to_string(fd);
  if (mmAccess2(proc_fd_path.c_str(), M_R_OK) != EN_OK) {
    GELOGE(FAILED,
           "[CustomOpSoLoader] proc fd path[%s] is not readable for custom op so[%s], errno:%d, %s",
           proc_fd_path.c_str(), so_key.c_str(), errno, kNoDiskFallbackHint);
    (void)mmClose(fd);
    return FAILED;
  }
  mem_fd = fd;
  return SUCCESS;
}

Status CustomOpSoLoader::WriteSoBinToFd(const OpSoBinPtr &op_so_bin, const int32_t mem_fd) const {
  GE_ASSERT_NOTNULL(op_so_bin);
  GE_ASSERT_TRUE(mem_fd != kInvalidFd, "mem fd is invalid for custom op so[%s].", op_so_bin->GetSoName().c_str());
  GE_ASSERT_TRUE(op_so_bin->GetBinData() != nullptr, "so[%s] bin data is null.", op_so_bin->GetSoName().c_str());
  GE_ASSERT_TRUE(op_so_bin->GetBinDataSize() > 0U, "so[%s] bin data size is zero.", op_so_bin->GetSoName().c_str());
  size_t written_len = 0U;
  const auto *bin_data = op_so_bin->GetBinData();
  const size_t bin_size = static_cast<size_t>(op_so_bin->GetBinDataSize());
  while (written_len < bin_size) {
    const ssize_t current_write_len = write(mem_fd, bin_data + written_len, bin_size - written_len);
    GE_ASSERT_TRUE(current_write_len > 0,
                   "[CustomOpSoLoader] write memfd for custom op so[%s] failed, errno:%d",
                   op_so_bin->GetSoName().c_str(), errno);
    written_len += static_cast<size_t>(current_write_len);
  }
  GE_ASSERT_TRUE(lseek(mem_fd, 0, SEEK_SET) != -1,
                 "[CustomOpSoLoader] reset memfd for custom op so[%s] failed, errno:%d",
                 op_so_bin->GetSoName().c_str(), errno);
  return SUCCESS;
}

Status CustomOpSoLoader::DlopenSoByFd(const int32_t mem_fd, void *&handle) const {
  GE_ASSERT_TRUE(mem_fd != kInvalidFd, "mem fd is invalid when loading custom op so.");
  const std::string so_path = std::string(kProcFdPrefix) + std::to_string(mem_fd);
  const int32_t open_flag =
      static_cast<int32_t>(static_cast<uint32_t>(MMPA_RTLD_NOW) | static_cast<uint32_t>(MMPA_RTLD_GLOBAL));
  handle = mmDlopen(so_path.c_str(), open_flag);
  GE_ASSERT_TRUE(handle != nullptr, "dlopen custom op so[%s] failed, errmsg:%s", so_path.c_str(), mmDlerror());
  GELOGI("[CustomOpSoLoader] dlopen custom op so[%s] success.", so_path.c_str());
  return SUCCESS;
}

Status CustomOpSoLoader::LoadCustomOpSoBins(const std::vector<OpSoBinPtr> &custom_so_bins) {
  if (custom_so_bins.empty()) {
    return SUCCESS;
  }
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &so_bin : custom_so_bins) {
    std::string so_key;
    GE_ASSERT_SUCCESS(GetSoKey(so_bin, so_key));
    SoBinFingerprint current_fingerprint;
    GE_ASSERT_SUCCESS(CalculateSoBinFingerprint(so_bin, current_fingerprint));
    const auto loaded_state_it = loaded_states_.find(so_key);
    if (loaded_state_it != loaded_states_.end()) {
      if (loaded_state_it->second.fingerprint == current_fingerprint) {
        GELOGI("[CustomOpSoLoader] custom op so key[%s] already loaded with same content, skip reload.",
               so_key.c_str());
        continue;
      }
      GELOGE(FAILED, "[CustomOpSoLoader] custom op so key[%s] already loaded with different content.",
             so_key.c_str());
      return FAILED;
    }
    int32_t mem_fd = kInvalidFd;
    GE_ASSERT_SUCCESS(CreateSoMemFd(so_key, mem_fd));
    if (WriteSoBinToFd(so_bin, mem_fd) != SUCCESS) {
      (void)mmClose(mem_fd);
      return FAILED;
    }
    void *handle = nullptr;
    if (DlopenSoByFd(mem_fd, handle) != SUCCESS) {
      (void)mmClose(mem_fd);
      return FAILED;
    }
    const auto emplace_ret = loaded_states_.emplace(so_key, LoadedSoState{current_fingerprint, handle, mem_fd});
    if (!emplace_ret.second) {
      if (handle != nullptr) {
        (void)mmDlclose(handle);
      }
      (void)mmClose(mem_fd);
      GELOGE(FAILED, "[CustomOpSoLoader] emplace loaded custom op so key[%s] failed.", so_key.c_str());
      return FAILED;
    }
  }
  return SUCCESS;
}

void CustomOpSoLoader::Cleanup() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &loaded_state : loaded_states_) {
    if ((loaded_state.second.handle != nullptr) && (mmDlclose(loaded_state.second.handle) != 0)) {
      GELOGW("[CustomOpSoLoader] dlclose custom op so key[%s] failed, errmsg:%s",
             loaded_state.first.c_str(), mmDlerror());
    }
    if ((loaded_state.second.mem_fd != kInvalidFd) && (mmClose(loaded_state.second.mem_fd) != EN_OK)) {
      GELOGW("[CustomOpSoLoader] close mem fd for custom op so key[%s] failed, errno:%d",
             loaded_state.first.c_str(), errno);
    }
  }
  loaded_states_.clear();
}
}  // namespace ge
