/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "file_const_loader.h"

#include <algorithm>
#include <fstream>
#include <future>
#include <memory>
#include <set>
#include <vector>

#include "acl/acl_rt.h"
#include "common/checker.h"
#include "common/ge_common/scope_guard.h"
#include "common/ge_inner_error_codes.h"
#include "om2_external_weight_manager.h"
#include "om2_var_manager.h"
#include "om2_file_utils.h"
#include "om2_malloc_helper.h"
#include "om2_thread_pool.h"
#include "rt_external_mem.h"

namespace gert {
namespace {
constexpr size_t kCombinedConstCopyBlockSize = 10485760U;

ge::Status RtMemcpyToDevice(const void *const host_addr, const size_t size, void *const device_addr) {
  if ((size == 0U) || (device_addr == nullptr)) {
    return ge::SUCCESS;
  }
  const auto memcpy_ret = aclrtMemcpy(device_addr, size, host_addr, size, ACL_MEMCPY_HOST_TO_DEVICE);
  if (memcpy_ret != ACL_SUCCESS) {
    GELOGE(ge::FAILED, "[OM2][Memcpy] rtMemcpy H2D failed, size=%zu, rt_ret=%u", size, memcpy_ret);
    return ge::FAILED;
  }
  return ge::SUCCESS;
}

ge::Status RtMallocAndCopyToDevice(const void *const host_addr, const size_t size, const FileConstContext &ctx,
                                   void *&device_addr) {
  device_addr = nullptr;
  if (size == 0U) {
    return ge::SUCCESS;
  }
  const auto malloc_ret = Om2Malloc(&device_addr, size, RT_MEMORY_HBM, 0);
  if (malloc_ret != ACL_SUCCESS) {
    GELOGE(ge::FAILED, "[OM2][Alloc] aclrtMalloc failed, size=%zu, rt_ret=%u", size, malloc_ret);
    return ge::FAILED;
  }
  const auto memcpy_ret = RtMemcpyToDevice(host_addr, size, device_addr);
  if (memcpy_ret != ge::SUCCESS) {
    (void)aclrtFree(device_addr);
    device_addr = nullptr;
    return memcpy_ret;
  }
  if ((ctx.owned_buffers != nullptr) && (device_addr != nullptr)) {
    ctx.owned_buffers->push_back(device_addr);
  }
  return ge::SUCCESS;
}

ge::Status CopyCombinedConstFromFile(std::ifstream &ifs, const std::string &file_path, void *const device_addr,
                                     const size_t file_size) {
  if (file_size == 0U) {
    return ge::SUCCESS;
  }
  const size_t host_buffer_size = std::min(kCombinedConstCopyBlockSize, file_size);
  auto host_buffer = std::make_unique<uint8_t[]>(host_buffer_size);
  GE_ASSERT_NOTNULL(host_buffer, "[OM2][Alloc] Failed to allocate combined host buffer, path=%s, size=%zu.",
                    file_path.c_str(), host_buffer_size);
  size_t copied_size = 0U;
  while (copied_size < file_size) {
    const size_t copy_size = std::min(kCombinedConstCopyBlockSize, file_size - copied_size);
    ifs.read(reinterpret_cast<char *>(host_buffer.get()), static_cast<std::streamsize>(copy_size));
    const auto read_size = static_cast<size_t>(ifs.gcount());
    GE_ASSERT_TRUE(read_size == copy_size,
                   "[OM2][Read] Failed to read combined file, path=%s, expected=%zu, actual=%zu.", file_path.c_str(),
                   copy_size, read_size);

    auto *const cur_device_addr = static_cast<uint8_t *>(device_addr) + copied_size;
    const auto memcpy_ret =
        aclrtMemcpy(cur_device_addr, file_size - copied_size, host_buffer.get(), copy_size, ACL_MEMCPY_HOST_TO_DEVICE);
    if (memcpy_ret != ACL_SUCCESS) {
      GELOGE(ge::FAILED, "[OM2][Memcpy] Combined const H2D failed, path=%s, offset=%zu, size=%zu, rt_ret=%u.",
             file_path.c_str(), copied_size, copy_size, memcpy_ret);
      return ge::FAILED;
    }
    copied_size += copy_size;
  }
  return ge::SUCCESS;
}

ge::Status ReadFileSlice(const std::string &file_path, size_t offset, size_t size, std::vector<uint8_t> &buffer) {
  std::ifstream ifs(file_path, std::ios::binary);
  GE_ASSERT_TRUE(ifs.is_open(), "[OM2][Open] Failed to open file const file.");
  (void)ifs.seekg(0, std::ifstream::end);
  const auto end_pos = ifs.tellg();
  GE_ASSERT_TRUE(end_pos >= 0, "[OM2][Check] Failed to get file const file size.");
  const auto file_size = static_cast<size_t>(end_pos);
  GE_ASSERT_TRUE((offset + size) <= file_size, "[OM2][Check] Invalid file const offset or size.");
  (void)ifs.seekg(static_cast<std::streamoff>(offset), std::ifstream::beg);
  buffer.resize(size);
  ifs.read(reinterpret_cast<char *>(buffer.data()), static_cast<std::streamsize>(size));
  GE_ASSERT_TRUE(ifs.good(), "[OM2][Read] Failed to read file const slice.");
  return ge::SUCCESS;
}

std::string MakeFileConstKey(const std::string &file_name, const size_t offset) {
  return file_name + ":" + std::to_string(offset);
}

Om2VarManagerPtr GetVarManager(const FileConstContext &ctx) {
  if (ctx.device_id < 0) {
    return nullptr;
  }
  return Om2VarManagerPool::Instance().GetManager(ctx.session_id);
}

Om2ExternalWeightManagerPtr GetExternalWeightManager(const FileConstContext &ctx) {
  if (ctx.device_id < 0) {
    return nullptr;
  }
  return Om2ExternalWeightManagerPool::Instance().GetManager(ctx.session_id);
}

ge::Status LoadCombinedConstBase(const std::string &file_name, const FileConstContext &ctx, void *&base_addr,
                                 size_t &file_size) {
  base_addr = nullptr;
  file_size = 0U;
  if ((ctx.user_file_const_mems != nullptr) && !ctx.user_file_const_mems->empty()) {
    const auto iter = ctx.user_file_const_mems->find(file_name);
    if (iter != ctx.user_file_const_mems->end()) {
      GE_ASSERT_TRUE(iter->second.device_mem != nullptr, "[OM2][Check] User combined device memory is nullptr.");
      base_addr = const_cast<void *>(iter->second.device_mem);
      file_size = iter->second.mem_size;
      return ge::SUCCESS;
    }
  }

  std::string file_path;
  GE_ASSERT_SUCCESS(ResolveFileConstFilePath(ctx.weight_dir, file_name, file_path));
  std::ifstream ifs(file_path, std::ios::binary);
  GE_ASSERT_TRUE(ifs.is_open(), "[OM2][Open] Failed to open combined file.");
  (void)ifs.seekg(0, std::ifstream::end);
  const auto end_pos = ifs.tellg();
  GE_ASSERT_TRUE(end_pos >= 0, "[OM2][Check] Failed to get combined file size.");
  file_size = static_cast<size_t>(end_pos);
  (void)ifs.seekg(0, std::ifstream::beg);
  GE_ASSERT_TRUE(ifs.good(), "[OM2][Seek] Failed to seek combined file to beginning.");
  if (file_size == 0U) {
    return ge::SUCCESS;
  }

  const auto malloc_ret = Om2Malloc(&base_addr, file_size, RT_MEMORY_HBM, 0);
  if (malloc_ret != ACL_SUCCESS) {
    GELOGE(ge::FAILED, "[OM2][Alloc] aclrtMalloc failed, size=%zu, rt_ret=%u", file_size, malloc_ret);
    return ge::FAILED;
  }

  const auto copy_ret = CopyCombinedConstFromFile(ifs, file_path, base_addr, file_size);
  if (copy_ret != ge::SUCCESS) {
    (void)aclrtFree(base_addr);
    base_addr = nullptr;
    return copy_ret;
  }
  if ((ctx.owned_buffers != nullptr) && (base_addr != nullptr)) {
    ctx.owned_buffers->push_back(base_addr);
  }
  return ge::SUCCESS;
}

ge::Status MapCombinedConstItem(const Om2ConstItem &const_item, void *base_addr, size_t file_size,
                                std::vector<void *> &constants) {
  GE_ASSERT_TRUE(const_item.index < constants.size(), "[OM2][Check] Invalid combined const index.");
  GE_ASSERT_TRUE(const_item.offset <= file_size, "[OM2][Check] Invalid combined const offset or size.");
  GE_ASSERT_TRUE(const_item.size <= (file_size - const_item.offset),
                 "[OM2][Check] Invalid combined const offset or size.");
  constants[const_item.index] =
      (base_addr == nullptr) ? nullptr : (static_cast<uint8_t *>(base_addr) + const_item.offset);
  return ge::SUCCESS;
}
}  // namespace

ge::Status BuildUserFileConstMemMap(const std::vector<ge::FileConstantMem> &file_constant_mems,
                                    std::map<std::string, ge::FileConstantMem> &file_name_to_mem) {
  file_name_to_mem.clear();
  for (const auto &mem : file_constant_mems) {
    GE_ASSERT_TRUE(!mem.file_name.empty(), "[OM2][Check] File const file_name must not be empty.");
    file_name_to_mem[mem.file_name] = mem;
  }
  return ge::SUCCESS;
}

ge::Status ResolveFileConstWeightDir(const ge::ModelData &model_data, std::string &weight_dir) {
  weight_dir.clear();
  if (!model_data.weight_path.empty()) {
    const auto real_weight_path = ge::om2::RealPath(model_data.weight_path.c_str());
    GE_ASSERT_TRUE(!real_weight_path.empty(), "[OM2][Check] Failed to resolve weight path: [%s].", model_data.weight_path.c_str());
    weight_dir = real_weight_path + "/";
    return ge::SUCCESS;
  }
  if (model_data.om_path.empty()) {
    return ge::SUCCESS;
  }
  const auto real_om_path = ge::om2::RealPath(model_data.om_path.c_str());
  GE_ASSERT_TRUE(!real_om_path.empty(), "[OM2][Check] Failed to resolve om path: [%s].", model_data.om_path.c_str());
  std::string om_dir;
  std::string om_name;
  ge::om2::SplitFilePath(real_om_path, om_dir, om_name);
  GE_ASSERT_TRUE(!om_name.empty(), "[OM2][Check] Invalid om_path.");
  weight_dir = om_dir + "/weight/";
  return ge::SUCCESS;
}

ge::Status ResolveFileConstFilePath(const std::string &weight_dir, const std::string &file_name, std::string &file_path) {
  GE_ASSERT_TRUE(!weight_dir.empty(), "[OM2][Check] weight_dir is empty.");
  GE_ASSERT_TRUE(!file_name.empty(), "[OM2][Check] file_name is empty.");
  file_path = weight_dir;
  if (!file_path.empty() && file_path.back() != '/') {
    file_path.push_back('/');
  }
  file_path += file_name;
  const auto real_file_path = ge::om2::RealPath(file_path.c_str());
  GE_ASSERT_TRUE(!real_file_path.empty(), "[OM2][Check] Failed to resolve file path: [%s].", file_path.c_str());
  file_path = real_file_path;
  return ge::SUCCESS;
}

ge::Status PrepareCombinedConsts(const std::vector<Om2ConstItem> &const_items, const FileConstContext &ctx,
                                 std::vector<void *> &constants) {
  if (const_items.empty()) {
    return ge::SUCCESS;
  }

  const auto &combined_file_name = const_items.front().file_name;
  for (const auto &const_item : const_items) {
    GE_ASSERT_TRUE(const_item.type == "COMBINED", "[OM2][Check] PrepareCombinedConsts only supports COMBINED.");
    GE_ASSERT_TRUE(const_item.file_name == combined_file_name,
                   "[OM2][Check] Combined consts must use the same file.");
  }

  void *base_addr = nullptr;
  size_t file_size = 0U;
  GE_ASSERT_SUCCESS(LoadCombinedConstBase(combined_file_name, ctx, base_addr, file_size));
  for (const auto &const_item : const_items) {
    GE_ASSERT_SUCCESS(MapCombinedConstItem(const_item, base_addr, file_size, constants));
  }
  return ge::SUCCESS;
}

ge::Status PrepareIndividualConst(const Om2ConstItem &const_item, const FileConstContext &ctx, void *&const_addr) {
  const_addr = nullptr;
  GE_ASSERT_TRUE(const_item.type == "INDIVIDUAL", "[OM2][Check] PrepareIndividualConst only supports INDIVIDUAL.");
  if ((ctx.user_file_const_mems != nullptr) && !ctx.user_file_const_mems->empty()) {
    const auto iter = ctx.user_file_const_mems->find(const_item.file_name);
    GE_ASSERT_TRUE(iter != ctx.user_file_const_mems->end(), "[OM2][Check] User file const memory not found.");
    GE_ASSERT_TRUE(iter->second.device_mem != nullptr, "[OM2][Check] User file const device memory is nullptr.");
    GE_ASSERT_TRUE(iter->second.mem_size >= (const_item.offset + const_item.size),
                   "[OM2][Check] User file const device memory size is invalid.");
    const_addr = static_cast<uint8_t *>(const_cast<void *>(iter->second.device_mem)) + const_item.offset;
    return ge::SUCCESS;
  }
  std::string file_path;
  GE_ASSERT_SUCCESS(ResolveFileConstFilePath(ctx.weight_dir, const_item.file_name, file_path));
  std::vector<uint8_t> host_buffer;
  GE_ASSERT_SUCCESS(ReadFileSlice(file_path, const_item.offset, const_item.size, host_buffer));
  GE_ASSERT_SUCCESS(RtMallocAndCopyToDevice(host_buffer.data(), host_buffer.size(), ctx, const_addr));
  return ge::SUCCESS;
}

ge::Status LoadIndividualConstToAddr(const Om2ConstItem &const_item, const FileConstContext &ctx, void *const const_addr) {
  GE_ASSERT_TRUE(const_item.type == "INDIVIDUAL", "[OM2][Check] LoadIndividualConstToAddr only supports INDIVIDUAL.");
  std::string file_path;
  GE_ASSERT_SUCCESS(ResolveFileConstFilePath(ctx.weight_dir, const_item.file_name, file_path));
  std::vector<uint8_t> host_buffer;
  GE_ASSERT_SUCCESS(ReadFileSlice(file_path, const_item.offset, const_item.size, host_buffer));
  GE_ASSERT_SUCCESS(RtMemcpyToDevice(host_buffer.data(), host_buffer.size(), const_addr));
  return ge::SUCCESS;
}

ge::Status LoadUniqueIndividualConsts(const std::map<std::string, const Om2ConstItem *> &unique_file_consts,
                                      const FileConstContext &ctx, const int32_t device_id,
                                      const Om2VarManagerPtr &var_manager,
                                      const Om2ExternalWeightManagerPtr &external_weight_manager) {
  if (unique_file_consts.empty()) {
    return ge::SUCCESS;
  }

  const uint32_t pool_size = static_cast<uint32_t>(std::min<size_t>(4U, unique_file_consts.size()));
  ge::om2::ThreadPool thread_pool("om2fc", pool_size == 0U ? 1U : pool_size);
  std::vector<std::future<ge::Status>> futures;
  futures.reserve(unique_file_consts.size());
  for (const auto &item : unique_file_consts) {
    const auto key = item.first;
    const auto *const_item = item.second;
    futures.emplace_back(thread_pool.commit([const_item, key, &ctx, device_id, var_manager, external_weight_manager]() {
      const auto set_device_ret = aclrtSetDevice(device_id);
      if (set_device_ret != ACL_ERROR_NONE) {
        GELOGE(ge::FAILED, "[OM2][SetDevice] aclrtSetDevice failed, device_id=%d, ret=%u", device_id, set_device_ret);
        return ge::FAILED;
      }
      GE_MAKE_GUARD(reset_device, [device_id]() {
        GE_CHK_RT(aclrtResetDevice(device_id));
      });
      void *const_addr = nullptr;
      if (const_item->size == 0U) {
        return ge::SUCCESS;
      }
      if (external_weight_manager->CheckAndSetWeightLoading(key, static_cast<uint32_t>(device_id))) {
        return ge::SUCCESS;
      }
      if (!var_manager->TryGetVarAddr(key, static_cast<uint32_t>(device_id), const_addr)) {
        GELOGE(ge::INTERNAL_ERROR, "[OM2][Get][VarAddr] Failed to get addr, key=%s, device_id=%d.", key.c_str(),
               device_id);
        return ge::INTERNAL_ERROR;
      }
      return LoadIndividualConstToAddr(*const_item, ctx, const_addr);
    }));
  }
  for (auto &future : futures) {
    GE_ASSERT_TRUE(future.valid(), "[OM2][Check] Invalid individual const future.");
    GE_ASSERT_SUCCESS(future.get());
  }
  return ge::SUCCESS;
}

ge::Status PrepareIndividualConsts(const std::vector<Om2ConstItem> &const_items, const FileConstContext &ctx,
                                   int32_t device_id, std::vector<void *> &constants) {
  if (const_items.empty()) {
    return ge::SUCCESS;
  }

  const auto var_manager = GetVarManager(ctx);
  const auto external_weight_manager = GetExternalWeightManager(ctx);
  GE_ASSERT_TRUE(((var_manager != nullptr) && (external_weight_manager != nullptr)) ||
                     ((ctx.user_file_const_mems != nullptr) && !ctx.user_file_const_mems->empty()),
                 "[OM2][Check] Invalid individual const managers.");
  std::map<std::string, const Om2ConstItem *> unique_file_consts;
  for (const auto &const_item : const_items) {
    GE_ASSERT_TRUE(const_item.index < constants.size(), "[OM2][Check] Invalid individual const index.");
    if ((ctx.user_file_const_mems != nullptr) &&
        (ctx.user_file_const_mems->find(const_item.file_name) != ctx.user_file_const_mems->end())) {
      GE_ASSERT_SUCCESS(PrepareIndividualConst(const_item, ctx, constants[const_item.index]));
    } else {
      GE_ASSERT_NOTNULL(var_manager);
      const auto key = MakeFileConstKey(const_item.file_name, const_item.offset);
      const auto [iter, inserted] = unique_file_consts.emplace(key, nullptr);
      if (inserted) {
        iter->second = &const_item;
      }
      void *const_addr = nullptr;
      GE_ASSERT_SUCCESS(var_manager->GetOrCreateVarAddr(key, static_cast<uint32_t>(device_id), const_item.size,
                                                       const_addr));
      constants[const_item.index] = const_addr;
    }
  }
  GE_ASSERT_SUCCESS(
      LoadUniqueIndividualConsts(unique_file_consts, ctx, device_id, var_manager, external_weight_manager));
  return ge::SUCCESS;
}
}  // namespace gert
