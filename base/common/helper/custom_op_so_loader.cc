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
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <dlfcn.h>
#include <iomanip>
#include <functional>
#include <sstream>
#include <unistd.h>
#include <sys/syscall.h>
#include <utility>
#include <vector>

#include "common/checker.h"
#include "framework/common/debug/log.h"
#include "graph/custom_op_load_context.h"
#include "graph/utils/file_utils.h"
#include "mmpa/mmpa_api.h"

/*
 * 文件职责：
 *   本文件实现 CustomOpSoLoader，用于将 OM 中携带的自定义算子 so 二进制以“仅内存”方式加载到当前进程。
 *
 * 核心流程：
 *   1) 生成稳定标识：使用 so 二进制内容的 hash hex 字符串作为 fingerprint key。
 *   2) 内存加载：通过 memfd_create 创建匿名 fd，写入 so 数据，再以 /proc/self/fd/<fd> 调用 mmDlopen。
 *   3) 状态去重：以 fingerprint key 缓存 weak lease；同内容复用存活 lease，同名不同内容互不冲突。
 *   4) 生命周期管理：CustomOpSoHandle 析构执行 mmDlclose + mmClose，loader 仅维护 weak cache，
 *      长期强引用由模型级 CustomOpRegistry 持有。
 *
 * 设计约束：
 *   严格禁止任何“回退落盘”路径；当 memfd_create 或 /proc/self/fd 路径不可用时，直接返回失败。
 */
namespace ge {
namespace {
constexpr int32_t kInvalidFd = -1;
constexpr const char_t *kProcFdPrefix = "/proc/self/fd/";
constexpr const char_t *kNoDiskFallbackHint = "strict no-disk-fallback is enabled.";
constexpr const char *kReleaseOpsRegInfoSymbol = "ReleaseOpsRegInfo";

using ReleaseOpsRegInfoFunc = void (*)();

class PendingSoResource {
 public:
  ~PendingSoResource() {
    if ((handle_ != nullptr) && (mmDlclose(handle_) != 0)) {
      GELOGW("[CustomOpSoLoader] dlclose pending custom op so failed, errmsg:%s", mmDlerror());
    }
    handle_ = nullptr;
    if ((mem_fd_ != kInvalidFd) && (mmClose(mem_fd_) != EN_OK)) {
      GELOGW("[CustomOpSoLoader] close pending mem fd failed, errno:%d", errno);
    }
    mem_fd_ = kInvalidFd;
  }

  PendingSoResource() = default;
  PendingSoResource(const PendingSoResource &) = delete;
  PendingSoResource &operator=(const PendingSoResource &) = delete;

  int32_t &MutableFd() noexcept {
    return mem_fd_;
  }

  int32_t GetFd() const noexcept {
    return mem_fd_;
  }

  void *&MutableHandle() noexcept {
    return handle_;
  }

  void *GetHandle() const noexcept {
    return handle_;
  }

  void Release() noexcept {
    handle_ = nullptr;
    mem_fd_ = kInvalidFd;
  }

 private:
  int32_t mem_fd_ = kInvalidFd;
  void *handle_ = nullptr;
};

std::string CalculateBinHash(const uint8_t *data, const size_t data_len) {
  const size_t hash_val = std::hash<std::string>{}(std::string(reinterpret_cast<const char *>(data), data_len));
  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(sizeof(size_t) * 2) << hash_val;
  return oss.str();
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
}  // namespace

CustomOpSoLoader::CustomOpSoLoader() = default;

CustomOpSoHandle::CustomOpSoHandle(std::string fingerprint_key, void *handle, std::string so_name,
                                   const size_t bin_size, const int32_t mem_fd)
    : fingerprint_key_(std::move(fingerprint_key)),
      handle_(handle),
      so_name_(std::move(so_name)),
      bin_size_(bin_size),
      mem_fd_(mem_fd) {}

CustomOpSoHandle::~CustomOpSoHandle() {
  if ((handle_ != nullptr) && (mmDlclose(handle_) != 0)) {
    GELOGW("[CustomOpSoLoader] dlclose custom op so[%s] fingerprint[%s] failed, errmsg:%s", so_name_.c_str(),
           fingerprint_key_.c_str(), mmDlerror());
  }
  handle_ = nullptr;
  if ((mem_fd_ != kInvalidFd) && (mmClose(mem_fd_) != EN_OK)) {
    GELOGW("[CustomOpSoLoader] close mem fd for custom op so[%s] fingerprint[%s] failed, errno:%d", so_name_.c_str(),
           fingerprint_key_.c_str(), errno);
  }
  mem_fd_ = kInvalidFd;
}

void CustomOpSoHandle::AdoptResource(void *handle, const int32_t mem_fd) noexcept {
  handle_ = handle;
  mem_fd_ = mem_fd;
}

void *CustomOpSoHandle::GetHandle() const {
  return handle_;
}

const std::string &CustomOpSoHandle::GetFingerprintKey() const {
  return fingerprint_key_;
}

const std::string &CustomOpSoHandle::GetSoName() const {
  return so_name_;
}

CustomOpSoLoader &CustomOpSoLoader::GetInstance() {
  static CustomOpSoLoader instance;
  return instance;
}

CustomOpSoLoader::~CustomOpSoLoader() = default;

void CustomOpSoLoader::Finalize() {
  auto &loader = GetInstance();
  bool any_alive = false;
  {
    std::lock_guard<std::mutex> lock(loader.mutex_);
    for (const auto &entry : loader.loaded_states_) {
      if (!entry.second.expired()) {
        any_alive = true;
        break;
      }
    }
  }
  if (any_alive) {
    const auto release_ops_reg_info = GetReleaseOpsRegInfoFunc();
    if (release_ops_reg_info != nullptr) {
      release_ops_reg_info();
    }
  }
  loader.Cleanup();
}

Status CustomOpSoLoader::GetSoKey(const OpSoBinPtr &op_so_bin, std::string &so_key) const {
  GE_ASSERT_NOTNULL(op_so_bin);
  so_key = GetSanitizedName(op_so_bin->GetVendorName()) + "_" + GetSanitizedName(op_so_bin->GetSoName());
  GE_ASSERT_TRUE(!so_key.empty(), "custom op so key is empty.");
  return SUCCESS;
}

Status CustomOpSoLoader::CalculateSoBinFingerprint(const OpSoBinPtr &op_so_bin, std::string &fingerprint_key) const {
  GE_ASSERT_NOTNULL(op_so_bin);
  GE_ASSERT_TRUE(op_so_bin->GetBinData() != nullptr, "so[%s] bin data is null.", op_so_bin->GetSoName().c_str());
  GE_ASSERT_TRUE(op_so_bin->GetBinDataSize() > 0U, "so[%s] bin data size is zero.", op_so_bin->GetSoName().c_str());

  const auto *bin_data = op_so_bin->GetBinData();
  fingerprint_key = CalculateBinHash(bin_data, op_so_bin->GetBinDataSize());
  GE_ASSERT_TRUE(!fingerprint_key.empty(), "so[%s] fingerprint hash is empty.", op_so_bin->GetSoName().c_str());
  return SUCCESS;
}

Status CustomOpSoLoader::CreateSoMemFd(const std::string &so_key, int32_t &mem_fd) const {
  GE_ASSERT_TRUE(!so_key.empty(), "custom op so key is empty.");
  const int32_t fd = CreateMemFdBySyscall(so_key);
  GE_ASSERT_TRUE(fd != kInvalidFd, "[CustomOpSoLoader] create memfd for custom op so[%s] failed, errno:%d, %s",
                 so_key.c_str(), errno, kNoDiskFallbackHint);
  const std::string proc_fd_path = std::string(kProcFdPrefix) + std::to_string(fd);
  if (mmAccess2(proc_fd_path.c_str(), M_R_OK) != EN_OK) {
    GELOGE(FAILED, "[CustomOpSoLoader] proc fd path[%s] is not readable for custom op so[%s], errno:%d, %s",
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
    GE_ASSERT_TRUE(current_write_len > 0, "[CustomOpSoLoader] write memfd for custom op so[%s] failed, errno:%d",
                   op_so_bin->GetSoName().c_str(), errno);
    written_len += static_cast<size_t>(current_write_len);
  }
  const off_t actual_fd_size = lseek(mem_fd, 0, SEEK_END);
  GE_ASSERT_TRUE((actual_fd_size >= 0) && (static_cast<size_t>(actual_fd_size) == bin_size),
                 "[CustomOpSoLoader] memfd size[%jd] does not match declared so[%s] bin size[%zu].",
                 static_cast<intmax_t>(actual_fd_size), op_so_bin->GetSoName().c_str(), bin_size);
  GE_ASSERT_TRUE(lseek(mem_fd, 0, SEEK_SET) != -1,
                 "[CustomOpSoLoader] reset memfd for custom op so[%s] failed, errno:%d", op_so_bin->GetSoName().c_str(),
                 errno);
  return SUCCESS;
}

Status CustomOpSoLoader::DlopenSoByFd(const int32_t mem_fd, void *&handle) const {
  GE_ASSERT_TRUE(mem_fd != kInvalidFd, "mem fd is invalid when loading custom op so.");
  const std::string so_path = std::string(kProcFdPrefix) + std::to_string(mem_fd);
  const int32_t open_flag = static_cast<int32_t>(MMPA_RTLD_NOW);
  ScopedOfflineCustomOpSoLoadGuard offline_custom_op_so_load_guard;
  handle = mmDlopen(so_path.c_str(), open_flag);
  GE_ASSERT_TRUE(handle != nullptr, "dlopen custom op so[%s] failed, errmsg:%s", so_path.c_str(), mmDlerror());
  GELOGI("[CustomOpSoLoader] dlopen custom op so[%s] success.", so_path.c_str());
  return SUCCESS;
}

Status CustomOpSoLoader::LoadCustomOpSoBins(const std::vector<OpSoBinPtr> &custom_so_bins,
                                            std::vector<CustomOpSoHandlePtr> &loaded_handles) {
  if (custom_so_bins.empty()) {
    return SUCCESS;
  }
  std::vector<CustomOpSoHandlePtr> current_loaded_handles;
  for (const auto &so_bin : custom_so_bins) {
    GE_ASSERT_SUCCESS(LoadSingleCustomOpSoBin(so_bin, current_loaded_handles));
  }
  loaded_handles.insert(loaded_handles.end(), current_loaded_handles.begin(), current_loaded_handles.end());
  return SUCCESS;
}

Status CustomOpSoLoader::LoadSingleCustomOpSoBin(const OpSoBinPtr &so_bin,
                                                 std::vector<CustomOpSoHandlePtr> &loaded_handles) {
  GE_ASSERT_NOTNULL(so_bin);
  std::string diagnostic_so_key;
  GE_ASSERT_SUCCESS(GetSoKey(so_bin, diagnostic_so_key));
  std::string fingerprint_key;
  GE_ASSERT_SUCCESS(CalculateSoBinFingerprint(so_bin, fingerprint_key));

  auto loaded_handle = GetLoadedHandle(fingerprint_key);
  if (loaded_handle != nullptr) {
    GELOGI("[CustomOpSoLoader] custom op so[%s] fingerprint[%s] already loaded, reuse lease.",
           so_bin->GetSoName().c_str(), fingerprint_key.c_str());
    loaded_handles.emplace_back(loaded_handle);
    return SUCCESS;
  }

  CustomOpSoHandlePtr candidate_handle;
  const Status load_status = LoadCustomOpSoBinCandidate(so_bin, diagnostic_so_key, fingerprint_key, candidate_handle);
  if (load_status != SUCCESS) {
    loaded_handle = GetLoadedHandle(fingerprint_key);
    if (loaded_handle != nullptr) {
      GELOGI("[CustomOpSoLoader] custom op so[%s] fingerprint[%s] was published after local load failure, reuse lease.",
             so_bin->GetSoName().c_str(), fingerprint_key.c_str());
      loaded_handles.emplace_back(loaded_handle);
      return SUCCESS;
    }
    return load_status;
  }
  PublishOrReuseLoadedHandle(fingerprint_key, candidate_handle, loaded_handle);
  GE_ASSERT_NOTNULL(loaded_handle);
  loaded_handles.emplace_back(loaded_handle);
  return SUCCESS;
}

CustomOpSoHandlePtr CustomOpSoLoader::GetLoadedHandle(const std::string &fingerprint_key) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto loaded_state_it = loaded_states_.find(fingerprint_key);
  if (loaded_state_it == loaded_states_.end()) {
    return nullptr;
  }
  const auto loaded_handle = loaded_state_it->second.lock();
  if (loaded_handle == nullptr) {
    (void)loaded_states_.erase(loaded_state_it);
  }
  return loaded_handle;
}

Status CustomOpSoLoader::LoadCustomOpSoBinCandidate(const OpSoBinPtr &so_bin, const std::string &diagnostic_so_key,
                                                    const std::string &fingerprint_key,
                                                    CustomOpSoHandlePtr &candidate_handle) const {
  auto new_handle = std::make_shared<CustomOpSoHandle>(fingerprint_key, nullptr, so_bin->GetSoName(),
                                                       so_bin->GetBinDataSize(), kInvalidFd);
  PendingSoResource pending_resource;
  Status load_status = CreateSoMemFd(fingerprint_key, pending_resource.MutableFd());
  if (load_status == SUCCESS) {
    load_status = WriteSoBinToFd(so_bin, pending_resource.GetFd());
  }
  if (load_status == SUCCESS) {
    load_status = DlopenSoByFd(pending_resource.GetFd(), pending_resource.MutableHandle());
  }
  if (load_status != SUCCESS) {
    return load_status;
  }

  new_handle->AdoptResource(pending_resource.GetHandle(), pending_resource.GetFd());
  pending_resource.Release();
  GELOGI("[CustomOpSoLoader] custom op so[%s] diagnostic key[%s] fingerprint[%s] candidate loaded.",
         so_bin->GetSoName().c_str(), diagnostic_so_key.c_str(), fingerprint_key.c_str());
  candidate_handle = new_handle;
  return SUCCESS;
}

void CustomOpSoLoader::PublishOrReuseLoadedHandle(const std::string &fingerprint_key,
                                                  const CustomOpSoHandlePtr &candidate_handle,
                                                  CustomOpSoHandlePtr &loaded_handle) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto loaded_state_it = loaded_states_.find(fingerprint_key);
  if (loaded_state_it != loaded_states_.end()) {
    loaded_handle = loaded_state_it->second.lock();
    if (loaded_handle != nullptr) {
      GELOGI("[CustomOpSoLoader] custom op so fingerprint[%s] was published concurrently, reuse lease.",
             fingerprint_key.c_str());
      return;
    }
    (void)loaded_states_.erase(loaded_state_it);
  }
  loaded_states_[fingerprint_key] = candidate_handle;
  loaded_handle = candidate_handle;
}

void CustomOpSoLoader::Cleanup() {
  std::lock_guard<std::mutex> lock(mutex_);
  loaded_states_.clear();
}
}  // namespace ge
