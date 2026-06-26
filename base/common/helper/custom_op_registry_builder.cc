/**
 * Copyright (c) 2026 Huawei Technologies Co., Ltd.
 * This program is free software, you can redistribute it and/or modify it under the terms and conditions of
 * CANN Open Software License Agreement Version 2.0 (the "License").
 * Please refer to the License for details. You may not use this file except in compliance with the License.
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY, OR FITNESS FOR A PARTICULAR PURPOSE.
 * See LICENSE in the root of the software repository for the full text of the License.
 */

#include "common/helper/custom_op_registry_builder.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "common/helper/custom_op_so_loader.h"
#include "graph/custom_op.h"
#include "framework/common/debug/log.h"
#include "graph_metadef/common/ge_common/util.h"
#include "graph/custom_op_pull_registry.h"
#include "mmpa/mmpa_api.h"

namespace ge {
namespace {
constexpr const char *kGetCreatorAbiVersionSymbol = "GetRegisteredCustomOpCreatorAbiVersion";
constexpr const char *kGetCreatorNumSymbol = "GetRegisteredCustomOpCreatorNum";
constexpr const char *kGetCreatorsSymbol = "GetRegisteredCustomOpCreators";

using GetCreatorAbiVersionFunc = uint32_t (*)();
using GetCreatorNumFunc = size_t (*)();
using GetCreatorsFunc = int32_t (*)(CustomOpTypeToCreator *, size_t, size_t);

struct PullCreatorSymbols {
  GetCreatorAbiVersionFunc get_abi_version = nullptr;
  GetCreatorNumFunc get_creator_num = nullptr;
  GetCreatorsFunc get_creators = nullptr;
};

struct PendingCreator {
  std::string op_type;
  CustomOpCreateFunc creator = nullptr;
};

Status ResolvePullCreatorSymbols(void *const so_handle, const CustomOpRegistryBuilder::DlsymFunc dlsym_func,
                                 PullCreatorSymbols &symbols) {
  symbols.get_abi_version =
      reinterpret_cast<GetCreatorAbiVersionFunc>(dlsym_func(so_handle, kGetCreatorAbiVersionSymbol));
  symbols.get_creator_num = reinterpret_cast<GetCreatorNumFunc>(dlsym_func(so_handle, kGetCreatorNumSymbol));
  symbols.get_creators = reinterpret_cast<GetCreatorsFunc>(dlsym_func(so_handle, kGetCreatorsSymbol));
  if ((symbols.get_abi_version == nullptr) || (symbols.get_creator_num == nullptr) ||
      (symbols.get_creators == nullptr)) {
    GELOGE(FAILED, "[CUSTOM OP] pull creator ABI symbols are incomplete.");
    return FAILED;
  }
  return SUCCESS;
}

Status LoadRawCreators(const PullCreatorSymbols &symbols, std::vector<CustomOpTypeToCreator> &raw_creators) {
  const uint32_t abi_version = symbols.get_abi_version();
  if (abi_version != kCustomOpCreatorPullAbiVersion) {
    GELOGE(FAILED, "[CUSTOM OP] pull creator ABI version %u does not match expected %u.", abi_version,
           kCustomOpCreatorPullAbiVersion);
    return FAILED;
  }

  const size_t creator_num = symbols.get_creator_num();
  raw_creators.resize(creator_num);
  const auto ret = symbols.get_creators(raw_creators.empty() ? nullptr : raw_creators.data(), raw_creators.size(),
                                        sizeof(CustomOpTypeToCreator));
  if (ret != 0) {
    GELOGE(FAILED, "[CUSTOM OP] get registered custom op creators failed, ret:%d.", ret);
    return FAILED;
  }
  return SUCCESS;
}

Status ValidateAndCollectCreator(const CustomOpTypeToCreator &raw_creator, const CustomOpRegistryPtr &registry,
                                 std::set<std::string> &pending_op_types,
                                 std::vector<PendingCreator> &pending_creators) {
  if ((raw_creator.struct_size != sizeof(CustomOpTypeToCreator)) || (raw_creator.op_type == nullptr) ||
      (raw_creator.op_type[0] == '\0') || (raw_creator.creator == nullptr)) {
    GELOGE(FAILED, "[CUSTOM OP] invalid custom op pull creator entry.");
    return FAILED;
  }

  const std::string op_type(raw_creator.op_type);
  if (registry->HasCreator(AscendString(op_type.c_str())) ||
      (pending_op_types.find(op_type) != pending_op_types.end())) {
    GELOGE(FAILED, "[CUSTOM OP] duplicate custom op creator for %s in model registry.", op_type.c_str());
    return FAILED;
  }

  (void)pending_op_types.insert(op_type);
  pending_creators.push_back({op_type, raw_creator.creator});
  return SUCCESS;
}

Status CollectCreatorsFromSoHandle(const CustomOpSoHandlePtr &so_handle, const CustomOpRegistryPtr &registry,
                                   const CustomOpRegistryBuilder::DlsymFunc dlsym_func,
                                   std::set<std::string> &pending_op_types,
                                   std::vector<PendingCreator> &pending_creators) {
  if ((so_handle == nullptr) || (so_handle->GetHandle() == nullptr)) {
    GELOGE(FAILED, "[CUSTOM OP] custom op so handle is null.");
    return FAILED;
  }

  PullCreatorSymbols symbols;
  GE_CHK_STATUS_RET(ResolvePullCreatorSymbols(so_handle->GetHandle(), dlsym_func, symbols),
                    "[CUSTOM OP] resolve pull creator symbols failed.");
  std::vector<CustomOpTypeToCreator> raw_creators;
  GE_CHK_STATUS_RET(LoadRawCreators(symbols, raw_creators), "[CUSTOM OP] load raw pull creators failed.");
  for (const auto &raw_creator : raw_creators) {
    GE_CHK_STATUS_RET(ValidateAndCollectCreator(raw_creator, registry, pending_op_types, pending_creators),
                      "[CUSTOM OP] validate pull creator failed.");
  }
  return SUCCESS;
}
}  // namespace

Status CustomOpRegistryBuilder::AddCreatorsFromSoHandles(const std::vector<CustomOpSoHandlePtr> &so_handles,
                                                         const CustomOpRegistryPtr &registry) {
  return AddCreatorsFromSoHandles(so_handles, registry, mmDlsym);
}

Status CustomOpRegistryBuilder::AddCreatorsFromSoHandles(const std::vector<CustomOpSoHandlePtr> &so_handles,
                                                         const CustomOpRegistryPtr &registry,
                                                         const DlsymFunc dlsym_func) {
  if ((registry == nullptr) || (dlsym_func == nullptr)) {
    GELOGE(FAILED, "[CUSTOM OP] registry or dlsym function is null.");
    return FAILED;
  }

  std::set<std::string> pending_op_types;
  std::vector<PendingCreator> pending_creators;
  for (const auto &so_handle : so_handles) {
    GE_CHK_STATUS_RET(CollectCreatorsFromSoHandle(so_handle, registry, dlsym_func, pending_op_types, pending_creators),
                      "[CUSTOM OP] collect creators from custom op so handle failed.");
  }

  for (const auto &pending_creator : pending_creators) {
    const auto register_ret = registry->RegisterCreator(
        AscendString(pending_creator.op_type.c_str()),
        [creator = pending_creator.creator]() { return std::unique_ptr<BaseCustomOp>(creator()); });
    GE_CHK_STATUS_RET(register_ret, "[CUSTOM OP] register pull creator to model registry failed.");
  }
  registry->AddSoHandles(so_handles);
  return SUCCESS;
}
}  // namespace ge
